#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "tdp_api.h"

typedef struct Config{
    uint32_t frequency;
    uint32_t bandwidth;
    int module;
    uint32_t audio_pid;
    uint32_t video_pid;
    uint32_t audio_type;
    uint32_t video_type;
    char time[10];
    char date[10];
} Config;

struct Stream
{
    uint8_t stream_type;
    uint16_t elementary_pid;
    uint16_t ES_info_length;
};

struct PmtTable
{
    uint16_t section_length;
    uint16_t program_number;
    uint16_t program_info_lenght;
    uint8_t stream_count;
    struct Stream streamArray[16];

};

struct PatTable
{
    uint16_t section_length;
    uint16_t transport_stream_id;
    uint8_t version_number;

};

struct ChannelInfo{
    uint16_t program_number;
    uint16_t pid;
};

typedef struct{
    uint16_t section_length;
    uint16_t program_info_lenght;
    uint16_t program_number;
    uint16_t stream_type[15];
    uint16_t elemetary_pid[15];
    uint16_t ES_info_lenght[15];
}Pmt;



void parseConfigFile(char *name, struct Config *config);
int32_t parsePMT(uint8_t *buffer, int pmt_count, uint16_t audioPidArray[], uint16_t videoPidArray[]);
int32_t parsePAT(uint8_t *buffer, struct ChannelInfo channelArray[]);
