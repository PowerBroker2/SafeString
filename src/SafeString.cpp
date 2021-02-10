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

  createSafeStringFromCharPtrWithSize(name, char*, size_t);  or cSFPS(name, char*, size_t);
   wraps an existing char[] pointed to by char* in a SafeString of the given name and sets the capacity to the given size
  e.g.
  char charBuffer[15]; // can hold 14 char + terminating '\0'
  char *bufPtr = charBuffer;
  createSafeStringFromCharPtrWithSize(str,bufPtr, 15); or cSFPS(str,bufPtr, 15);
  expands in the pre-processor to
   SafeString str(15,charBuffer, charBuffer, "str", true);
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

/*
  SafeString.cpp V2.0.0 static memory SafeString library modified by
  Matthew Ford
  Mods Copyright(c)2020 Forward Computing and Control Pty. Ltd.
  All rights reservered subject to the License below

  extensively modified from

  WString.cpp - SafeString library for Wiring & Arduino
  ...mostly rewritten by Paul Stoffregen...
  Copyright (c) 2009-10 Hernando Barragan.  All rights reserved.
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

#include <Arduino.h>
#include "SafeString.h"
#include <limits.h>

// to skip this for SparkFun RedboardTurbo
#ifndef ARDUINO_SAMD_ZERO
#if defined(ARDUINO_ARDUINO_NANO33BLE) || defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_MEGAAVR)
using namespace arduino;
#endif
#endif // #ifndef ARDUINO_SAMD_ZERO


// to remove all the error debug outputs, comment out
// #define SSTRING_DEBUG
// in SafeString.h file
// this saves program space used by the error messages and the ram used by the SafeString object names
//
// usually just leave as is and use SafeString::setOutput(..) to control the debug output
// there will be no debug output is SafeString::setOutput(..) has not been called from your sketch
// SafeString.debug() is always available regardless of the SSTRING_DEBUG define setting
//   but SafeString::setOutput() still needs to be called to set where the output should go.

char SafeString::nullBufferSafeStringBuffer[1] = {'\0'}; // use if buf arg is NULL
char SafeString::emptyDebugRtnBuffer[1] = {'\0'}; // use for debug() returns
Print* SafeString::debugPtr = NULL; // nowhere to send the debug output yet
Print* SafeString::currentOutput = &SafeString::emptyPrint; // nowhere to send Output to yet
SafeString::noDebugPrint SafeString::emptyPrint;
SafeString::DebugPrint SafeString::Output;
bool SafeString::fullDebug = true; // output current contents of SafeString and input arg

/*********************************************/
/**  Constructor                             */
/*********************************************/

// This constructor overwrites any data already in buf with cstr unless  &buf == &cstr  in that case just clear both.
// _fromBuffer true does extra checking before each method execution for SafeStrings created from existing char[] buffers
// _fromPtr is not checked unless _fromBuffer is true
// _fromPtr true allows for any array size, if false it prevents passing char* by checking sizeof(charArray) != sizeof(char*)
SafeString::SafeString(size_t maxLen, char *buf, const char* cstr, const char* _name, bool _fromBuffer, bool _fromPtr) {
  name = _name; // save name
  fromBuffer = _fromBuffer;
  timeoutRunning = false;
  if (!fromBuffer) {
    _fromPtr = false;
  }
  bool keepBufferContents = false;
  if ((buf != NULL) && (cstr != NULL) && (buf == cstr)) {
    keepBufferContents = true;
  }
  if ((!_fromPtr) && (maxLen == sizeof(char*))) {
    buffer = nullBufferSafeStringBuffer;
    _capacity = 0;
    len = 0;
    buffer[0] = '\0';
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      debugPtr->print(F("Error: createSafeStringFromCharArray("));
      outputName(); debugPtr->print(F(", ...) sizeof(charArray) == sizeof(char*). \nCheck you are passing a char[] to createSafeStringFromCharArray OR use a slightly larger char[]\n"
                                      "  To wrap a char* use either createSafeStringFromCharPtr(..), cSFP(..) or createSafeStringFromCharPtrWithSize(..), cSFPS(.. )"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return;
  }

  if (buf != NULL) {
    buffer = buf;
    if ((maxLen == 0) || (maxLen == ((size_t) - 1))) { // -1 => find cap from strlen()
      if (_fromPtr) { // either ..fromCharPtr or ..fromCharPtrWithSize
        if (maxLen == 0) { // ..fromCharPtrWithSize
          buffer = nullBufferSafeStringBuffer;
          _capacity = 0;
          len = 0;
          buffer[0] = '\0';
#ifdef SSTRING_DEBUG
          if (debugPtr) {
            debugPtr->print(F("Error: createSafeStringFromCharArrayWithSize("));
            outputName(); debugPtr->print(F(", ..., 0) was passed zero passed for array size"));
            debugInternalMsg(fullDebug);
          }
#endif // SSTRING_DEBUG
          return;
        } else { // -1 ..fromCharPtr  use strlen()
          // calculate capacity from inital contents length. Have check buffer not NULL
          _capacity = strlen(buf);
        }
      } else { // from char[] most likely maxLen == 0 as (size_t)-1 is very very large
        buffer = nullBufferSafeStringBuffer;
        _capacity = 0;
        len = 0;
        buffer[0] = '\0';
#ifdef SSTRING_DEBUG
        if (debugPtr) {
          debugPtr->print(F("Error: createSafeStringFromCharArray("));
          outputName(); debugPtr->print(F(", ...) passed a zero length array"));
          debugInternalMsg(fullDebug);
        }
#endif // SSTRING_DEBUG
        return;
      }
    } else { //    if (maxLen > 0) and != (size_t)-1 {
      _capacity = maxLen - 1;
    }
    if (!keepBufferContents) {
      len = 0;
      buffer[0] = '\0'; // clears cstr is it is the same as buf !!
    }
  } else {
    buffer = nullBufferSafeStringBuffer;
    _capacity = 0;
    len = 0;
    buffer[0] = '\0';
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      debugPtr->print(F("Error: SafeString("));
      outputName(); debugPtr->print(F(", ...) was passed a NULL pointer for its char array"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return;
  }

  if (cstr == NULL) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      debugPtr->print(F("Error: SafeString("));
      outputName(); debugPtr->print(F(", ...) was passed a NULL pointer for initial value."));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return;
  }

  size_t cstrLen = strlen(cstr);
  if (cstrLen > _capacity) { // may be unterminated buffer
    if (!keepBufferContents) {
      // done above
      //  len = 0;
      //  buffer[0] = '\0'; // clears cstr is it is the same as buf !!
    } else {
      // does cleanUp for all new objects
      len = _capacity;
      buffer[len] = '\0'; // truncate buffer to given size
    }
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      if (!keepBufferContents) {
        debugPtr->print(F("Error: SafeString("));
        outputName(); debugPtr->print(F(", ...) needs capacity of ")); debugPtr->print(cstrLen);  debugPtr->print(F(" for initial value."));
        if (fullDebug) {
          debugPtr->println(); debugPtr->print(F("       "));
          debugPtr->print(F(" Initial value arg was '")); debugPtr->print(cstr); debugPtr->print('\'');
        }
        debugInternalMsg(fullDebug);
      } else {
        debugPtr->print(F("Warning: SafeString("));
        outputName(); debugPtr->print(F(", ...) passed unterminated buffer of length ")); debugPtr->print(cstrLen);
        if (_fromPtr) {
          debugPtr->print(F(" to createSafeStringFromCharPtrWithSize."));
        } else {
          debugPtr->print(F(" to createSafeStringFromCharArray."));
        }
        if (fullDebug) {
          debugPtr->println(); debugPtr->print(F("       "));
          debugPtr->print(F(" Truncated value saved is '")); debugPtr->print(cstr); debugPtr->print('\'');
        }
        debugInternalMsg(fullDebug);
      }
    }
#endif // SSTRING_DEBUG
    return;
  }
  if (!keepBufferContents) {
    memmove(buffer, cstr, cstrLen);
  }
  // does cleanUp for all new objects
  len = cstrLen;
  buffer[len] = '\0';
}

// this is private and should never be called.
SafeString::SafeString(const SafeString& other ) {
  //#ifdef SSTRING_DEBUG
  //    if (debugPtr) {
  //      debugPtr->println(F("Error: SafeString arguments must be declared as references, SafeString&  e.g. void test(SafeString& strIn)"));
  //    }
  //#endif // SSTRING_DEBUG
  (void)(other);
  buffer = nullBufferSafeStringBuffer;
  _capacity = 0;
  len = 0;
  buffer[0] = '\0';
  timeoutRunning = false;
}
/**  end of Constructor methods ***********/


/*********************************************/
/**  Information and utility methods         */
/** clear(), length(), capacity(),           */
/** isFull(), isEmpty(), availableForWrite() */
/**  and the private methods cleanUp()       */
/*********************************************/

// make the SafeString empty
SafeString & SafeString::clear(void) {
  len = 0;
  buffer[len] = '\0';
  return *this;
}

// return the equalent of strlen( ) for this SafeString
size_t SafeString::length(void) {
  cleanUp();
  return len;
}

// return the maximum number of chars that this SafeString can store
size_t SafeString::capacity(void) {
  cleanUp();
  return _capacity;
}

// cannot store any more chars in this SafeString
bool SafeString::isFull(void) {
  return (length() == capacity()); // each calls cleanUp()
}

// no chars stored in this SafeString
bool SafeString::isEmpty(void) {
  return length() == 0; // calls cleanUp()
}

// how many chars can be added to this SafeString before it is full
int SafeString::availableForWrite(void) {
  return (capacity() - length());  // each calls cleanUp()
}

/**  private utility method cleanUp() **/
// cleanUp() --  a private utility method only used  when SafeString created
//              from createSafeStringFromBuffer macro
// If this SafeString was created by createSafeStringFromBuffer then
// call cleanUp() before each method to ensure the SafeString len matches the
// actual strlen of the buffer.  Also ensure buffer is still terminated with '\0'
void SafeString::cleanUp() {
  if (!fromBuffer) {
    return; // skip scanning for length changes in the buffer in normal SafeString
  }
  buffer[_capacity] = '\0'; // make sure the buffer is terminated to prevent memory overruns
  len = strlen(buffer);  // scan for current length to pickup an changes made outside SafeString methods
}
/** end of  Information and utility methods ****************/


/***********************************************/
/**  Output and debug control methods          */
/** setOutput(), turnOutputOff(), setVerbose() */
/***********************************************/

// setOutput( ) -- turns on Error msgs and debug( )
// This static method effects ALL SafeStrings
// use
//   SafeString::setOutput(Serial);
// to output Error msgs and debug to the Serial connection
// use
//   SafeString::turnOutputOff();
// to stop all Error msgs and debug output (sets debugPtr to NULL)
// verbose is an optional argument, if missing defaults to true, use false for compact error messages or call setVerbose(false)
void SafeString::setOutput(Print& debugOut, bool verbose) {
  debugPtr = &debugOut;
  fullDebug = verbose;  // the verbose argument is optional, if missing fullDebug is true
  currentOutput = debugPtr;
}


// turnOutputOff() -- turns off error msgs and debugging output
// This static method effects ALL SafeStrings
// use
//   SafeString::turnOutputOff();
// to stop all Error msgs and debug output (sets debugPtr to NULL)
void SafeString::turnOutputOff() {
  debugPtr = NULL;
  currentOutput = &emptyPrint;
}

// setVerbose( ) -- controls level of error msgs
// This static method effects ALL SafeStrings
// use
//   SafeString::setVerbose(true);
// for detailed error msgs.use this to control error messages verbose output
//   SafeString::setVerbose(false);
// for minimal error msgs
// setVerbose( ) does not effect debug() methods which have their own optional verbose argument
void SafeString::setVerbose(bool verbose) {
  fullDebug = verbose;
}
/** end of Output and debug control methods ***********/

/*********************************************/
/**  debug methods                           */
/*********************************************/

// debug() -- these debug( ) methods print out info on this SafeString object, iff SaftString::setOutput( ) has been called
// setVerbose( ) does NOT effect these methods which have their own verbose argument
// Each of these debug( ) methods defaults to outputting the string contents.
// Set the optional verbose argument to false to suppress outputting string contents
//
// NOTE!! all these debug methods return a pointer to an empty char[].
// This is so that if you add .debug() to Serial.println(str);  i.e.
//    Serial.println(str.debug());
// will work as expected
const char* SafeString::debug(bool verbose) { // verbose optional defaults to true
  debug((const char*) NULL, verbose); // calls cleanUp();
  emptyDebugRtnBuffer[0] = '\0'; // if the last return was changed
  return emptyDebugRtnBuffer;
}

// These three versions print leading text before the debug output.
const char* SafeString::debug(const __FlashStringHelper * pstr, bool verbose) { // verbose optional defaults to true
  cleanUp();
  if (debugPtr) {
    if (pstr) {
      debugPtr->print(pstr);
    } else {
      debugPtr->print(F("SafeString"));
    }
    debugInternal(verbose);
  }
  emptyDebugRtnBuffer[0] = '\0'; // if the last return was changed
  return emptyDebugRtnBuffer;
}

const char* SafeString::debug(const char *title, bool verbose) { // verbose optional defaults to true
  cleanUp();
  if (debugPtr) {
    if (title) {
      debugPtr->print(title);
    } else {
      debugPtr->print(F("SafeString"));
    }
    debugInternal(verbose);
  }
  emptyDebugRtnBuffer[0] = '\0'; // if the last return was changed
  return emptyDebugRtnBuffer;
}

const char* SafeString::debug(SafeString &stitle, bool verbose) { // verbose optional defaults to true
  cleanUp();
  stitle.cleanUp();
  if (debugPtr) {
    if (stitle.len != 0) {
      debugPtr->print(stitle);
    } else {
      debugPtr->print(F("SafeString"));
    }
    debugInternal(verbose);
  }
  emptyDebugRtnBuffer[0] = '\0'; // if the last return was changed
  return emptyDebugRtnBuffer;
}

/*****************  end public debug methods *************************/


/*********************************************/
/**  Printable interface, printTo()          */
/*********************************************/
// this interface allows you to print to a SafeString
// e.g.
//  sfStr.print(55,HEX);
//
size_t SafeString::printTo(Print& p) const {
  SafeString *ptr = (SafeString *)(this); // do this to get around the required const designator
  ptr->cleanUp();
  return p.print(buffer);
}
/***************** end of implementation of Printable interface *************************/

/*************************************************/
/**  Print support, write(), print..() methods   */
/*************************************************/
size_t SafeString::write(uint8_t b) {
  cleanUp();
  if (b == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("write"));
      debugPtr->print(F(" of 0"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return 0;
  }
  size_t newlen = len + 1;
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("write"), newlen, NULL, NULL);
#endif // SSTRING_DEBUG
    return 0;
  }
  concat((char)b);
  return 1;
}

size_t SafeString::write(const uint8_t *buffer, size_t length) {
  cleanUp();
  if (length == 0) {
    return 0;
  }
  size_t initialLen = len;
  size_t newlen = len + length;
  if (length > strlen((char *)buffer)) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("write"));
      debugPtr->print(F(" length ")); debugPtr->print(length); debugPtr->print(F(" > uint8_t* arg strlen."));
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was "));
        debugPtr->print(F(" { "));
        for (size_t i = 0; i < length; i++) {
          debugPtr->print(buffer[i]); debugPtr->print(i < (length - 1) ? ',' : ' ');
        }
        debugPtr->print(F("} "));
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return 0;
  }
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("write"), newlen, (const char*)buffer, NULL, '\0', length);
#endif // SSTRING_DEBUG
    return 0;
  }
  concat((const char*)buffer, length);
  return len - initialLen;
}

size_t SafeString::println() {
  cleanUp();
  size_t newlen = len + 2;
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("println"), newlen, NULL, NULL);
#endif // SSTRING_DEBUG
    return 0;
  }
  Print::println();
  return 2;
}

SafeString & SafeString::newline() {
  println(); // calls cleanUp()
  return *this;
}

// used by println
SafeString & SafeString::concatln(char c) {
  cleanUp();
  if (c == '\0') {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      printlnErr();
      debugPtr->print(F(" of '\\0'"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  size_t newlen = len + 1 + 2;
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("println"), newlen, NULL, NULL, c);
#endif // SSTRING_DEBUG
    return *this;
  }
  concat(c);
  Print::println();
  return *this;
}

SafeString & SafeString::concatln(const __FlashStringHelper * pstr) {
  cleanUp();
  if (!pstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      printlnErr();
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  size_t pLen = strlen_P((PGM_P)pstr);
  size_t newlen = len + pLen + 2;
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("println"), newlen, NULL, pstr);
#endif // SSTRING_DEBUG
    return *this;
  }
  concat(pstr, strlen_P((PGM_P)pstr));
  Print::println();
  return *this;
}

SafeString & SafeString::concatln(const char *cstr, size_t length) {
  cleanUp();
  size_t newlen = len + length + 2;
  if (!cstr)  {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      printlnErr();
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }

  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("println"), newlen, cstr, NULL, '\0', length);
#endif // SSTRING_DEBUG
    return *this;
  }
  concat(cstr, length);
  Print::println();
  return *this;
}

// concat at most length chars from cstr
SafeString & SafeString::concat(const char *cstr, size_t length) {
  cleanUp();
  size_t newlen = len + length;
  if (!cstr)  {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      concatErr();
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  if (length == 0) {
    return *this;
  }
  if (length > strlen(cstr)) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      concatErr();
      debugPtr->print(F(" length ")); debugPtr->print(length); debugPtr->print(F(" > char* arg strlen."));
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(cstr); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("concat"), newlen, cstr, NULL, '\0', length);
#endif // SSTRING_DEBUG
    return *this;
  }
  memmove(buffer + len, cstr, length);
  len = newlen;
  buffer[len] = '\0';
  return *this;
}

size_t SafeString::print(SafeString &str) {
  str.cleanUp();
  cleanUp();
  size_t addLen = str.len;
  size_t newlen = len + addLen;
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("print"), newlen, str.buffer, NULL);
#endif // SSTRING_DEBUG
    return 0;
  }
  concat(str);
  return addLen;
}

size_t SafeString::println(SafeString &str) {
  str.cleanUp();
  cleanUp();
  size_t newlen = len + str.len + 2;
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("println"), newlen, str.buffer);
#endif // SSTRING_DEBUG
    return 0;
  }
  size_t addLen = str.len + 2;
  concat(str);
  Print::println();
  return addLen;
}

size_t SafeString::print(const char* cstr) {
  cleanUp();
  if (!cstr)  {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("print"));
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return 0;
  }
  size_t str_len = strlen(cstr);
  size_t newlen = len + str_len;
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("print"), newlen, cstr, NULL);
#endif // SSTRING_DEBUG
    return 0;
  }
  concat(cstr, str_len);
  return str_len;
}

size_t SafeString::print(char c) {
  cleanUp();
  if (c == '\0') {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("print"));
      debugPtr->print(F(" of '\\0'"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return 0;
  }

  size_t newlen = len + 1;
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("print"), newlen, NULL, NULL, c);
#endif // SSTRING_DEBUG
    return 0;
  }
  concat(c);
  return 1;
}

size_t SafeString::print(unsigned char b, int base) {
  return print((unsigned long) b, base); // calls cleanUp()
}

size_t SafeString::print(int n, int base) {
  return print((long) n, base); // calls cleanUp()
}


size_t SafeString::print(unsigned int n, int base) {
  return print((unsigned long) n, base); // calls cleanUp()
}

size_t SafeString::print(long num, int base) {
  cleanUp();
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  size_t n = temp.Print::print(num, base);
  size_t newlen = len + temp.length();
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("print"), newlen, temp.buffer, NULL);
#endif // SSTRING_DEBUG
    return 0;
  }
  concat(temp);
  return n;
}

size_t SafeString::print(unsigned long num, int base) {
  cleanUp();
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  size_t n = temp.Print::print(num, base);
  size_t newlen = len + temp.length();
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("print"), newlen, temp.buffer, NULL);
#endif // SSTRING_DEBUG
    return 0;
  }
  concat(temp);
  return n;
}

size_t SafeString::print(double num, int digits) {
  cleanUp();
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  if (digits > 7) {
    digits = 7; // seems to be the limit for print
  }
  size_t n = temp.Print::print(num, digits);
  size_t newlen = len + temp.length();
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("print"), newlen, temp.buffer, NULL);
#endif // SSTRING_DEBUG
    return 0;
  }
  concat(temp);
  return n;
}

size_t SafeString::print(const __FlashStringHelper *pstr) {
  cleanUp();
  size_t newlen = len + strlen_P((PGM_P)pstr);
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("print"), newlen, NULL, pstr);
#endif // SSTRING_DEBUG
    return 0;
  }
  concat(pstr);
  return (strlen_P((PGM_P)pstr));
}

size_t SafeString::println(const __FlashStringHelper *pstr) {
  concatln(pstr); // calls cleanUp()
  return (strlen_P((PGM_P)pstr) + 2);
}

size_t SafeString::println(const char* cstr) {
  if (!cstr)  {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      printlnErr();
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return 0;
  }
  size_t length = strlen(cstr);
  concatln(cstr, length);
  return length + 2;
}

size_t SafeString::println(char c) {
  concatln(c); // calls cleanUp()
  return 3;
}

size_t SafeString::println(unsigned char b, int base) {
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  size_t n = temp.Print::print(b, base);
  concatln(temp.buffer, temp.len); // calls cleanUp()
  return n + 2;
}

size_t SafeString::println(int num, int base) {
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  size_t n = temp.Print::print(num, base);
  concatln(temp.buffer, temp.len); // calls cleanUp()
  return n + 2;
}

size_t SafeString::println(unsigned int num, int base) {
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  size_t n = temp.Print::print(num, base);
  concatln(temp.buffer, temp.len); // calls cleanUp()
  return n + 2;
}

size_t SafeString::println(long num, int base) {
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  size_t n = temp.Print::print(num, base);
  concatln(temp.buffer, temp.len); // calls cleanUp()
  return n + 2;
}

size_t SafeString::println(unsigned long num, int base) {
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  size_t n = temp.Print::print(num, base);
  concatln(temp.buffer, temp.len); // calls cleanUp()
  return n + 2;
}

size_t SafeString::println(double num, int digits) {
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  if (digits > 18) {
    digits = 18;
  }
  size_t n = temp.Print::print(num, digits);
  concatln(temp.buffer, temp.len); // calls cleanUp()
  return n + 2;
}

/*** end of write() and print..() methods for Print support ***********/


/*********************************************/
/* No Memory Management methods              */
/*********************************************/
// just checks there is enough spare space
bool SafeString::reserve(size_t size) {
  if (_capacity >= size) { // buffer never NULL
    return true;
  }
  return false; // error
}
/**** end of memory mamagement methods *****/

/*************************************************/
/**  assignment operator methods                 */
/*************************************************/
SafeString & SafeString::operator = (SafeString &rhs) {
  rhs.cleanUp();
  cleanUp();
  if (buffer == rhs.buffer) { // allow for same buffer in different SafeStrings
    return *this;
  }
  clear();
  return concat(rhs.buffer);//print(rhs);
  //return *this;
}

SafeString & SafeString::operator = (const char *cstr) {
  cleanUp();
  if (!cstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      debugPtr->print(F("Error:"));
      outputName();
      debugPtr->print(F(" = NULL assigned "));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  } // else {
  if (cstr == buffer) { // assign to itself
    return *this;
  }
  clear();
  return concat(cstr);
}

SafeString & SafeString::operator = (const __FlashStringHelper *pstr) {
  cleanUp();
  if (!pstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      debugPtr->print(F("Error:"));
      outputName();
      debugPtr->print(F(" = NULL assigned "));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  } // else
  clear();
  return concat(pstr);
  //return *this;
}

SafeString & SafeString::operator = (char c) {
  clear();  // calls cleanUp()
  return concat(c);
  //return *this;
}

SafeString & SafeString::operator = (unsigned char c) {
  clear(); // calls cleanUp()
  print(c);
  return *this;
}

SafeString & SafeString::operator = (int num) {
  clear(); // calls cleanUp()
  print(num);
  return *this;
}

SafeString & SafeString::operator = (unsigned int num) {
  clear(); // calls cleanUp()
  print(num);
  return *this;
}

SafeString & SafeString::operator = (long num) {
  clear(); // calls cleanUp()
  print(num);
  return *this;
}

SafeString & SafeString::operator = (unsigned long num) {
  clear(); // calls cleanUp()
  print(num);
  return *this;
}

SafeString & SafeString::operator = (float num) {
  clear(); // calls cleanUp()
  print(num);
  return *this;
}

SafeString & SafeString::operator = (double num) {
  clear(); // calls cleanUp()
  print(num);
  return *this;
}
/**********  assignment operator methods *************/


/*************************************************/
/**  prefix methods                              */
/** for the -= prefix operator methods           */
/**    see the SafeString.h file                 */
/*************************************************/
SafeString & SafeString::prefix(char c) {
  cleanUp();
  if (c == '\0') {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      prefixErr();
      debugPtr->print(F(" of '\\0'"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  char buf[2];
  buf[0] = c;
  buf[1] = 0;
  return prefix(buf, 1);
}

SafeString & SafeString::prefix(const char *cstr, size_t length) {
  cleanUp();
  size_t newlen = len + length;
  if (!cstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      prefixErr();
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  if (length == 0) {
    return *this;
  }
  if (length > strlen(cstr)) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      prefixErr();
      debugPtr->print(F(" length ")); debugPtr->print(length); debugPtr->print(F(" > char* arg strlen."));
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(cstr); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }

  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("prefix"), newlen, cstr, NULL, '\0', length);
#endif // SSTRING_DEBUG
    return *this;
  }
  memmove(buffer + length, buffer, len);
  if (cstr == buffer) {
    // prepending to ourselves so stop here
  } else {
    memmove(buffer, cstr, length);
  }
  len = newlen;
  buffer[len] = '\0';
  return *this;
}

SafeString & SafeString::prefix(const __FlashStringHelper * pstr) {
  cleanUp();
  if (!pstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      prefixErr();
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  return prefix(pstr, strlen_P((PGM_P)pstr));
}

SafeString & SafeString::prefix(const __FlashStringHelper * pstr, size_t length) {
  cleanUp();
  if (!pstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      prefixErr();
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  if (length == 0) {
    return *this;
  }
  if (length > strlen_P((PGM_P)pstr)) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      prefixErr();
      debugPtr->print(F(" length ")); debugPtr->print(length); debugPtr->print(F(" > F() arg strlen."));
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(pstr); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }

  size_t newlen = len + length;
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("prefix"), newlen, NULL, pstr, '\0', length);
#endif // SSTRING_DEBUG
    return *this;
  }
  memmove(buffer + length, buffer, len);
  memcpy_P(buffer, (const char *) pstr, length);
  len = newlen;
  buffer[len] = '\0';
  return *this;
}

// you can prefix to your self if there is enough room,  i.e. str.prefix(str);
SafeString & SafeString::prefix(SafeString &s) {
  s.cleanUp();
  cleanUp();
  return prefix(s.buffer, s.len);
}

SafeString & SafeString::prefix(const char *cstr) {
  cleanUp();
  if (!cstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      prefixErr();
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  } // else
  return prefix(cstr, strlen(cstr));
}

SafeString & SafeString::prefix(unsigned char num) {
  createSafeString(temp, 2 + 3 * sizeof(unsigned char));
  temp.print(num);
  return prefix(temp); // calls cleanUp()
}

SafeString & SafeString::prefix(int num) {
  createSafeString(temp, 2 + 3 * sizeof(int));
  temp.print(num);
  return prefix(temp); // calls cleanUp()
}

SafeString & SafeString::prefix(unsigned int num) {
  createSafeString(temp, 2 + 3 * sizeof(unsigned int));
  temp.print(num);
  return prefix(temp); // calls cleanUp()
}

SafeString & SafeString::prefix(long num) {
  createSafeString(temp, 2 + 3 * sizeof(long));
  temp.print(num);
  return prefix(temp); // calls cleanUp()
}

SafeString & SafeString::prefix(unsigned long num) {
  createSafeString(temp, 2 + 3 * sizeof(unsigned long));
  temp.print(num);
  return prefix(temp); // calls cleanUp()
}

SafeString & SafeString::prefix(float num) {
  createSafeString(temp, 22);
  temp.print(num);
  return prefix(temp); // calls cleanUp()
}

SafeString & SafeString::prefix(double num) {
  createSafeString(temp, 22);
  temp.print(num);
  return prefix(temp); // calls cleanUp()
}
/******** end of prefix methods **************************/


/*************************************************/
/**  concat methods                              */
/** for the += concat operator methods           */
/**    see the SafeString.h file                 */
/*************************************************/
SafeString & SafeString::concat(char c) {
  cleanUp();
  if (c == '\0') {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      concatErr();
      debugPtr->print(F(" of '\\0'"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  char buf[2];
  buf[0] = c;
  buf[1] = 0;
  return concat(buf, 1);
}

// you can concat to yourself if there is enough room, i.e. str.concat(str);
SafeString & SafeString::concat(SafeString &s) {
  s.cleanUp();
  return concat(s.buffer, s.len);
}

SafeString & SafeString::concat(const char *cstr) {
  cleanUp();
  if (!cstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      concatErr();
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  } // else
  return concat(cstr, strlen(cstr));
}

SafeString & SafeString::concat(unsigned char num) {
  createSafeString(temp, 2 + 3 * sizeof(unsigned char));
  temp.print(num);
  return concat(temp); // calls cleanUp()
}

SafeString & SafeString::concat(int num) {
  createSafeString(temp, 2 + 3 * sizeof(int));
  temp.print(num);
  return concat(temp); // calls cleanUp()
}

SafeString & SafeString::concat(unsigned int num) {
  createSafeString(temp, 2 + 3 * sizeof(unsigned int));
  temp.print(num);
  return concat(temp); // calls cleanUp()
}

SafeString & SafeString::concat(long num) {
  createSafeString(temp, 2 + 3 * sizeof(long));
  temp.print(num);
  return concat(temp); // calls cleanUp()
}

SafeString & SafeString::concat(unsigned long num) {
  createSafeString(temp, 2 + 3 * sizeof(unsigned long));
  temp.print(num);
  return concat(temp); // calls cleanUp()
}

SafeString & SafeString::concat(float num) {
  createSafeString(temp, 22);
  temp.print(num);
  return concat(temp); // calls cleanUp()
}

SafeString & SafeString::concat(double num) {
  createSafeString(temp, 22);
  temp.print(num);
  return concat(temp); // calls cleanUp()
}

SafeString & SafeString::concat(const __FlashStringHelper * pstr) {
  cleanUp();
  if (!pstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      concatErr();
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  return concat(pstr, strlen_P((PGM_P)pstr));
}

// concat at most length chars
SafeString & SafeString::concat(const __FlashStringHelper * pstr, size_t length) {
  cleanUp();
  if (!pstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      concatErr();
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  if (length == 0) {
    return *this;
  }
  if (length > strlen_P((PGM_P)pstr)) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      concatErr();
      debugPtr->print(F(" length ")); debugPtr->print(length); debugPtr->print(F(" > F() arg strlen."));
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(pstr); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }

  size_t newlen = len + length;
  if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
    capError(F("concat"), newlen, NULL, pstr, '\0', length);
#endif // SSTRING_DEBUG
    return *this;
  }
  memcpy_P(buffer + len, (PGM_P)pstr, length);
  len = newlen;
  buffer[len] = '\0';
  return *this;
}

/******** end of concat methods **************************/

/*********************************************/
/**  Concatenate                             */
/*********************************************/
// sfStr = str + 5 etc not supported because of the need to allocate/reallocat buffers
// BUT
//   (sfStr = str) += 5;
// works because = and += and -= all return a reference to the updated SafeString
// i.e. (sfStr = str) += 5; is equvalent to pair of statements
//   sfStr = str;
//   sfStr += 5;

/*************************************************/
/** Comparison methods                           */
/** for the > >= < <= == !=  operator methods    */
/**    see the SafeString.h file                 */
/*************************************************/
int SafeString::compareTo(SafeString &s) {
  s.cleanUp();
  cleanUp();
  // do a quick check of the lengths
  if (len < s.len) {
    return -1;
  } else if (len > s.len) {
    return +1;
  } //else
  return strcmp(buffer, s.buffer);
}

int SafeString::compareTo(const char* cstr) {
  cleanUp();
  if (!cstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("compareTo"));
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return 1; // > for NULL
  }
  return strcmp(buffer, cstr);
}

bool SafeString::equals(SafeString &s2) {
  s2.cleanUp();
  cleanUp();
  return ((len == s2.len) && (strcmp(buffer, s2.buffer) == 0));
}

// error if cstr is NULL
bool SafeString::equals(const char *cstr) {
  cleanUp();
  if (!cstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("equals"));
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false; // false NULL never matched any non-null SafeString since buffer is never NULL
  }
  if (len == 0) {
    return (*cstr == 0);
  } // else
  return strcmp(buffer, cstr) == 0;
}

// compare string to char
bool SafeString::equals(const char c) {
  cleanUp();
  if (c == '\0') {
    if (len == 0) {
      return true;
    } else {
      return false;
    }
  } else {
    if (len != 1) {
      return false;
    }
  }
  // else
  return buffer[0] == c;
}

bool SafeString::equalsIgnoreCase(const char *str2) {
  cleanUp();
  if (!str2) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("equalsIgnoreCase"));
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false; // false for null
  }
  if (buffer == str2) {
    return true; // same as buffer
  }
  size_t str2Len = strlen(str2);
  if (len != str2Len) {
    return false;
  }
  if (len == 0) {
    return true;
  }
  const char *p1 = buffer;
  const char *p2 = str2;
  while (*p1) {
    if (tolower(*p1++) != tolower(*p2++)) {
      return false;
    }
  }
  return true;
}

bool SafeString::equalsIgnoreCase( SafeString &s2 ) {
  s2.cleanUp();
  cleanUp();
  if (buffer == s2.buffer) { // allow for same buffer in different SafeStrings
    return true;
  }
  if (len != s2.len) {
    return false;
  }
  if (len == 0) {
    return true;
  }
  const char *p1 = buffer;
  const char *p2 = s2.buffer;
  while (*p1) {
    if (tolower(*p1++) != tolower(*p2++)) {
      return false;
    }
  }
  return true;
}

bool SafeString::equalsConstantTime(SafeString &s2) {
  s2.cleanUp();
  cleanUp();
  // To avoid possible time-based attacks present function
  // compares given strings in a constant time.
  if (len != s2.len) {
    return false;
  }
  //at this point lengths are the same
  if (len == 0) {
    return true;
  }
  //at this point lenghts are the same and non-zero
  const char *p1 = buffer;
  const char *p2 = s2.buffer;
  size_t equalchars = 0;
  size_t diffchars = 0;
  while (*p1) {
    if (*p1 == *p2)
      ++equalchars;
    else
      ++diffchars;
    ++p1;
    ++p2;
  }
  //the following should force a constant time eval of the condition without a compiler "logical shortcut"
  bool equalcond = (equalchars == len);
  bool diffcond = (diffchars == 0);
  return (bool)(equalcond & diffcond); //bitwise AND
}
/******** end of comparison methods **************************/

/*********************************************/
/** startsWith(), endsWith()  methods        */
/*********************************************/
bool SafeString::startsWith( SafeString &s2 ) {
  s2.cleanUp();
  cleanUp();
  if (len < s2.len) {
    return false;
  }
  return startsWith(s2, 0);
}

bool SafeString::startsWith( const char *str2) {
  return startsWith(str2, 0); // calls cleanUp()
}

bool SafeString::startsWith( const char *str2, size_t fromIndex ) {
  cleanUp();
  if (!str2) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("startsWith"));
      debugPtr->print(F(" was passed a NULL pointer"));
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }
  size_t str2Len = strlen(str2);
  if (str2Len == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("startsWith"));
      debugPtr->print(F(" was passed an empty char array"));
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }

  if (fromIndex == len) {
    return false;
  }
  if (fromIndex > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("startsWith"));
      debugPtr->print(F(" fromIndex ")); debugPtr->print(fromIndex); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(str2); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }
  if ((fromIndex > (len - str2Len)) ) {
    return false;
  }
  return strncmp( &buffer[fromIndex], str2, str2Len ) == 0;
}

bool SafeString::startsWith( SafeString &s2, size_t fromIndex ) {
  s2.cleanUp();
  cleanUp();
  if (s2.len == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("startsWith"));
      debugPtr->print(F(" was passed an empty SafeString ")); s2.outputName();
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }

  if (fromIndex == len) {
    return false;
  }
  if (fromIndex > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("startsWith"));
      debugPtr->print(F(" fromIndex ")); debugPtr->print(fromIndex); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(s2.buffer); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }
  if ((fromIndex + s2.len) > len ) {
    return false;
  }
  return strncmp( &buffer[fromIndex], s2.buffer, s2.len ) == 0;
}


bool SafeString::startsWithIgnoreCase( SafeString &s2 ) {
  s2.cleanUp();
  cleanUp();
  if (len < s2.len) {
    return false;
  }
  return startsWithIgnoreCase(s2, 0);
}

bool SafeString::startsWithIgnoreCase( SafeString &s2, size_t fromIndex ) {
  s2.cleanUp();
  cleanUp();
  if (s2.len == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("startsWithIgnoreCase"));
      debugPtr->print(F(" was passed an empty SafeString ")); s2.outputName();
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }

  if (fromIndex == len) {
    return false;
  }
  if (fromIndex > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("startsWithIgnoreCase"));
      debugPtr->print(F(" fromIndex ")); debugPtr->print(fromIndex); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(s2.buffer); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }
  if ((fromIndex + s2.len) > len) {
    return false;
  }
  const char *p1 = &buffer[fromIndex];
  const char *p2 = s2.buffer;
  while (*p2) { // loop through str2 have check lengths above
    if (tolower(*p1++) != tolower(*p2++)) {
      return false;
    }
  }
  return true;
}

bool SafeString::startsWithIgnoreCase( const char *str2 ) {
  return startsWithIgnoreCase(str2, 0); // calls cleanUp()
}

// return 0 false 1 true
bool SafeString::startsWithIgnoreCase( const char *str2, size_t fromIndex ) {
  cleanUp();
  if (!str2) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("startsWithIgnoreCase"));
      debugPtr->print(F(" was passed a NULL pointer"));
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }

  size_t str2Len = strlen(str2);
  if (str2Len == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("startsWithIgnoreCase"));
      debugPtr->print(F(" was passed an empty char array"));
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }

  if (fromIndex == len) {
    return false;
  }
  if (fromIndex > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("startsWithIgnoreCase"));
      debugPtr->print(F(" fromIndex ")); debugPtr->print(fromIndex); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(str2); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }
  if ((fromIndex + str2Len) > len) {
    return false;
  }
  const char *p1 = &buffer[fromIndex];
  const char *p2 = str2;
  while (*p2) { // loop through str2 have check lengths above
    if (tolower(*p1++) != tolower(*p2++)) {
      return false;
    }
  }
  return true;
}

bool SafeString::endsWith( SafeString &s2 ) {
  s2.cleanUp();
  cleanUp();
  if (s2.len == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("endsWith"));
      debugPtr->print(F(" was passed an empty SafeString ")); s2.outputName();
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }

  if (buffer == s2.buffer) {
    return true; // same SafeString
  }
  if ( len < s2.len ) {
    return false;
  }
  return strcmp(&buffer[len - s2.len], s2.buffer) == 0;
}

bool SafeString::endsWith(const char *suffix) {
  cleanUp();
  if (!suffix) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("endsWith"));
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }
  size_t str2Len = strlen(suffix);
  if (str2Len == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("endsWith"));
      debugPtr->print(F(" was passed an empty char array"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }

  size_t s_len = strlen(suffix);
  if (len < s_len ) {
    return false;
  }
  return strcmp(&buffer[len - s_len], suffix) == 0;
}

bool SafeString::endsWithCharFrom(SafeString &s2) {
  s2.cleanUp();
  cleanUp();
  if (s2.len == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("endsWithCharFrom"));
      debugPtr->print(F(" was passed an empty SafeString ")); s2.outputName();
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }

  if (buffer == s2.buffer) {
    return true; // same SafeString
  }
  return endsWithCharFrom(s2.buffer);
}

bool SafeString::endsWithCharFrom(const char *suffix) {
  cleanUp();
  if (!suffix) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("endsWithCharFrom"));
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }
  size_t str2Len = strlen(suffix);
  if (str2Len == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("endsWithCharFrom"));
      debugPtr->print(F(" was passed an empty char array"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }
  if (len == 0) {
    return false;
  }
  char c = buffer[len - 1];
  if (strchr(suffix, c) != NULL) {
    return true;
  } // else
  return false;
}

/*******  end of startsWith(), endsWith() methods ***********/

/*********************************************/
/**  Character Access                        */
/*********************************************/
char SafeString::charAt(size_t index) {
  cleanUp();
  if (index >= len ) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      debugPtr->print(F("Error: "));
      outputName();
      debugPtr->print(F(".charAt() index ")); debugPtr->print(index); debugPtr->print(F(" >= ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return '\0';
  }
  return buffer[index];
}

SafeString& SafeString::setCharAt(size_t index, char c) {
  cleanUp();
  if (c == '\0') {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      debugPtr->print(F("Error:"));
      outputName();
      debugPtr->print(F(".setCharAt("));
      debugPtr->print(index); debugPtr->print(F(",'\\0');"));
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));  debugPtr->print(F(" Setting character to '\\0' not allowed."));
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  if (index >= len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      debugPtr->print(F("Error:"));
      outputName();
      debugPtr->print(F(".setCharAt() index ")); debugPtr->print(index); debugPtr->print(F(" >= ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(c); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  buffer[index] = c;
  return *this;
}

char SafeString::operator[]( size_t index ) {
  cleanUp();
  if (index >= len ) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      debugPtr->print(F("Error: "));
      outputName();
      debugPtr->print(F("[] index ")); debugPtr->print(index); debugPtr->print(F(" >= ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return '\0';
  }
  return buffer[index];
}

const char* SafeString::c_str() {
  cleanUp();
  // mark as subject to external modification now as we have exposed the internal buffer
  fromBuffer = true;  // added V3.0.4
  return buffer;
}

/****  end if character access methods *******************/

/*************************************************/
/**  Search methods  indexOf() lastIndexOf()     */
/*************************************************/
/**
    Search
       Arrays are indexed by a size_t varialble which for 8bit Arduino boards e.g. UNO,MEGA  is unsigned int but can be larger for more powerful chips.
       SafeString methods that return an index use size_t where as Arduino methods use int
       See the SafeStringIndexOf.ino example sketch

      All indexOf methods return length() if not found ( or length() + 1 on error)
      so test with
        size_t a_idx = str.indexOf('a');
        if (a_idx >= str.length()) { // not found

      DO NOT use the test
      if (a_idx < 0) {
        it is NEVER true,  size_t variable is ALWAYS >= 0
**********************************************/
// The fromIndex is offset into this SafeString where check is to searching (inclusive)
// 0 to length() is valid for fromIndex, if fromIndex == length()  length() (i.e. not found) is returned
// if fromIndex > length(), than the error return length()+1 is returned and prints an error if debug enabled
size_t SafeString::indexOf(char c) {
  return indexOf(c, 0); // calls cleanUp()
}

size_t SafeString::indexOf( char c, size_t fromIndex ) {
  cleanUp();
  if (c == '\0') {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("indexOf"));
      debugPtr->print(F(" char arg was '\\0'"));
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1; // probabily an error
  }

  if (fromIndex == len) {
    return len;
  }
  if (fromIndex > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("indexOf"));
      debugPtr->print(F(" fromIndex ")); debugPtr->print(fromIndex); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(c); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }

  const char* temp = strchr(buffer + fromIndex, c);
  if (temp == NULL) {
    return len; // not found
  }
  return temp - buffer;
}

size_t SafeString::indexOf(SafeString &s2) {
  s2.cleanUp();
  cleanUp();
  if (s2.len == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("indexOf"));
      debugPtr->print(F(" was passed an empty SafeString ")); s2.outputName();
      outputFromIndexIfFullDebug(0);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }

  if (buffer == s2.buffer) {
    return 0; // same SafeString
  }
  return indexOf(s2, 0);
}

size_t SafeString::indexOf(SafeString &s2, size_t fromIndex) {
  s2.cleanUp();
  cleanUp();
  if (s2.len == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("indexOf"));
      debugPtr->print(F(" was passed an empty SafeString ")); s2.outputName();
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }

  if (fromIndex == len) {
    return len;
  }

  if (fromIndex > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("indexOf"));
      debugPtr->print(F(" fromIndex ")); debugPtr->print(fromIndex); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(s2.buffer); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }

  if (len == 0)  {
    return len;
  }

  const char *found = strstr(buffer + fromIndex, s2.buffer);
  if (found == NULL) {
    return len;
  }
  return found - buffer;
}

size_t SafeString::indexOf( const char* str ) {
  return indexOf(str, 0); // calls cleanUp()
}

size_t SafeString::indexOf(const char* cstr , size_t fromIndex) {
  cleanUp();
  if (!cstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("indexOf"));
      debugPtr->print(F(" was passed a NULL pointer"));
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }
  size_t cstrLen = strlen(cstr);
  if (cstrLen == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("indexOf"));
      debugPtr->print(F(" was passed an empty char array"));
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }

  if (fromIndex == len) {
    return len;
  }
  if (fromIndex > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("indexOf"));
      debugPtr->print(F(" fromIndex ")); debugPtr->print(fromIndex); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(cstr); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }

  if (len == 0)  {
    return len;
  }

  const char *found = strstr(buffer + fromIndex, cstr);
  if (found == NULL) {
    return len;
  }
  return found - buffer;
}

size_t SafeString::lastIndexOf( char theChar ) {
  return lastIndexOf(theChar, len); // calls cleanUp()
}

size_t SafeString::lastIndexOf(char ch, size_t fromIndex) {
  cleanUp();
  if (ch == '\0') {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("lastIndexOf"));
      debugPtr->print(F(" char arg was '\\0'"));
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1; // probabily an error
  }
  if (len == 0) {
    return len;
  }
  if (fromIndex == len) {
    fromIndex = len - 1;
  }
  if (fromIndex > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("lastIndexOf"));
      debugPtr->print(F(" fromIndex ")); debugPtr->print(fromIndex); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(ch); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }
  char tempchar = buffer[fromIndex + 1];
  buffer[fromIndex + 1] = '\0';
  char* temp = strrchr( buffer, ch );
  buffer[fromIndex + 1] = tempchar;
  if (temp == NULL) {
    return len;
  }
  return temp - buffer;
}

size_t SafeString::lastIndexOf(SafeString &s2) {
  s2.cleanUp();
  cleanUp();
  if (len < s2.len) {
    return len;
  } // else len - s2.len is valid
  return lastIndexOf(s2, len - s2.len);
}

size_t SafeString::lastIndexOf( const char *cstr ) {
  if (!cstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("lastIndexOf"));
      debugPtr->print(F(" was passed a NULL pointer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }
  size_t cstrlen = strlen(cstr);
  if (len < cstrlen) {
    return len;
  } // else len - strlen(cstr) is valid
  return lastIndexOf(cstr, len - cstrlen);
}

size_t SafeString::lastIndexOf(SafeString &s2, size_t fromIndex) {
  s2.cleanUp();
  cleanUp();
  if (s2.len == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("lastIndexOf"));
      debugPtr->print(F(" was passed an empty SafeString ")); s2.outputName();
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }
  if (len == 0) {
    return len;
  }
  if (fromIndex == len) {
    fromIndex = len - 1;
  }
  if (fromIndex > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("lastIndexOf"));
      debugPtr->print(F(" fromIndex ")); debugPtr->print(fromIndex); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(s2.buffer); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }
  if (s2.len > len) {
    return len;
  }

  return lastIndexOf(s2.buffer, fromIndex);
}

size_t SafeString::lastIndexOf(const char* cstr, size_t fromIndex) {
  cleanUp();
  if (!cstr) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("lastIndexOf"));
      debugPtr->print(F(" was passed a NULL pointer"));
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }

  size_t cstrlen = strlen(cstr);
  if (cstrlen == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("lastIndexOf"));
      debugPtr->print(F(" was passed an empty char array"));
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }

  if (len == 0) {
    return len;
  }

  if (fromIndex == len) {
    fromIndex = len - 1; // checked for len==0 above
  }

  if (fromIndex > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("lastIndexOf"));
      debugPtr->print(F(" fromIndex ")); debugPtr->print(fromIndex); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(cstr); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }
  if (cstrlen > len) {
    return len;
  }

  int found = len;
  for (char *p = buffer; p <= buffer + fromIndex; p++) {
    p = strstr(p, cstr);
    if (!p) { // not found
      break;
    } // else
    if ((size_t)(p - buffer) <= fromIndex) {
      found = p - buffer;
    }
  }
  return found;
}

/**
  find first index of one of the chars in the arg
*/
size_t SafeString::indexOfCharFrom(SafeString &str) {
  str.cleanUp();
  return indexOfCharFrom(str.buffer, 0); // calls cleanUp()
}

size_t SafeString::indexOfCharFrom(SafeString &str, size_t fromIndex) {
  str.cleanUp();
  return indexOfCharFrom(str.buffer, fromIndex); // calls cleanUp()
}

size_t SafeString::indexOfCharFrom(const char* chars) {
  return indexOfCharFrom(chars, 0); // calls cleanUp()
}

size_t SafeString::indexOfCharFrom(const char* chars, size_t fromIndex) {
  cleanUp();
  if (!chars) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("indexOfCharFrom"));
      debugPtr->print(F(" was passed a NULL pointer"));
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }
  size_t charsLen = strlen(chars);
  if (charsLen == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("indexOfCharFrom"));
      debugPtr->print(F(" was passed an empty set of chars"));
      outputFromIndexIfFullDebug(fromIndex);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }
  if (fromIndex == len) {
    return len;
  }
  if (fromIndex > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("indexOfCharFrom"));
      debugPtr->print(F(" fromIndex ")); debugPtr->print(fromIndex); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(chars); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }
  if (len == 0)  {
    return len;
  }
  int minIdx = len; // not found
  const char* cPtr = chars;
  while (*cPtr) {
    int idx = indexOf(*cPtr, fromIndex);
    if (idx == 0) {
      return 0; // found min
    } else if (idx > 0)  {
      if (idx < minIdx) {
        minIdx = idx; // update new min
      } // else nothing
    }
    cPtr++;
  }
  return minIdx;
}

/****  end of Search methods  *******************************/

/*************************************************/
/**  substring methods                           */
/*************************************************/
// 0 to length() is valid for beginIdx;
// if beginIdx == length(), an empty result will be returned
// substring is from beginIdx to end of string
// can take substring of yourself  e.g. str.substring(str,3);
SafeString & SafeString::substring(SafeString &result, size_t beginIdx) {
  result.cleanUp();
  cleanUp();
  if ((len == 0) && (beginIdx == 0)) {
    result.clear();
    return result;
  }
  // len == 0;
  if (len == 0) {
    if (beginIdx == len) {
      return result; // no change
    }

    if (beginIdx > len) {
#ifdef SSTRING_DEBUG
      if (debugPtr) {
        errorMethod(F("substring"));
        debugPtr->print(F(" beginIdx ")); debugPtr->print(beginIdx); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
        debugInternalMsg(fullDebug);
      }
#endif // SSTRING_DEBUG
      return result; // no change
    }
  }
  // else  (len > 0) {
  return substring(result, beginIdx, len);
}


// 0 to length() is valid for beginIdx;
// if beginIdx == length(), an empty result will be returned
// substring is from beginIdx to endIdx-1, endIdx is exclusive (was includsive in V1)
// endIdx must be >= beginIdx and <= length()
// substring is from beginIdx to endIdx (endiIdx is EXCLUSIVE) Note this changed in V2 used to be inclusive
// can take substring of yourself  e.g. str.substring(str,3,6);
SafeString & SafeString::substring(SafeString &result, size_t beginIdx, size_t endIdx) {
  result.cleanUp();
  cleanUp();
  if ((len == 0) && (beginIdx == 0) && (endIdx == 0)) {
    result.clear();
    return result;
  }

  if (beginIdx > len) { //== len is OK
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("substring"));
      debugPtr->print(F(" beginIdx ")); debugPtr->print(beginIdx); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return result; // no change
  }

  if (endIdx < beginIdx) {  // beginIdx == len == endIdx is OK
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("substring"));
      debugPtr->print(F(" endIdx ")); debugPtr->print(endIdx); debugPtr->print(F(" <")); debugPtr->print(F(" beginIdx ")); debugPtr->print(beginIdx);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return result; // no change
  }

  if (endIdx > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("substring"));
      debugPtr->print(F(" endIdx ")); debugPtr->print(endIdx); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return result; // no change
  }
  // here beginIdx <= endIdx AND endidx <= len
  if ((beginIdx == len) || (beginIdx == endIdx)) {
    return result.clear();
  }
  // here beginIdx < len AND endIdx <= len
  //  if (endIdx == len) {
  //    endIdx = len - 1;
  //  }

  // copy to result
  size_t copyLen = endIdx - beginIdx; // endIdx is exclusive 5,5 copies 0 chars 5,6 copies 1 char char[5].
  if (copyLen > result.capacity()) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("substring"));
      debugPtr->print(F(" result SafeString")); result.outputName(); debugPtr->print(F(" needs capacity of "));
      debugPtr->print(copyLen);
      result.debugInternalResultMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return result; // no change
  }
  // memmove incase result and SafeString are the same
  memmove(result.buffer, buffer + beginIdx, copyLen);
  result.len = copyLen;
  result.buffer[result.len] = '\0';
  return result;
}
/*****  end of substring methods  *************************/


/*********************************************************/
/**  Modification replace(), remove(), removeLast()      */
/*********************************************************/

/*****  replace(), methods ***********/
// replace single char with another
SafeString & SafeString::replace(char f, char r) {
  cleanUp();
  if (len == 0) {
    return *this;
  }
  if (f == '\0') {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("replace"));
      debugPtr->print(F(" find char is '\\0'"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  if (r == '\0') {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("replace"));
      debugPtr->print(F(" replace char is '\\0'"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  for (char *p = buffer; *p; p++) {
    if (*p == f) {
      *p = r;
    }
  }
  return *this;
}

// replace sequence of chars with another sequence (case sensitive)
SafeString & SafeString::replace(SafeString& f, SafeString& r) {
  f.cleanUp();
  r.cleanUp();
  cleanUp();
  if (f.len == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("replace"));
      debugPtr->print(F(" find SafeString")); f.outputName(); debugPtr->print(F(" is empty."));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  return replace(f.buffer, r.buffer);
}

SafeString & SafeString::replace(const char* find, const char *replace) {
  cleanUp();
  if (!find) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("replace"));
      debugPtr->print(F(" find arg is NULL"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  if (!replace) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("replace"));
      debugPtr->print(F(" replace arg is NULL"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }

  size_t findLen = strlen(find);
  size_t replaceLen = strlen(replace);
  if (len == 0) {
    return *this;
  }
  if (find == buffer) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("replace"));
      debugPtr->print(F(" find arg is same SafeString"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  if (replace == buffer) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("replace"));
      debugPtr->print(F(" replace arg is same SafeString"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  if (findLen == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("replace"));
      debugPtr->print(F(" find ")); debugPtr->print(F(" is empty."));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  int diff = replaceLen - findLen;
  char *_readFrom = buffer;
  char *foundAt;
  if (diff == 0) {
    while ((foundAt = strstr(_readFrom, find)) != NULL) {
      memmove(foundAt, replace, replaceLen);
      _readFrom = foundAt + replaceLen; // prevents replacing the replace
    }
  } else if (diff < 0) {
    char *writeTo = buffer;
    while ((foundAt = strstr(_readFrom, find)) != NULL) {
      size_t n = foundAt - _readFrom;
      memmove(writeTo, _readFrom, n);
      writeTo += n;
      memmove(writeTo, replace, replaceLen);
      writeTo += replaceLen;
      _readFrom = foundAt + findLen; // prevents replacing the replace
      len += diff;
    }
    memmove(writeTo, _readFrom, strlen(_readFrom) + 1);
  } else {
    size_t newlen = len; // compute size needed for result
    while ((foundAt = strstr(_readFrom, find)) != NULL) {
      _readFrom = foundAt + findLen;
      newlen += diff;
    }
    if (!reserve(newlen)) {
#ifdef SSTRING_DEBUG
      capError(F("replace"), newlen, find);
      if (fullDebug) {
        debugPtr->print(F("       "));
        debugPtr->print(F(" Replace arg was '")); debugPtr->print(replace); debugPtr->println('\'');
      }
#endif // SSTRING_DEBUG
      return *this;
    }

    size_t index = len - 1; // len checked for != above
    while ((index = lastIndexOf(find, index)) < len) {
      _readFrom = buffer + index + findLen;
      memmove(_readFrom + diff, _readFrom, len - (_readFrom - buffer));
      int newLen = len + diff;
      memmove(buffer + index, replace, replaceLen);
      len = newLen;
      buffer[newLen] = 0;
      if (index == 0) {
        break; // at front of string
      } // else
      index--;
    }
    len = newlen;
    buffer[newlen] = 0;
  }
  return *this;
}
/***** end of  replace(), methods ***********/


/***** removeFrom(), keepFrom() remove(), remooveLast(), keepLast() methods ***********/
// remove from index to end of SafeString
// 0 to length() is valid for index
SafeString & SafeString::removeFrom(size_t startIndex) {
  return remove(startIndex, len - startIndex); // calls cleanUp()
}

// remove from 0 to startIdx (excluding startIdx)
// 0 to length() is valid for startIndex
SafeString & SafeString::removeBefore(size_t startIndex) {
  return remove(0, startIndex); // calls cleanUp()
}

// remove count chars starting from index
// 0 to length() is valid for index
// 0 to (length()- index) is valid for count
SafeString & SafeString::remove(size_t index, size_t count) {
  cleanUp();
  if ((index == len) && (count == 0)) {
    return *this;
  }
  if (index > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("remove"));
      debugPtr->print(F(" index ")); debugPtr->print(index); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  if (count == 0) {
    /**
      #ifdef SSTRING_DEBUG
      if (debugPtr) {
      errorMethod(F("remove"));
      debugPtr->print(F(" count is 0"));
      debugInternalMsg(fullDebug);
      }
      #endif // SSTRING_DEBUG
    **/
    return *this;
  }
  if ((count + index) > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("remove"));
      debugPtr->print(F(" index + count ")); debugPtr->print(count + index); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
    //count = len - index;
  }
  char *writeTo = buffer + index;
  memmove(writeTo, buffer + index + count, len - index - count + 1);
  len -= count;
  buffer[len] = 0;
  return *this;
}

// remove the last 'count' chars
// 0 to length() is valid for count, passing in count == length() clears the SafeString
SafeString & SafeString::removeLast(size_t count) {
  cleanUp();
  if (count == 0) {
    //nothing to do
    return *this;
  }
  // else
  if (count == len) {
    // remove all
    clear();
    return *this;
  }
  if (count > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("removeLast"));
      debugPtr->print(F(" count ")); debugPtr->print(count); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  // else
  return remove(len - count, count);
}

// keep last 'count' number of chars remove the rest
// 0 to length() is valid for count, passing in count == 0 clears the SafeString
SafeString & SafeString::keepLast(size_t count) {
  cleanUp();
  if (count == 0) {
    //just clear string
    clear();
    return *this;
  }
  // else
  if (count == len) {
    // keep whole string
    return *this;
  }
  if (count > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("keepLast"));
      debugPtr->print(F(" count ")); debugPtr->print(count); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return *this;
  }
  // else
  return remove(0, len - count);
}

/***** end of removeFrom(), keepFrom() remove(), remooveLast(), keepLast() methods ***********/

/*****  end of Modification replace(), remove(), removeLast()  *******/

/******************************************************/
/** Change case methods, toLowerCase(), toUpperCase() */
/******************************************************/
SafeString & SafeString::toLowerCase(void) {
  cleanUp();
  for (char *p = buffer; *p; p++) {
    *p = tolower(*p);
  }
  return *this;
}

SafeString & SafeString::toUpperCase(void) {
  cleanUp();
  for (char *p = buffer; *p; p++) {
    *p = toupper(*p);
  }
  return *this;
}
/** end of Change case methods, toLowerCase(), toUpperCase() *******/

/******************************************************/
/** trim()                                             */
/******************************************************/
// trim() -- remove white space from front and back of SafeString
// the method isspace( ) is used to.  For the 'C' local the following are trimmed
//    ' '     (0x20)  space (SPC)
//    '\t'  (0x09)  horizontal tab (TAB)
//    '\n'  (0x0a)  newline (LF)
//    '\v'  (0x0b)  vertical tab (VT)
//    '\f'  (0x0c)  feed (FF)
//    '\r'  (0x0d)  carriage return (CR)
SafeString & SafeString::trim(void) {
  cleanUp();
  if ( len == 0) {
    return *this;
  }
  char *begin = buffer;
  while (isspace(*begin)) {
    begin++;
  }
  char *end = buffer + len - 1;
  while (isspace(*end) && end >= begin) {
    end--;
  }
  len = end + 1 - begin;
  if (begin > buffer) {
    memmove(buffer, begin, len);
  }
  buffer[len] = 0;
  return *this;
}
/** end of trim()  *****************************/

/******************************************************/
/** processBackspaces()                               */
/******************************************************/
// processBackspaces -- recursively remove backspaces, '\b' and the preceeding char
// use for processing inputs from terminal (Telent) connections
SafeString & SafeString::processBackspaces(void) {
  cleanUp();
  if ( len == 0) {
    return *this;
  }
  size_t idx = 0;
  while ((!isEmpty()) && (idx < length())) {
    if (charAt(idx) == '\b') {
      if (idx == 0) {
        remove(idx, 1); // no previous char just remove backspace
      } else {
        idx = idx - 1; // remove previous char and this backspace
        remove(idx, 2);
      }
    } else {
      idx++;
    }
  }
  return *this;
}
/** end of processBackspaces() **************************/

/*********************************************/
/**  Number Parsing / Conversion  methods    */
/*********************************************/
// convert numbers
// If the SafeString is a valid number update the argument with the result
// else leave the argument unchanged
// SafeString conversions are stricter than the Arduino String version
// trailing chars can only be white space

// convert decimal number to int, arg i unchanged if no valid number found
bool SafeString::toInt(int &i) {
  cleanUp();
  if (len == 0) {
    return false; // not found
  }
  char* endPtr;
  long result = strtol(buffer, &endPtr, 10); // handles 123 (use 0 for 0xAF and 037 (octal))
  if (result > INT_MAX) {
    return false;
  }
  if (result < INT_MIN) {
    return false;
  }
  // check endPtr to see if number valid 5a is invalid,  5. is valid
  if (endPtr == buffer)  { // no numbers found at all
    return false;
  } // else
  // else check for trailing white space
  while (*endPtr != '\0') {
    if (!isspace(*endPtr)) { // number terminated by white space
      return false;
    }
    endPtr++;
  }
  // else all OK
  i = result;
  return true; // OK
}

// convert decimal number to long, arg 1 unchanged if no valid number found
bool SafeString::toLong(long &l) {
  cleanUp();
  if (len == 0) {
    return false; // not found
  }
  char* endPtr;
  long result = strtol(buffer, &endPtr, 10); // handles 123 (use 0 for 0xAF and 037 (octal))
  if (result == LONG_MAX) {
    return false;
  }
  if (result == LONG_MIN) {
    return false;
  }
  // check endPtr to see if number valid 5a is invalid,  5. is valid
  if (endPtr == buffer)  { // no numbers found at all
    return false;
  } // else
  // else check for trailing white space
  while (*endPtr != '\0') {
    if (!isspace(*endPtr)) { // number terminated by white space
      return false;
    }
    endPtr++;
  }
  // else all OK
  l = result;
  return true; // OK
}

// convert binary number to long, arg 1 unchanged if no valid number found
bool SafeString::binToLong(long &l) {
  cleanUp();
  if (len == 0) {
    return false; // not found
  }
  char* endPtr;
  long result = strtol(buffer, &endPtr, 2);
  if (result == LONG_MAX) {
    return false;
  }
  if (result == LONG_MIN) {
    return false;
  }
  // check endPtr to see if number valid 5a is invalid,  5. is valid
  if (endPtr == buffer)  { // no numbers found at all
    return false;
  } // else
  // else check for trailing white space
  while (*endPtr != '\0') {
    if (!isspace(*endPtr)) { // number terminated by white space
      return false;
    }
    endPtr++;
  }
  // else all OK
  l = result;
  return true; // OK
}

// convert octal number to long, arg 1 unchanged if no valid number found
bool SafeString::octToLong(long &l) {
  cleanUp();
  if (len == 0) {
    return false; // not found
  }
  char* endPtr;
  long result = strtol(buffer, &endPtr, 8);
  if (result == LONG_MAX) {
    return false;
  }
  if (result == LONG_MIN) {
    return false;
  }
  // check endPtr to see if number valid 5a is invalid,  5. is valid
  if (endPtr == buffer)  { // no numbers found at all
    return false;
  } // else
  // else check for trailing white space
  while (*endPtr != '\0') {
    if (!isspace(*endPtr)) { // number terminated by white space
      return false;
    }
    endPtr++;
  }
  // else all OK
  l = result;
  return true; // OK
}

// convert hex number to long, arg 1 unchanged if no valid number found
bool SafeString::hexToLong(long &l) {
  cleanUp();
  if (len == 0) {
    return false; // not found
  }
  char* endPtr;
  long result = strtol(buffer, &endPtr, 16); //
  if (result == LONG_MAX) {
    return false;
  }
  if (result == LONG_MIN) {
    return false;
  }
  // check endPtr to see if number valid 5a is invalid,  5. is valid
  if (endPtr == buffer)  { // no numbers found at all
    return false;
  } // else
  // else check for trailing white space
  while (*endPtr != '\0') {
    if (!isspace(*endPtr)) { // number terminated by white space
      return false;
    }
    endPtr++;
  }
  // else all OK
  l = result;
  return true; // OK
}

// convert float number , arg f unchanged if no valid number found
bool SafeString::toFloat(float  &f) {
  cleanUp();
  double d;
  if (toDouble(d)) {
    f = (float)d;
    return true;
  } // else
  return false;
}

// convert double number , arg d unchanged if no valid number found
bool SafeString::toDouble(double  &d) {
  cleanUp();
  if (len == 0) {
    return false; // not found
  }
  char* endPtr;
  double result = strtod(buffer, &endPtr); // handles 123 (use 0 for 0xAF and 037 (octal))
  // check endPtr to see if number valid 5a is invalid,  5. is valid
  if (endPtr == buffer)  { // no numbers found at all
    return false;
  } // else
  // else check for trailing white space
  while (*endPtr != '\0') {
    if (!isspace(*endPtr)) { // number terminated by white space
      return false;
    }
    endPtr++;
  }
  d = result;
  return true; // OK
}

/** end of Number Parsing / Conversion  methods *****************/


/*******************************************************/
/**  Tokenizing methods,  stoken(), nextToken()        */
/**   Differences between stoken() and nextToken
       stoken() leaves the SafeString unchanged, nextToken() removes the token (and leading delimiters) from the SafeString giving space to add more input
       In stoken() the end of the SafeString is always a delimiter, i.e. the last token is returned even if it is not followed by one of the delimiters
       In nextToken() the end of the SafeString is NOT a delimiter, i.e. if the last token is not terminated it is left in the SafeString
       this allows partial tokens to be read from a Stream and kept until the full token and delimiter is read
*/
/*******************************************************/
/*
      stoken  -- The SafeString itself is not changed
      stoken breaks into the SafeString into tokens using chars in delimiters string and the end of the SafeString as delimiters.
      Any leading delimiters are first stepped over and then the delimited token is return in the token argument (less the delimiter).
      The token argument is always cleared at the start of the stoken().

      params
      token - the SafeString to return the token in, it is cleared if no delimited token found or if there are errors
              The found delimited token (less the delimiter) is returned in the token SafeString argument if there is capacity.
              The token's capacity should be >= this SafeString's capacity incase the entire SafeString needs to be returned.
              If the token's capacity is < the next token, then token is returned empty and an error messages printed if debug is enabled.
              In this case the return (nextIndex) is still updated.
      fromIndex -- where to start the search from  0 to length() is valid for fromIndex
      delimiters - the characters that any one of which can delimit a token. The end of the SafeString is always a delimiter.
      returnEmptyFields -- default false, if true only skip one leading delimiter after each call. If the fromIndex is 0 and there is a delimiter at the beginning of the SafeString, an empty token will be returned  
      useAsDelimiters - default true, if false then token consists only of chars in the delimiters and any other char terminates the token

      return -- nextIndex, the next index in this SafeString after the end of the token just found.
               Use this as the fromIndex for the next call
               NOTE: if there are no delimiters then length() is returned and the whole SafeString returned in token if the SafeString token argument is large enough
               If the token's capacity is < the next token, the token returned is empty and an error messages printed if debug is enabled.
               In this case the returned nextIndex is still updated to end of the token just found so that that the program will not be stuck in an infinite loop testing for nextInded >= length()
               while being consistent with the SafeString's all or nothing insertion rule

      Input argument errors return length()+1
      If the returned, nextIndex is <= length() and the returned token is empty, then the SafeString token argument did not have the capacity to hold the next token.
**/

size_t SafeString::stoken(SafeString & token, size_t fromIndex, const char delimiter, bool returnEmptyFields, bool useAsDelimiters) {
  if (!delimiter) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("stoken"));
      debugPtr->print(F(" was passed a '\\0' delimiter"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }  	  
  return stokenInternal(token, fromIndex, NULL, delimiter, returnEmptyFields,  useAsDelimiters);
}

size_t SafeString::stoken(SafeString &token, size_t fromIndex, SafeString &delimiters, bool returnEmptyFields, bool useAsDelimiters) {
  delimiters.cleanUp();
  return stoken(token, fromIndex, delimiters.buffer, returnEmptyFields, useAsDelimiters); // calls cleanUp()
}

size_t SafeString::stoken(SafeString &token, size_t fromIndex, const char* delimiters, bool returnEmptyFields, bool useAsDelimiters) {
  if (!delimiters) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("stoken"));
      debugPtr->print(F(" was passed a NULL pointer for delimiters"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }
  if (*delimiters == '\0') {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("stoken"));
      debugPtr->print(F(" was passed a empty list of delimiters"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }
  return stokenInternal(token, fromIndex, delimiters, '\0', returnEmptyFields,  useAsDelimiters);
}
	
size_t SafeString::stokenInternal(SafeString &token, size_t fromIndex, const char* delimitersIn, char delimiterIn, bool returnEmptyFields, bool useAsDelimiters) {
  cleanUp();
  token.clear(); // no need to clean up token
  char charDelim[2];
  charDelim[0] = delimiterIn;
  charDelim[1] = '\0';
  const char *delimiters = delimitersIn;
  if (delimiters == NULL) {
  	delimiters = charDelim;
  }
  if (fromIndex == len) {
    return len; // reached end of input return empty token and len
    // this is a common case when stepping over delimiters
  }
  // else invalid fromIndex
  if (fromIndex > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("stoken"));
      debugPtr->print(F(" fromIndex ")); debugPtr->print(fromIndex); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }
  size_t count = 0;
  // skip leading delimiters  (prior to V2.0.2 leading delimiters not skipped)
  if (useAsDelimiters) {
    count = strspn(buffer + fromIndex, delimiters); // count chars ONLY in delimiters
  } else {
    count = strcspn(buffer + fromIndex, delimiters); // count chars NOT in delimiters
  }
  if (returnEmptyFields) {
    // only step over one
    if (count > 0) {
      if (fromIndex == 0) {
        return 1; // leading empty token
      } // else skip over only one of the last delimiters
      count = 1;
    } 
  }
  fromIndex += count;
  if (fromIndex == len) {
    return len; // reached end of input return empty token and len
  }
  // find length of token
  if (useAsDelimiters) {
    count = strcspn(buffer + fromIndex, delimiters); // count chars NOT in delimiters, i.e. the token
  } else {
    count = strspn(buffer + fromIndex, delimiters); // count chars ONLY in delimiters, i.e. the delimiters are the token
  }
  if (count == 0) {
    return fromIndex; // not found and empty token returned
  }
  if (count > token._capacity) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("stoken"));
      debugPtr->print(F(" token SafeString ")); token.outputName(); debugPtr->print(F(" needs capacity of "));
      debugPtr->print(count); debugPtr->print(F(" for token '")); debugPtr->write((uint8_t*)(buffer + fromIndex), count); debugPtr->print('\'');
      token.debugInternalResultMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return fromIndex + count;  // used to be len + 1; prior to V2.0.3
  }
  // else get substring
  substring(token, fromIndex, fromIndex + count);
  return fromIndex + count; // may == len
}
/** end of stoken methods *******************/

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
                If the return is true, but the returned token is empty, then the SafeString token argument did not have the capacity to hold the next token.
                In this case to next token is still removed from the SafeString so that the program will not be stuck in an infinite loop calling nextToken()
                while being consistent with the SafeString's all or nothing insertion rule
**/
bool SafeString::nextToken(SafeString& token, const char delimiter) {
  if (!delimiter) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("nextToken"));
      debugPtr->print(F(" was passed a '\\0' delimiter"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }  	
  return nextTokenInternal(token, NULL, delimiter);
}	

bool SafeString::nextToken(SafeString& token, SafeString &delimiters) {
  delimiters.cleanUp();
  return nextToken(token, delimiters.buffer); // calls cleanUp()
}

bool SafeString::nextToken(SafeString& token, const char* delimiters) {
  if (!delimiters) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("nextToken"));
      debugPtr->print(F(" was passed a NULL pointer for delimiters"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }
  if (*delimiters == '\0') {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("nextToken"));
      debugPtr->print(F(" was passed a empty list of delimiters"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }
  return nextTokenInternal(token, delimiters, '\0' );
}

bool SafeString::nextTokenInternal(SafeString& token, const char* delimitersIn, const char delimiterIn) {
  cleanUp();
  token.clear();
  if (isEmpty()) {
    return false;
  }
  char charDelim[2];
  charDelim[0] = delimiterIn;
  charDelim[1] = '\0';
  const char *delimiters = delimitersIn;
  if (delimiters == NULL) {
  	delimiters = charDelim;
  }
  
  // remove leading delimiters
  size_t delim_count = 0;
  // skip leading delimiters  (prior to V2.0.2 leading delimiters not skipped)
  delim_count = strspn(buffer, delimiters); // count char ONLY in delimiters
  remove(0, delim_count); // remove leading delimiters
  // check for token
  // find first char not in delimiters
  size_t token_count = 0;
  token_count = strcspn(buffer, delimiters);
  if ((token_count) == len) {
    // no trailing delimiter
    return false; // delimited token not found
  }
  if (token_count > token._capacity) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("nextToken"));
      debugPtr->print(F(" token SafeString ")); token.outputName(); debugPtr->print(F(" needs capacity of ")); debugPtr->print(token_count);
      debugPtr->print(F(" for token '")); debugPtr->write((uint8_t*)(buffer), token_count); debugPtr->print('\'');
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    remove(0, token_count); // remove token but not following delimiters
    return true; // but token is empty => error // prior to V2.0.3 returned false
  }

  substring(token, 0, token_count); // do not return trailing delimiters.
  remove(0, token_count); // remove token but not following delimiters
  return true;
}
/** end of nextToken methods *******************/
/**** end of   Tokenizing methods,  stoken(), nextToken()  ****************/


/****************************************************************/
/**  ReadFrom from SafeString, writeTo SafeString               */
/****************************************************************/
/*
   readFrom(SafeString & sfInput, size_t startIdx = 0)  reads from the SafeString starting at startIdx into the SafeString calling this method
   params
     sfInput - the SafeString to read from
     startIdx - where to start reading from, defaults to 0,
                if startIdx >= sfInput.length(), nothing read and sfInput.length() returned

   returns new startIdx
   read stops when the end if the sfInput is reached or the calling SafeString is full
   Note: if the SafeString is already full, then nothing will be read and startIdx will be returned
**/
size_t SafeString::readFrom(SafeString & input, size_t startIdx) {
  input.cleanUp();
  cleanUp();
  if (startIdx > input.len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("readFrom"));
      debugPtr->print(F(" startIdx:"));  debugPtr->print(startIdx);
      debugPtr->print(F(" > input.length():"));  debugPtr->print(input.len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Input arg was '")); debugPtr->print(input); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif
    return input.len; // nothing to read
  }
  if (len == _capacity) {
    // not room to read
    return startIdx; // no change
  }
  size_t readLen = input.len - startIdx;
  if (readLen > (_capacity - len)) {
    readLen = (_capacity - len); // limit to space available
  }
  memmove(buffer + len, input.buffer + startIdx, readLen);
  len += readLen;
  buffer[len] = '\0';
  return (startIdx + readLen);
}

/*
   writeTo(SafeString & output, size_t startIdx = 0)  writes from SafeString, starting at startIdx to output
   params
     output - the SafeString to write to
     startIdx - where to start writing from calling SafeString, defaults to 0,
                if startIdx >= length(), nothing written and length() returned

   returns new startIdx for next write
   write stops when the end if the calling SafeString is reached or the output is full
   Note: if the sfOutput is already full, then nothing will be written and startIdx will be returned
**/
size_t SafeString::writeTo(SafeString & output, size_t startIdx) {
  output.cleanUp();
  cleanUp();
  if (startIdx > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("writeTo"));
      debugPtr->print(F(" startIdx:"));  debugPtr->print(startIdx);
      debugPtr->print(F(" > length():"));  debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Output arg was '")); debugPtr->print(output); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif
    return len; // nothing written
  }
  if (output.len == output._capacity) {
    // not room to write
    return startIdx; // no change
  }
  size_t writeLen = len - startIdx;
  if (writeLen > (output._capacity - output.len)) {
    writeLen = (output._capacity - output.len); // limit to space available
  }
  memmove(output.buffer + output.len, buffer + startIdx, writeLen);
  output.len += writeLen;
  output.buffer[output.len] = '\0';
  return (startIdx + writeLen);
}

/** end of  ReadFrom from SafeString, writeTo SafeString ************/

/****************************************************************/
/**  NON-Blocking reads from Stream,  read() and readUntil()    */
/****************************************************************/

/* read(Stream& input) --  reads from the Stream (if chars available) into the SafeString
      The is NON-BLOCKING and returns immediately if nothing available to be read
      returns true if something added to string else false
      Note: if the SafeString is already full, then nothing will be read and false will be returned
*/
bool SafeString::read(Stream& input) {
  cleanUp();
  bool rtn = false;
  noCharsRead = 0;
  while (input.available() && (len < _capacity)) {
    int c = input.read();
    noCharsRead++;
    if (c != '\0') { // skip any nulls read
      concat((char)c);
      rtn = true;
    }
  }
  return rtn; // true if something added to string
}

/*
  readUntil( ) ---  returns true if delimiter found or string filled, found else false
      NON-blocking readUntil of Stream, if chars are available
      returns true if delimiter found or string filled, found else false
      if a delimiter is found it is included in the return

      params
        input - the Stream object to read from
        delimiters - string of valid delimieters
      return true if SaftString is full or a delimiter is read, else false
      Any delimiter read is returned.  Only at most one delimiter is added per call
     Multiple sucessive delimiters require multiple calls to read them
**/
bool SafeString::readUntil(Stream& input, const char delimiter) {
  if (!delimiter) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("readUntil"));
      debugPtr->print(F(" was passed a '\\0' delimiter"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }  	
  return readUntilInternal(input, NULL, delimiter);
}

bool SafeString::readUntil(Stream& input, SafeString &delimiters) {
  delimiters.cleanUp();
  return readUntil(input, delimiters.buffer); // calls cleanUp()
}

bool SafeString::readUntil(Stream& input, const char* delimiters) {
  if (!delimiters) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("readUntil"));
      debugPtr->print(F(" was passed a NULL pointer for delimiters"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false; // no match
  }
  if (*delimiters == '\0') {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("readUntil"));
      debugPtr->print(F(" was passed a empty list of delimiters"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false; // no match
  }
  return readUntilInternal(input, delimiters, '\0');
}

	
bool SafeString::readUntilInternal(Stream& input, const char* delimitersIn, const char delimiterIn) {
  cleanUp();
  char charDelim[2];
  charDelim[0] = delimiterIn;
  charDelim[1] = '\0';
  const char *delimiters = delimitersIn;
  if (delimiters == NULL) {
  	delimiters = charDelim;
  }  
  noCharsRead = 0;
  while (input.available() && (len < (capacity()))) {
    int c = input.read();
    noCharsRead++;
    if (c == '\0') {
      continue; // skip nulls
    }
    concat((char)c); // add char may be delimiter
    if (strchr(delimiters, c) != NULL) {
      return true; // found delimiter return true
    }
  }
  if (isFull()) {
    return true;
  } // else
  return false;
}


/*
      NON-blocking readUntilToken
      returns true if a delimited token is found, else false
      ONLY delimited tokens of length less than this SafeString's capacity will return true with a non-empty token.
      Streams of chars that overflow this SafeString's capacity are ignored and return an empty token on the next delimiter or timeout
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
        echoInput - defaults to true to echo the chars read
        timeout_mS - defaults to never timeout, pass a non-zero mS to autoterminate the last token if no new chars received for that time.

      returns true if a delimited series of chars found that fit in this SafeString else false
      If this SafeString OR the SaftString & token argument is too small to hold the result, the returned token is returned empty
      The delimiter is NOT included in the SaftString & token return. It will the first char of the this SafeString when readUntilToken returns true
 **/
 
bool SafeString::readUntilToken(Stream & input, SafeString& token, const char delimiter, bool & skipToDelimiter, uint8_t echoInput, unsigned long timeout_mS) {
   if (!delimiter) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("readUntilToken"));
      debugPtr->print(F(" was passed a '\\0' delimiter"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return len + 1;
  }  	
  return readUntilTokenInternal(input,token,NULL,delimiter,skipToDelimiter,echoInput, timeout_mS);
}

bool SafeString::readUntilToken(Stream & input, SafeString& token, SafeString& delimiters, bool & skipToDelimiter, uint8_t echoInput, unsigned long timeout_mS) {
  delimiters.cleanUp();
  return readUntilToken(input, token, delimiters.buffer, skipToDelimiter, echoInput, timeout_mS); // calls cleanUp()
}

bool SafeString::readUntilToken(Stream & input, SafeString& token, const char* delimiters, bool & skipToDelimiter, uint8_t echoInput, unsigned long timeout_mS) {
  if (!delimiters) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("readUntilToken"));
      debugPtr->print(F(" was passed a NULL pointer for delimiters"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false; // no match
  }
  if (*delimiters == '\0') {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("readUntilToken"));
      debugPtr->print(F(" was passed a empty list of delimiters"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false; // no match
  }
  return readUntilTokenInternal(input,token,delimiters,'\0',skipToDelimiter,echoInput, timeout_mS);
}

bool SafeString::readUntilTokenInternal(Stream & input, SafeString& token, const char* delimitersIn, const char delimiterIn, bool & skipToDelimiter, uint8_t echoInput, unsigned long timeout_mS) {
   token.clear(); // always
	if ((echoInput != 0) && (echoInput != 1) && (timeout_mS == 0)) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("readUntilToken"));
      debugPtr->println(F(" was passed timeout for the echo setting, method format is:-"));
      debugPtr->println(F("   readTokenUntil(stream,token,delimiters,skipToDelimiter,echoOn,timeout_mS)"));
    }
#endif // SSTRING_DEBUG
    timeout_mS = echoInput; // swap to timeout
    echoInput = true; // use default
  }	  
  
  if (capacity() < 2) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("readUntilToken"));
      debugPtr->println(F(" SafeString needs capacity of at least 2, one char + one delimiter"));
    }
#endif // SSTRING_DEBUG
    return false;
  }	  

  cleanUp();
  char charDelim[2];
  charDelim[0] = delimiterIn;
  charDelim[1] = '\0';
  const char *delimiters = delimitersIn;
  if (delimiters == NULL) {
  	delimiters = charDelim;
  }  
  // remove leading delimiters 
  size_t delim_count = 0;
  // skip leading delimiters  (prior to V2.0.2 leading delimiters not skipped)
  delim_count = strspn(buffer, delimiters); // count char ONLY in delimiters
  remove(0, delim_count); // remove leading delimiters

  // NOTE: this method's contract says you can set skipToDelimiter true at any time
  // so here stop reading on first delimiter and process results
  // loop here until either isFill() OR no more chars avail OR found delimiter
  // read at most capacity() each time if skipping 
  // this prevent infinite loop if using SafeStringStream with echoOn and rx buffer overflow has dropped all the delimiters
  //size_t charRead = 0;
  noCharsRead = 0;
  while (input.available() && (len < capacity()) && (noCharsRead < capacity()) ) {
    int c = input.read();
    noCharsRead++;
    if (c == '\0') {
      continue; // skip nulls  // don't update timer on null chars
    }
    if (echoInput) {
      input.print((char) c);
    }
    if (timeout_mS > 0) {
      // got new char reset timeout
      timeoutRunning = true;
      timeoutStart_mS = millis(); // start timer
    }
    if (!skipToDelimiter) {
    	concat((char)c); // add char may be delimiter
    }
    if (strchr(delimiters, c) != NULL) {
      if (skipToDelimiter) {
        // if skipToDelimiter then started with empty SafeString
        skipToDelimiter = false; // found next delimiter not added above because skipToDelimiter
      	clear(); 
        concat((char)c); // is a delimiter
        return true; // empty token
      } else {
      	// added c above
      	break; // process this token
      }
    }
  }
  // here either isFill() OR no more chars avail OR found delimiter
  // skipToDelimiter may still be true here if no delimiter found above
  if (nextToken(token, delimiters)) {  // removes leading delimiters if any
     // returns true only if have found delimited token, returns false if full and no delimiter
    // IF found delimited token, delimiter was add just now and so timer reset
    // skipToDelimiter is false here since found delimiter
    return true; // not full and not timeout and have new token
  }
  
  // else
  if (isFull()) { // note if full here then > max token as capacity needs to allow for one delimiter
      // SafeString is full of chars but no delimiter
      // discard the chars and skip input until get next delimiter
      clear();  // not full now
      skipToDelimiter = true;
      return false; // will do timeout check next call.  No token found return false
   }
   
   // else no token found AND not full AND last char NOT a delimiter
  if (timeoutRunning) { // here not full because called nextToken OR checked for full
    if ((millis() - timeoutStart_mS) > timeout_mS) {
      // no new chars for timeout add terminator
      timeoutRunning = false;
      // put in delimiter   
      if ((len != 0) || skipToDelimiter) { // have something to delimit or had somthing, else just stop timer
       concat(delimiters[0]);   // certainly NOT full from above  	 
       if (echoInput) {
         input.print(delimiters[0]);
       }
       if (debugPtr) {
         debugPtr->println(); debugPtr->print("!! ");outputName();
         debugPtr->println(" -- Input timed out.");
       }
       if (skipToDelimiter) {
       	  skipToDelimiter = false;
      	  return true;
       } // else pick up token
       nextToken(token, delimiters); // collect this token just delimited, this will clear input
       return true;
     }
    }
  }
  return false; // no token 
}

size_t SafeString::getLastReadCount() {
	return noCharsRead;
}

/** end of NON-Blocking reads from Stream,  read() and readUntil() *******************/

/*******************************************************/
/** Private methods for Debug and Error support           */
/*******************************************************/
void SafeString::debugInternal(bool verbose) const {
  if (debugPtr) {
    if (name) {
      debugPtr->print(' ');
      debugPtr->print(name);
    } // else no name set
    debugPtr->print(F(" cap:")); debugPtr->print(_capacity);
    debugPtr->print(F(" len:")); debugPtr->print(len);
    if (verbose) { // print SafeString current contents
      debugPtr->print(F(" '")); debugPtr->print(buffer); debugPtr->print('\'');
    }
    debugPtr->println();
  }
}

// this internal msg debug does not add line indent if not fullDebug
// always need to add debugPtr->println() at end of debug output
void SafeString::debugInternalMsg(bool verbose) const {
#ifdef SSTRING_DEBUG
  if (debugPtr) {
    if (verbose) {
      debugPtr->println(); // terminate first line
      debugPtr->print(F("       "));
    } else {
      debugPtr->print(F(" --- ")); // not full debug
    }
    if (name) {
      debugPtr->print(' ');
      debugPtr->print(name);
    } // else no name set
    debugPtr->print(F(" cap:")); debugPtr->print(_capacity);
    debugPtr->print(F(" len:")); debugPtr->print(len);
    if (verbose) { // print SafeString current contents
      debugPtr->print(F(" '")); debugPtr->print(buffer); debugPtr->print('\'');
    }
    debugPtr->println();
  }
#endif // SSTRING_DEBUG
}

void SafeString::debugInternalResultMsg(bool verbose) const {
#ifdef SSTRING_DEBUG
  if (verbose) {
    debugPtr->println(); // terminate first line
    debugPtr->print(F("       "));
  } else {
    debugPtr->print(F(" --- ")); // not full debug
  }
  if (name) {
    debugPtr->print(' ');
    debugPtr->print(name);
  } // else no name set
  debugPtr->print(F(" cap:")); debugPtr->print(_capacity);
  debugPtr->print(F(" len:")); debugPtr->print(len);
  if (verbose) { // print SafeString current contents
    debugPtr->print(F(" '")); debugPtr->print(buffer); debugPtr->print('\'');
  }
  debugPtr->println();
#endif // SSTRING_DEBUG
}

void SafeString::outputName() const {
  if (debugPtr) {
    debugPtr->print(' ');
    if (name) {
      debugPtr->print(name);
    } else {
      debugPtr->print(F("SafeString"));
    }
  }
}

void SafeString::capError(const __FlashStringHelper * methodName, size_t neededCap, const char* cstr, const __FlashStringHelper * pstr, char c, size_t length) const {
#ifdef SSTRING_DEBUG
  if (debugPtr) {
    errorMethod(methodName);
    debugPtr->print(F(" needs capacity of "));
    debugPtr->print(neededCap);
    if (length != 0) {
      debugPtr->print(" for the first "); debugPtr->print(length); debugPtr->print(" chars of the input.");
    }
    if (!fullDebug) {
      debugInternalMsg(fullDebug);
    } else {
      debugPtr->println();
      debugPtr->print(F("       ")); // indent next line
      if (cstr || pstr || (c != '\0')) {
        debugPtr->print(F(" Input arg was '"));
        if (cstr) {
          debugPtr->print(cstr);
        } else if (pstr) { // for nl
          debugPtr->print(pstr);
        } else if (c != '\0') {
          debugPtr->print(c);
        }
      }
      debugPtr->print('\'');
      debugInternalMsg(fullDebug);
    }
  }
#endif
}

void SafeString::errorMethod(const __FlashStringHelper * methodName) const {
#ifdef SSTRING_DEBUG
  debugPtr->print(F("Error:"));
  outputName();
  debugPtr->print('.');
  debugPtr->print(methodName);
  debugPtr->print(F("()"));
#endif
}

void SafeString::warningMethod(const __FlashStringHelper * methodName) const {
#ifdef SSTRING_DEBUG
  debugPtr->print(F("Warning:"));
  outputName();
  debugPtr->print('.');
  debugPtr->print(methodName);
  debugPtr->print(F("()"));
#endif
}

void SafeString::outputFromIndexIfFullDebug(size_t fromIndex) const {
#ifdef SSTRING_DEBUG
  if (fullDebug) {
    debugPtr->println(); debugPtr->print(F("       "));
    debugPtr->print(F(" fromIndex is ")); debugPtr->print(fromIndex);
  }
#endif
}

void SafeString::concatErr() const {
#ifdef SSTRING_DEBUG
  errorMethod(F("concat"));
#endif
}

void SafeString::printlnErr() const {
#ifdef SSTRING_DEBUG
  errorMethod(F("println"));
#endif
}

void SafeString::prefixErr() const {
#ifdef SSTRING_DEBUG
  errorMethod(F("prefix"));
#endif
}
/*****************  end of private internal debug support methods *************************/

