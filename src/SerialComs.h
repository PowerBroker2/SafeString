#ifndef SERIAL_COMS_H
#define SERIAL_COMS_H
/*
  SerialComs.h  a send/receive lines of text between Arduinos via Serial
  by Matthew Ford
  (c)2021 Forward Computing and Control Pty. Ltd.
  This code is not warranted to be fit for any purpose. You may only use it at your own risk.
  This code may be freely used for both private and commercial use.
  Provide this copyright is maintained.
**/
// SafeStringReader.h include defines for Stream
#include "SafeStringReader.h"
#include "millisDelay.h"


/**
  Brief overview.
  
  #include "SerialComs.h"
  SerialComs coms;  // instantsiate the SerialComs object
  // default is 60 chars sendSize, 60 chars receiveSize
  // sendSize and receiveSize limits are actual number msg chars that can be handled
  // excluding the checksum,XON and terminating '\0' which are handled internally
  
  
  in setup()
  
  void setup() {
    // serial's begin()
    
    // comment this line out once everything is running to suppress the SerialComs debug output
    SafeString::setOutput(Serial); // for SafeString error messages and SerialComs debug output
    
    // set one side (and ONLY one side) as the controller. 
    // If one side is using SoftwareSerial it MUST be set as the controller
    coms.setAsController();
    
    // connect the coms to the stream to send/receive from
    // ALWAYS check the return
    if (!coms.connect(Serial1)) {
      while(1) {
        Serial.println(F("Out-Of-Memory"));
        delay(3000);
      }
    }
    
  in loop()
  void loop() {
    // sendAndReceive() MUST be called every loop!!
    coms.sendAndReceive();  // clears coms.textReceived every time before checking for new received message
    // .  . . 
    if (!coms.textReceived.isEmpty()) {
      // got a message from the other side process it here
    }
    // .  .  .
    if (coms.textToSend.isEmpty()) {
      // last msg has been sent, can set new message to be sent
      coms.textToSend.print("... ");
      coms.textToSend.print(number); // etc
    }
  
    
  coms.textToSend and coms.textReceived are both SafeStrings of fixed capacity.
  coms.textToSend has sendSize capacity and will not accept any more than that many chars
  Once the text is sent the coms.textToSend is cleared.
  
  Any text received, upto the receiveSize appears in coms.textReceived.
  coms.textRecieved is cleared each time sendAndReceive() is called
  
*/
// This class uses the SafeString::Output for debug and error messages.
// call SafeString::setOutput(Stream..); to set where the debug and error msgs are to be sent to
//  NOT to the stream pass to the connect( ) :-(

// these define allow you to access the SafeString& like they were class variables
#define textToSend getTextToSend()
#define textReceived getTextReceived()

// handle namespace arduino
#include "SafeStringNameSpaceStart.h"

class SerialComs : public SafeString {
  public:
    // SerialComs coms();  // creates default coms with send/recieve fo 60char each
    // this default will tolerate very long delays() on either side with out losing data
    // SerialComs coms(120,200); // set sendSize to 120 and receiveSize to 200
    //   on the other side you need SerialComs coms(200,120) to match send <-> receive
    SerialComs(size_t sendSize = 60, size_t receiveSize = 60);

    // One side (and only one side) must be set as the controller
    // SoftwareSerial board should be set as the controller
    void setAsController(); // default no the controller

    // connect sets the Stream to send to/receive from
    // connect returns false if there is not enough memory for the linesizes specified in the constructor
    // you should always check the return.
    bool connect(Stream &io);

    // these three methods are all that you need to use in the loop() code.

    // sendAndReceive() MUST be called every loop.
    // is sends any data if we can send, receives any data if we are waiting for the other side
    // each call to sendAndReceive() first clears the textReceived SafeString, so you must process the received text in the loop() it is received.
    // sendAndReceive() clears the textToSend SafeString once it is sent.
    void sendAndReceive();

    // in the loop() you can use
    // coms.textReceived and coms.textToSend to access the SafeStrings that hold the received text and the text to send.
    // instead of coms.getTextReceived() and coms.getTextToSend()
    SafeString& getTextReceived(); // alias textReceived
    SafeString& getTextToSend();   // alias textToSend

    bool isConnected();

    // default is to add checkSum and check incoming checksum
    // calling noCheckSum() disables both of these.
    // need to do the same on the other side as well.
    void noCheckSum();

    ~SerialComs(); // frees memory
  private:
    char *receive_INPUT_BUFFER;
    char *receiver_TOKEN_BUFFER;
    char *send_BUFFER;
    SafeString* textToSendPtr;
    SafeStringReader* textReceivedPtr;
    SafeString* receiver_SF_INPUT;

    void receiveNextMsg();
    void sendNextMsg();
    void deleteSerialComs();
    void clearIO_Available();
    void checkConnectionTimeout();
    bool checkCheckSum(SafeString& msg);
    void calcCheckSum(SafeString& msg, SafeString& chkHexStr);
    void lostConnection();
    void setConnected();
    void resetConnectionTimer();

    bool connected;
    bool clearToSendFlag;
    bool isController;
    bool notUsingCheckSum;

    millisDelay connectionTimeout;
    bool outOfMemory;
    bool memoryLow;
    size_t _receiveSize;
    size_t _sendSize;
    Stream *stream_io_ptr;
    unsigned long connectionTimeout_mS;// = 250; // 0.25 sec
    static char emptyCharArray[0];
};

#include "SafeStringNameSpaceEnd.h"

#endif // SERIAL_COMS_H
