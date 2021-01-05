
#ifndef LOOP_TIMER_H
#define LOOP_TIMER_H
// loopTimer.h
/*
 * (c)2019 Forward Computing and Control Pty. Ltd.
 * NSW Australia, www.forward.com.au
 * This code is not warranted to be fit for any purpose. You may only use it at your own risk.
 * This generated code may be freely used for both private and commercial use
 * provided this copyright is maintained.
 */

#include <Arduino.h>

// download millisDelay from https://www.forward.com.au/pfod/ArduinoProgramming/TimingDelaysInArduino.html
#include <millisDelay.h>


class loopTimerClass {
  public:
    loopTimerClass(const char *_name = NULL); // name of this timer, if NULL then "loop" is used as the name
    void check(Print *out = NULL); // if loopTimer.check() called nothing is printed but timing done
    void check(Print &out); // if loopTimer.check() called nothing is printed but timing done
    void print(Print &out); // this prints the latest timings
    void print(Print *out); // this prints the latest timings
    void clear(); // clears all previous data

  private:
    void init();
    const char *name;
    bool initialized; // true after first call.
    unsigned long maxLoop5sec_uS; // max in last 5 sec
    unsigned long totalLoop5sec_uS; // total for last 5 sec
    unsigned long loopCount5sec;  // count in last 5 sec
    unsigned long lastLoopRun_uS; // start uS last call

    // print vars
    unsigned long p_avgLoop5sec_uS; // last calculated 5 sec average latency
    unsigned long p_maxLoop5sec_uS; // last max value in 5 sec

    unsigned long p_maxLoop_uS;  // max so far update every 5 sec
    unsigned long p_maxAvgLoop_uS; // max avg so far , updated every 5

    unsigned long PRINT_US_DELAY = 5000; // mS calculate and print every 5 sec
    millisDelay print_uS_Delay;
};

static loopTimerClass loopTimer;
#endif // LOOP_TIMER_H
