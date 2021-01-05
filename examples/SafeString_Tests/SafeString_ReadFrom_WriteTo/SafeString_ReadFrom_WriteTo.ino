// SafeString_ReadFrom_WriteTo.ino
// The sketch illustrates the use of readFrom(), writeTo() nextToken(), removeBefore() and the use of static SafeStrings to hold values between method calls
//
// This example reads from a arbitarily large char array into a small SafeString which is then parsed for valid commands.
// Any commands found are then written to a very small output buffer which is very slowly printed out.
// None of these operation block.  Any other loop() code continues to run at fill speed
//

// download and install the SafeString library from
// www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
#include "SafeString.h"
// download and install millisDelay from
// https://www.forward.com.au/pfod/ArduinoProgramming/TimingDelaysInArduino.html
#include "millisDelay.h"

char text[]  = " some leading text start, stop, reset, start dummy text stop reset."; // note trailing delimiter needed to parse last token
size_t textIdx = 0;  // the next index to read text from

cSF(delimiters, 5, " .,\r\n"); // space dot comma CR NL are command delimiters

const size_t outputBufferSize = 4; // a very small output buffer which is printed very slowly
char outputBuffer[outputBufferSize]; // output buffer
millisDelay printDelay;
const unsigned long PRINT_DELAY_MS = 1000;  // the output buffer is only emptied once every sec


/** ****** Set up an array of commands to check against ********  */
const size_t MAX_CMD_LENGTH = 5; // max length of a command
enum cmdEnum { START_CMD, STOP_CMD, RESET_CMD};
const size_t NO_OF_CMDS = RESET_CMD + 1;
char cmdsArray[NO_OF_CMDS][MAX_CMD_LENGTH + 1]; // +1 for terminating '\0'

// initialize commands in this method so SafeString will catch commands to are too long to fit in the cmdsArray
void initializeCmds() {
  cSFA(cmd0, cmdsArray[START_CMD]);
  cmd0 = "start";
  cSFA(cmd1, cmdsArray[STOP_CMD]);
  cmd1 = "stop";
  cSFA(cmd2, cmdsArray[RESET_CMD]);
  cmd2 = "reset";
}

// -----------  setup() -----------
void setup() {
  Serial.begin(9600);    // Open serial communications and wait a few seconds
  for (int i = 10; i > 0; i--) {
    Serial.print(' '); Serial.print(i);
    delay(500);
  }
  Serial.println();
  // see the SafeString_ConstructorAndDebugging example for debugging settings
  //SafeString::setOutput(Serial); // uncomment this line to see the debugging msgs

  initializeCmds(); // will print message if any cmd too long if SafeString::setOutput(Serial); has been called
  printDelay.start(PRINT_DELAY_MS); // start print delay slowly outputs outputBuffer

  Serial.println(F("Input text containing the commands to be extracted, continually re-entered :-"));
  Serial.println(text);
  Serial.println(F("Parsed Output is"));
}

// When outputToken returns either token empty OR outputBuffer is full
void outputToken(SafeString &token, char* outputBuffer, size_t outBufSize) {
  cSFPS(sfOut, outputBuffer, outBufSize); // keeps any existing data in outBuf,  cSFPS 3rd arg is size of the underlying array
  size_t tokenIdx = token.writeTo(sfOut); // writeTo outputBuffer stops when token empty OR outputBuffer is full
  token.removeBefore(tokenIdx); tokenIdx = 0;  // remove the data just written
}

// Check token against valid commands
// If valid command found call outputToken to add to outputBuffer (char* out)
// chars that don't fit in the outputBuffer are keep in token and output later when there is space
void processCmd(SafeString &token, char* out, size_t outSize) {
  for (size_t i = 0; i < NO_OF_CMDS; i++) {
    if (token == cmdsArray[i]) { // found one
      token += '\n'; // add separator to token
      token.debug("Found cmd ");
      outputToken(token, out, outSize); // send to output
      return; // found cmd
    }
  }
  // else no cmd found have finished processing this token, clear it so it is not output
  token.debug(F(" Not valid cmd "));
  token.clear();
}

// returns new value for txtIdx, the next index in text to readFrom
// writes any commands found to outBuf, outBufSize is the valid size of the outBuf array
size_t processInput(char* text, size_t txtIdx, char* outBuf, size_t outBufSize ) {
  // these two method static's keep their values between method calls
  static char inputArray[MAX_CMD_LENGTH + 2]; // +1 for delimiter, +1 for terminating '\0' keep this array from call to call
  static char tokenArray[MAX_CMD_LENGTH + 2]; // +1 for delimiter, +1 for terminating '\0' keep this array from call to call
  cSFA(input, inputArray); // create a 'static' SafeString string by wrapping the static array inputArray
  cSFA(token, tokenArray); // create a 'static' SafeString string by wrapping the static array tokenArray

  if (!token.isEmpty()) {  // more to copy to output
    outputToken(token, outBuf, outBufSize);
    return txtIdx;
  } // reach here when token is empty. i.e. when all of last token written to outputBuffer

  cSFP(sfText, text); // wrap the text in a SafeString for processing
  if (txtIdx >= sfText.length()) {
    // no new input just return
    return txtIdx;
  }

  txtIdx = input.readFrom(sfText, txtIdx); // readFrom text returns next idx to be read
  {
    // build a message for the debugging, use a code { } block so stack memory used by msg is immediately recovered
    cSF(msg, 60);  msg = " after readFrom( )  txtIdx:"; msg += txtIdx;
    input.debug(msg);
  }

  // parse input for delimited tokens
  while (input.nextToken(token, delimiters)) { // skips leading delimiters, true while there is a nextToken to process
    input.debug(); token.debug();
    processCmd(token, outBuf, outBufSize);
    if (!token.isEmpty()) {
      break; // break here waiting for output to clear
    }
  } // have processed all the tokens in the current input or are waiting for space in the outputBuffer
  if (input.isFull()) {
    input.debug(F("!! Skipping input that is too large to be a command: "));
    input.clear(); // no delimited token < token size
  }

  return txtIdx; // next read location
}


void loop() {
  // ... other loop stuff

  textIdx = processInput(text, textIdx, outputBuffer, outputBufferSize );
  if (textIdx >= strlen(text)) {
    textIdx = 0; // restart with another set of the input
  }

  if (printDelay.justFinished()) { // slow output of parsed commands
    printDelay.restart();
    cSFP(sfOutput, outputBuffer); // wrap in a SafeString for processing
    Serial.print(sfOutput);  // print it
    sfOutput.clear(); // finished processing parsed commandws, clear outputBuffer
  }
}
