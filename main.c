#include "p18f24j50.h"

#pragma config WDTEN=OFF,CP0=OFF,OSC=HS,WPDIS=OFF,WPCFG=OFF,XINST=OFF,FCMEN=OFF,IOL1WAY=OFF//,CPUDIV=OSC2_PLL2
#include "light.h"
#include "spi.h"
	#pragma udata a
	char rx_buffer1[256]; 
	#pragma udata b
	char rx_buffer2[256]; 
	#pragma udata c
	char rx_buffer3[256]; 
	#pragma udata d
	char rx_buffer4[256]; 

	int bytes_received = 0;
	char *rx_end = &rx_buffer4[255];
	char *rx_read = rx_buffer1;
	char *rx_write = rx_buffer1;

	extern void _startup (void);        // See c018i.c in your C18 compiler dir
	void _low_int(void);
	void _RX_recv(void);

#pragma code HIGH_INTERRUPT_VECTOR = 0x08
void High_ISR (void)
{
     _asm goto _RX_recv _endasm
}

#pragma code LOW_INTERRUPT_VECTOR = 0x18
void Low_ISR (void)
{
     _asm goto _low_int _endasm
}
#pragma code
	
#pragma interruptlow _RX_recv
#pragma interruptlow _low_int
void _RX_recv()
{
	if (RCSTA1bits.OERR)
	{
		RCSTA1bits.CREN=0;
		RCSTA1bits.CREN=1;
	}
	(*rx_read)=RCREG1;
	rx_read++;
	bytes_received++;
	if (rx_read >= rx_end)
	{
		rx_read=rx_buffer1;
	}
}

void _low_int ()
{

}
void init_uart()
{
	TRISC=0x80; // TRISC[7]=input, TRISC[6]=output
	BAUDCON1bits.BRG16=1; 
	SPBRGH1=0;
//	SPBRG1=42; // page 325 of data sheet for 10MHz clock
	SPBRG1=86; // page 325 of data sheet for 20MHz clock
	TXSTA1=0x26;
	RCSTA1=0x80; 
	IPR1bits.RC1IP=1;	// high priority for RX interrupt
	PIE1bits.RC1IE=1;	//enable RX interrupt
	RCONbits.IPEN=1;	//enable interrupt priorities

//	 //enable high priority interrupts
}

void init_slow_gps()
{
//	char cmd[] = "$PMTK314,5,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n";
	char cmd[] = "$PMTK414*33\r\n";
	char res[50];
	int i;
	int x=0;
	for (i=0; i<13; i++)
	{
		TXREG1=cmd[i];
		while (PIR1bits.TX1IF == 0); // wait for the interrupt flag to be set (=data done transmitting)
	}

	RCSTA1bits.CREN=1;

	for (i=0; i<50; i++)
	{
		res[i] = RCREG1;
		while (PIR1bits.RC1IF == 0);
		if (x==1)
			continue;
		if (res[i] == '\n')
		{
			x=1;
		}
		else
		{
			i--;
		}
		

		
	}
	i++;
}

void main()
{

//	spi_write_test();
	char addr=0x00;
	char *buf_ptr;
	char retval;
	init_light();
	SD_init();
	init_uart();
	init_slow_gps();
	INTCONbits.GIEH=1; //start 'er up
	while (1)
	{	
		if (bytes_received >= 512)
		{

			retval  = SD_write_sector(0x00, addr, rx_write);
			if (retval != 0)
				goto bad;
			bytes_received-=512;


			 //!= 0); // keep trying until something gives
		
			rx_write+=512;


			if (rx_write >= rx_end)
			{
				rx_write = rx_buffer1;
				
			}
			addr+=2;
			if (addr==16) 
			{
				goto good;
			}
		}
	}
bad:
	while(1) 
	{
		light_on();
	}
good:
	while(1)
	{
		light_toggle();
		Delayms();
		Delayms();
	}

/*
	for (i=0; i<768; i++)
	{

		if (bytes_received == 512)
		{
			SD_write_sector(0x00, rx_buffer1);

			INTCONbits.GIEH=0;
			rx_ptr-=512;
			bytes_received-=512;
			INTCONbits.GIEH=1;
		}


	}
*/



}
	