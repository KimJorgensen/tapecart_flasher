#include "tcrt_file.h"

// Adapted from crc32b - http://www.hackersdelight.org/hdcodetxt/crc.c.txt
static uint32_t calculate_crc32(void *data, size_t size)
{
   uint8_t *bytes = (uint8_t *)data;
   uint32_t crc = 0xFFFFFFFF;

   while(size--)
   {
      crc ^= *bytes++;
      for(uint8_t i=0; i<8; i++)
      {
         crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
      }
   }

   return ~crc;
}

static bool validate_tcrt_signature(TcrtHeader *header)
{
     if(memcmp(header->file_signature, TCRT_FILE_SIGNATURE, sizeof(header->file_signature)) == 0)
     {
         if(header->version_number == TCRT_VERSION)
         {
             return true;
         }
     }

     return false;
}

static bool read_tcrt_header(int fd, TcrtHeader *header)
{
    bool result = false;

    memcpy(header->file_signature, TCRT_FILE_SIGNATURE, sizeof(header->file_signature));
    header->version_number = TCRT_VERSION;

    if(read_loadinfo(fd, &header->loadinfo))
    {
        if(read_loader(fd, &header->initial_loader))
        {
            header->misc_flags = MiscFlags_InitialLoaderValid;

            DeviceSizes device_sizes;
            if(get_device_sizes(fd, &device_sizes))
            {
                header->flash_content_length = device_sizes.total_size;
                result = true;
            }
            else
            {
                fprintf(stderr, "Failed to read device sizes from Tapecart\n");
            }
        }
        else
        {
            fprintf(stderr, "Failed to read loader from Tapecart\n");
        }
    }
    else
    {
        fprintf(stderr, "Failed to read loadinfo from Tapecart\n");
    }

    return result;
}

static ssize_t flash_tcrt_header(int fd, TcrtHeader *header)
{
    bool result = false;

    printf("Writing loadinfo\n");
    if(write_loadinfo(fd, &header->loadinfo))
    {
        if(header->misc_flags & MiscFlags_InitialLoaderValid)
        {
            printf("Writing initial loader\n");
            if(write_loader(fd, &header->initial_loader))
            {
                result = true;
            }
            else
            {
                fprintf(stderr, "Failed to write loader to Tapecart\n");
            }
        }
        else
        {
            result = true;
            printf("No initial loader in file\n");
        }
    }
    else
    {
        fprintf(stderr, "Failed to write loadinfo to Tapecart\n");
    }

    return result;
}

static bool dump_tcrt_to_file(int fd, int file)
{
    bool result = false;

    TcrtHeader header = {};
    if(read_tcrt_header(fd, &header))
    {
        if(write_file(file, &header, sizeof(header)))
        {
            result = true;
            uint8_t buffer[0x100];
            uint32_t buffer_size = sizeof(buffer);

            for(uint32_t i = 0; i < header.flash_content_length; i += buffer_size)
            {
                if(read_flash(fd, i, buffer_size, buffer))
                {
                    if(write_file(file, buffer, buffer_size))
                    {
                        double percent = (100.0 / header.flash_content_length) * (i + buffer_size);
                        printf("\rReading %u bytes from flash [%.1f%%] ", header.flash_content_length, percent);
                        fflush(stdout);
                    }
                    else
                    {
                        fprintf(stderr, "Failed to write data to file. %s\n", strerror(errno));
                        result = false;
                        break;
                    }
                }
                else
                {
                    fprintf(stderr, "Failed to read from flash address %06x\n", i);
                    result = false;
                    break;
                }
            }

            printf("\n");
        }
        else
        {
            fprintf(stderr, "Failed to write header to file. %s\n", strerror(errno));
        }
    }

    return result;
}

static bool flash_tcrt_file(int fd, int file)
{
    bool result = false;

    TcrtHeader header = {};
    if(read_file(file, &header, sizeof(header)))
    {
        if(validate_tcrt_signature(&header))
        {
            if(flash_tcrt_header(fd, &header))
            {
                DeviceSizes device_sizes;
                if(get_device_sizes(fd, &device_sizes))
                {
                    result = true;
                    uint32_t flash_block_size = device_sizes.page_size * device_sizes.erase_pages;

                    WriteFlash write_flash_data = {};
                    write_flash_data.length = sizeof(write_flash_data.data);

                    for(uint32_t i = 0; i < header.flash_content_length; i += write_flash_data.length)
                    {
                        if(read_file(file, &write_flash_data.data, write_flash_data.length))
                        {
                            if(flash_block_size && (i % flash_block_size) == 0)
                            {
                                if(!erase_flash_block(fd, i))
                                {
                                    fprintf(stderr, "Failed to erase flash block at address %06x", i);
                                    result = false;
                                    break;
                                }
                            }

                            write_flash_data.start_address = i;
                            if(write_flash(fd, &write_flash_data))
                            {
                                double percent = (100.0 / header.flash_content_length) * (i + write_flash_data.length);
                                printf("\rWriting %u bytes to flash [%.1f%%] ", header.flash_content_length, percent);
                                fflush(stdout);
                            }
                            else
                            {
                                fprintf(stderr, "Failed to write to flash address %06x\n", i);
                                result = false;
                                break;
                            }
                        }
                        else
                        {
                            fprintf(stderr, "Failed to read data from file. %s\n", strerror(errno));
                            result = false;
                            break;
                        }
                    }

                    printf("\n");
                }
                else
                {
                    fprintf(stderr, "Failed to read device sizes from Tapecart\n");
                }
            }
        }
        else
        {
            fprintf(stderr, "Invalid TCRT file\n");
        }
    }
    else
    {
        fprintf(stderr, "Failed to load TCRT file. %s\n", strerror(errno));
    }

    return result;
}

static bool validate_tcrt_header(int fd, int file, TcrtHeader *fileHeader)
{
    bool result = false;

    if(read_file(file, fileHeader, sizeof(*fileHeader)))
    {
        if(validate_tcrt_signature(fileHeader))
        {
            TcrtHeader header = {};
            if(read_tcrt_header(fd, &header))
            {
                if(memcmp(&fileHeader->loadinfo, &header.loadinfo, sizeof(header.loadinfo)) == 0)
                {
                    if((fileHeader->misc_flags & MiscFlags_InitialLoaderValid) == 0 ||
                       memcmp(&fileHeader->initial_loader, &header.initial_loader, sizeof(header.initial_loader)) == 0)
                    {
                        result = true;
                    }
                    else
                    {
                        fprintf(stderr, "Initial loader does not match\n");
                    }
                }
                else
                {
                    fprintf(stderr, "Loadinfo does not match\n");
                }
            }
        }
        else
        {
            fprintf(stderr, "Invalid TCRT file\n");
        }
    }
    else
    {
        fprintf(stderr, "Failed to load TCRT file. %s\n", strerror(errno));
    }

    return result;
}

static bool validate_tcrt_file(int fd, int file)
{
    bool result = false;

    TcrtHeader header = {};
    if(validate_tcrt_header(fd, file, &header))
    {
        DeviceSizes device_sizes;
        if(get_device_sizes(fd, &device_sizes))
        {
            result = true;
            uint32_t flash_block_size = device_sizes.page_size * device_sizes.erase_pages;
            uint32_t buffer_size = flash_block_size ? flash_block_size : 4*1024;
            uint8_t *buffer = (uint8_t *)malloc(buffer_size);

            for(uint32_t i = 0; i < header.flash_content_length; i += buffer_size)
            {
                if(read_file(file, buffer, buffer_size))
                {
                    uint32_t file_crc32 = calculate_crc32(buffer, buffer_size);

                    uint32_t flash_crc32;
                    if(crc32_flash(fd, i, buffer_size, &flash_crc32))
                    {
                        if(file_crc32 == flash_crc32)
                        {
                            double percent = (100.0 / header.flash_content_length) * (i + buffer_size);
                            printf("\rValidating %u bytes [%.1f%%] ", header.flash_content_length, percent);
                            fflush(stdout);
                        }
                        else
                        {
                            fprintf(stderr, "CRC32 check failed for flash block at address %06x\n", i);
                            result = false;
                            break;
                        }
                    }
                    else
                    {
                        fprintf(stderr, "Failed to get CRC32 for flash block at address %06x\n", i);
                        result = false;
                        break;
                    }
                }
                else
                {
                    fprintf(stderr, "Failed to read data from file. %s\n", strerror(errno));
                    result = false;
                    break;
                }
            }

            free(buffer);

            if(result)
            {
                printf("\nTCRT file matches Tapecart flash\n");
            }
        }
        else
        {
            fprintf(stderr, "Failed to read device sizes from Tapecart\n");
        }
    }

    return result;
}
