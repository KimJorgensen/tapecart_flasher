#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include "file_io.cpp"
#include "serial_port.cpp"
#include "commands.cpp"
#include "tcrt_file.cpp"

static char *filename;

static bool init_tapecart(int fd, bool print_sketch_version)
{
    bool result = false;

    ArduinoSketchVersion sketch_version;
    if(get_sketch_version(fd, &sketch_version))
    {
        if(sketch_version.api_version < SUPPORTED_API_VERSION)
        {
            fprintf(stderr, "Warning: Sketch uses old API v%u, newest supported is v%u\n",
                    sketch_version.api_version, SUPPORTED_API_VERSION);
        }
        else if(sketch_version.api_version > SUPPORTED_API_VERSION)
        {
            fprintf(stderr, "Warning: Sketch uses unknown API v%u, newest supported is v%u\n",
                    sketch_version.api_version, SUPPORTED_API_VERSION);
        }

        if(print_sketch_version)
        {
            printf("Arduino type %u Sketch v%u.%u/%u\n", sketch_version.arduino_type,
                   sketch_version.major_version, sketch_version.minor_version, sketch_version.api_version);
        }

        if(send_arduino_command(fd, ArduinoCommand_StartCommandMode))
        {
            result = true;
        }
        else
        {
            fprintf(stderr, "Failed to connect to Tapecart\n");
        }
    }
    else
    {
        fprintf(stderr, "Failed to connect to Arduino\n");
    }

    return result;
}

static bool info_command(int fd)
{
    bool result = false;
    DeviceInfo device_info;

    if(get_device_info(fd, &device_info))
    {
        printf("Tapecart:\n");
        printf("    Device       %s\n", device_info.str);

        DeviceSizes device_sizes;
        if(get_device_sizes(fd, &device_sizes))
        {
            printf("    Flash size   %u\n", device_sizes.total_size);
            printf("    Page size    %u\n", device_sizes.page_size);
            printf("    Erase pages  %u\n", device_sizes.erase_pages);

            Loadinfo loadinfo;
            if(read_loadinfo(fd, &loadinfo))
            {
                printf("Load info:\n");
                printf("    Data address $%04x\n", loadinfo.data_address);
                printf("    Data length  $%04x\n", loadinfo.data_length);
                printf("    Call address $%04x\n", loadinfo.call_address);
                printf("    Filename     %.*s\n", (int)sizeof(loadinfo.filename), loadinfo.filename);
                result = true;
            }
            else
            {
                fprintf(stderr, "Failed to read loadinfo from Tapecart\n");
            }
        }
        else
        {
            fprintf(stderr, "Failed to read device sizes from Tapecart\n");
        }
    }
    else
    {
        fprintf(stderr, "Failed to read device info from Tapecart\n");
    }

    return result;
}

static bool reset_command(int fd)
{
    if(set_dtr(fd, false))
    {
        usleep(10000);
        set_dtr(fd, true);

        printf("Arduino reset\n");

        while(receive_debug_output(fd));
        return true;
    }

    fprintf(stderr, "Failed to reset Arduino\n");
    return false;
}

static bool led_on_command(int fd)
{
    if(send_tapecart_command(fd, TapecartCommand_LedOn))
    {
        printf("LED is on\n");
        return true;
    }

    fprintf(stderr, "Failed to turn on LED on Tapecart\n");
    return false;
}

static bool led_off_command(int fd)
{
    if(send_tapecart_command(fd, TapecartCommand_LedOff))
    {
        printf("LED is off\n");
        return true;
    }

    fprintf(stderr, "Failed to turn off LED on Tapecart\n");
    return false;
}

static bool dump_tcrt_command(int fd)
{
    bool result = false;

    int file = open_file(filename, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(file != -1)
    {
        result = dump_tcrt_to_file(fd, file);
        close(file);
    }
    else
    {
        fprintf(stderr, "Failed to open %s. %s\n", filename, strerror(errno));
    }

    return result;
}

static bool flash_tcrt_command(int fd)
{
    bool result = false;    

    int file = open_file(filename, O_RDONLY);
    if(file != -1)
    {
        result = flash_tcrt_file(fd, file);
        close(file);
    }
    else
    {
        fprintf(stderr, "Failed to open %s. %s\n", filename, strerror(errno));
    }

    return result;
}

static bool validate_tcrt_command(int fd)
{
    bool result = false;

    int file = open_file(filename, O_RDONLY);
    if(file != -1)
    {
        result = validate_tcrt_file(fd, file);
        close(file);
    }
    else
    {
        fprintf(stderr, "Failed to open %s. %s\n", filename, strerror(errno));
    }

    return result;
}

int main(int argc, char** argv)
{
    bool (*command)(int) = NULL;
    bool skip_init_tapecart = false;
    bool print_sketch_version = false;

    if(argc == 3)
    {       
        if(strcmp(argv[2], "info") == 0)
        {
            command = info_command;
            print_sketch_version = true;
        }
        else if(strcmp(argv[2], "reset") == 0)
        {
            command = reset_command;
            skip_init_tapecart = true;
        }
    }
    else if(argc == 4)
    {
        if(strcmp(argv[2], "led") == 0)
        {
            if(strcmp(argv[3], "on") == 0)
            {
                command = led_on_command;
            }
            else if(strcmp(argv[3], "off") == 0)
            {
                command = led_off_command;
            }
        }
        else if(strcmp(argv[2], "dump") == 0)
        {
            command = dump_tcrt_command;
            filename = argv[3];
        }
        else if(strcmp(argv[2], "flash") == 0)
        {
            command = flash_tcrt_command;
            filename = argv[3];
        }
        else if(strcmp(argv[2], "validate") == 0)
        {
            command = validate_tcrt_command;
            filename = argv[3];
        }
    }

    int result = EXIT_FAILURE;
    if(command)
    {
        int fd = open_serial_port(argv[1]);
        if(fd != -1)
        {
            if(setup_serial_port(fd))
            {
                if(skip_init_tapecart || init_tapecart(fd, print_sketch_version))
                {
                    if(command(fd))
                    {
                        result = EXIT_SUCCESS;
                    }
                }
            }
            else
            {
                fprintf(stderr, "Failed to setup serial port. %s\n", strerror(errno));
            }

            close(fd);
        }
        else
        {
            fprintf(stderr, "Failed to open %s. %s\n", argv[1], strerror(errno));
        }
    }
    else
    {
        fprintf(stderr, "Tapecart Flasher v0.2\n");
        fprintf(stderr, "Usage: %s <tty device> <command>\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "    info\n");
        fprintf(stderr, "    reset\n");
        fprintf(stderr, "    led {on|off}\n");
        fprintf(stderr, "    dump <out.tcrt>\n");
        fprintf(stderr, "    flash <file.tcrt>\n");
        fprintf(stderr, "    validate <file.tcrt>\n");
        fprintf(stderr, "Example: \n");
        fprintf(stderr, "  %s /dev/ttyACM0 info\n", argv[0]);
    }

    return result;
}
