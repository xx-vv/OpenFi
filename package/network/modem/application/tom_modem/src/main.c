
#include "main.h"
#include <sys/file.h>

FDS_T s_fds;
PROFILE_T s_profile;   // global profile     

int parse_user_input(int argc, char *argv[], PROFILE_T *profile)
{
    int opt = 1;
    int option;
    profile->sms_index = -1;
#define has_more_argv() (opt < argc ? 1 : 0)
    while (opt < argc)
    {
        option = match_option(argv[opt]);
        if (option == -1)
        {
            usage(argv[0]);
            return INVALID_PARAM;
        }
        opt++;
        switch (option)
        {
        case AT_CMD:
            if (!has_more_argv())
            {
                usage(argv[0]);
                return INVALID_PARAM;
            }
            profile->at_cmd = argv[opt++];
            break;
        case TTY_DEV:
            if (!has_more_argv())
            {
                usage(argv[0]);
                return INVALID_PARAM;
            }
            profile->tty_dev = argv[opt++];
            break;
        case BAUD_RATE:
            if (!has_more_argv())
            {
                usage(argv[0]);
                return INVALID_PARAM;
            }
            profile->baud_rate = atoi(argv[opt++]);
            break;
        case DATA_BITS:
            if (!has_more_argv())
            {
                usage(argv[0]);
                return INVALID_PARAM;
            }
            profile->data_bits = atoi(argv[opt++]);
            break;
        case PARITY:
            if (!has_more_argv())
            {
                usage(argv[0]);
                return INVALID_PARAM;
            }
            profile->parity = argv[opt++];
            break;
        case STOP_BITS:
            if (!has_more_argv())
            {
                usage(argv[0]);
                return INVALID_PARAM;
            }
            profile->stop_bits = atoi(argv[opt++]);
            break;
        case FLOW_CONTROL:
            if (!has_more_argv())
            {
                usage(argv[0]);
                return INVALID_PARAM;
            }
            profile->flow_control = argv[opt++];
            break;
        case TIMEOUT:
            if (!has_more_argv())
            {
                usage(argv[0]);
                return INVALID_PARAM;
            }
            profile->timeout = atoi(argv[opt++]);
            break;
        case OPERATION:
            if (!has_more_argv())
            {
                usage(argv[0]);
                return INVALID_PARAM;
            }
            profile->op = match_operation(argv[opt++]);
            break;
        case DEBUG:
            profile->debug = 1;
            break;
        case SMS_PDU:
            if (!has_more_argv())
            {
                usage(argv[0]);
                return INVALID_PARAM;
            }
            profile->sms_pdu = argv[opt++];
            break;
        case SMS_INDEX:
            if (!has_more_argv())
            {
                usage(argv[0]);
                return INVALID_PARAM;
            }
            profile->sms_index = atoi(argv[opt++]);
            break;
        default:
            err_msg("Invalid option: %s", argv[opt]);
            break;
        }
    }
    // default settings:
    if (profile->baud_rate == 0 )
    {
        profile->baud_rate = 115200;
    }
    if (profile->data_bits == 0)
    {
        profile->data_bits = 8;
    }
    if (profile->timeout == 0)
    {
        profile->timeout = 3;
    }
    if (profile->op == 0 || profile->op == -1)
    {
        profile->op = AT_OP;
    }
    return SUCCESS;
}
int run_op(PROFILE_T *profile,FDS_T *fds)
{
    switch (profile->op)
    {
    case AT_OP:
        return at(profile,fds);
    case BINARY_AT_OP:
        return binary_at(profile,fds);
    case SMS_READ_OP:
        return sms_read(profile,fds);
    case SMS_SEND_OP:
        return sms_send(profile,fds);
    case SMS_DELETE_OP:
        return sms_delete(profile,fds);
    default:
        err_msg("Invalid operation");
    }
    return UNKNOWN_ERROR;
}
static void clean_up()
{
    if (tcsetattr(s_fds.tty_fd, TCSANOW, &s_fds.old_termios) != 0)
    {
        err_msg("Error restoring old tty attributes");
        return;
    }
    dbg_msg("Clean up success");
    tcflush(s_fds.tty_fd, TCIOFLUSH);
    if (s_fds.tty_fd >= 0) {
        flock(s_fds.tty_fd, LOCK_UN);
        close(s_fds.tty_fd);
    }
}

int main(int argc, char *argv[])
{
    PROFILE_T *profile = &s_profile;
    FDS_T *fds = &s_fds;
    parse_user_input(argc, argv, profile);
    dump_profile();
    // try open tty devices
    if (tty_open_device(profile,fds))
    {
        err_msg("Failed to open tty device");
        return COMM_ERROR;
    }
    atexit(clean_up);
    if (run_op(profile,fds))
    {
        err_msg("Failed to run operation %d", profile->op);
        kill(getpid(), SIGINT); 
    }
    
    dbg_msg("Exit");
    return SUCCESS;
}
