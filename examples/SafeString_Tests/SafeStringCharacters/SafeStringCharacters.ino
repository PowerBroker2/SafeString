/*
  SafeString charAt() and setCharAt()
  Examples of how to get and set characters of a SafeString

  by Matthew Ford
  Mods Copyright(c)2020 Forward Computing and Control Pty. Ltd.
  Modified from String Examples by Tom Igoe
  This example code is in the public domain.

  www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
*/

#include "SafeString.h"
createSafeString(sensorStr, 20);
createSafeString(message, 90);

void setup() {
  // Open serial communications and wait a few seconds
  Serial.begin(9600);
  for (int i = 10; i > 0; i--) {
    Serial.print(' '); Serial.print(i);
    delay(500);
  }
  Serial.println();
  Serial.println(F("SafeString charAt() and setCharAt() and [] access operator."));
  Serial.println(F("SafeString::setOutput(Serial); // verbose == true"));
  // see the SafeString_ConstructorAndDebugging example for debugging settings
  SafeString::setOutput(Serial); // enable debugging error msgs

  Serial.println();
  // make a SafeString to report a sensor reading:
  sensorStr = F("SensorReading : 456");
  sensorStr.debug(F("sensorStr = F(\"SensorReading : 456\"); => "));
  // the reading's most significant digit is at position 16 in the sensorStr:
  char mostSignificantDigit = sensorStr.charAt(16);
  Serial.println(F("char mostSignificantDigit = sensorStr.charAt(16);"));

  message = F("mostSignificantDigit is: ");
  message += mostSignificantDigit;
  Serial.println(message);

  Serial.println(F("Using the [] operator."));
  Serial.print(F("The char at sensorStr[6] is ")); Serial.println(sensorStr[6]);
  Serial.println();

  Serial.println(F(" You can also set the character of a SafeString. Change the : to a = character"));
  sensorStr.setCharAt(14, '=');

  sensorStr.debug(F("sensorStr.setCharAt(14, '='); => "));
  Serial.println(F("sensorStr[14] = '=';   is not supported."));
  Serial.println(F("  because the code can not guard against sensorStr[14] = '\\0' which would invaliate the SafeString."));
  Serial.println(F("  use setCharAt( ) instead."));

  Serial.println();
  Serial.println(F("Error checking.."));
  Serial.println();
  Serial.println(F("sensorStr.setCharAt(14, '\\0');"));
  sensorStr.setCharAt(14, '\0');
  Serial.println();
  Serial.println(F(" Use removeFrom(14) instead"));
  sensorStr.removeFrom(14);
  sensorStr.debug(F("sensorStr.removeFrom(14); => "));
  Serial.println();

  Serial.println(F("sensorStr.setCharAt(33, 'a');"));
  sensorStr.setCharAt(33, 'a');
  Serial.println();

  Serial.println(F("charAt() and [ ] return '\\0' if index is >= length()"));
  Serial.println(F("char c = sensorStr.charAt(19);"));
  char c = sensorStr.charAt(19);
  Serial.println();

  Serial.println(F("char c = sensorStr[22];"));
  c = sensorStr[22];

}

void loop() {
}
