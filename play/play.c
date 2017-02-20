#include <stdio.h>
#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"
#include <stdlib.h>

#include <string.h>

//gcc -o play play.c -DNO_CRYPTO -I/usr/local/include -lrtmp
//play 指定直播地址，并保存视频到本地
int play(char *url, char *local)
{
    double duration=-1;
    int nRead;
    //is live stream ?
    int bLiveStream=1;

    int bufsize=1024*1024*10;
    char *buf=(char*)malloc(bufsize);
    memset(buf,0,bufsize);
    long countbufsize=0;

    FILE *fp=fopen(local,"wb");
    if (!fp){
        RTMP_LogPrintf("Open File Error.\n");
        return -1;
    }

    /* set log level */
    //RTMP_LogLevel loglvl=RTMP_LOGDEBUG;
    //RTMP_LogSetLevel(loglvl);

    RTMP *rtmp=RTMP_Alloc();
    RTMP_Init(rtmp);
    //set connection timeout,default 30s
    rtmp->Link.timeout=30;
    // HKS's live URL
    if(!RTMP_SetupURL(rtmp,url))
    {
        RTMP_Log(RTMP_LOGERROR,"SetupURL Err\n");
        RTMP_Free(rtmp);
        return -1;
    }
    if (bLiveStream){
        rtmp->Link.lFlags|=RTMP_LF_LIVE;
    }

    //1hour
    RTMP_SetBufferMS(rtmp, 3600*1000);

    if(!RTMP_Connect(rtmp,NULL)){
        RTMP_Log(RTMP_LOGERROR,"Connect Err\n");
        RTMP_Free(rtmp);
        return -1;
    }

    if(!RTMP_ConnectStream(rtmp,0)){
        RTMP_Log(RTMP_LOGERROR,"ConnectStream Err\n");
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
        return -1;
    }

    while(nRead=RTMP_Read(rtmp,buf,bufsize)){
        fwrite(buf,1,nRead,fp);

        countbufsize+=nRead;
        RTMP_LogPrintf("Receive: %5dByte, Total: %5.2fkB\n",nRead,countbufsize*1.0/1024);
    }

    if(fp) {
        fclose(fp);
    }

    if(buf) {
        free(buf);
    }

    if(rtmp){
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
        rtmp=NULL;
    }
    return 0;
}


int main()
{
    char *local = "receive.flv";
    char *url   = "rtmp://live.hkstv.hk.lxdns.com/live/hks";
    play(url, local);
    return 0;
}
