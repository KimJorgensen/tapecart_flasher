#include "commands.h"

static bool receive_debug_output(int fd)
{
    bool result = false;

    char rx;
    while(read_bytes(fd, &rx, 1))
    {
        result = true;

        fprintf(stderr, "%c", rx);
        if(rx == '\n')
        {
            break;
        }
    }

    return result;
}

static bool receive_command(int fd, CommandGroup group, uint8_t send_command, void *data = NULL, size_t max_data_size = 0)
{
    ReceiveCommandHeader header = {};

    while(read_bytes(fd, &header, sizeof(header)))
    {
        if(header.prefix == CommandPrefix_Debug)
        {
            // Print debug output from arduino
            fprintf(stderr, "%.*s", (int)sizeof(header), (uint8_t *)&header);
            receive_debug_output(fd);
        }
        else if(header.prefix == CommandPrefix_SOH)
        {
            if(header.length <= max_data_size)
            {
                uint8_t *buffer = (uint8_t *)data;
                if(read_bytes(fd, buffer, header.length))
                {
                    uint8_t checksum = 0;
                    if(read_bytes(fd, &checksum, 1))
                    {           
                        uint8_t calc_checksum = 0;
                        uint8_t *header_data = (uint8_t *)&header;

                        for(size_t i = 1; i < sizeof(header); i++)
                        {
                            calc_checksum ^= header_data[i];
                        }
                        for(size_t i = 0; i < header.length; i++)
                        {
                            calc_checksum ^= buffer[i];
                        }

                        if(calc_checksum == checksum)
                        {
                            if(header.group == group)
                            {
                                if(header.command == send_command)
                                {
                                    if(header.result == 0)
                                    {
                                        return true;
                                    }
                                    else if(header.result != 1)
                                    {
                                        fprintf(stderr, "Command failed with result 0x%02X\n", header.result);
                                    }
                                }
                                else
                                {
                                    fprintf(stderr, "Invalid command received %u, expected %u\n",
                                            header.command, send_command);
                                }
                            }
                            else
                            {
                                fprintf(stderr, "Invalid command group received %u, expected %u\n",
                                        header.group, group);
                            }
                        }
                        else
                        {
                            fprintf(stderr, "Invalid checksum %02x for received command, expected %02x\n", calc_checksum, checksum);
                        }
                    }
                    else
                    {
                        fprintf(stderr, "Failed to read checksum\n");
                    }
                }
                else
                {
                    fprintf(stderr, "Failed to read command data\n");
                }
            }
            else
            {
                fprintf(stderr, "Invalid command length received %u, expected max %u bytes\n",
                        header.length, (int)max_data_size);
            }

            break;
        }
        else
        {
            fprintf(stderr, "Invalid command received 0x%02X\n", header.prefix);
            break;
        }
    }

    return false;
}


static bool send_command(int fd, CommandGroup group, uint8_t send_command, void *data = NULL, size_t data_size = 0)
{
    SendCommandHeader header =
    {
        CommandPrefix_SOH,
        group,
        send_command,
        (uint16_t)data_size
    };

    uint8_t checksum = 0;
    uint8_t *header_data = (uint8_t *)&header;
    uint8_t *data_location = (uint8_t *)data;

    for(size_t i = 1; i < sizeof(header); i++)
    {
        checksum ^= header_data[i];
    }
    for(size_t i = 0; i < data_size; i++)
    {
        checksum ^= data_location[i];
    }

    bool result = false;
    discard_rx_buffer(fd);

    if(send_bytes(fd, &header, sizeof(header)))
    {
        result = true;
        bool waitForHandshake = data_size > 32;

        while(data_size > 0 && result)
        {
            size_t data_size_to_send = data_size > 32 ? 32 : data_size;
            if(send_bytes(fd, data_location, data_size_to_send))
            {
                data_location += data_size_to_send;
                data_size -= data_size_to_send;

                if(waitForHandshake)
                {
                    uint8_t rx;
                    while(read_bytes(fd, &rx, 1))
                    {
                        if(rx == CommandPrefix_Debug)
                        {
                            // Print debug output from arduino
                            fprintf(stderr, "*");
                            receive_debug_output(fd);
                        }
                        else if(rx == CommandPrefix_ENQ)
                        {
                            break;
                        }
                        else
                        {
                            result = false;
                            break;
                        }
                    }
                }
            }
            else
            {
                result = false;
            }
        }

        if(result)
        {
            result = send_bytes(fd, &checksum, sizeof(checksum));
        }
    }

    return result;
}

static bool send_arduino_command(int fd, ArduinoCommand command, void *rx_data = NULL, size_t rx_data_size  = 0)
{
    if(send_command(fd, CommandGroup_Arduino, command))
    {
        return receive_command(fd, CommandGroup_Arduino, command, rx_data, rx_data_size);
    }

    return false;
}

static bool send_tapecart_read_command(int fd, TapecartCommand command, void *rx_data, size_t rx_data_size)
{
    if(send_command(fd, CommandGroup_Tapecart, command))
    {
        return receive_command(fd, CommandGroup_Tapecart, command, rx_data, rx_data_size);
    }

    return false;
}

static bool send_tapecart_write_command(int fd, TapecartCommand command, void *rx_data, size_t rx_data_size)
{
    if(send_command(fd, CommandGroup_Tapecart, command, rx_data, rx_data_size))
    {
        return receive_command(fd, CommandGroup_Tapecart, command);
    }

    return false;
}

static bool send_tapecart_command(int fd, TapecartCommand command)
{
    return send_tapecart_read_command(fd, command, NULL, 0);
}

static bool get_sketch_version(int fd, ArduinoSketchVersion *version)
{
    return send_arduino_command(fd, ArduinoCommand_Version, version, sizeof(ArduinoSketchVersion));
}

static bool get_device_info(int fd, DeviceInfo *info)
{
    memset(info, 0, sizeof(DeviceInfo));    // NOTE: Make sure that string is null-terminated
    return send_tapecart_read_command(fd, TapecartCommand_ReadDeviceinfo, info, sizeof(DeviceInfo) - 1);
}

static bool get_device_sizes(int fd, DeviceSizes *sizes)
{
    return send_tapecart_read_command(fd, TapecartCommand_ReadDevicesizes, sizes, sizeof(DeviceSizes));
}

static bool read_loader(int fd, InitialLoader *loader)
{
    return send_tapecart_read_command(fd, TapecartCommand_ReadLoader, loader, sizeof(InitialLoader));
}

static bool write_loader(int fd, InitialLoader *loader)
{
    return send_tapecart_write_command(fd, TapecartCommand_WriteLoader, loader, sizeof(InitialLoader));
}

static bool read_loadinfo(int fd, Loadinfo *info)
{
    return send_tapecart_read_command(fd, TapecartCommand_ReadLoadinfo, info, sizeof(Loadinfo));
}

static bool write_loadinfo(int fd, Loadinfo *info)
{
    return send_tapecart_write_command(fd, TapecartCommand_WriteLoadinfo, info, sizeof(Loadinfo));
}

static bool read_flash(int fd, uint32_t start_address, uint16_t length, void *rx_data)
{
    assert(start_address <= 0xFFFFFF);
    assert(length <= 0x100);    // Arduino only support reading 256 bytes

    ReadFlash read_flash
    {
        start_address,
        length
    };

    if(send_command(fd, CommandGroup_Tapecart, TapecartCommand_ReadFlash, &read_flash, sizeof(read_flash)))
    {
        return receive_command(fd, CommandGroup_Tapecart, TapecartCommand_ReadFlash, rx_data, length);
    }

    return false;
}

static bool write_flash(int fd, WriteFlash *write_flash)
{
    return send_tapecart_write_command(fd, TapecartCommand_WriteFlash, write_flash, sizeof(*write_flash));
}

static bool erase_flash_block(int fd, uint32_t start_address)
{
    assert(start_address <= 0xFFFFFF);

    EraseFlash erase_flash
    {
        start_address
    };

    return send_tapecart_write_command(fd, TapecartCommand_EraseFlashBlock, &erase_flash, sizeof(erase_flash));
}

static bool crc32_flash(int fd, uint32_t start_address, uint32_t length, uint32_t *crc)
{
    assert(start_address <= 0xFFFFFF);
    assert(length <= 0xFFFFFF);

    ReadCrc32Flash read_crc32
    {
        start_address,
        length
    };

    if(send_command(fd, CommandGroup_Tapecart, TapecartCommand_Crc32Flash, &read_crc32, sizeof(read_crc32)))
    {
        return receive_command(fd, CommandGroup_Tapecart, TapecartCommand_Crc32Flash, crc, sizeof(*crc));
    }

    return false;
}
