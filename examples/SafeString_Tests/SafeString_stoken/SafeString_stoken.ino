/*
  Tokenizing SafeStrings and converting to numbers
  Examples of how to use the stoken and toLong() and toDouble() to parse a CSV line

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
  Serial.println(F("Using SafeString stoken() to parse a comma separated line for the numbers it contains."));
  Serial.println(F("SafeString::setOutput(Serial); // verbose"));
  // see the SafeString_ConstructorAndDebugging example for debugging settings
  SafeString::setOutput(Serial); // enable full debugging error msgs

  char line[] = ",23.5, 44a ,, , -5. , +.5, 7a, 33,fred5, 6.5.3, a.5,b.3";
  cSFP(sfLine, line);
  Serial.print(F("Input line is '")); Serial.print(sfLine); Serial.println('\'');
  createSafeString(field, 10); // for the field strings. Should have capacity > largest field length
  size_t nextIdx = 0;
  char delimiters[] = ","; // just comma for delimiter, could also use ",;" if comma or semi-colon seperated fields
  Serial.println();
  int fieldCounter = 0;
  Serial.println(F("Using  nextIdx = sfLine.stoken(field, nextIdx, delimiters, true); // true => return all fields even empty ones"));
  Serial.println(F("to find the fields that contain numbers :-"));
  while (nextIdx < sfLine.length()) {
    nextIdx = sfLine.stoken(field, nextIdx, delimiters, true); // true => return all fields even empty ones
    fieldCounter++;
    Serial.print(F("  Field ")); Serial.print(fieldCounter); Serial.print(F("  "));
    double d;
    if (field.toDouble(d)) {
      Serial.println(d);
    } else {
      Serial.print(F(",")); Serial.print(field); Serial.println(F(", is not a number"));
    }
  }
  Serial.println();
  createSafeString(sDelimiters, 4, ",");
  Serial.println(F(" Using a SafeString for the delimiters and not returning empty fields (i.e. ,,)"));
  sDelimiters.debug();
  Serial.println(F("     nextIdx = sfLine.stoken(field, nextIdx, sDelimiters); // default => skip empty fields"));
  Serial.println(F(" This means the field count will be one less this time"));
  Serial.println(F(" The fields with integers are:-"));
  fieldCounter = 0;
  nextIdx = 0; // restart from beginning
  while (nextIdx < sfLine.length()) {
    nextIdx = sfLine.stoken(field, nextIdx, sDelimiters); // default => skip empty fields
    fieldCounter++;
    Serial.print(F("  Field ")); Serial.print(fieldCounter); Serial.print(F("  "));
    long l_num;
    if (field.toLong(l_num)) {
      Serial.println(l_num);
    } else {
      Serial.print(F(",")); Serial.print(field); Serial.println(F(", is not an integer number"));
    }
  }
  Serial.println();


  Serial.println(F("The use of the delimiters argument can be inverted so that it lists the valid chars of a token"));
  Serial.println(F(" Pick out the number in this string"));
  createSafeString(stringOne, 20, "size = 55.33cm");
  stringOne.debug();
  createSafeString(validChars, 12, "0123456789.");
  validChars.debug(F(" Valid token chars are => "));
  Serial.println();

  Serial.println(F(" First use "));
  Serial.println(F("nextIdx = stringOne.stoken(field, 0, validChars);"));
  nextIdx = stringOne.stoken(field, 0, validChars);
  Serial.print(F(" to find the first digit. This returns nextIdx : ")); Serial.println(nextIdx);
  Serial.println(F(" Then use validChars not as delimiters but as the valid token chars to extract the number"));
  Serial.println(F(" the last false does this   nextIdx = stringOne.stoken(field, nextIdx, validChars, false, false); "));
  Serial.println(F(" The first char not in validChars terminates the token."));
  nextIdx = stringOne.stoken(field, nextIdx, validChars, false, false); // first false => do not return empty fields, second false => do NOT use delimiters as delimiters but use them as valid token chars
  field.debug(F("stringOne.stoken(field, nextIdx, validChars, false, false); => "));
  Serial.print(F(" Returned nextIdx is ")); Serial.println(nextIdx);
  Serial.println();

  Serial.println(F("Error checking.."));
  Serial.println();

  Serial.println(F("Check if field SafeString not large enough for token"));
  createSafeString(smallField, 2);
  nextIdx = 0;
  Serial.println(F("nextIdx = sfLine.stoken(smallField, nextIdx, delimiters);"));
  nextIdx = sfLine.stoken(smallField, nextIdx, delimiters);
  Serial.print(F(" nextIdx is updated to ")); Serial.print(nextIdx); Serial.print(F(" but the smallField returned is empty.")); Serial.println();
  Serial.println(F(" nextIdx is updated to prevent token processing loops like "));
  Serial.println(F("while (nextIdx < sfLine.length()) {"));
  Serial.println(F(" looping forever "));
  Serial.println();

  Serial.println(F("Check if empty delimitiers"));
  nextIdx = 0;
  Serial.println(F("nextIdx = sfLine.stoken(field, nextIdx, \"\");"));
  nextIdx = sfLine.stoken(field, nextIdx, "");
  Serial.println();

  Serial.println(F("Check if delimitiers NULL"));
  nextIdx = 0;
  char *nullDelims = NULL;
  Serial.println(F("nextIdx = sfLine.stoken(field, nextIdx, nullDelims);"));
  nextIdx = sfLine.stoken(field, nextIdx, nullDelims);
  Serial.println();

  Serial.println(F("Check if fromIndex past end of SafeString"));
  Serial.println(F("nextIdx = sfLine.stoken(field, 60, delimiters);"));
  nextIdx = sfLine.stoken(field, 60, delimiters);
  Serial.println();

  Serial.println(F(" fromIndex == length() is a valid argument, result field will be empty"));
  Serial.println(F("nextIdx = sfLine.stoken(field, 54, delimiters);"));
  nextIdx = sfLine.stoken(field, 54, delimiters);
  Serial.print(F("returned nextIdx : ")); Serial.println(nextIdx);
  field.debug(F("field.debug() => "));

}

void loop() {
  // nothing here
}
