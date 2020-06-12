/*
  SafeSting length(), trim()  and toCharArray()

  by Matthew Ford
  Copyright(c)2020 Forward Computing and Control Pty. Ltd.
  This example code is in the public domain.

  www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
*/
#include "SafeString.h"


void setup() {
  // Open serial communications and wait a few seconds
  Serial.begin(9600);
  for (int i = 10; i > 0; i--) {
    Serial.print(' '); Serial.print(i);
    delay(500);
  }
  Serial.println();
  Serial.println(F("SafeString length(), trim() and toCharArray() functions"));
  Serial.println(F("SafeString::setOutput(Serial); // verbose == true"));
  // see the SafeString_ConstructorAndDebugging example for debugging settings
  SafeString::setOutput(Serial); // enable full debugging error msgs
  Serial.println();

  // here's a String with empty spaces at the end (called white space):
  createSafeString(stringOne, 35, "  Hello!       ");
  stringOne.debug();
  Serial.print(F("stringOne.length() = ")); Serial.println(stringOne.length());
  Serial.println();

  stringOne.trim();
  stringOne.debug(F("stringOne.trim(); => "));
  Serial.print(F("stringOne.length() = ")); Serial.println(stringOne.length());
  Serial.println();

  Serial.println(F(" Usually you don't need to use toCharArray() but it is available"));
  Serial.println(F(" c_str() gives to access to the underly char array as a const char* so you cannot change the contents"));
  Serial.println(F(" toCharArray() copies the characters to your own char[] where you can change them"));
  Serial.println(F(" toCharArray() returns a '\\0' terminated array"));
  Serial.println();

  Serial.print(F("stringOne.c_str() = ")); Serial.println(stringOne.c_str());
  Serial.println();

  const size_t chrsSize = 5;
  char chrs[chrsSize]; // max chrsSize -1 chrs copied and '\0' terminated
  Serial.println(F("const size_t chrsSize = 5;"));
  Serial.println(F("char chrs[chrsSize];"));
  Serial.println(F(" toCharArray() returns the number of characters copied"));
  size_t n = stringOne.toCharArray(chrs, chrsSize, 0);
  Serial.print(F("stringOne.toCharArray(chrs,chrsSize,0); => ")); Serial.println(n);
  Serial.print(F("chrs[] == ")); Serial.println(chrs);
  Serial.println(F(" The number of characters in array is limited to charSize -1. The terminating '\\0' take up one location"));
  Serial.println();

  n = stringOne.toCharArray(chrs, chrsSize, 3);
  Serial.println(F(" You can copy characters starting from a given index. "));
  Serial.print(F("stringOne.toCharArray(chrs,chrsSize,3); => ")); Serial.println(n);
  Serial.print(F("chrs[] == ")); Serial.println(chrs);
  Serial.println();

  Serial.println(F("Error checking.."));
  Serial.println();
  Serial.println(F("stringOne.toCharArray(chrs,0,3);"));
  n = stringOne.toCharArray(chrs, 0, 3);
  Serial.print(F("stringOne.toCharArray(chrs,0,3); => ")); Serial.println(n);
  Serial.println();

  char *nullChars = NULL;
  Serial.println(F("char *nullChars = NULL;"));
  Serial.println(F("stringOne.toCharArray(nullChars,2,3);"));
  n = stringOne.toCharArray(nullChars, 2, 3);
  Serial.print(F("stringOne.toCharArray(nullChars,2,3); => ")); Serial.println(n);
  Serial.println();

  Serial.println(F("stringOne.toCharArray(chrs,chrsSize,6);"));
  n = stringOne.toCharArray(chrs, chrsSize, 6);
  Serial.print(F("stringOne.toCharArray(chrs,chrsSize,6); => ")); Serial.println(n);
  Serial.print(F(" fromIndex == stringOne.length() is OK, the returned chrs[] is empty if fromIndex >= length()  strlen(chrs) == ")); Serial.println(strlen(chrs));
  Serial.println();

  Serial.println(F("stringOne.toCharArray(chrs,chrsSize,7);"));
  n = stringOne.toCharArray(chrs, chrsSize, 7);
  Serial.print(F("stringOne.toCharArray(chrs,chrsSize,7); => ")); Serial.println(n);
  Serial.print(F("The returned chrs[] is empty if fromIndex >= length()  strlen(chrs) == ")); Serial.println(strlen(chrs));

}

void loop() {
}
