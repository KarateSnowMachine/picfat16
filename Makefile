#this is a makefile for gcc only -- I use the MPLAB build system for a project under windows

all: fat16_gcc_linux.c
	gcc -o fat16 fat16_gcc_linux.c -std=c99

blank: 
	dd if=/dev/zero of=fat16_image_blank.img bs=1M count=100                                                                                                                                                       
	mkdosfs -s 32 -F 16 fat16_image_blank.img

copy:
	dd if=/dev/mmcblk0 of=fat16.img bs=1M count=20
	
