/*
  SafeStringReader.h  a SafeString non-blocking delimited reader
  by Matthew Ford
  (c)2020 Forward Computing and Control Pty. Ltd.
  This code is not warranted to be fit for any purpose. You may only use it at your own risk.
  This code may be freely used for both private and commercial use.
  Provide this copyright is maintained.
**/

#include "SafeStringReader.h"
#include "SafeString.h" // for SSTRING_DEBUG
// un comment this to get SafeString output messages about skip to delimiter when input buffer fills up

// to skip this for SparkFun RedboardTurbo
#ifndef ARDUINO_SAMD_ZERO
#if defined(ARDUINO_ARDUINO_NANO33BLE) || defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_MEGAAVR)
using namespace arduino;
#endif
#endif // #ifndef ARDUINO_SAMD_ZERO

SafeStringReader::SafeStringReader(SafeString &sfInput_, size_t bufSize, char* tokenBuffer, const char* _name, const char delimiter, bool skipToDelimiterFlag_, uint8_t echoInput_, unsigned long timeout_mS_) : SafeString(bufSize, tokenBuffer, "", _name) {
  internalCharDelimiter[0] = delimiter;
  internalCharDelimiter[1] = '\0';
  init(sfInput_, internalCharDelimiter, skipToDelimiterFlag_, echoInput_, timeout_mS_);	
}	

SafeStringReader::SafeStringReader(SafeString &sfInput_, size_t bufSize, char* tokenBuffer, const char* _name, const char* delimiters_, bool skipToDelimiterFlag_, uint8_t echoInput_, unsigned long timeout_mS_) : SafeString(bufSize, tokenBuffer, "", _name) {
  init(sfInput_, delimiters_, skipToDelimiterFlag_, echoInput_, timeout_mS_);
}
	
void SafeStringReader::init(SafeString& sfInput_,const char* delimiters_, bool skipToDelimiterFlag_, uint8_t echoInput_, unsigned long timeout_mS_) {
  sfInputPtr = &sfInput_;
  end();
  delimiters = delimiters_;
  skipToDelimiterFlag = skipToDelimiterFlag_;
  echoInput = echoInput_;
  timeout_mS = timeout_mS_;
}

void SafeStringReader::connect(Stream& stream) {
  streamPtr = &stream;
  charCounter = 0;
}

bool SafeStringReader::end() {
  bool rtn = sfInputPtr->nextToken(*this, delimiters);
  if (!rtn && (!sfInputPtr->isEmpty())) {
	sfInputPtr->concat(delimiters[0]);
	rtn =sfInputPtr->nextToken(*this, delimiters);
  }
  sfInputPtr->clear();
  skipToDelimiterFlag = false;
  echoInput = false;
  timeout_mS = 0;
  streamPtr = NULL;
  charCounter = 0;
  return rtn;
}

size_t SafeStringReader::getReadCount() {
	return charCounter;
}

//set back to false at next delimiter
void SafeStringReader::skipToDelimiter() {
#ifdef SSTRING_DEBUG
  SafeString::Output.print(F("\nSkipping Input upto next delimiter.\n")); // input overflow
#endif // SSTRING_DEBUG
  skipToDelimiterFlag = true; // sets skipToDelimiter to true
}

void SafeStringReader::setTimeout(unsigned long mS) {
  timeout_mS = mS;
}

void SafeStringReader::echoOn() {
  echoInput = true;
}
void SafeStringReader::echoOff() {
  echoInput = false;
}

char SafeStringReader::getDelimiter() {
  if ((!streamPtr) || (sfInputPtr->isEmpty())) {
    return (char) - 1;
  }
  char c = sfInputPtr->charAt(0);
  if (strchr(delimiters, c) != NULL) {
    // found c in delimiters
    return c;
  }
  //else {
  return (char) - 1;
}

// Each call to this method removes any leading delimiters so if you need to check the delimiter do it BEFORE the next call to read()
bool SafeStringReader::read() {
  if (!streamPtr) {
    SafeString::Output.println();
    SafeString::Output.println(F("SafeStringReader Error: need to call connect(...); first in setup()"));
    SafeString::Output.println();
    SafeString::Output.flush();
    delay(5000);
    return false;
  }
  bool skipMsg = false;
  bool rtn = false;
  bool skipToDelimiterPrior = skipToDelimiterFlag;
  rtn = sfInputPtr->readUntilToken(*streamPtr, *this, delimiters, skipToDelimiterFlag, echoInput, timeout_mS);
  charCounter += sfInputPtr->getLastReadCount();
  if ((!skipToDelimiterPrior) && skipToDelimiterFlag) {
  	skipMsg = true;
  }
  // if skipToDelimiterFlag true the rtn is always false and sfInput has been cleared
  // try to read some more may return true if delimiter found this time
  if ((!rtn) && (skipToDelimiterFlag)) {
    rtn = sfInputPtr->readUntilToken(*streamPtr, *this, delimiters, skipToDelimiterFlag, echoInput, timeout_mS);
    charCounter += sfInputPtr->getLastReadCount();
  }
  if (skipMsg) {
#ifdef SSTRING_DEBUG
    SafeString::Output.println();
    SafeString::Output.print(F("!! Input exceeded buffer size. Skipping Input upto next delimiter.\n")); // input overflow
#endif // SSTRING_DEBUG
  }
  return rtn;
}

/*************************************************/
/**  SafeStringReader input buffer debug methods */
/*************************************************/

// debugInputBuffer() -- these debugInputBuffer( ) methods print out info on this SafeString object, iff SaftString::setOutput( ) has been called
// setVerbose( ) does NOT effect these methods which have their own verbose argument
// Each of these debug( ) methods defaults to outputting the string contents.
// Set the optional verbose argument to false to suppress outputting string contents
//
// NOTE!! all these debug methods return a pointer to an empty char[].
// This is so that if you add .debugInputBuffer() to Serial.println(sfReader);  i.e.
//    Serial.println(sfReader.debugInputBuffer());
// will work as expected
const char* SafeStringReader::debugInputBuffer(bool verbose) { // verbose optional defaults to true
  return sfInputPtr->debug(verbose);
}

// These three versions print leading text before the debug output.
const char* SafeStringReader::debugInputBuffer(const __FlashStringHelper * pstr, bool verbose) { // verbose optional defaults to true
  return sfInputPtr->debug(pstr, verbose);
}

const char* SafeStringReader::debugInputBuffer(const char *title, bool verbose) { // verbose optional defaults to true
  return sfInputPtr->debug(title, verbose);
}

const char* SafeStringReader::debugInputBuffer(SafeString &stitle, bool verbose) { // verbose optional defaults to true
  return sfInputPtr->debug(stitle, verbose);
}

/*****************  end public debug methods *************************/
