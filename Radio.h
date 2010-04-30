/*
 Radio.h
 Radio
 
 Created by Kelly Egan on 4/30/10.
 
 This library is indebted to the RF12 library created by Jean-Claude Wippler @ 
 Jee labs http://jeelaps.org It is substantially different than the his 
 library particularly in packet structure and that it is now a C++ class. I also
 attempted to additionally comment the source code mostly for my own understanding.
 
 The MIT License
 
 Copyright (c) <2010> Kelly Egan
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
*/

#ifndef Radio_h
#define Radio_h

#include "stdint.h"
#include "WProgram.h"
#include <util/crc16.h>

//Arduino pins for RFM12B communication (RFM12B pin name)
#define RFM_IRQ  0    //0 is the first interupt, Arduino pin 2 (nIRQ)
#define SPI_SS   10   //Chip select. Pull low to send signal to RF module (nSEL)
#define SPI_MOSI 11   //Data out from Arduino (SDI)
#define SPI_MISO 12   //Data input from RFM12B (SDO)
#define SPI_SCK  13   //Clock input (SCK)

//Commands given to RFM12B via SPI
#define RF_RECEIVER_ON  0x82DD  // Turn on receiver (See Power Management Command, RF12B Program Guide)
#define RF_XMITTER_ON   0x823D  // Turn on transmitter (See Power Management Command, RF12B Program Guide)
#define RF_IDLE_MODE    0x820D  // See Power Management Command, RF12B Program Guide
#define RF_SLEEP_MODE   0x8205  // See Power Management Command, RF12B Program Guide
#define RF_WAKEUP_MODE  0x8207  // Enable wakeup timer (See Power Management Command, RF12B Program Guide)
#define RF_TXREG_WRITE  0xB800  // Send a char (See Transmitter Register Write Command, RF12B Program Guide)
#define RF_RX_FIFO_READ 0xB000  // See Receiver FIFO Read command, RF12B Program Guide
#define RF_WAKEUP_TIMER 0xE000  // See WakeUp Command, RF12B Program Guide

//RFM12B carrier frequencies
#define RF12_433MHZ     1
#define RF12_868MHZ     2
#define RF12_915MHZ     3   //Used in the North and South America


class Radio
{
public:
    Radio(char freq, char grp, char node); 
	void begin();  //Initialize the radio
    void send(char destination, char *message);  //Send a message
    //void send(char destination, char *data, boolean anon);   //Send a message anonymously
    boolean received(); //Has a message been received?
    void read(char *message); //Read the message into char array at message
    
    uint16_t SPIcmd(uint16_t cmd);      //give command to RFM12B via SPI
    
private:
	uint8_t _frequency;            //Carrier frequency: RF12_433MHZ, RF12_868MHZ or RF12_915MHZ 
	uint8_t _group;                //Network group 212 (0xD4) is default.
	uint8_t _nodeID;               //Node ID can be 1-31
};

#endif