#define SUPPORTED_API_VERSION 2

enum CommandPrefix : uint8_t
{
    CommandPrefix_SOH = 0x01,
    CommandPrefix_ENQ = 0x05,
    CommandPrefix_Debug = '*'
};

enum CommandGroup : uint8_t
{
    CommandGroup_Arduino = 0x01,
    CommandGroup_Tapecart = 0x02
};

enum ArduinoCommand : uint8_t
{
    ArduinoCommand_Version = 0x01,
    ArduinoCommand_StartCommandMode = 0x02
};

enum TapecartCommand : uint8_t
{
    TapecartCommand_Exit = 0,
    TapecartCommand_ReadDeviceinfo,
    TapecartCommand_ReadDevicesizes,
    TapecartCommand_ReadCapabilities,

    TapecartCommand_ReadFlash = 0x10,
    TapecartCommand_ReadFlashFast,
    TapecartCommand_WriteFlash,
    TapecartCommand_WriteFlashFast,
    TapecartCommand_EraseFlash64K,
    TapecartCommand_EraseFlashBlock,
    TapecartCommand_Crc32Flash,

    TapecartCommand_ReadLoader = 0x20,
    TapecartCommand_ReadLoadinfo,
    TapecartCommand_WriteLoader,
    TapecartCommand_WriteLoadinfo,

    TapecartCommand_LedOff = 0x30,
    TapecartCommand_LedOn,
    TapecartCommand_ReadDebugflags,
    TapecartCommand_WriteDebugflags,

    TapecartCommand_DirSetparams = 0x40,
    TapecartCommand_DirLookup
};

enum CommandResult : uint8_t
{
    CommandResult_Ok = 0x00,
    CommandResult_Error = 0x01,
    CommandResult_NotImplemented = 0x02,
    CommandResult_ChecksumError = 0x03,
};

#pragma pack(push)
#pragma pack(1)
struct SendCommandHeader
{
    CommandPrefix prefix;
    CommandGroup group;
    uint8_t command;
    uint16_t length;    // NOTE: Assume we compile on little endian
};

struct ReceiveCommandHeader
{
    CommandPrefix prefix;
    CommandGroup group;
    uint8_t command;
    CommandResult result;

    uint16_t length;
};

struct ReceiveCommand
{
    ReceiveCommandHeader header;
    void *data;
    uint8_t checksum;
};

enum ArduinoType : uint8_t
{
    ArduinoType_None =      0x00,
    ArduinoType_Uno =       0x01,
    ArduinoType_Nano =      0x02,
    ArduinoType_Mega2560 =  0x03
};

struct ArduinoSketchVersion
{
    uint8_t minor_version;
    uint8_t major_version;
    uint8_t api_version;
    ArduinoType arduino_type;
};

struct DeviceInfo
{
    char str[33];   // NOTE: 32 characters + null-terminator
};

struct DeviceSizes
{
    uint32_t total_size : 24;   // NOTE: 24 bits
    uint16_t page_size;
    uint16_t erase_pages;
};

struct InitialLoader
{
    uint8_t data[171];
};

struct Loadinfo
{
    uint16_t data_address;
    uint16_t data_length;
    uint16_t call_address;
    char filename[16];
};

struct EraseFlash
{
    uint32_t start_address : 24;
};

struct ReadFlash
{
    uint32_t start_address : 24;
    uint16_t length;
};

struct WriteFlash
{
    uint32_t start_address : 24;
    uint16_t length;
    uint8_t data[0x100];
};

struct ReadCrc32Flash
{
    uint32_t start_address : 24;
    uint32_t length : 24;
};
#pragma pack(pop)
