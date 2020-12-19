/*
  SafeStringStream.h  a Stream wrapper for a SafeString
  by Matthew Ford
  (c)2020 Forward Computing and Control Pty. Ltd.
  This code is not warranted to be fit for any purpose. You may only use it at your own risk.
  This code may be freely used for both private and commercial use.
  Provide this copyright is maintained.
**/

#include "SafeStringStream.h"

#ifdef ARDUINO_ARDUINO_NANO33BLE
using namespace arduino;
#endif

/**  SafeStringStream methods **/
SafeStringStream::SafeStringStream( SafeString& sf ) {
	sfPtr = &sf;
	baudRate = 0;
}

SafeStringStream::SafeStringStream() {
	sfPtr = NULL;
	baudRate = 0;
}

// private and so never called
SafeStringStream::SafeStringStream(const SafeStringStream& other) {
	(void)(other); // to suppress unused warning
	sfPtr = NULL;
}

void SafeStringStream::begin(const uint32_t _baudRate) {
	baudRate = _baudRate;
	uS_perByte = 0;
	if (baudRate != 0) { 
      uS_perByte = ((unsigned long)(13000000.0 / (float)baudRate)) + 1; // 1sec / (baud/13) in uS  baud is in bits
      // => ~13bits/byte, i.e. start+8+parity+2stop+1  may be less if no parity and only 1 stop bit
	  sendTimerStart = micros();
    }	
}

void SafeStringStream::begin(SafeString &sf, const uint32_t baudRate) {
	// start to release at this baud rate, 0 means infinite baudRate
	sfPtr =&sf;
	begin(baudRate);
}

int SafeStringStream::availableForWrite() {
	if (sfPtr == NULL) {
		return 0;
	}
	int rtn = sfPtr->availableForWrite();
	return rtn;
}

size_t SafeStringStream::write(uint8_t b) {
	if (sfPtr == NULL) {
		return 0;
	}
	releaseNextByte();
	size_t rtn = sfPtr->write(b);
	return rtn;
}

int SafeStringStream::available() {
	if (sfPtr == NULL) {
		return 0;
	}
	if (baudRate == 0) {
		return sfPtr->length();
	} // else
	releaseNextByte();
	cSFA(sfRxBuffer,Rx_BUFFER);
	return sfRxBuffer.length();
}

int SafeStringStream::read() {
   if (sfPtr == NULL) {
		return -1;
	}
	if (baudRate == 0) {
	  if (sfPtr->isEmpty()) {
		return -1;
	  } // else
	  char c = sfPtr->charAt(0);
      sfPtr->remove(0,1);
      return c;
    } // else
    
	releaseNextByte();
    cSFA(sfRxBuffer,Rx_BUFFER);
	if (sfRxBuffer.isEmpty()) {
		return -1;
	} // else
	char c = sfRxBuffer.charAt(0);
    sfRxBuffer.remove(0,1);
    return c;
}

int SafeStringStream::peek() {
   if (sfPtr == NULL) {
		return -1;
	}
	if (baudRate == 0) {
   	  if (sfPtr->isEmpty()) {
		return -1;
	  } // else
	  return sfPtr->charAt(0);
	} // else
	
	releaseNextByte();
    cSFA(sfRxBuffer,Rx_BUFFER);
	if (sfRxBuffer.isEmpty()) {
		return -1;
	} // else
	return sfRxBuffer.charAt(0);
}

void SafeStringStream::SafeStringStream::flush() {
	releaseNextByte();
	// do nothing here
}

// note built in Rx buffer is only 8 chars
void SafeStringStream::releaseNextByte() {
  if (sfPtr == NULL) {
		return;
  }
  if (baudRate == 0) {
  	  return;
  }
  if (sfPtr->length() == 0) {
    return; // nothing connected or nothing to do
  }
  // micros() has 8uS resolution on 8Mhz systems, 4uS on 16Mhz system
  unsigned long uS = micros();
  unsigned long noOfCharToRelease = (uS - sendTimerStart)/uS_perByte;
  if (noOfCharToRelease > 0) {
    unsigned long excessTime = (uS - sendTimerStart) - (noOfCharToRelease*uS_perByte);
    sendTimerStart = uS-excessTime;
  }
  
  // noOfCharToRelease  limit to available chars
  cSFA(sfRxBuffer,Rx_BUFFER);
  if (noOfCharToRelease > sfPtr->length()) {
  	  noOfCharToRelease = sfPtr->length(); // limit to char left in SF
  }
  for (size_t i=0; i<noOfCharToRelease; i++) {
  	  if (!sfRxBuffer.availableForWrite()) {
  	  	  // make space
  	  	  sfRxBuffer.remove(0,1);
  	  } 
  	  sfRxBuffer += sfPtr->charAt(0);
  	  sfPtr->remove(0,1);
  }
}



