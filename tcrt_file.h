#define TCRT_FILE_SIGNATURE "tapecartImage\015\012\032"
#define TCRT_VERSION 1

enum MiscFlags : uint8_t
{
    MiscFlags_None =                    0x00,
    MiscFlags_InitialLoaderValid =      0x01,
    MiscFlags_DataBlockOffsetsSupport = 0x02
};

#pragma pack(push)
#pragma pack(1)
struct  TcrtHeader
{
    uint8_t file_signature[16];
    uint16_t version_number;

    Loadinfo loadinfo;

    MiscFlags misc_flags;
    InitialLoader initial_loader;

    uint32_t flash_content_length;
};
#pragma pack(pop)
