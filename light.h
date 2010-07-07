#define init_light()\
	PORTBbits.RB1=0;\
	TRISBbits.TRISB1=0; \
	ANCON0=0xff; \
	ANCON1=0xff; 
	
#define light_on() 	PORTBbits.RB1=1;
#define light_off() PORTBbits.RB1=0;
#define light_toggle() PORTBbits.RB1=!PORTBbits.RB1;