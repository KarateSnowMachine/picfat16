#include "spi.h"
#include "light.h"


unsigned char WriteSPIM( BYTE data_out )
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
	// magical incantation to enable remapping of pins
	EECON2=0x55;
	EECON2=0xAA;
	PPSCONbits.IOLOCK = 0;

	RPINR21=5; // Map SDI2 (21) to RP5
	RPINR22=7;//Map SCK2 (10) to RP7 INPUT 
	RPOR7=10; //Map SCK2 (10) to RP7 OUTPUT -- as per data sheet
	RPOR6=9; //Map SDO2 (9) to RP6
	RPOR8=12; //Map CS2 (12) to RP8

	// incantation to write the pin mapping changes
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
	
	// set all the port mappings, GPIO pin settings
	init_spi();
	SSP2CON1bits.SSPM = 0;
	// the preamble-esque sequence needs to have chip select off (CS is active low)
	SD_CS=1;

	Delayms();
	// send 80 clock pulses
	for(sentClocks=0; sentClocks<12; sentClocks++)
	{
		// Each bit sends is accompanied by 1 clock, 8 bits per iteration 
		WriteSPIM(0xFF); // sets OUT high and strobes clock
	}

	//TODO: make a macro for command generation so it is clearer what is happening?
	// SD commands are all prefixed with 01 followed by a 6 bit command, hence the 0x40 = 01_00|0000 
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
	// wait for a command response ( 0 = OK )
	while ((x & 0x80) != 0);

// CMD1 = init command	
	while(1)
	{
		//cmd1 = 01_00|0001 = 0x41
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
			// init complete!
			break;
		}
	}
	// crank up the speed
	SSP2CON1bits.SSPM = 1;
	WriteSPIM(0xFF);
}
void SD_read_sector(SD_addr addr, BYTE *buf)
{
	BYTE cmd_response, data_token;
	int d; 

	SD_CS = 0; 
	
	//cmd17 = read sector 01_01|0001 = 0x51
	WriteSPIM(0xFF); 
	WriteSPIM(0x51); 
	WriteSPIM(addr.addr3); 
	WriteSPIM(addr.addr2); 
	WriteSPIM(addr.addr1); 
	WriteSPIM(0x00);
	WriteSPIM(0xFF);
	// wait for command response 
	do 
	{
	 	cmd_response = read_spi_byte();
	}
	while ((cmd_response & 0x80) != 0);

	if (cmd_response & 0x7F != 0) {
		ERROR();
	}


	// TODO: check command response error condition here

	// wait for data token
	do {
		data_token = read_spi_byte(); 
	}
	while(data_token != 0xFE);

	for (d=0; d<512; ++d)
	{
		buf[d] = read_spi_byte();
	}
	// 2 CRC tokens
	read_spi_byte();
	read_spi_byte();

	SD_CS=1; 
}
char SD_write_sector(SD_addr addr, BYTE *buf)
{
	BYTE x,data_response;
	int d;
	light_off();
	SD_CS=0;
	
		// I completely forget why this is here, probably as some kind of state transition point to denote the start of the command on the next byte
		WriteSPIM(0xFF); 
		// CMD24 = write to sector, 01_01|1000 = 0x58
		WriteSPIM(0x58);
		WriteSPIM(addr.addr3); 
		WriteSPIM(addr.addr2); 
		WriteSPIM(addr.addr1); 
		WriteSPIM(0x00);
		WriteSPIM(0xFF); //CRC byte
		do 
		{
		 	x = read_spi_byte();
		}
		//wait for R1 response (0 followed by status code)
		while (x != 0x00);

		// send at least 1 byte of empty clocks before sending out data token (2 just to be safe?)
		WriteSPIM(0xFF); 
		WriteSPIM(0xFF); 

		// data token
		WriteSPIM(0xFE);
		// 512 bytes of data
		for (d=0; d<512; d++)
		{
			WriteSPIM(buf[d]);
			x = SPIBUF; //clear buffer
		}
		// 2 crap CRC bytes
		WriteSPIM(0x55);
		WriteSPIM(0x55);
		// data response should come directly after the CRC byte
		data_response = read_spi_byte();
		if ((x & 0x1F) == 0x0d || (x & 0x1F) == 0x0b) { // write error
			SLOW_ERROR();
		} else if ((x & 0x1F) != 0x05) {
			goto good; 	
		} else {
			ERROR();
		}
good:
		//read until non zero(0 = still busy)
		do 
		{
			x = read_spi_byte();
		}
		while (x == 0);

		light_on();
		SD_CS=1;
}