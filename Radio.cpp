/*
 Radio.h
 Radio
 
 Created by Kelly Egan on 4/30/10.
 
 See licence, in RadioRadio.h
 
*/

#include "Radio.h"


//Radio class PUBLIC METHODS

Radio::Radio(char freq, char grp, char node) {
    _frequency = freq;
    _group = grp;
    _nodeID = node;
}

void Radio::begin() {
    //Initialize SPI communications pins for RFM12B
	pinMode(SPI_MOSI, OUTPUT);     
	pinMode(SPI_MISO, INPUT);
	pinMode(SPI_SCK, OUTPUT);
	pinMode(SPI_SS, OUTPUT); 
    
    //Disable communications
	digitalWrite(SPI_SS, HIGH);  
	
	//IF Clock speed of Arduino doesn't exceed 10Mhz
#if F_CPU <= 10000000
	// clk/4 is ok for the RF12's SPI
	SPCR = _BV(SPE) | _BV(MSTR);
#else
	// use clk/8 (2x 1/16th) to avoid exceeding RF12's SPI specs of 2.5 MHz
	SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);
	SPSR |= _BV(SPI2X);
#endif

    //Initialize interrupt pin
    pinMode(RFM_IRQ, INPUT);
    digitalWrite(RFM_IRQ, 1); //pull-up
    
    //Begin initialization of Radio
    SPIcmd(0x0000);
    SPIcmd(RF_SLEEP_MODE);
    
    //This while loop seems,on occasion to hang. Why?
    //Serial.println("Waiting...");
    while (digitalRead(RFM_IRQ) == 0)
        SPIcmd(0x0000);
    //Serial.println("Radio awake...");
    
    //Details of these commands can be found in the Command Reference
    SPIcmd(0x80C7 | (_frequency << 4)); // EL (enable TX), EF (enable RX FIFO), 12.0pF 
    SPIcmd(0xA640); // Frequency setting command 
    SPIcmd(0xC606); // Data Rate Command - 57.6Kbps (38.4: 8, 19.2: 11, 9.6: 23, 4.8: 47)
    SPIcmd(0x94A2); // Receiver Control Command - VDI,FAST,134kHz,0dBm,-91dBm 
    SPIcmd(0xC2AC); // Data Filter Command - AL,!ml,DIG,DQD4 
    SPIcmd(0xCA83); // FIFO and Reset Mode Command - FIFO8,SYNC,!ff,DR 
    SPIcmd(0xCE00 | _group); // Syncron pattern command - SYNC=2DXX； 
    SPIcmd(0xC483); // @PWR,NO RSTRIC,!st,!fi,OE,EN 
    SPIcmd(0x9850); // !mp,90kHz,MAX OUT 
    SPIcmd(0xCC77); // OB1，OB0, LPX,！ddy，DDIT，BW0 
    SPIcmd(0xE000); // Wake-Up Timer Command - NOT USED
    SPIcmd(0xC800); // Low Duty-Cycle Command - NOT USED 
    SPIcmd(0xC049); // Low Battery Detector and Microcontroller Clock Divider Command - 1.66MHz,3.1
    
}

uint16_t Radio::SPIcmd(uint16_t cmd) {
    uint16_t reply;
    digitalWrite(SPI_SS, 0);
    SPDR = cmd >> 8;
    while (!(SPSR & _BV(SPIF)))
        ;
    reply = SPDR << 8;
    SPDR = cmd;
    while (!(SPSR & _BV(SPIF)))
        ;
    reply |= SPDR;
    digitalWrite(SPI_SS, 1);
    return reply;
}

//Radio class PRIVATE METHODS

