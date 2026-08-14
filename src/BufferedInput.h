#ifndef BufferedInput_h
#define BufferedInput_h
#ifdef __cplusplus

/**
  BufferedInput.h
  by Matthew Ford
  (c)2020 Forward Computing and Control Pty. Ltd.
  This code is not warranted to be fit for any purpose. You may only use it at your own risk.
  This code may be freely used for both private and commercial use.
  Provide this copyright is maintained.
*/

/***
  Usage:
  createBufferedInput( input, 64); // buffered input called input with an extra buffer size of 64 chars

  Then in setup()
  input.connect(Serial); // connect the buffered input to Serial to read from

  Then in loop()  use input instead of Serial e.g.
  void loop() {
    // put this line at the top of the loop, must be called each loop to transfer the
    // available chars from the Serial RX buffer into this buffer before the RX buffer overflows
    input.nextByteIn();
   ...
    if (input.available()) { int c = input.read(); } // read from input instead of Serial
   ...
    input.print(" this is the msg"); // can also write to input, not buffered, writes directly to Serial.
   ...
   }

  Sizing the buffer: input.maxBufferUsed() returns the high water mark of this buffer and
  input.maxStreamAvailable() returns the high water mark of the Serial RX buffer.  Both are
  reset to zero by the call.  If maxBufferUsed() reaches the buffer size, increase the size.
  If maxStreamAvailable() approaches the Serial RX buffer size, call nextByteIn() more often.
*/

#include <Print.h>
#include <Printable.h>

#include "SafeString.h" // for SSTRING_DEBUG and SafeString::Output and stream support

// handle namespace arduino
#include "SafeStringNameSpaceStart.h"

#define createBufferedInput(name, size) uint8_t name ## _INPUT_BUFFER[(size)]; BufferedInput name(sizeof(name ## _INPUT_BUFFER),name ## _INPUT_BUFFER);

/**************
  To create a BufferedInput use the macro **createBufferedInput**  see the detailed description. 
  
  The createBufferedInput macro takes 2 arguments.<br> 
  createBufferedInput(name, bufferSize); creates a BufferedInput called <i>name</i> with a buffer size of <i>bufferSize</i>.<br>
  e.g. to create a BufferedInput called bufferedInput with a buffer size of 128 use<br>
 <code>createBufferedInput(bufferedInput, 128)</code><br>
 
  Add a call to <br>
  <code>bufferedInput.nextByteIn();</code><br>
  at the top of the loop() to read more chars from the input.  You can add more of these calls through out the loop() code if needed.<br>
  Most BufferedInput methods also read more chars from the input<br> 
  
  See [Arduino Serial I/O for the Real World - BufferedInput](https://www.forward.com.au/pfod/ArduinoProgramming/Serial_IO/index.html#BufferedInput) for an example of its use.
***************************************************************************************/
class BufferedInput : public Stream {
  public:
    /**
         use createBufferedInput(name, size); instead
         BufferedInput(size_t _bufferSize, uint8_t *_buf);

         buf -- the user allocated buffer to store the bytes, must be at least bufferSize long.  Defaults to an internal 8 char buffer if buf is NULL (there is no default, buf cannot be omitted)
         bufferSize -- number of bytes to buffer,max bufferSize is limited to 32766. Defaults to an internal 8 char buffer if bufferSize is < 8 (there is no default, bufferSize cannot be omitted)
    */
    BufferedInput(size_t _bufferSize, uint8_t *_buf);

    /**
        void connect(Stream& _stream); // the stream to read from, can also write to
            stream -- the stream to buffer input for
    */
    void connect(Stream& _stream);

    void nextByteIn();
    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *buf, size_t size);
    virtual int available();
    virtual int read();
    virtual int peek();
    virtual void flush();
    virtual int availableForWrite();
    size_t getSize(); // returns buffer size

    // These two are HIGH WATER MARKS, not drop counts.  BufferedInput does not count dropped chars;
    // nextByteIn() only ever moves as many bytes as currently fit, it never discards any.
    // Each value is reset to zero by the call that reads it, so each reading covers the interval
    // since the previous call.
    int maxStreamAvailable(); // the largest available() seen on the connected Stream, ie how close the
    // hardware RX buffer came to overflowing.  If this approaches the RX buffer size, call nextByteIn() more often
    int maxBufferUsed();      // the largest fill level seen in this buffer.  If this reaches the buffer
    // size then chars ARE being lost at the Stream, increase the size passed to createBufferedInput( )

  private:
    Stream* streamPtr;
    int maxAvail;
    int bufUsed;
    uint8_t defaultBuffer[8]; // if buffer passed in too small or NULL

    // ringBuffer methods
    /**
       _buf must be at least _size in length
       _size is limited to 32766
    */
    void rb_init(uint8_t* _buf, size_t _size);
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
    void rb_clear();
    void rb_dump(Stream* streamPtr);
    uint8_t* rb_buf;
    uint16_t rb_bufSize;
    uint16_t rb_buffer_head;
    uint16_t rb_buffer_tail;
    uint16_t rb_buffer_count;
    uint16_t rb_wrapBufferIdx(uint16_t idx);
    void rb_internalWrite(uint8_t b);
};

#include "SafeStringNameSpaceEnd.h"

#endif  // __cplusplus
#endif // BufferedInput_h
