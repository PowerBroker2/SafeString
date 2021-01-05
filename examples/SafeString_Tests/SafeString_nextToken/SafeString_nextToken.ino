/*
  Tokenizing SafeStrings and converting to numbers
  Examples of how to use the nextToken() and toLong() and toDouble() to parse a CSV line

  by Matthew Ford
  Copyright(c)2020 Forward Computing and Control Pty. Ltd.
  This example code is in the public domain.

  www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
*/
#include "SafeString.h"

char line[] = "23.5, 44a ,, , -5. , +.5, 7a, 33,fred5, 6.5.3, a.5,b.3\n";
char delimiters[] = ",\n"; // just comma for delimiter, could also use ",;" if comma or semi-colon seperated fields

void setup() {
  // Open serial communications and wait a few seconds
  Serial.begin(9600);
  for (int i = 10; i > 0; i--) {
    Serial.print(' '); Serial.print(i);
    delay(500);
  }
  Serial.println();
  Serial.println(F("Using SafeString nextToken() to parse a comma separated line for the numbers it contains."));
  Serial.println(F("SafeString::setOutput(Serial); // verbose"));
  // see the SafeString_ConstructorAndDebugging example for debugging settings
  SafeString::setOutput(Serial); // enable full debugging error msgs

  createSafeString(sfLine,64); // a SafeString large enough to hold the whole line.
  sfLine = line; // initialize the SaftString for processing
  Serial.print(F("Input line is '")); Serial.print(sfLine); Serial.println('\'');
  Serial.println(F("The delimiters are comma and newline (\\n)"));
  Serial.println(F("Note: the last field must be terminated by a delimiter or it will not be returned by nextToken()"));
  createSafeString(field, 10); // for the field strings. Should have capacity > largest field length
  Serial.println();
  Serial.println(F("Fields with numbers are:-"));
  while (sfLine.nextToken(field,delimiters)) {
    double d;
    if (field.toDouble(d)) {
      Serial.println(d);
    } else {
      Serial.print(F("  Field '")); Serial.print(field); Serial.println(F("' is not a number"));
    }
  }
  Serial.println();

  Serial.println(F("After processing by nextToken() the Input line is empty because nextToken() removes the tokens and delimiters from the line being processed."));
  sfLine.debug();
  
  Serial.println();
  Serial.println(F("sfLine = line; // re-initialize the line since nextToken removes the tokens and delimiters"));
  sfLine = line; // re-initialize the line since nextToken removes the tokens and delimiters
  Serial.print(F("Input line is '")); Serial.print(sfLine); Serial.println('\'');
  createSafeString(sDelimiters, 4, ",\n");
  sDelimiters.debug(F(" Test using a SafeString for the delimiters  --  "));
  Serial.println(F(" The fields with integers are:-"));
  while (sfLine.nextToken(field,sDelimiters)) {
    long l_num;
    if (field.toLong(l_num)) {
      Serial.println(l_num);
    } else {
      Serial.print(F("  Field ,")); Serial.print(field); Serial.println(F(" is not an integer number"));
    }
  }
  Serial.println();


  Serial.println(F("Error checking.."));
  Serial.println();

  Serial.println(F("Check if field SafeString not large enough for token"));
  Serial.println(F("sfLine = line;"));
  sfLine = line;
  createSafeString(smallField, 2);
  Serial.println(F("bool rtn = sfLine.nextToken(smallField, delimiters);"));
  bool rtn = sfLine.nextToken(smallField, delimiters);
  if (rtn) {
    smallField.debug(F(" rtn true, but smallField is empty."));
  }
  Serial.println(F(" If the SafeString argument is too small, nextToken() returns true and removes the token, but returns an empty smallField"));
  sfLine.debug();
  Serial.println(F(" This is to prevent token processing loops like "));
  Serial.println(F("while (sfLine.nextToken(smallField,delimiters)) {"));
  Serial.println(F(" looping forever "));
  Serial.println();

  Serial.println(F("Check for empty delimitiers"));
  char emptyDelimiters[] = "";
  Serial.println(F("sfLine.nextToken(smallField,emptyDelimiters);"));
  sfLine.nextToken(smallField,emptyDelimiters);
  Serial.println();

  Serial.println(F("Check if delimitiers NULL"));
  char *nullDelims = NULL;
  Serial.println(F("sfLine.nextToken(smallField,nullDelims);"));
  sfLine.nextToken(smallField,nullDelims);
  Serial.println();

}

void loop() {
  // nothing here
}
