/*
   PinFlasher.cpp
   by Matthew Ford,  2021/12/06
   (c)2021 Forward Computing and Control Pty. Ltd.
   NSW, Australia  www.forward.com.au
   This code may be freely used for both private and commerical use.
   Provide this copyright is maintained.
*/
#include "PinFlasher.h"
/** 
 *  PIN_ON is a 'magic' number that turns the output ON when setOnOff(PIN_ON) called
 */
const int PIN_ON = -1; 
/** 
 *  PIN_OFF is a 'magic' number that turns the output OFF when setOnOff(PIN_ON) called
 */
const int PIN_OFF = 0; 
/**
   Constructor.
   if pin != 0 it is initally set to output and OFF<br>
   @param pin -- the pin number to flash, default 0 (not set)<br>
   @param invert -- true to make pin LOW for on, false (default) to make pin HIGH for on.
*/
PinFlasher::PinFlasher(int pin, bool invert) {
  outputInverted = invert;
  setPin(pin);
}

/**
   check if output should be changed now.
   update() should be called often, atleast every loop()
*/
void PinFlasher::update() {
  if (!isRunning()) {
    return;
  }
  if (justFinished()) {
    if (half_period == PIN_OFF) {  // should not happen
      io_pin_on = false; // stay off
      stop(); // stop flash timer
    } else if (half_period == (unsigned long)(PIN_ON)) { // should not happen
      io_pin_on = true;       // stay on
      stop(); // stop flash timer
    } else { //restart flash
      restart(); // slips time
      io_pin_on = !io_pin_on;
    }
    setOutput(); // off does nothing if io_pin =0
  }
}

/**
   Set the output pin to flash.
   Call setOnOff( ) to start flashing, after calling setPin()<br>
   If pinNo changes, stop any current flashing, else ignore this call<br>
   @param pin -- the pin number to flash, default 0 (not set)<br>
*/
void PinFlasher::setPin(int pin) {
  if (io_pin == pin) {
    return;
  }
  // else pin changed re-init
  io_pin = pin;
  if (io_pin < 0) {
    io_pin = 0;
  }
  stop(); // stop flash timer
  half_period = PIN_OFF; // off
  io_pin_on = false;
  if (io_pin) {
    pinMode(io_pin, OUTPUT);
  }
  setOutput();
}

/**
    Set the On and Off length, period is twice this setting.
    @param onOff_ms -- ms for on and also for off, i.e. half the period, duty cycle 50%<br>
    PIN_OFF (0) turns off the output<br>
    PIN_ON (-1) turns the output on <br>
    other values turn the output on for that length of time and then off for the same time
    */
void PinFlasher::setOnOff(unsigned long onOff_ms) {
  half_period = onOff_ms;
  if (half_period == PIN_OFF) { // stay off
    io_pin_on = false;
    stop(); // stop flash timer
  } else if (half_period == (unsigned long)(PIN_ON)) {  // stay on
    io_pin_on = true;
    stop(); // stop flash timer
  } else { //restart flash
    io_pin_on = true;
    if (io_pin) { // if have a pin
      start(half_period);  // restart
    }
  }
  setOutput();
}


/**
    Normally pin output is LOW for off, HIGH for on.
    This inverts the current setting for on/off<br>
    @return -- the current setting, true if on == LOW, false if on == HIGH<br>
    e.g. <br>
    PinFlasher f(2,true);  // pin 2, inverted, i.e. On is LOW, off is HIGH<br>
    f.setOnOff(100); // set flash 1/10sec on then 1/10sec off<br>
    ...<br>
    f.setOnOff(PIN_ON); // set output on, i.e. HIGH<br>
    f.invertOutput(); // now pin 2 still on but now is LOW,<br>
    ...<br>
    f.setOnOff(PIN_OFF);  // set output OFF, i.e. HIGH because of invertOutput above
*/
bool PinFlasher::invertOutput() {
  outputInverted = !outputInverted;
  setOutput();
  return outputInverted;
}

/**
   set the output based on io_pin, io_pin_on and outputInverted
*/
void PinFlasher::setOutput() { // uses io_pin_on global
  if (!io_pin) {
    return;
  }
  if (io_pin_on) {
    if (!outputInverted) {
      digitalWrite(io_pin, HIGH); // on
    } else {
      digitalWrite(io_pin, LOW); // on inverted
    }
  } else { // off
    if (!outputInverted) {
      digitalWrite(io_pin, LOW); // off
    } else {
      digitalWrite(io_pin, HIGH); // off inverted
    }
  }
}
