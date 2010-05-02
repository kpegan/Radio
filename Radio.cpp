/*
 Radio.h
 Radio
 
 Created by Kelly Egan on 4/30/10.
 
 See licence in Radio.h
 
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
    Serial.println("Waiting...");
    uint16_t x = 0;
    while ((digitalRead(RFM_IRQ) == 0) && (x < 1000)) {
        SPIcmd(0x0000);
        x++;
    }
    if (x < 1000) {
        Serial.print("Radio awake... ");
        Serial.println(x,DEC);
    } else {
        Serial.println("Slept through alarm.");
    }
    
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
    cli();
    attachInterrupt(0, interrupt, LOW);
    uint16_t response = SPIcmd(0x0000);
    //Serial.println(response,BIN);
    //Serial.println("Hey fuckhead i am here");
    while (response & 0x8000) {
        SPIcmd(RF_TXREG_WRITE+0);
        SPIcmd(RF_RX_FIFO_READ);
    }
    sei();
}

int Radio::canWrite() {
    // no need to test with interrupts disabled: state TXRECV is only reached
    // outside of ISR and we don't care if rxfill jumps from 0 to 1 here
    cli(); // start critical section so we can call SPIcmd() safely
    if (SPIcmd(0x0000) & RF_RSSI_BIT) {
        // carrier sensed: we're over the RSSI threshold, don't start TX!
        sei(); // end critical section
        return 0;
    }
    SPIcmd(RF_IDLE_MODE); // stop receiver
    //XXX just in case, don't know whether these RF21 reads are needed!
    SPIcmd(0x0000); // status register
    SPIcmd(RF_RX_FIFO_READ); // fifo read
    RadioState = IDLE;
    sei(); // end critical section
    return 1;
}

int Radio::write(char destination, char *message) {
    uint8_t length;
    uint16_t fullHeader = 0;    
    uint16_t packet_crc = 0;
    
    //Determine length of the string if too big, just cut it off
    length = strlen(message);
    if(length > MAX_MESSAGE) {
        length = MAX_MESSAGE;
    }
    
    //Assemble the header
    fullHeader = destination << 11;            //Receiver ID is 5 left most bits
    fullHeader = fullHeader | (_nodeID << 6);  //Sender ID is next 5 bits
    fullHeader = fullHeader | (length & 0x3F); //Length is 6 right most bits
    
    //Add preamble and header bytes to buffer
    TXbuffer[0] = 0xAA;               //Preamble
    TXbuffer[1] = 0xAA;
    TXbuffer[2] = 0xAA;
    TXbuffer[3] = 0x2D;               //Sync
    TXbuffer[4] = _group;             //Sync2 (group)
    TXbuffer[5] = fullHeader >> 8;    //Header
    packet_crc = _crc16_update(packet_crc, TXbuffer[5] );
    
    TXbuffer[6] = fullHeader & 0xFF;  //Header
    packet_crc = _crc16_update(packet_crc, TXbuffer[6] );
    
    //Add data to buffer
    for (int i = 0; i < length; i++) {
        TXbuffer[i + 7] = message[i];
        packet_crc = _crc16_update(packet_crc, message[i] );
    }
    
    //Error correction and dummy bytes
    TXbuffer[length + 7] = packet_crc >> 8;   //error correction byte 1
    TXbuffer[length + 8] = packet_crc & 0xFF; //error correction byte 2
    TXbuffer[length + 9] = 0xAA;              //dummy bytes
    TXbuffer[length + 10] = 0xAA;
    TXbuffer[length + 11] = 0xAA;
    
    //Set other globals for buffer information
    TXposition = 0;
    if( length + 12 < MAX_SIGNAL) {
        TXlength = length + 12;  //Total packet length (data + 12 bytes);
    } else {
        TXlength = MAX_SIGNAL;
    }
    
    //Wait until finished receiving... this may need some work.
    //Serial.println(RadioState);
    while (RadioState == RECEIVING)
        ;
    SPIcmd(0x0000);
    
    //Will this help, we may never know...
    cli(); // start critical section so we can call SPIcmd() safely
    if (SPIcmd(0x0000) & RF_RSSI_BIT) {
        // carrier sensed: we're over the RSSI threshold, don't start TX!
        sei(); // end critical section
        return 0;
    }
    SPIcmd(RF_IDLE_MODE); // stop receiver
    
    SPIcmd(0x0000); // status register
    SPIcmd(RF_RX_FIFO_READ); // fifo read
    SPIcmd(RF_RX_FIFO_READ); // fifo read

    sei(); // end critical section
    
    
    RadioState = SENDING;
    
    //Turn on the transmitter
    SPIcmd(0x0000);
    SPIcmd(RF_XMITTER_ON);
}

boolean Radio::available() {
    if (RadioState == IDLE) {
        RadioState = LISTENING;
        SPIcmd(0x0000);
        SPIcmd(RF_RECEIVER_ON);
        return 0;
    }
    return RXavailable;
}

void Radio::read(){
    if(RXavailable) {
        _length = RXlength;
        
        //Pull the sender and receiver out of the header bytes
        _sender = ((RXbuffer[0] & 0x07) << 2) | (RXbuffer[1] >> 6);
        _receiver = RXbuffer[0] >> 3;
        
        for (int i = 0; i < RXlength; i++) {
            _message[i] = RXbuffer[i+2];
        }
        
        //Turn on receiver
        SPIcmd(0x0000);
        SPIcmd(RF_RECEIVER_ON);   //Turn receiver back on
        
        RXlength = 0;
        RadioState = LISTENING;
        RXavailable = 0;          //Reset message received flag
    }
}

char Radio::sender(){
    return _sender;
}

char Radio::receiver(){
    return _receiver;
}

char Radio::length(){
    return _length;
}

void Radio::interrupt() {
    uint16_t response = 0;
    response = SPIcmd(0x0000);

    switch (RadioState) {
        case LISTENING:
            RadioState = RECEIVING;
        case RECEIVING:
            if (response & 0x8000) {
                switch (RXposition) {
                    case 2:
                        RXlength = RXbuffer[1] & 0x3F;
                    case 1:
                    case 0:
                        RXbuffer[RXposition] = SPIcmd(RF_RX_FIFO_READ) & 0x00FF;
                        RXposition++;
                        break;
                    default:
                        if(RXposition < RXlength + 2 && RXposition < MAX_PACKET) {
                            RXbuffer[RXposition] = SPIcmd(RF_RX_FIFO_READ) & 0x00FF;
                            RXposition++;
                        } else {
                            SPIcmd(0x0000);
                            SPIcmd(RF_IDLE_MODE);
                            resetFIFO();
                            RXposition = 0;
                            RXavailable = 1;
                            RadioState = RECEIVE_DONE;
                        }
                        break;
                }                
            }
            break;
        case RECEIVE_DONE:
            //Something on finished receiving... probably don't need this.
            break;
        case SENDING:
            if(TXposition < TXlength && TXposition < MAX_SIGNAL) {
                SPIcmd(RF_TXREG_WRITE + TXbuffer[TXposition]);
                TXposition++;
            } else {
                SPIcmd(0x0000);
                SPIcmd(RF_IDLE_MODE);            //Turn off the transceiver
                SPIcmd(RF_TXREG_WRITE + 0xAA);   //This is some strong mojo, clears out the bad spirits
                RadioState = IDLE;        
            } 
            break;
        case IDLE:
            //Spend your days relaxing on a warm beach...
            break;
        default:
            break;
    }
}

void Radio::resetFIFO() {
    SPIcmd(0xCA81);
    SPIcmd(0xCA83);  
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

