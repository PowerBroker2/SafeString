/*
  SafeStringReader.h  a SafeString non-blocking delimited reader
  by Matthew Ford
  (c)2020 Forward Computing and Control Pty. Ltd.
  This code is not warranted to be fit for any purpose. You may only use it at your own risk.
  This code may be freely used for both private and commercial use.
  Provide this copyright is maintained.
**/

#include "SafeString.h" // for SSTRING_DEBUG
#include "SafeStringReader.h"
// un comment this to get SafeString output messages about skip to delimiter when input buffer fills up

#include "SafeStringNameSpace.h"

// here buffSize is max size of the token + 1 for delimiter + 1 for terminating '\0;
SafeStringReader::SafeStringReader(SafeString &sfInput_, size_t bufSize, char* tokenBuffer, const char* _name, const char delimiter, bool skipToDelimiterFlag_, uint8_t echoInput_, unsigned long timeout_ms_) : SafeString(bufSize, tokenBuffer, "", _name) {
  internalCharDelimiter[0] = delimiter;
  internalCharDelimiter[1] = '\0';
  init(sfInput_, internalCharDelimiter, skipToDelimiterFlag_, echoInput_, timeout_ms_);	
}	

SafeStringReader::SafeStringReader(SafeString &sfInput_, size_t bufSize, char* tokenBuffer, const char* _name, const char* delimiters_, bool skipToDelimiterFlag_, uint8_t echoInput_, unsigned long timeout_ms_) : SafeString(bufSize, tokenBuffer, "", _name) {
  init(sfInput_, delimiters_, skipToDelimiterFlag_, echoInput_, timeout_ms_);
}
	
void SafeStringReader::init(SafeString& sfInput_,const char* delimiters_, bool skipToDelimiterFlag_, uint8_t echoInput_, unsigned long timeout_ms_) {
  sfInputPtr = &sfInput_;
  delimiters = delimiters_;
  end();  // end needs delimiters set!!
  skipToDelimiterFlag = skipToDelimiterFlag_;
  echoInput = echoInput_;
  timeout_ms = timeout_ms_;
  emptyTokensReturned = false;
  flagFlushInput = false;
  longTokenDiscardedFlag = false;
  haveToken = false;
  streamPtr = NULL;
  charCounter = 0;
}

bool SafeStringReader::isSkippingToDelimiter() {
	return (flagFlushInput || skipToDelimiterFlag);
}

// true if the last read() found a token longer than size and discarded it.
// Over-long input is bad DATA, not a coding error, so no error is raised and nothing is printed.
// While true, this SafeStringReader holds the leading chars of that input so they can be shown to
// whoever sent them.  Cleared at the start of every read(), see the header for the usage pattern.
bool SafeStringReader::longTokenDiscarded() {
	return longTokenDiscardedFlag;
}

void SafeStringReader::connect(Stream& stream) {
  streamPtr = &stream;
  charCounter = 0;
  if (flagFlushInput) {
  	  flushInput();
  }
}

void SafeStringReader::returnEmptyTokens(bool flag) {
	emptyTokensReturned = flag;
}
	
bool SafeStringReader::end() {
	// skip multiple delimiters and return last one
  bool rtn = sfInputPtr->nextToken(*this, delimiters, false, true);
//  if (!rtn && (!sfInputPtr->isEmpty())) {
//	sfInputPtr->concat(delimiters[0]);
//	rtn =sfInputPtr->nextToken(*this, delimiters);
//  }
  sfInputPtr->clear(); // clears delimiter and rest of input
  skipToDelimiterFlag = false;
  flagFlushInput = false;
  //echoInput = false;
  //timeout_ms = 0;
  streamPtr = NULL;
  charCounter = 0;
  return rtn;
}

size_t SafeStringReader::getReadCount() {
	return charCounter;
}

//set back to false at next delimiter
// Nothing is printed here.  The caller asked for this skip, so it is neither a coding error nor
// unexpected input, and SafeString::Output is the channel for SafeString coding errors.
void SafeStringReader::skipToDelimiter() {
  skipToDelimiterFlag = true; // sets skipToDelimiter to true
}

void SafeStringReader::setTimeout(unsigned long ms) {
  timeout_ms = ms;
}

void SafeStringReader::echoOn() {
  echoInput = true;
}
void SafeStringReader::echoOff() {
  echoInput = false;
}

int SafeStringReader::getDelimiter() {
  if ((!streamPtr) || (sfInputPtr->isEmpty())) {
    return - 1;
  } 
  char c = sfInputPtr->charAt(0);
  if (strchr(delimiters, c) != NULL) {
    // found c in delimiters
    int rtn = c; // may sign extend to -ve number if char is signed and delimiter is 0xf0 to 0xff
    return (rtn & 255); // clear upper bits to clean up any sign extension
  }
  //else {
  return - 1;
}


// clear any buffered input and Stream RX buffer then skips to next delimiter or times out.
// then goes back to normal input processing
void SafeStringReader::flushInput() {
  flagFlushInput = true;
  if (!streamPtr) {
  	  return; // connect will call back here if flagFlushInput true
  }
  
  flagFlushInput = false;
  // clear any buffered chars
  sfInputPtr->clear();
  //clear(); // clear SafeStringReader no leave is as may be still processing
  //  clear stream RX buffer
  while(streamPtr->available()) {
  	 char c = (char)streamPtr->read();
     if (c == '\0') {
       setError(); // found '\0' in input
#ifdef SSTRING_DEBUG
// skip this because on power up RX buffer all zeros
//       if (debugPtr) {
//         debugPtr->println(); debugPtr->print(F("!! Error:")); outputName();
//         debugPtr->println(F(" -- read '\\0' from Stream."));
//       }
#endif // SSTRING_DEBUG
     } else if (echoInput) {
       streamPtr->print(c);
     }
  }
  skipToDelimiterFlag = true; // sets skipToDelimiter to true  
  read(); // handle timeout if set
  // skip msg
  //skipToDelimiter(); // skip to next delimiter or time out if set
}

// Each call to this method removes the lead delimiter so if you need to check the delimiter do it BEFORE the next call to read()
// NOTE: this call always clears the SafeStringReader so no need to call clear() on sfReader at end of processing.
//
// Returns true when a delimited token has been read.  The token may be empty, that is an empty
// field between two successive delimiters, and is only returned if returnEmptyTokens(true) was set.
//
// Returns false when there is no complete token yet, AND when an over-long token has just been
// detected and discarded.  Those two are told apart by longTokenDiscarded():
//   true  -> the input was longer than the size this reader was created with.  The leading chars
//            of it are left in this SafeStringReader so they can be shown to whoever sent them,
//            the rest is discarded as it arrives, up to the next delimiter.
//            This is the only case where read() returns false with a non-empty SafeStringReader.
//   false -> nothing to report, just carry on with the rest of loop()
// Over-long input is bad DATA, not a coding error, so it raises no error and prints nothing.
bool SafeStringReader::read() {
  if (!streamPtr) {
    SafeString::Output.println();
    SafeString::Output.println(F("SafeStringReader Error: need to call connect(...); first in setup()"));
    SafeString::Output.println();
    SafeString::Output.flush();
    delay(5000);
    return false;
  }
  longTokenDiscardedFlag = false; // refers to this read() call only
  bool rtn = false;
  bool skipToDelimiterPrior = skipToDelimiterFlag;
  rtn = sfInputPtr->readUntilToken(*streamPtr, *this, delimiters, skipToDelimiterFlag, echoInput, timeout_ms);
  charCounter += sfInputPtr->getLastReadCount();
  if ((!skipToDelimiterPrior) && skipToDelimiterFlag) {
    // skipToDelimiter has just gone from false to true, so readUntilToken found input longer than
    // capacity and is discarding it.  That is bad DATA, not a coding error, so nothing is printed
    // and no error is raised, the caller tests longTokenDiscarded() instead.
    longTokenDiscardedFlag = true;
  }
  // if skipToDelimiterFlag true the rtn is always false and sfInput has been cleared
  // try to read some more may return true if delimiter found this time
  // NOT when longTokenDiscardedFlag was just set though, because readUntilToken() clears the token
  // at the start of every call and that would wipe the over-long chars before the caller sees them.
  // Skipping then starts on the next read() instead of this one, which costs nothing.
  if ((!rtn) && (skipToDelimiterFlag) && (!longTokenDiscardedFlag)) {
    rtn = sfInputPtr->readUntilToken(*streamPtr, *this, delimiters, skipToDelimiterFlag, echoInput, timeout_ms);
    charCounter += sfInputPtr->getLastReadCount();
  }
  if ((!emptyTokensReturned) && isEmpty()) {
  	  return false;
  } // else
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

/*************************************************/
/**  assignment operator methods                 */
/*************************************************/
SafeStringReader & SafeStringReader::operator = (SafeString &rhs) {
	return 	(SafeStringReader &)((*this).SafeString::operator= (rhs));
}

SafeStringReader & SafeStringReader::operator = (char c) {
	return 	(SafeStringReader &)((*this).SafeString::operator= (c));
}

SafeStringReader & SafeStringReader::operator = (const char *cstr) {
  return 	(SafeStringReader &)((*this).SafeString::operator= (cstr));
}

SafeStringReader & SafeStringReader::operator = (const __FlashStringHelper *pstr) {
	return 	(SafeStringReader &)((*this).SafeString::operator= (pstr));
}

SafeStringReader & SafeStringReader::operator = (unsigned char c) {
	return 	(SafeStringReader &)((*this).SafeString::operator= (c));
}

SafeStringReader & SafeStringReader::operator = (int num) {
	return 	(SafeStringReader &)((*this).SafeString::operator= (num));
}

SafeStringReader & SafeStringReader::operator = (unsigned int num) {
	return 	(SafeStringReader &)((*this).SafeString::operator= (num));
}

SafeStringReader & SafeStringReader::operator = (long num) {
 return 	(SafeStringReader &)((*this).SafeString::operator= (num));
}

SafeStringReader & SafeStringReader::operator = (unsigned long num) {
	return 	(SafeStringReader &)((*this).SafeString::operator= (num));
}

SafeStringReader & SafeStringReader::operator = (float num) {
	return 	(SafeStringReader &)((*this).SafeString::operator= (num));
}

SafeStringReader & SafeStringReader::operator = (double num) {
	return 	(SafeStringReader &)((*this).SafeString::operator= (num));
}
/**********  assignment operator methods *************/

/*****************  end public debug methods *************************/
