#include "xml_parser.h"

struct PatTable     patTable;
struct PmtTable     pmtTable;
Pmt pmt[16]; 
void parseConfigFile(char *name, struct Config *config){
    long result;
    char *line = NULL;
    size_t len = 0;
    FILE *file;
    
    file = fopen(name,"r");
    
    while(getline(&line, &len, file) != -1){
        if(strstr(line, "bandwidth") != 0){
            while (*line) {
                if ( isdigit(*line))
                    result = strtol(line, &line, 10);
                else
                    line++;
            }
            line = NULL;
            config->bandwidth = result;
        }
        else if(strstr(line, "frequency") != 0){
            while (*line) {
                if ( isdigit(*line))
                    result = strtol(line, &line, 10);
                else
                    line++;
            }
            line = NULL;
            config->frequency = result;
        }
        else if(strstr(line, "module") != 0){
            if(strstr(line, "DVB-T") != 0)
                config->module = 0;
            else
                config->module = 1;
            line = NULL;
        }
        else if(strstr(line, "audio_pid") != 0){
            while (*line) {
                if ( isdigit(*line))
                    result = strtol(line, &line, 10);
                else
                    line++;
            }
            line = NULL;
            config->audio_pid = result;
        }
        else if(strstr(line, "video_pid") != 0){
            while (*line) {
                if ( isdigit(*line))
                    result = strtol(line, &line, 10);
                else
                    line++;
            }
            line = NULL;
            config->video_pid = result;
        }
                else if(strstr(line, "audio_type") != 0){
            while (*line) {
                if ( isdigit(*line))
                    result = strtol(line, &line, 10);
                else
                    line++;
            }
            line = NULL;
            config->audio_type = result;
        }
        else if(strstr(line, "video_type") != 0){
            while (*line) {
                if ( isdigit(*line))
                    result = strtol(line, &line, 10);
                else
                    line++;
            }
            line = NULL;
            config->video_type = result;
        }
    }

    if (NULL == file) {
        printf("file can't be opened \n");
    }

    fclose(file);
    return;    
}

int32_t parsePAT(uint8_t *buffer, struct ChannelInfo channelArray[]){

    
    patTable.section_length = (uint16_t)(((*(buffer+1)<<8) + *(buffer + 2)) & 0x0FFF);
    //printf("Length:%d ", patTable.section_length);

    patTable.transport_stream_id = (uint16_t)(((*(buffer+3)<<8) + *(buffer+4)));
    //printf("Transport stream ID:%d ", patTable.transport_stream_id);

    patTable.version_number = (uint8_t)((*(buffer+5)>>1 & 0x1F));
    //printf("Version Number:%d ", patTable.version_number);

    int broj_kanala = (patTable.section_length*8-64)/32;
    //printf("Broj Kanala :%d \n",broj_kanala);
    
    int i;
    for(i = 0; i<broj_kanala;i++){
        channelArray[i].program_number = (uint16_t)(*(buffer+8+(4*i))<<8)+(*(buffer+9+(4*i)));
        channelArray[i].pid = (uint16_t)((*(buffer+10+(i*4))<<8) + (*(buffer+11+(i*4))) & 0x1FFF);

        //printf("i : %d | p# : %d | pid : %d \n",i,channelArray[i].program_number,channelArray[i].pid);
    }
    
    //printf("****-EOPAT****\n");
    return 0;
}

uint16_t programNumberArray[7] = {0,0,0,0,0,0,0};
uint16_t videoPidCounter, audioPidCounter, programNumberCounter = 0;

int32_t parsePMT(uint8_t *buffer, int pmt_count, uint16_t audioPidArray[], uint16_t videoPidArray[]){
    pmt[pmt_count].section_length = (uint16_t) (((*(buffer+1) << 8) + *(buffer + 2)) & 0x0FFF);
    pmt[pmt_count].program_number = (uint16_t) (((*(buffer+3) << 8) + *(buffer + 4)));
    pmt[pmt_count].program_info_lenght = (uint16_t) (((*(buffer+10) << 8) + *(buffer + 11)) & 0x0FFF);
   // int stream_count=(uint8_t)(pmt[i].section_length - (12 + pmt[i].program_info_lenght + 4 - 3))/8;
    //printf("%d,%d,%d,%d\n",pmt[i].section_length,pmt[i].program_number,pmt[i].program_info_lenght, stream_count);
    //section_len-(12+program_info_len+4-3)/8
    uint8_t *m_buffer = (uint8_t*)(buffer) + 12 + pmt[pmt_count].program_info_lenght;
    int j;
    for (j= 0; j<10; j++){
            pmt[pmt_count].stream_type[j] = m_buffer[0];
            pmt[pmt_count].elemetary_pid[j] = (uint16_t) (((*(m_buffer+1)<<8) + *(m_buffer +2)) & 0x1FFF);
            pmt[pmt_count].ES_info_lenght[j] = (uint16_t) (((*(m_buffer+3)<<8) + *(m_buffer +4)) & 0x0FFF);
            //printf("program num:%d,elem_pid: %d,ES_info: %d\n",pmt[pmt_count].program_number,pmt[pmt_count].elemetary_pid[j],pmt[pmt_count].ES_info_lenght[j]);
            m_buffer += 5 + pmt[pmt_count].ES_info_lenght[j];
        
            if(pmt[pmt_count].stream_type[j] == 2){
                if(!valueinarray(pmt[pmt_count].elemetary_pid[j],videoPidArray,7))
                {
                    videoPidArray[videoPidCounter] = pmt[pmt_count].elemetary_pid[j];
                    //printf("Added videoPid: %d \n ", videoPidArray[videoPidCounter]);
                    videoPidCounter++;
                }
            }

            if(pmt[pmt_count].stream_type[j] == 3){
                 if(!valueinarray(pmt[pmt_count].program_number, programNumberArray,7))
                {
		            programNumberArray[programNumberCounter] = pmt[pmt_count].program_number;
		            programNumberCounter++;
                    audioPidArray[audioPidCounter] = pmt[pmt_count].elemetary_pid[j];
                    //printf("Added audioPid: %d \n ", audioPidArray[audioPidCounter]);
                    audioPidCounter++;
                }
            }
                    
                


            }
}