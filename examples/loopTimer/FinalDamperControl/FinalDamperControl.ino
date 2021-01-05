// FinalDamperControl.ino
// has extra calls to runStepper() in the loop().  
// on UNO runStepper() called every 1mS i.e. max 1000 steps/sec consistently

/*
   (c)2020 Forward Computing and Control Pty. Ltd.
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
#include <AccelStepper.h>

#include <MAX31856_noDelay.h>
// Use software SPI: CS, DI, DO, CLK
MAX31856_noDelay maxthermo = MAX31856_noDelay(10, 11, 12, 13);
// use hardware SPI, just pass in the CS pin
//MAX31856_noDelay maxthermo = MAX31856_noDelay(10);

createBufferedOutput(bufferedOut, 80, DROP_UNTIL_EMPTY);

char closeCmd[] = "close"; // will compare in lower case
char runCmd[] = "run"; // will compare in lower case
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

float tempReading = 0.0; // from readTemp
float simulatedTempReading = 0.0; // from user input
bool closeDampler = true;

millisDelay max31856Delay;
const unsigned long MAX31856_DELAY_MS = 200; // max single shot conversion time is 185mS
bool readingStarted = false;

AccelStepper stepper; // Defaults to AccelStepper::FULL4WIRE (4 pins) on 2, 3, 4, 5

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(9600);
  for (int i = 10; i > 0; i--) {
    Serial.println(i);
    delay(500);
  }
  bufferedOut.connect(Serial);  // connect nonBlocking to a buffered stream to Serial

  //initialize digital pin led as an output.
  pinMode(led, OUTPUT);

  maxthermo.begin();
  maxthermo.setThermocoupleType(MAX31856_TCTYPE_K);

  ledDelay.start(1000); // start the ledDelay, toggle every 1000mS
  printDelay.start(5000); // start the printDelay, print every 5000mS
  Serial.print(F("Enter simulated temperature, 0 to 100, or "));
  Serial.print(runCmd); Serial.print(F(" to start damper control or "));
  Serial.print(closeCmd); Serial.println(F(" to close the damper."));

  stepper.setMaxSpeed(1000);
  stepper.setSpeed(500); // need to call atleast every 2mS
  stepper.setAcceleration(50);
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
  runStepper(); // <<<< extra call here
    bufferedOut.print(F("Temp:")); bufferedOut.println(simulatedTempReading);
  runStepper(); // <<<< extra call here
    bufferedOut.print(F("Position current:")); bufferedOut.print(stepper.currentPosition());
  runStepper(); // <<<< extra call here
    if (closeDampler) {
      bufferedOut.println(F(" Close Damper"));
    } else {
      bufferedOut.println(F(" Damper running"));
    }
  runStepper(); // <<<< extra call here
  } // else nothing to do this call just return, quickly
}

// task to get the user's cmds, input commands terminated by space or , or \r or \n or no new characters for 2secs
// set Global variable with input cmd
void processUserInput() {
  if (input.readUntilToken(bufferedOut, token, delimiters, skipToDelimiter, true, 2000)) { // echo input and 2000mS timeout, non-blocking!!
    token.toLowerCase(); // make all lower case
    if (token ==  closeCmd) {
      closeDampler = true;
    } else if (token ==  runCmd) {
      closeDampler = false;
    } else { //try and convert as temp
      float newSimulatedTempReading = simulatedTempReading;
      if (!token.toFloat(newSimulatedTempReading)) {
        // conversion failed,  newSimulatedTempReading unchanged
        bufferedOut.print(F(" -- Invalid SimulatedTemp or ")); bufferedOut.print(closeCmd); bufferedOut.print(F(" or ")); bufferedOut.print(runCmd); bufferedOut.println(F(" cmds."));
      } else { // have valid float, check range
        if ((newSimulatedTempReading < 0.0) || (newSimulatedTempReading > 100.0)) {
          bufferedOut.print(F(" -- Invalid SimulatedTemp must be between 0.0 and 100.0 ")); bufferedOut.println();
        } else {
          simulatedTempReading = newSimulatedTempReading; // update it
        }
      }
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

void setDamperPosition() {
  if (closeDampler) {
    stepper.moveTo(0);
  } else {
    long stepPosition = simulatedTempReading * 50;
    stepper.moveTo(stepPosition);
  }
}

void runStepper() {
  loopTimer.check(bufferedOut); // moved here from loop()
  stepper.run();
}

// the loop function runs over and over again forever
void loop() {
  bufferedOut.nextByteOut(); // call this one or more times each loop() to release buffered chars
  processUserInput();
  blinkLed7(closeDampler); // call the method to blink the led
  printTemp(); // print the temp
  int rtn = readTemp(); // check for errors here
  setDamperPosition();
  runStepper();
}
