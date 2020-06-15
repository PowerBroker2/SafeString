/*
  SafeString.cpp static memory SafeString library modified by
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

// to remove all the error debug outputs, comment out
// #define SSTRING_DEBUG
// in SafeString.h file
// this saves program space used by the error messages and the ram used by the SafeString object names
//
// usually just leave as is and use SafeString::setOutput(..) to control the debug output
// there will be no debug output is SafeString::setOutput(..) has not been called from your sketch
// SafeString.debug() is always available regardless of the SSTRING_DEBUG define setting

 char SafeString::nullBufferSafeStringBuffer[1] = {'\0'}; // use if buf arg is NULL
 char SafeString::emptyDebugRtnBuffer[1] = {'\0'}; // use for debug() returns
 Print* SafeString::debugPtr = NULL; // nowhere to send the debug output yet
 Print* SafeString::currentOutput = &SafeString::emptyPrint; // nowhere to send Output to yet
 SafeString::noDebugPrint SafeString::emptyPrint;
 SafeString::DebugPrint SafeString::Output;
 bool SafeString::fullDebug = true; // output current contents of SafeString and input arg

/*********************************************/
/*  Constructor                             */
/*********************************************/


// This constructor overwrites any data already in buf with cstr
SafeString::SafeString(size_t maxLen, char *buf, const char* cstr, const char* _name) {
  name = _name; // save name
  if (buf != NULL) {
    buffer = buf;
    if (maxLen > 0) {
      _capacity = maxLen - 1;
    } else {
      _capacity = 0;
    }
    len = 0;
    buffer[0] = '\0';
  } else {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      debugPtr->print(F("Error: SafeString("));
      outputName(); debugPtr->print(F(", ...) was passed a NULL pointer for its buffer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    buffer = nullBufferSafeStringBuffer;
    _capacity = 0;
    len = 0;
    buffer[0] = '\0';
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
  if (cstrLen > _capacity) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      debugPtr->print(F("Error: SafeString("));
      outputName(); debugPtr->print(F(", ...) needs capacity of ")); debugPtr->print(cstrLen);  debugPtr->print(F(" for initial value."));
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" Initial value arg was '")); debugPtr->print(cstr); debugPtr->print('\'');
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    len = 0;
    buffer[0] = '\0';
    return;
  }
  memcpy(buffer, cstr, cstrLen);
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
}

SafeString & SafeString::clear(void) {
  len = 0;
  buffer[len] = '\0';
  return *this;
}

// call setOutput( ) to turn on Error msgs and debug( ) output for all SafeStrings
// This also sets output for the debug( ) method
// verbose is an optional argument, if missing defaults to true, use false for compact error messages or call setVerbose(false)
void SafeString::setOutput(Print& debugOut, bool verbose) {
  debugPtr = &debugOut;
  fullDebug = verbose;  // the verbose argument is optional, if missing fullDebug is true
  currentOutput = debugPtr;
}


// call this to turn all debugging OFF, both error messages AND debug( ) method output
void SafeString::turnOutputOff() {
  debugPtr = NULL;
  currentOutput = &emptyPrint;
}

// use this to control error messages verbose output
// turn verbose error msgs on/off.  setOutput( ) sets verbose to true 
// does not effect debug() methods which have their own optional verbose argument
void SafeString::setVerbose(bool verbose) {
  fullDebug = verbose;
}

/*******************  debug methods *******************************/
// these methods print out info on this SafeString object, iff setOutput( ) has been called
// setVerbose( ) does NOT effect these methods which have their own verbose argument
// Each of these debug( ) methods defaults to outputing the string contents.  Set the optional verbose argument to false to suppress outputing string contents
// NOTE!! all these debug methods return a pointer to an empty string. 
// This is so that if you add .debug() to Serial.println(str);  i.e. Serial.println(str.debug()) will work as expected
// Each of these debug() methods returns a pointer to an empty char[] 
const char* SafeString::debug(bool verbose) {
  debug((const char*) NULL, verbose);
  emptyDebugRtnBuffer[0] = '\0'; // the last return was changed
  return emptyDebugRtnBuffer;
}

const char* SafeString::debug(const __FlashStringHelper * pstr, bool verbose) { // verbose optional defaults to true
  if (debugPtr) {
    if (pstr) {
      debugPtr->print(pstr);
    } else {
      debugPtr->print(F("SafeString"));
    }
    debugInternal(verbose);
  }
  emptyDebugRtnBuffer[0] = '\0'; // the last return was changed
  return emptyDebugRtnBuffer;
}

// prints SafeString to debugPtr
const char* SafeString::debug(const char *title, bool verbose) { // verbose optional defaults to true
  if (debugPtr) {
    if (title) {
      debugPtr->print(title);
    } else {
      debugPtr->print(F("SafeString"));
    }
    debugInternal(verbose);
  }
  emptyDebugRtnBuffer[0] = '\0'; // the last return was changed
  return emptyDebugRtnBuffer;
}

const char* SafeString::debug(const SafeString &stitle, bool verbose) { // verbose optional defaults to true
  if (debugPtr) {
    if (stitle.len != 0) {
      debugPtr->print(stitle);
    } else {
      debugPtr->print(F("SafeString"));
    }
    debugInternal(verbose);
  }
  emptyDebugRtnBuffer[0] = '\0'; // the last return was changed
  return emptyDebugRtnBuffer;
}

/*****************  end public debug methods *************************/

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

size_t SafeString::write(uint8_t b) {
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

size_t SafeString::printTo(Print& p) const {
  return p.print(buffer);
}

size_t SafeString::println() {
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

SafeString & SafeString::nl() {
  println();
  return *this;
}

// used by println
SafeString & SafeString::concatln(char c) {
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

SafeString & SafeString::concat(const char *cstr, size_t length) {
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
  memcpy(buffer + len, cstr, length);
  len = newlen;
  buffer[len] = '\0';
  return *this;
}

/***********************************
   Print methods
***********************************/

size_t SafeString::print(SafeString &str) {
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

/***********************************
   Print overrides
***********************************/

size_t SafeString::print(const char* cstr) {
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
  return print((unsigned long) b, base);
}

size_t SafeString::print(int n, int base) {
  return print((long) n, base);
}


size_t SafeString::print(unsigned int n, int base) {
  return print((unsigned long) n, base);
}

size_t SafeString::print(long num, int base) {
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
  concatln(pstr);
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
  concatln(c);
  return 3;
}

size_t SafeString::println(unsigned char b, int base) {
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  size_t n = temp.Print::print(b, base);
  concatln(temp.buffer, temp.len);
  return n + 2;
}

size_t SafeString::println(int num, int base) {
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  size_t n = temp.Print::print(num, base);
  concatln(temp.buffer, temp.len);
  return n + 2;
}

size_t SafeString::println(unsigned int num, int base) {
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  size_t n = temp.Print::print(num, base);
  concatln(temp.buffer, temp.len);
  return n + 2;
}

size_t SafeString::println(long num, int base) {
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  size_t n = temp.Print::print(num, base);
  concatln(temp.buffer, temp.len);
  return n + 2;
}

size_t SafeString::println(unsigned long num, int base) {
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  size_t n = temp.Print::print(num, base);
  concatln(temp.buffer, temp.len);
  return n + 2;
}

size_t SafeString::println(double num, int digits) {
  createSafeString(temp, 8 * sizeof(long) + 4); // null + sign + nl
  if (digits > 18) {
    digits = 18;
  }
  size_t n = temp.Print::print(num, digits);
  concatln(temp.buffer, temp.len);
  return n + 2;
}

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

SafeString & SafeString::operator = (const SafeString &rhs) {
  if (this == &rhs) {
    return *this;
  }
  clear();
  return concat(rhs.buffer);//print(rhs);
  //return *this;
}


SafeString & SafeString::operator = (const char *cstr) {
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
  clear();
  return concat(c);
  //return *this;
}

SafeString & SafeString::operator = (unsigned char c) {
  clear();
  print(c);
  return *this;
}

SafeString & SafeString::operator = (int num) {
  clear();
  print(num);
  return *this;
}

SafeString & SafeString::operator = (unsigned int num) {
  clear();
  print(num);
  return *this;
}

SafeString & SafeString::operator = (long num) {
  clear();
  print(num);
  return *this;
}

SafeString & SafeString::operator = (unsigned long num) {
  clear();
  print(num);
  return *this;
}

SafeString & SafeString::operator = (float num) {
  clear();
  print(num);
  return *this;
}

SafeString & SafeString::operator = (double num) {
  clear();
  print(num);
  return *this;
}

/*********************************************/
/*  concat / prefix                         */
/*********************************************/

/**************************************/
SafeString & SafeString::prefix(char c) {
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
    memcpy(buffer, cstr, length);
  }
  len = newlen;
  buffer[len] = '\0';
  return *this;
}

SafeString & SafeString::prefix(const __FlashStringHelper * pstr) {
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
SafeString & SafeString::prefix(const SafeString &s) {
  return prefix(s.buffer, s.len);
}

SafeString & SafeString::prefix(const char *cstr) {
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
  return prefix(temp);
}

SafeString & SafeString::prefix(int num) {
  createSafeString(temp, 2 + 3 * sizeof(int));
  temp.print(num);
  return prefix(temp);
}

SafeString & SafeString::prefix(unsigned int num) {
  createSafeString(temp, 2 + 3 * sizeof(unsigned int));
  temp.print(num);
  return prefix(temp);
}

SafeString & SafeString::prefix(long num) {
  createSafeString(temp, 2 + 3 * sizeof(long));
  temp.print(num);
  return prefix(temp);
}

SafeString & SafeString::prefix(unsigned long num) {
  createSafeString(temp, 2 + 3 * sizeof(unsigned long));
  temp.print(num);
  return prefix(temp);
}

SafeString & SafeString::prefix(float num) {
  createSafeString(temp, 22);
  temp.print(num);
  return prefix(temp);
}

SafeString & SafeString::prefix(double num) {
  createSafeString(temp, 22);
  temp.print(num);
  return prefix(temp);
}


SafeString & SafeString::concat(char c) {
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
SafeString & SafeString::concat(const SafeString &s) {
  return concat(s.buffer, s.len);
}


SafeString & SafeString::concat(const char *cstr) {
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
  return concat(temp);
}

SafeString & SafeString::concat(int num) {
  createSafeString(temp, 2 + 3 * sizeof(int));
  temp.print(num);
  return concat(temp);
}

SafeString & SafeString::concat(unsigned int num) {
  createSafeString(temp, 2 + 3 * sizeof(unsigned int));
  temp.print(num);
  return concat(temp);
}

SafeString & SafeString::concat(long num) {
  createSafeString(temp, 2 + 3 * sizeof(long));
  temp.print(num);
  return concat(temp);
}

SafeString & SafeString::concat(unsigned long num) {
  createSafeString(temp, 2 + 3 * sizeof(unsigned long));
  temp.print(num);
  return concat(temp);
}

SafeString & SafeString::concat(float num) {
  createSafeString(temp, 22);
  temp.print(num);
  return concat(temp);
}

SafeString & SafeString::concat(double num) {
  createSafeString(temp, 22);
  temp.print(num);
  return concat(temp);
}

SafeString & SafeString::concat(const __FlashStringHelper * pstr) {
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

SafeString & SafeString::concat(const __FlashStringHelper * pstr, size_t length) {
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

/*********************************************/
/*  Concatenate                              */
/*********************************************/
// string = str + 5 etc not supported because of the need to allocate/reallocat buffers

/*********************************************/
/*  Comparison                               */
/*********************************************/

int SafeString::compareTo(const SafeString &s) const {
  // do a quick check of the lengths
  if (len < s.len) {
    return -1;
  } else if (len > s.len) {
    return +1;
  } //else 
  return strcmp(buffer, s.buffer);
}

int SafeString::compareTo(const char* cstr) const {
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

bool SafeString::equals(const SafeString &s2) const {
  return ((len == s2.len) && (strcmp(buffer, s2.buffer) == 0));
}

// error if cstr is NULL
bool SafeString::equals(const char *cstr) const {
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

bool SafeString::operator<(const SafeString &rhs) const {
  return compareTo(rhs) < 0;
}

bool SafeString::operator>(const SafeString &rhs) const {
  return compareTo(rhs) > 0;
}

bool SafeString::operator<=(const SafeString &rhs) const {
  return compareTo(rhs) <= 0;
}

bool SafeString::operator>=(const SafeString &rhs) const {
  return compareTo(rhs) >= 0;
}

bool SafeString::operator <  (const char* rhs) const {
  return compareTo(rhs) < 0;
}

bool SafeString::operator >  (const char* rhs) const {
  return compareTo(rhs) > 0;
}

bool SafeString::operator <= (const char* rhs) const {
  return compareTo(rhs) <= 0;
}

bool SafeString::operator >= (const char* rhs) const {
  return compareTo(rhs) >= 0;
}

bool SafeString::equalsIgnoreCase(const char *str2) const {
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

bool SafeString::equalsIgnoreCase( const SafeString &s2 ) const {
  if (this == &s2) {
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

bool SafeString::equalsConstantTime(const SafeString &s2) const {
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

bool SafeString::startsWith( const SafeString &s2 ) const {
  if (len < s2.len) {
    return false;
  }
  return startsWith(s2, 0);
}

bool SafeString::startsWith( const char *str2) const {
  return startsWith(str2, 0);
}

bool SafeString::startsWith( const char *str2, size_t fromIndex ) const {
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

bool SafeString::startsWith( const SafeString &s2, size_t fromIndex ) const {
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


bool SafeString::startsWithIgnoreCase( const SafeString &s2 ) const {
  if (len < s2.len) {
    return false;
  }
  return startsWithIgnoreCase(s2, 0);
}

bool SafeString::startsWithIgnoreCase( const SafeString &s2, size_t fromIndex ) const {
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

bool SafeString::startsWithIgnoreCase( const char *str2 ) const {
  return startsWithIgnoreCase(str2, 0);
}

// return 0 false 1 true
bool SafeString::startsWithIgnoreCase( const char *str2, size_t fromIndex ) const {
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

bool SafeString::endsWith( const SafeString &s2 ) const {
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

bool SafeString::endsWith(const char *suffix) const {
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

bool SafeString::endsWithCharFrom(const SafeString &s2) const {
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

bool SafeString::endsWithCharFrom(const char *suffix) const {
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
  char c = buffer[len-1];
  if (strchr(suffix, c) != NULL) {
     return true;
  } // else
  return false;
}


/*********************************************/
/*  Character Access                         */
/*********************************************/

char SafeString::charAt(size_t index) const {
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


char SafeString::operator[]( size_t index ) const {
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


/**********************************************
  Search
  All indexOf return length() if not found (or length()+1 on error)
  so test with
  size_t a_idx = str.indexOf('a');
  if (a_idx >= str.length()) { // not found

  DO NOT use the test
  if (a_idx < 0) { is NEVER true,  size_t variable is ALWAYS >= 0
**********************************************/
size_t SafeString::indexOf(char c) const {
  return indexOf(c, 0);
}

size_t SafeString::indexOf( char c, size_t fromIndex ) const {
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

size_t SafeString::indexOf(const SafeString &s2) const {
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

size_t SafeString::indexOf(const SafeString &s2, size_t fromIndex) const {
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

size_t SafeString::indexOf( const char* str ) const {
  return indexOf(str, 0);
}

size_t SafeString::indexOf(const char* cstr , size_t fromIndex) const {
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

size_t SafeString::lastIndexOf( char theChar ) const {
  return lastIndexOf(theChar, len);
}

size_t SafeString::lastIndexOf(char ch, size_t fromIndex) const {
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

size_t SafeString::lastIndexOf(const SafeString &s2) const {
  if (len < s2.len) {
    return len;
  } // else len - s2.len is valid
  return lastIndexOf(s2, len - s2.len);
}

size_t SafeString::lastIndexOf( const char *cstr ) const {
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

size_t SafeString::lastIndexOf(const SafeString &s2, size_t fromIndex) const {
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

size_t SafeString::lastIndexOf(const char* cstr, size_t fromIndex) const {
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
size_t SafeString::indexOfCharFrom(const SafeString &str) const {
  return indexOfCharFrom(str.buffer, 0);
}

size_t SafeString::indexOfCharFrom(const SafeString &str, size_t fromIndex) const {
  return indexOfCharFrom(str.buffer, fromIndex);
}

size_t SafeString::indexOfCharFrom(const char* chars) const {
  return indexOfCharFrom(chars, 0);
}

size_t SafeString::indexOfCharFrom(const char* chars, size_t fromIndex) const {
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

SafeString & SafeString::substring(SafeString &result, size_t beginIdx) {
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


// can take substring of yourself  e.g. str.substring(str,3,4);
SafeString & SafeString::substring(SafeString &result, size_t beginIdx, size_t endIdx) {
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
  if (beginIdx == len) {
    return result.clear();
  }
  // here beginIdx < len AND endIdx <= len
  if (endIdx == len) {
    endIdx = len - 1;
  }

  // copy to result
  size_t copyLen = endIdx - beginIdx + 1 ; // inclusive 5,5 copies 1 char.
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


/*********************************************/
/*  Modification                             */
/*********************************************/

SafeString & SafeString::replace(char f, char r) {
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

SafeString & SafeString::replace(const SafeString& f, const SafeString& r) {
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
  char *readFrom = buffer;
  char *foundAt;
  if (diff == 0) {
    while ((foundAt = strstr(readFrom, find)) != NULL) {
      memmove(foundAt, replace, replaceLen);
      readFrom = foundAt + replaceLen; // prevents replacing the replace
    }
  } else if (diff < 0) {
    char *writeTo = buffer;
    while ((foundAt = strstr(readFrom, find)) != NULL) {
      size_t n = foundAt - readFrom;
      memmove(writeTo, readFrom, n);
      writeTo += n;
      memmove(writeTo, replace, replaceLen);
      writeTo += replaceLen;
      readFrom = foundAt + findLen; // prevents replacing the replace
      len += diff;
    }
    memmove(writeTo, readFrom, strlen(readFrom) + 1);
  } else {
    size_t newlen = len; // compute size needed for result
    while ((foundAt = strstr(readFrom, find)) != NULL) {
      readFrom = foundAt + findLen;
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
      readFrom = buffer + index + findLen;
      memmove(readFrom + diff, readFrom, len - (readFrom - buffer));
      int newLen = len + diff;
      memmove(buffer + index, replace, replaceLen);
      len = newLen;
      buffer[newLen] = 0;
      index--;
    }
    len = newlen;
    buffer[newlen] = 0;
  }
  return *this;
}

// remove from index to end of SafeString
// 0 to length() is valid for index
SafeString & SafeString::remove(size_t index) {
  return remove(index, len - index);
}

// remove count chars starting from index
// 0 to length() is valid for index
// 0 to (length()- index) is valid for count
SafeString & SafeString::remove(size_t index, size_t count) {
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
  len = len - count;
  buffer[len] = 0;
  return *this;
}

// remove the last 'count' chars
SafeString & SafeString::removeLast(size_t count) {
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
	return remove(len-count,count);  
}

// keep last 'count' number of chars and remove those in front of them
SafeString & SafeString::keepLast(size_t count) {
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
	return remove(0,len-count);  
}	


SafeString & SafeString::toLowerCase(void) {
  for (char *p = buffer; *p; p++) {
    *p = tolower(*p);
  }
  return *this;
}

SafeString & SafeString::toUpperCase(void) {
  for (char *p = buffer; *p; p++) {
    *p = toupper(*p);
  }
  return *this;
}

// trims front and back
SafeString & SafeString::trim(void) {
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

/*********************************************/
/*  Parsing / Conversion                     */
/*********************************************/

// long 1 unchanged if no valid number found
bool SafeString::toLong(long &l) const {
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
  	if (!isSpace(*endPtr)) { // number terminated by white space
      return false;
    }
    endPtr++;
  }
  // else all OK
  l = result;
  return true; // OK
}

bool SafeString::toFloat(float  &f) const {
  double d;
  if (toDouble(d)) {
    f = (float)d;
    return true;
  } // else
  return false;
}

bool SafeString::toDouble(double  &d) const {
  if (len == 0) {
    return false; // not found
  }
  char* endPtr;
  double result = strtod(buffer, &endPtr); // handles 123 (use 0 for 0xAF and 037 (octal))
  // check endPtr to see if number valid 5a is invalid,  5. is valid
  if (endPtr == buffer)  { // no numbers found at all
    return false;
  } // else
  if ( (*endPtr == '\0') || // number at end of string
       isSpace(*endPtr) ) { // number terminated by space
    // 5. is OK but 5.5.3 is not ends on 5.5. second do
    d = result;
    return true; // OK
  }
  // else
  return false;
}

/*********************************************/
/*  Tokenizing                               */
/*********************************************/

size_t SafeString::stoken(SafeString &token, size_t fromIndex, const SafeString &delimiters, bool useAsDelimiters) {
  return stoken(token, fromIndex, delimiters.buffer, useAsDelimiters);
}

size_t SafeString::stoken(SafeString &token, size_t fromIndex, const char* delimiters, bool useAsDelimiters) {
  token.clear();
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
  if (useAsDelimiters) {
    count = strcspn(buffer + fromIndex, delimiters);
  } else {
    count = strspn(buffer + fromIndex, delimiters); // count char ONLY in delimiters
  }
  if (count == 0) {
    return fromIndex; // not found and no token returned
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
    return len + 1;
  }
  // else get substring
  substring(token, fromIndex, fromIndex + count - 1);
  return fromIndex + count; // may == len
}


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
bool SafeString::nextToken(SafeString& token, SafeString &delimiters) {
	return nextToken(token,delimiters.buffer);
}

bool SafeString::nextToken(SafeString& token, char* delimiters) {
	if (isEmpty()) {
		token.clear();
		return false;
	}
	
	if (token._capacity < _capacity) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("nextToken"));
      debugPtr->print(F(" token SafeString ")); token.outputName(); debugPtr->print(F(" needs capacity of ")); debugPtr->print(_capacity);
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return false;
  }
  
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
  	  
	token.clear();
	// remove leading delimiters	
  size_t delimIdx = stoken(token, 0, delimiters, false); 
  remove(0, delimIdx);

   // check for token
  size_t index = stoken(token, 0, delimiters); 
  if (token.isEmpty()) { // no token, just leading delimiters
    return false;
  }
  
  if (index == len ) { // no delimiter
  	// keep input
    token.clear();
    if (isFull()) {
      clear(); // token overflow
    }
    return false;
  }
  // have token since index != input.length() must have at least one delimiter
  delimIdx = stoken(token, index, delimiters, false); // check for following delimiters
  substring(token, 0, index - 1); // skip delimiters
  remove(0, delimIdx); // remove token and all following delimiters
  // here have found token followed by at least 1 delimiter
  return true;
}

/**
   size_t readBuffer(char* buffer, size_t fromBufferIndex)
   Fills SafeString with chars from character array
   If buffer[fromBufferIndex] == '\0' OR SafeString was already full, fromIndex is returned unchanged.
   NOTE: this method is NOT robust invalid fromBufferIndex can cause the program to crash

   return -- next index to read

   params
   buffer -- null terminated character array to read from
   fromIndex -- where to start reading from
   Note: if string is already full then nothing will be read and fromIndex will be returned
*/
size_t SafeString::readBuffer(char* buffer, size_t fromIndex) {
	  if (!buffer) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("readBuffer"));
      debugPtr->print(F(" was passed a NULL pointer for buffer"));
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return fromIndex;
  }
  if (isFull()) {
    return fromIndex;
  }
  while (buffer[fromIndex] && (len < (capacity()))) {
      concat(buffer[fromIndex]);
      fromIndex++;
  }
  return fromIndex; // next index to read
}

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
size_t SafeString::writeBuffer(char *buf, size_t bufsize, size_t fromSafeStringIndex) const {
  if (buf == buffer) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("writeBuffer"));
      debugPtr->print(F(" cannot copy chars to same SafeString."));
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" fromSafeStringIndex is ")); debugPtr->print(fromSafeStringIndex);
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return 0;
  }
  if (!buf) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("writeBuffer"));
      debugPtr->print(F(" was passed a NULL pointer"));
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" fromSafeStringIndex is ")); debugPtr->print(fromSafeStringIndex);
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return 0;
  }
  if (bufsize == 0) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("writeBuffer"));
      debugPtr->print(F(" passed 0 for char array size"));
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       "));
        debugPtr->print(F(" fromSafeStringIndex is ")); debugPtr->print(fromSafeStringIndex);
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    return 0;
  }
  if (fromSafeStringIndex == len) {
    buf[0] = '\0';
    return 0;
  }

  if (fromSafeStringIndex > len) {
#ifdef SSTRING_DEBUG
    if (debugPtr) {
      errorMethod(F("writeBuffer"));
      debugPtr->print(F(" fromSafeStringIndex ")); debugPtr->print(fromSafeStringIndex); debugPtr->print(F(" > ")); outputName(); debugPtr->print(F(".length() : ")); debugPtr->print(len);
      if (fullDebug) {
        debugPtr->println(); debugPtr->print(F("       ")); debugPtr->print(F(" nothing written."));
      }
      debugInternalMsg(fullDebug);
    }
#endif // SSTRING_DEBUG
    buf[0] = 0;
    return 0;
  }
  size_t n = bufsize - 1;
  if (n > (len - fromSafeStringIndex)) {
    n = len - fromSafeStringIndex;
  }
  strncpy(buf, buffer + fromSafeStringIndex, n);
  buf[n] = 0;
  return n;
}

/**
   bool read(Stream& input)
   returns true if something added to string else false
   Note: if string is already full then nothing will be read and false will be returned
*/
bool SafeString::read(Stream& input) {
  bool rtn = false;
  while (input.available() && (len < _capacity)) {
    int c = input.read();
    if (c != '\0') { // skip any nulls read
      concat((char)c);
      rtn = true;
    }
  }
  return rtn; // true if something added to string
}

/********************************************
   non-blocking readUntil
  return true if delimiter or string filled, found else false

  parameters:
  input - the Stream object to read from
  delimiters - string of valid delimieters
 *********************************************/
bool SafeString::readUntil(Stream& input, SafeString &delimiters) {
  return readUntil(input, delimiters.buffer);
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
  
  while (input.available() && (len < (capacity()))) {
    int c = input.read();
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

/*********************************************/
/*  Debug Error support                      */
/*********************************************/

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

void SafeString::capError(const __FlashStringHelper * methodName, size_t neededCap, const char* cstr, const __FlashStringHelper * pstr, char c, size_t length) {
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

void SafeString::outputFromIndexIfFullDebug(size_t fromIndex) const {
#ifdef SSTRING_DEBUG
  if (fullDebug) {
    debugPtr->println(); debugPtr->print(F("       "));
    debugPtr->print(F(" fromIndex is ")); debugPtr->print(fromIndex);
  }
#endif
}

void SafeString::concatErr() {
#ifdef SSTRING_DEBUG
  errorMethod(F("concat"));
#endif
}

void SafeString::printlnErr() {
#ifdef SSTRING_DEBUG
  errorMethod(F("println"));
#endif
}

void SafeString::prefixErr() {
#ifdef SSTRING_DEBUG
  errorMethod(F("prefix"));
#endif
}
