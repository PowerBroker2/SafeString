#include <Arduino.h>
#include "BufferedOutput.h"

#ifdef ARDUINO_ARDUINO_NANO33BLE
using namespace arduino;
#endif

/**
  BufferedOutput.cpp 
  (c)2020 Forward Computing and Control Pty. Ltd.
  This code may be freely used for both private and commerical use.
  Provide this copyright is maintained.
*/

// uncomment this next line to output debug to the stream. This output will BLOCK!!
//#define DEBUG
// DEBUG will show the connect( ) settings
// DEBUG will show -1- extra outputs
// If the mode is BLOCK_IF_FULL then -1- will be output each time the output blocks a write, so delaying the loop()

/**
      use
      createBufferedOutput(name, size, mode);
      instead of calling this constructor

      BufferedOutput(const uint32_t _baudRate, uint8_t *_buf, size_t _bufferSize, BufferedOutputMode _mode, bool _allOrNothing  = true) ;
      You must call one of the BufferedOutput methods read, write, available.. , peek, flush, bytesToBeSent each loop() in order to release the buffered chars
      Usually just call bufferedStream.available(); at the top of loop()

       buf -- the user allocated buffer to store the bytes, must be at least bufferSize long.  Defaults to an internal 64 char buffer if buf is omitted or NULL
       bufferSize -- number of bytes to buffer,max bufferSize is limited to 32766. Defaults to an internal 64 char buffer if bufferSize is < 8 or is omitted
       mode -- BLOCK_IF_FULL, DROP_UNTIL_EMPTY or DROP_IF_FULL
               BLOCK_IF_FULL,    like normal print, but with a buffer. Use this to see ALL the output, but will block the loop() when the output buffer fills
               DROP_UNTIL_EMPTY, when the output buffer is full, drop any more chars until it completely empties.  ~~<CR><NL> is inserted in the output to show chars were dropped.
                                   Useful when there too much output.  It allow multiple prints to be output consecutively to give meaning full output
                                   avaliableForWrite() will return 0 from when the buffer fills until is empties
               DROP_IF_FULL,     when the output buffer is full, drop any more chars until here is space.  ~~<CR><NL> is inserted in the output to show chars were dropped.
       allOrNothing -- defaults to true,  If true AND output buffer not empty then if write(buf,size) will not all fit don't output any of it.
                                      Else if false OR output buffer is empty then write(buf,size) will output partial data to fill output buffer.
                       allOrNothing setting is ignored if mode is BLOCK_IF_FULL
*/
BufferedOutput::BufferedOutput( size_t _bufferSize, uint8_t _buf[],  BufferedOutputMode _mode, bool _allOrNothing) {
  rb_buf = NULL;
  rb_bufSize = 0; // prevents access to a NULL buf
  rb_clear();
  serialPtr = NULL;
  streamPtr = NULL;
  debugOut = NULL;
  txBufferSize = 0;
  dropMarkWritten = false;
  lastCharWritten = ' ';
  baudRate = 0;
  mode = _mode; // default DROP_IF_FULL if not passed in
  if ((mode < 0) || (mode > 2)) { // not really needed as compiler should catch this but..
    mode = DROP_IF_FULL;
  }
  allOrNothingSetting = _allOrNothing;
  allOrNothing = false; // reset after first write(buf,size) // default true if not passed in call.
  waitForEmpty = false; // can write now
  if ((_buf == NULL) || (_bufferSize < 8)) {
    // use default
    rb_init(defaultBuffer, sizeof(defaultBuffer));
  } else {
    rb_init(_buf, _bufferSize);
  }
}


/**
        void connect(HardwareSerial& _serial); // the output to write to, can also read from
            serial -- the HardwareSerial to buffer output to, usually Serial.
                     You must call nextByteOut() each loop() in order to release the buffered chars. 
*/
void BufferedOutput::connect(HardwareSerial& _serial) { // the output to write to, can also read from
  serialPtr = &_serial;
  streamPtr = serialPtr;
  debugOut = streamPtr;
  delay(10); // wait for a few ms for Tx buffer to clear if flush() does not do it
  serialPtr->flush(); // try and clear hardware buffer
  size_t avail = serialPtr->availableForWrite();
  if (txBufferSize < (size_t)avail) txBufferSize = avail;
  baudRate = 0;
  if (txBufferSize == 0) {
    // need baud rate
    while (1) {
      streamPtr->println("availableForWrite() returns 0");
      streamPtr->println("You need to specify the I/O baudRate");
      streamPtr->println("and add extra calls to nextByteOut() as only one byte is released each call.");
      delay(10000);
    }
  } // else use avaiableForWrite to throttle I/O
  uS_perByte = 0;
#ifdef DEBUG
  if (debugOut) {
    debugOut->print("BufferedOutput connected with Serial. Combined buffer size:"); debugOut->print(getSize()); debugOut->println("");
    debugOut->print(" consisting of BufferedOutput buffer "); debugOut->print(rb_getSize()); debugOut->print(" and Serial Tx buffer "); debugOut->println(txBufferSize);
    debugOut->print(" using Serial's availableForWrite to throttle output");
    debugOut->println();
  }
#endif // DEBUG 
  clear();
}
    
/**
        void connect(Stream& _stream, const uint32_t baudRate); // write to and how fast to write output, can also read from
            stream -- the stream to buffer output to
            baudRate -- the maximum rate at which the bytes are to be released.  Bytes will be relased slower depending on how long your loop() method takes to execute
                         You must call nextByteOut() each loop() in order to release the buffered chars. 
*/
void BufferedOutput::connect(Stream& _stream, const uint32_t _baudRate) {
  serialPtr = NULL;
  streamPtr = &_stream;
  debugOut = streamPtr;
  delay(10); // wait for a few ms for Tx buffer to clear if flush() does not do it
  streamPtr->flush(); // try and clear hardware buffer
  baudRate = _baudRate;
  if (baudRate == 0) {  // no baudrate
    while (1) {
      streamPtr->println("connect(stream,baudrate) must have a non-zero baudrate");
      delay(10000);
    }
  }
  // else   // have a baudrate
  txBufferSize = 0; // ignore stream buffer
  uS_perByte = ((unsigned long)(13000000.0 / (float)baudRate)) + 1; // 1sec / (baud/13) in uS  baud is in bits
  // => ~13bits/byte, i.e. start+8+parity+2stop+1  may be less if no parity and only 1 stop bit
  sendTimerStart = micros();
#ifdef DEBUG
  if (debugOut) {
    debugOut->print("BufferedOutput connected with "); debugOut->print(getSize()); debugOut->println(" byte buffer.");
    debugOut->print(" BaudRate:");  debugOut->print(baudRate);
    debugOut->print(" Send interval "); debugOut->print(uS_perByte); debugOut->print("uS");
    debugOut->println();
  }
#endif // DEBUG   
  clear();
}

// allow 4 for dropMark
size_t BufferedOutput::getSize() {
  return rb_getSize() - 4 + txBufferSize;
}

// clears space in outgoing (write) buffer, by removing last bytes written,  if len == 0 clear whole buffer, Serial Tx buffer is NOT changed
void BufferedOutput::clearSpace(size_t len) {
  size_t initLen = len;
  waitForEmpty = false;
  allOrNothing = false; // force something next write
  if (len == 0) {
    clear(); // clears waitForEmpty and allOrNothing
    return;
  }
  len += 8; // should be only 4 for the drop mark but...
  if (internalAvailableForWrite() > len) {
    return; // have space
  }
  if (rb_clearSpace(len)) {
    dropMarkWritten = false;
    writeDropMark();
  }
}

// only clears the BufferedOutput buffer not any HardwareSerial buffer
void BufferedOutput::clear() {
  bool notEmpty = (rb_available() != 0);
  rb_clear();
  if (notEmpty) {
    dropMarkWritten = false;
    if (!dropMarkWritten) {
      writeDropMark();
    }
  }
  waitForEmpty = false;
  allOrNothing = false; // force something next write
}

// ignores waitForWrite
int BufferedOutput::internalAvailableForWrite() {
  if (!streamPtr) {
    return 0;
  }
  int rtn = 0;
  if (serialPtr) {
    size_t avail = serialPtr->availableForWrite();
    rtn += avail;
  }
  int ringAvail = rb_availableForWrite();
  if (ringAvail <= 4) {
    ringAvail = 0;
  } else {
    ringAvail -= 4;
  }
  rtn += ringAvail;
  return rtn;
}

// allow 4 for dropMark and return if waitForEmpty
int BufferedOutput::availableForWrite() {
  if (!streamPtr) {
    return 0;
  }
  nextByteOut(); // try sending first to free some buffer space
  int rtn = 0;
  if (waitForEmpty) {
    return 0;
  } // else
  if (serialPtr) {
    size_t avail = serialPtr->availableForWrite();
    rtn += avail;
  }
  int ringAvail = rb_availableForWrite();
  if (ringAvail <= 4) {
    ringAvail = 0;
  } else {
    ringAvail -= 4;
  }
  rtn += ringAvail;
  return rtn;
}

size_t BufferedOutput::terminateLastLine() {
  if (lastCharWritten != '\n') {
    if (internalAvailableForWrite() > 2) { 
      return write((const uint8_t*)"\r\n", 2);
    } else {
      return write('\n'); // may only be one space left 
    }
  }
  return 0; // nothing written
}

// NOTE: if DROP_UNTIL_EMPTY and allOrNothing == true,
//      then when buffer, pretend allOrNothing == false so that will get some output
size_t BufferedOutput::write(const uint8_t *buffer, size_t size) {
  if (!streamPtr) {
    return 0;
  }

  if (mode == BLOCK_IF_FULL) { // ignores all or nothing
    for (size_t i = 0; i < size; i++) {
      lastCharWritten = buffer[i];
      write(lastCharWritten); // sets dropMarkWritten = false; and calls nextByteOut each time
    }
    return size;
  } // else not BLOCK_IF_FULL

  // else not BLOCK_IF_FULL so either DROP_IF_FULL or DROP_UNTIL_EMPTY
  if (mode != DROP_UNTIL_EMPTY) {
    waitForEmpty = false; // always
  }

  size_t btbs = bytesToBeSent(); // calls nextByteOut and sets clears waitForEmpty in necessary
  if (waitForEmpty) {
    if (!dropMarkWritten) {
      writeDropMark();
    }
    return 0;
  }
  // check for full writes only
  if ( (btbs != 0) && allOrNothing &&
       // availableForWrite returns 0 if waitForEmpty
       (availableForWrite() < ((int)(size))) ) { // leave 4 for next write attempt drop mark
    if (!dropMarkWritten) {
      writeDropMark();
    }
    waitForEmpty = true;
    return 0;
  } // else  writing a partial at least

  // reduce size to fit
  size_t initSize = size;
  size_t strWriteLen = 0; // nothing written yet
  if ((rb_available() == 0) && serialPtr) {
    size_t avail = serialPtr->availableForWrite();
    strWriteLen = size; // try to write it all
    if (avail < strWriteLen) { // only write some of it
      strWriteLen = avail;
    }
    streamPtr->write(buffer, strWriteLen);
    lastCharWritten = buffer[strWriteLen - 1];
    dropMarkWritten = false;
    buffer += strWriteLen; // update buffer
    size -= strWriteLen;  // reduce size to be written
  }
  size_t rbWriteLen = size; // try to write what is left (may be 0) to ringBuffer
  if (rbWriteLen != 0) {
    if (rb_availableForWrite() < ((int)(rbWriteLen + 4))) { // leave 4 for next write attempt drop mark
      // reduce size to get partial write
      if (rb_availableForWrite() < 4) {
        rbWriteLen = 0;
      } else {
        rbWriteLen = rb_availableForWrite() - 4;
      }
    }
    for (size_t i = 0; i < rbWriteLen; i++) {
      lastCharWritten = buffer[i];
      rb_write(lastCharWritten);
      dropMarkWritten = false;
    }
  } // else all written to Serial Tx buffer and so ringBuffer is empty
  
  if ((rbWriteLen + strWriteLen) < initSize) { // dropped something
    if (!dropMarkWritten) {
      writeDropMark();
    }
    waitForEmpty = true;
  }
  allOrNothing = allOrNothingSetting; // cleared on clear() and makeSpace, reset to input setting
  return rbWriteLen + strWriteLen;
}

size_t BufferedOutput::write(uint8_t c) {
  if (!streamPtr) {
    return 0;
  }
#ifdef DEBUG
  bool showDelay = true;
#endif // DEBUG    

  if (mode != BLOCK_IF_FULL) {
    nextByteOut(); // try sending first to free some buffer space
    if (mode == DROP_IF_FULL) {
      waitForEmpty = false; // always
    }
    if ((waitForEmpty) || (rb_availableForWrite() <= 4)) {
      if (!dropMarkWritten) {
        writeDropMark();
      }
      return 0;
    }
    // else have some ringBuffer space
    if ((serialPtr) && (rb_available() == 0)) {
      if (serialPtr->availableForWrite()) {
        lastCharWritten = c;
        streamPtr->write(lastCharWritten);
        dropMarkWritten = false;
        return 1;
      }
    }
    // else now stream Tx buffer space
    if (rb_availableForWrite() > 4) { // leave space for next |\r\n
      dropMarkWritten = false;
      lastCharWritten = c;
      return rb_write(lastCharWritten);
    } else {
      if (!dropMarkWritten) {
        writeDropMark();
      }
      waitForEmpty = true;
      return 0;
    }

  } else { // may block but no drop marks here
    dropMarkWritten = false; // something will be written!!
    if (rb_availableForWrite() != 0) {
      lastCharWritten = c;
      return rb_write(lastCharWritten);
    } else { // block here
      while (rb_availableForWrite() == 0) {
        // spin
#ifdef DEBUG
        if (showDelay) {
          showDelay = false; // only show this once per write
          if (debugOut) {
            debugOut->print("-1-"); // indicate write( ) is delaying the loop()
          }
        }
#endif // DEBUG    
        delay(1); // wait 1mS, expect this to call yield() for those boards that need it e.g. ESP8266 and ESP32
        nextByteOut(); // try sending first to free some buffer space
      }
      lastCharWritten = c;
      return rb_write(lastCharWritten);
    }
  }
}

// private so no need to check streamPtr here
size_t BufferedOutput::bytesToBeSent() {
  //nextByteOut(); don't call this here as may loop
  size_t btbs = (size_t)rb_available();
  if (serialPtr) {
    size_t avail = (size_t)serialPtr->availableForWrite();
    if (txBufferSize < (size_t)avail) txBufferSize = avail;
    btbs += (txBufferSize - avail);
  }
  return btbs;
}

// NOTE nextByteOut will block if baudRate is set higher then actual i/o baudrate
void BufferedOutput::nextByteOut() {
  if (!streamPtr) {
    return;
  }
  if (rb_available() == 0) { // nothing to send from ringBuffer
    if (serialPtr) { // have HardwareSerial 
      if ((serialPtr->availableForWrite() >= txBufferSize)) {
    	// Serial tx buffer empty, all of txBufferSize availableForWrite
        waitForEmpty = false; // both buffers empty
      }
    } else { // no serialPtr so cannot call availableForWrite, just using ringBuffer and baudrate
       waitForEmpty = false; // ringBuffer empty
       // not HardwareSerial so using baudrate to release bytes instead of availableForWrite()
       sendTimerStart = micros(); // restart baudrate release timer
    } 
    return; // nothing to release
  }

  if (serialPtr) { // common case use streamPtr->avaiableForWrite() to throttle output
    // check if space available and fill from ringBuffer
    // some boards return 0 for availableForWrite
    while (serialPtr->availableForWrite() && rb_available()) {
      serialPtr->write((uint8_t)rb_read());
    }
    int btbs = (size_t)rb_available();
    if (btbs) {
      return;
    }
    int avail = (size_t)serialPtr->availableForWrite();
    if (txBufferSize < ((size_t)avail)) txBufferSize = avail; // incase calls to availableForWrite() in connect did not return full Txbuffer size
    btbs += (txBufferSize - avail);
    if (btbs <= 0) {
      waitForEmpty = false;
    }
    return;
  } // else

  // serialPtr == NULL and txBufferSize == 0 so use timer to throttle output
  // sendTimerStart will have been set above

  unsigned long uS = micros();
  // micros() has 8uS resolution on 8Mhz systems, 4uS on 16Mhz system
  // NOTE throw away any excess of (uS - sendTimerStart) > uS_perByte
  // output will be slower then specified
  if ((uS - sendTimerStart) < uS_perByte) {
    return; // nothing to do not time to release next byte
  }
  // else send next byte
  sendTimerStart = uS; //releasing next byte, restart timer
  streamPtr->write((uint8_t)rb_read()); // may block if set baudRate higher then actual I/O baud rate
  if (rb_available() == 0) {
    waitForEmpty = false;
  }
}

// always expect there to be at least 4 spaces available in the ringBuffer when this is called
void BufferedOutput::writeDropMark() {
  rb_write((const uint8_t*)"~~\r\n", 4); // will truncate if not enough room
  dropMarkWritten = true;
}


int BufferedOutput::available() {
  if (!streamPtr) {
    return 0;
  }
  nextByteOut();
  return streamPtr->available();
}

int BufferedOutput::read() {
  if (!streamPtr) {
    return -1; // -1
  }
  nextByteOut();
  return streamPtr->read();
}

int BufferedOutput::peek() {
  if (!streamPtr) {
    return -1; // -1
  }
  nextByteOut();
  return streamPtr->peek();
}

// this blocks!!
void BufferedOutput::flush() {
  if (!streamPtr) {
    return;
  }
  while (bytesToBeSent() != 0) {
    nextByteOut();
  }
}

//===============  ringBuffer methods ==============
// write() will silently fail if ringbuffer is full

/**
   _buf must be at least _size in length
   _size is limited to 32766
   assumes size_t is atleast 16bits as specified by C spec
*/
void BufferedOutput::rb_init(uint8_t* _buf, size_t _size) {
  rb_clear();
  if ((_buf == NULL) || (_size == 0)) {
    rb_buf = _buf;
    rb_bufSize = 0; // prevents access to a NULL buf
  } else {
    // available etc returns int, check that _size fits in int
    // limit _size to max int16_t - 1
    if (_size >= 32766) {
      _size = 32766; // (2^16/2)-1 minus 1 since uint16_t vars used
    }
    rb_buf = _buf;
    rb_bufSize = _size; // buffer_count use to detect buffer full
  }
}

void BufferedOutput::rb_clear() {
  rb_buffer_head = 0;
  rb_buffer_tail = rb_buffer_head;
  rb_buffer_count = 0;
}

/*
   This should return size_t,
   but someone stuffed it up in the Arduino libraries
*/
// defined in BufferedOutput.h in BufferedOutputRingBuffer class declaration
//int BufferedOutput::available() { return buffer_count; }


size_t BufferedOutput::rb_getSize() {
  return rb_bufSize;
}

/*
   This should return size_t,
   but someone stuffed it up in the Arduino libraries
*/
// defined in BufferedOutput.h in BufferedOutputRingBuffer class declaration
int BufferedOutput::rb_availableForWrite() {
  return (rb_bufSize - rb_buffer_count);
}


int BufferedOutput::rb_peek() {
  if (rb_buffer_count == 0) {
    return -1;
  } else {
    return rb_buf[rb_buffer_tail];
  }
}


void BufferedOutput::rb_dump(Stream * streamPtr) {
  if (streamPtr == NULL) {
    return;
  }
  size_t idx = rb_buffer_tail;
  size_t count = rb_buffer_count;
  while (count > 0) {
    unsigned char c = rb_buf[idx];
    idx = rb_wrapBufferIdx(idx);
    // if (buffer_count > 0) { checked above
    count--;
    //streamPtr->print(" "); streamPtr->print(idx);  streamPtr->print(":");
    streamPtr->print((char)c);
  }
  streamPtr->println("-");
}

int BufferedOutput::rb_read() {
  if (rb_buffer_count == 0) {
    return -1;
  } else {
    unsigned char c = rb_buf[rb_buffer_tail];
    rb_buffer_tail = rb_wrapBufferIdx(rb_buffer_tail);
    // if (buffer_count > 0) { checked above
    rb_buffer_count--;
    return c;
  }
}

size_t BufferedOutput::rb_write(const uint8_t *_buffer, size_t _size) {
  if (_size > ((size_t)rb_availableForWrite())) {
    _size = rb_availableForWrite();
  }
  for (size_t i = 0; i < _size; i++) {
    rb_internalWrite(_buffer[i]);
  }
  return _size;
}

size_t BufferedOutput::rb_write(uint8_t b) {
  // check for buffer full
  if (rb_buffer_count >= rb_bufSize) {
    return 0;
  }
  // else
  rb_internalWrite(b);
  return 1;
}

void BufferedOutput::rb_internalWrite(uint8_t b) {
  // check for buffer full done by caller
  rb_buf[rb_buffer_head] = b;
  rb_buffer_head = rb_wrapBufferIdx(rb_buffer_head);
  rb_buffer_count++;
}

uint16_t BufferedOutput::rb_wrapBufferIdx(uint16_t idx) {
  if (idx >= (rb_bufSize - 1)) {
    // wrap around
    idx = 0;
  } else {
    idx++;
  }
  return idx;
}

// clears space in outgoing (write) buffer, by removing last bytes written,  if len == 0 clear whole buffer, Serial Tx buffer is NOT changed
// returns true if some output dropped
bool BufferedOutput::rb_clearSpace(size_t len) {
  if ((len == 0) || (len >= rb_bufSize)) {
    rb_clear();
    return true;
  }
  int avail = rb_availableForWrite();
  if (len <= avail) {
    return false;
  }
  // else avail < len
  size_t tobedropped = len - (size_t)(avail);
  for (; tobedropped > 0; tobedropped--) {
    rb_unWrite();
  }
  return true;
}

void BufferedOutput::rb_unWrite() {
  if (rb_buffer_count == 0) {
    return; // empty
  }
  // else
  rb_buffer_head--; // will wrap to very large number if idx == 0
  rb_buffer_count--;
  if (rb_buffer_head > (rb_bufSize - 1)) {
    rb_buffer_head = (rb_bufSize - 1);
  }
}


