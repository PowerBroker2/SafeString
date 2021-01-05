// TempInputBlink_Tasks.ino
// Uses MAX31856_noDelay library which does not use delay(), (good)
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

#include <MAX31856_noDelay.h>
// Use software SPI: CS, DI, DO, CLK
MAX31856_noDelay maxthermo = MAX31856_noDelay(10, 11, 12, 13);
// use hardware SPI, just pass in the CS pin
//MAX31856_noDelay maxthermo = MAX31856_noDelay(10);

createBufferedOutput(bufferedOut, 80, DROP_UNTIL_EMPTY);

char startCmd[] = "startTemps";
char stopCmd[] = "stopTemps";
char delimiters[] = " ,\r\n"; // space dot comma CR NL are cmd delimiters

const size_t maxCmdLength = 15; // > length of largest command to be recognized, can handle longer input but will not tokenize it.
createSafeString(input, maxCmdLength + 1); //  to read input cmd, large enough to hold longest cmd + leading and trailing delimiters
createSafeString(token, maxCmdLength + 1); // for parsing, capacity should be >= input
bool skipToDelimiter = false; // bool variable to hold the skipToDelimiter state across calls to readUntilToken()
// set skipToDelimiter = true to skip initial data upto first delimiter.

int led = 7; // new pin for led
// Pin 13 is used for the MAX31856 board
bool ledOn = false; // keep track of the led state
millisDelay ledDelay;
millisDelay printDelay;

bool stopTempReadings = true;
float tempReading = 0.0;
millisDelay max31856Delay;
const unsigned long MAX31856_DELAY_MS = 200; // max single shot conversion time is 185mS
bool readingStarted = false;


// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(9600);
  for (int i = 10; i > 0; i--) {
    Serial.println(i);
    delay(500);
  }
  bufferedOut.connect(Serial);  // connect bufferedOut to Serial
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  maxthermo.begin();
  maxthermo.setThermocoupleType(MAX31856_TCTYPE_K);

  ledDelay.start(1000); // start the ledDelay, toggle every 1000mS
  printDelay.start(5000); // start the printDelay, print every 5000mS
  Serial.print(F("To control Temp readings use commands ")); Serial.print(startCmd); Serial.print(" or ");  Serial.println(stopCmd);
}

// the task method
void blinkLed7(bool stop) {
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
void printTemp() {
  if (printDelay.justFinished()) {
    printDelay.repeat(); // start delay again without drift
    if (stopTempReadings) {
      bufferedOut.println(F("Temp reading stopped"));
    } else {
      bufferedOut.print(F("Temp:")); bufferedOut.println(tempReading);
    }
  } // else nothing to do this call just return, quickly
}

void handleStartCmd() {
  stopTempReadings = false; bufferedOut.terminateLastLine();
}
void handleStopCmd() {
  stopTempReadings = true; bufferedOut.terminateLastLine();
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

// return 0 if have new reading and no errors
// returns -1 if no new reading
// returns >0 if have errors
int readTemp() {
  if (!readingStarted) { // start one now
    maxthermo.oneShotTemperature();
    // start delay to pick up results
    max31856Delay.start(MAX31856_DELAY_MS);
  }
  if (max31856Delay.justFinished()) {
    readingStarted = false;
    // can pick up both results now
    tempReading = maxthermo.readThermocoupleTemperature();
    return 0; // new reading
  }
  return -1; // no new reading
}


// the loop function runs over and over again forever
void loop() {
  bufferedOut.nextByteOut(); // call this one or more times each loop() to release buffered chars
  loopTimer.check(bufferedOut);
  processUserInput();
  blinkLed7(stopTempReadings); // call the method to blink the led
  printTemp(); // print the temp
  if (!stopTempReadings) {
    int rtn = readTemp(); // check for errors here
  }
}
