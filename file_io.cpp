#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

static int open_file(char *pathname, int flags, mode_t mode = 0)
{
    int fd;

    while(true)
    {
        fd = open(pathname, flags, mode);
        if(fd != -1 || errno != EINTR)
        {
            break;
        }
    }

    return fd;
}

static bool read_file(int fd, void *buffer, size_t size)
{
    ssize_t total_bytes_read = 0;
    uint8_t *buffer_location = (uint8_t*)buffer;

    while(total_bytes_read < size)
    {
        ssize_t bytes_read = read(fd, buffer_location, size - total_bytes_read);
        if(bytes_read > 0)
        {
            total_bytes_read += bytes_read;
            buffer_location += bytes_read;
        }
        else if(bytes_read != -1 || errno != EINTR)
        {
            break;
        }
    }

    return total_bytes_read == size;
}

static bool write_file(int fd, void *buffer, size_t size)
{
    ssize_t total_bytes_written = 0;
    uint8_t *buffer_location = (uint8_t*)buffer;

    while(total_bytes_written < size)
    {
        ssize_t bytes_written = write(fd, buffer_location, size - total_bytes_written);
        if(bytes_written > 0)
        {
            total_bytes_written += bytes_written;
            buffer_location += bytes_written;
        }
        else if(bytes_written != -1 || errno != EINTR)
        {
            break;
        }
    }

    return total_bytes_written == size;
}
