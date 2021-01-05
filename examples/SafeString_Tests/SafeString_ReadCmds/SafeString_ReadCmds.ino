// SafeString_ReadCmds.ino
//
// This example takes commmands from the Arduino Monitor input and acts on them
// the available commands are start stop and reset
// Commands are delimited by space dot comma NL or CR
// If you set the Arduino Monitor to No line ending then the command will be ignored!!!
//  Use the settings Newline or Carrage Return or Both NL & CR
//
// These commands can be picked out of a line of user input
// start  stop  reset
//
// download and install the SafeString library from
// www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
#include "SafeString.h"

char startCmd[] = "start";
char stopCmd[] = "stop";
char resetCmd[] = "reset";
char delimiters[] = " ,\r\n"; // space dot comma CR NL are cmd delimiters

const size_t maxCmdLength = 5; // length of largest command to be recognized, can handle longer input but will not tokenize it.
createSafeString(input, maxCmdLength+1); //  to read input cmd, large enough to hold longest cmd + leading and trailing delimiters
createSafeString(token, maxCmdLength+1); // for parsing, capacity should be >= input
bool skipToDelimiter = false; // bool variable to hold the skipToDelimiter state across calls to readUntilToken()
// set skipToDelimiter = true to skip initial data upto first delimiter.
// skipToDelimiter = true can be set at any time to next delimiter.


void setup() {
  Serial.begin(9600);    // Open serial communications and wait a few seconds
  for (int i = 10; i > 0; i--) {
    Serial.print(' '); Serial.print(i);
    delay(500);
  }
  Serial.println();
  // first run without any outputting any error msgs or debugging
  //SafeString::setOutput(Serial); // enable error messages and debug() output to be sent to Serial

  Serial.print(F("Enter one, or more, of the commands : ")); Serial.print(startCmd); Serial.print(" , "); Serial.print(stopCmd); Serial.print(" or "); Serial.print(resetCmd);
  Serial.println(F("  separated by space , CR or NL"));
  Serial.println(F(" Arduino monitor must be set to Newline or Carrage Return or Both NL & CR for this sketch to work."));
}

void handleStartCmd() {
  Serial.println(); Serial.println(F(" Found startCmd"));
}
void handleStopCmd() {
  Serial.println(); Serial.println(F(" Found stopCmd"));
}
void handleResetCmd() {
  Serial.println(); Serial.println(F(" Found resetCmd"));
}

void loop() {
  if (input.readUntilToken(Serial, token, delimiters, skipToDelimiter)) {
    if (token == startCmd) {
      handleStartCmd();
    } else if (token == stopCmd) {
      handleStopCmd();
    } else if (token == resetCmd) {
      handleResetCmd();
    }
  } // else token is empty

  // rest of code here is executed while the user typing in commands
}
