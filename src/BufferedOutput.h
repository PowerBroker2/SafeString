#ifndef BufferedOutput_h
#define BufferedOutput_h
#ifdef __cplusplus

/**
  (c)2018 Forward Computing and Control Pty. Ltd.
  This code may be freely used for both private and commerical use.
  Provide this copyright is maintained.
*/

/***
  Usage:
     // modes DROP_IF_FULL or DROP_UNTIL_EMPTY or BLOCK_IF_FULL.  BLOCK_IF_FULL will delay loop() when buffer fills up
  createBufferedOutput( output, 64, DROP_IF_FULL); // buffered out called output with buffer size 64 and mode drop chars if buffer full
  OR
  createBufferedOutput( output, 64, DROP_IF_FULL, false);  // for partial output of print(...)

  Then in setup()
  output.connect(Serial,9600); // connect the buffered output to Serial releasing bytes at 9600 baud.

  Then in loop()  use output instead of Serial e.g.
  void loop() {
    // put this line at the top of the loop, must be called each loop to release the buffered bytes
    output.nextByteOut(); // send a byte to Serial if it is time i.e. release at 9600baud
   ...
    output.print(" this is the msg"); // print to output instead of Serial
   ...
    output.read(); // can also read from output, not buffered reads directly from Serial.
   ...
   }
*/

#include <Print.h>
#include <Printable.h>
// This include handles the rename of Stream for MBED compiles
#if defined(ARDUINO_ARDUINO_NANO33BLE)
#include <Stream.h>
#elif defined( __MBED__ ) || defined( MBED_H )
#include <WStream.h>
#define Stream WStream
#else
#include <Stream.h>
#endif

#ifdef ARDUINO_ARDUINO_NANO33BLE
namespace arduino {
#endif

#define createBufferedOutput(name, size, ...) uint8_t name ## _OUTPUT_BUFFER[(size)]; BufferedOutput name(sizeof(name ## _OUTPUT_BUFFER),name ## _OUTPUT_BUFFER,  __VA_ARGS__ );

typedef enum {BLOCK_IF_FULL, DROP_UNTIL_EMPTY, DROP_IF_FULL } BufferedOutputMode;


class BufferedOutput : public Stream {
  public:
    /**
        use
        createBufferedOutput(name, size, mode);
        instead of calling this constructor

        BufferedOutput(const uint32_t _baudRate, uint8_t *_buf, size_t _bufferSize, BufferedOutputMode _mode, bool _allOrNothing  = true) ;
        You must call one of the BufferedOutput methods read, write, available.. , peek, flush, bytesToBeSent each loop() in order to release the buffered chars
        Usually just call bufferedStream.available(); at the top of loop()

         buf -- the user allocated buffer to store the bytes, must be at least bufferSize long.  Defaults to an internal 64 char buffer if buf is omitted or NULL
         bufferSize -- number of bytes to buffer,max bufferSize is limited to 32766. Defaults to an internal 64 char buffer if bufferSize is < 8 or is omitted
         mode -- BLOCK_IF_FULL (default), DROP_UNTIL_EMPTY or DROP_IF_FULL
                 BLOCK_IF_FULL,    like normal print, but with a buffer. Use this to see ALL the output, but will block the loop() when the output buffer fills
                 DROP_UNTIL_EMPTY, when the output buffer is full, drop any more chars until it completely empties.  ~~<CR><NL> is inserted in the output to show chars were dropped.
                                     Useful when there too much output.  It allow multiple prints to be output consecutively to give meaning full output
                                     avaliableForWrite() will return 0 from when the buffer fills until is empties
                 DROP_IF_FULL,     when the output buffer is full, drop any more chars until here is space.  ~~<CR><NL> is inserted in the output to show chars were dropped.
         allOrNothing -- defaults to true,  If true AND output buffer not empty then if write(buf,size) will not all fit don't output any of it.
                                        Else if false OR output buffer is empty then write(buf,size) will output partial data to fill output buffer.
                         allOrNothing setting is ignored if mode is BLOCK_IF_FULL
    */
    BufferedOutput(size_t _bufferSize, uint8_t *_buf, BufferedOutputMode = BLOCK_IF_FULL, bool allOrNothing = true);

    /**
        void connect(Stream& _stream, const uint32_t baudRate); // write to and how fast to write output, can also read from
            stream -- the stream to buffer output to, usually Serail.
            baudRate -- the maximum rate at which the bytes are to be released.  Bytes will be relased slower depending on how long your loop() method takes to execute
                         You must call one of the BufferedOutput methods read, write, available.. , peek, flush, bytesToBeSent each loop() in order to release the buffered chars
    */
    void connect(Stream& _stream, const uint32_t baudRate = 0); // if baudRate not specified use availableForWrite to control release
    
    void nextByteOut();
    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *buf, size_t size);
    virtual int available();
    virtual int read();
    virtual int peek();
    virtual void flush(); // this blocks until write buffer empty
    virtual int availableForWrite();
    size_t getSize(); // returns buffer size + any hardwareSerial buffer size found on connect
    void clearSpace(size_t len = 0); // clears space in outgoing (write) buffer, by removing last bytes written,  if len == 0 clear whole buffer, Serial Tx buffer is NOT changed
    void clear(); // clears outgoing (write) buffer
    size_t terminateLastLine(); // adds a newline if one not already there


  private:
    int internalAvailableForWrite();
    void writeDropMark();
    void updateSerialBufSize(size_t serialAvail);
    size_t bytesToBeSent(); // bytes in this buffer to be sent, // this ignores any data in the HardwareSerial buffer
    BufferedOutputMode mode; // = 0;
    bool allOrNothing; // = true current setting reset to allOrNothingSetting after each write(buf,size)
    bool allOrNothingSetting; // = true as passed in to constructor
    uint8_t defaultBuffer[8]; // if buffer passed in too small or NULL
    unsigned long uS_perByte; // == 1000000 / (baudRate/10) == 10000000 / baudRate
    Stream* streamPtr;
    uint32_t baudRate;
    unsigned long sendTimerStart;
    bool waitForEmpty;
    Print* debugOut; // only used if #define DEBUG uncomment in BufferedOutput.cpp
    size_t txBufferSize; // hardware serial tx buffer, if any
    bool dropMarkWritten;
    uint8_t lastCharWritten; // check for \n

    // ringBuffer methods
    /**
       _buf must be at least _size in length
       _size is limited to 32766
    */
    void rb_init(uint8_t* _buf, size_t _size);
    void rb_clear();
    bool rb_clearSpace(size_t len = 0); //returns true if some output dropped, clears space in outgoing (write) buffer, by removing last bytes written,  if len == 0 clear whole buffer, Serial Tx buffer is NOT changed
    // from Stream
    inline int rb_available() {
      return rb_buffer_count;
    }
    int rb_peek();
    int rb_read();
    size_t rb_write(uint8_t b); // does not block, drops bytes if buffer full
    size_t rb_write(const uint8_t *buffer, size_t size); // does not block, drops bytes if buffer full
    int rb_availableForWrite(); // {   return (bufSize - buffer_count); }
    size_t rb_getSize(); // size of ring buffer
    void rb_unWrite(); // removes last char written, if any
    void rb_dump(Stream* streamPtr);

    uint8_t* rb_buf;
    uint16_t rb_bufSize;
    uint16_t rb_buffer_head;
    uint16_t rb_buffer_tail;
    uint16_t rb_buffer_count;
    uint16_t rb_wrapBufferIdx(uint16_t idx);
    void rb_internalWrite(uint8_t b);
};

#ifdef ARDUINO_ARDUINO_NANO33BLE
} // namespace arduino
#endif

#endif  // __cplusplus
#endif // BufferedOutput_h
