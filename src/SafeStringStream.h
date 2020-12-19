#ifndef SAFE_STRING_STREAM_H
#define SAFE_STRING_STREAM_H
/*
  SafeStringStream.h  a Stream wrapper for a SafeString
  by Matthew Ford
  (c)2020 Forward Computing and Control Pty. Ltd.
  This code is not warranted to be fit for any purpose. You may only use it at your own risk.
  This code may be freely used for both private and commercial use.
  Provide this copyright is maintained.
**/
#ifdef __cplusplus
#include <Arduino.h>
#include "SafeString.h"

#ifdef ARDUINO_ARDUINO_NANO33BLE
namespace arduino {
#endif

class SafeStringStream : public Stream {
  public:
    explicit SafeStringStream(); // nothing to send yet
    explicit SafeStringStream(SafeString &sf);
    void begin(const uint32_t baudRate = 0); // start to release at this baud rate, 0 means infinite baudRate
    void begin(SafeString &sf, const uint32_t baudRate = 0); // start to release at this baud rate, 0 means infinite baudRate
    size_t write(uint8_t b);
    int available();
    int read();
    int peek();
    void flush(); // for ESP32 etc
    int availableForWrite();
  private:
  	SafeStringStream(const SafeStringStream& other);
    unsigned long uS_perByte; // == 1000000 / (baudRate/10) == 10000000 / baudRate
    uint32_t baudRate;
    void releaseNextByte();
    unsigned long sendTimerStart;
    char Rx_BUFFER[9]; // 8char + null

  protected:
  	 SafeString *sfPtr;
};



#ifdef ARDUINO_ARDUINO_NANO33BLE
} // namespace arduino
#endif


#endif  // __cplusplus
#endif  // SAFE_STRING_STREAM_H