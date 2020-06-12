// SafeString_ReadBuffer_WriteBuffer.ino
//
// This example reads a arbitary char array of input and extracts commmands
// and writes the commands found to a char[] which is printed as it fills up
//
// download and install the SafeString library from
// www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
#include "SafeString.h"

const char commands[]  = " some leading text  stop, reset, start dummy text";
size_t commandsIdx = 0;
const size_t outputBufferSize = 4; // a very small output buffer can be larger
char outputBuffer[outputBufferSize]; // output buffer

createSafeString(startCmd, 5, "start"); // commands case sensitive
createSafeString(stopCmd, 5, "stop"); // commands case sensitive
createSafeString(resetCmd, 5, "reset"); // commands case sensitive
createSafeString(delimiters, 5, " .,\r\n"); // space dot comma CR NL are cmd delimiters

createSafeString(input, 5); // create an empty string called input
createSafeString(token, 5 + 1); // to process input token same size as input + space to add output buffer separator

void setup() {
  Serial.begin(9600);    // Open serial communications and wait a few seconds
  for (int i = 10; i > 0; i--) {
    Serial.print(' '); Serial.print(i);
    delay(500);
  }
  Serial.println();
  // see the SafeString_ConstructorAndDebugging example for debugging settings
  //SafeString::setOutput(Serial); // uncomment this line to see the debugging msgs

  // show msg to user
  Serial.println(F("Input buffer containing the commands to be extracted :-"));
  Serial.println(commands);
  Serial.println(F("Output is :-"));
}


void loop() {

  while (commands[commandsIdx] != '\0') { // not end of buffer
    commandsIdx = input.readBuffer(commands, commandsIdx);
    input.debug("after read : ");
    size_t index = 0;
    bool putBackPartialToken = false;
    while (index < input.length()) {
      index = input.stoken(token, index, delimiters, false); // skip delimiters
      index = input.stoken(token, index, delimiters);  // check for cmds
      if ((token == startCmd) || (token == stopCmd) || (token == resetCmd)) {
        // found cmd output it to output buffer,  == is fast fail for 2 SafeStrings by first comparing lengths
        token += ' '; // add separator to token
        token.debug();
        size_t charsWritten = 0;
        while (charsWritten < token.length()) { // output buffer full
          charsWritten += token.writeBuffer(outputBuffer, outputBufferSize, charsWritten); // write from last token index
          SafeString::Output.print(" -- outputBuffer '"); // only printed if SafeString::setOutput(..) called
          Serial.print(outputBuffer); // print the buffer. Always printed.
          SafeString::Output.println('\''); // only printed if SafeString::setOutput(..) called
        }
      } else { // not a valid cmd
        if ((index == input.length()) && (!token.isEmpty()) && (token.length() < input.capacity())) {// no delimiter for last token
          putBackPartialToken = true; // put last partial token back as it may be the start of a command
        }
      }
    }
    input.clear(); // finished parsing this input
    if (putBackPartialToken) {
      input = token;
      input.debug(" partial token put back =>");
    }
    if (commands[commandsIdx] == '\0') { // reached end of input buffer
      Serial.println(); // finished
    }
  }
}
