#pragma pack(1)

typedef struct Display_info{
    uint8_t bss_id[6];
    int power;
    int beacons;
    int channel;
    struct Display_info* next;
    char* essid;
} Display_info;

typedef struct{
	uint8_t version;
	uint8_t pad;
	uint16_t len;
    uint32_t* present;
} Radio_tap_header;

typedef struct {
    Radio_tap_header header;
    int8_t power;
} Radio_tap;

#define MAX_PAYLOAD_SIZE 2312

typedef struct {
    uint16_t frame_control;
    uint16_t duration_id;
    uint8_t da[6];
    uint8_t sa[6];
    uint8_t bss_id[6];
    uint16_t sequence_control;
} MACHeader;

typedef struct {
    uint64_t timestamp;
    uint16_t beacon_interval;
    uint16_t capacity_info;
    uint8_t tag_name;
    uint8_t tag_len;
    uint8_t data[1024];
} FrameBody;

typedef struct {
    MACHeader header;
    FrameBody body;
} Beacon_Frame;
#pragma pack()
