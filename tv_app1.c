#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "errno.h"
#include "tdp_api.h"
#include "xml_parser.h"

#include <string.h>
#include <stdint.h>
#include <directfb.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>


#define CONFIG_FILE "config.xml"

#define NO_ERROR 		0
#define ERROR			1
#define NUM_EVENTS  5

static int32_t inputFileDesc;
int channel = 0;
int volume = 50;
static inline void textColor(int32_t attr, int32_t fg, int32_t bg)
{
   char command[13];

   sprintf(command, "%c[%d;%d;%dm", 0x1B, attr, fg + 30, bg + 40);
   printf("%s", command);
}


#define ASSERT_TDP_RESULT(x,y)  if(NO_ERROR == x) \
                                    printf("%s success\n", y); \
                                else{ \
                                    textColor(1,1,0); \
                                    printf("%s fail\n", y); \
                                    textColor(0,7,0); \
                                    return -1; \
                                }


static pthread_cond_t statusCondition = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t statusMutex = PTHREAD_MUTEX_INITIALIZER;

static int32_t tunerStatusCallback(t_LockStatus status);
int32_t mySecFilterCallback(uint8_t *buffer);

int32_t remoteMainFunction();
int32_t getKey(int32_t count, uint8_t* buf, int32_t* eventsRead);
static void *remoteThreadTask();

/*******************GRAFIKA SECTION*******************/

int32_t drawPressedButton();
void clear_screen_notify();
void clear_screen();

/// STVARI ZA TIMER
timer_t timerId;
int32_t timerFlags = 0;
struct itimerspec timerSpec;
struct itimerspec timerSpecOld;

/// STVARI ZA INTERFACE
static IDirectFBSurface *primary = NULL;
IDirectFB *dfbInterface = NULL;
static int screenWidth = 0;
static int screenHeight = 0;
DFBSurfaceDescription surfaceDesc;
///

IDirectFBImageProvider *provider;
IDirectFBSurface *logoSurface = NULL;
int32_t logoHeight, logoWidth;

//// STVARI ZA OK
void volumeManager();
void drawVolume(int boxesNumber);

void timer_init();
void timer_deinit();
void timer_reset();







#define DFBCHECK(x...)                                      \
{                                                           \
DFBResult err = x;                                          \
                                                            \
if (err != DFB_OK)                                          \
  {                                                         \
    fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );  \
    DirectFBErrorFatal( #x, err );                          \
  }                                                         \
}




/*****************************************************/


int pmt_count = 0;

struct Stream       stream;
struct ChannelInfo  channelArray[8];

Config config;
static pthread_t remote;
int main_running = 1;





uint32_t playerHandle = 0;
uint32_t sourceHandle = 0;
uint32_t filterHandle = 0;
    
uint32_t audioStreamHandle = 0;
uint32_t videoStreamHandle = 0;

bool valueinarray(uint16_t val, uint16_t arr[], size_t n);
uint16_t videoPidArray[7] = {0,0,0,0,0,0,0};
uint16_t audioPidArray[7] = {0,0,0,0,0,0,0};


void Stream(int channel){
    Player_Stream_Remove(playerHandle,sourceHandle,videoStreamHandle);
    Player_Stream_Remove(playerHandle,sourceHandle,audioStreamHandle);
    Player_Stream_Create(playerHandle,sourceHandle,videoPidArray[channel-1],config.video_type,&videoStreamHandle);
    Player_Stream_Create(playerHandle,sourceHandle,audioPidArray[channel-1],config.audio_type,&audioStreamHandle);   
    drawPressedButton();
}

int main(int32_t argc, char** argv)
{

    parseConfigFile(CONFIG_FILE, &config);
    
    int result;   
	struct timespec lockStatusWaitTime;
	struct timeval now;
    
    gettimeofday(&now,NULL);
    lockStatusWaitTime.tv_sec = now.tv_sec+10;

    
    result = Tuner_Init();
    ASSERT_TDP_RESULT(result, "Tuner_Init");

    result = Tuner_Register_Status_Callback(tunerStatusCallback);
    ASSERT_TDP_RESULT(result, "Tuner_Register_Status_Callback");

    result = Tuner_Lock_To_Frequency(config.frequency, config.bandwidth, config.module);
    ASSERT_TDP_RESULT(result, "Tuner_Lock_To_Frequency");
    

    pthread_mutex_lock(&statusMutex);
    if(ETIMEDOUT == pthread_cond_timedwait(&statusCondition, &statusMutex, &lockStatusWaitTime))
    {
        printf("\n%s:ERROR Lock timeout exceeded!\n",__FUNCTION__);
        Tuner_Deinit();
        return -1;
    }
    pthread_mutex_unlock(&statusMutex);


    result = Player_Init(&playerHandle);
    ASSERT_TDP_RESULT(result, "Player_Init");

    result = Player_Source_Open(playerHandle, &sourceHandle);
    ASSERT_TDP_RESULT(result, "Player_Source_Open");
    printf("TEST\n");

    result = Demux_Register_Section_Filter_Callback(mySecFilterCallback);
    ASSERT_TDP_RESULT(result, "Demux_Register_Section_Filter_Callback");

    pmt_count++;

    result = Demux_Set_Filter(playerHandle, 0x0000, 0x00, &filterHandle);
    ASSERT_TDP_RESULT(result, "Demux_Set_Filter");

    sleep(1);

    int i;

    for(i=1;i<8;i++){
        Demux_Free_Filter(playerHandle, filterHandle);
        Demux_Set_Filter(playerHandle, channelArray[i].pid, 0x02, &filterHandle);
        sleep(2);
    };
    Demux_Free_Filter(playerHandle, filterHandle);
    Demux_Unregister_Section_Filter_Callback(mySecFilterCallback);


    timer_init();
    /* initialize DirectFB */   
	DFBCHECK(DirectFBInit(&argc, &argv));
    /* fetch the DirectFB interface */
	DFBCHECK(DirectFBCreate(&dfbInterface));
    /* tell the DirectFB to take the full screen for this application */
	DFBCHECK(dfbInterface->SetCooperativeLevel(dfbInterface, DFSCL_FULLSCREEN));
    /* create primary surface with double buffering enabled */
	surfaceDesc.flags = DSDESC_CAPS;
	surfaceDesc.caps = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
	DFBCHECK (dfbInterface->CreateSurface(dfbInterface, &surfaceDesc, &primary)); 
    /* fetch the screen size */
    DFBCHECK (primary->GetSize(primary, &screenWidth, &screenHeight));    
    /* clear the screen before drawing anything (draw black full screen rectangle)*/
    //DFBCHECK(primary->SetColor(primary,0x00,0x00,0x00,0xff));
	//DFBCHECK(primary->FillRectangle(/*surface to draw on*/ primary,0,0,screenWidth,screenHeight));
    /* draw text */
	IDirectFBFont *fontInterface = NULL;
	DFBFontDescription fontDesc;
    /* specify the height of the font by raising the appropriate flag and setting the height value */
	fontDesc.flags = DFDESC_HEIGHT;
	fontDesc.height = 48;
    /* create the font and set the created font for primary surface text drawing */
	DFBCHECK(dfbInterface->CreateFont(dfbInterface, "/home/galois/fonts/DejaVuSans.ttf", &fontDesc, &fontInterface));
	DFBCHECK(primary->SetFont(primary, fontInterface));
    /* switch between the displayed and the work buffer (update the display) */
	DFBCHECK(primary->Flip(primary,
                       /*region to be updated, NULL for the whole surface*/NULL,
                       /*flip flags*/0));

    
    Player_Stream_Create(playerHandle,sourceHandle,config.video_pid,config.video_type,&videoStreamHandle);
    Player_Stream_Create(playerHandle,sourceHandle,config.audio_pid,config.audio_type,&audioStreamHandle);
    
    pthread_create(&remote, NULL, &remoteThreadTask, NULL);
    int count;
    for(count=0;count<7;count++){ printf("audioPidArray[{%d}]:{%d} \n",count,audioPidArray[count]);}
    while(main_running);

    Player_Stream_Remove(playerHandle,sourceHandle,videoStreamHandle);
    Player_Stream_Remove(playerHandle,sourceHandle,audioStreamHandle);
    Player_Source_Close(playerHandle, sourceHandle);

    primary->Release(primary);
	dfbInterface->Release(dfbInterface);
    //  timer_deinit2();
    timer_deinit();
    //pthread_join(remote, NULL);
    Player_Deinit(playerHandle);
    Tuner_Deinit();

    return 0;
}

int32_t tunerStatusCallback(t_LockStatus status)
{
    if(status == STATUS_LOCKED)
    {
        pthread_mutex_lock(&statusMutex);
        pthread_cond_signal(&statusCondition);
        pthread_mutex_unlock(&statusMutex);
        printf("\n%s -----TUNER LOCKED-----\n",__FUNCTION__);
    }
    else
    {
        printf("\n%s -----TUNER NOT LOCKED-----\n",__FUNCTION__);
    }
    return 0;
}

int32_t mySecFilterCallback(uint8_t *buffer)
{   
    uint16_t table_type = *(buffer);
    if(table_type==0x00) parsePAT(buffer, channelArray);
    if(table_type==0x02) parsePMT(buffer, pmt_count, audioPidArray, videoPidArray);
    return 0;
}

void timer_init(){
    struct sigevent signalEvent;
    signalEvent.sigev_notify = SIGEV_THREAD;
    signalEvent.sigev_notify_function = clear_screen_notify;
    signalEvent.sigev_value.sival_ptr = NULL;
    signalEvent.sigev_notify_attributes = NULL;
    timer_create(CLOCK_REALTIME,&signalEvent,&timerId);
    
    memset(&timerSpec,0,sizeof(timerSpec));
    timerSpec.it_value.tv_sec = 5;
    timerSpec.it_value.tv_nsec = 0;

}

void timer_deinit(){
    memset(&timerSpec,0,sizeof(timerSpec));
    timer_settime(timerId, timerFlags,&timerSpec,&timerSpecOld);
    timer_delete(timerId);

}

void timer_reset(){
    timer_settime(timerId, timerFlags,&timerSpec,&timerSpecOld);
}

int32_t getKeys(int32_t count, uint8_t* buf, int32_t* eventsRead)
{
    int32_t ret = 0;
    
    /* read input events and put them in buffer */
    ret = read(inputFileDesc, buf, (size_t)(count * (int)sizeof(struct input_event)));
    if(ret <= 0)
    {
        printf("Error code %d", ret);
        return ERROR;
    }
    /* calculate number of read events */
    *eventsRead = ret / (int)sizeof(struct input_event);
    
    return NO_ERROR;
}




bool valueinarray(uint16_t val, uint16_t arr[], size_t n) {
    int i;
    for(i = 0; i < n; i++) {
        if(arr[i] == val)
            return true;
    }
    return false;
}
static void *remoteThreadTask() {
    

    uint32_t i;
    const char* dev = "/dev/input/event0";
    char deviceName[20];
    struct input_event* eventBuf;
    uint32_t eventCnt;
    inputFileDesc = open(dev, O_RDWR);
    
    if(inputFileDesc == -1)
    {
        printf("Error while opening device (%s) !", strerror(errno));
	    //return ERROR;
    }


    eventBuf = malloc(NUM_EVENTS * sizeof(struct input_event));
    if(!eventBuf)
    {
        printf("Error allocating memory !");
        //return ERROR;
    }

    ioctl(inputFileDesc, EVIOCGNAME(sizeof(deviceName)), deviceName);
	printf("RC device opened succesfully [%s]\n", deviceName);
    
    int running = 1;
    while(running)
    {
        /* read input eventS */
        if(getKeys(NUM_EVENTS, (uint8_t*)eventBuf, &eventCnt))
        {
			printf("Error while reading input events !");
			//return ERROR;
		}
 
        for(i = 0; i < eventCnt; i++)
        {
            /*
            printf("Event time: %d sec, %d usec\n",(int)eventBuf[i].time.tv_sec,(int)eventBuf[i].time.tv_usec);
            printf("Event type: %hu\n",eventBuf[i].type);
            printf("Event code: %hu\n",eventBuf[i].code);
            printf("Event value: %d\n",eventBuf[i].value);
            printf("\n");
            */
    
        if (eventBuf[i].type == 1 && (eventBuf[i].value == 1 || eventBuf[i].value ==2)) {
            switch(eventBuf[i].code) {
                

                case 2:{
                    channel= 1;
                    printf("Kanal: %d\n", channel);


                    Stream(channel);

                    
                    break;
                }
                 
                case 3:{
                    channel = 2;
                    printf("Kanal: %d\n", channel);

                    Stream(channel);
                    break;
                }

                case 4:{
                    channel = 3;
                    printf("Kanal: %d\n", channel);

                    Stream(channel);
                    break;
                }
                
                case 5:{
                    channel = 4;
                    printf("Kanal: %d\n", channel);

                    Stream(channel);
                    break;
                }

                case 6:{
                    channel = 5;
                    printf("Kanal: %d\n", channel);

                    Stream(channel);
                    break;
                }
                
                case 7:{
                    channel = 6;
                    printf("Kanal: %d\n", channel);

                    Stream(channel);
                    break;
                }

                case 8:{
                    channel = 7;
                    printf("Kanal: %d\n", channel);

                    Stream(channel);
                    break;
                }
                
                case 9:{
                    channel = 8;
                    printf("Kanal: %d\n", channel);

                    Stream(channel);
                    break;
                }

                case 10:{
                    channel = 9;
                    printf("Kanal: %d\n", channel);

                    Stream(channel);
                    break;
                }

                
                case 61: {
                    if(channel>1){channel--;printf("Kanal: %d\n", channel);

                    Stream(channel);
                    } 
                    break;
                    
                }
                case 62: {
                    if(channel<9){channel++;printf("Kanal: %d\n", channel);

                    Stream(channel);
                    }
                    break;
                }

                case 64: {
                    if(volume>1){volume--;printf("Volume minus \n");
                    drawPressedButton();
                    } 
                    break;
                    
                }
                case 63: {
                    if(volume<100){volume++;printf("Volume plus \n");
                    drawPressedButton();
                    }
                    break;
                }

                case 102:{
                    printf("Exit\n");
                    running=0;
                    main_running=0;
                    break;
                }
            }
        }
        }
    }
    
    free(eventBuf);
    return;
    //return NO_ERROR;
}

void clear_screen(){
    primary->SetColor(primary, 0x00,0x00,0x00,0x00);
	primary->FillRectangle(primary,0,0,screenWidth,screenHeight);
}

void clear_screen_notify(){
    clear_screen();
    primary->Flip(primary,NULL,0);
}




int32_t drawPressedButton(){


    //DFBCHECK(primary->SetColor(primary, 0x00,0x00,0x00,0xff));
	//DFBCHECK(primary->FillRectangle(primary,0,0,screenWidth,screenHeight));
   
/* 	DFBCHECK(dfbInterface->CreateImageProvider(dfbInterface, "krug.png", &provider));
	DFBCHECK(provider->GetSurfaceDescription(provider, &surfaceDesc));
	DFBCHECK(dfbInterface->CreateSurface(dfbInterface, &surfaceDesc, &logoSurface));
	DFBCHECK(provider->RenderTo(provider, logoSurface, NULL));
	provider->Release(provider);
	DFBCHECK(logoSurface->GetSize(logoSurface, &logoWidth, &logoHeight));
	DFBCHECK(primary->Blit(primary, logoSurface, */
                     //      /*source region, NULL to blit the whole surface*/ NULL,
                   //        /*destination x coordinate of the upper left corner of the image*/125,
             //              /*destination y coordinate of the upper left corner of the image*/25));


    

    volumeManager();


    DFBCHECK(primary->Flip(primary,NULL,0));
    timer_reset();
    
    return 0;
}

void volumeManager(){

    //x_start i y_start oznacavaju gornji lijevi rub najvišeg volume boxa
    int x_start = screenWidth/10 * 9;
    int y_start  = 200;

	DFBCHECK(primary->SetColor(primary, 34, 139, 34, 0xFF));
	DFBCHECK(primary->FillRectangle(primary,x_start-1650,y_start+450,screenWidth-150,400));
    //Crta sivu pozadinu
    DFBCHECK(primary->SetColor(primary, 0xff,0xff,0xff,0xff));
    DFBCHECK(primary->FillRectangle(primary,x_start-1650,y_start-100,screenWidth/10-100,410));

        char channel_str[2];
    sprintf( channel_str, "%d", channel );
    
    
    char *ch = "Channel ";
    DFBCHECK(primary->SetColor(primary, 0xff,0xff,0xff,0xff));
   
    DFBCHECK(primary->DrawString(primary,ch,7,screenWidth-1700,750, DSTF_CENTER));

    DFBCHECK(primary->DrawString(primary,channel_str,1,screenWidth-1550,750,DSTF_CENTER));
    
    //prvi pomiče u lijevo, drugi gore, treće koliko široko + je za šire
    //Crni ispis vrijednosti volumea
/*     char volume_str[3];
    sprintf( volume_str, "%d", volume );
    DFBCHECK(primary->SetColor(primary, 0xff,0xff,0xff,0xff));
    if(volume==100){
        DFBCHECK(primary->DrawString(primary,volume_str,3,x_start-1600,y_start-150, DSTF_CENTER));
    }
    else if(volume<10){
        DFBCHECK(primary->DrawString(primary,volume_str,1,x_start-1650,y_start-120, DSTF_CENTER));
    }
    else{
        DFBCHECK(primary->DrawString(primary,volume_str,2,x_start-1600,y_start-150, DSTF_CENTER));
    } */
    
    if(volume==100){
        drawVolume(10);
    }
    if(volume<100&&volume>90){
        drawVolume(9);
    }
    if(volume<=90&&volume>80){
        drawVolume(8);
    }
    if(volume<=80&&volume>70){
        drawVolume(7);
    }
    if(volume<=70&&volume>60){
        drawVolume(6);
    }
    if(volume<=60&&volume>50){
        drawVolume(5);
    }
    if(volume<=50&&volume>40){
        drawVolume(4);
    }
    if(volume<=40&&volume>30){
        drawVolume(3);
    }
    if(volume<=30&&volume>20){
        drawVolume(2);
    }
    if(volume<=20&&volume>10){
        drawVolume(1);
    }
    if(volume<=10){
        drawVolume(11);
        
    }
    

}

void drawVolume(int boxesNumber){

    Player_Volume_Set(playerHandle,volume*10000000);
    int x_start = screenWidth/10 * 9;
    int y_start  = 200;
    int difference = 40;
    int i = 0;

    //nacrtaj 10-x blijedih i x punih - Ako je 11, crta 10 praznih
    if(boxesNumber==11){
            for(i=0;i<10;i++){
            //DFBCHECK(primary->SetColor(primary, 34, 139, 34, 0xFF));
            DFBCHECK(primary->FillRectangle(primary,x_start-1645,y_start-90+i*difference,80,25));
        }
    }
    else
    {
        DFBCHECK(primary->SetColor(primary, 128, 128, 128, 0xFF));
        for(i = 0; i<10-boxesNumber; i++){
            DFBCHECK(primary->FillRectangle(primary,x_start-1645,y_start-90+i*difference,80,25));
        }
        DFBCHECK(primary->SetColor(primary, 34, 139, 34, 0xFF));
        for(i = 10-boxesNumber; i<10; i++ )
        {
            DFBCHECK(primary->FillRectangle(primary,x_start-1645,y_start-90+i*difference,80,25));
        }
    }
    

}
