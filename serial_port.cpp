#include <termios.h>
#include <sys/ioctl.h>

static int open_serial_port(char *device)
{
    int fd = open_file(device, O_RDWR);

    if(fd != -1 && isatty(fd) == 0)
    {
        close(fd);
        fd = -1;
    }

    return fd;
}

static bool read_bytes(int fd, void *buffer, size_t size)
{
    return read_file(fd, buffer, size);
}

static bool send_bytes(int fd, void *buffer, size_t size)
{
    return write_file(fd, buffer, size);
}

static bool setup_serial_port(int fd)
{   
    bool result = false;

    termios tio = {};
    if(tcgetattr(fd, &tio) != -1)
    {
        tio.c_iflag &= ~(IGNBRK|BRKINT|ICRNL|INLCR|     // Turn off input processing
                         IGNCR|PARMRK|INPCK|ISTRIP|
                         IXON|IXOFF|IXANY);

        tio.c_oflag &= ~OPOST;                          // Turn off output processing

        tio.c_lflag &= ~(ECHO|ECHONL|ICANON|            // Raw mode
                         IEXTEN|ISIG);

        tio.c_cflag &= ~(CSIZE|PARENB|CRTSCTS|HUPCL);   // 8n1, no hardware flow control
        tio.c_cflag |= CS8|CSTOPB|CREAD|CLOCAL;

        tio.c_cc[VMIN] = 0;
        tio.c_cc[VTIME] = 30;                           // 3.0 seconds timeout

        if(cfsetospeed(&tio, B115200) != -1 &&          // 115200 baud
           cfsetispeed(&tio, B115200) != -1)
        {
            if(tcsetattr(fd, TCSANOW, &tio) != -1)
            {
                result = true;
            }
        }
    }

    return result;
}

static bool set_dtr(int fd, bool state)
{
    int result;
    unsigned long request = state ? TIOCMBIS : TIOCMBIC;
    int dtr_flag = TIOCM_DTR;

    while (true)
    {
        result = ioctl(fd, request, &dtr_flag);
        if(result != -1 || errno != EINTR)
        {
            break;
        }
    }

    return result != -1;
}

static void discard_rx_buffer(int fd)
{
    usleep(10000);   // Work-around for USB serial port drivers
    tcflush(fd, TCIOFLUSH);
}
