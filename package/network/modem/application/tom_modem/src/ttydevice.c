#include "ttydevice.h"
#include <sys/file.h> 

static int tty_set_device(PROFILE_T *profile, FDS_T *fds)
{
    int baud_rate, data_bits;
    struct termios tty;
    baud_rate = profile->baud_rate;
    data_bits = profile->data_bits;
    if (tcgetattr(fds->tty_fd, &tty) != 0)
    {
        err_msg("Error getting tty attributes");
        return COMM_ERROR;
    }
    memmove(&fds->old_termios, &tty, sizeof(struct termios));
    cfmakeraw(&tty);
    tty.c_cflag |= CLOCAL; // 忽略调制解调器控制线，允许本地连接
    tty.c_cflag |= CREAD;  // 使能接收

    // clear flow control ,stop bits parity
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~PARENB;
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    // set data bits 5,6,7,8
    tty.c_cflag &= ~CSIZE; // 清除数据位设置
    switch (data_bits)
    {
    case 5:
        tty.c_cflag |= CS5;
        break;
    case 6:
        tty.c_cflag |= CS6;
        break;
    case 7:
        tty.c_cflag |= CS7;
        break;
    case 8:
        tty.c_cflag |= CS8;
        break;
    default:
        tty.c_cflag |= CS8;
        break;
    }

    // set baud rate
    switch (baud_rate)
    {
    case 4800:
        cfsetspeed(&tty, B4800);
        break;
    case 9600:
        cfsetspeed(&tty, B9600);
        break;
    case 19200:
        cfsetspeed(&tty, B19200);
        break;
    case 38400:
        cfsetspeed(&tty, B38400);
        break;
    case 57600:
        cfsetspeed(&tty, B57600);
        break;
    case 115200:
        cfsetspeed(&tty, B115200);
        break;

    default:
        cfsetspeed(&tty, B115200);
        break;
    }
    if (tcsetattr(fds->tty_fd, TCSANOW, &tty) != 0)
    {
        err_msg("Error setting tty attributes");
        return COMM_ERROR;
    }
    return SUCCESS;
}
int tty_open_device(PROFILE_T *profile,FDS_T *fds)
{
    fds->tty_fd = open(profile->tty_dev, O_RDWR | O_NOCTTY);
    if (fds->tty_fd < 0)
    {
        err_msg("Error opening tty device: %s", profile->tty_dev);
        return COMM_ERROR;
    }

    if (flock(fds->tty_fd, LOCK_EX) < 0)
    {
        err_msg("Error locking tty device: %s is already in use", profile->tty_dev);
        close(fds->tty_fd);
        return COMM_ERROR;
    }

    if (tty_set_device(profile,fds) != 0)
    {
        err_msg("Error setting tty device");
        return COMM_ERROR;
    }
    tcflush(fds->tty_fd, TCIOFLUSH);
    if (fds->tty_fd >= 0) {
        flock(fds->tty_fd, LOCK_UN);
        close(fds->tty_fd);
    }
    else
        return COMM_ERROR;
    fds->tty_fd = open(profile->tty_dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    fds->fdi = fdopen(fds->tty_fd, "r");
    fds->fdo = fdopen(fds->tty_fd, "w");
    if (fds->fdi == NULL || fds->fdo == NULL)
    {
        err_msg("Error opening file descriptor");
        return COMM_ERROR;
    }

    if (flock(fds->tty_fd, LOCK_EX) < 0)
    {
        err_msg("Error locking tty device: %s is already in use", profile->tty_dev);
        close(fds->tty_fd);
        return COMM_ERROR;
    }

    if (setvbuf(fds->fdo , NULL, _IOFBF, 0))
    {
        err_msg("Error setting buffer for fdi");
        return COMM_ERROR;
    }

    if (setvbuf(fds->fdi , NULL, _IOLBF, 0))
    {
        err_msg("Error setting buffer for fdi");
        return COMM_ERROR;
    }
    return SUCCESS;
}

int tty_read(FILE *fdi, char *output, int len, int soft_timeout)
{
    return tty_read_keyword(fdi, output, len, NULL, soft_timeout);
}

int tty_read_keyword(FILE *fdi, char *output, int len, char *key_word, int soft_timeout)
{
    char tmp[LINE_BUF] = {0};
    int msg_len = 0;
    int read_flag = 0;
    time_t start_time = time(NULL);
    int exitcode = TIMEOUT_WAITING_NEWLINE;
    while (difftime(time(NULL), start_time) < soft_timeout)
    {      
        memset(tmp, 0, LINE_BUF);
        if (fgets(tmp, LINE_BUF, fdi))
        {
            read_flag = 1;
            dbg_msg("%s", tmp);
            if (output != NULL) 
                msg_len += snprintf(output + msg_len, len - msg_len, "%s", tmp);

            if (strncmp(tmp, "OK", 2) == 0 ||
                strncmp(tmp, "ERROR", 5) == 0 ||
                strncmp(tmp, "+CMS ERROR:", 11) == 0 ||
                strncmp(tmp, "+CME ERROR:", 11) == 0 ||
                strncmp(tmp, "NO CARRIER", 10) == 0 ||
                (key_word != NULL && strncmp(tmp, key_word, strlen(key_word)) == 0))
                {
                    if (key_word != NULL && strncmp(tmp, key_word, strlen(key_word)) == 0)
                    {
                        dbg_msg("keyword found");
                        exitcode = SUCCESS;
                    }
                    else if (key_word == NULL)
                    {
                        exitcode = SUCCESS;
                    }
                    else
                    {
                        exitcode = KEYWORD_NOT_MATCH;
                    }
                    break;
                }
        }
#ifdef EARLY_RETURN
        else
        {
            if (read_flag > 500){
                dbg_msg("early return");
                exitcode = TIMEOUT_WAITING_NEWLINE;
                break;
            }
            if (read_flag){
                read_flag++;
            }
        }
#endif
        usleep(10000);
    }
    if (read_flag == 0)
    {
        exitcode = COMM_ERROR;
    }
    return exitcode;
}

int tty_write_raw(FILE *fdo, char *input)
{
    int ret;
    ret = fputs(input, fdo);
    fflush(fdo);
    usleep(100);
    if (ret < 0)
    {
        err_msg("Error writing to tty %d" , ret);
        return COMM_ERROR;
    }
    return SUCCESS;
}

int tty_write(FILE *fdo, char *input)
{
    int cmd_len, ret;
    char *cmd_line;
    cmd_len = strlen(input) + 3;
    cmd_line = (char *)malloc(cmd_len);
    if (cmd_line == NULL)
    {
        err_msg("Error allocating memory");
        return COMM_ERROR;
    }
    snprintf(cmd_line, cmd_len, "%s\r\n", input);
    ret =  tty_write_raw(fdo, cmd_line);
    free(cmd_line);
    return ret;
}
