// Input_Blink_Tasks.ino
/*
   (c)2019 Forward Computing and Control Pty. Ltd.
   NSW Australia, www.forward.com.au
   This code is not warranted to be fit for any purpose. You may only use it at your own risk.
   This generated code may be freely used for both private and commercial use
   provided this copyright is maintained.
*/
#include <loopTimer.h>
// install the loopTimer library from https://www.forward.com.au/pfod/ArduinoProgramming/RealTimeArduino/TimingDelaysInArduino.html
// loopTimer.h also needs the millisDelay library installed from https://www.forward.com.au/pfod/ArduinoProgramming/TimingDelaysInArduino.html
#include <SafeString.h>
#include <BufferedOutput.h>
// install SafeString library from Library manager or from https://www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
// to get BufferedOutput. See https://www.forward.com.au/pfod/ArduinoProgramming/Serial_IO/index.html for a full tutorial
// on Arduino Serial I/O that Works
#include <millisDelay.h>

//Example of using BufferedOutput to release bytes when there is space in the Serial Tx buffer, extra buffer size 80
createBufferedOutput(bufferedOut, 80, DROP_UNTIL_EMPTY);

int led = 13;
// Pin 13 has an led connected on most Arduino boards.
bool ledOn = false; // keep track of the led state
millisDelay ledDelay;
millisDelay printDelay;
bool stopBlinking = false;

char startCmd[] = "start";
char stopCmd[] = "stop";
char delimiters[] = " ,\r\n"; // space dot comma CR NL are cmd delimiters

const size_t maxCmdLength = 15; // > length of largest command to be recognized, can handle longer input but will not tokenize it.
createSafeString(input, maxCmdLength + 1); //  to read input cmd, large enough to hold longest cmd + leading and trailing delimiters
createSafeString(token, maxCmdLength + 1); // for parsing, capacity should be >= input
bool skipToDelimiter = false; // bool variable to hold the skipToDelimiter state across calls to readUntilToken()
// set skipToDelimiter = true to skip initial data upto first delimiter.

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(9600);
  for (int i = 10; i > 0; i--) {
    Serial.println(i);
    delay(500);
  }
  bufferedOut.connect(Serial);  // connect bufferedOut to Serial

  // initialize digital pin led as an output.
  pinMode(led, OUTPUT);
  ledDelay.start(1000); // start the ledDelay, toggle every 1000mS
  printDelay.start(5000); // start the printDelay, print every 5000mS
  Serial.println(F("To control the Led Blinking, enter either stop or start"));
}

// the task method
void blinkLed13(bool stop) {
  if (ledDelay.justFinished()) {   // check if delay has timed out
    ledDelay.repeat(); // start delay again without drift
    if (stop) {
      digitalWrite(led, LOW); // turn led on/off
      ledOn = false;
      return;
    }
    ledOn = !ledOn;     // toggle the led
    digitalWrite(led, ledOn ? HIGH : LOW); // turn led on/off
  } // else nothing to do this call just return, quickly
}

// the task method
void print_mS() {
  if (printDelay.justFinished()) {
    printDelay.repeat(); // start delay again without drift
    bufferedOut.println(millis());   // print the current mS
  } // else nothing to do this call just return, quickly
}

void handleStartCmd() {
  stopBlinking = false;
  bufferedOut.println(F(" Blinking Started"));
}
void handleStopCmd() {
  stopBlinking = true;
  bufferedOut.println(F(" Blinking Stopped"));
}

// task to get the user's cmds, input commands terminated by space or , or \r or \n or no new characters for 2secs
// set Global variable with input cmd
void processUserInput() {
  if (input.readUntilToken(bufferedOut, token, delimiters, skipToDelimiter, true, 2000)) { // echo input and 2000mS timeout, non-blocking!!
    if (token == startCmd) {
      handleStartCmd();
    } else if (token == stopCmd) {
      handleStopCmd();
    } else {
      bufferedOut.println(F(" -- Invalid cmd."));
    } 
  } // else token is empty
}

// the loop function runs over and over again forever
void loop() {
  bufferedOut.nextByteOut(); // call this one or more times each loop() to release buffered chars
  loopTimer.check(bufferedOut);
  processUserInput();
  blinkLed13(stopBlinking); // call the method to blink the led
  print_mS(); // print the time
}
