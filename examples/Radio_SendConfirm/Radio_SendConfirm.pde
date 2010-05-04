/*

  Radio library example: send and confirm
  
  This program uses the Radio library to send a message via wireless 
  and then waits for a response. If if doesn't receive a response in 
  5 tries it assumes failure. The tranceiver used by the library
  is a Hope RFM12B FSK radio connected to an arduino/arduino clone 
  such as the JeeNode from http://jeelabs.net 
  
  When program is loaded on two arduino clones with a RFM12B tranceiver
  they take turns sending messages and sending confirmations.
 
  There is a 10 second interval between exchanges. 
  
  Arduino pins connected to RFM12B:
  
  2 RFM IRQ     Interupt pin from RFM12B
  10 SPI_SS     Chip select. 
  11 SPI_MOSI   Data output from Arduino
  12 SPI_MISO   Data input from RFM12B
  13 SPI_SCK    Clock input
  
  Note most Arduinos and clones run on 5V while the RFM12B runs on 3.3V
  before connecting an Arduino to the RFM12B be sure and account for this voltage difference.

  Created May 4 2010
  by Kelly Egan
  
*/

#include <Radio.h>

char sent[] = "Message sent. Please confirm.";   //Initial message
char received[] = "Roger. Message received.";    //Confirmation message

boolean confirmed;  //Has the message been confirmed by a reply? 
boolean failed;     //Has the the attempt failed 5 times?
int tries;          //Number of attempts at sending the initial message
long lastMessage;   //Milliseconds since last message

//Initialize the Radio object
char frequency = RF12_915MHZ;  //Frequency to operate radio at. 
                               //RF12_915MHZ can be used in North America
                               
char group = 0xD4;  //This 0-255 represents the operating "group" of the radio
                    //Message for radios sent to a different group will not be
                    //seen. Default is 212 (0xD4)
                      
char nodeID = 1;    //This is a number (1-31) to identify this radio transmission
                    //Nothing stops to radios from having the same ID, but may cause 
                    //confusion
                      
Radio theRadio(frequency, group, nodeID);

void setup() {
  Serial.begin(57600);
  theRadio.begin();

  tries = 0;
  confirmed = false;
  failed = false;
  lastMessage = millis();
}

void loop() {
  //If a message is available available() method returns true and saves message data
  if(theRadio.available()) {
    Serial.print("<incoming> ");
    Serial.println(theRadio.message());       //After calling available() the data
                                              //can be retrieved by calling message()
    //Other message data                             
    //Serial.println(theRadio.length());      //Length of the message
    //Serial.println(theRadio.sender());      //What node sent the data
    //Serial.println(theRadio.destination()); //Normally this is this nodes ID but could
                                              //be 0 if the message was broadcast.
    
    if( !strcmp(theRadio.message(), received) ) {
      Serial.println("Confirmation received. Success.");
      Serial.println();
      confirmed = true; 
    }
    //If the message matches sent string then send reply
    if( !strcmp(theRadio.message(), sent) ) {
      
      theRadio.write(0, received, false);  //write() sends a message
      //First parameter is destination node ID (0 for broadcast)
      //Second is the message to be sent char
      //Third is whether or not to send message anonymously.
      //This will set the sender id to 0.
      
      confirmed = false;
      failed = false;
      tries = 0;
      lastMessage = millis();// + 20000;
      
      Serial.print("<OUTGOING> ");
      Serial.println(received);
      Serial.println();
    }
  }
  
  //Attempt to send message every 10 seconds
  if( (millis() - lastMessage) > 10000) {
    //If haven't failed yet and have not yet confirmed keep trying
    if( !failed && !confirmed )  {
      //If tried less than 5 times keep going.
      if( tries < 5 ) {
        theRadio.write(0,sent,0);
        Serial.print("<OUTGOING> ");
        Serial.print(sent);
        Serial.print(" ");
        Serial.print(tries + 1);
        Serial.println(" attempt(s).");
        tries++;
      } else {
        Serial.println("FAILED!");
        failed = true;
      }
    }
    //Reset last message attempt
    lastMessage = millis(); 
  }

}




