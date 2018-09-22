#include <termios.h>
#include <sys/ioctl.h>

static int open_serial_port(char *device)
{
    return open_file(device, O_RDWR);
}

static bool read_bytes(int fd, void *buffer, size_t size)
{
    return read_file(fd, buffer, size);
}

static bool send_bytes(int fd, void *buffer, size_t size)
{
    return write_file(fd, buffer, size);
}

static void setup_serial_port(int fd)
{
    termios tio = {};

    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_cflag= CS8|CREAD|CLOCAL;  // 8n1
    tio.c_lflag = 0;                // Raw mode
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 30;           // 3.0 seconds timeout

    cfsetospeed(&tio, B115200);     // 115200 baud
    cfsetispeed(&tio, B115200);     // 115200 baud
    tcsetattr(fd, TCSANOW, &tio);
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
