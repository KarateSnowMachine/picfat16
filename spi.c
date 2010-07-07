#include "spi.h"
#include "light.h"

#define BYTE char

unsigned char WriteSPIM( unsigned char data_out )
{
    BYTE clear;
    clear = SPIBUF;
    SPI_INTERRUPT_FLAG = 0;
    SPIBUF = data_out;
/*
    if (SPICON1 & 0x80)
	{
        return -1;
	}
    else
*/
    while (!SPI_INTERRUPT_FLAG); // wait for the transfer to finish
    return 0;
}

BYTE read_spi_byte(void)
{
    BYTE clear;
    clear = SPIBUF;
   	SPI_INTERRUPT_FLAG = 0;
    SPIBUF = 0xFF;
    while (!SPISTAT_RBF);
	return SPIBUF;
}

void init_spi()
{

	EECON2=0x55;
	EECON2=0xAA;
	PPSCONbits.IOLOCK = 0;

	RPINR21=5; // Map SDI2 (21) to RP5
	RPINR22=7;//Map SCK2 (10) to RP7 INPUT 
	RPOR7=10; //Map SCK2 (10) to RP7 OUTPUT -- as per data sheet
	RPOR6=9; //Map SDO2 (9) to RP6
	RPOR8=12; //Map CS2 (12) to RP8

	EECON2=0x55;
	EECON2=0xAA;
	PPSCONbits.IOLOCK = 1;

//    SD_CD_TRIS = INPUT;            //Card Detect - input
//   SD_WE_TRIS = INPUT;            //Write Protect - input

    SD_CS = 1;                     //Initialize Chip Select line (and turn off the card by setting CS=1)
    SD_CS_TRIS = OUTPUT;            //Card Select - output
 
    SPICLOCK = OUTPUT;
    SPIOUT = OUTPUT;                  // define SDO1 as output (master or slave)
    SPIIN = INPUT;                  // define SDI1 as input (master or slave)

    SSP2STAT = 0xC0;               // power on state 
	//SPISTATbits.CKE = 1;
	SSP2CON1 = 0x02;	//FOSC/64 clock, SSP2 enabled, Clock polarity = high on idle
	SSP2CON1bits.SSPEN=1;
}

void Delayms()
{
	int i;
	for (i=0; i<5000; i++)
	{
	}
}

//perform SD initialization (i.e. send CMD0 and CMD1 )
void SD_init(void)
{

	int sentClocks,i;
	BYTE x;

	//give the SD card a chance to settle on down
	Delayms();
	Delayms();
	Delayms();

	init_spi();
	SD_CS=1;

	Delayms();
	// send 80 clock pulses
	for(sentClocks=0; sentClocks<12; sentClocks++)
	{
		WriteSPIM(0xFF); // sets OUT high and strobes clock
	}


// CMD0 = software reset
	SD_CS=0;
	WriteSPIM(0x40);
	WriteSPIM(0x00); 
	WriteSPIM(0x00); 
	WriteSPIM(0x00); 
	WriteSPIM(0x00);
	WriteSPIM(0x95);
	do
	{
		x = read_spi_byte();
	}	
	while ((x & 0x80) != 0);

	if (x != 0x01)
	{
		while (1) 
		{
			light_toggle();
			Delayms();
		}
	}
	

// CMD1 = init command	
	while(1)
	{
		WriteSPIM(0x41);
		WriteSPIM(0x00); 
		WriteSPIM(0x00); 
		WriteSPIM(0x00); 
		WriteSPIM(0x00);
		WriteSPIM(0xFF);
	
		//read until bus is no longer floating (i.e. first bit is 0, not 1)
		do 
		{
			x = read_spi_byte();
		}
		while ((x & 0x80) != 0);
	
		if (x == 0x00) {
			break;
		}
	}
	// crank up the speed
	SSP2CON1bits.SSPM = 1;
	WriteSPIM(0xFF);
}

char SD_write_sector(SD_addr addr, char *buf)
{
	BYTE x,r;
	int d;
	light_off();
	SD_CS=0;
// CD24 = write to sector 0
		WriteSPIM(0xFF);
		WriteSPIM(0x58);
		WriteSPIM(addr.addr3); 
		WriteSPIM(addr.addr2); 
		WriteSPIM(addr.addr1); 
		WriteSPIM(0x00);
		WriteSPIM(0xFF);
		do 
		{
		 	x = read_spi_byte();
		}
		while ((x & 0x80) != 0);

		if (x  != 0x00)
		{	
			light_off()	 
			return x;
		}
		

		// data token
		WriteSPIM(0xFE);
		// 512 bytes of data
		for (d=0; d<512; d++)
		{
			WriteSPIM(buf[d]);
			x = SPIBUF; //clear buffer
		}
		// 2 crap CRC bytes
		WriteSPIM(0xFF);
		WriteSPIM(0xFF);
		do 
		{
		 	x = read_spi_byte();
		}
		while (((x & 0x1F)>> 1) != 0x02); // last byte is always 1 in this response packet, we are looking for the last byte = 0101

		//read until non zero
		do 
		{
			x = read_spi_byte();
		}
		while (x != 0);
/*
		WriteSPIM(0x4D);
		WriteSPIM(0x00); 
		WriteSPIM(0x00); 
		WriteSPIM(0x00); 
		WriteSPIM(0x00);
		WriteSPIM(0xFF);
			x = read_spi_byte();
			if (x == 0xff);
			x = read_spi_byte();
			if (x == 0x00);
			x = read_spi_byte();
			if (x == 0x00);
*/
		light_on();
		SD_CS=1;
		return 0;
}