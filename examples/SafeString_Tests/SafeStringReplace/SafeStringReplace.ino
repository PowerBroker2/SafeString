/*
  SafeString replace()
  Examples of SafeString replace for chars and strings

  by Matthew Ford
  Copyright(c)2020 Forward Computing and Control Pty. Ltd.
  This example code is in the public domain.

  www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
*/

#include "SafeString.h"
createSafeString(stringOne, 20, "<html><head><body>");
createSafeString(stringTwo, 30);

void setup() {
  // Open serial communications and wait a few seconds
  Serial.begin(9600);
  for (int i = 10; i > 0; i--) {
    Serial.print(' '); Serial.print(i);
    delay(500);
  }
  Serial.println();

  Serial.println(F("SafeString  replace() usage"));
  Serial.println(F("SafeString::setOutput(Serial); // verbose == true"));
  // see the SafeString_ConstructorAndDebugging example for debugging settings
  SafeString::setOutput(Serial); // enable verbose debugging error msgs
  Serial.println();

  stringOne.debug();
  Serial.println();
  stringTwo = stringOne;
  stringTwo.debug(F("stringTwo = stringOne; => "));
  stringTwo.replace('<', ' ');
  stringTwo.replace('>', ' ');
  Serial.print(F("stringTwo.replace('>', ' '); stringTwo.replace('<', ' '); => ")); stringTwo.debug();
  Serial.println();

  stringTwo = stringOne;
  stringTwo.debug(F("stringTwo = stringOne; => "));
  stringTwo.replace("<body>", "");
  stringTwo.debug(F("stringTwo.replace(\"<body>\", \"\"); => "));
  Serial.println();

  stringTwo = stringOne;
  stringTwo.debug(F("stringTwo = stringOne; => "));
  stringTwo.replace("<", "</");
  stringTwo.debug(F("stringTwo.replace(\"<\", \"</\"); => "));
  Serial.println();

  createSafeString(find, 5);   createSafeString(replace, 5);
  Serial.println(F("createSafeString(find,5);   createSafeString(replace,5);"));
  find = "</";
  find.debug(F("find = \"</\"; => "));
  replace = "<";
  replace.debug(F("replace = \"<\"; => "));
  stringTwo.replace(find, replace);
  stringTwo.debug(F("stringTwo.replace(find,replace); => "));
  Serial.println();

  Serial.println(F("Error checking.."));
  Serial.println();

  Serial.println(F("stringTwo.replace(\"<\", \"<_____\");"));
  stringTwo.replace("<", "<_____");
  Serial.println();

  Serial.println(F("stringTwo.replace(stringTwo,replace);"));
  stringTwo.replace(stringTwo, replace);
  Serial.println();

  Serial.println(F("stringTwo.replace(find,stringTwo);"));
  stringTwo.replace(find, stringTwo);
  Serial.println();

  createSafeString(emptyString, 3);
  emptyString.debug();
  Serial.println(F("stringTwo.replace(emptyString,replace);"));
  stringTwo.replace(emptyString, replace);
  Serial.println();

  char *nullStr = NULL;
  Serial.println(F("char *nullStr = NULL;"));
  Serial.println(F("stringTwo.replace(nullStr,\"\");"));
  stringTwo.replace(nullStr, "");
  Serial.println();

  Serial.println(F("stringTwo.replace(\"<\",nullStr);"));
  stringTwo.replace("<", nullStr);
  Serial.println();

}

void loop() {
}
