/*
  // The SafeString class
  //
// -----------------  creating SafeStrings ---------------------------------
// createSafeString(name, size) and createSafeString(name, size, "initialText") 
// are utility macros to create an SafeString of a given name and size and optionally, an initial value
//
// createSafeString(str, 40);
// expands in the pre-processor to
//   char str_SAFEBUFFER[40+1];
//   SafeString str(sizeof(str_SAFEBUFFER),str_SAFEBUFFER,"","str");
//
// createSafeString(str, 40, "test");
// expands in the pre-processor to
//   char str_SAFEBUFFER[40+1];
//   SafeString str(sizeof(str_SAFEBUFFER),str_SAFEBUFFER,"test","str");

//
// string = .. works for signed/unsigned ints, char*, char, F(".."), SafeString float, double etc
// string.concat(..) and string.prefix(..) also work for those
// string.stoken(..) can be used to split a string in to tokens
//
// SafeStrings are never invalid, even if called with invalid arguments.


  SafeString.h static memory SafeString library modified by
  Matthew Ford
  Mods Copyright(c)2020 Forward Computing and Control Pty. Ltd.
  All rights reservered subject to the License below

  modified from
  WString.h - String library for Wiring & Arduino
  ...mostly rewritten by Paul Stoffregen...
  Copyright (c) 2009-10 Hernando Barragan.  All right reserved.
  Copyright 2011, Paul Stoffregen, paul@pjrc.com

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef SafeString_class_h
#define SafeString_class_h
#ifdef __cplusplus

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ESP8266 defines an nl() macro which interfers with SafeString's nl() method 
#ifdef nl
#undef nl
// define here to raise re-define warning if esp8266 included later
#define nl() nl()
#endif

#if defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_ESP8266)
#include <pgmspace.h>
#elif defined(ARDUINO_ARDUINO_NANO33BLE) 
#include <api/deprecated-avr-comp/avr/pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif

#include <stdint.h>
#include <Print.h>
#include <Printable.h>

#ifdef ARDUINO_ARDUINO_NANO33BLE
namespace arduino {
#endif

class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper *>(PSTR(string_literal)))

// to remove all the error messages, comment out
#define SSTRING_DEBUG
// this saves program bytes and the ram used by the SafeString object names
//
// usually just leave as is and use SafeString::setOutput(..) to control the error messages and debug output
// there will be no error messages or debug output if SafeString::setOutput(..) has not been called from your sketch
//
// SafeString.debug() is always available regardless of the SSTRING_DEBUG define setting
//   but SafeString::setOutput() still needs to be called

// -----------------  creating SafeStrings ---------------------------------
// createSafeString(name, size) and createSafeString(name, size, "initialText") 
// are utility macros to create an SafeString of a given name and size and optionally, an initial value
//
// createSafeString(str, 40);
// expands in the pre-processor to
//   char str_SAFEBUFFER[40+1];
//   SafeString str(sizeof(str_SAFEBUFFER),str_SAFEBUFFER,"","str");
//
// createSafeString(str, 40, "test");
// expands in the pre-processor to
//   char str_SAFEBUFFER[40+1];
//   SafeString str(sizeof(str_SAFEBUFFER),str_SAFEBUFFER,"test","str");

#ifdef SSTRING_DEBUG
#define createSafeString(name, size,...) char name ## _SAFEBUFFER[(size)+1]; SafeString name(sizeof(name ## _SAFEBUFFER),name ## _SAFEBUFFER, ""  __VA_ARGS__, #name);
#else
#define createSafeString(name, size,...) char name ## _SAFEBUFFER[(size)+1]; SafeString name(sizeof(name ## _SAFEBUFFER),name ## _SAFEBUFFER, ""  __VA_ARGS__);
#endif


// arrays are indexed by a size_t varialble which for 8bit Arduino boards e.g. UNO,MEGA  is unsigned int but can be larger for more powerful chips.
class SafeString : public Printable, public Print { 

  public:
    explicit SafeString(size_t maxLen, char *buf, const char* cstr, const char* _name = NULL);
    
  private: // to force compile errors if function definition of the SafeString argument is not a refernce, i.e. not SafeString&   
    SafeString(const SafeString& other ); // You must declare SafeStrings function arguments as a reference, SafeString&,  e.g. void test(SafeString& strIn)
    // NO other constructors, NO conversion constructors
    
  public:
    // call setOutput( ) to turn on Error msgs and debug( ) output for all SafeStrings
    // This also sets output for the debug( ) method
    // verbose is an optional argument, if missing defaults to true, use false for compact error messages or call setVerbose(false)
    static void setOutput(Print& debugOut, bool verbose = true);
    // static SafeString::DebugPrint Output;  // a Print object controlled by setOutput() / turnOutputOff() is defined at the bottom
    
    static void turnOutputOff(void);     // call this to turn all debugging OFF, both error messages AND debug( ) method output
    
    // use this to control error messages verbose output
    static void setVerbose(bool verbose); // turn verbose error msgs on/off.  setOutput( ) sets verbose to true

    // these methods print out info on this SafeString object, iff setOutput has been called
    // setVerbose( ) does NOT effect these methods which have their own verbose argument
    // Each of these debug( ) methods defaults to outputing the string contents.  Set the optional verbose argument to false to suppress outputing string contents
    // NOTE!! all these debug methods return a pointer to an empty string. 
    // This is so that if you add .debug() to Serial.println(str);  i.e. Serial.println(str.debug()) will work as expected
    const char* debug(bool verbose = true);  
    const char* debug(const char* title, bool verbose = true);
    const char* debug(const __FlashStringHelper *title, bool verbose = true);
    const char* debug(const SafeString &stitle, bool verbose = true);

    virtual size_t write(uint8_t b);
    virtual size_t write(const uint8_t *buffer, size_t length);
    
    size_t printTo(Print& p) const;
    
    inline size_t length(void) const {
      return len;
    }
    inline size_t capacity(void) const {
      return _capacity;
    }
    inline bool isFull(void) const {
      return (length() == capacity());
    }
    inline bool isEmpty(void) const {
      return length() == 0;
    }
    inline bool availableForWrite(void) const {
      return (capacity() - length());
    }

    SafeString & clear(void);


    // use a function pointer to allow for "if (s)" without the
    // complications of an operator bool(). for more information, see:
    // http://www.artima.com/cppsource/safebool.html

  public:
    size_t print(unsigned char, int = DEC);
    size_t print(int, int = DEC);
    size_t print(unsigned int, int = DEC);
    size_t print(long, int = DEC);
    size_t print(unsigned long, int = DEC);
    size_t print(double, int = 2);
    size_t print(const __FlashStringHelper *);
    size_t print(const char[]);
    size_t print(char);
    size_t print(SafeString &str);

    size_t println(unsigned char, int = DEC);
    size_t println(int, int = DEC);
    size_t println(unsigned int, int = DEC);
    size_t println(long, int = DEC);
    size_t println(unsigned long, int = DEC);
    size_t println(double, int = 2);
    size_t println(const __FlashStringHelper *);
    size_t println(const char[]);
    size_t println(char);
    size_t println(SafeString &str);
    size_t println(void);

    // creates a copy of the assigned value.
    // if the value is null or invalid,
    // or too large to be fit in the string's internal buffer
    // the string will be left empty
    SafeString & operator = (char c);
    SafeString & operator = (unsigned char c);
    SafeString & operator = (int num);
    SafeString & operator = (unsigned int num);
    SafeString & operator = (long num);
    SafeString & operator = (unsigned long num);
    SafeString & operator = (float num);
    SafeString & operator = (double num);

    SafeString & operator = (const SafeString &rhs);
    SafeString & operator = (const char *cstr);
    SafeString & operator = (const __FlashStringHelper *str);

    SafeString & prefix(const SafeString &s);
    SafeString & prefix(const char *cstr);
    SafeString & prefix(char c);
    SafeString & prefix(unsigned char c);
    SafeString & prefix(int num);
    SafeString & prefix(unsigned int num);
    SafeString & prefix(long num);
    SafeString & prefix(unsigned long num);
    SafeString & prefix(float num);
    SafeString & prefix(double num);
    SafeString & prefix(const __FlashStringHelper * str);
    SafeString & prefix(const char *cstr, size_t length);
    SafeString & prefix(const __FlashStringHelper * str, size_t length);

    // concatenate (works w/ built-in types)

    // returns true on success, false on failure (in which case, the string
    // is left unchanged).  if the argument is null or invalid, the
    // concatenation is considered unsucessful.
    SafeString & concat(const SafeString &str);
    SafeString & concat(const char *cstr);
    SafeString & concat(char c);
    SafeString & concat(unsigned char c);
    SafeString & concat(int num);
    SafeString & concat(unsigned int num);
    SafeString & concat(long num);
    SafeString & concat(unsigned long num);
    SafeString & concat(float num);
    SafeString & concat(double num);
    SafeString & concat(const __FlashStringHelper * str);
    SafeString & concat(const char *cstr, size_t length);
    SafeString & concat(const __FlashStringHelper * str, size_t length);
    SafeString & nl(); // append newline \r\n same as concat("\r\n");

    // if there's not enough memory for the concatenated value, the string
    // will be left unchanged
    // prefix  -=
    SafeString & operator -= (const SafeString &rhs)  {
      prefix(rhs);
      return (*this);
    }
    SafeString & operator -= (const char *cstr)    {
      prefix(cstr);
      return (*this);
    }

    SafeString & operator -= (char c)      {
      prefix(c);
      return (*this);
    }
    SafeString & operator -= (unsigned char num)   {
      prefix(num);
      return (*this);
    }
    SafeString & operator -= (int num)     {
      prefix(num);
      return (*this);
    }
    SafeString & operator -= (unsigned int num)    {
      prefix(num);
      return (*this);
    }
    SafeString & operator -= (long num)      {
      prefix(num);
      return (*this);
    }
    SafeString & operator -= (unsigned long num) {
      prefix(num);
      return (*this);
    }
    SafeString & operator -= (float num)   {
      prefix(num);
      return (*this);
    }
    SafeString & operator -= (double num)    {
      prefix(num);
      return (*this);
    }
    SafeString & operator -= (const __FlashStringHelper *str) {
      prefix(str);
      return (*this);
    }
    // suffix/append +=
    SafeString & operator += (const SafeString &rhs)  {
      concat(rhs);
      return (*this);
    }
    SafeString & operator += (const char *cstr)    {
      concat(cstr);
      return (*this);
    }
    SafeString & operator += (char c)      {
      concat(c);
      return (*this);
    }
    SafeString & operator += (unsigned char num)   {
      concat(num);
      return (*this);
    }
    SafeString & operator += (int num)     {
      concat(num);
      return (*this);
    }
    SafeString & operator += (unsigned int num)    {
      concat(num);
      return (*this);
    }
    SafeString & operator += (long num)      {
      concat(num);
      return (*this);
    }
    SafeString & operator += (unsigned long num) {
      concat(num);
      return (*this);
    }
    SafeString & operator += (float num)   {
      concat(num);
      return (*this);
    }
    SafeString & operator += (double num)    {
      concat(num);
      return (*this);
    }
    SafeString & operator += (const __FlashStringHelper *str) {
      concat(str);
      return (*this);
    }


    // comparison (only works with Strings and "strings")
    int compareTo(const SafeString &s) const;
    int compareTo(const char *cstr) const;
    bool equals(const SafeString &s) const;
    bool equals(const char *cstr) const;
    bool equals(const char c) const;
    bool operator == (const SafeString &rhs) const {
      return equals(rhs);
    }
    bool operator == (const char *cstr) const {
      return equals(cstr);
    }
    bool operator == (const char c) const {
      return equals(c);
    }
    bool operator != (const SafeString &rhs) const {
      return !equals(rhs);
    }
    bool operator != (const char *cstr) const {
      return !equals(cstr);
    }
    bool operator != (const char c) const {
      return !equals(c);
    }
    bool operator <  (const SafeString &rhs) const;
    bool operator >  (const SafeString &rhs) const;
    bool operator <= (const SafeString &rhs) const;
    bool operator >= (const SafeString &rhs) const;
    bool operator <  (const char*) const;
    bool operator >  (const char*) const;
    bool operator <= (const char*) const;
    bool operator >= (const char*) const;
    bool equalsIgnoreCase(const SafeString &s) const;
    bool equalsIgnoreCase(const char *str2) const;
    bool equalsConstantTime(const SafeString &s) const;

    // fromIndex is offset into this string where check is to start
    // For these methods 0 to length() is valid for fromIndex
    // if fromIndex == length() return 0 (false)
    bool startsWith( const char *str2) const;
    bool startsWith( const char *str2, size_t fromIndex ) const;
    bool startsWithIgnoreCase( const char *str2 ) const;
    bool startsWithIgnoreCase( const char *str2, size_t fromIndex ) const;
    bool startsWith( const SafeString &s2) const;
    bool startsWith(const SafeString &s2, size_t fromIndex) const;
    bool startsWithIgnoreCase( const SafeString &s2 ) const;
    bool startsWithIgnoreCase( const SafeString &s2, size_t fromIndex ) const;
    bool endsWith(const SafeString &suffix) const;
    bool endsWith(const char *suffix) const;
    bool endsWithCharFrom(const SafeString &suffix) const;
    bool endsWithCharFrom(const char *suffix) const;

    // character acccess
    // NOTE: There is no access to modify the underlying char buffer directly
    // For these methods 0 to length()-1 is valid for index
    // Other values will print errors if debug enabled
    char charAt(size_t index) const; // if index >= length() returns 0 and prints a error msg
    char operator [] (size_t index) const; // if index >= length() returns 0 and prints a error msg
    // str[..] = c;  is not supported because it allows direct access to modify the underlying char buffer
    
    SafeString & setCharAt(size_t index, char c); // calls charAt(length(), ..) or charAt(.. , '\0') are ignored but print an error msg
    
    const char* c_str() const {
      return buffer;
    }

    // search
    /**********************************************
      Search
      All indexOf return length() if not found (or length()+1 on error)
      so test with
      size_t a_idx = str.indexOf('a');
      if (a_idx >= str.length()) { // not found

      DO NOT use the test
      if (a_idx < 0) { is NEVER true,  size_t variable is ALWAYS >= 0
    **********************************************/
    // 0 to length() is valid for fromIndex;
    // if fromIndex == length(), length() will be returned
    size_t indexOf( char ch ) const;
    size_t indexOf( char ch, size_t fromIndex ) const;
    size_t indexOf( const SafeString &str ) const;
    size_t indexOf( const char* str ) const;
    size_t indexOf(const char* str , size_t fromIndex) const;
    size_t indexOf( const SafeString &str, size_t fromIndex ) const;
    size_t lastIndexOf( char ch ) const;
    size_t lastIndexOf( char ch, size_t fromIndex ) const;
    size_t lastIndexOf( const SafeString &str ) const;
    size_t lastIndexOf( const char *cstr ) const;
    size_t lastIndexOf( const SafeString &str, size_t fromIndex ) const;
    size_t lastIndexOf(const char* cstr, size_t fromIndex) const;
    // first index of the chars listed in chars string
    // loop through chars and look for index of each and return the min OR -1 if none found
    size_t indexOfCharFrom(const SafeString &str) const ;
    size_t indexOfCharFrom(const SafeString &str, size_t fromIndex) const ;
    size_t indexOfCharFrom(const char* chars) const ;
    // start searching from fromIndex
    size_t indexOfCharFrom(const char* chars, size_t fromIndex) const ;

    /***** substring ************/
    // 0 to length() is valid for beginIdx;
    // endIdx must be >= beginIdx and <= length()
    // if beginIdx == length(), an empty result will be returned
    SafeString & substring(SafeString &result, size_t beginIdx, size_t endIdx);
    SafeString & substring(SafeString &result, size_t beginIdx);

    // modification
    SafeString & replace(char find, char replace);
    SafeString & replace(const SafeString& find, const SafeString& replace);
    SafeString & replace(const char* find, const char *replace);
    
    // 0 to length() is valid for index
    // 0 to (length()- index) is valid for count
    SafeString & remove(size_t index); // remove from index to end of SafeString
    SafeString & remove(size_t index, size_t count); // remove count chars starting from index
    
    SafeString & removeLast(size_t count); // remove the last 'count' chars
    SafeString & keepLast(size_t count); // keep last 'count' number of chars remove the rest
    
    SafeString & toLowerCase(void);
    SafeString & toUpperCase(void);
    SafeString & trim(void); // trims front and back

    // parsing/conversion
    bool toLong(long &l) const;
    bool toFloat(float  &f) const;
    bool toDouble(double  &d) const;

    // break into tokens using chars in delimiters string as delimiters
    // if useAsDelimiters is true (default) the token is terminated when of those chars read
    // if useAsDelimiters is false, the token is terminated when of a char not in the delimiters is read
    // call with useAsDelimiters false to skip over delimiters after finding a token
    // fromIndex is where to start the search from
    // the found token is returned in the token SafeString
    // return -- the next index in this SafeString after the end of the token just found
    //           use this as the startidx for the next call
    //           length() is returned if no token found and token is cleared
    // errors return length()+1 and clear token
    // NOTE: if nextIndex < length(), charAt( return value ) gives the delimiter. Use nextIndex++ to step over it.
    // see the SafeString_stoken example sketch
    size_t stoken(SafeString &token, size_t fromIndex, const char* delimiters, bool useAsDelimiters = true);
    size_t stoken(SafeString &token, size_t fromIndex, const SafeString &delimiters, bool useAsDelimiters = true);

    // nextToken
    //  parse this SafeString via delimiters and return the first delimited token found
    //  that token and its delimiters are removed from the SafeString
    // params
    //  token - the SafeString to return the token in, it is cleared in no delimited token found
    //          the token's capacity must be >= this SafeString's capacity 
    //  delimiters - the characters that any one of which can delimit a token
    //
    // returns true if find a token in this SafeString that is terminated by one of the delimiters
    //
    // unterminated tokens are left in the SafeString and not returned.
    bool nextToken(SafeString& token, SafeString &delimiters);
    bool nextToken(SafeString& token, char* delimiters);
    
/**
   size_t readBuffer(char* buffer, size_t fromBufferIndex)
   Fills SafeString with chars from character array
   If buffer[fromBufferIndex] == '\0' OR SafeString was already full, fromIndex is returned unchanged.
   NOTE: this method is NOT robust invalid fromBufferIndex can cause the program to crash

   return -- next index to read

   params
   buffer -- null terminated character array to read from
   fromIndex -- where to start reading from, defaults to 0 read from start of buffer
   Note: if string is already full then nothing will be read and fromIndex will be returned
*/
    size_t readBuffer(const char* buffer, size_t fromIndex = 0);
    
/**
   size_t writeBuffer(char* buffer, size_t bufsize, size_t fromSafeStringIndex)
   copied characters from this SafeString, starting at fromSafeStringIndex, into the buffer.
   At most bufsize-1 chars are written. A terminating '\0' is always written to the buffer
   NOTE: this method is NOT robust. An invalid bufsize can cause the program to crash
   
   return -- the number of chars copied to buffer
   
   params
   buffer -- character array to write to
   buffersize -- size of buffer (including s
   fromSafeStringIndex -- where in the SafeString to start writing from
*/
    size_t writeBuffer(char *buf, size_t bufsize, size_t fromSafeStringIndex = 0) const;

/**
 * bool read(Stream& input)
 * returns true if something added to string else false
 ** Note: if string is already full then nothing will be read and false will be returned
 */
    bool read(Stream& input); 
    
/********************************************
   non-blocking readUntil
  return true if delimiter or string filled, found else false
  if a delimiter is found it is included in the return 

  parameters:
  input - the Stream object to read from
  delimiters - string of valid delimieters
 *********************************************/
    bool readUntil(Stream& input, const char* delimiters);
    bool readUntil(Stream& input, SafeString &delimiters);

  protected:
    static Print* debugPtr;
    static bool fullDebug;
    char *buffer = NULL;          // the actual char array
    size_t _capacity = 0; // the array length minus one (for the '\0')
    size_t len = 0;       // the SafeString length (not counting the '\0')

    class noDebugPrint : public Print {
      inline size_t write(uint8_t b) { (void)(b); return 0;}
      inline size_t write(const uint8_t *buffer, size_t length) {(void)(buffer);(void)(length); return 0;};
    };

    static SafeString::noDebugPrint emptyPrint;
    
    static Print* currentOutput;// = &emptyPrint;
    
    class DebugPrint : public Print {
      size_t write(uint8_t b) {  return currentOutput->write(b);}
      size_t write(const uint8_t *buffer, size_t length) { return currentOutput->write(buffer,length);};
    };
    
  public:
    static SafeString::DebugPrint Output; // a Print object controlled by setOutput() / turnOutputOff()

  protected:
    SafeString & concatln(const __FlashStringHelper * pstr);
    SafeString & concatln(char c);
    SafeString & concatln(const char *cstr, size_t length);
    void outputName() const ;
    
  private:
    const char *name; 
    // reserve returns 0 if _capacity < size
    bool reserve(size_t size);
    static char nullBufferSafeStringBuffer[1];
    static char emptyDebugRtnBuffer[1];
    void debugInternal(bool _fullDebug) const ;
    void debugInternalMsg(bool _fullDebug) const;
    void debugInternalResultMsg(bool _fullDebug) const ;
    void concatErr();
    void prefixErr();
    void capError(const __FlashStringHelper * methodName, size_t neededCap, const char* cstr, const __FlashStringHelper *pstr = NULL, char c = '\0', size_t length = 0);
    void printlnErr();
    void errorMethod(const __FlashStringHelper * methodName) const;
    void outputFromIndexIfFullDebug(size_t fromIndex) const;
};

#ifdef ARDUINO_ARDUINO_NANO33BLE
} // namespace arduino
#endif

#endif  // __cplusplus
#endif  // SafeString_class_h
