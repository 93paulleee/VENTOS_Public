/****************************************************************************/
/// @file    emit_eebl.c
/// @author  Mani Amoozadeh <maniam@ucdavis.edu>
/// @author
/// @date    Jun 2016
///
/****************************************************************************/
// @section LICENSE
//
// This software embodies materials and concepts that are confidential to Redpine
// Signals and is made available solely pursuant to the terms of a written license
// agreement with Redpine Signals
//

// check here: http://stackoverflow.com/questions/5378778/what-does-d-xopen-source-do-mean
#define _XOPEN_SOURCE 700
// check here: http://stackoverflow.com/questions/10053788/implicit-declaration-of-function-usleep
#define _BSD_SOURCE

#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <pthread.h>   // multi-threading
#include <stdbool.h>   // boolean

#include "rsi_wave_util.h"
#include "BSM_create.h"

// ARM OS supports 'System V IPC' only and not 'POSIX IPC'
#include <sys/ipc.h>
#include <sys/msg.h>

struct msgbuf
{
    long mtype;     /* message type, a positive number (cannot be zero). */
    struct msgbody
    {
        unsigned char buff[4096];
        int mlen;
    } mdata;
};

#define BUF_SIZ 1024

#define Signal_ForwardCollisionWarning  0xa4
#define Signal_EmergencyBreak           0xb4
#define Signal_EEBL                     0xc4

// forward declarations
void init_GPIO();
void sigint(int sigint);
int bsm_create(bsm_t *, char *, int *);

// forward declaration --VENTOS
int connectAppl2VENTOS(char *hostname, int remote_server_port, char *applName);
int sendIntegerToVENTOS(int n);
void recvFromVENTOS();
void LED_blink();
int disconnectVENTOS();

// global variables
int lsi = 0;
uint8 psid[4] = {0x20};
int no_of_tx = 0;
int gpio6 = 6;  // LED
int gpio9 = 9;  // Push bottom
int mq;  // message queue descriptor
pthread_t thread_id;

// global variables - bsm_message
blob_t *blob = NULL;
char *time_buf = NULL;
ddate_t *date = NULL;
initialPosition_t *initialPosition = NULL;
path_t *path = NULL;
pathPrediction_t *pathPrediction = NULL;
rTCMPackage_t *rTCMPackage = NULL;
vehiclesafetyExtension_t *vehiclesafetyextension = NULL;
bsm_t *bsm_message = NULL;
char *pay_load = NULL;


int main(void)
{
    signal(SIGINT, sigint);

    printf("===[ WAVE stack initialization ... ]=== \n\n");

    // initialize message queues to send requests to 1609 stack
    printf("Initializing Queue... ");
    int status = rsi_wavecombo_msgqueue_init();
    if(status == FAIL)
        return 1;

    // initialize the management information base (MIB) in 1609 stack
    printf("Initializing MIB... ");
    status = rsi_wavecombo_1609mib_init();
    if(status == FAIL)
        return 1;
    printf("Done! \n");

    // send channel synchronization parameters to Wave Combo Module
    printf("Calling update sync params... ");
    status = rsi_wavecombo_update_channel_sync_params(OPERATING_CLASS, CONTROL_CHANNEL, CCH_INTERVEL, SCH_INTERVEL, SYNC_TOLERANCE, MAX_SWITCH_TIME);
    if(status == FAIL)
        return 1;
    printf("Done! \n");

    //    printf("Setting UTC... ");
    //    status = rsi_wavecombo_set_utc();
    //    if(status == FAIL)
    //        return 1;
    //    printf("Done! \n");

    // get a local service index for this user from 1609 stack
    lsi = rsi_wavecombo_local_service_index_request();
    if(lsi <= 0)
        return 1;

    // initialize message queue to receive wsm packets from 1609 stack
    status = rsi_wavecombo_wsmp_queue_init(lsi);
    if(status == FAIL)
        return 1;

    // indicating that a higher layer entity requests a short message service
    printf("Sending WSMP service request... ");
    status = rsi_wavecombo_wsmp_service_req(ADD, lsi, psid);
    if(status == FAIL)
        return 1;
    printf("Done! \n");

    // request stack to allocate radio resources to the indicated service channel
    printf("Sending SCH service request... ");
    status = rsi_wavecombo_sch_start_req(172, RATE_6, 1, 255);
    if(status == FAIL)
        return 1;
    printf("Done! \n");

    printf("\n===[ GPIO initialization ... ]=== \n\n");

    init_GPIO();

    printf("\n===[ Establish bi-directional connection with VENTOS ... ]=== \n\n");

    // connect this application to VENTOS
    mq = connectAppl2VENTOS("192.168.60.30" /*ip address of VENTOS*/,
            34676 /*port number VENTOS is listening to*/,
            "application_1" /*application name*/);

    // if successful, create a thread that handles received data from VENTOS
    if(mq != -1)
        pthread_create(&thread_id, NULL, recvFromVENTOS, NULL);

    printf("\n===[ Generating BSM message ... ]=== \n\n");

    blob = malloc(sizeof(blob_t));
    if(!blob)
    {
        perror("malloc");
        return 1;
    }
    memset(blob, 0, sizeof(sizeof(blob_t)));

    blob->MsgCnt = 0;
    blob->Id = 0x32;
    blob->SecMark = 0x01;
    blob->Lat = 0xbc4d4f21;
    blob->Long = 0xcb39edc1;
    blob->Elev = 0xcb;
    blob->Accuracy = 0;
    blob->Heading = 0;
    blob->AccelLong = 0;
    blob->AccelLatSet = 0;
    blob->AccelVert = 0;
    blob->AccelYaw = 0;
    blob->Brakes = 1;
    blob->VehicleWidth_MSB = 0x24;
    blob->VehicleLength_WidthLSB = 0x18;

    time_buf = malloc(50);
    if(!time_buf)
    {
        perror("malloc");
        return 1;
    }
    memset(time_buf, 0, 50);

    date = malloc(sizeof(ddate_t));
    if(!date)
    {
        perror("malloc");
        return 1;
    }
    memset(date,0,sizeof(ddate_t));

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t current_time = tv.tv_sec;
    strftime(time_buf, 50, "%Y %d %m %H %M %S", localtime(&current_time));
    sscanf(time_buf, "%ld %ld %ld %ld %ld %ld", &date->year, &date->day, &date->month, &date->hour, &date->minute, &date->second);

    initialPosition = malloc(sizeof(initialPosition_t));
    if(!initialPosition)
    {
        perror("malloc");
        return 1;
    }
    memset(initialPosition, 0, sizeof(initialPosition_t));

    initialPosition->utcTime    = date;
    initialPosition->Long       = 0xbc4d4f21;
    initialPosition->lat        = 0xcb39edc1;
    initialPosition->elevation_size = 0x02;
    initialPosition->elevation[0]   = 0x5b;
    initialPosition->elevation[1]   = 0x5b;
    initialPosition->heading    = 0xffff;
    initialPosition->timeConfidence = 3;
    initialPosition->posConfidence  = 3;
    initialPosition->speedConfidence= 3;
    initialPosition->elevation_size = 1;
    initialPosition->elevation[0] = 0x02;
    initialPosition->speed_size = 0x01;
    initialPosition->speed[0] = 0x03;
    initialPosition->posAccuracy_size = 0x01;
    initialPosition->posAccuracy[0] = 0x03;

    path = malloc(sizeof(path_t));
    if(!path)
    {
        perror("malloc");
        return 1;
    }
    memset(path,0,sizeof(path_t));

    path->crumbData.present = 1;
    path->crumbData.pathHistoryPointSets_01_count = 1;
    path->crumbData.pathHistoryPointSets_01[0].latOffset = 0x05;
    path->crumbData.pathHistoryPointSets_01[0].longOffset = 0x04;
    path->crumbData.pathHistoryPointSets_01[0].elevationOffset = 0x00;
    path->crumbData.pathHistoryPointSets_01[0].timeOffset = 0x0;
    path->crumbData.pathHistoryPointSets_01[0].posAccuracy_size = 0x02;
    path->crumbData.pathHistoryPointSets_01[0].posAccuracy[0] = 0x00;
    path->crumbData.pathHistoryPointSets_01[0].posAccuracy[1] = 0x00;
    path->crumbData.pathHistoryPointSets_01[0].heading = 0x01;
    path->crumbData.pathHistoryPointSets_01[0].speed_size = 0x02;
    path->crumbData.pathHistoryPointSets_01[0].speed[0] = 0x01;
    path->crumbData.pathHistoryPointSets_01[0].speed[1] = 0x02;
    path->crumbData.data_size = 0x01;
    path->crumbData.data[0] = 0x01;
    path->crumbData.data[1] = 0x02;
    path->crumbData.data[2] = 0x03;

    pathPrediction = malloc(sizeof(pathPrediction_t));
    if(!pathPrediction)
    {
        perror("malloc");
        return 1;
    }
    memset(pathPrediction,0,sizeof(pathPrediction_t));

    pathPrediction->radiusOfCurve = 0x3;
    pathPrediction->confidence = 0x3;

    rTCMPackage = malloc(sizeof(rTCMPackage_t));
    if(!rTCMPackage)
    {
        perror("malloc");
        return 1;
    }
    memset(rTCMPackage, 0, sizeof(rTCMPackage_t));

    rTCMPackage->anchorPoint = initialPosition;
    rTCMPackage->rtcmHeader_size = 0x1;
    rTCMPackage->rtcmHeader[0] = 0x42;
    rTCMPackage->msg1001_size = 0x01;
    rTCMPackage->msg1001[0] = 0x01;

    vehiclesafetyextension = malloc(sizeof(vehiclesafetyExtension_t));
    if(!vehiclesafetyextension)
    {
        perror("malloc");
        return 1;
    }
    memset(vehiclesafetyextension, 0, sizeof(vehiclesafetyExtension_t));

    vehiclesafetyextension->events = 128;
    vehiclesafetyextension->path = path;
    vehiclesafetyextension->pathPrediction = pathPrediction;
    vehiclesafetyextension->theRTCM = NULL; //rTCMPackage;

    bsm_message = malloc(sizeof(bsm_t));
    if(!bsm_message)
    {
        perror("malloc");
        return 1;
    }
    memset(bsm_message, 0, sizeof(sizeof(bsm_t)));

    bsm_message->blob = (char *) blob;
    bsm_message->extention = vehiclesafetyextension;

    printf("\n===[ Start polling the switch status ... ]=== \n\n");

    pay_load = malloc(512);
    if(!pay_load)
    {
        perror("malloc");
        return 1;
    }
    memset(pay_load, 0, 512);

    char status_buf[80] = {0};
    sprintf(status_buf, "/sys/class/gpio/gpio%d/value", gpio9);

    while(1)
    {
        char switch_status = '1';

        // keep polling switch until pressed -- not pressed: '1' or 0x31, pressed: '0' or 0x30
        while(switch_status != '0')
        {
            // we need to open this every time!
            int fd = open(status_buf, O_RDONLY);
            if(fd == -1)
            {
                perror("open:value");
                return 1;
            }
            read(fd, &switch_status, 1);
            close(fd);
        }

        // send 'emergency break signal'
        printf("\nSending 'Emergency Break' signal to VENTOS. \n");
        sendIntegerToVENTOS(Signal_EmergencyBreak);

        blob->MsgCnt = ++no_of_tx;
        int pay_load_len = 0;
        if(bsm_create(bsm_message, pay_load, &pay_load_len) != SUCCESS)
        {
            printf("BSM message encoding failed. \n");
            return 1;
        }

        waveShortMessage *wsm = malloc(sizeof(waveShortMessage));
        if(!wsm)
        {
            perror("malloc");
            return 1;
        }
        memset(wsm, 0, sizeof(waveShortMessage));

        wsm->dataRate     = RATE_6;
        wsm->txPwr        = 15;
        wsm->psid[0]      = psid[0];
        wsm->psid[1]      = psid[1];
        wsm->psid[2]      = psid[2];
        wsm->psid[3]      = psid[3];
        wsm->priority     = 3;
        wsm->wsm_expiry_time = 50;
        wsm->wsm_length = pay_load_len;
        wsm->channelNumber = 172;
        memcpy(wsm->WSM_Data, pay_load, pay_load_len);
        char peer_mac_address[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
        memcpy(wsm->peer_mac_address, peer_mac_address, 6);

        printf("Sending BSM msg #%3d of size %d... ", no_of_tx, pay_load_len);

        status = rsi_wavecombo_wsmp_msg_send(wsm);
        if(status < 0)
            printf("Failed! \n");
        else
            printf("Done! \n");

        free(wsm);
        sleep(1);
    }

    return 0;
}


void init_GPIO()
{
    char status_buf[80] = {0};

    printf("Exporting the GPIO6 pin... ");
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if(fd == -1)
    {
        perror("open:export");
        exit (1);
    }
    sprintf(status_buf, "%d", gpio6);
    write(fd, status_buf, strlen(status_buf));
    close(fd);
    printf("Done! \n");

    printf("Feeding direction 'out' to GPIO6... ");
    sprintf(status_buf, "/sys/class/gpio/gpio%d/direction", gpio6);
    fd = open(status_buf, O_WRONLY);
    if(fd == -1)
    {
        perror("open:direction");
        exit (1);
    }
    write(fd, "out", 3);
    close(fd);
    printf("Done! \n");

    // -------------------------------------------

    printf("Exporting the GPIO9 pin... ");
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if(fd == -1)
    {
        perror("open:export");
        exit (1);
    }
    sprintf(status_buf, "%d", gpio9);
    write(fd, status_buf, strlen(status_buf));
    close(fd);
    printf("Done! \n");

    printf("Feeding direction 'in' to GPIO9... ");
    sprintf(status_buf, "/sys/class/gpio/gpio%d/direction", gpio9);
    fd = open(status_buf, O_WRONLY);
    if(fd == -1)
    {
        perror("open:direction");
        exit (1);
    }
    write(fd, "in", 2);
    close(fd);
    printf("Done! \n");
}


void recvFromVENTOS()
{
    while(true)
    {
        struct msgbuf recv_msg;
        int rc = msgrcv(mq, &recv_msg, sizeof(recv_msg.mdata), 0, 0);
        if (rc == -1)
        {
            perror("ERROR in msgrcv");
            exit(1);
        }

        if(recv_msg.mdata.mlen == 1 && recv_msg.mdata.buff[0] == Signal_ForwardCollisionWarning)
        {
            // send emergency break signal to the car
            sendIntegerToVENTOS(Signal_EmergencyBreak);

            LED_blink();
        }
        else if(recv_msg.mdata.mlen == 1 && recv_msg.mdata.buff[0] == Signal_EEBL)
        {
            // send emergency break signal to the car
            sendIntegerToVENTOS(Signal_EmergencyBreak);

            LED_blink();
        }
    }
}


void LED_blink()
{
    int i = 0;
    for(i = 1; i <= 5; i++)
    {
        system("echo 1 > /sys/class/gpio/gpio6/value");
        usleep(500000);  // 0.5 second

        system("echo 0 > /sys/class/gpio/gpio6/value");
        usleep(500000);  // 0.5 second
    }
}


void sigint(int signum)
{
    printf("\nTotal Tx: %d \n", no_of_tx);

    if(pay_load)
        free(pay_load);

    if(blob)
        free(blob);

    if(time_buf)
        free(time_buf);

    if(date)
        free(date);

    if(initialPosition)
        free(initialPosition);

    if(rTCMPackage)
        free(rTCMPackage);

    if(path)
        free(path);

    if(pathPrediction)
        free(pathPrediction);

    if(vehiclesafetyextension)
        free(vehiclesafetyextension);

    if(bsm_message)
        free(bsm_message);

    // unexporting gpio6
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if(fd == -1)
        perror("open");
    char status_buf[80] = {0};
    sprintf(status_buf, "%d", gpio6);
    write(fd, status_buf, strlen(status_buf));
    close(fd);

    // unexporting gpio9
    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if(fd == -1)
        perror("open");
    sprintf(status_buf, "%d", gpio9);
    write(fd, status_buf, strlen(status_buf));
    close(fd);

    rsi_wavecombo_wsmp_service_req(DELETE , lsi, psid);
    rsi_wavecombo_msgqueue_deinit();

    disconnectVENTOS();

    exit(0);
}
