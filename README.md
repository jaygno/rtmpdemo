## Rtmp Library

* git clone git://git.ffmpeg.org/rtmpdump

* cd librtmp

* vim Makefile

* add # before CRYPTO=OPENSSL (because crypto result in double free when RTMP_Init-> TLS_INIT)

* make && make install 

* cp last .h to /usr/local/include/librtmp/


## Play

## Publish


