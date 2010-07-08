#include "p18f24j50.h"

#pragma config WDTEN=OFF,CP0=OFF,OSC=HS,WPDIS=OFF,WPCFG=OFF,XINST=OFF,FCMEN=OFF,IOL1WAY=OFF//,CPUDIV=OSC2_PLL2

#include "light.h"
#include "spi.h"
#include "fat16.h"

	#pragma udata a
	char rx_buffer1[256]; 
	#pragma udata b
	char rx_buffer2[256]; 
	#pragma udata c
	char rx_buffer3[256]; 


void init_uart()
{
	TRISC=0x80; // TRISC[7]=input, TRISC[6]=output
	BAUDCON1bits.BRG16=1; 

#if 1
	SPBRGH1=0; // page 325 of data sheet for 20MHz clock
	SPBRG1=86; // page 325 of data sheet for 20MHz clock
#else
	SPBRGH1=0x02; // page 325 of data sheet for 20MHz clock
	SPBRG1=0x08; // page 325 of data sheet for 20MHz clock
#endif


//	SPBRGH1=0;
//	SPBRG1=42; // page 325 of data sheet for 10MHz clock
//	SPBRG1=86; // page 325 of data sheet for 20MHz clock
	TXSTA1=0x26;
	RCSTA1=0x80; 
	IPR1bits.RC1IP=1;	// high priority for RX interrupt
	PIE1bits.RC1IE=1;	//enable RX interrupt
	RCONbits.IPEN=1;	//enable interrupt priorities

//	 //enable high priority interrupts
}

void init_slow_gps()
{
	char cmd1[] = "$PMTK314,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*2D\r\n";
//	char cmd2[] = "$PMTK251,4800*14\r\n";
	char *ptr;
	char res[50];


//send break character

//	char cmd[] = "$PMTK414*33\r\n";
	
	int i;
	int x=0;
#if 1
	SPBRGH1=0; // page 325 of data sheet for 20MHz clock
	SPBRG1=86; // page 325 of data sheet for 20MHz clock
#else
	SPBRGH1=0x04; // page 325 of data sheet for 20MHz clock
	SPBRG1=0x12; // page 325 of data sheet for 20MHz clock
#endif

	TXSTA1bits.SENDB=1;
	TXREG1='a';
	TXREG1=0x55; 
	ptr = cmd1;
	while (*ptr != 0)
	{
		TXREG1=*ptr;
		while (PIR1bits.TX1IF == 0); // wait for the interrupt flag to be set (=data done transmitting)
		ptr++;
	}

	ptr = 0;//cmd2;
	while (*ptr != 0)
	{
		TXREG1=*ptr;
		while (PIR1bits.TX1IF == 0); // wait for the interrupt flag to be set (=data done transmitting)
		ptr++;
	}
}

char get_uart_byte()
{

	char byte;
	if (RCSTA1bits.OERR) 
	{
		RCSTA1bits.CREN=0;
		while (1)
		{
			Delayms();
			Delayms();
			Delayms();
			Delayms();		
			light_toggle();
		}
		RCSTA1bits.CREN=1;
	}
	while (PIR1bits.RC1IF == 0);
	byte = RCREG1;

	return byte;
}



typedef enum _packet_type {UNKNOWN, RMC, GGA} packet_type;

#define wait_for_eol() 	while (get_uart_byte()!='\n');

void main()
{
	init_light();
	SD_init();
	light_off();
	SD_read_test();
	while(1);	


}


// below is the "real main" 
#if 0
void main()
{

	char tmpchr;
	char retval;
	DWORD sector_num=0;
	SD_addr addr=0;
	int a = sizeof(int);
	int sentence_start=-1;
	char sentence_finished=0;

	int bytes_received = 0;
	char *rx_read = rx_buffer3;
	packet_type pkt_type=UNKNOWN;

	init_light();
	SD_init();
	init_uart();


//	init_slow_gps();
	//INTCONbits.GIEH=1; //start 'er up

	RCSTA1bits.CREN=1;
	tmpchr = get_uart_byte();
	wait_for_eol();
	while (1)
	{	
		tmpchr = get_uart_byte();
		if (tmpchr == '$')
		{
			sentence_start=0;
			sentence_finished=0;
			pkt_type=UNKNOWN;
		}
		if (tmpchr == '\n')
		{
			sentence_finished=1;
			pkt_type=UNKNOWN;
		}
		light_off();
		//according to the data sheet 1=SPS mode(?), 2=DGPS SBS mode, 6=Dead Reconing(?) mean the fixes are valid
		if (pkt_type == GGA && sentence_start == 43 && !(tmpchr == '1' || tmpchr== '2' || tmpchr=='6'))
		{
			rx_read-=43;
			bytes_received-=43;
			wait_for_eol();
			continue;
		}
		// in an RMC sentence, the "A" field indicates that a fix is valid
		if (pkt_type == RMC && sentence_start == 18 && tmpchr != 'A')
		{
			rx_read-=18;
			bytes_received-=18;
			wait_for_eol();
			continue;
		}
		// only consider $GPGGA and $GPRMC packets by the 4th byte we can figure out if we need this transmission or not
		// if we don't need it, just roll back the pointers/counters in the buffer and throw away the remaining bytes in this sentence

		if (sentence_start == 4)
		{
			if (tmpchr == 'M') 
			{
				pkt_type = RMC;
			}
			else if (tmpchr == 'G')
			{
				pkt_type = GGA;
			}
			else
			{
				rx_read-=4;
				bytes_received-=4;
				wait_for_eol();
				pkt_type = UNKNOWN;
				continue;
			}
		}

		*rx_read = tmpchr;
		rx_read++;
		sentence_start++;
		bytes_received++;
		light_on();

		if (bytes_received >= 512 && sentence_finished)
		{
			RCSTA1bits.CREN=0;	//disable reception for now to avoid overrun errors
			//write data to the SD card
			addr.full_addr = sector_num<<9;
			retval  = SD_write_sector(addr, (BYTE *)(rx_read-bytes_received));
			if (retval != 0)
				goto bad;

			bytes_received-=512;
			memcpy(rx_buffer3, rx_read-bytes_received, bytes_received);
			rx_read=rx_buffer3+bytes_received;

			sector_num++;
			if (sector_num >= ((2<<20)-1))
				goto good;

			//renable reception
			RCSTA1bits.CREN=1;
			wait_for_eol();
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



} // end main
#endif 
	