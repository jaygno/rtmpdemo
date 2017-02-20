/**
 * This program can send local h264 stream to net server as rtmp live stream.
 */

#include <stdio.h>
#include "librtmp_send264.h"


FILE *fp_send1;

int read_buffer1(unsigned char *buf, int buf_size ){
	if(!feof(fp_send1)){
		int true_size=fread(buf,1,buf_size,fp_send1);
		return true_size;
	}else{
		return -1;
	}
}

int main(int argc, char* argv[])
{
	fp_send1 = fopen("cuc_ieschool.h264", "rb");

	RTMP264_Connect("rtmp://localhost/publishlive/livestream");
	
	RTMP264_Send(read_buffer1);

	RTMP264_Close();

	return 0;
}
