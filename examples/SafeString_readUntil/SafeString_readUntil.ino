/*
  SafeString readUntil, non-blocking until delimiter found
  Example of how to use the non-blocking readUntil() method to parse a CSV line

  by Matthew Ford
  Copyright(c)2020 Forward Computing and Control Pty. Ltd.
  This example code is in the public domain.

  www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
*/
#include "SafeString.h"

/** test inputs  copy and past to the monitor input with Newline settings
**   this one has an empty final field
  5.33 , abcd,33 ,test,
** this one has a non empty final field
  5.33 , abcd,33 ,test, 66
** this one has fields which are too long
  1234567890123abcdefghijklmn,441234567890123,33
****/

createSafeString(input, 10); // SafeString to collect Stream input. Should have capacity > largest field length + 1 for delimiter
createSafeString(delimiters,3,",\n");
int fieldCount = 1;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  for (int i = 10; i > 0; i--) {
    Serial.print(' '); Serial.print(i);
    delay(500);
  }
  Serial.println();

  Serial.println();
  Serial.println(F("Using SafeString readUntil() to read a comma separated line from Serial and parse it into fields."));
  Serial.println(F("  The input SafeString is only a capacity 8 >= the size of the largest field + 1 for the endOfLine sequence."));
  Serial.println(F("  This sketch assumes CSV lines are terminated by Newline (\\n) "));
  // see the SafeString_ConstructorAndDebugging example for debugging settings
  //SafeString::setOutput(Serial); // uncomment this line to see the input debugging

  Serial.println("Input data terminated by Newline (only)");
}

// this is only called for strings ending in a delimiter
void processField() {
  if (!input.endsWithCharFrom(delimiters)) { //  input overflowed
    Serial.print(F(" Field:")); Serial.print(fieldCount); Serial.print(F(" overflowed '")); Serial.print(input); Serial.println('\'');
    input.clear(); 
    return;
  }
  bool endOfLine = input.endsWith("\n");
  input.removeLast(1); // remove delimiter
  Serial.print(F(" Field:")); Serial.print(fieldCount); Serial.print(F(" '"));  Serial.print(input); Serial.println('\''); 
  input.clear(); // ready for next field
  fieldCount++;
  if (endOfLine) {
    fieldCount = 1; // reset
  }
}

void loop() {

  if (input.readUntil(Serial, delimiters)) { // returns true if delimiter found or input full
    input.debug(F(" readUntil => "));
    processField();
  }

}
