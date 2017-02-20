/**
 *  Simplest Librtmp Send FLV
 *
 *  本程序用于将FLV格式的视音频文件使用RTMP推送至RTMP流媒体服务器。
 *  This program can send local flv file to net server as a rtmp live stream.
 *  gcc -o publish_flv publish_flv.c -g -DNO_CRYPTO -I/usr/local/include -lrtmp
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"

#define FLV_KEY_FRAME  0x17
#define PACKET_SIZE    64*1024
#define HTON16(x)   ((x>>8&0xff)|(x<<8&0xff00))
#define HTON24(x)   ((x>>16&0xff)|(x<<16&0xff0000)|(x&0xff00))
#define HTON32(x)   ((x>>24&0xff)|(x>>8&0xff00)|(x<<8&0xff0000)|(x<<24&0xff000000))
#define HTONTIME(x) ((x>>16&0xff)|(x<<16&0xff0000)|(x&0xff00)|(x&0xff000000))

int ReadU8(uint32_t *u8,FILE*fp){
    if(fread(u8,1,1,fp)!=1)
        return 0;
    return 1;
}

int ReadU16(uint32_t *u16,FILE*fp){
    if(fread(u16,2,1,fp)!=1)
        return 0;
    *u16=HTON16(*u16);
    return 1;
}

int ReadU24(uint32_t *u24,FILE*fp){
    if(fread(u24,3,1,fp)!=1)
        return 0;
    *u24=HTON24(*u24);
    return 1;
}

int ReadU32(uint32_t *u32,FILE*fp){
    if(fread(u32,4,1,fp)!=1)
        return 0;
    *u32=HTON32(*u32);
    return 1;
}

/*read 1 byte,and loopback 1 byte at once*/
int PeekU8(uint32_t *u8,FILE*fp){
    if(fread(u8,1,1,fp)!=1)
        return 0;
    fseek(fp,-1,SEEK_CUR);
    return 1;
}

/*read 4 byte and convert to time format*/
int ReadTime(uint32_t *utime,FILE*fp){
    if(fread(utime,4,1,fp)!=1)
        return 0;
    *utime=HTONTIME(*utime);
    return 1;
}

int RTMP_Connect_Target(RTMP *r, RTMPPacket *cp, char *webserver, int port)
{
    struct sockaddr_in service;
    if (!r->Link.hostname.av_len)
        return FALSE;

    memset(&service, 0, sizeof(struct sockaddr_in));
    service.sin_family = AF_INET;

    service.sin_addr.s_addr = inet_addr(webserver);
    if (service.sin_addr.s_addr == INADDR_NONE) {
        return FALSE;
    }

    service.sin_port = htons(port);

    if (!RTMP_Connect0(r, (struct sockaddr *)&service)) {
        return FALSE;
    }

    r->m_bSendCounter = TRUE;

    return RTMP_Connect1(r, cp);
}

//publish local flv file to remote platform
int publish_flv(char* flvfile, char* rtmpurl, char* webserver, int port, int timeout) {
    int      ret            =  0;
    int      keyframe       =  1;
    uint32_t last_time      =  0;
    uint32_t start_time     =  0;
    uint32_t curr_time      =  0;
    uint32_t deadline       =  0;
    uint32_t prevframe_time =  0;
    uint32_t preTagsize     =  0;

    //packet attributes
    uint32_t type           =  0;
    uint32_t streamid       =  0;
    uint32_t timestamp      =  0;
    uint32_t datalength     =  0;

    RTMP     rtmp           = { 0 };
    FILE*    fp             =  NULL;
    RTMPPacket* packet      =  NULL;

    if (!flvfile) {
        return -1;
    }

    fp=fopen(flvfile, "rb");
    if (!fp){
        RTMP_LogPrintf("Open File Error.\n");
        return -1;
    }

    //RTMP_LogSetLevel(RTMP_LOGDEBUG);

    RTMP_Init(&rtmp);

    //set connection timeout,default 30s
    //rtmp.Link.timeout = timeout;
    if(!RTMP_SetupURL(&rtmp, rtmpurl))
    {
        return -1;
    }

    //if unable,the AMF command would be 'play' instead of 'publish'
    RTMP_EnableWrite(&rtmp);

#if 0
    if (!RTMP_Connect_Target(&rtmp, NULL, webserver, port)){
        RTMP_Log(RTMP_LOGERROR,"Connect Err\n");
        return -1;
    }
#else
    if (!RTMP_Connect(&rtmp, NULL)){
        RTMP_Log(RTMP_LOGERROR,"Connect Err\n");
        return -1;
    }

#endif
    if (!RTMP_ConnectStream(&rtmp, 0)){
        RTMP_Log(RTMP_LOGERROR,"ConnectStream Err\n");
        RTMP_Close(&rtmp);
        return -1;
    }

    packet = (RTMPPacket*)calloc(1, sizeof(RTMPPacket));
    if (!packet) {
        RTMP_Log(RTMP_LOGERROR,"Malloc Packet err\n");
        RTMP_Close(&rtmp);
        return -1;
    }
    RTMPPacket_Alloc(packet, PACKET_SIZE);
    RTMPPacket_Reset(packet);

    packet->m_hasAbsTimestamp = 0;
    packet->m_nChannel = 0x04;
    packet->m_nInfoField2 = rtmp.m_stream_id;

    RTMP_LogPrintf("Start to send data ...\n");

    //jump over FLV Header
    fseek(fp,9,SEEK_SET);
    //jump over previousTagSizen
    fseek(fp,4,SEEK_CUR);

    start_time = RTMP_GetTime();
    deadline   = start_time + timeout * 1000;
    while(1)
    {
        curr_time = RTMP_GetTime();
        if (curr_time > deadline) {
            break;
        }

        printf("curr :%d, start:%d, prev: %d\n", curr_time, start_time, prevframe_time);
        if(( (curr_time - start_time) < prevframe_time) && keyframe){
            //wait for 1 sec if the send process is too fast
            //this mechanism is not very good,need some improvement
            if(prevframe_time > last_time){
                last_time = prevframe_time;
            }

            printf("wait to sleep------\n");
            sleep(1);
            continue;
        }

        //not quite the same as FLV spec
        if (!ReadU8(&type, fp)){
            break;
        }
        if (!ReadU24(&datalength, fp)){
            break;
        }
        if (!ReadTime(&timestamp, fp)){
            break;
        }
        if (!ReadU24(&streamid, fp)){
            break;
        }

        if (type!=0x08&&type!=0x09){
            //jump over non_audio and non_video frame，
            //jump over next previousTagSizen at the same time
            fseek(fp, datalength+4, SEEK_CUR);
            continue;
        }

        if (fread(packet->m_body, 1, datalength, fp) != datalength) {
            break;
        }

        packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
        packet->m_nTimeStamp = timestamp;
        packet->m_packetType = type;
        packet->m_nBodySize  = datalength;
        prevframe_time = timestamp;
        printf("PREV FRAME_ TIME %d\n", timestamp);

        if (!RTMP_IsConnected(&rtmp)){
            ret = -1;
            RTMP_Log(RTMP_LOGERROR,"rtmp is not connect\n");
            break;
        }

        if (!RTMP_SendPacket(&rtmp, packet, 0)){
            ret = -1;
            RTMP_Log(RTMP_LOGERROR,"Send Error\n");
            break;
        }
        RTMP_Log(RTMP_LOGERROR,"Send Success\n");

        if (!ReadU32(&preTagsize, fp)){
            break;
        }

        if (!PeekU8(&type,fp)){
            break;
        }

        if (type==0x09){
            if (fseek(fp, 11, SEEK_CUR) != 0){
                break;
            }
            if (!PeekU8(&type, fp)){
                break;
            }
            if (type == FLV_KEY_FRAME){
                keyframe = 1;
            } else {
                keyframe = 0;
            }

            fseek(fp, -11, SEEK_CUR);
        }
    }

    RTMP_LogPrintf("\nSend Data Over\n");

    if(fp != NULL) {
        fclose(fp);
        fp = NULL;
    }

    if (packet != NULL){
        RTMPPacket_Free(packet);
        free(packet);
        packet=NULL;
    }

    RTMP_Close(&rtmp);

    return ret;
}

int main(int argc, char* argv[]){
    int  port = 1935;
    int  timeout = 300;
    char *flvfile = "receive.flv";
    char *rtmpurl = "rtmp://172.16.97.15/live/test";
    char *ip = "172.16.97.15";

    publish_flv(flvfile, rtmpurl, ip, port, timeout);
    return 0;
}
