// !!!!!!!!! WARNING in V2 substring endIdx is EXCLUSIVE !!!!!!!!!! change from V1 inclusive
/*
   The SafeString class V3.0.6
   Note: ESP32 gives warning: "F" redefined which can be ignored

  -----------------  creating SafeStrings ---------------------------------
  See the example sketches SafeString_ConstructorAndDebugging.ino and SafeStringFromCharArray.ino
   and SafeStringFromCharPtr.ino and SafeStringFromCharPtrWithSize.ion

  createSafeString(name, size) and createSafeString(name, size, "initialText")
  are utility macros to create an SafeString of a given name and size and optionally, an initial value

  createSafeString(str, 40);  or  cSF(str, 40);
  expands in the pre-processor to
   char str_SAFEBUFFER[40+1];
   SafeString str(sizeof(str_SAFEBUFFER),str_SAFEBUFFER,"","str");

  createSafeString(str, 40, "test");  or cSF(str, 40, "test");
  expands in the pre-processor to
   char str_SAFEBUFFER[40+1];
   SafeString str(sizeof(str_SAFEBUFFER),str_SAFEBUFFER,"test","str");

  createSafeStringFromCharArray(name, char[]);  or cSFA(name, char[]);
   wraps an existing char[] in a SafeString of the given name
  e.g.
  char charBuffer[15];
  createSafeStringFromCharArray(str,charBuffer); or cSFA(str,charBuffer);
  expands in the pre-processor to
   SafeString str(sizeof(charBuffer),charBuffer, charBuffer, "str", true);

  createSafeStringFromCharPtrWithSize(name, char*, unsigned int);  or cSFPS(name, char*, unsigned int);
   wraps an existing char[] pointed to by char* in a SafeString of the given name and sets the capacity to the given size
  e.g.
  char charBuffer[15]; // can hold 14 char + terminating '\0'
  char *bufPtr = charBuffer;
  createSafeStringFromCharPtrWithSize(str,bufPtr, 14); or cSFPS(str,bufPtr, 14);
  expands in the pre-processor to
   SafeString str(14+1,charBuffer, charBuffer, "str", true);
  The capacity of the SafeString is set to 14.

  createSafeStringFromCharPtr(name, char*);  or cSFP(name, char*);
   wraps an existing char[] pointed to by char* in a SafeString of the given name
  createSafeStringFromCharPtr(name, char* s) is the same as   createSafeStringFromCharPtrWithSzie(name, char* s, strlen(s));
  That is the current strlen() is used to set the SafeString capacity.
  e.g.
  char charBuffer[15] = "test";
  char *bufPtr = charBuffer;
  createSafeStringFromCharPtr(str,bufPtr); or cSFP(str,bufPtr);
  expands in the pre-processor to
   SafeString str(0,charBuffer, charBuffer, "str", true);
  and the capacity of the SafeString is set to strlen(charBuffer) and cannot be increased.


   If str is a SafeString then
   str = .. works for signed/unsigned ints, char*, char, F(".."), SafeString float, double etc
   str.concat(..) and string.prefix(..) also works for those
   str.stoken(..) can be used to split a string in to tokens

   SafeStrings created via createSafeString(  ) are never invalid, even if called with invalid arguments.
   SafeStrings created via createSafeStringFromBuffer(  ) are valid as long at the buffer is valid.
     Usually the only way the buffer can become invalid is if it exists in a struct that is allocated (via calloc/malloc)
     and then freed while the SafeString wrapping it is still in use.
*********************************/

/*
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
// bool versus unsigned char
// on UNO, ESP8266 and ESP32  sizeof(bool) == 1  i.e. same size as unsigned char
// but bool is safer as  bool + 1 does not compile
// however Arduino uses unsigned char as return value so...
#ifndef SafeString_class_h
#define SafeString_class_h

#if defined(ARDUINO_ARCH_SAM)
#define bool int
#endif

#ifdef __cplusplus

#include <stdlib.h>
#include <string.h>
#include <ctype.h>



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
// This include handles the rename of Stream for MBED compiles
#if defined(ARDUINO_ARDUINO_NANO33BLE) || defined(ARDUINO_ARCH_SAMD)
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

class __FlashStringHelper;
#ifndef F
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper *>(PSTR(string_literal)))
// ESP32 breaks this define into two defines and gives warnings about redfinition of the F( ) macro which can be ignored
#endif


// to remove all the error messages, comment out
#define SSTRING_DEBUG
// this saves program bytes and the ram used by the SafeString object names
//
// Usually just leave as is and use SafeString::setOutput(..) to control the error messages and debug output
// there will be no error messages or debug output if SafeString::setOutput(..) has not been called from your sketch
//
// SafeString.debug() is always available regardless of the SSTRING_DEBUG define setting
//   but SafeString::setOutput() still needs to be called to set where the output should go.

/* -----------------  creating SafeStrings ---------------------------------
  See the example sketches SafeString_ConstructorAndDebugging.ino and SafeStringFromCharArray.ino
   and SafeStringFromCharPtr.ino and SafeStringFromCharPtrWithSize.ion

  createSafeString(name, size) and createSafeString(name, size, "initialText")
  are utility macros to create an SafeString of a given name and size and optionally, an initial value

  createSafeString(str, 40);  or  cSF(str, 40);
  expands in the pre-processor to
   char str_SAFEBUFFER[40+1];
   SafeString str(sizeof(str_SAFEBUFFER),str_SAFEBUFFER,"","str");

  createSafeString(str, 40, "test");  or cSF(str, 40, "test");
  expands in the pre-processor to
   char str_SAFEBUFFER[40+1];
   SafeString str(sizeof(str_SAFEBUFFER),str_SAFEBUFFER,"test","str");

  createSafeStringFromCharArray(name, char[]);  or cSFA(name, char[]);
   wraps an existing char[] in a SafeString of the given name
  e.g.
  char charBuffer[15];
  createSafeStringFromCharArray(str,charBuffer); or cSFA(str,charBuffer);
  expands in the pre-processor to
   SafeString str(sizeof(charBuffer),charBuffer, charBuffer, "str", true);

  createSafeStringFromCharPtr(name, char*);  or cSFP(name, char*);
   wraps an existing char[] pointed to by char* in a SafeString of the given name
  e.g.
  char charBuffer[15];
  char *bufPtr = charBuffer;
  createSafeStringFromCharPtr(str,bufPtr); or cSFP(str,bufPtr);
  expands in the pre-processor to
   SafeString str((unsigned int)-1,charBuffer, charBuffer, "str", true);
  and the capacity of the SafeString is set to strlen(charBuffer) and cannot be increased.

  createSafeStringFromCharPtrWithSize(name, char*, unsigned int);  or cSFPS(name, char*, unsigned int);
   wraps an existing char[] pointed to by char* in a SafeString of the given name and sets the capacity to the given size
  e.g.
  char charBuffer[15];
  char *bufPtr = charBuffer;
  createSafeStringFromCharPtrWithSize(str,bufPtr, 15); or cSFPS(str,bufPtr, 15);
  expands in the pre-processor to
   SafeString str(15,charBuffer, charBuffer, "str", true);
  The capacity of the SafeString is set to 14.

****************************************************************************************/
/***************************************************
   If str is a SafeString then
   str = .. works for signed/unsigned ints, char*, char, F(".."), SafeString float, double etc
   str.concat(..) and string.prefix(..) also works for those
   str.stoken(..) can be used to split a string in to tokens

   SafeStrings created via createSafeString(..) or cSF(..) are never invalid, even if called with invalid arguments.
   SafeStrings created via createSafeStringFromCharArray(..) or cSFA(..) are valid as long at the underlying char[] is valid
     Usually the only way the char[] can become invalid is if it exists in a struct that is allocated (via calloc/malloc)
     and then freed while the SafeString wrapping it is still in use.
   SafeStrings created via createSafeStringFromCharPtr(..) or cSFP(..) are valid if the char[] pointed to is validly terminated
   SafeStrings created via createSafeStringFromCharWithSize(..) or cSFPS(..) are valid if the char[] and size specified is valid.
   For both createSafeStringFromCharPtr() and createSafeStringFromCharWithSize()
   the SafeStrings created remain valid as long as the underlying char[] is valid.
     Usually the only way the char[] can become invalid is if it was allocated (via calloc/malloc)
     and then freed while the SafeString wrapping it is still in use.
****************************************************/


#ifdef SSTRING_DEBUG
#define createSafeString(name, size,...) char name ## _SAFEBUFFER[(size)+1]; SafeString name(sizeof(name ## _SAFEBUFFER),name ## _SAFEBUFFER,  ""  __VA_ARGS__ , #name);
#define createSafeStringFromCharArray(name, charArray)  SafeString name(sizeof(charArray),charArray, charArray, #name, true, false);
#define createSafeStringFromCharPtr(name, charPtr) SafeString name((unsigned int)-1,charPtr, charPtr, #name, true);
#define createSafeStringFromCharPtrWithSize(name, charPtr, arraySize) SafeString name((arraySize),charPtr, charPtr, #name, true);
#else
#define createSafeString(name, size,...) char name ## _SAFEBUFFER[(size)+1]; SafeString name(sizeof(name ## _SAFEBUFFER),name ## _SAFEBUFFER, ""  __VA_ARGS__);
#define createSafeStringFromCharArray(name,charArray)  SafeString name(sizeof(charArray),charArray, charArray, NULL, true, false);
#define createSafeStringFromCharPtr(name, charPtr) SafeString name((unsigned int)-1,charPtr, charPtr, NULL, true);
#define createSafeStringFromCharPtrWithSize(name, charPtr, arraySize) SafeString name((arraySize),charPtr, charPtr, NULL, true);
#endif

// define typing shortcuts
#define cSF createSafeString
#define cSFA createSafeStringFromCharArray
#define cSFP createSafeStringFromCharPtr
#define cSFPS createSafeStringFromCharPtrWithSize

class SafeString : public Printable, public Print {

  public:

    explicit SafeString(unsigned int maxLen, char *buf, const char* cstr, const char* _name = NULL, bool _fromBuffer = false, bool _fromPtr = true);
    // _fromBuffer true does extra checking before each method execution for SafeStrings created from existing char[] buffers
    // _fromPtr is not checked unless _fromBuffer is true
    // _fromPtr true allows for any array size, if false prevents passing char* by checking sizeof(charArray) != sizeof(char*)

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

    // returns true if error detected, errors are detected even is setOutput has not been called
    // each call to hasError() clears the errorFlag
    unsigned char hasError();

    // returns true if error detected in any SafeString object, errors are detected even is setOutput has not been called
    // each call to errorDetected() clears the classErrorFlag
    static unsigned char errorDetected();

    // these methods print out info on this SafeString object, iff setOutput has been called
    // setVerbose( ) does NOT effect these methods which have their own verbose argument
    // Each of these debug( ) methods defaults to outputing the string contents.  Set the optional verbose argument to false to suppress outputing string contents
    // NOTE!! all these debug methods return a pointer to an empty string.
    // This is so that if you add .debug() to Serial.println(str);  i.e. Serial.println(str.debug()) will work as expected
    const char* debug(bool verbose = true);
    const char* debug(const char* title, bool verbose = true);
    const char* debug(const __FlashStringHelper *title, bool verbose = true);
    const char* debug(SafeString &stitle, bool verbose = true);

    virtual size_t write(uint8_t b);
    virtual size_t write(const uint8_t *buffer, size_t length);

    size_t printTo(Print& p) const;

    // reserve returns 0 if _capacity < size
    unsigned char reserve(unsigned int size);

    unsigned int length(void);
    unsigned int capacity(void);
    unsigned char isFull(void);
    unsigned char isEmpty(void);
    int availableForWrite(void);

    SafeString & clear(void);

  public:
    // support for print
    size_t print(unsigned char, int = DEC);
    size_t print(int, int = DEC);
    size_t print(unsigned int, int = DEC);
    size_t print(long, int = DEC);
    size_t print(unsigned long, int = DEC);
    size_t print(double, int = 2);
    size_t print(const __FlashStringHelper *);
    size_t print(const char*);
    size_t print(char);
    size_t print(SafeString &str);

    size_t println(unsigned char, int = DEC);
    size_t println(int, int = DEC);
    size_t println(unsigned int, int = DEC);
    size_t println(long, int = DEC);
    size_t println(unsigned long, int = DEC);
    size_t println(double, int = 2);
    size_t println(const __FlashStringHelper *);
    size_t println(const char*);
    size_t println(char);
    size_t println(SafeString &str);
    size_t println(void);


    /* Assignment operators **********************************
      Set the SafeString to a char version of the assigned value.
      For = (const char *) the contents are copied to the SafeString buffer
      if the value is null or invalid,
      or too large to be fit in the string's internal buffer
      the string will be left empty
     **/
    SafeString & operator = (char c);
    SafeString & operator = (unsigned char c);
    SafeString & operator = (int num);
    SafeString & operator = (unsigned int num);
    SafeString & operator = (long num);
    SafeString & operator = (unsigned long num);
    SafeString & operator = (float num);
    SafeString & operator = (double num);

    SafeString & operator = (SafeString &rhs);
    SafeString & operator = (const char *cstr);
    SafeString & operator = (const __FlashStringHelper *str); // handle F(" .. ") values

    /* Prefix methods **************************
      add to the front of the current SafeString
      returns the current SafeString so you can cascade calls
      eg. sfStr.prefix(2).prefix("first");
      if there's not enough memory for the to add prefix, the SafeString
      will be left unchanged
    **/
    SafeString & prefix(SafeString &s);
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

    /* Concat i.e. append methods ******************
      adds to the end of the current SafeString
      returns the current SafeString so you can cascade calls
      eg. sfStr.concat(1).concat("second");
      if there's not enough memory for the concatenated value, the SafeString
      will be left unchanged
       if the argument is null or invalid, the
      concatenation is considered unsucessful.
    **/
    SafeString & concat(SafeString &str);
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
    // ------------------------------------------------------
    // no corresponding methods these three (3) in  prefix, +=, -+
    SafeString & concat(const char *cstr, size_t length); // concat at most length chars from cstr
    SafeString & concat(const __FlashStringHelper * str, size_t length); // concat at most length chars
    SafeString & newline(); // append newline \r\n same as concat("\r\n"); same a println()
    // e.g. sfStr.concat("test").nl();

    /* prefix() operator  -=  ******************
      Operator version of prefix( )
      prefix  -=
      To cascade operators use ( )
      e.g. (sfStr -= 'a') -= 5;
     **/
    SafeString & operator -= (SafeString &rhs) {
      return prefix(rhs);
    }
    SafeString & operator -= (const char *cstr) {
      return prefix(cstr);
    }
    SafeString & operator -= (char c) {
      return prefix(c);
    }
    SafeString & operator -= (unsigned char num) {
      return prefix(num);
    }
    SafeString & operator -= (int num) {
      return prefix(num);
    }
    SafeString & operator -= (unsigned int num) {
      return prefix(num);
    }
    SafeString & operator -= (long num)  {
      return prefix(num);
    }
    SafeString & operator -= (unsigned long num) {
      return prefix(num);
    }
    SafeString & operator -= (float num) {
      return prefix(num);
    }
    SafeString & operator -= (double num) {
      return prefix(num);
    }
    SafeString & operator -= (const __FlashStringHelper *str) {
      return prefix(str);
    }

    /* concat() operator  +=  ******************
      Operator versions of concat( )
      suffix/append +=
      To cascade operators use ( )
      e.g. (sfStr += 'a') += 5;
     **/
    SafeString & operator += (SafeString &rhs)  {
      return concat(rhs);
    }
    SafeString & operator += (const char *cstr) {
      return concat(cstr);
    }
    SafeString & operator += (char c) {
      return concat(c);
    }
    SafeString & operator += (unsigned char num) {
      return concat(num);
    }
    SafeString & operator += (int num) {
      return concat(num);
    }
    SafeString & operator += (unsigned int num) {
      return concat(num);
    }
    SafeString & operator += (long num) {
      return concat(num);
    }
    SafeString & operator += (unsigned long num) {
      return concat(num);
    }
    SafeString & operator += (float num) {
      return concat(num);
    }
    SafeString & operator += (double num)  {
      return concat(num);
    }
    SafeString & operator += (const __FlashStringHelper *str) {
      return concat(str);
    }

    /* Comparision methods and operators  ******************
      comparisons only work with SafeStrings and "strings"
      These methods used to be  ... const {
      but now with createSafeStringFromBuffer( ) the SafeString may be modified by cleanUp()
     **/
    int compareTo(SafeString &s) ;
    int compareTo(const char *cstr) ;
    unsigned char equals(SafeString &s) ;
    unsigned char equals(const char *cstr) ;
    unsigned char equals(const char c) ;
    unsigned char operator == (SafeString &rhs) {
      return equals(rhs);
    }
    unsigned char operator == (const char *cstr) {
      return equals(cstr);
    }
    unsigned char operator == (const char c) {
      return equals(c);
    }
    unsigned char operator != (SafeString &rhs) {
      return !equals(rhs);
    }
    unsigned char operator != (const char *cstr) {
      return !equals(cstr);
    }
    unsigned char operator != (const char c) {
      return !equals(c);
    }
    unsigned char operator <  (SafeString &rhs) {
      return compareTo(rhs) < 0;
    }
    unsigned char operator >  (SafeString &rhs) {
      return compareTo(rhs) > 0;
    }
    unsigned char operator <= (SafeString &rhs) {
      return compareTo(rhs) <= 0;
    }
    unsigned char operator >= (SafeString &rhs) {
      return compareTo(rhs) >= 0;
    }
    unsigned char operator <  (const char* rhs) {
      return compareTo(rhs) < 0;
    }
    unsigned char operator >  (const char* rhs) {
      return compareTo(rhs) > 0;
    }
    unsigned char operator <= (const char* rhs) {
      return compareTo(rhs) <= 0;
    }
    unsigned char operator >= (const char* rhs) {
      return compareTo(rhs) >= 0;
    }
    unsigned char equalsIgnoreCase(SafeString &s) ;
    unsigned char equalsIgnoreCase(const char *str2) ;
    unsigned char equalsConstantTime(SafeString &s) ;

    /* startsWith methods  *******************
      The fromIndex is offset into this SafeString where check is to start
      0 to length() and (unsigned int)(-1) are valid for fromIndex, if fromIndex == length() or -1 false is returned
      if the argument is null or fromIndex > length(), an error is flagged and false returned
    **/
    unsigned char startsWith( const char *str2) ;
    unsigned char startsWith( const char *str2, unsigned int fromIndex ) ;
    unsigned char startsWithIgnoreCase( const char *str2 ) ;
    unsigned char startsWithIgnoreCase( const char *str2, unsigned int fromIndex ) ;
    unsigned char startsWith( SafeString &s2) ;
    unsigned char startsWith(SafeString &s2, unsigned int fromIndex) ;
    unsigned char startsWithIgnoreCase( SafeString &s2 ) ;
    unsigned char startsWithIgnoreCase( SafeString &s2, unsigned int fromIndex ) ;
    
    /* endsWith methods  *******************/
    unsigned char endsWith(SafeString &suffix) ;
    unsigned char endsWith(const char *suffix) ;
    unsigned char endsWithCharFrom(SafeString &suffix) ;
    unsigned char endsWithCharFrom(const char *suffix) ;

    /* character acccess methods  *******************
      NOTE: There is no access to modify the underlying char buffer directly
      For these methods 0 to length()-1 is valid for index
      index greater than length() -1 will return 0 and set the error flag and will print errors if debug enabled
    **/
    char charAt(unsigned int index) ; // if index >= length() returns 0 and prints a error msg
    char operator [] (unsigned int index) ; // if index >= length() returns 0 and prints a error msg

    // setting a char in the SafeString
    // str[..] = c;  is not supported because it allows direct access to modify the underlying char buffer
    void setCharAt(unsigned int index, char c); //if index >= length() the error flag is set
    // calls to setCharAt(length(), ..) and  setCharAt(.. , '\0') are ignored and error flag is set

    // returning the underlying buffer
    // returned as a const and should not be changesdor recast to a non-const
    const char* c_str();


    /* search methods  *******************
       Arrays are indexed by a unsigned int variable
       See the SafeStringIndexOf.ino example sketch
      All indexOf methods return -1 if not found
    **********************************************/
    // The fromIndex is offset into this SafeString where to start searching (inclusive)
    // 0 to length() and -1 is valid for fromIndex
    // if fromIndex > length(), than the error flag is set and -1 returned and prints an error if debug enabled
    // if fromIndex == (unsigned int)(-1) -1 is returned without error.
    int indexOf( char ch ) ;
    int indexOf( char ch, unsigned int fromIndex ) ;
    int indexOf( SafeString & str ) ;
    int indexOf( const char* str ) ;
    int indexOf(const char* str , unsigned int fromIndex) ;
    int indexOf( SafeString & str, unsigned int fromIndex ) ;
    int lastIndexOf( char ch ) ;
    int lastIndexOf( char ch, unsigned int fromIndex ) ;
    int lastIndexOf( SafeString & str ) ;
    int lastIndexOf( const char *cstr ) ;
    int lastIndexOf( SafeString & str, unsigned int fromIndex ) ;
    int lastIndexOf(const char* cstr, unsigned int fromIndex);
    // first index of the chars listed in chars string
    // loop through chars and look for index of each and return the min index or -1 if none found
    int indexOfCharFrom(SafeString & str);
    int indexOfCharFrom(const char* chars);
    // start searching from fromIndex
    int indexOfCharFrom(SafeString & str, unsigned int fromIndex);
    int indexOfCharFrom(const char* chars, unsigned int fromIndex);


    /* *** substring methods ************/
    // substring is from beginIdx to end of string
    // The result substring is ALWAYS first cleared by this method so it will be empty on errors
    // if beginIdx = length(), an empty result will be returned without error
    // if beginIdx > length(), an empty result will be returned with error flag set on both this SafeString and the result SafeString
    // beginIdx == (unsigned int)(-1) returns an empty result without an error
    // You can take substring of yourself  e.g. str.substring(str,3);
    // if result does not have the capacity to hold the substring, hasError() is set on both this SafeString and the result SafeString
    SafeString & substring(SafeString & result, unsigned int beginIdx);

    // The result substring is ALWAYS first cleared by this method so it will be empty on errors
    // if beginIdx = length(), an empty result will be returned without error
    // if beginIdx > length(), an empty result will be returned with error flag set on both this SafeString and the result SafeString
    // if beginIdx > endIdx, beginIdx and endIdx will be swapped so that beginIdx <= endIdx and the error flag is set on both this SafeString and the result SafeString
    // if endIdx > length(), endIdx is set to length(); and the error flag is set on both this SafeString and the result SafeString
    // endIdx == (unsigned int)(-1) is treated as endIdx == length() returns a result without an error
    // substring is from beginIdx to endIdx-1, endIdx is exclusive
    // You can take substring of yourself  e.g. str.substring(str,3,6);
    // if result does not have the capacity to hold the substring, and empty result is returned and hasError() is set on both this SafeString and the result SafeString
    SafeString & substring(SafeString & result, unsigned int beginIdx, unsigned int endIdx);

    /* *** SafeString modification methods ************/

    /* *** replace ************/
    void replace(char findChar, char replaceChar);
    void replace(const char findChar, const char *replaceStr);
    void replace(const char findChar, SafeString& sfReplace);
    void replace(const char* findStr, const char *replaceStr);
    void replace(SafeString & sfFind, SafeString & sfReplace);

    /* *** remove ************/
    // remove from index to end of SafeString
    // 0 to length() and (unsigned int)(-1) are valid for index,
    // -1 => length() for processing and just returns without error
    void removeFrom(unsigned int startIndex);

    // remove from 0 to startIdx (excluding startIdx)
    // 0 to length() and (unsigned int)(-1) are valid for index,
    // -1 => length() for processing
    void removeBefore(unsigned int startIndex);

    // remove from index to end of SafeString
    // 0 to length() and (unsigned int)(-1) are valid for index,
    // -1 => length() for processing and just returns without error
    void remove(unsigned int index);

    // remove count chars starting from index
    // 0 to length() and unsigned int(-1) are valid for index
    // -1 just returns without error
    // 0 to (length()- index) is valid for count, larger values set the error flag and remove from idx to end of string
    void remove(unsigned int index, unsigned int count);

    // remove the last 'count' chars
    // 0 to length() is valid for count,
    // count >= length() clears the SafeString
    // count > length() set the error flag
    void removeLast(unsigned int count);

    // keep last 'count' number of chars remove the rest
    // 0 to length() is valid for count, passing in count == 0 clears the SafeString
    // count > length() sets error flag and returns SafeString unchanged
    void keepLast(unsigned int count);


    /* *** change case ************/
    void toLowerCase(void);
    void toUpperCase(void);

    /* *** remove white space from front and back of SafeString ************/
    // the method isspace( ) is used to.  For the 'C' local the following are trimmed
    //    ' '     (0x20)  space (SPC)
    //    '\t'  (0x09)  horizontal tab (TAB)
    //    '\n'  (0x0a)  newline (LF)
    //    '\v'  (0x0b)  vertical tab (VT)
    //    '\f'  (0x0c)  feed (FF)
    //    '\r'  (0x0d)  carriage return (CR)
    void trim(void); // trims front and back

    // processBackspaces recursively remove backspaces, '\b' and the preceeding char
    // use for processing inputs from terminal (Telent) connections
    void processBackspaces(void);

    /* *** numgber parsing/conversion ************/
    // convert numbers
    // If the SafeString is a valid number update the argument with the result
    // else leave the argument unchanged
    // SafeString conversions are stricter than the Arduino String version
    // trailing chars can only be white space
    unsigned char toInt(int & i) ;
    unsigned char toLong(long & l) ;
    unsigned char binToLong(long & l) ;
    unsigned char octToLong(long & l) ;
    unsigned char hexToLong(long & l) ;
    unsigned char toFloat(float  & f) ;
    unsigned char toDouble(double & d) ;

    /* Tokenizeing methods,  stoken(), nextToken() ************************/
    /* Differences between stoken() and nextToken
       stoken() leaves the SafeString unchanged, nextToken() removes the token (and leading delimiters) from the SafeString giving space to add more input
       In stoken() the end of the SafeString is always treated as a delimiter, i.e. the last token is returned even if it is not followed by one of the delimiters
       In nextToken() the end of the SafeString is NOT a delimiter, i.e. if the last token is not terminated it is left in the SafeString
       this allows partial tokens to be read from a Stream and kept until the full token and delimiter is read
    */
    /*
         stoken  -- The SafeString itself is not changed
         stoken breaks into the SafeString into tokens using chars in delimiters string and the end of the SafeString as delimiters.
         Any leading delimiters are first stepped over and then the delimited token is return in the token argument (less the delimiter).
         The token argument is always cleared at the start of the stoken().
         if there are any argument errors or the token does not have the capacity to hold the substring, hasError() is set on both this SafeString and the token SafeString

         params
         token - the SafeString to return the token in, it is cleared if no delimited token found or if there are errors
                 The found delimited token (less the delimiter) is returned in the token SafeString argument if there is capacity.
                 The token's capacity should be >= this SafeString's capacity incase the entire SafeString needs to be returned.
                 If the token's capacity is < the next token, then token is returned empty and an error messages printed if debug is enabled.
                 In this case the return (nextIndex) is still updated.
         fromIndex -- where to start the search from  0 to length() and -1 is valid for fromIndex,  -1 => length() for processing
         delimiters - the characters that any one of which can delimit a token. The end of the SafeString is always a delimiter.
         returnEmptyFields -- default false, if true only skip one leading delimiter after each call. If the fromIndex is 0 and there is a delimiter at the beginning of the SafeString, an empty token will be returned
         useAsDelimiters - default true, if false then token consists only of chars in the delimiters and any other char terminates the token

         return -- nextIndex, the next index in this SafeString after the end of the token just found, -1 if this is the last token
                  Use this as the fromIndex for the next call
                  NOTE: if there are no delimiters then -1 is returned and the whole SafeString returned in token if the SafeString token argument is large enough
                  If the token's capacity is < the next token, the token returned is empty and an error messages printed if debug is enabled.
                  In this case the returned nextIndex is still updated to end of the token just found so that that the program will not be stuck in an infinite loop testing for nextIndex >=0
                  while being consistent with the SafeString's all or nothing insertion rule

         Input argument errors return -1 and an empty token and hasError() is set on both this SafeString and the token SafeString.
    **/

    int stoken(SafeString & token, unsigned int fromIndex, const char delimiter, bool returnEmptyFields = false, bool useAsDelimiters = true);
    int stoken(SafeString & token, unsigned int fromIndex, const char* delimiters, bool returnEmptyFields = false, bool useAsDelimiters = true);
    int stoken(SafeString & token, unsigned int fromIndex, SafeString & delimiters, bool returnEmptyFields = false, bool useAsDelimiters = true);

    /* nextToken -- The token is removed from the SafeString ********************
      nextToken -- Any leading delimiters are first removed, then the delimited token found is removed from the SafeString.
                   The following delimiters remain in the SafeString so you can test which delimiter terminated the token.
      The token argument is always cleared at the start of the nextToken().
      IMPORTANT !! Only delimited tokens are returned. Partial un-delimited tokens are left in the SafeString and not returned
      This allows the SafeString to hold partial tokens when reading from an input stream one char at a time.

      params
      token - the SafeString to return the token in, it will be empty if no delimited token found or if there are errors
              The token's capacity should be >= this SafeString's capacity incase the entire SafeString needs to be returned.
              If the token's capacity is < the next token, then nextToken() returns true, but the returned token argument is empty and an error messages printed if debug is enabled.
              In this case to next token is still removed from the SafeString so that the program will not be stuck in an infinite loop calling nextToken()
      delimiters - the delimiting characters, any one of which can delimit a token

      return -- true if nextToken() finds a token in this SafeString that is terminated by one of the delimiters after removing any leading delimiters, else false
                If token argument did not have the capacity to hold the next token, hasError() is set on both this SafeString and the token SafeString.
                In this case to next token is still removed from the SafeString so that the program will not be stuck in an infinite loop calling nextToken()
                while being consistent with the SafeString's all or nothing insertion rule
         Input argument errors return false and an empty token and hasError() is set on both this SafeString and the token SafeString.
    **/
    unsigned char nextToken(SafeString & token, char delimiter);
    unsigned char nextToken(SafeString & token, SafeString & delimiters);
    unsigned char nextToken(SafeString & token, const char* delimiters);


    /* *** ReadFrom from SafeString, writeTo SafeString ************************/
    /*
       readFrom(SafeString & sfInput, unsigned int startIdx = 0)  reads from the SafeString starting at startIdx into the SafeString calling this method
       params
         sfInput - the SafeString to read from
         startIdx - where to start reading from, defaults to 0,
                    if startIdx >= sfInput.length(), nothing read and sfInput.length() returned

       returns new startIdx
       read stops when the end of the sfInput is reached or the calling SafeString is full
       Note: if the SafeString is already full, then nothing will be read and startIdx will be returned
    **/
    unsigned int readFrom(SafeString & input, unsigned int startIdx = 0);

    /*
       writeTo(SafeString & output, unsigned int startIdx = 0)  writes from SafeString, starting at startIdx to output
       params
         output - the SafeString to write to
         startIdx - where to start writing from calling SafeString, defaults to 0,
                    if startIdx >= length(), nothing written and length() returned

       returns new startIdx for next write
       write stops when the end if the calling SafeString is reached or the output is full
       Note: if the sfOutput is already full, then nothing will be written and startIdx will be returned
    **/
    unsigned int writeTo(SafeString & output, unsigned int startIdx = 0);

    /* *** NON-blocking reads from Stream ************************/
    /*
      read(Stream& input)  reads from the Stream (if chars available) into the SafeString
      The is NON-BLOCKING and returns immediately if nothing available to be read

      returns true if something added to string else false
      Note: if the SafeString is already full, then nothing will be read and false will be returned
     **/
    unsigned char read(Stream & input);

    /*
      NON-blocking readUntil if chars available
      returns true if delimiter found or string filled, found else false
      if a delimiter is found it is included in the return

      params
        input - the Stream object to read from
        delimiters - string of valid delimieters
      return true if SaftString is full or a delimiter is read, else false
      Any delimiter read is returned in input arg.  Only at most one delimiter is added per call
       Multiple sucessive delimiters require multiple calls to read them
    **/
    unsigned char readUntil(Stream & input, const char delimiter);
    unsigned char readUntil(Stream & input, const char* delimiters);
    unsigned char readUntil(Stream & input, SafeString & delimiters);

    /*
      NON-blocking readUntilToken
      returns true if a delimited token is found, else false
      ONLY delimited tokens of length less than this SafeString's capacity will return true with a non-empty token.
      Streams of chars that overflow this SafeString's capacity are ignored and return an empty token on the next delimiter
      That is this SafeString's capacity should be at least 1 more then the largest expected token.
      If this SafeString OR the SaftString & token return argument is too small to hold the result, the token is returned empty and an error message output if debugging is enabled.
      The delimiter is NOT included in the SaftString & token return.  It will the first char of the this SafeString when readUntilToken returns true
      It is recommended that the capacity of the SafeString & token argument be >= this SaftString's capacity
      Each call to this method removes any leading delimiters so if you need to check the delimiter do it BEFORE the next call to readUntilToken()

      params
        input - the Stream object to read from
        token - the SafeString to return the token found if any, always cleared at the start of this method
        delimiters - string of valid delimieters
        skipToDelimiter - a bool variable to hold the skipToDelimiter state between calls
        echoInput - defaults to false, pass non-zero (true) to echo the chars read
        timeout_mS - defaults to never timeout, pass a non-zero mS to autoterminate the last token if no new chars received for that time.

      returns true if a delimited series of chars found that fit in this SafeString else false
      If this SafeString OR the SaftString & token argument is too small to hold the result, the returned token is returned empty
      The delimiter is NOT included in the SaftString & token return. It will the first char of the this SafeString when readUntilToken returns true
    **/
    unsigned char readUntilToken(Stream & input, SafeString & token, const char delimiter, bool & skipToDelimiter, uint8_t echoInput = false, unsigned long timeout_mS = 0);
    unsigned char readUntilToken(Stream & input, SafeString & token, const char* delimiters, bool & skipToDelimiter, uint8_t echoInput = false, unsigned long timeout_mS = 0);
    unsigned char readUntilToken(Stream & input, SafeString & token, SafeString & delimiters, bool & skipToDelimiter, uint8_t echoInput = false, unsigned long timeout_mS = 0);

    size_t getLastReadCount(); // number of chars read on previous call to read, readUntil or readUntilToken (includes '\0' read if any)  each call read, readUntil, readUntilToken first clears this count
    /* *** END OF PUBLIC METHODS ************/

  protected:
    static Print* debugPtr;
    static bool fullDebug;
    char *buffer;          // the actual char array
    size_t _capacity; // the array length minus one (for the '\0')
    size_t len;       // the SafeString length (not counting the '\0')

    class noDebugPrint : public Print {
        inline size_t write(uint8_t b) {
          (void)(b);
          return 0;
        }
        inline size_t write(const uint8_t *buffer, size_t length) {
          (void)(buffer);
          (void)(length);
          return 0;
        };
      public:
        void flush() { }
    };

    static SafeString::noDebugPrint emptyPrint;

    static Print* currentOutput;// = &emptyPrint;

    class DebugPrint : public Print {
        size_t write(uint8_t b) {
          return currentOutput->write(b);
        }
        size_t write(const uint8_t *buffer, size_t length) {
          return currentOutput->write(buffer, length);
        };
      public:
        void flush() {
#if defined(ESP_PLATFORM) || defined(ARDUINO_SAM_DUE) || defined(ARDUINO_ARCH_STM32F1) || defined(ARDUINO_ARCH_STM32F4)
          // ESP32 has no flush in Print!! but ESP8266 has
#else
          currentOutput->flush();
#endif
        }
    };

  public:
    static SafeString::DebugPrint Output; // a Print object controlled by setOutput() / turnOutputOff()

  protected:
    SafeString & concatln(const __FlashStringHelper * pstr);
    SafeString & concatln(char c);
    SafeString & concatln(const char *cstr, size_t length);
    void outputName() const ;
    SafeString & concatInternal(const char *cstr, size_t length, bool assignOp = false); // concat at most length chars from cstr
    SafeString & concatInternal(const __FlashStringHelper * str, size_t length, bool assignOp = false); // concat at most length chars

    SafeString & concatInternal(const char *cstr, bool assignOp = false);
    SafeString & concatInternal(char c, bool assignOp = false);
    SafeString & concatInternal(const __FlashStringHelper * str, bool assignOp = false);
    size_t printInternal(long, int = DEC, bool assignOp = false);
    size_t printInternal(unsigned long, int = DEC, bool assignOp = false);
    size_t printInternal(double, int = 2, bool assignOp = false);
    void setError();
    void printlnErr()const ;
    void debugInternalMsg(bool _fullDebug) const ;
    size_t limitedStrLen(const char* p, size_t limit);

  private:
    bool readUntilTokenInternal(Stream & input, SafeString & token, const char* delimitersIn, char delimiterIn, bool & skipToDelimiter, uint8_t echoInput, unsigned long timeout_mS);
    bool readUntilInternal(Stream & input, const char* delimitersIn, char delimiterIn);
    bool nextTokenInternal(SafeString & token, const char* delimitersIn, char delimiterIn);
    int stokenInternal(SafeString &token, unsigned int fromIndex, const char* delimitersIn, char delimiterIn, bool returnEmptyFields, bool useAsDelimiters);
    bool fromBuffer; // true if createSafeStringFromBuffer created this object
    bool errorFlag; // set to true if error detected, cleared on each call to hasError()
    static bool classErrorFlag; // set to true if any error detected in any SafeString, cleared on each call to SafeString::errorDetected()
    void cleanUp(); // reterminates buffer at capacity and resets len to current strlen
    const char *name;
    unsigned long timeoutStart_mS;
    bool timeoutRunning;
    size_t noCharsRead; // number of char read on last call to readUntilToken
    static char nullBufferSafeStringBuffer[1];
    static char emptyDebugRtnBuffer[1];
    void debugInternal(bool _fullDebug) const ;
    void debugInternalResultMsg(bool _fullDebug) const ;
    void concatErr()const ;
    void concatAssignError() const;
    void prefixErr()const ;
    void capError(const __FlashStringHelper * methodName, size_t neededCap, const char* cstr, const __FlashStringHelper *pstr = NULL, char c = '\0', size_t length = 0)const ;
    void assignError(size_t neededCap, const char* cstr, const __FlashStringHelper *pstr = NULL, char c = '\0', bool numberFlag = false) const;
    void errorMethod(const __FlashStringHelper * methodName) const ;
    void assignErrorMethod() const ;
    void warningMethod(const __FlashStringHelper * methodName) const ;
    void outputFromIndexIfFullDebug(unsigned int fromIndex) const ;
};

// to skip this for SparkFun RedboardTurbo
#ifndef ARDUINO_SAMD_ZERO
#if defined(ARDUINO_ARDUINO_NANO33BLE) || defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_MEGAAVR)
} // namespace arduino
#endif
#endif  // #ifndef ARDUINO_SAMD_ZERO

#endif  // __cplusplus
#endif  // SafeString_class_h