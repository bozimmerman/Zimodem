class XModem 
{
  typedef enum {
    Crc,
    ChkSum  
  } transfer_t;
  
  private:
    //delay when receive bytes in frame - 7 secs
    static const int receiveDelay;
    //retry limit when receiving
    static const int rcvRetryLimit;
    //holds readed byte (due to dataAvail())
    int byte;
    //expected block number
    unsigned char blockNo;
    //extended block number, send to dataHandler()
    unsigned long blockNoExt;
    //retry counter for NACK
    int retries;
    //buffer
    char buffer[128];
    //repeated block flag
    bool repeatedBlock;

    int  (*recvChar)(int);
    void (*sendChar)(char);
    bool (*dataHandler)(unsigned long number, char *buffer, int len);
    unsigned short crc16_ccitt(char *buf, int size);
    bool dataAvail(int delay);
    int dataRead(int delay);
    void dataWrite(char symbol);
    bool receiveFrameNo(void);
    bool receiveData(void);
    bool checkCrc(void);
    bool checkChkSum(void);
    bool receiveFrames(transfer_t transfer);
    bool sendNack(void);
    void init(void);
    
    bool transmitFrames(transfer_t);
    unsigned char generateChkSum(void);
    
  public:
    static const unsigned char NACK;
    static const unsigned char ACK;
    static const unsigned char SOH;
    static const unsigned char EOT;
    static const unsigned char CAN;
  
    XModem(int (*recvChar)(int), void (*sendChar)(char));
    XModem(int (*recvChar)(int), void (*sendChar)(char), 
                bool (*dataHandler)(unsigned long, char*, int));
    bool receive();
    bool transmit();
};

/*
 Download a file from an SD card
 
 This example shows how to download a file from an SD card
 using the XModem protocol.
   
 The circuit:
 * SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4
 
 created  22 December 2010
 by Limor Fried
 modified 9 Apr 2012
 by Tom Igoe
 modified 4 Jul 204
 by Peter Turczak
 
 This example code is in the public domain.
   
 */
/*

#include <SD.h>
#include <XModem.h>

XModem xmodem(&Serial, ModeXModem);


// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
const int chipSelect = 10;    

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }


  Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");
}

void loop()
{
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("windlog.txt");

  // if the file is available, write to it:
  if (dataFile) {
    Serial.println("Here comes the X-Modem!");
    xmodem.sendFile(dataFile, "datalog.txt");
  }  
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  } 
}
*/
