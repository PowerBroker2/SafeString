// loopTimer.cpp
/*
 * (c)2019 Forward Computing and Control Pty. Ltd.
 * NSW Australia, www.forward.com.au
 * This code is not warranted to be fit for any purpose. You may only use it at your own risk.
 * This generated code may be freely used for both private and commercial use
 * provided this copyright is maintained.
 */
#include "loopTimer.h"

// name of this timer, if NULL then "loop" is used as the name
loopTimerClass::loopTimerClass(const char * _name) {
  initialized = false;
  name = _name;
}

void loopTimerClass::check(Print &out) {
	check(&out);
}

// if loopTimer.check() called nothing is printed each 5sec but timing done
void loopTimerClass::check(Print *out) {
  uint32_t uS = micros(); // stop timing <<<<<<<<<<<<<<<<
  if (initialized) {
    // not first time
    uint32_t d_uS = uS - lastLoopRun_uS;
    if (d_uS > maxLoop5sec_uS) {
      maxLoop5sec_uS = d_uS;
    }
    loopCount5sec++;
    totalLoop5sec_uS += d_uS;
  } else {
    init();
  }

  // every 5 sec do the calcs
  if (print_uS_Delay.justFinished()) {
    print_uS_Delay.restart(); // this may drift
    // calculate print vars
    p_avgLoop5sec_uS = totalLoop5sec_uS / loopCount5sec; // last calculated 5 sec average latency
    p_maxLoop5sec_uS = maxLoop5sec_uS; // last max value in 5 sec
    if (maxLoop5sec_uS > p_maxLoop_uS) {
      p_maxLoop_uS = maxLoop5sec_uS;  // max so far update every 5 sec
    }
    if (p_avgLoop5sec_uS > p_maxAvgLoop_uS) {
      p_maxAvgLoop_uS = p_avgLoop5sec_uS; // max avg so far , updated every 5
    }
    maxLoop5sec_uS = loopCount5sec = totalLoop5sec_uS = 0; // clear for next 5 sec
    print(out); // print results if out != NULL
  }

  // ignore time spent checking and printing above
  lastLoopRun_uS = micros(); // start timing again <<<<<<<<<<<<<<<<
}

void loopTimerClass::print(Print &out) {
	print(&out);
}

// this prints the latest timings
void loopTimerClass::print(Print * out) {
  if (out != NULL) {
  	unsigned long uS = micros();  
    if (name) {
      out->print(name);
    } else {
      out->print("loop");
    }
    out->println(" uS Latency");
    out->print(" 5sec max:"); out->print(p_maxLoop5sec_uS); out->print(" avg:"); out->print(p_avgLoop5sec_uS);
    out->println();
    out->print(" sofar max:"); out->print(p_maxLoop_uS); out->print(" avg:"); out->print(p_maxAvgLoop_uS);
    out->print(" max - prt:");
    // skip the print time
    out->println((micros() - uS));
    lastLoopRun_uS += (micros() - uS) ; // move start of last loop in by this amount
  }
}

void loopTimerClass::clear() {
	initialized = false;
}

void loopTimerClass::init() {
  // initialize
  maxLoop5sec_uS = 0; // max in last 5 sec
  totalLoop5sec_uS = 0; // total for last 5 sec
  loopCount5sec = 0;  // count in last 5 sec
  lastLoopRun_uS = 0; // start uS last call
  // print vars
  p_avgLoop5sec_uS = 0; // last calculated 5 sec average latency
  p_maxLoop5sec_uS = 0; // last max value in 5 sec
  p_maxLoop_uS = 0;  // max so far update every 5 sec
  p_maxAvgLoop_uS = 0; // max avg so far , updated every 5
  print_uS_Delay.start(PRINT_US_DELAY);
  initialized = true;
}
