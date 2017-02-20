CC = gcc
CFLAGS = -O2 -Wall -DNO_CRYPTO -I/usr/local/include -lrtmp

executables := $(patsubst %.c,%,$(wildcard *.c))  
                               
all : $(executables)  
	  
clean :    
	@rm -rf *.o *~    
	@rm -rf ${executables}    
.PHONY : clean  
