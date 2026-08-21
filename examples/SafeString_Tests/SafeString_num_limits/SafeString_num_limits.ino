// SafeString_num_limits.ino
//
// Boundary-value check for the SafeString number conversions after the errno fix.
// Every case below is a row of the behaviour table in the plan.
//
// Expected on a patched library: ALL PASS.
// On an unpatched upstream library (<= V4.1.43) the cases marked [was FAIL] fail.
//
// long is 32 bit on AVR, ESP32, ESP8266 and RP2040, so the 2147483647 / 4294967295
// literals below are the true limits on all of them.  int is 16 bit on AVR and 32 bit
// on the others, so the int narrowing case is selected on sizeof(int).

#include <SafeString.h>
#include <limits.h>   // for INT_MAX, used to select the toInt() narrowing case below

// Every limit value below assumes a 32 bit long, which is true on every Arduino core
// (AVR, SAMD, ESP32, ESP8266, RP2040, mbed).  If that ever stops being true, fail at compile
// time rather than report a screen of confusing FAILs.
static_assert(sizeof(long) == 4, "this sketch's limit values assume a 32 bit long");

int passCount = 0;
int failCount = 0;

/**
 * Report one test result.
 * @param name  - description of the case
 * @param ok    - true if the case behaved as expected
 */
void check(const char* name, bool ok) {
  if (ok) {
    passCount++;
    Serial.print(F("PASS  "));
  } else {
    failCount++;
    Serial.print(F("FAIL  "));
  }
  Serial.println(name);
}

/**
 * Expect a successful long conversion yielding an exact value.
 */
void expectLong(const char* name, const char* text, long expected) {
  cSF(sf, 32);
  sf = text;
  long v = 0;
  bool ok = sf.toLong(v);
  check(name, ok && (v == expected));
}

/**
 * Expect a long conversion to be rejected.
 */
void expectLongFail(const char* name, const char* text) {
  cSF(sf, 32);
  sf = text;
  long v = 0;
  check(name, !sf.toLong(v));
}

void setup() {
  Serial.begin(115200);
  for (int i = 10; i > 0; i--) {
    Serial.print(i); Serial.print(' ');
    delay(500);
  }
  Serial.println();
  SafeString::setOutput(Serial);

  Serial.println(F("--- limits that must now be ACCEPTED  [was FAIL upstream] ---"));
  expectLong("toLong LONG_MAX  2147483647", "2147483647", 2147483647L);
  expectLong("toLong LONG_MIN -2147483648", "-2147483648", -2147483648L);
  {
    cSF(sf, 32); sf = "4294967295";
    unsigned long v = 0;
    check("toUnsignedLong ULONG_MAX", sf.toUnsignedLong(v) && (v == 4294967295UL));
  }
  {
    cSF(sf, 32); sf = "7FFFFFFF";
    long v = 0;
    check("hexToLong 7FFFFFFF", sf.hexToLong(v) && (v == 2147483647L));
  }
  {
    cSF(sf, 32); sf = "FFFFFFFF";
    unsigned long v = 0;
    check("hexToUnsignedLong FFFFFFFF", sf.hexToUnsignedLong(v) && (v == 4294967295UL));
  }
  {
    cSF(sf, 40); sf = "1111111111111111111111111111111"; // 31 ones == LONG_MAX
    long v = 0;
    check("binToLong 31 ones", sf.binToLong(v) && (v == 2147483647L));
  }
  {
    cSF(sf, 32); sf = "17777777777"; // octal LONG_MAX
    long v = 0;
    check("octToLong 17777777777", sf.octToLong(v) && (v == 2147483647L));
  }
  {
    cSF(sf, 32); sf = "9223372036854775807";
    int64_t v = 0;
    check("toInt64_t INT64_MAX", sf.toInt64_t(v) && (v == (int64_t)9223372036854775807LL));
  }

  Serial.println(F("--- genuine overflows that must still be REJECTED ---"));
  expectLongFail("toLong 99999999999999", "99999999999999");
  expectLongFail("toLong -99999999999999", "-99999999999999");
  {
    cSF(sf, 32); sf = "99999999999999";
    unsigned long v = 0;
    check("toUnsignedLong overflow", !sf.toUnsignedLong(v));
  }
  {
    cSF(sf, 40); sf = "99999999999999999999999999";
    int64_t v = 0;
    check("toInt64_t overflow", !sf.toInt64_t(v));
  }
  {
    cSF(sf, 32); sf = "FFFFFFFFF"; // 9 hex digits, too big for 32 bit
    unsigned long v = 0;
    check("hexToUnsignedLong overflow", !sf.hexToUnsignedLong(v));
  }

  Serial.println(F("--- toInt: overflow must not be silently truncated  [was FAIL on 32 bit] ---"));
  {
    cSF(sf, 32); sf = "99999999999999";
    int v = 0;
    check("toInt overflow rejected", !sf.toInt(v));
  }
  {
    // narrowing long -> int.  Only rejected where int is narrower than long, i.e. AVR.
    cSF(sf, 32); sf = "100000";
    int v = 0;
    bool ok = sf.toInt(v);
    // NOTE: this has to be a preprocessor test, not if (sizeof(int) < sizeof(long)).
    // With a runtime test the compiler still compiles the dead branch, and on a 16 bit int board
    // (v == 100000) is outside the range of an int, which raises a -Wtype-limits
    // "comparison is always false" warning.
#if INT_MAX >= 100000
    check("toInt 100000 accepted (32 bit int)", ok && (v == 100000));
#else
    check("toInt 100000 rejected (16 bit int)", !ok);
#endif
  }
  {
    cSF(sf, 32); sf = "32767";
    int v = 0;
    check("toInt INT_MAX(16 bit) 32767", sf.toInt(v) && (v == 32767));
  }

  Serial.println(F("--- unchanged behaviour: these must still work as before ---"));
  expectLong("toLong plain 123", "123", 123L);
  expectLong("toLong  leading/trailing space ", "  123  ", 123L);
  expectLongFail("toLong trailing junk 5a", "5a");
  expectLongFail("toLong trailing dot 5.", "5.");
  expectLongFail("toLong empty", "");
  expectLongFail("toLong no digits abc", "abc");
  {
    cSF(sf, 32); sf = "3.75";
    double d = 0;
    check("toDouble 3.75 still ok", sf.toDouble(d) && (d > 3.74) && (d < 3.76));
  }
  {
    // out-parameter must be left untouched on failure
    cSF(sf, 32); sf = "notanumber";
    long v = 12345L;
    sf.toLong(v);
    check("failed conversion leaves arg unchanged", v == 12345L);
  }

  Serial.println();
  Serial.print(F("PASS: ")); Serial.print(passCount);
  Serial.print(F("   FAIL: ")); Serial.println(failCount);
  Serial.println(failCount == 0 ? F("ALL OK") : F("*** FAILURES ***"));
}

void loop() {
}
