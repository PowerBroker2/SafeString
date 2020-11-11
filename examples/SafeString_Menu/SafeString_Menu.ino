// SafeString_Menu.ino
//
// This example is an example of menu driven non-block commmands from the Arduino Monitor or a terminal input
// the available commands are start stop reset and setInc <num>
// The example works with and without NL/CR terminations and also processes backspaces from terminal inputs.
// Input cmds can be echoed or not.

// Commands are delimited by space comma NL or CR
//
// These commands can be picked out of a line of user input
// setEcho(true) will echo input, use setEcho(false) if entering cmds from a terminal that has local echo set.
//
//
// download and install the SafeString library from
// www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
#include "SafeString.h"
#include "millisDelay.h"

// menu methods ==========
// these are the only menu methods the sketch needs to know about
// if you split the menu off into a separate .cpp file,
// this is the header .h contents
// https://www.forward.com.au/pfod/ArduinoProgramming/Libraries/index.html
// for details on breaking up large arduino programs in to separate files
void setupProcessCmds();
void processCmds();
bool isMenuWaitingForCmd();
// ===========================


//===========================================
//  the sketch methods and variables used by the menu to control the program

bool running = false;
long counter = 0;
long counterInc = 1;

// the menu needs to access these methods to control the program
void setCounterInc(long inc) {
  counterInc = inc;
}

long getCounterInc() {
  return counterInc;
}

void resetCounter() {
  counter = 0;
}

long getCounter() {
  return counter;
}

void startCounter() {
  running = true;
}

void stopCounter() {
  running = false;
}

bool isCounterRunning() {
  return running;
}

// ===================================================================
//  the program being controlled

millisDelay printDelay;
unsigned long PRINT_DELAY_MS = 2000; // print counter every 2sec

void setup() {
  Serial.begin(9600);    // Open serial communications and wait a few seconds
  for (int i = 10; i > 0; i--) {
    Serial.print(' '); Serial.print(i);
    delay(500);
  }
  Serial.println();
  // first run without any outputting any error msgs or debugging
  //SafeString::setOutput(Serial); // enable error messages and debug() output to be sent to Serial

  setupProcessCmds(); // print initial menu
  setEcho(true);  // set to false if terminal has local echo set on
  printDelay.start(PRINT_DELAY_MS);
}


void loop() {

  processCmds();  // handle the users input

  // rest of code here is executed while the user typing in commands
  if (running) {
    counter += counterInc;
  }
  if (printDelay.justFinished()) {
    printDelay.repeat();
    if (isMenuWaitingForCmd()) {
      Serial.print(F("Loop Counter:")); Serial.println(counter);
    }
  }
}
// =============================================================


// ===========================================
// menu methods and variables
// if you split the menu off into a separate .cpp file,
// this is the .cpp contents

static char helpCmd[] = "?";
static char startCmd[] = "start";
static char stopCmd[] = "stop";
static char resetCmd[] = "reset";
static char incCmd[] = "setInc"; // set increment i.e.   setInc <number>

const size_t maxCmdLength = 15; // make SafeStrings at least large enough to hold longest cmd / arg
// input must be large enough to hold longest cmd + 1 delimiter
createSafeString(input, maxCmdLength + 1); //  to read input cmd + 1 delimiter
createSafeString(token, maxCmdLength + 1); // for parsing capacity >= input.capacity()
createSafeString(delimToken, maxCmdLength + 1); // for parsing capacity >= input.capacity()

static char delimiters[] = " ,\r\n"; // space comma CR NL are cmd delimiters
static char eolDelimiters[] = "\r\n"; // end of line delimiters are \r and,or \n

static bool skipToEndOfLine = false;
static bool echoInput = true;

typedef enum {WAITING_FOR_CMD, PROCESSING_INPUT, INC_CMD} cmdStateEnum;
static cmdStateEnum cmdState = WAITING_FOR_CMD;

static void displayUserMenu();

void setupProcessCmds() {
  cmdState = WAITING_FOR_CMD;
  input = "?\n";
}

void setEcho(bool on) {
  echoInput = on;
}

// use this to suppress output while waiting for arg input
bool isMenuWaitingForCmd() {
  return (cmdState == WAITING_FOR_CMD);
}

millisDelay inputTimeout;
unsigned long INPUT_TIMEOUT_MS = 2000; // 2sec

void processCmds() {
  if (input.read(Serial)) {  // read from Serial, returns true if at least one character was added to SafeString input
    //input.debug("after read => ");
    inputTimeout.start(INPUT_TIMEOUT_MS); // restart a 0.1sec timer every time something is read
    if (cmdState == WAITING_FOR_CMD) {
      cmdState = PROCESSING_INPUT;
    }
  }

  if (inputTimeout.justFinished()) { // nothing received for 0.1secs, terminated last chars so token will be processed.
    input += eolDelimiters; // add \r\n delimiter if nothing input for 2sec incase CR / NL is not set on input
    SafeString::Output.print(F("Input timed out"));
    if ((!isMenuWaitingForCmd()) && (cmdState != INC_CMD)) {
      cmdState = WAITING_FOR_CMD;
    }
  }

  if (skipToEndOfLine) {
    // look for \n or \r as first char in remaing input
    input.stoken(token, 0, eolDelimiters, false); // ignore return just pickup leading chars that are in eolDelimiters
    if (!token.isEmpty()) { // found leading chars from eolDelimiters
      skipToEndOfLine = false;
    } else {
      // skip over and ignore input that is not eol
      input.nextToken(token, eolDelimiters);
    }
    return;
  }

  if (input.nextToken(token, delimiters)) { // removes leading and trailing delimiters
    token.processBackspaces(); // handle terminal backspaces
    token.debug("after processBackspaces => ");
    if (echoInput) {
      // echo input
      Serial.print(token);
      input.stoken(delimToken, 0, delimiters, false); // just pickup delimiters and print them
      Serial.print(delimToken);
    }

    if (token.equals(helpCmd)) {
      displayUserMenu();
      skipToEndOfLine = true; // skip rest of input to newline
      cmdState = WAITING_FOR_CMD;
      return;
    }

    // here cmds are compared ignoring case so either upper or lower case inputs are OK
    if ((cmdState == WAITING_FOR_CMD) || (cmdState == PROCESSING_INPUT)) {
      if (token.equalsIgnoreCase(startCmd)) {
        startCounter();
        Serial.println(" Counter started ");
        cmdState = WAITING_FOR_CMD; // finished processing arg for this command

      } else if (token.equalsIgnoreCase(stopCmd)) {
        stopCounter();
        Serial.println(" Counter stopped ");
        cmdState = WAITING_FOR_CMD; // finished processing arg for this command

      } else if (token.equalsIgnoreCase(resetCmd)) {
        resetCounter();
        Serial.println(" Counter reset to 0 ");
        cmdState = WAITING_FOR_CMD; // finished processing arg for this command

      } else if (token.equalsIgnoreCase(incCmd)) {
        cmdState = INC_CMD; // expecting inc arg
        Serial.print(" Enter counter increment +/- : ");

      } else {
        Serial.println(" !!unexpected input!! -- Enter ? for list of valid cmds");
        cmdState = WAITING_FOR_CMD; // finished processing arg for this command
      }

    } else { // not waiting for cmd so expect value
      float num = 0.0;
      bool validFloat = token.toFloat(num); // allow floats here and truncate later
      if (!validFloat) {
        Serial.println(" !!invalid number!!");
      } else {
        switch (cmdState) {
          case INC_CMD:
            setCounterInc((long)num);
            Serial.print(" Counter increment set to : ");
            Serial.println(getCounterInc());
            break;
          default:
            Serial.println(" !!unexpected input!! -- Enter ? for list of valid cmds");
        }
      }
      cmdState = WAITING_FOR_CMD; // finished processing arg for this command
    }
  }
}


static void displayUserMenu() {
  Serial.println();
  Serial.println(F("Sample Control Menu "));
  Serial.println(F("   Enter cmds terminated separated by space or , or newline"));
  Serial.println(F("   You can enter complete commands on one line, e.g. setinc -50 "));
  Serial.println(F("   You can enter multiple commands on one line, e.g. stop setinc -50 start"));
  Serial.println(F("   Use ? to abandon a partial command"));
  Serial.println();
  Serial.print(F(" Counter is currently ")); Serial.println(isCounterRunning() ? "RUNNING" : "STOPPED");
  Serial.print(helpCmd);
  Serial.print(F(" => display this menu (skips rest of the input line)")); Serial.println();
  Serial.print(startCmd);
  Serial.print(F(" => start counter,  currently ")); Serial.println(getCounter());
  Serial.print(stopCmd);
  Serial.print(F(" => stop counter ")); Serial.println();
  Serial.print(resetCmd);
  Serial.print(F(" => reset counter to zero ")); Serial.println();
  Serial.print(incCmd);
  Serial.print(F(" => set counter increment +/-,  currently ")); Serial.println(getCounterInc());
  Serial.println();
}
