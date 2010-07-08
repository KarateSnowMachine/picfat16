#ifndef _SPI_H_
#define _SPI_H_

#include "p18f24j50.h"
#define ERROR() while (1) { light_toggle(); Delayms(); Delayms(); }
typedef unsigned char BYTE; 
typedef unsigned short WORD;
		#define INPUT 1;
		#define OUTPUT 0;

        #define SD_CS               LATBbits.LATB5
        // Description: SD-SPI Chip Select TRIS bit
        #define SD_CS_TRIS          TRISBbits.TRISB5
        
        // Description: SD-SPI Card Detect Input bit
//        #define SD_CD               PORTAbits.RA0
        // Description: SD-SPI Card Detect TRIS bit
//        #define SD_CD_TRIS          TRISAbits.TRISA0
        
        // Description: SD-SPI Write Protect Check Input bit
//        #define SD_WE               PORTAbits.RA1
        // Description: SD-SPI Write Protect Check TRIS bit
//        #define SD_WE_TRIS          TRISAbits.TRISA1
        
        // Registers for the SPI module you want to use

        // Description: The main SPI control register
        #define SPICON1             SSP2CON1
        // Description: The SPI status register
        #define SPISTAT             SSP2STAT
        // Description: The SPI buffer
        #define SPIBUF              SSP2BUF
        // Description: The receive buffer full bit in the SPI status register
        #define SPISTAT_RBF         SSP2STATbits.BF
        // Description: The bitwise define for the SPI control register (i.e. _____bits)
        #define SPICON1bits         SSP2CON1bits
        // Description: The bitwise define for the SPI status register (i.e. _____bits)
        #define SPISTATbits         SSP2STATbits

        // Description: The interrupt flag for the SPI module
        #define SPI_INTERRUPT_FLAG  PIR3bits.SSP2IF   
        // Description: The enable bit for the SPI module
        #define SPIENABLE           SPICON1bits.SSPEN
        // Tris pins for SCK/SDI/SDO lines
        #define SPICLOCK            TRISBbits.TRISB4
        #define SPIIN               TRISBbits.TRISB2
        #define SPIOUT              TRISBbits.TRISB3

        // Latch pins for SCK/SDI/SDO lines
        #define SPICLOCKLAT         LATBbits.LATB4
        #define SPIINLAT            LATBbits.LATB2
        #define SPIOUTLAT           LATBbits.LATB3

        // Port pins for SCK/SDI/SDO lines
        #define SPICLOCKPORT        PORTBbits.RB4
        #define SPIINPORT           PORTBbits.RB2
        #define SPIOUTPORT          PORTBbits.RB3

typedef unsigned long		DWORD;
typedef union {
	struct {
		DWORD full_addr;
	};
	struct {
		char addr0;
		char addr1;
		char addr2;
		char addr3;
	};
} SD_addr;

#endif

void Delayms(void);
void init_spi(void);
void SD_init(void);
char SD_write_sector(SD_addr addr, BYTE *buf);
void SD_read_sector(SD_addr addr, BYTE *buf);

