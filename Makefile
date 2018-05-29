all:
	/home/disk2/zp/tools/arago-2011.09/armv7a/bin/arm-arago-linux-gnueabi-gcc -o demo -I./ -I./comport *.c *.cpp ./comport/comport.c -lstdc++ -lm -lpthread