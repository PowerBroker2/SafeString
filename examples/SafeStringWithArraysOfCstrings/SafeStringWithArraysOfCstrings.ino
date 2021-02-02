
/* SafeStringWithArraysOfCstrings.ino
    Example of using SafeString for working with char[][xx]

  by Matthew Ford
  Copyright(c)2020 Forward Computing and Control Pty. Ltd.
  This example code is in the public domain.

  download and install the SafeString library from Arduino library manager
  or from www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
*/

#include <SafeString.h>

#define MAX_STRING_SIZE 40
char arr[][MAX_STRING_SIZE] = { 
  "array of c string",
  "is fun to use",
  "make sure to properly",
  "tell the array size"
};

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  for (int i = 10; i > 0; i--) {
    Serial.print(i); Serial.print(' ');
  }
  Serial.println();
  SafeString::setOutput(Serial); // enable error msgs

  // to modify the string safely wrap it in a SafeString
  cSFA(sfarr0, arr[0]); // OR in the long form   createSafeStringFromCharArray(sfarr0,arr[0]);
  // the capacity is automatically picked up from the arr[][xx] definition
  Serial.print("sfarr0 capacity:"); Serial.println(sfarr0.capacity());
  sfarr0 += " add a bit more";
  Serial.println(" Print the underlying array arr[0]");
  Serial.println(arr[0]);
  sfarr0 += " try to add alot more ";
  
  Serial.println();
  createSafeStringFromCharArray(sfarr1, arr[1]);
  sfarr1.removeFrom(2); // just keep the first 2 chars idx 0,1
  Serial.println(arr[1]);
}

void loop() {
}
