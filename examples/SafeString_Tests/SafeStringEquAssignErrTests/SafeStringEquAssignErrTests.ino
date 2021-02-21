/*
  Test Errors for Assignment to SafeStrings using the = operator

  by Matthew Ford
  Mods Copyright(c)2020 Forward Computing and Control Pty. Ltd.
  Modified from String Examples by Tom Igoe
  This example code is in the public domain.

  www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
*/

#include "SafeString.h"

createSafeString(stringOne, 2, "s1");
createSafeString(stringZeroCapacity, 0);

void setup() {
  // Open serial communications and wait a few seconds
  Serial.begin(9600);
  for (int i = 10; i > 0; i--) {
    Serial.print(' '); Serial.print(i);
    delay(500);
  }
  Serial.println();

  Serial.println(F("Test Errors for Assignment to a SafeString using ="));
  Serial.println(F("SafeString::setOutput(Serial); // verbose output "));
  SafeString::setOutput(Serial); // enable debugging error msgs
  // see the SafeString_ConstructorAndDebugging example for debugging settings
  Serial.println();


  stringZeroCapacity = 'a';
  Serial.println();

  stringOne = "test";
  Serial.println();
  char *nullPtr = NULL;
  stringOne = nullPtr;
  Serial.println();

  stringOne = F("some text");
  Serial.println();
  stringOne = 5.5;
  Serial.println();
  stringOne = 100;
  Serial.println();

}

void loop() {
  // put your main code here, to run repeatedly:

}
