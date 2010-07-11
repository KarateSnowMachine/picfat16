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
		SLOW_ERROR();
		RCSTA1bits.CREN=1;
	}
	while (PIR1bits.RC1IF == 0);
	byte = RCREG1;

	return byte;
}



typedef enum _packet_type {UNKNOWN, RMC, GGA} packet_type;

#define wait_for_eol() 	while (get_uart_byte()!='\n');

BYTE find_date_time_from_sentence_buffer(WORD *date, WORD *time, char *bufx, unsigned buf_size) 
{
	#define UTC_OFFSET 5
	char date_buf[6];
	char time_buf[6];
	WORD tmp_date=0, tmp_time=0;
	unsigned short h,m,s,y,M,d;
	
	char c = '$';
	char *start, *end;
	unsigned i=0,x;

	start = bufx;
	end = bufx;
	while (*start != 'R') { if ( i>= buf_size) return -1; start++; i++; } //find the start of an RMC sentence
	while (*start != ',') { if ( i>= buf_size) return -1; start++; i++; } // go to the first field (time)
	start++; // point to the first char of the time stamp
	end = start+1;
	while (*end   != '.') { if ( i>= buf_size) return -1; end++; i++; } // point end to the decimal point in the time

	memcpy (time_buf, start, end-start); // copy out the time field

	//fast forward 8 fields to the date stamp
	for (x=0; x<8; ++x) {
		while (*start != ',') { if ( i>= buf_size) return -1; start++; i++; } start++;
	}
	end = start+1;
	while (*end != ',') { if ( i>= buf_size) return -1; end++; i++; } // point end to the decimal point in the time
	memcpy (date_buf, start, end-start);

	// convert the ascii values into actual integers
	for (i=0; i<6; ++i)	{
		date_buf[i] -= '0';
		time_buf[i] -= '0';
	}
	// deal with the time first since it is easier [0|5bits H|5bits M|5bits S/2]

	y = (date_buf[4]*10+date_buf[5]+20); // +20 because FAT16 is relative to jan 1 1980
	M = (date_buf[2]*10 + date_buf[3]); //m
	d = ((date_buf[0]*10 + date_buf[1])); //d

	h = (time_buf[0]*10 + time_buf[1]);
	if (h > UTC_OFFSET) { 
		h -= UTC_OFFSET;
	} else {
		d--;
		h+24-UTC_OFFSET;
	}
	m = (time_buf[2]*10 + time_buf[3]);
	s = ((time_buf[4]*10 + time_buf[5])/2);

	*time = (h<<11) | (m<<5) | s;
	*date = (y<<9) | (M<<5) | d;

	return 0;
}

void main()
{

	char tmpchr;
	char retval;
	SD_addr addr=0;

	//number of characters since the start of the current sentence
	int sentence_start=-1;
	unsigned short sentence_finished=0;
	unsigned char file_created=0;

	unsigned long bytes_received = 0;
	char *rx_read = rx_buffer3;
	char *rx_read_start; 
	WORD fat16_date, fat16_time;

	packet_type pkt_type=UNKNOWN;

	init_light();
	SD_init();
	init_uart();

	init_fat16();
	

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
			sentence_start=0;
			sentence_finished=1;
			pkt_type=UNKNOWN;
		}
	//	light_off();
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
//		light_on();
	
		if (bytes_received >= 512 && sentence_finished)
		{
			RCSTA1bits.CREN=0;	//disable reception for now to avoid overrun errors
			rx_read_start = rx_read - bytes_received;  // should always be the same as rx_buffer3 

			if (!file_created) 
			{
				retval = find_date_time_from_sentence_buffer(&fat16_date, &fat16_time, rx_read_start, bytes_received);				
				if (retval == 0) {
					create_file(
						 fat16_date,
						 fat16_time
					);
				} else {
					create_file(
						 0x3ee7, /* july 7 2010 */
						 0x2108 /*04:08:16am */
							);	
				}
				file_created = 1; 
			}

			//write data to the SD card
			write_buf((BYTE *)rx_read_start);

/*
			if (retval != 0)
				goto bad;
*/
			bytes_received-=512;
			if (bytes_received > 0) {
				// to make life easy, move the leftover parts of the buffer back to the top and continue copying data below it
				memcpy(rx_buffer3, rx_read - bytes_received, bytes_received);
				rx_read=rx_buffer3+bytes_received;
			} else {
				rx_read = rx_buffer3;
			}


			//renable reception
			RCSTA1bits.CREN=1;
			/* odds are we are going to have a fraction of a sentence if we start logging 
				whatever comes in now, so wait for the sentence to end before going on */
			wait_for_eol();
		}
	}
bad:
	SLOW_ERROR();

} // end main

	