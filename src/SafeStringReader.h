#ifndef SAFE_STRING_READER_H
#define SAFE_STRING_READER_H
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

// This include handles the rename of Stream for MBED compiles
#if defined(ARDUINO_ARDUINO_NANO33BLE)
  #include <Stream.h>
#elif defined( __MBED__ ) || defined( MBED_H )
  #include <WStream.h>
  #define Stream WStream
#else
  #include <Stream.h>
#endif

// to skip this for SparkFun RedboardTurbo
#ifndef ARDUINO_SAMD_ZERO
#if defined(ARDUINO_ARDUINO_NANO33BLE) || defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_MEGAAVR)
namespace arduino {
#endif
#endif // #ifndef ARDUINO_SAMD_ZERO


#define createSafeStringReader(name, size, ...) \
char name ## _INPUT_BUFFER[(size)+2]; \
char name ## _TOKEN_BUFFER[(size)+2]; \
SafeString name ## _SF_INPUT((size)+2,  name ## _INPUT_BUFFER, "", #name "_InputBuffer"); \
SafeStringReader name(name ## _SF_INPUT, (size)+2, name ## _TOKEN_BUFFER, #name, __VA_ARGS__ );



class SafeStringReader : public SafeString {
public:
    explicit SafeStringReader(SafeString& _sfInput, size_t bufSize, char *tokenBuf, const char* _name, const char* delimiters, bool skipToDelimiterFlag=false, uint8_t echoInput = false, unsigned long timeout_mS = 0 );
    explicit SafeStringReader(SafeString& _sfInput, size_t bufSize, char *tokenBuf, const char* _name, const char delimiter, bool skipToDelimiterFlag=false, uint8_t echoInput = false, unsigned long timeout_mS = 0 );
    void connect(Stream& stream); // clears getReadCount() as well
    bool end(); // returns true if have another token, terminates last token if any, disconnect from stream, turn echo off, set timeout to 0 and clear skipToDelimiter,  clears getReadCount()
   bool read(); // NOTE: this call always clears the SafeStringReader so no need to call clear() on sfReader at end of processing.
  void echoOn();
  void echoOff();
  void flushInput(); // clear any buffered input and Stream RX buffer then skips to next delimiter or times out. Then goes back to normal input processing
  void skipToDelimiter(); // sets skipToDelimiter to true
  void setTimeout(unsigned long mS);
  size_t getReadCount(); // number of chars read since last connect called, cleared when end() called

      /* Assignment operators **********************************
      Set the SafeString to a char version of the assigned value.
      For = (const char *) the contents are copied to the SafeString buffer
      if the value is null or invalid,
      or too large to be fit in the string's internal buffer
      the string will be left empty
     **/
    SafeStringReader & operator = (char c);
    SafeStringReader & operator = (unsigned char c);
    SafeStringReader & operator = (int num);
    SafeStringReader & operator = (unsigned int num);
    SafeStringReader & operator = (long num);
    SafeStringReader & operator = (unsigned long num);
    SafeStringReader & operator = (float num);
    SafeStringReader & operator = (double num);
    SafeStringReader & operator = (SafeString &rhs);
    SafeStringReader & operator = (const char *cstr);
    SafeStringReader & operator = (const __FlashStringHelper *str); // handle F(" .. ") values

  // return the delimiter that terminated the last token
  // only valid when read() returns true
  // will return ((char)-1) is there is none, e.g. timed out or argument error
  char getDelimiter(); 
  const char* debugInputBuffer(bool verbose = true);
  const char* debugInputBuffer(const char* title, bool verbose = true);
  const char* debugInputBuffer(const __FlashStringHelper *title, bool verbose = true);
  const char* debugInputBuffer(SafeString &stitle, bool verbose = true);

  private:
  	SafeStringReader(const SafeStringReader& other);
  	void init(SafeString& _sfInput, const char* delimiters, bool skipToDelimiterFlag, uint8_t echoInput, unsigned long timeout_mS);
  //	void bufferInput(); // get more input
    SafeString* sfInputPtr;
    const char* delimiters;
    bool skipToDelimiterFlag;
    bool echoInput;
    bool flagFlushInput; // true if flushing
    unsigned long timeout_mS;
    bool haveToken; // true if have token but read() not called yet
    Stream *streamPtr;
    size_t charCounter; // counts bytes read, useful for http streams
    char internalCharDelimiter[2]; // used if char delimiter passed
};

// to skip this for SparkFun RedboardTurbo
#ifndef ARDUINO_SAMD_ZERO
#if defined(ARDUINO_ARDUINO_NANO33BLE) || defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_MEGAAVR)
} // namespace arduino
#endif
#endif  // #ifndef ARDUINO_SAMD_ZERO

#endif  // __cplusplus
#endif // SAFE_STRING_READER_H