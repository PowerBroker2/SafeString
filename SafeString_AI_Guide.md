# SafeString — AI Reference Guide for Safe C-String Handling

**Library:** SafeString v4.1.45 by Matthew Ford, Forward Computing and Control Pty. Ltd.
**Sources:** `SafeString/src/*.h`, `SafeString/src/*.cpp`, `SafeString/examples/**`,
<https://www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html>,
<https://www.forward.com.au/pfod/ArduinoProgramming/SafeString/docs/html/index.html>

**Purpose of this document:** a complete, unambiguous specification an AI coding assistant can follow to
write Arduino/embedded C++ that manipulates text **without buffer overruns, without heap fragmentation,
and without silent truncation**. Every rule below is derived from the library source, not inferred.

---

## 1. The core idea

`SafeString` is **not** a string class that owns memory. It is a **bounds-checked wrapper around a
fixed-size `char[]`**. The `char[]` lives wherever you declare it (global, stack, class member, or an
existing buffer you already have). SafeString adds a length, a capacity, and an error flag, and it
checks every operation against that capacity.

Three guarantees, in the library's own terms:

1. **Safe** — once created, a SafeString is *always* in a valid, usable, `'\0'`-terminated state, even if
   you pass it `NULL`, a `'\0'`, or data that exceeds its capacity. It never reboots the board.
2. **Fast / deterministic** — no `malloc`, no `realloc`, no heap fragmentation, no hidden copies
   (SafeStrings are passed by reference only).
3. **Debuggable** — every rejected operation sets an error flag and (if enabled) prints a detailed
   message naming the variable, its capacity, its length, its contents, and the offending argument.

### The all-or-nothing rule (most important behavioural rule)

> If an operation would exceed capacity, **nothing is written**. The SafeString is left exactly as it
> was, the error flag is set on it and on the global class flag, and an error message is printed if
> output is enabled.

There is **no silent truncation**. This is the opposite of `strncpy`/`snprintf`, which truncate quietly
and (for `strncpy`) may leave the buffer unterminated.

### Two different jobs — do not confuse them

SafeString is designed to do two things:

**(a) Prevent crashes caused by coding errors**, and **(b) give you detailed debugging of those coding
errors.** The error flag and the printed error messages exist for (b). They mean *"the program you wrote
is wrong — fix the code"*: an under-sized buffer, an index past the end, a `NULL` pointer, a `cSFP` of an
empty string.

**Bad input data is not a coding error.** A user who types 30 characters into a field that accepts 15, or
types `12x.5` where a number was expected, has not exposed a bug in your program. That is *expected*
traffic on a system boundary, and your code is supposed to detect it, reject it, and tell the user. It
must do so **without raising a SafeString error**, because:

- a SafeString error message on the console says "you have a bug", which is misleading and noisy when the
  real story is "the operator mistyped";
- `hasError()` and `errorDetected()` are sticky-until-read *global-ish* diagnostics — burning them on
  routine bad input destroys their value for catching real bugs;
- the all-or-nothing rule means an over-long assignment leaves you with **nothing**, which is rarely what
  you want when you are trying to report what the user actually sent.

SafeString provides a separate set of tools for input handling that **never set the error flag** —
`readFrom()`, `readUntilToken()`, `SafeStringReader`, `toInt()`/`toFloat()`/`toLong()` and friends.
Use those at every boundary. See **§4.8**, which is the single most commonly mis-applied part of this
library.

| | Coding error | Bad input data |
|---|---|---|
| Cause | your program is wrong | the outside world sent something unexpected |
| Fix | change the code | reject it and report it to the sender |
| Detected by | `hasError()`, `errorDetected()`, printed messages | your own checks — `readFrom()` + `isFull()`, `toInt()`, `readUntilToken()`, `SafeStringReader` |
| Should reach production? | no — you fix it during development | yes, constantly |
| Sets the SafeString error flag? | **yes** | **no** |

---

## 2. Installation, includes, namespace

```cpp
#include <SafeString.h>          // core class
#include <SafeStringReader.h>    // non-blocking tokenised Stream input (includes SafeString.h)
#include <SafeStringStream.h>    // fake Stream for automated testing
#include <BufferedOutput.h>      // non-blocking print
#include <BufferedInput.h>       // extra RX buffering
#include <SerialComs.h>          // Arduino<->Arduino messaging
#include <millisDelay.h>         // non-blocking delay
#include <loopTimer.h>           // loop() timing statistics
#include <PinFlasher.h>          // non-blocking pin flashing
```

Available in the Arduino Library Manager as **SafeString**. PlatformIO users take one of the two zips in
`SafeString/PlatformIO/` — `SafeStringIO_namespace.zip` if the board core declares `namespace arduino {`
in `Print.h`, otherwise `SafeStringIO.zip`.

**Sparkfun SAMD compile failure** (`error: expected class-name before '{' token ... class SafeStringReader
: public SafeString`): empty out the three files `SafeStringNameSpace.h`, `SafeStringNameSpaceStart.h`,
`SafeStringNameSpaceEnd.h`.

---

## 3. Creating SafeStrings — the four macros

**There are no public constructors you should call directly.** The copy constructor is private, and there
are no conversion constructors. Always use one of these four macros (each has a short alias).

| Macro | Alias | Wraps | Resulting capacity |
|---|---|---|---|
| `createSafeString(name, size)` <br> `createSafeString(name, size, "init")` | `cSF` | a `char[size+1]` it declares for you | `size` |
| `createSafeStringFromCharArray(name, charArray)` | `cSFA` | an existing `char[N]` | `N - 1` |
| `createSafeStringFromCharPtr(name, charPtr)` | `cSFP` | an existing `char*` | `strlen(charPtr)` **at creation**, cannot grow |
| `createSafeStringFromCharPtrWithSize(name, charPtr, arraySize)` | `cSFPS` | an existing `char*` with known array size | `arraySize - 1` |

Capacity **excludes** the terminating `'\0'`, which is always accounted for separately.

### 3.1 `cSF` — new storage

```cpp
createSafeString(msg, 40);              // char msg_SAFEBUFFER[41]; capacity 40, empty
cSF(greeting, 20, "hello");             // capacity 20, contents "hello"
```

Expands to:

```cpp
char msg_SAFEBUFFER[40+1];
SafeString msg(sizeof(msg_SAFEBUFFER), msg_SAFEBUFFER, "", "msg");
```

The buffer has the **storage duration of where the macro is written**: file scope → global; inside a
function → stack. SafeStrings created this way are *never* invalid, whatever arguments you throw at them.

### 3.2 `cSFA` — wrap a `char[]` you already have

```cpp
char buffer[15];
cSFA(sfBuf, buffer);        // capacity 14
sfBuf = "hello";            // buffer now holds "hello"
```

This is the **primary tool for making existing C code safe**: you keep your `char[]` (and any code that
reads it, prints it, or DMAs into it) and gain checked mutation.

> **Gotcha:** `cSFA` verifies it was given a real array by checking `sizeof(arg) != sizeof(char*)`. On a
> board where `sizeof(char*) == 4`, passing a genuine `char[4]` trips this check and raises an error. Use
> `char[5]` or larger, or switch to `cSFPS` with an explicit size.

### 3.3 `cSFP` — wrap a `char*` (capacity = current `strlen`)

```cpp
char charArray[20] = "initial value";
char* arrayPtr = charArray;
cSFP(sfPtr, arrayPtr);      // capacity == strlen("initial value") == 13, NOT 19
```

Because a `char*` carries no size information, the only length SafeString can trust is the current
`strlen()`. **The capacity can never be increased.** You may shrink, replace, substring, and rewrite the
contents up to 13 chars — you can never append beyond them.

Failure modes handled without crashing:
- `NULL` pointer → valid SafeString with capacity 0, **error flagged**.
- Empty string (`""`) → valid SafeString with capacity 0, and **no error is raised** — the constructor
  simply takes `strlen() == 0` as the capacity. The result is valid but useless: every append fails.
  This is the quiet one; `errorDetected()` will not catch it, so never `cSFP` a buffer that might be
  empty at that moment.
- **Unterminated** source `char[]` → `strlen()` runs into neighbouring memory and the capacity comes out
  too large. SafeString *cannot* detect this. Prefer `cSFA` or `cSFPS` whenever the true size is known.

### 3.4 `cSFPS` — wrap a `char*` with an explicit array size

```cpp
char charBuffer[15];
char* bufPtr = charBuffer;
cSFPS(sfBuf, bufPtr, 15);   // capacity 14 (size-1 for the '\0')
```

Use this for pointers into structs, `malloc`ed regions, `char[][N]` rows accessed through a pointer, and
anywhere the buffer size is known but the type has decayed to `char*`. Passing `0` for `arraySize` raises
an error and yields a capacity-0 SafeString.

> **The sentinel convention — why `cSFP` alone uses `strlen`.** All four macros call one constructor
> whose `maxLen` is normally the **array size**, so `capacity = maxLen - 1`. Two values are sentinels,
> and they do *not* mean the same thing:
>
> | `maxLen` | from a `char*` (`cSFP`/`cSFPS`) | from a `char[]` (`cSF`/`cSFA`) |
> |---|---|---|
> | `(unsigned int)-1` | `capacity = strlen(buf)` — **the only path that calls `strlen`** | error, capacity 0 |
> | `0` | error, capacity 0 — `cSFPS(name, ptr, 0)` | error, capacity 0 |
> | anything else | `capacity = maxLen - 1` | `capacity = maxLen - 1` |
>
> `0` does **not** mean "use `strlen`". Through v4.1.43 the header comment said it did
> (`if maxLen == -1 Or maxLen == 0, then capacity == strlen(char*)`) and the `@param maxLen` line
> described the capacity rather than the array size; both are corrected in this copy.
>
> `0` is rejected up front rather than left to fail later, because `maxLen` is unsigned and the normal
> path is `_capacity = maxLen - 1` — `0 - 1` underflows to `SIZE_MAX`, and the SafeString would believe
> it owns a huge buffer. Three routes reach it: `cSFPS(name, ptr, 0)` (the usual one),
> `cSFA(name, x)` where `x` is a **zero-length array** `char x[0]`, and `cSF(name, -1)`. `cSFP` cannot —
> it always passes `-1`.
>
> On the zero-length array: `char x[0]` is ill-formed in standard C++, but GCC allows it as an
> extension with `sizeof(x) == 0`, and the Arduino cores compile with `-std=gnu++…`, so it is live.
> Verified with `static_assert(sizeof(x) == 0)` on both `arduino:avr:uno` and `esp32:esp32`.

### 3.5 Choosing between them

```
Need new storage?                        → cSF
Have a real char[N] in scope?            → cSFA        (best: size is exact and compiler-checked)
Have char* + know the buffer size?       → cSFPS       (next best)
Have char* + only know it's terminated?  → cSFP        (last resort: capacity frozen at strlen)
```

### 3.6 Lifetime rules

- `cSF` SafeStrings are always valid.
- `cSFA` / `cSFP` / `cSFPS` SafeStrings are valid **exactly as long as the wrapped buffer is valid**. The
  usual way to break this is wrapping a buffer inside a `malloc`/`calloc`ed struct and then `free`ing it
  while the SafeString is still in use.
- A `cSF`/`cSFA` written inside a function wraps stack memory. **Never** return a reference or a
  `c_str()` pointer to it.

---

## 4. Error handling — how to know your *code* is wrong

Everything in §4.1–§4.7 is a **development-time debugging facility**. It tells you that the program is
defective. §4.8 covers the separate — and much more frequently needed — job of validating input data,
which must not go through this machinery at all.

### 4.1 What a SafeString error means

The error flag is raised when an operation is impossible *as written*. Every one of these is a bug in the
source code, fixable by editing the source code:

| Trigger | The coding mistake |
|---|---|
| assignment / `concat` / `prefix` does not fit | the buffer was declared too small for what the program itself builds |
| `charAt(i)` / `setCharAt(i, c)` with `i >= length()` | index arithmetic is wrong |
| `substring()` with `beginIdx > length()` | index arithmetic is wrong |
| `result` SafeString too small in `substring()` / `stoken()` | destination declared smaller than the source |
| `cSFP` of a `NULL` pointer | pointer never initialised |
| `cSFPS(name, ptr, 0)` / `cSFA` of a `char[0]` | nonsensical size argument |
| `concat('\0')`, `setCharAt(i, '\0')`, `write(0)` | storing a terminator inside a string |
| `cleanUp()` finds `buffer[capacity] != '\0'` | some other code overran the buffer |
| negative `decs` in `print(d, decs, width)` | bad formatting argument |

Note what is **absent** from that list: "the user typed too much", "the field was not a number", "the
line was longer than the buffer". Those are data conditions, not coding conditions, and the library
deliberately gives you no-error ways to handle them (§4.8).

The correct response to a SafeString error during development is to **fix the code** — usually by
enlarging a buffer or correcting an index — not to write a runtime handler for it. Production code
generally should not be able to raise one at all.

### 4.2 Turn on messages

```cpp
void setup() {
  Serial.begin(9600);
  SafeString::setOutput(Serial);        // enables error messages AND debug() output; verbose by default
}
```

| Call | Effect |
|---|---|
| `SafeString::setOutput(Print& out, bool verbose = true)` | route all messages to `out` |
| `SafeString::setVerbose(bool)` | `true` = detailed messages, `false` = compact |
| `SafeString::turnOutputOff()` | silence all messages; **errors are still detected and flagged** |

Without `setOutput()`, nothing is printed — but the flags below still work.

### 4.3 Check the flags

| Call | Scope | Note |
|---|---|---|
| `sfStr.hasError()` | this SafeString | **reading clears the flag** |
| `SafeString::errorDetected()` | any SafeString anywhere | static; **reading clears the flag** |

Both are "sticky until read". Their intended use is a development-time assertion — *did this program do
anything impossible?* — not a per-message input check:

```cpp
buildReport(sfReport);                  // internal composition, sized by me
if (sfReport.hasError()) {
  // I under-sized sfReport. This is a bug: enlarge the buffer.
  Serial.println(F("BUG: sfReport too small"));
}
```

> **Do not use `hasError()` to test incoming data.** `sfBuf = untrustedText;` followed by
> `if (sfBuf.hasError())` is the classic misuse: it prints a bug report for what is merely a long input,
> it leaves `sfBuf` empty so you cannot show the user what they sent, and it consumes the flag that was
> supposed to be reserved for real defects. §4.8 gives the correct constructs.

Because reading clears the flag, `errorDetected()` is best used as a periodic "has anything gone wrong
anywhere?" sweep — but only if nothing else in the program is deliberately generating errors. Once input
handling starts setting the flag, this sweep becomes worthless.

### 4.4 A real error message

```
Error: msgStr.concat() needs capacity of 8 for the first 3 chars of the input.
        Input arg was '598'
        msgStr cap:5 len:5 'A0 = '
```

The variable name (`msgStr`) comes from the `#name` stringification inside the creation macros.

### 4.5 Inspect a SafeString at runtime

```cpp
sfStr.debug();                          // full dump: name, capacity, length, contents
sfStr.debug("after parse: ");           // with a title (const char*, F(".."), or SafeString)
sfStr.debug(false);                     // suppress contents
Serial.println(sfStr.debug());          // debug() returns "" so this composes cleanly
```

`debug()` is always compiled in and is *not* affected by `setVerbose()`; it still needs `setOutput()` to
have somewhere to print.

### 4.6 Globals constructed before `setup()`

Global SafeStrings and classes containing them are constructed before `setOutput()` can run, so their
construction errors cannot print. Test for them explicitly:

```cpp
void setup() {
  Serial.begin(115200);
  SafeString::setOutput(Serial);
  if (SafeString::errorDetected()) {
    Serial.println(F("Error constructing global SafeStrings!"));
    // to see the message, temporarily move the construction inside setup()
  }
}
```

### 4.7 Compile-time error control

`SafeString.h` defines `SSTRING_DEBUG`. Commenting it out removes all error-message code **and** the RAM
holding SafeString names — smaller binary, no diagnostics. Normally leave it defined and control output
at runtime with `setOutput()` / `turnOutputOff()`.

### 4.8 Validating and sanitising input — the no-error toolkit

> **Rule: nothing arriving from outside the program may be allowed to raise a SafeString error.**

"Outside the program" means a `Stream`, a serial port, a network socket, a pfod/app command, an EEPROM or
SD field, a sensor's text response — anything whose length and content you do not control. At every such
boundary use the constructs below. Each one handles over-long or malformed data by *returning a value you
test*, leaving the error flag untouched and free to do its real job.

| Problem at the boundary | Correct, no-error construct | How you detect the bad data |
|---|---|---|
| text may be longer than the buffer | `sf.clear(); sf.readFrom(ptr);` (§8.1) | size `sf` one larger than the longest valid input, then `sf.isFull()` |
| a `Stream` delivering delimited commands | `SafeStringReader` (§9) | `read()` returns false and `longTokenDiscarded()` is true; the reader holds the leading chars of the rejected input |
| the same, without the reader wrapper | `readUntilToken()` (§7.3) | returns false with `skipToDelimiter` just risen false → true; `token` holds the leading chars of the rejected input |
| copying from a bigger SafeString | `sfDst.clear(); sfDst.readFrom(sfSrc);` (§8.1) | returned index `< sfSrc.length()` means it did not all fit |
| the field should be a number | `toInt()` / `toLong()` / `toFloat()` / `toDouble()` / `hexToLong()` … (§6.10) | the `false` return; the output variable is left untouched |
| the value parsed but is out of range | your own `if (v < lo || v > hi)` | your own comparison |
| the text should match a known command | `equals()` / `equalsIgnoreCase()` / `startsWith()` (§6.4, §6.5) | the `false` return |
| stray case or terminal backspaces | `toLowerCase()`, `processBackspaces()`, `trim()` | n/a — these sanitise, they cannot fail on data |

By contrast, these **do** raise an error on over-long data and are therefore wrong at a boundary:
`=`, `+=`, `-=`, `concat()`, `prefix()`, `print()`, and `substring()`/`stoken()` into an under-sized
result. Reserve them for text your own code composes, where "it did not fit" genuinely is a bug.

Note that "the input has a known upper bound" is **not** a reason to skip the check. A pfod message is
capped at 255 bytes by `pfodParser`, so the text handed to a touch handler can never be unbounded — but
255 is still vastly longer than a 15-character field, so the field-length check is exactly as necessary
as if the input were unlimited. A bound that is larger than valid buys you nothing.

#### The `readFrom()` + `isFull()` idiom — checking length without an error

`readFrom(const char*)` copies as much as fits and stops. It never complains, so on its own it cannot
tell you whether it stopped because the input ended or because the buffer filled. The standard trick is
to give the SafeString **one character more than the longest valid input**, so a full buffer can only
mean "too long":

```cpp
const size_t MAX_NAME = 15;             // the longest name we accept
char nameBuf[MAX_NAME + 2];             // +1 for the overflow detector, +1 for the '\0'
cSFA(sfName, nameBuf);                  // capacity == MAX_NAME + 1

sfName.clear();
sfName.readFrom(untrustedText);         // copies at most MAX_NAME+1 chars, never errors
if (sfName.isFull()) {                  // MAX_NAME+1 chars fitted, so the input was at least that long
  reportToUser(F("Name too long"));     // bad data — report it, do not flag a bug
  return;
}
// sfName now holds the complete input, known to be MAX_NAME chars or fewer
```

`isFull()` is `length() == capacity()`. Sizing the buffer at exactly `MAX_NAME` would make a valid
15-character name indistinguishable from a truncated 40-character one — hence the extra char.

A second benefit over `=`: `readFrom()` leaves you holding the first `MAX_NAME+1` characters of what was
sent, so the rejection message can quote the offending text. A failed `=` leaves the SafeString empty.

> **Order matters: test `isFull()` before anything shortens the SafeString.** `trim()`, `remove()`,
> `processBackspaces()`, `replace()` with a shorter replacement and `nextToken()` all drop the length
> below capacity and so destroy the only evidence that the input overflowed. `isFull()` is meaningful
> *immediately* after the `readFrom()` and nowhere else. If you need the answer later, save it first:
> `bool tooLong = sfName.isFull();`
>
> The residual imprecision is deliberate, and errs on the safe side: `MAX_NAME` characters followed by
> trailing spaces also fills the buffer and is rejected as too long. If that matters, give the buffer
> enough spare room for the whitespace you are prepared to accept, then `trim()` and test
> `sfName.length() > MAX_NAME` instead — but do not also rely on `isFull()` in that version.

The same idea works when the source is a SafeString, using the returned index:

```cpp
sfDst.clear();
if (sfDst.readFrom(sfSrc) < sfSrc.length()) {   // did not all fit
  reportToUser(F("Too long"));
}
```

#### `trim()` is for presentation, not for parsing

Every `toInt()` / `toLong()` / `toUnsignedLong()` / `toInt64_t()` / `toFloat()` / `toDouble()` and the
bin/oct/hex variants already accept leading **and** trailing whitespace: the underlying `strtol`/`strtod`
skips leading whitespace, and the library then walks the remaining characters accepting anything
`isspace()` and rejecting everything else (§6.10). So `" 12.5 \r\n"` parses; `"12.5x"` does not. Adding a
`trim()` to make a number parse is unnecessary.

The legitimate reasons to `trim()` are:

- **the text is going to be echoed back** — quoting raw input in a message reproduces the user's padding,
  so `"'12x   ' is not a number"` instead of `"'12x' is not a number"`;
- **the text is compared** — `equals("STOP")` and `startsWith()` do *not* ignore whitespace, so a
  trailing `\r` from a terminal causes a mismatch.

This distinction matters because `trim()` is exactly the kind of shortening call that invalidates a
pending `isFull()` test. Knowing *why* you are trimming tells you where it belongs: after the length
check, and on the display/compare path rather than the parse path.

#### Worked contrast — the same handler done wrongly and correctly

A pfod drawing hands a touched-text command to the sketch as a `const byte*` — bounded at 255 bytes by
`pfodParser`, but far longer than the 15 characters this field accepts. It is input, so it must be
handled without SafeString errors.

**Wrong** — this is the pattern to recognise and avoid:

```cpp
cSFA(sfInput, inputText);
sfInput = (const char*)editedText;   // ERROR + console bug report if it does not fit
if (sfInput.hasError()) {            // reading it here also steals the flag from real bugs
  // ... and sfInput is now EMPTY, so we cannot even show what the user sent
}
```

Three separate faults: it prints a "your code is broken" diagnostic for an ordinary typing mistake; it
consumes the sticky error flag that `errorDetected()` relies on; and the all-or-nothing rule leaves
`sfInput` empty just when you wanted to echo the offending text back.

**Right** — `inputText` is declared one char longer than the longest acceptable entry, the error message
is cleared once up front, and the length test is an ordinary `if`:

```cpp
// inputText is char[SLIDER_INPUT_TEXT_SIZE + 2] — one spare char detects over-length input.
// ERR_MSG_TEXT_SIZE allows for that spare char too, since setInputErrMsg() quotes inputText.
bool SliderInputErrorControl::Dwg_SliderInputErr_cmd_sliderValue(int row, int col,
                                    uint8_t touchType, const byte* editedText) {
  (void)row;  (void)col;  (void)touchType;             // suppress warnings
  cSFA(sfInput, inputText);
  sfInput.clear();
  sfInput.readFrom((const char*)editedText);           // takes what fits, never raises an error

  clearErrMsg();                       // blank unless one of the checks below sets a message

  if (sfInput.isFull()) {              // SLIDER_INPUT_TEXT_SIZE+1 chars fitted, so it was too long
    setInputErrMsg(F("is too long"));  // bad data, reported to the user — not a bug
  } else {
    float value;
    if (parseInput(value)) {           // parseInput() sets the Err Msg label itself if it returns false
      setPosition(value);
    }
  }
  sendUpdate();                        // updates the bar, the labels and the hidden label
  return true;                         // handled here, only this dwg needs updating
}
```

Clearing the message first makes the invariant plain — *the label is blank unless a check found
something wrong* — so the accepted path needs no code at all, instead of the original arrangement where
only the success branch cleared and every failure path had to remember to set.

`parseInput()` shows the same separation one level down. It does call `sfInput.trim()`, but for the
presentational reason above, not to help `toFloat()`:

```cpp
bool SliderInputErrorControl::parseInput(float& result) {
  cSFA(sfInput, inputText);
  sfInput.trim();                      // so the quoted text in the error message has no padding
  float value;
  if (!sfInput.toFloat(value)) {       // false, not an error flag — "12x" is bad data
    setInputErrMsg(F("is not a number"));
    return false;
  }
  if ((value < MIN_POSITION) || (value > MAX_POSITION)) {   // range check is the caller's job, not
    setInputErrMsg(F("must be 0 to 100"));                  //   something any library call can do
    return false;
  }
  result = value;                      // only written on success
  return true;
}
```

Three checks — length, format, range — none of which touches `hasError()`. The `trim()` is safe here
precisely because it runs *after* the caller's `isFull()` test. If a SafeString error *does* appear on
the console while this runs, it now means something it never meant before: a genuine bug worth chasing.

#### Where the boundary is

Sanitise **once**, at the point the data enters, then treat it as trusted:

```
Stream / app / file  ──►  readFrom()          ──►  isFull() length check  ──►  parse / range-check
                          or SafeStringReader                                          │
                                                            everything downstream is now known-good,
                                                            so `=` and `+=` are fine there
```

A common mistake is to sanitise at the boundary and then, three functions later, assign the *raw*
original into a small buffer anyway. Pass the validated SafeString on, not the original pointer.

---

## 5. Mapping unsafe C-string idioms to SafeString

This is the practical core of the library. `sf` denotes a SafeString.

| Unsafe C | Why it's unsafe | SafeString replacement |
|---|---|---|
| `strcpy(dst, src)` | no bound at all | `sfDst = src;` |
| `strncpy(dst, src, n)` | may leave `dst` unterminated; truncates silently | trusted `src`: `sfDst = src;` then check `hasError()`. Untrusted `src`: `sfDst.clear(); sfDst.readFrom(src);` then check `isFull()` (§4.8) |
| `strcat(dst, src)` | no bound | `sfDst += src;` / `sfDst.concat(src);` |
| `String c = a + b;` (Arduino `String`) | heap allocation + fragmentation, silent failure | `(sfC = a) += b;` — there is no `operator+`, see §6.3 |
| `strncat(dst, src, n)` | easy off-by-one on the size argument | `sfDst.concat(src, n);` — **stricter**: raises an error and appends nothing if `strlen(src) < n`, where `strncat` would stop at the `'\0'` |
| `sprintf(buf, "%d", v)` | unbounded | `sf = v;` or `sf.print(v);` |
| `snprintf(buf, n, ...)` | truncates silently | `sf.clear(); sf.print(a); sf += b;` + `hasError()` (composition you control, so a misfit is a bug) |
| `strlen(s)` | walks off unterminated buffers | `sf.length()` (O(1), stored) |
| `strcmp(a,b) == 0` | crashes on `NULL` | `sfA == "b"` / `sfA.equals(b)` |
| `strcasecmp` | non-portable | `sfA.equalsIgnoreCase(b)` |
| `strstr(h, n)` | pointer arithmetic on the result | `int i = sf.indexOf("n");` (`-1` if absent) |
| `strchr` / `strrchr` | ditto | `sf.indexOf('c')` / `sf.lastIndexOf('c')` |
| `strpbrk` | obscure | `sf.indexOfCharFrom("abc")` |
| `strtok(s, d)` | destroys input, static state, not reentrant | `sf.stoken(...)` (non-destructive) or `sf.nextToken(...)` (destructive, explicit) |
| `atoi` / `atol` | `0` means both "zero" and "invalid" | `if (sf.toInt(i)) { … }` — `i` only touched on success |
| `atof` | same ambiguity | `if (sf.toFloat(f)) { … }` |
| `strtol(s, NULL, 16)` | error signalling via `errno` | `sf.hexToLong(l)` |
| `buf[i] = c` | no bound check | `sf.setCharAt(i, c)` |
| `c = buf[i]` | reads past the end | `char c = sf.charAt(i);` / `sf[i]` (returns `0` + error if out of range) |
| `Serial.readBytesUntil(...)` | **blocks** the loop | `SafeStringReader` (§9) |

**Prohibited content:** `'\0'` can never be stored in a SafeString. `write(0)`, `concat('\0')` and
`setCharAt(i, '\0')` are rejected and raise an error.

**Read this table with §4.8 in mind.** The replacements above are written for the common case where the
source text is your own — a literal, a computed value, another buffer you sized. Where the source is
external, swap the assigning forms (`=`, `+=`, `print`) for `readFrom()` or a `SafeStringReader`, so that
over-long data produces a value you test rather than an error you have to interpret.

---

## 6. Complete API reference

`sf`, `sfOther`, `token`, `result` are SafeStrings. Return type `SafeString&` means the call returns
`*this` so calls can be cascaded: `sf.clear().concat("a").concat(1);`

### Notation used in the signatures below

The signature listings in §6–§8 are **descriptions of overload sets, not code to copy**. Two conventions
appear in them:

| Notation | Means | Example | Expands to |
|---|---|---|---|
| `a / b / c` | **one of** these argument types — separate overloads | `sf.indexOf(char / "str" / sfOther)` | `sf.indexOf('x');` **or** `sf.indexOf("xy");` **or** `sf.indexOf(sfOther);` |
| `arg = value` | that parameter's **default**, so it may be omitted | `sf.indexOf(…, fromIndex = 0)` | `sf.indexOf('x');` is `sf.indexOf('x', 0);` |

The `/` is read as "or", the same way `-1 / 0 / +1` describes the possible return values in §6.4. It is
separator notation only — it never appears in real code, and it is not a division.

So `sf.startsWith(c / "str" / sfOther, fromIndex = 0)` describes six real calls — three argument types,
each with and without `fromIndex`.

### 6.1 Size and state

| Method | Returns | Notes |
|---|---|---|
| `length()` | `unsigned int` | chars excluding `'\0'` |
| `capacity()` | `unsigned int` | max chars excluding `'\0'` |
| `isEmpty()` | `unsigned char` | non-zero if length 0 |
| `isFull()` | `unsigned char` | non-zero if length == capacity |
| `availableForWrite()` | `int` | `capacity() - length()` |
| `reserve(size)` | `unsigned char` | 0 if `capacity < size`; a **check**, never a reallocation |
| `clear()` | `SafeString&` | empties |
| `c_str()` | `const char*` | valid, terminated. **Do not cast away the `const`.** |

### 6.2 Assignment (`=` clears then sets)

Overloads for: `char`, `unsigned char`, `int`, `unsigned int`, `long`, `unsigned long`, `int64_t`,
`float`, `double`, `const char*`, `const __FlashStringHelper*` (i.e. `F("..")`), `SafeString&`.

If the value is `NULL`, invalid, or too large, **the SafeString is left empty** and an error is raised.
(Note: assignment differs from concat/prefix here — a failed `=` empties, a failed `+=` leaves the
previous contents intact.)

### 6.3 Append and prepend

```cpp
sf.concat(x);            sf += x;      // append   — same overload set as '='
sf.prefix(x);            sf -= x;      // prepend  — same overload set
sf.concat(cstr, len);                  // append at most len chars; ERROR if strlen(cstr) < len
sf.concat(F("txt"), len);
sf.newline();                          // append "\r\n"
```

**There is no `operator+`.** `a + b` would require constructing a temporary SafeString, which the library
forbids by design. Every assignment and append returns `SafeString&`, so the replacement for `+` is to
parenthesise and chain onto that reference:

| Instead of | Write |
|---|---|
| `sf = a + b;` | `(sf = a) += b;` |
| `sf = a + b + c;` | `((sf = a) += b) += c;` |
| `sf += 'a' + 5;` | `(sf += 'a') += 5;` |

The parentheses are required, not stylistic: `sf = a` yields `sf` itself, and `+= b` then appends to it.

> **Each step is checked separately, and the chain does not stop at the first failure.** Because a failed
> `=` empties the SafeString while a failed `+=` leaves it untouched (§6.2), `(sf = a) += b` with an
> over-long `a` leaves `sf` holding **just `b`** — not empty, and not `a + b`. Worth knowing when you are
> reading a puzzling result on the bench, but not something to guard at runtime: `sf` being too small for
> what your own code builds is a coding error, fixed by sizing `sf` for the worst case (rule 4, §26).

The longer form is often clearer and is exactly equivalent:

```cpp
sf = a;
sf += b;
sf += c;
```

`sf` is also a `Print`, so every `print`/`println` overload appends to it:

```cpp
sf.print(3.14159, 2);            // "3.14"
sf.print(255, HEX);              // "FF"
sf.println();                    // "\r\n"
sf.print(value, decs, width, forceSign);   // fixed-width, see below
```

**Fixed-width formatting** — `print(double d, int decs, int width, bool forceSign = false)` and the
matching `println(...)`:
- `width > 0` pads on the left (right-justified), `width < 0` pads on the right.
- `decs` is capped at **7** (`if (decs > 7) decs = 7;`) and is then **reduced automatically** to make the
  value fit `abs(width)`. 
- A negative `decs` raises an error.
- If it still won't fit with `decs == 0`, an error is raised.
- Pass `decs = 0` to format `int`/`long` values.
- Special values are **not** errors — they print as the literal text `nan`, `inf`, `ovf` or `-ovf`
  (`ovf`/`-ovf` at `d >= 4294967039.0` / `d <= -4294967039.0`, threshold empirically).
   Nothing sets the error flag, so a `nan` reaches your output silently.

### 6.4 Comparison

`compareTo(SafeString& / const char*)` → `-1` / `0` / `+1`.
`equals(SafeString& / const char* / char)`, `equalsIgnoreCase(...)`,
`equalsConstantTime(SafeString&)` (timing-attack-resistant, for comparing secrets).
Operators `==`, `!=`, `<`, `>`, `<=`, `>=` against `SafeString&` and `const char*` (plus `==`/`!=` against
`char`). All return `unsigned char`.

### 6.5 Prefix / suffix tests

```cpp
sf.startsWith(c / "str" / sfOther, fromIndex = 0);
sf.startsWithIgnoreCase(c / "str" / sfOther, fromIndex = 0);
sf.endsWith(c / "str" / sfOther);
sf.endsWithCharFrom("\r\n");    // true if the last char is any one of these
```

`fromIndex > length()` raises an error and returns false. `fromIndex == length()` or `-1` returns false
without error.

### 6.6 Searching — all return `int`, `-1` when not found

```cpp
sf.indexOf(char / "str" / sfOther, fromIndex = 0);
sf.lastIndexOf(char / "str" / sfOther);              // searches backwards from the end
sf.lastIndexOf(char / "str" / sfOther, fromIndex);   // backwards from fromIndex, inclusive
sf.indexOfCharFrom("abc", fromIndex = 0);            // first occurrence of ANY of these chars
```

`fromIndex > length()` → error + `-1`. `fromIndex == length()` or `(unsigned int)-1` → `-1`, no error.

### 6.7 Character access

```cpp
char c = sf.charAt(i);     // i >= length() → returns 0 and raises an error
char c = sf[i];            // identical
sf.setCharAt(i, c);        // i >= length() → error, no change; c == '\0' → error, no change
```

`sf[i] = c` is **deliberately not supported** — it would hand out a writable reference into the buffer.

### 6.8 Substrings — `endIdx` is EXCLUSIVE

```cpp
sf.substring(result, beginIdx);              // beginIdx .. end
sf.substring(result, beginIdx, endIdx);      // beginIdx .. endIdx-1
sf.substring(sf, 3);                         // in-place is allowed
```

- `result` is **always cleared first**, so it is empty on any error.
- `beginIdx == length()` or `-1` → empty result, no error. `beginIdx > length()` → error.
- `endIdx > length()` → clamped to `length()`, error raised. `endIdx == -1` → treated as `length()`, no error.
- `beginIdx > endIdx` → swapped, error raised.
- If `result` lacks capacity → empty result, error on **both** SafeStrings.

> Migration note: in SafeString V1 `endIdx` was inclusive. From V2 onward it is exclusive.

### 6.9 Modification

```cpp
sf.replace(findChar, replaceChar);
sf.replace(findChar, "replacement" / sfReplace);
sf.replace("find", "replacement");
sf.replace(sfFind, sfReplace);

sf.remove(index);                 // index .. end
sf.remove(index, count);          // count chars from index; excess count is clamped + error
sf.removeFrom(startIndex);        // startIndex .. end  (inclusive)
sf.removeBefore(startIndex);      // 0 .. startIndex-1  (startIndex char survives)
sf.removeLast(count);             // count > length() → error
sf.keepLast(count);               // count > length() → error, unchanged; count == 0 clears

sf.toLowerCase();
sf.toUpperCase();
sf.trim();                        // strips isspace() from BOTH ends: ' ' \t \n \v \f \r
sf.processBackspaces();           // recursively removes '\b' and the char before it (terminal input)
```

`(unsigned int)-1` is accepted as an index everywhere and is treated as `length()` without error.

### 6.10 Number parsing — strict, and never ambiguous

Every conversion returns non-zero on success, `0` on failure, and **only writes the output parameter on
success**. Leading and trailing whitespace is permitted; any other trailing character is a failure.
This is stricter than Arduino `String::toInt()`, which returns `0` for both `"0"` and `"abc"`.

**None of these ever set the SafeString error flag.** A field that is not a number is bad *data*, not a
coding error, so the failure is reported solely through the return value — which is precisely why these
are the right tools at an input boundary (§4.8). Because whitespace is already tolerated at both ends,
there is no need to `trim()` first; see §4.8 for why an unnecessary `trim()` can be actively harmful.

```cpp
int i;            if (sf.toInt(i))              { … }
int64_t ll;       if (sf.toInt64_t(ll))         { … }   // e.g. time_t
long l;           if (sf.toLong(l))             { … }
unsigned long ul; if (sf.toUnsignedLong(ul))    { … }
float f;          if (sf.toFloat(f))            { … }   // decimal notation, not scientific
double d;         if (sf.toDouble(d))           { … }

sf.binToLong(l);   sf.octToLong(l);   sf.hexToLong(l);
sf.binToUnsignedLong(ul); sf.octToUnsignedLong(ul); sf.hexToUnsignedLong(ul);
```

Canonical validating-parse pattern:

```cpp
int value;
if (!sfField.toInt(value)) {
  Serial.print(F("Not a valid integer: ")); Serial.println(sfField.c_str());
  return;
}
```

### 6.11 UTF-8

**SafeString is byte-oriented.** `length()`, `capacity()`, `charAt()`, `indexOf()` and every index are
counts of **bytes**, not characters. A `°` costs 2 bytes and an emoji 4, so size buffers by bytes and
never assume one index is one character.

That is safe because UTF-8 was designed for it: every byte of a multi-byte character is ≥ 0x80, so no
part of one can be mistaken for an ASCII character, and `'\0'` — the one value a SafeString cannot hold
(§5) — never appears inside a sequence.

```cpp
int i = sf.utf8index(endIdx);       // largest index <= endIdx that does not split a code point
int j = sf.utf8nextIndex(startIdx); // start of the next code point (startIdx+1 .. startIdx+4)

sf.substring(result, 0, sf.utf8index(endIdx));                     // safe truncation
sf.substring(result, sf.utf8index(idx), sf.utf8nextIndex(idx));    // extract one code point
```

For pure ASCII these reduce to `endIdx` and `idx+1`. `utf8index`: `endIdx > length()` → clamped + error;
`-1` → treated as `length()`, no error. `utf8nextIndex`: `startIdx > length()` → `(unsigned int)-1` +
error; `startIdx == length()` or `-1` → `(unsigned int)-1`, no error.

#### What is and is not UTF-8 safe

| Operation | With non-ASCII text |
|---|---|
| Storing, `=`, `+=`, `substring()`, `readFrom()` | **byte-transparent** — bytes in, same bytes out |
| Tokenising (§7), `SafeStringReader`, `SerialComs` | **safe** — delimiters are ASCII, and no UTF-8 byte can be mistaken for one |
| `trim()`, `toInt()` / `toFloat()` / … | **safe** — a UTF-8 byte is not whitespace, so `trim()` stops at it and the parsers reject it |
| Slicing at an arbitrary index | use `utf8index()` / `utf8nextIndex()` above, or you will split a character |
| `toLowerCase()`, `toUpperCase()`, `equalsIgnoreCase()` | **ASCII-only.** Bytes ≥ 0x80 pass through unchanged, so `"ÉCOLE"` becomes `"École"` — the ASCII letters fold, the `É` does not. Nothing is corrupted; nothing non-ASCII is folded either |
| `length()` as a character count | **wrong** — it is bytes |

Proper Unicode case folding needs code-point tables the library deliberately does not carry, so the
ASCII-only behaviour of the last row is a design limit rather than a defect.


---

## 7. Tokenising

Three mechanisms with genuinely different semantics. Pick deliberately — note in particular **what each
one tokenises**, since only the third touches a `Stream` at all:

| Method | Tokenises | Effect on it | Typical use |
|---|---|---|---|
| `stoken()` (§7.1) | a SafeString | unchanged | text already fully in memory, walked by index |
| `nextToken()` (§7.2) | a SafeString | **consumes** the token, freeing space | a SafeString used as a rolling buffer you refill yourself |
| `readUntilToken()` (§7.3) | a `Stream`, via an internal SafeString | manages it for you | reading commands as they arrive |

The first two never read anything. If the text is arriving from a Stream, *you* put it into the
SafeString first, with `read()` / `readUntil()` (§8.2) — or use `readUntilToken()`, which does both.

The prototypes below use the §6 notation: `a / b / c` marks **one of** several overloads, and
`arg = value` marks a default.

### 7.1 `stoken()` — non-destructive, index-driven

The source SafeString is **unchanged**; you walk it with the returned index. The end of the SafeString is
*always* a delimiter, so the final unterminated token is always returned.

```cpp
int stoken(SafeString& token, unsigned int fromIndex,
           char delimiter / const char* delimiters / SafeString& delimiters,
           bool returnEmptyFields = false,
           bool useAsDelimiters = true);
```

Returns the next index to pass in, or `-1` after the last token.

```cpp
cSF(sfLine, 60, "one,two,,three");
cSF(sfTok, 60);                      // capacity >= sfLine's capacity
int idx = 0;
while (idx >= 0) {
  idx = sfLine.stoken(sfTok, idx, ",");
  if (!sfTok.isEmpty()) { /* use sfTok */ }
}
```

- `token` is cleared at the start of every call.
- `returnEmptyFields = true` returns an empty token for each consecutive delimiter (CSV semantics).
- `useAsDelimiters = false` inverts the character set: the token consists *only* of chars from
  `delimiters` and any other char terminates it.
- If `token` lacks capacity, it comes back empty with an error on both SafeStrings — **but the returned
  index still advances**, so loops cannot hang.

### 7.2 `nextToken()` / `firstToken()` — destructive, frees space

Called on a SafeString, it removes the returned token (and leading delimiters) **from that SafeString**.
No Stream is involved. Freeing that space is what makes a fixed-size SafeString usable as a rolling
buffer: consume the tokens it already holds with `nextToken()`, then top it up from a Stream with
`read()` or `readUntil()` (§8.2), and repeat. `stoken()` cannot be used this way — it leaves the source
unchanged, so the buffer only ever fills up.

```cpp
unsigned char nextToken(SafeString& token,
                        char delimiter / const char* delimiters / SafeString& delimiters,
                        bool returnEmptyFields = false,
                        bool returnLastNonDelimitedToken = true,
                        bool firstToken = false);

unsigned char firstToken(SafeString& token, delimiters, bool returnLastNonDelimitedToken = true);
```

- The **delimiter itself is left in the source**, so you can test which one ended the token — check it
  *before* the next call, which strips leading delimiters.
- `returnLastNonDelimitedToken = true` (the default since V4.0.4) returns a trailing unterminated token.
  Set it to **`false` when the SafeString is being topped up from a Stream**, so a partial token stays in
  the SafeString until the rest of it, and its delimiter, have been read in. Leave it `true` only when
  the SafeString already holds the whole text to be tokenised.
- `firstToken()` exists so a leading delimiter can yield an empty first field; only use it for the first
  call, because the previous delimiter is still sitting at the front afterwards.
#### When the token does not fit `token`

If the next token is longer than `token`'s capacity, `nextToken()` / `firstToken()` do **all** of this:

| | What happens |
|---|---|
| Return value | **`true`** — same as success, *not* false |
| `token` | **empty** (it is cleared on entry and never written) |
| The source | the token **is still consumed** — `remove(0, token_count)`; the following delimiters stay |
| Error flag | set on **both** the source SafeString and `token` |
| Message | `Error: sfLine.nextToken() token SafeString sfTok needs capacity of N for token '…'`, if `setOutput()` is on |

Consuming the token even though it could not be delivered is deliberate: the loop always advances, so a
token that is too big can never hang it.  

Once `token` is sized correctly, a `true` return with an empty token has a single meaning — an **empty
field** between two delimiters, which you only ever see with `returnEmptyFields = true`. With the
default `returnEmptyFields = false` all leading delimiters are stripped before the token is measured, so
a returned token is never empty at all.  

If you give the `token` a capacity **≥ the source SafeString's capacity**, a token can
never be too long.

### 7.3 `readUntilToken()` — non-blocking, straight from a Stream

This is **safe on untrusted input**: no length or
content of valid data can make it raise an error.

```cpp
bool skipToDelimiter = false;
cSF(sfInput, 32);
cSF(sfToken, 32);                          // capacity MUST be >= sfInput's capacity

bool wasSkipping = skipToDelimiter;        // remember before the call
if (sfInput.readUntilToken(Serial, sfToken, ",\r\n", skipToDelimiter, false /*echo*/, 0 /*timeout ms*/)) {
  // sfToken holds one complete token, delimiter not included.
  // An EMPTY sfToken here means an empty field, i.e. two successive delimiters — nothing else.
} else if (!wasSkipping && skipToDelimiter) {
  // input was longer than sfInput's capacity; it is being discarded up to the next delimiter.
  // sfToken holds the leading chars of it, so you can show the sender what was rejected.
}
```

- Non-blocking: returns immediately when there is nothing to read.
- `sfInput.capacity()` must be **at least one more than the largest expected token**. Longer runs of
  chars are discarded up to the next delimiter — **no error is raised**, because over-long input is
  data, not a coding error (§4.8).
- `sfToken.capacity()` must be **≥ `sfInput.capacity()`**. This is checked once on entry and *is* a
  coding error if violated, since it is a fault in your declarations rather than in the data. Meeting it
  guarantees any token found fits, which is what makes `true` + empty token mean "empty field" and
  nothing else.
- `skipToDelimiter` is a `bool&` holding state between calls — set it true yourself to discard
  everything up to the next delimiter. Its **rising edge** (false → true across a call) is how you
  detect over-long input.
- `timeout_ms != 0` returns the buffered text as a token when input goes quiet.
- Prefer `SafeStringReader` (§9), which wraps all of this and exposes the rising edge as
  `longTokenDiscarded()`.

Complete return-state table:

| Outcome | returns | `token` | `skipToDelimiter` |
|---|---|---|---|
| delimited token found | `true` | the token | unchanged |
| empty field — two successive delimiters | `true` | empty | unchanged |
| **input too long, discarding** | `false` | its leading chars | false → **true** |
| discard finished at the next delimiter | `false` | empty | true → **false** |
| timeout while discarding | `false` | empty | true → **false** |
| timeout, pending chars flushed as a token | `true` | the token | unchanged |
| nothing available yet | `false` | empty | unchanged |

---

## 8. Reading and writing without a heap

### 8.1 SafeString ↔ SafeString / `char*`

```cpp
unsigned int readFrom(SafeString& sfInput, unsigned int startIdx = 0);   // returns new startIdx
unsigned int readFrom(const char* strPtr, unsigned int maxCharsToRead = (unsigned int)-1); // returns chars read
unsigned int writeTo(SafeString& output, unsigned int startIdx = 0);     // returns new startIdx
```

`readFrom` is the safe way to pull from an oversized `char*` — it copies as much as fits and stops,
**without raising a capacity error**, unlike `=` or `+=`:

```cpp
sfSmall.clear();
sfSmall.readFrom(someLongCharPtr);       // fills to capacity, no error, no overrun
```

This makes `readFrom()` **the standard entry point for untrusted text** (§4.8). Two things to remember:

- It **appends**; it does not clear first. Call `clear()` before it unless you are deliberately
  accumulating. `readFrom(const char*)` also stops at the first `'\0'` or after `maxCharsToRead` chars.
- Its return value alone cannot distinguish "the input ended" from "I ran out of room". For the `char*`
  overload, size the SafeString one char larger than the longest valid input and test `isFull()`
  immediately afterwards; for the SafeString overload, compare the returned index against
  `sfInput.length()`.

```cpp
sfSmall.clear();
sfSmall.readFrom(someLongCharPtr);
if (sfSmall.isFull()) { /* input was longer than we accept — report it, no error flag involved */ }
```

Use these to move data between buffers of mismatched size in chunks.

### 8.2 Non-blocking Stream reads

```cpp
unsigned char read(Stream& input);                              // read whatever is available now
unsigned char readUntil(Stream& input, char / const char* / SafeString& delimiters);
size_t getLastReadCount();                                      // chars read by the last read* call
```

`read()` returns false immediately if nothing is available or the SafeString is full. `readUntil()`
returns true when a delimiter is read (the delimiter **is** appended) or the SafeString fills; at most
one delimiter is consumed per call.

---

## 9. SafeStringReader — the non-blocking input workhorse

For reading commands, CSV, GPS sentences, or any delimited text from a `Stream` without ever blocking
`loop()`.

```cpp
createSafeStringReader(name, size, delimiters,
                       skipToDelimiterFlag = false,
                       echoInput = false,
                       timeout_ms = 0);
```

`size` is the maximum token length excluding the delimiter. Tokens longer than `size` are **discarded**
along with everything up to the next delimiter — the input line itself may be arbitrarily long. The macro
declares both buffers and the internal input SafeString for you.

Discarding an over-long token raises **no error and prints nothing** — it is bad input data, not a coding
error (§4.8). `read()` returns false, and `longTokenDiscarded()` distinguishes that from the ordinary
"no complete token yet" false. While it is true the reader holds the **leading characters** of the
rejected input, so you can quote them back at whoever sent them:

```cpp
if (sfReader.read()) {
  handleCommand(sfReader);                 // delimited token found (may be empty)
} else if (sfReader.longTokenDiscarded()) {
  Serial.print(F("Input too long, starts with '"));
  Serial.print(sfReader);                  // SafeStringReader is Printable
  Serial.println('\'');
} else {
  // no complete token yet, carry on with the rest of loop()
}
```

`longTokenDiscarded()` is cleared at the start of every `read()`, so it refers to the most recent call
and is true for exactly one `read()` per over-long token. This is the **only** case where `read()`
returns false with a non-empty reader.


```cpp
#include "SafeStringReader.h"

createSafeStringReader(sfReader, 5, " ,\r\n");   // commands up to 5 chars
bool running = true;

void setup() {
  Serial.begin(9600);
  SafeString::setOutput(Serial);
  sfReader.connect(Serial);
  sfReader.echoOn();
}

void loop() {
  if (sfReader.read()) {                 // true only when a complete token has arrived
    if (sfReader == "start")     { running = true; }
    else if (sfReader == "stop") { running = false; }
    else { Serial.print(F("unknown command: ")); Serial.println(sfReader); }
  } else if (sfReader.longTokenDiscarded()) {
    // the user typed more than 5 chars, so it was discarded. Not an error, just bad input,
    // and sfReader still holds its leading chars so they can be echoed back.
    Serial.print(F("command too long: ")); Serial.println(sfReader);
  }
  // the rest of loop() keeps running at full speed while the user types
}
```

Both the `else` and the `else if` matter. Without them a mistyped or over-long command is silently
ignored and the user is left wondering — and since v4.1.45 the library prints nothing of its own for
over-long input (§4.8), so if you do not report it, nobody does.

`SafeStringReader` **is a** `SafeString`, so every method above (`==`, `toInt`, `startsWith`, …) works on
the token directly.

| Method | Purpose |
|---|---|
| `connect(Stream&)` | choose the input stream; clears the read count |
| `read()` | non-blocking; `true` when a delimited token is available. Always clears itself first |
| `longTokenDiscarded()` | `true` if *this* `read()` found and discarded an input longer than `size`. The reader then holds its leading chars. No error is raised. Cleared at the start of every `read()` |
| `getDelimiter()` | `int` delimiter that ended the token; `-1` on timeout or error. Valid only right after `read()` returns true |
| `echoOn()` / `echoOff()` | echo received chars back to the stream (default off) |
| `setTimeout(ms)` | return buffered text as a token after `ms` of silence (default 0 = never) |
| `returnEmptyTokens(bool = true)` | emit an empty token per delimiter instead of collapsing runs |
| `flushInput()` | discard buffered input **and** the Stream RX buffer, then skip to the next delimiter |
| `skipToDelimiter()` | discard the token currently being assembled |
| `isSkippingToDelimiter()` | true while discarding. **Not** a reliable overflow detector — use `longTokenDiscarded()` |
| `getReadCount()` | chars read since `connect()` — useful for HTTP `Content-Length` |
| `end()` | return any final token, flush the input buffer, disconnect, clear the read count. **Does not** reset echo or the timeout, so both carry over to the next `connect()`. Set them explicitly if you want them back at their defaults |
| `debugInputBuffer(...)` | dump the partially-assembled input buffer |

Flush idiom:

```cpp
sfReader.setTimeout(1000);
sfReader.flushInput();
sfReader.setTimeout(0);
```

Note that one `read()` call transfers at most `size + 1` characters from the stream, so a small reader
drains a port slowly. See **§19** if input is arriving in bursts or faster than that.

`setTimeout()` terminates an unfinished *token*; for "no command has arrived at all for N seconds" use a
`millisDelay` instead — see **§13.3**.

---

## 10. Passing SafeStrings around

### 10.1 Function parameters must be `SafeString&`

The copy constructor is **private**, specifically so that passing by value fails to compile:

```cpp
void process(SafeString& sfIn);         // CORRECT
void process(SafeString sfIn);          // compile error, by design
```

The actual compiler output is the private-copy-constructor error, **not** a custom message:

```
error: 'SafeString::SafeString(const SafeString&)' is private within this context
note: declared private here
```

The `note` points at `SafeString.h`, where the declaration carries the explanatory comment
`// You must declare SafeStrings function arguments as a reference, SafeString&`. So if you see
"`is private within this context`" anywhere near a SafeString, the cause is a by-value SafeString —
a parameter, a return type, or an attempt to copy one.

This also means passing a SafeString is free — no copy, ever.

### 10.2 You cannot return a SafeString

Return by value needs a copy constructor; returning a reference to a local returns a dangling stack
buffer. Pass the destination in instead:

```cpp
void buildStatus(int count, SafeString& sfOut) {   // caller owns the buffer
  sfOut = "loop count = ";
  sfOut += count;
}

createSafeString(sfStatus, 40);
buildStatus(n, sfStatus);
```

### 10.3 SafeStrings as class members

You cannot use `cSF` for a member (it declares two things). Declare a `char[]` member and wrap it with
`cSFA` inside each method that needs to modify it:

```cpp
class Loco {
  private:
    char _niceName[12];
    char _locoName[12];
  public:
    Loco(const char* niceName, const char* locoName) {
      cSFA(sfNiceName, _niceName);      // wrapper is a cheap local; the data lives in the member
      cSFA(sfLocoName, _locoName);
      sfNiceName = niceName;            // length-checked
      sfLocoName = locoName;
    }
};
```

The wrapper object is a few bytes on the stack and disappears at the end of the method; the `char[]` is
the real storage.

### 10.4 Arrays of C strings

```cpp
char arr[][40] = { "first", "second" };
cSFA(sfRow, arr[0]);                    // capacity 39, picked up from the declaration
sfRow += " more";

const char* ptrs[] = { "alpha", "beta" };
cSFP(sfPtr, (char*)ptrs[0]);            // capacity == strlen("alpha") == 5
```

> **Warning:** string literals in a `const char*[]` are read-only and the compiler is free to merge
> identical literals. Mutating one through a SafeString can change what several array elements appear to
> contain, and writing to genuinely read-only memory is undefined. Use `char[][N]` when you intend to
> modify.

---

## 11. Interoperating with existing C code

This is why `cSFA` / `cSFP` / `cSFPS` exist: keep the buffer, add checking.

```cpp
char rxBuffer[64];                     // filled by an ISR, a library, DMA, whatever
cSFA(sfRx, rxBuffer);                  // wrap it once

sfRx.trim();
if (sfRx.startsWith("AT+")) { … }
Serial.println(rxBuffer);              // the original C code still works unchanged
```

**Every SafeString method on a wrapped buffer first runs `cleanUp()`, which:**

1. Checks that `buffer[capacity] == '\0'`. If external code overwrote it, **the error flag is set** and
   (with output enabled) `"SafeString cleanUp detected buffer overrun by external code."` is printed.
2. Re-terminates the buffer at `capacity` regardless — containing the damage.
3. Recomputes `len = strlen(buffer)` so the SafeString re-synchronises with whatever the C code wrote.

So mixing is *survivable* and *detectable*:

```cpp
cSFP(sfPtr, arrayPtr);
strcat(arrayPtr, "123");        // unsafe C write, done behind SafeString's back
sfPtr.endsWith("123");          // this call cleans up, re-terminates at capacity, resyncs length
```

Nevertheless: **do not routinely mix `strcat`/`strcpy` with SafeString on the same buffer.** SafeString
can only detect the overrun after the fact — the write has already happened, and if it ran far enough it
may already have corrupted adjacent memory. Wrap once and use SafeString methods exclusively.

Reading the data back out:

```cpp
const char* p = sf.c_str();      // always valid and terminated; treat as read-only
Serial.println(sf);              // SafeString implements Printable
Serial.println(sf.c_str());      // equivalent
```

---

## 12. Companion classes (brief)

For which of these can be combined with which — including the combinations to avoid — see the full
matrix in **§25**.

| Class | Creation | Purpose |
|---|---|---|
| `SafeStringReader` | `createSafeStringReader(name, size, delims, …)` | non-blocking tokenised Stream input (§9) |
| `SafeStringStream` | `SafeStringStream ss(sfData);` then `ss.begin(baud)` | a `Stream` that replays a SafeString at a simulated baud rate, for repeatable testing — **see §18** |
| `BufferedOutput` | `createBufferedOutput(name, size, mode, allOrNothing)` | non-blocking `print()` — **see §14** |
| `BufferedInput` | `createBufferedInput(name, size)` | extra RX buffering — **see §17** |
| `SerialComs` | `SerialComs coms(sendSize, receiveSize);` | framed, checksummed messages between Arduinos — **see §15** |
| `millisDelay` | `millisDelay d;` | non-blocking delay — **see §13** |
| `loopTimerClass` | `loopTimer` (predefined instance) | max/average `loop()` latency — **see §13** |
| `PinFlasher` | `PinFlasher f(pin);` | non-blocking pin flashing — **see §16** |

---

## 13. millisDelay and loopTimer

These two classes ship with SafeString and exist to support the same goal from the timing side: keeping
`loop()` free-running so that non-blocking text I/O (`SafeStringReader`, `BufferedOutput`) actually gets
serviced. `delay()` blocks everything; a blocked `loop()` means dropped serial input regardless of how
safe your string handling is.

Canonical tutorials: [How to code Timers and Delays in
Arduino](https://www.forward.com.au/pfod/ArduinoProgramming/TimingDelaysInArduino.html) and [Simple
Multitasking Arduino](https://www.forward.com.au/pfod/ArduinoProgramming/RealTimeArduino/index.html).

### 13.1 millisDelay — non-blocking, rollover-safe delay

```cpp
#include <millisDelay.h>

millisDelay ledDelay;                 // one instance per independent delay, usually global

void setup() {
  ledDelay.start(1000);               // start a 1000 ms delay
}

void loop() {
  if (ledDelay.justFinished()) {      // true exactly once, when the delay expires
    ledDelay.repeat();                // schedule the next one with no drift
    // ... do the periodic work here
  }
  // the rest of loop() keeps running at full speed
}
```

#### API

| Method | Returns | Behaviour |
|---|---|---|
| `start(unsigned long ms)` | `void` | start (or restart with a new duration). `start(0)` makes `justFinished()` true on the first call |
| `justFinished()` | `bool` | **true exactly once**, the first call at or after expiry. Internally calls `stop()`, so it cannot fire twice |
| `repeat()` | `void` | run the same delay again, advancing the start time by the delay — **no cumulative drift** |
| `restart()` | `void` | run the same delay again, timed from *now* — drift accumulates |
| `stop()` | `void` | cancel. `justFinished()` will never return true until started again |
| `finish()` | `void` | force expiry; the next `justFinished()` returns true |
| `isRunning()` | `bool` | true while a `justFinished()` is still pending |
| `remaining()` | `unsigned long` | ms left; `0` if stopped, expired, or `finish()` was called |
| `delay()` | `unsigned long` | the duration passed to `start()` |
| `getStartTime()` | `unsigned long` | `millis()` value at the last `start()`/`repeat()`/`restart()`; `0` if never started |

#### Rollover safety

`justFinished()` tests `(millis() - startTime) >= ms_delay` using unsigned arithmetic, which stays
correct across the `millis()` wrap at ~49.7 days. Do **not** hand-roll the arithmetic form
`millis() >= (startTime + ms_delay)` — it fails at the wrap. Always store times in `unsigned long`; an
`int` appears to run backwards after ~65 seconds. Maximum delay is the `unsigned long` range, ~49.7 days.

#### The one rule that matters

> `justFinished()` **must be called every `loop()`, unconditionally, at the outermost level.**

It must not sit inside another `if`, `while`, or `switch`. It may live inside a function, provided that
function is called every loop. Do not combine it with another condition:

```cpp
if (ledDelay.justFinished()) { … }              // CORRECT
if (enabled && ledDelay.justFinished()) { … }   // WRONG — short-circuit skips the check, delay never fires
```

To make a delay conditional, gate the *action*, not the test:

```cpp
if (ledDelay.justFinished()) {
  ledDelay.repeat();
  if (enabled) { … }
}
```

#### `repeat()` vs `restart()` vs `start()`

- **`repeat()`** sets `startTime += ms_delay`, so 100 repeats of 1 s take exactly 100 s even if
  `justFinished()` is noticed late. Use it for periodic work — sampling, heartbeats, blinking.
  If `repeat()` is called while the delay has *not* actually expired
  (i.e. after `stop()`/`finish()`, or while still running), it degrades to `start(ms_delay)` to avoid a
  wrap-around. So `repeat()` is safe to call at any time; it just loses the drift correction there.
- **`restart()`** is `start(ms_delay)` — times from now, accumulating the loop's latency each cycle.
- **`start(ms)`** also changes the duration.

#### Patterns

```cpp
// one-shot
timeout.start(5000);
if (timeout.justFinished()) { handleTimeout(); }

// periodic, drift-free
if (sampleDelay.justFinished()) { sampleDelay.repeat(); takeSample(); }

// timeout guard around a non-blocking operation
if (replyDelay.justFinished()) {           // no reply in time
  replyDelay.stop();
  reportNoReply();
} else if (sfReader.read()) {              // reply arrived first
  replyDelay.stop();
  handleReply();
}

// sequencing: each step arms the next
if (stepDelay.justFinished()) {
  step++;
  stepDelay.start(stepDuration[step]);
}
```

### 13.2 loopTimer — proving that nothing blocks

`loopTimer` measures **loop latency**: the elapsed time between the end of one `check()` call and the
start of the next, i.e. how long everything else in `loop()` took. Its own measuring and printing time is
subtracted out, so it does not inflate its own numbers.

```cpp
#include <loopTimer.h>

void loop() {
  loopTimer.check(Serial);        // prints accumulated statistics every 5 seconds
  // ... rest of loop()
}
```

Output:

```
loop us Latency
 5sec max:7276 avg:12
 sofar max:7276 avg:12 max - prt:15512
```

- `5sec max` / `avg` — maximum and average latency in microseconds over the last 5 seconds.
- `sofar max` — the largest 5-second maximum seen so far, and the largest 5-second average seen so far.
- `max - prt` — microseconds this print statement itself consumed (it is compensated for, not included
  in the latency figures).

#### API

| Method | Purpose |
|---|---|
| `loopTimerClass(const char* name = NULL)` | create a named timer; `NULL` prints as `"loop"` |
| `check(Print& out)` / `check(Print* out)` | accumulate timing and print every 5 s |
| `check()` | accumulate only, print nothing |
| `print(Print& out)` / `print(Print* out)` | print the most recent statistics on demand |
| `clear()` | discard all accumulated data and re-initialise on the next `check()` |

`loopTimer` is a predefined instance, ready to use. For timing a specific function instead of `loop()`,
either move the `check()` call into it or create a separate named instance:

```cpp
loopTimerClass stepTimer("step");

void step() {
  stepTimer.check(Serial);        // prints "step us Latency ..."
  // ...
}
```

Deferred printing:

```cpp
void loop() {
  loopTimer.check();              // measure silently
  if (reportDelay.justFinished()) {
    reportDelay.repeat();
    loopTimer.print(Serial);      // print when it suits you
  }
}
```

> **Gotcha:** `loopTimer.h` declares the predefined instance as `static loopTimerClass loopTimer;` at file
> scope. `static` gives it internal linkage, so **every `.cpp` that includes the header gets its own
> separate instance**. In a single-file sketch this is invisible. Across multiple translation units,
> calling `loopTimer.check()` from two files times two unrelated things. Use explicitly named instances
> in multi-file projects.

#### How to use it

Aim for a loop running faster than about 1000 µs (1ms). A `5sec max` in the
hundreds of thousands means something in the loop is blocking; the usual culprits are `delay()`,
`Serial.print()` to a full TX buffer, `Serial.readBytesUntil()`, `analogRead()` on some cores, and
`SoftwareSerial`. The combination to reach for: `millisDelay` instead of `delay()`, `SafeStringReader`
instead of blocking reads, `BufferedOutput` instead of raw `print()`, and `loopTimer` to verify the
result.

### 13.3 millisDelay and SafeStringReader — which timeout is which

`SafeStringReader` has its **own** timeout, and it is not a `millisDelay`. Confusing the two is the
common mistake.

| Need | Mechanism |
|---|---|
| return the last token when characters stop arriving mid-token | `sfReader.setTimeout(ms)` (§9) |
| act when *no command at all* has arrived for a while — session timeout, watchdog, "still there?" prompt | `millisDelay` |

`setTimeout()` is per-token and **resets on every character received**, so it fires only after input
genuinely goes quiet. Reimplementing that with a `millisDelay` around `read()` gets it wrong: you would
have to restart the delay on each character, which you cannot see from outside the reader. The library's
own `SafeStringReader_CmdsTimed.ino` — the "timed commands" example — uses `setTimeout(1000)` and no
`millisDelay` at all.

Use `millisDelay` for the *application-level* timeout, which the reader knows nothing about:

```cpp
millisDelay idleDelay;
idleDelay.start(30000);              // 30s with no command

void loop() {
  if (sfReader.read()) {
    idleDelay.restart();             // saw a command — restart the idle timer
    handleCommand(sfReader);
  }

  if (idleDelay.justFinished()) {    // top level, NOT nested in the read() above
    logOut();
  }
}
```

#### The nesting bug

This is where the §13.1 rule is most often broken, because the wrong version reads so naturally:

```cpp
if (sfReader.read()) {
  if (statusDelay.justFinished()) { … }   // WRONG — only checked when a token arrives
}
```

`read()` returns false on the vast majority of loops, so the delay is almost never tested and fires late
or not at all. Keep every `justFinished()` at the outermost level of `loop()`, alongside the `read()`
rather than inside it.

---

## 14. BufferedOutput — non-blocking `print()`

`Serial.print()` is only non-blocking while the hardware TX buffer has room. Once it fills, `print()`
**blocks** until bytes drain — at 9600 baud that is roughly 1 ms per character, so printing an 80-char
line can stall `loop()` for 80 ms. `BufferedOutput` adds a ring buffer in front of the stream and
releases bytes a few at a time from `loop()`, so printing never blocks.

```cpp
#include <BufferedOutput.h>

createBufferedOutput(bufferedOut, 80, DROP_UNTIL_EMPTY);

void setup() {
  Serial.begin(115200);
  bufferedOut.connect(Serial);        // throttle using Serial's availableForWrite()
}

void loop() {
  bufferedOut.nextByteOut();          // MUST be called every loop() to release bytes
  bufferedOut.println(millis());      // use bufferedOut instead of Serial
}
```

### Creation

```cpp
createBufferedOutput(name, size, mode);                  // allOrNothing = true
createBufferedOutput(name, size, mode, allOrNothing);
```

The macro declares a `uint8_t name_OUTPUT_BUFFER[size+4]` — the extra 4 bytes hold the drop mark. Buffer
size is capped at 32766; sizes under 8 (or a `NULL` buffer) fall back to an internal 8-byte buffer.

| Mode | Behaviour when the buffer fills |
|---|---|
| `BLOCK_IF_FULL` | blocks until space frees up. **Not recommended** — it reintroduces exactly the stall you are trying to remove. Useful only in testing, to guarantee every character is seen |
| `DROP_UNTIL_EMPTY` | drop all further output until the buffer has *completely* emptied. Keeps consecutive prints intact so surviving output is more readable. `availableForWrite()` returns 0 for the whole drop period |
| `DROP_IF_FULL` | drop only until there is space again. |

Whenever characters are dropped, `~~` (followed by CR NL) is inserted into the output so the gap is
visible.

**`allOrNothing`** (default `true`): if a whole `print(...)` will not fit, none of it is written — you
never see half a line. Set `false` to emit the part that fits. Ignored in `BLOCK_IF_FULL` mode.
### Connecting — and the trap

```cpp
bufferedOut.connect(Serial);                 // HardwareSerial: throttles via availableForWrite()
bufferedOut.connect(anyStream, 9600);        // any Stream: releases at ~13 bits/byte for that baud rate
```

> **Trap:** `connect(HardwareSerial&)` requires a working `availableForWrite()`. On boards where it is
> absent or returns ≤ 2 — Arduino Due, NRF52/NRF5, STM32F1/F4, megaTinyCore — the library enters an
> **infinite `while(1)` loop printing instructions every 5 seconds** rather than failing quietly. The
> sketch appears hung. On those boards you must use `connect(stream, baudRate)` and add extra
> `nextByteOut()` calls, because only one byte is released per call in that mode.

With an explicit baud rate the release interval is `13000000 / baudRate` microseconds (≈13 bits per byte:
start + 8 data + parity + 2 stop). A baud rate **above** 13000000 makes that interval round to zero and
is rejected with the same `while(1)` message.

### API

| Method | Purpose |
|---|---|
| `nextByteOut()` | release buffered bytes. **Call at the top of every `loop()`**, and more often in long loops. Most other methods also release bytes |
| `write` / `print` / `println` | inherited from `Print`; write into the buffer |
| `available()` / `read()` / `peek()` | reads pass **straight through** to the underlying stream — input is *not* buffered |
| `availableForWrite()` | space in the ring buffer plus the stream's TX space |
| `getSize()` | ring buffer size plus any detected hardware TX buffer |
| `clearSpace(len)` | force `len` bytes of room by discarding existing buffered output (adds a `~~` mark). Stops at the `protect()` mark. Does not touch the hardware TX buffer |
| `protect()` | mark the current buffer contents so `clearSpace()` will not discard them |
| `clear()` | discard the whole buffer, protected content included. Does not touch the hardware TX buffer |
| `terminateLastLine()` | append `\r\n` (or just `\n` if space is tight) unless the last char is already `\n` |
| `flush()` | **blocks** until the buffer empties |

`'\0'` bytes are filtered out of the final output, and `write(0)` is equivalent to calling `protect()`.

### It is a Stream, so it composes

Because `BufferedOutput` is a `Stream`, a `SafeStringReader` can read *through* it — reads go directly to
the underlying serial port while the reader's echo goes out through the buffer:

```cpp
bufferedOut.connect(Serial);
sfReader.connect(bufferedOut);      // echo is buffered, so echoing never blocks
sfReader.echoOn();
```

`loopTimer` can also print through it: `loopTimer.check(bufferedOut);`

**§22** covers this pairing in detail, including echo ordering and when it is worth doing.

### The full non-blocking stack

```cpp
createSafeStringReader(sfReader, 15, " ,\r\n");
createBufferedOutput(bufferedOut, 80, DROP_UNTIL_EMPTY);
millisDelay ledDelay;

void loop() {
  bufferedOut.nextByteOut();        // 1. release buffered output
  loopTimer.check(bufferedOut);     // 2. measure, and prove the loop is fast
  processUserInput();               // 3. sfReader.read() — non-blocking input
  blinkLed();                       // 4. ledDelay.justFinished() — non-blocking timing
}
```

That is the library's complete answer to "do text I/O without blocking": `SafeStringReader` in,
`BufferedOutput` out, `millisDelay` for timing, `loopTimer` to verify.

---

## 15. SerialComs — reliable messages between two boards

`SerialComs` sends and receives whole lines of text between two Arduinos (or an Arduino and a PC) over a
serial link, with a checksum and flow control, so a message either arrives intact or not at all.

> It owns its link stream completely. In particular, **never put a `BufferedOutput` between `SerialComs`
> and its port** — a dropping buffer discards protocol bytes and injects `~~` drop marks into the frame.
> See §24, which covers that and where a `BufferedOutput` does belong in such a sketch.

```cpp
#include "SerialComs.h"

SerialComs coms;                     // default 60 chars each way
unsigned long counter = 0;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);               // a board with a second UART, e.g. Mega/ESP32/Due

  SafeString::setOutput(Serial);     // SafeString errors AND SerialComs debug output
  coms.setAsController();            // exactly ONE side calls this

  if (!coms.connect(Serial1)) {      // ALWAYS check — connect() allocates
    while (1) { Serial.println(F("Out-Of-Memory")); delay(3000); }
  }
}

void loop() {
  coms.sendAndReceive();             // MUST be called every loop()

  if (!coms.textReceived.isEmpty()) {
    // handle the message NOW — it is cleared on the next sendAndReceive()
  }

  if (coms.textToSend.isEmpty()) {   // previous message has gone out
    counter++;
    coms.textToSend.print(F("count "));
    coms.textToSend.print(counter);
  }
}
```

### The protocol (from `SerialComs.cpp`)

- Message format: `<messageText><MOD256 checksum as 2 hex chars><XON>`, where `XON` is `0x11`.
  An empty message is a bare `XON`.
- **Token passing:** `XON` terminates a message and grants the other side permission to send. Only one
  side may transmit at a time, so there are no collisions.
- Exactly one side must call `setAsController()`. On startup the non-controller stays silent until the
  controller prompts it with an empty message; they then alternate. **If one side uses `SoftwareSerial`,
  that side must be the controller.**
- All UTF-8 text can be sent **except `0x11`** — any `XON` in your message is silently replaced with a
  space (`0x20`).
- If nothing is received for the connection timeout, `isConnected()` goes false and the controller
  re-prompts. The timeout is **5000 ms** (`connectionTimeout_ms = 5000` in the constructor).

### API

| Member | Purpose |
|---|---|
| `SerialComs(size_t sendSize = 60, size_t receiveSize = 60)` | capacities in message characters, excluding checksum, `XON` and `'\0'` |
| `setAsController()` | call on exactly one side |
| `connect(Stream& io)` | choose the link; **returns `false` on out-of-memory — always check** |
| `sendAndReceive()` | call every `loop()`; sends if it may, receives if waiting |
| `textToSend` / `getTextToSend()` | `SafeString&` of capacity `sendSize`; cleared once sent |
| `textReceived` / `getTextReceived()` | `SafeString&` holding the received message; **cleared at the start of every `sendAndReceive()`** |
| `isConnected()` | false after a link timeout |
| `noCheckSum()` | disable checksum generation and verification — must be done on **both** sides |

`textToSend` and `textReceived` are `#define`d aliases for the getter calls, so they read like member
variables. Being SafeStrings, the whole API from §6 applies — `print()`, `toInt()`, `==`, tokenising.

**§20** covers combining `SerialComs` with a separate `SafeStringReader` for local console input, and why
the link stream must not be shared. **§24** covers `BufferedOutput` — which must never carry the link
itself, but is the right home for the protocol debug output.

### Rules

1. **Sizes must mirror.** `SerialComs coms(120, 200)` on one side requires `SerialComs coms(200, 120)` on
   the other.
2. **Exactly one controller**, and it must be the `SoftwareSerial` side if there is one.
3. **Process `textReceived` in the same `loop()`** it arrives — the next `sendAndReceive()` clears it.
4. **Only write to `textToSend` when `isEmpty()`** is true, otherwise you append to a message still
   queued for transmission.
5. **Check `connect()`'s return value.** This is the one place in the library that uses `malloc`/`new`
   (three buffers plus the reader objects, allocated once at `connect()` and freed by the destructor).
   Everything else in SafeString is heap-free.
6. `noCheckSum()` must match on both sides or every message fails verification.
7. Error output goes to **`SafeString::setOutput(...)`** by default, deliberately *not* to the stream
   passed to `connect()` — you cannot debug onto the link itself.
7. Debug, when enabled, also goes to **`SafeString::setOutput(...)`** by default, see below

### The two output switches: `ERROR_STREAM` and `DEBUG`

`SerialComs` splits its own output into two categories, each behind its own compile-time switch near the
top of `SerialComs.cpp`. This is the same coding-error / routine-traffic distinction as §4.8, applied to
a protocol:

```cpp
// uncomment this for  per message debugging
// #define DEBUG SafeString::Output

// Stream out for error messages
#define ERROR_STREAM SafeString::Output
```

| | `ERROR_STREAM` | `DEBUG` |
|---|---|---|
| Default | **enabled** | **disabled** |
| Carries | faults — something went wrong | routine per-message tracing |
| Volume | rare, only on a problem | a line for **every** message, both directions |
| Messages | out of memory; low memory after `connect()`; `connect()` not called; an over-long message discarded; receive timed out without the terminating `XON`, followed by the partial text; checksum failures | `Received '…'`, `Sending '…'`, `Made Connection.`, `Connection timed out`, `Got prompted by Controller`, `Prompt other side to connect`, startup progress |

By default both expand to `SafeString::Output`, but you can define any Stream as the output.  For `SafeString::Output` you need to use `SafeString::setOutput(...)`,  **nothing prints
until you call it** — `Output` starts routed to an empty `Print`.

Leave `ERROR_STREAM` enabled. Its messages are rare, and each one reports something you would otherwise
have to infer from a message that never arrived — a discarded over-long message, a failed checksum,
low memory.

Neither switch affects SafeString's own error messages, which do not go through these macros. So a
deployed sketch with `DEBUG` off and `setOutput(Serial)` on still reports genuine coding errors (§4.1)
and `SerialComs` faults, and prints nothing routine.

Commenting out `SafeString::setOutput(...)` in the sketch is a different: it silences
everything, SafeString errors included.

---

## 16. PinFlasher — non-blocking pin flashing

Built on `millisDelay` (§13.1). Flashes an output pin without blocking, and — the point of the class —
lets you drive it from state logic that runs every loop without having to track whether anything changed.

```cpp
#include <PinFlasher.h>

PinFlasher ledFlasher(13);          // pin 13, HIGH = on
// PinFlasher ledFlasher(13, true); // inverted: LOW = on

void loop() {
  ledFlasher.update();              // MUST be called every loop()

  // safe to call every loop — a repeated identical value is a no-op
  if (alarm)        ledFlasher.setOnOff(100);      // fast flash
  else if (warning) ledFlasher.setOnOff(1000);     // slow flash
  else              ledFlasher.setOnOff(PIN_OFF);  // hard off
}
```

### The two magic values

| Constant | Value | Meaning |
|---|---|---|
| `PIN_ON` | `-1` | hold the pin **on** (stops flashing) |
| `PIN_OFF` | `0` | hold the pin **off** (stops flashing) |

Any other value is a half-period in ms. "On" and "off" are logical — with `invert = true`, on drives the
pin LOW.

### API

| Method | Behaviour |
|---|---|
| `PinFlasher(int pin = -1, bool invert = false)` | records the pin; **does not touch the hardware yet** |
| `update()` | advance the flash state. **Call every `loop()`** |
| `setOnOff(unsigned long onOff_ms)` | equal on and off times (50% duty, period = 2×). `PIN_ON` / `PIN_OFF` hold the pin |
| `setOnAndOff(unsigned long on_ms, unsigned long off_ms)` | independent on and off times. `PIN_ON` and `PIN_OFF` are **invalid here and silently ignored** |
| `setPin(int pin)` | change pins: stops flashing, sets the new pin to output and off, returns the previous pin to `INPUT`. Passing the *same* pin is ignored and does not disturb flashing. Any negative value means "no pin" |
| `invertOutput()` | flip the on/off polarity, keeping the current logical state. Returns the new setting (`true` = on is LOW) |
| `~PinFlasher()` | returns the pin to `INPUT` |

### Why calls are idempotent

`setOnOff()` and `setOnAndOff()` **do nothing if the requested timing already matches the current
setting** — they just call `update()` and return. This is deliberate: you can call them unconditionally
from `loop()` based on program state, and the flashing continues undisturbed rather than restarting its
phase every pass. There is no need to track "did the mode change?" yourself.

### Two behaviours worth knowing

- **The constructor does not call `pinMode()`.** A global `PinFlasher` is constructed during static
  initialisation, before the board core is ready, and touching hardware there breaks some boards
  (ESP32-C variants in particular). The pin is claimed lazily by the first `update()`, `setOnOff()`,
  `setPin()` or `invertOutput()` call. Until then the pin stays in its power-on reset state — so a global
  `PinFlasher` needs at least one `update()` before the pin is driven at all.
- **Flash timing slips.** `update()` restarts the delay with `start()`, not `repeat()`, so each transition
  is timed from when it was noticed. Over many cycles the flashing drifts slightly. It is a visual
  indicator, not a clock — use `millisDelay` with `repeat()` if you need drift-free periodic timing.

`PinFlasher` inherits `millisDelay` **protected**, so the timing methods are not part of its public
interface — you cannot call `justFinished()` on a `PinFlasher`. `setOutput()` is `virtual`, so the class
can be subclassed to drive something other than a GPIO (the library author's WS2812 flasher does this).

---

## 17. BufferedInput — extra RX buffering

The input-side counterpart to `BufferedOutput` (§14). A hardware serial RX buffer is small (64 bytes on
an UNO). If `loop()` occasionally takes longer than the time it takes for that buffer to fill, incoming
characters are lost. `BufferedInput` sits in front of the stream and drains it into a larger buffer of
your choosing.

```cpp
#include <BufferedInput.h>

createBufferedInput(bufferedIn, 128);

void setup() {
  Serial.begin(115200);
  bufferedIn.connect(Serial);
}

void loop() {
  bufferedIn.nextByteIn();          // MUST be called every loop() to drain the RX buffer
  // then read from bufferedIn instead of Serial
}
```

The macro declares `uint8_t name_INPUT_BUFFER[size]`. Size is
capped at 32766; sizes under 8 or a `NULL` buffer fall back to an internal 8-byte buffer.

| Method | Purpose |
|---|---|
| `connect(Stream& stream)` | choose the input stream and clear the buffer |
| `nextByteIn()` | move everything currently available from the stream into the buffer. **Call every `loop()`**, and more often in long loops. Most other methods also call it |
| `available()` / `read()` / `peek()` | read from the buffer |
| `write(...)` | passes **straight through** to the underlying stream — output is not buffered |
| `getSize()` | buffer size |
| `maxStreamAvailable()` | high-water mark of the *stream's* `available()` seen so far; **calling the method resets count to 0** |
| `maxBufferUsed()` | high-water mark of this buffer's fill level; **calling the method resets count to 0** |

### Sizing the buffer

`maxStreamAvailable()` and `maxBufferUsed()` exist to be measured, not guessed:

```cpp
if (reportDelay.justFinished()) {
  reportDelay.repeat();
  Serial.print(F("maxStreamAvailable: ")); Serial.println(bufferedIn.maxStreamAvailable());
  Serial.print(F("maxBufferUsed: "));      Serial.println(bufferedIn.maxBufferUsed());
}
```

If `maxStreamAvailable()` approaches the hardware RX buffer size, you are close to losing characters and
need `nextByteIn()` called more often. `maxBufferUsed()` tells you how much of your buffer is actually
needed. Both reset on read, so each report covers the interval since the last one.

### Notes and traps

- **`nextByteIn()` before `connect()` blocks for 5 seconds.** It prints
  `"BufferedInput Error: need to call connect(..) first in setup()"` to `SafeString::Output` and then
  calls `delay(5000)`. In a loop that becomes a 5-second stall per iteration, which reads as a hang.
- **Buffering is not a substitute for a fast loop.** `BufferedInput` absorbs *bursts*; it cannot help if
  the average consumption rate is below the incoming data rate. Use `loopTimer` (§13.2) to find what is
  blocking instead.
- **Often unnecessary.** `SafeStringReader` (§9) already reads non-blockingly every loop. Reach for
  `BufferedInput` only when something in the loop is unavoidably slow and measurement shows characters
  are being lost. **§19 covers pairing the two in detail**, including the reason a fast loop can still
  drop input.

---

## 18. SafeStringStream — automated testing of input code

`SafeStringStream` is a `Stream` whose data comes from a SafeString instead of a serial port. Anything
that reads from a `Stream` — `SafeStringReader` (§9), `readUntilToken()` (§7.3), your own parser — can be
driven from a canned string with no terminal, no typing, and identical results every run. It can also
release the data at a **simulated baud rate**, which is what makes it a test rig rather than just a string
reader: it reproduces the condition your parser must actually survive, data arriving a few characters at
a time across many `loop()` passes.

```cpp
#include "SafeStringStream.h"

cSF(sfTestData, 128);
SafeStringStream sfStream;

void setup() {
  Serial.begin(9600);
  SafeString::setOutput(Serial);

  sfTestData = F("looooooooooooong stop input, start,, nothing, stop, reset\n");
  sfStream.begin(sfTestData, 1200);        // release at 1200 baud
}
```

### 18.1 Construction and begin()

| Form | Effect |
|---|---|
| `SafeStringStream ss;` | no data yet — supply it with `begin(sf, baud)` |
| `SafeStringStream ss(sfData);` | reads from `sfData`, internal **8-byte** RX buffer |
| `SafeStringStream ss(sfData, sfRxBuffer);` | as above, but `sfRxBuffer` becomes the RX buffer — use this to simulate a larger hardware buffer |

| Call | Effect |
|---|---|
| `begin()` or `begin(0)` | infinite baud rate — the whole string is available immediately |
| `begin(baudRate)` | release one byte per `13000000 / baudRate` µs (≈13 bits per byte) |
| `begin(sf, baudRate)` | also replaces the data SafeString |

> **`begin()` is mandatory.** `available()`, `read()`, `write()` and `flush()` called before it print
> `"SafeStringStream Error: need to call begin(..) first in setup()"` and then `delay(5000)` — in a loop
> that presents as a hang. **`peek()` is the exception: it silently returns `-1`**, so a parser that only
> peeks sees an empty stream with no diagnostic at all. A `baudRate` above 13000000 triggers an infinite
> `while(1)` error loop.

### 18.2 What the two modes actually model

This distinction is the whole point of the class:

- **`begin(0)` — infinite baud.** `available()` returns the full remaining length, so a single pass reads
  everything. Use it to exercise parsing logic quickly.
- **`begin(9600)` — realistic.** Bytes trickle from the data SafeString into the RX buffer at the
  simulated rate, and `available()` reports **only what is in the RX buffer** — 8 bytes by default, not
  the whole string. This is what catches the bugs: parsers that assume a whole line arrives at once,
  tokenisers that mishandle a token split across `loop()` calls, and code that is too slow to keep up.

```cpp
// at infinite baud rate, this prints the whole input in one go
while (sfStream.available()) { Serial.print((char)sfStream.read()); }
```

### 18.3 Reading consumes, writing appends

- **Reading removes characters from the underlying SafeString.** After draining the stream, the data
  SafeString is empty. To re-run a test, re-assign the data.
- **Writing to the stream appends to the underlying SafeString**, so you can inject more test data at
  runtime. If the SafeString is full the write is discarded — it never blocks — and an error is raised.
- `availableForWrite()` reports the underlying SafeString's free space.

### 18.4 RxBufferOverflow() — the assertion worth making

```cpp
size_t dropped = sfStream.RxBufferOverflow();   // count since the last call; calling this method resets dropped count to 0
```

When the RX buffer is full and another byte is due, the oldest byte is discarded and this counter
increments. That is precisely a hardware RX overrun. A non-zero value means **your code did not read fast
enough at that baud rate and would lose data on a real port** — so check it in tests rather than trusting
that the parse looked right.

### 18.5 The echo feedback loop

If the code under test echoes — `sfReader.echoOn()`, or `readUntilToken(..., echoInput = true)` — the
echoed characters are *written back into the data SafeString*, because writing appends. The test data
then repeats forever.

This is a trap and a technique. The `SafeStringStream_testdata.ino` example relies on it deliberately to
loop its test input indefinitely. If you did not intend it, turn echo off or write the echo to a
different stream.

### 18.6 Swapping test input for live input

The idiomatic pattern — one `#define` switches the whole sketch between canned and live data, with no
other code changes. **Fragment**, not a complete sketch: `input`, `token`, `delimiters` and
`skipToDelimiter` are declared as in §7.3, and it needs `#include <SafeString.h>` and
`#include <SafeStringStream.h>`.

```cpp
#define TEST_DATA          // comment out to read from Serial instead

#ifdef TEST_DATA
  SafeStringStream sfStream;
  cSF(sfTestData, 128);
#endif

void setup() {
#ifdef TEST_DATA
  sfTestData = F("start, stop, reset\n");
  sfStream.begin(sfTestData, 1200);
  #define StreamInput sfStream
#else
  #define StreamInput Serial
#endif
}

void loop() {
  if (input.readUntilToken(StreamInput, token, delimiters, skipToDelimiter, true)) {
    // identical parsing code for both sources
  }
}
```

Because `SafeStringStream` is a real `Stream`, it also works as the argument to `sfReader.connect(...)`,
`sfStr.read(...)` and `sfStr.readUntil(...)`. **§21** covers pairing it with `SafeStringReader` as a test
harness, including RX buffer sizing and the error paths worth exercising.

---

## 19. Using BufferedInput with SafeStringReader

`SafeStringReader` (§9) is already non-blocking, so `BufferedInput` (§17) is not needed most of the
time. This section covers the specific case where it *is* needed, and how to tell.

### 19.1 Why a fast loop can still lose characters

`SafeStringReader::read()` does not drain the port. Inside `readUntilTokenInternal()` the read loop is
bounded:

```cpp
while (input.available() && (len < capacity()) && (noCharsRead < capacity())) { … }
```

So **one `read()` call transfers at most `capacity()` characters** from the stream, where `capacity()` is
the reader's internal input buffer — `size + 1` for `createSafeStringReader(name, size, …)`.

> `read()` calls `readUntilToken()` twice in one case only: when it entered the call **already**
> discarding an over-long token and still has not reached the delimiter. That pass moves up to
> `2 × capacity()`. The call that *first* detects the overflow runs the loop once, because the second
> pass is deliberately skipped — it would clear the token and destroy the discarded characters that
> `longTokenDiscarded()` leaves for you (§9).

Concretely: `createSafeStringReader(sfReader, 5, " ,\r\n")` pulls **at most 6 characters per `read()`
call**. If `read()` is called once per `loop()` and the port delivers more than 6 characters in that
time, the hardware RX buffer — 64 bytes on an UNO — fills and the excess is silently lost. The loop can
be fast and the sketch can still drop input.

This is the situation `BufferedInput` addresses: a small reader on a fast or bursty stream, or a loop
with an occasional long pass.

### 19.2 The wiring

`BufferedInput` sits between the port and the reader. Both are `Stream`s, so it is a two-line change:

```cpp
#include <SafeStringReader.h>
#include <BufferedInput.h>

createSafeStringReader(sfReader, 5, " ,\r\n");
createBufferedInput(bufferedIn, 64);

void setup() {
  Serial.begin(115200);
  bufferedIn.connect(Serial);        // BufferedInput reads from the port
  sfReader.connect(bufferedIn);      // the reader reads from BufferedInput
}

void loop() {
  bufferedIn.nextByteIn();           // 1. drain the hardware RX buffer — FIRST, and every loop
  if (sfReader.read()) {             // 2. take up to capacity() chars from BufferedInput
    // handle the token
  }
}
```

`nextByteIn()` moves **everything currently available** from the hardware buffer into your buffer in one
call — it is not bounded the way the reader is. That is the whole mechanism: the hardware buffer is
emptied promptly into a buffer whose size you control, and the reader then consumes from there at its own
pace.

Call `nextByteIn()` at the top of `loop()`, and again around anything slow inside it.

### 19.3 What this fixes, and what it does not

| Problem | Does BufferedInput help? |
|---|---|
| Bursts of input larger than the hardware RX buffer | **Yes** — that is exactly what it absorbs |
| An occasional slow pass through `loop()` | **Yes**, if the buffer is sized for the worst pass |
| Input arriving faster than `size + 1` chars per loop, sustained | **No** — your buffer fills instead of the hardware one, just more slowly |
| `loop()` blocked for long periods by `delay()` or blocking prints | **No** — fix the blocking (§13.2, §14) |

For a sustained rate mismatch the real fixes are to **increase the reader's `size`** (each `read()` then
transfers more), or to **call `sfReader.read()` more than once per loop**. `BufferedInput` buys headroom,
not throughput.

### 19.4 Sizing the buffer by measurement

`maxStreamAvailable()` and `maxBufferUsed()` (§17) exist for exactly this decision. Both are
high-water marks that reset to 0 when read, so each report covers the interval since the last one:

```cpp
if (reportDelay.justFinished()) {
  reportDelay.repeat();
  Serial.print(F("maxStreamAvailable: ")); Serial.println(bufferedIn.maxStreamAvailable());
  Serial.print(F("maxBufferUsed: "));      Serial.println(bufferedIn.maxBufferUsed());
}
```

| Reading | Meaning | Action |
|---|---|---|
| `maxBufferUsed()` == buffer size | the buffer is full and characters are being dropped | increase the `createBufferedInput` size |
| `maxStreamAvailable()` approaches the hardware RX size (64 on an UNO) while the buffer still has room | the hardware buffer is nearly overflowing before you drain it | call `nextByteIn()` **more often**, not just with a bigger buffer |
| both comfortably below their limits | sized correctly | — |

Note the second row: a bigger `BufferedInput` does nothing if the hardware buffer overflows before
`nextByteIn()` runs. Frequency and size are separate knobs and the two statistics distinguish them.

### 19.5 Echo through BufferedInput

`SafeStringReader` echoes by writing back to the stream it was connected to — so with this wiring the
echo goes to `bufferedIn`. `BufferedInput::write()` passes straight through to the underlying stream
(and conveniently calls `nextByteIn()` on the way), so echo still reaches Serial and still works.

But that pass-through write is a plain `Serial.write()`, so **echo can block** once the TX buffer fills —
reintroducing the stall you were avoiding. Options:

1. `sfReader.echoOff()` — simplest, if echo is not required.
2. Chain both buffers: `bufferedIn.connect(bufferedOut); bufferedOut.connect(Serial);` with
   `sfReader.connect(bufferedIn)`. Reads pass through `BufferedOutput` to Serial (§14) and echo writes
   land in the output buffer, so both directions are buffered. This requires **both**
   `bufferedIn.nextByteIn()` and `bufferedOut.nextByteOut()` every loop. The pass-through behaviour of
   both classes makes this work.
3. Connect the reader to `bufferedOut` instead (§14). Echo is buffered, but input is not — the right
   choice when echo is the bottleneck rather than input.

### 19.6 Complete example

```cpp
#include <SafeStringReader.h>
#include <BufferedInput.h>
#include <millisDelay.h>
#include <loopTimer.h>

createSafeStringReader(sfReader, 5, " ,\r\n");   // drains at most 6 chars per read()
createBufferedInput(bufferedIn, 64);             // headroom for bursts
millisDelay reportDelay;

void setup() {
  Serial.begin(115200);
  SafeString::setOutput(Serial);

  bufferedIn.connect(Serial);
  sfReader.connect(bufferedIn);
  sfReader.echoOff();                            // avoid blocking echo (see 17.5)
  reportDelay.start(5000);
}

void loop() {
  bufferedIn.nextByteIn();          // drain the hardware RX buffer every loop
  loopTimer.check(Serial);          // prove the loop is not blocking

  if (sfReader.read()) {
    if (sfReader == "start")     { /* … */ }
    else if (sfReader == "stop") { /* … */ }
  } else if (sfReader.longTokenDiscarded()) {
    Serial.print(F("command too long: ")); Serial.println(sfReader);
  }

  if (reportDelay.justFinished()) { // is the buffering actually adequate?
    reportDelay.repeat();
    Serial.print(F("maxStreamAvailable: ")); Serial.println(bufferedIn.maxStreamAvailable());
    Serial.print(F("maxBufferUsed: "));      Serial.println(bufferedIn.maxBufferUsed());
  }
}
```

Use `SafeStringStream` (§18) at a realistic baud rate to reproduce the fast-input condition on the bench,
and check `RxBufferOverflow()` to confirm nothing was dropped.

---

## 20. Using SerialComs with SafeStringReader

### 20.1 `textReceived` already *is* a SafeStringReader

`SerialComs::connect()` builds one internally:

```cpp
textReceivedPtr = new SafeStringReader(*receiver_SF_INPUT, _receiveSize + 2 + 2,
                                       receiver_TOKEN_BUFFER, "textReceived", XON);
textReceivedPtr->returnEmptyTokens();
textReceivedPtr->connect(*stream_io_ptr);
```

So the link is read by a `SafeStringReader` whose delimiter is `XON` (0x11), with empty tokens enabled
and a short internal timeout. `sendAndReceive()` drives it for you.

Two consequences:

- **`coms.textReceived` is returned as a `SafeString&`,** not a `SafeStringReader&`. The reader-specific
  methods — `read()`, `getDelimiter()`, `setTimeout()`, `echoOn()` — are not reachable through it, which
  is deliberate.
- **Never try to drive it yourself.** `SerialComs` owns that reader's state machine; calling `read()` on
  it out of band would consume a message the protocol layer needs.

You use `coms.textReceived` purely as a SafeString: the whole API in §6 applies.

### 20.2 One reader per stream — never share the link

The internal reader has already called `connect()` on the stream you passed to `coms.connect()`.
Attaching **your own** `SafeStringReader` to that same stream gives two consumers racing for the same
bytes: each steals characters the other needs, `XON` delimiters go missing, checksums fail, and the link
drops.

> **Rule:** the stream passed to `coms.connect()` belongs to `SerialComs` alone. A separate
> `SafeStringReader` must read from a *different* stream.

### 20.3 The normal layout: link on one port, console on another

The standard arrangement is `SerialComs` on the inter-board link and a `SafeStringReader` on the USB
monitor for local commands:

```cpp
#include "SerialComs.h"
#include <BufferedOutput.h>

SerialComs coms(60, 60);                          // link messages, both directions
createSafeStringReader(sfCmd, 30, " ,\r\n");      // local console commands
createBufferedOutput(bufferedOut, 80, DROP_UNTIL_EMPTY);

void setup() {
  Serial.begin(115200);      // console
  Serial1.begin(9600);       // link to the other board

  SafeString::setOutput(Serial);   // DIRECT to Serial, not through bufferedOut — see below
  bufferedOut.connect(Serial);
  sfCmd.connect(Serial);           // console reader — NOT Serial1
  sfCmd.echoOn();

  coms.setAsController();          // exactly one side
  if (!coms.connect(Serial1)) {    // link — owned by SerialComs
    while (1) { Serial.println(F("Out-Of-Memory")); delay(3000); }
  }
}

void loop() {
  bufferedOut.nextByteOut();
  coms.sendAndReceive();           // MUST be called every loop

  // remote -> console
  if (!coms.textReceived.isEmpty()) {
    bufferedOut.print(F("remote: "));
    bufferedOut.println(coms.textReceived.c_str());
  }

  // console -> remote
  if (sfCmd.read()) {
    if (coms.isConnected() && coms.textToSend.isEmpty()) {
      coms.textToSend = sfCmd;      // cannot fail: sfCmd's size (30) <= sendSize (60), see §20.5
    } else {
      bufferedOut.println(F("link busy or down — command dropped"));
    }
  } else if (sfCmd.longTokenDiscarded()) {
    bufferedOut.print(F("command too long, starts with '"));   // the user typed > 30 chars
    bufferedOut.print(sfCmd);
    bufferedOut.println('\'');
  }
}
```

Note which check goes where. The console command is **input**, so an over-long one is reported by
`sfCmd.longTokenDiscarded()` (§9) — no error, and the reader still holds the leading characters to quote
back. The assignment to `textToSend` needs no check at all: sizing the reader `≤ sendSize` makes it
impossible to fail, which is a declaration-time guarantee rather than a runtime branch.

Note `coms.textToSend = sfCmd;` — a plain SafeString assignment between two SafeStrings, bounds-checked
like any other (§6.2).

#### Where `setOutput()` points, and what it costs

`setOutput()` above targets the raw `Serial`, **not** `bufferedOut`. That single choice decides which
failure mode you get:

| | `setOutput(Serial)` — as above | `setOutput(bufferedOut)` — §24.3 |
|---|---|---|
| A message can be lost | **never** | yes — `DROP_UNTIL_EMPTY` discards under load |
| A message can block `loop()` | **yes** — raw `print()` stalls on a full TX buffer | no |
| Order against buffered output | interleaves out of order | correct |

For SafeString errors alone the direct route is the right trade, and for the reason in §22: they are
rare, they should never occur in working code (§4.1), and losing the one message that explains a failure
costs more than a brief stall.

**With the defaults, that is the whole story** — and the wiring above is right. `SerialComs` keeps its
routine per-message tracing behind `#define DEBUG`, which ships commented out (§15), so this channel
carries only SafeString errors and `SerialComs` **faults**: a discarded over-long message, a failed
checksum, low memory. All rare, none of them routine. The "can block" column therefore costs nothing in
practice, and everything on the channel is worth the stall it might cause.

**It changes the moment you uncomment `DEBUG`.** `SerialComs` then prints a line for *every* message in
each direction (`Received '…'`, `Sending '…'`). Sent to a raw `Serial`, that stalls `loop()` once per
message, on the very loop that has to keep calling `sendAndReceive()` — exactly the destabilisation
§24.3 describes, where the debug output meant to diagnose the link becomes what breaks it.

So the switches pair up like this:

| Situation | `DEBUG` | `ERROR_STREAM` | `setOutput()` |
|---|---|---|---|
| **Normal, including deployed** | commented out (default) | enabled (default) | `setOutput(Serial)` as above — faults only, never lost, no per-message stall |
| **Bringing the link up** | uncommented | enabled | §24.3 wiring: `bufferedOut.connect(Serial)` first, *then* `setOutput(bufferedOut)`. The chatter is voluminous and dropping some is fine |
| **Silence everything** | either | either | comment `setOutput()` out (§20.6). The error flags still work (§4.2) |

When you want quiet, reach for `DEBUG` rather than for `setOutput()`. Commenting out `DEBUG` removes the
chatter at compile time, recovers the flash, and leaves both `SerialComs` faults and SafeString errors
reporting — which are exactly the diagnostics you want to survive into production. Commenting out
`setOutput()` throws all three away at once.

### 20.4 Parsing a received message

`textReceived` is a SafeString, so tokenise it with the §7 methods. The checksum has **already been
stripped** — `checkCheckSum()` calls `msg.removeLast(2)` before verifying — so you see only your own
message text.

```cpp
if (!coms.textReceived.isEmpty()) {
  cSF(sfToken, 60);                     // >= receiveSize, the longest message text — see §20.5
  if (coms.textReceived.nextToken(sfToken, " ")) {    // destructive, which is fine here
    if (sfToken == "TEMP") {
      float t;
      if (coms.textReceived.nextToken(sfToken, " ") && sfToken.toFloat(t)) {
        handleTemperature(t);
      }
    }
  }
}
```

Destructive tokenising is safe because `textReceived` is cleared at the start of the next
`sendAndReceive()` anyway. **If you need the message after this loop pass, copy it first** into a
SafeString of your own.

### 20.5 Matching capacities

Three sizes have to line up. Two of the three mismatches are **loud** — they raise a SafeString error,
because they are coding errors in your own declarations (§4.1). Only the middle one is genuinely silent,
and it is the one to design against:

| Relationship | Consequence if wrong | Loud? |
|---|---|---|
| local reader `size` ≤ `sendSize` | the assignment `textToSend = sfCmd` fails — all-or-nothing, so **nothing** is queued | **Yes** — SafeString error on `textToSend`, plus a message |
| this side's `sendSize` == other side's `receiveSize` | a message longer than the far side's `receiveSize` overflows its reader and is discarded **there** | **No** — see below |
| token SafeString capacity ≥ `receiveSize` | `nextToken()` returns true with an **empty** token (§7.2) | **Yes** — error on both the source and the token |

The third row is `receiveSize`, **not** `textReceived`'s capacity, and the difference is worth being
clear about. `SerialComs` allocates its buffers as `_receiveSize + 2 + 2` — two bytes for the checksum,
two for the `XON` delimiter and the `'\0'` — so with `coms(60, 60)` the capacity is 63. But by the time
you parse it, both extras are gone: `checkCheckSum()` calls `msg.removeLast(2)`, and `XON` is the
delimiter, which `readUntilToken()` never puts in the token. The far side's `textToSend` holds at most
`sendSize` characters, so `textReceived`'s **content** is at most `receiveSize`, and a token SafeString
of `receiveSize` can always hold the longest possible token.

This is the one place where §7.2's blanket rule — token capacity ≥ the *source's capacity* — is stricter
than necessary. That rule is the safe default for when you do not know what bounds the content; here the
protocol bounds it, so `receiveSize` is the real requirement.

The middle row is silent for two compounding reasons. First, an over-long *message* is bad input, not a
coding error, so the receiving `SafeStringReader` discards it without raising anything (§4.8) — that is
correct and deliberate. Second, `SerialComs` exposes the reader as a plain `SafeString&`
(`SafeString& getTextReceived()`), so the receiving sketch **cannot** call `longTokenDiscarded()` on it —
that method belongs to `SafeStringReader`. The far side simply sees `textReceived` stay empty, exactly as
it would if nothing had been sent.

So there is no runtime test available to the far sketch. Match `sendSize` to the other side's
`receiveSize` by construction and the case cannot arise; that is the whole mitigation.

It is not invisible on the console, though. `SerialComs` checks `longTokenDiscarded()` itself and prints

```
 textReceived input overflowed receiveSize. Message discarded.
```

so to diagnose a suspected mismatch, enable `SafeString::setOutput()` on the **receiving** board and
watch for that line (§20.3 covers what else that channel then carries). What you cannot do is branch on
it in the receiving sketch.

### 20.6 Gotchas

- **A queued message can be discarded while the link is down.** In `sendNextMsg()`, the send is guarded
  by `isConnected()` but the clear is not:

  ```cpp
  if ((clearToSendFlag) && (isConnected() || isController)) {
    clearToSendFlag = false;
    if (!textToSendPtr->isEmpty() && isConnected()) { …send… }
    stream_io_ptr->print(XON);
    textToSendPtr->clear();          // runs even when nothing was sent
  }
  ```

  On the controller with the link down, a non-empty `textToSend` is cleared **without being
  transmitted**. Guard with `coms.isConnected()` before loading a message (as in §20.3), or be prepared
  to resend — `SerialComs` provides no delivery acknowledgement above the checksum.
- **Handle `textReceived` in the same loop pass.** The next `sendAndReceive()` clears it.
- **Only load `textToSend` when it is empty**, or you append to a message still waiting to go out.
- **A failed `connect()` is not fatal but is not harmless.** If allocation failed, `getTextToSend()` and
  `getTextReceived()` return the `SerialComs` object itself — a SafeString with a **1-byte static buffer
  and zero capacity**. So `coms.textToSend.print(...)` after a failed `connect()` raises capacity errors
  instead of crashing, and nothing is ever sent. Check the return of `connect()`.

---

## 21. Using SafeStringStream with SafeStringReader

`SafeStringStream` (§18) is a `Stream`, so a `SafeStringReader` connects to it exactly as it would to
`Serial`. This is the library's unit-testing arrangement: the same reader code, driven from canned data
at a controlled rate, with repeatable results.

### 21.1 The wiring

```cpp
#include <SafeStringReader.h>
#include <SafeStringStream.h>

createSafeStringReader(sfReader, 5, " ,\r\n");
cSF(sfTestData, 128);
SafeStringStream sfStream;

void setup() {
  Serial.begin(115200);
  SafeString::setOutput(Serial);

  sfTestData = F("start, stop, reset\n");
  sfStream.begin(sfTestData, 1200);   // release at 1200 baud
  sfReader.connect(sfStream);         // instead of sfReader.connect(Serial)
  sfReader.echoOff();                 // IMPORTANT — see 19.2
}

void loop() {
  if (sfReader.read()) {              // identical to live operation
    if (sfReader == "start") { … }
  }
}
```

Nothing else in the sketch changes. Because the reader polls `available()` every loop and
`SafeStringStream` releases bytes on those calls, the simulated baud clock advances naturally.

### 21.2 Echo must be off, or the test repeats forever

This is the trap that costs the most time. `SafeStringReader` echoes by writing back to the stream it
was connected to; writing to a `SafeStringStream` **appends to the data SafeString** (§18.3). So with
`sfReader.echoOn()`, every character read is written straight back onto the end of the test data and the
input never ends.

```cpp
sfReader.echoOff();                   // canned-data tests
```

Turn it off unless you deliberately want an endless loop of the same input — which is precisely what
`SafeStringStream_testdata.ino` does to keep its demo running. If you need to *see* the input during a
test, print it yourself from the token handler instead of enabling echo.

### 21.3 Size the RX buffer to match the real hardware

`SafeStringStream`'s default RX buffer is **8 bytes** — far smaller than the 64-byte UART buffer on an
UNO. Left at the default, the simulation is pessimistic: it reports overflows your real board would
absorb. Pass your own RX buffer to model the actual hardware:

```cpp
cSF(sfRxBuffer, 64);                          // model a 64-byte UART buffer
SafeStringStream sfStream(sfTestData, sfRxBuffer);
```

Then `RxBufferOverflow()` becomes a meaningful assertion rather than an artefact of the 8-byte default:

```cpp
size_t dropped = sfStream.RxBufferOverflow();   // non-zero => real hardware would lose data too
```

Combined with the bound from §19.1 — one `read()` call transfers at most `size + 1` characters — this
is how you find out, on the bench, whether a small reader can keep up with a given baud rate.

### 21.4 Testing the paths that are awkward to trigger by hand

This is the real payoff: the awkward branches inside `readUntilTokenInternal()` are hard to produce
reliably by typing, and trivial to produce from canned data.

| Path to exercise | Test data / setup |
|---|---|
| **Token longer than the reader** — discards the input and skips to the next delimiter, raising no error; check `longTokenDiscarded()` and the leading chars it leaves in the reader | a word longer than `size`, e.g. `"looooooooooooong stop\n"` with `size` 5 |
| **Empty tokens** between consecutive delimiters | `"start,,stop\n"`, with and without `sfReader.returnEmptyTokens()` |
| **The timeout path** — the last un-delimited token is returned when input goes quiet | `sfReader.setTimeout(100);` and test data **not** ending in a delimiter |
| **Which delimiter terminated a token** | mixed delimiters, checking `sfReader.getDelimiter()` right after `read()` returns true |
| **Slow arrival / split tokens** | a low `begin(baud)` so tokens straddle several `loop()` passes |
| **Data ≥ 0x80** (UTF-8) | any high-byte content |

The timeout case is worth spelling out, because it is deterministic here and not on a live port: once
the canned data is exhausted no further characters ever arrive, so the timeout fires exactly once and
returns whatever was buffered. That makes "does my code handle an unterminated final token?" a
repeatable test.

### 21.5 Running several cases in one sketch

Reading **consumes** the data SafeString, so each case needs fresh data. `begin(sf, baud)` replaces it:

```cpp
sfTestData = F("next test case\n");
sfStream.begin(sfTestData, 1200);
```

To also clear any partial token left in the reader between cases, reset it:

```cpp
sfReader.end();                 // return any final token, flush the input buffer, disconnect
sfReader.connect(sfStream);     // re-attach, clearing the read count
```

**That is all you need per case.** Echo and the timeout survive `end()`, so set them once in `setup()`
and leave them alone. Neither call disturbs them: in `SafeStringReader.cpp` the `echoInput = false;` and
`timeout_ms = 0;` lines inside `end()` are commented out (§9), and `connect()` only sets the stream
pointer and clears the read count.

The fact that `end()` leaves them alone is worth knowing for the opposite reason to the obvious one: if
you *want* echo off or the timeout cleared after an `end()`, you have to say so explicitly, because
`end()` will not restore those defaults for you. It leaves whatever was set before.

There is no assertion framework, so a harness is a table of cases plus printed results you compare
against expected output that you capture yourself on a known-good run. (The `.txt` files alongside the
example sketches are **not** that — they are the one-line Arduino IDE example descriptions.)

### 21.6 A minimal harness

```cpp
#include <SafeStringReader.h>
#include <SafeStringStream.h>

createSafeStringReader(sfReader, 5, " ,\r\n");   // max token 5 chars
cSF(sfTestData, 128);
cSF(sfRxBuffer, 64);                             // model a real UART buffer
SafeStringStream sfStream(sfTestData, sfRxBuffer);

const char* cases[] = {
  "start, stop\n",                 // normal
  "looooooooooooong stop\n",       // over-long token -> skipped
  "start,,stop\n",                 // consecutive delimiters
  "start, unterminated"            // exercises the timeout path
};
size_t caseIdx = 0;

void runCase(const char* data) {
  Serial.print(F("\n--- case: ")); Serial.println(data);
  sfTestData = data;
  sfStream.begin(sfTestData, 1200);
  sfReader.end();                  // clear any partial token from the previous case
  sfReader.connect(sfStream);      // echo and timeout are NOT disturbed by either call
}

void setup() {
  Serial.begin(115200);
  SafeString::setOutput(Serial);   // show coding errors; input problems are reported by the code below
  sfReader.echoOff();              // set once — both survive every end()/connect() below (§9)
  sfReader.setTimeout(100);        // so the last un-delimited token is returned
  runCase(cases[caseIdx]);
}

void loop() {
  if (sfReader.read()) {
    Serial.print(F("token: '")); Serial.print(sfReader.c_str());
    Serial.print(F("' delim: ")); Serial.println(sfReader.getDelimiter());
  } else if (sfReader.longTokenDiscarded()) {
    // the library prints nothing for this — over-long input is data, not a coding error (§4.8)
    Serial.print(F("too long, starts with: '")); Serial.print(sfReader);
    Serial.println('\'');
  }

  // case finished once the data is drained and nothing is pending
  if (sfTestData.isEmpty() && !sfReader.isSkippingToDelimiter() && (sfStream.available() == 0)) {
    size_t dropped = sfStream.RxBufferOverflow();
    if (dropped) { Serial.print(F("!! dropped ")); Serial.println(dropped); }
    caseIdx++;
    if (caseIdx < (sizeof(cases) / sizeof(cases[0]))) {
      runCase(cases[caseIdx]);
    } else {
      Serial.println(F("\nall cases done"));
      while (1) {}
    }
  }
}
```

Leave `SafeString::setOutput(Serial)` on in tests, but note what that now means: since v4.1.45 the
over-long case prints **nothing** from the library, so any `!! Error:` appearing during a run is a real
coding error and the run has failed. The expected result for the over-long case is the
`longTokenDiscarded()` branch in your own harness firing, with a silent console.

### 21.7 Switching back to the live port

Keep the swap to a single `#define` so the code under test is provably identical in both modes (§18.6):

```cpp
#ifdef TEST_DATA
  sfReader.connect(sfStream);
#else
  sfReader.connect(Serial);
#endif
```

---

## 22. Using BufferedOutput with SafeStringReader

`BufferedOutput` (§14) is a `Stream`, and its `read()`, `available()` and `peek()` pass **straight
through** to the underlying stream. So a `SafeStringReader` can connect to it: input is read unbuffered
from the port, while everything the reader *writes* — its echo — goes into the output buffer.

```cpp
createSafeStringReader(sfReader, 15, " ,\r\n");
createBufferedOutput(bufferedOut, 80, DROP_UNTIL_EMPTY);

void setup() {
  Serial.begin(115200);
  bufferedOut.connect(Serial);
  sfReader.connect(bufferedOut);      // not Serial
  sfReader.echoOn();                  // echo is now buffered, so it cannot block
}

void loop() {
  bufferedOut.nextByteOut();          // release buffered bytes every loop
  if (sfReader.read()) { … }
}
```

### 22.1 Why: echo is output, and output blocks

`echoOn()` is the reason to do this. Echoing doubles your outbound traffic at precisely the moment input
is arriving, and the reader echoes with a plain `input.print((char)c)`. Against a raw `Serial` that call
blocks as soon as the TX buffer fills — so turning echo on can reintroduce exactly the stall
`SafeStringReader` exists to avoid. Routed through `BufferedOutput`, the echo lands in the ring buffer and
returns immediately.

If you are not using `echoOn()`, connecting the reader to `bufferedOut` buys you nothing over
`connect(Serial)` except the extra release points in §22.3.

### 22.2 It also fixes echo ordering

A subtler reason, and the one most likely to produce a confusing symptom. Consider the *split* wiring:

```cpp
bufferedOut.connect(Serial);
sfReader.connect(Serial);     // echo goes DIRECTLY to Serial
sfReader.echoOn();
bufferedOut.println(F("some earlier program output"));
```

Program output is queued in the buffer and released over the following loops, while the echo bypasses
the queue and reaches the port immediately. The echoed characters therefore appear **ahead of** earlier
program output that is still buffered — the console shows the two interleaved in the wrong order, which
reads like a logic bug in the sketch.

Connecting the reader to `bufferedOut` puts echo and program output in the **same queue**, so they emerge
in the order they were generated.

### 22.3 Free release points

`BufferedOutput::read()`, `available()` and `peek()` each call `nextByteOut()` before delegating. Since
the reader polls the stream every `loop()`, buffered output gets released on every poll as well as at
your explicit `nextByteOut()` call. In a loop with a slow section this measurably improves output
smoothness at no cost.

Re-entrancy is not a concern: `nextByteOut()` holds a per-instance `inNextByteOut` flag and
returns immediately if called recursively.

You still need `bufferedOut.nextByteOut()` at the top of `loop()` — the reader's polling is a supplement,
not a replacement, and it stops happening if you ever guard `sfReader.read()` behind a condition.

### 22.4 Echo can be dropped

Echo is ordinary buffered output, so it obeys the buffer's mode (§14). In `DROP_UNTIL_EMPTY` or
`DROP_IF_FULL`, a burst of input can fill the buffer and the echo of subsequent characters is discarded,
marked with `~~` in the console. The input itself is unaffected — only the echo of it is lost — but a
user watching their typing vanish will report it as dropped input.

If echo fidelity matters, size the buffer for the worst-case burst (echo volume equals input volume) or
accept the gaps. `BLOCK_IF_FULL` guarantees complete echo but reinstates the blocking, defeating the
purpose.

### 22.5 Input is *not* buffered by this arrangement

The reader still reads directly from the port, and still transfers at most `size + 1` characters per
`read()` call (§19.1). `BufferedOutput` does nothing for input.

Choosing between the three arrangements:

| Wiring | Input | Echo / output |
|---|---|---|
| `sfReader.connect(Serial)` | unbuffered | unbuffered — echo can block |
| `sfReader.connect(bufferedOut)` | unbuffered | **buffered**, correctly ordered |
| `sfReader.connect(bufferedIn)` (§19) | **buffered** | passes through to Serial — echo can block |
| `bufferedIn` → `bufferedOut` → `Serial` (§19.5) | **buffered** | **buffered** — needs both `nextByteIn()` and `nextByteOut()` |

Pick by which side is actually hurting: `loopTimer` (§13.2) tells you whether the loop is blocking, and
`BufferedInput`'s statistics (§19.4) tell you whether input is being lost.

### 22.6 Complete example

```cpp
#include <SafeStringReader.h>
#include <BufferedOutput.h>
#include <loopTimer.h>

createSafeStringReader(sfReader, 15, " ,\r\n");
createBufferedOutput(bufferedOut, 80, DROP_UNTIL_EMPTY);

void setup() {
  Serial.begin(115200);
  SafeString::setOutput(Serial);      // library errors go direct, not through the buffer
  bufferedOut.connect(Serial);
  sfReader.connect(bufferedOut);
  sfReader.echoOn();
}

void loop() {
  bufferedOut.nextByteOut();
  loopTimer.check(bufferedOut);       // timing report is buffered too

  if (sfReader.read()) {
    bufferedOut.print(F("> "));       // echo and this reply share one queue, so they stay in order
    bufferedOut.println(sfReader.c_str());
  } else if (sfReader.longTokenDiscarded()) {
    bufferedOut.print(F("too long: "));
    bufferedOut.println(sfReader.c_str());
  }
}
```

Note `SafeString::setOutput(Serial)` rather than `setOutput(bufferedOut)`: error messages are diagnostics
you want unconditionally, and routing them through a dropping buffer risks losing the one message that
explains a failure. The cost is that error text can appear out of order relative to buffered output —
an acceptable trade for diagnostics.

---

## 23. Using SafeStringStream with BufferedOutput

There are two quite different arrangements here. The first is the common one; the second is a test
technique with real caveats.

### 23.1 Coexisting — the usual case

In a test sketch the two normally do **not** connect to each other. `SafeStringStream` supplies canned
input, `BufferedOutput` carries results to the console, and they simply sit on opposite sides of the
sketch:

```cpp
#include <SafeStringReader.h>
#include <SafeStringStream.h>
#include <BufferedOutput.h>

createSafeStringReader(sfReader, 5, " ,\r\n");
cSF(sfTestData, 128);
SafeStringStream sfStream;                        // input side
createBufferedOutput(bufferedOut, 80, DROP_UNTIL_EMPTY);   // output side

void setup() {
  Serial.begin(115200);
  SafeString::setOutput(Serial);
  bufferedOut.connect(Serial);

  sfTestData = F("start, stop\n");
  sfStream.begin(sfTestData, 1200);
  sfReader.connect(sfStream);
  sfReader.echoOff();               // required — see §21.2
}

void loop() {
  bufferedOut.nextByteOut();
  if (sfReader.read()) { bufferedOut.println(sfReader.c_str()); }
}
```

This is what the library's own example sketches do. If that is what you need, stop here.

### 23.2 Capturing output into a SafeString

`BufferedOutput::connect()` requires a **`Stream`**. A `SafeString` is only a `Print`, so it cannot be a
`connect()` target on its own — `SafeStringStream` is the adapter that makes one usable as a `Stream`.
Pointing a `BufferedOutput` at one therefore captures everything it releases into a SafeString you can
inspect:

```cpp
cSF(sfCapture, 200);                        // where the output lands
SafeStringStream captureStream(sfCapture);
createBufferedOutput(bufferedOut, 80, DROP_UNTIL_EMPTY);
millisDelay checkDelay;

void setup() {
  Serial.begin(115200);
  SafeString::setOutput(Serial);
  captureStream.begin(9600);                // begin() first — see §23.3
  bufferedOut.connect(captureStream, 9600); // pass a baud rate — see §23.3
  checkDelay.start(1000);
}

void loop() {
  bufferedOut.nextByteOut();                // releases into sfCapture
  // … code under test prints to bufferedOut …

  if (checkDelay.justFinished()) {
    checkDelay.repeat();
    Serial.print(F("captured: '")); Serial.print(sfCapture.c_str()); Serial.println("'");
    sfCapture.clear();                      // make room for the next batch
  }
}
```

**When this is worth doing:** to test the *buffering configuration itself*. Whether an 80-byte
`DROP_UNTIL_EMPTY` buffer preserves the messages you care about under load, where the `~~` drop marks
land, and whether `allOrNothing` is splitting your lines — none of that is observable by reading the
code, and all of it shows up in the captured text.

**When it is not:** if you only want to capture what your code prints, skip the machinery. `SafeString`
is a `Print`, so print straight into it:

```cpp
cSF(sfCapture, 200);
sfCapture.print(F("value = ")); sfCapture.println(x);   // no Stream, no buffer, no baud rate
```


### 23.3 Traps

- **Pass an explicit baud rate to `connect()`.** `bufferedOut.connect(stream)` with no baud rate falls
  back to the stream's `availableForWrite()`, which for a `SafeStringStream` is the free space in its
  SafeString. If that is ≤ 2 at connect time — a small or already-full capture buffer — `BufferedOutput`
  enters its infinite `while(1)` "availableForWrite() returns 0" loop (§14) and the sketch appears
  hung. `connect(captureStream, 9600)` skips the `availableForWrite()` path entirely and uses its timer
  instead.
- **`begin()` before `connect()`.** `SafeStringStream::write()` reaches `releaseNextByte()`, which before
  `begin()` prints an error and then `delay(5000)` (§18.1). Call `captureStream.begin(baud)` first.
- **The capture SafeString fills.** Once full, further writes are rejected with capacity errors (all-or-
  nothing, §1) and the output is lost. Size it for the batch, or drain it periodically with `clear()`.
  If it does fill, the capacity error and its message tell you the capture is incomplete — that is a
  sizing bug in the harness to fix, not a condition to handle at runtime.
- **Never use one `SafeStringStream` for both input and capture.** Writing appends to the very SafeString
  being read, which is the echo feedback loop of §18.5 — the captured output becomes new input and the
  test never terminates. Use two separate `SafeStringStream` objects over two separate SafeStrings.
- **`'\0'` is dropped.** `SafeString::write(0)` refuses the byte and raises an error, so binary output
  cannot be captured this way — text only.
- **Nothing is captured until `nextByteOut()` runs.** The buffer releases on that call (and on
  `read`/`available`/`peek`, §22.3), so a capture check placed before it in `loop()` reads stale content.

### 23.4 Which stream to test against

| Goal | Arrangement |
|---|---|
| test input parsing | `SafeStringStream` → `SafeStringReader` (§21) |
| test what your code prints | print into a SafeString directly — it is a `Print` |
| test the buffer's drop/throttle behaviour | `BufferedOutput` → `SafeStringStream` → SafeString (§23.2) |
| see output while testing input | `BufferedOutput` → `Serial`, alongside the input stream (§23.1) |

---

## 24. Using SerialComs with BufferedOutput

`BufferedOutput` is a `Stream`, so `coms.connect(bufferedOut)` compiles and appears to work. **Do not do
it.** This section explains why, and where `BufferedOutput` does belong in a `SerialComs` sketch.

### 24.1 Never put the link through a dropping BufferedOutput

`SerialComs` runs a framed protocol: `<messageText><2 hex checksum><XON>` (§15). Every byte is
load-bearing. In `DROP_IF_FULL` or `DROP_UNTIL_EMPTY` mode, `BufferedOutput` does two things that are
fatal to it:

1. **It discards bytes.** A dropped checksum digit fails verification; a dropped `XON` means the far side
   never gets its turn to send and the link times out.
2. **It injects bytes.** When output is dropped, `writeDropMark()` writes the literal sequence
   `~~\r\n` (or `~~\n`) *into the stream*. Those characters are not `XON`, so the receiver treats them as
   message content — corrupting the message body and its checksum.

The failure looks like an intermittent, load-dependent link: fine when idle, dropping out under traffic,
with `"CheckSum failed"` and `"timeout without receiving terminating XON"` in the debug log. Nothing
points at the output buffer.

### 24.2 `BLOCK_IF_FULL` is not a fix either

`BLOCK_IF_FULL` genuinely never writes drop marks — the source comments the branch `// may block but no
drop marks here` — so it does not corrupt the protocol. But it spins in `delay(1)` until space frees up,
which stalls `loop()`, and `sendAndReceive()` must be called every loop for the protocol to work. Adding
latency and stalls to a link with a 5-second connection timeout and strict turn-taking trades a
corruption bug for a timing one.

> **Rule:** give `SerialComs` the raw stream. `coms.connect(Serial1)`, never `coms.connect(bufferedOut)`.

### 24.3 Where BufferedOutput does belong: the console

The genuine pairing is `BufferedOutput` on the *other* port — and specifically for `SerialComs`' own
debug output, which is the thing most likely to break the link.

This applies **only while the per-message tracing is enabled**. It ships disabled: `SerialComs.cpp` has
`// #define DEBUG SafeString::Output` commented out, and every tracing statement is inside
`#ifdef DEBUG` (§15). So what follows is about bringing a link up, not about a finished sketch — with
the default there is nothing voluminous to buffer, since the `ERROR_STREAM` messages that remain enabled
are rare by nature.

Uncomment that line and `SerialComs` prints on **every message in both directions**:

```cpp
DEBUG.print(F("Received '")); DEBUG.write(…); DEBUG.println("'");
DEBUG.print(F("Sending '"));  DEBUG.print(*textToSendPtr); DEBUG.println("'");
```

Against a raw `Serial`, that chatter blocks as soon as the console TX buffer fills — stalling the very
`loop()` that has to keep calling `sendAndReceive()`. The debug output intended to diagnose the link can
therefore be what destabilises it. Routing it through a `BufferedOutput` on the console removes the
stall:

```cpp
bufferedOut.connect(Serial);            // console — connect BEFORE setOutput
SafeString::setOutput(bufferedOut);     // SerialComs debug + SafeString errors, non-blocking
coms.connect(Serial1);                  // link — raw stream, never buffered
```

Connect the `BufferedOutput` first: until it has a stream, its `write()` returns 0 and the output is
silently discarded (it does not recurse, but you lose the messages).

This is the one case where routing diagnostics through a dropping buffer is the right call — the
alternative is not "reliable diagnostics" but "diagnostics that break what they are measuring". Accept
the occasional `~~` in the console log.

### 24.4 Console buffer size and mode

- Use a **drop** mode. `BLOCK_IF_FULL` on the console reintroduces the stall and can time out the link.
- Size it for a whole message plus your prefix: with the default `receiveSize` of 60, an 80–100 byte
  buffer holds one forwarded message and its label without dropping.
- `DROP_UNTIL_EMPTY` keeps surviving lines readable; `DROP_IF_FULL` keeps more total text but fragments
  it. For message logs, prefer `DROP_UNTIL_EMPTY`.

### 24.5 Complete arrangement

```cpp
#include "SerialComs.h"
#include <BufferedOutput.h>

SerialComs coms(60, 60);
createBufferedOutput(bufferedOut, 100, DROP_UNTIL_EMPTY);

void setup() {
  Serial.begin(115200);       // console
  Serial1.begin(9600);        // link

  bufferedOut.connect(Serial);        // 1. connect the buffer
  SafeString::setOutput(bufferedOut); // 2. then route debug through it

  coms.setAsController();
  if (!coms.connect(Serial1)) {       // 3. link gets the RAW stream
    while (1) { Serial.println(F("Out-Of-Memory")); delay(3000); }
  }
}

void loop() {
  bufferedOut.nextByteOut();          // release console output every loop
  coms.sendAndReceive();              // link I/O — must run every loop

  if (!coms.textReceived.isEmpty()) {
    bufferedOut.print(F("remote: "));
    bufferedOut.println(coms.textReceived.c_str());
  }
}
```

This whole arrangement is only needed **while the per-message tracing is enabled**. `DEBUG` is off by
default (§15), so unless you have uncommented it there is little coming from the library and no reason
to buffer it. Once the link is working, comment `#define DEBUG` back out — the quietest option is always
the safest for link timing, and it recovers about 450 bytes of flash. That leaves `ERROR_STREAM` and
SafeString's own error messages still reaching `setOutput()`, which is what you want. Keep `loopTimer`
(§13.2) in during commissioning to confirm nothing in the loop is long enough to threaten the
turn-taking.

---

## 25. Pairings: quick verdicts

Every combination of the library's nine classes, with a verdict. Use this to answer "can I combine X and
Y?" before writing code.

**"No interaction"** means exactly that: the two classes never touch. Using both is just calling two
independent things from `loop()`, and each one's own rules apply unchanged — there is no combined
behaviour to learn and nothing extra to configure.

| Pairing | Verdict |
|---|---|
| SafeString × SafeStringReader | `SafeStringReader` **is a** `SafeString` — use the token directly (§9) |
| SafeString × SafeStringStream | `SafeStringStream` adapts a SafeString into a `Stream`: reading replays it, writing appends (§18) |
| SafeString × BufferedOutput | Print a SafeString to it (SafeString is `Printable`). A SafeString **cannot** be a `connect()` target — it is a `Print`, not a `Stream` (§23.2) |
| SafeString × BufferedInput | `sfStr.read(bufferedIn)` and `readUntil()` treat it as any other Stream (§8.2) |
| SafeString × SerialComs | `textToSend` / `textReceived` **are** SafeStrings; the whole §6 API applies (§15, §20) |
| SafeString × millisDelay | No interaction |
| SafeString × loopTimer | No interaction |
| SafeString × PinFlasher | No interaction |
| SafeStringReader × SafeStringStream | Canned-input test harness. **Echo must be off** or the test never ends (§21) |
| SafeStringReader × BufferedOutput | Buffers the reader's *echo* and fixes echo/output ordering (§22) |
| SafeStringReader × BufferedInput | Buffers *input*; needed because one `read()` takes at most `size + 1` chars (§19) |
| SafeStringReader × SerialComs | **Separate streams only** — the link belongs to SerialComs' internal reader (§20) |
| SafeStringReader × millisDelay | `setTimeout()` for token timeout, `millisDelay` for inactivity (§13.3) |
| SafeStringReader × loopTimer | Use it to confirm the reader keeps up; no direct interaction (§19.3, §22.5) |
| SafeStringReader × PinFlasher | No interaction |
| SafeStringStream × BufferedOutput | Captures output into a SafeString. Pass an explicit baud rate to `connect()` (§23) |
| SafeStringStream × BufferedInput | **Useful for testing** — reproduce the burst that overflows the buffer (§25.1) |
| SafeStringStream × SerialComs | **Don't** — the protocol needs a live responding peer (§25.1) |
| SafeStringStream × millisDelay | No interaction |
| SafeStringStream × loopTimer | No interaction |
| SafeStringStream × PinFlasher | No interaction |
| BufferedOutput × BufferedInput | Chain `bufferedIn` → `bufferedOut` → `Serial` to buffer both directions; both `nextByte…()` calls required (§19.5) |
| BufferedOutput × SerialComs | **Never on the link** — drop marks corrupt the frame. Correct for the console and debug output (§24) |
| BufferedOutput × loopTimer | `loopTimer.check(bufferedOut)` works, but changes what `max - prt` means (§25.1) |
| BufferedOutput × millisDelay | No interaction |
| BufferedOutput × PinFlasher | No interaction |
| BufferedInput × SerialComs | **Safe**, unlike BufferedOutput — it never drops or injects on the paths SerialComs uses (§25.1) |
| BufferedInput × millisDelay | No interaction |
| BufferedInput × loopTimer | No interaction — though `loopTimer` is how you discover you need BufferedInput |
| BufferedInput × PinFlasher | No interaction |
| SerialComs × millisDelay | It already owns one for its 5 s timeout — use `isConnected()`, not your own timer (§25.1) |
| SerialComs × loopTimer | Use it to confirm `sendAndReceive()` runs often enough (§24.5) |
| SerialComs × PinFlasher | No interaction |
| millisDelay × loopTimer | `loopTimer` uses a `millisDelay` internally for its 5 s report period. Nothing to do (§13.2) |
| millisDelay × PinFlasher | `PinFlasher` inherits `millisDelay` **protected** — the timing methods are not public (§16) |
| loopTimer × PinFlasher | No interaction |

### 25.1 Notes on the five that need more than a line

**SafeStringStream × BufferedInput — validating buffer size on the bench.**
`BufferedInput`'s sizing statistics (§19.4) only tell you something once real traffic has stressed them.
`SafeStringStream` at a realistic baud rate reproduces that traffic deterministically: feed a burst,
call `nextByteIn()` at your loop's real rate, and read `maxBufferUsed()`. Give the stream its own RX
buffer sized like the hardware UART (§21.3), or the 8-byte default makes the test pessimistic.

**SafeStringStream × SerialComs — not a viable test.**
`SerialComs` is a turn-taking protocol: the controller prompts, the peer answers, each `XON` hands over
the right to send (§15). A `SafeStringStream` replays a fixed script — it cannot compute a checksum
over what it just received or answer at the right moment, so the exchange stalls at the first turn.
Anything you script is valid only for one specific timing. Test `SerialComs` with two real endpoints.

**BufferedOutput × loopTimer — the report still works, one figure changes meaning.**
`loopTimer.check(bufferedOut)` is fine and is what the library's own examples do. Two caveats. The
`max - prt` figure is the microseconds the report's own printing consumed, and `print()` adds it back to
`lastLoopRun_us` to exclude itself; buffered, that measures a few ring-buffer writes rather than the real
serial time, so it collapses to near-zero. That is the intended benefit — printing no longer blocks — but
the number is no longer "what this report cost you". Second, in a DROP mode a report can be dropped
entirely, so a missing 5-second line is lost output, not a stalled loop.

**BufferedInput × SerialComs — safe, unlike BufferedOutput.**
§24 bans `BufferedOutput` from the link because it drops protocol bytes and injects `~~\r\n` drop marks.
`BufferedInput` does neither: its `write()` passes straight through to the stream untouched, and
`nextByteIn()` only moves as many bytes as currently fit (`if (rb_avail < avail) avail = rb_avail;`), so
it never discards what it has accepted and never inserts anything. Putting it in front of a `SerialComs`
link is therefore sound, and adds receive depth if the loop has an unavoidably slow section. It is rarely
needed — messages are bounded by `receiveSize` (60 by default) against a typical 64-byte UART buffer —
so fix a slow loop first. The library ships no example of this arrangement; verify it on your hardware.

**SerialComs × millisDelay — do not add a reconnect timer.**
`SerialComs` already contains a `millisDelay` driving its 5-second connection timeout, and re-prompts the
peer itself when the link goes quiet (§15). Poll `coms.isConnected()` for link state rather than timing
it yourself. Use your own `millisDelay` for application concerns the protocol knows nothing about — how
often to *generate* a message, or how long to wait before alarming on a persistently dead link — and load
`textToSend` only when connected, or the message can be discarded unsent (§20.6).

---

## 26. Rules checklist for generated code

0. **Decide first whether the text is yours or the outside world's.** For your own text, `=` / `+=` and
   `hasError()` are right, and an error means a bug to fix. For anything arriving from a Stream, an app,
   a file or a user, use `readFrom()` + `isFull()`, `SafeStringReader` + `longTokenDiscarded()`, and the
   `toXxx()` return values — and never let it raise a SafeString error (§4.8).
1. Call `SafeString::setOutput(Serial);` early in `setup()` during development.
2. Create every SafeString with `cSF` / `cSFA` / `cSFP` / `cSFPS`. Never call the constructor directly.
3. Prefer `cSFA` over `cSFP`; prefer `cSFPS` over `cSFP` whenever the buffer size is known.
4. Size the buffer for the **worst case** text your own code builds, then check `hasError()`. For input,
   size it one char larger than the longest valid input and test `isFull()` immediately after
   `readFrom()`, before anything shortens it.
5. Declare SafeString function parameters as `SafeString&`. Never return a SafeString.
6. Use `+=` / `-=` / `concat()` / `prefix()`. There is no `+`. Parenthesise to cascade:
   `sf = a + b;` becomes `(sf = a) += b;` — or just write the steps out on separate lines (§6.3).
7. Give `token`/`result` SafeStrings a capacity **≥** the source SafeString's capacity. For
   `readUntilToken()` this is a checked requirement, not a suggestion.
8. Check the boolean return of every `toInt` / `toLong` / `toFloat` / `toDouble` before using the value.
   They never set the error flag, and they already tolerate leading and trailing whitespace, so do not
   add a `trim()` on the parse path.
9. Remember `substring(result, begin, end)` excludes `end`.
10. Treat `c_str()` as read-only, and never let it outlive the underlying buffer.
11. Never store `'\0'` in a SafeString.
12. For stream input use `SafeStringReader`; if you tokenise a stream manually with `nextToken()`, pass
    `returnLastNonDelimitedToken = false`.
13. Do not `free()` a buffer that a SafeString still wraps, and do not return references to SafeStrings
    that wrap stack buffers.
14. Check `SafeString::errorDetected()` in `setup()` to catch construction errors in globals. This only
    works as a bug sweep if nothing in the program raises errors on ordinary input — see rule 0.
15. Do not mix `strcpy`/`strcat`/`sprintf` with SafeString on the same buffer — wrap it once and stay
    inside the SafeString API.
16. Report rejected input to whoever sent it, not to `SafeString::setOutput()`. That channel is for
    diagnosing your own code.

---

## 27. Worked example — parse and validate a delimited command line

```cpp
#include <SafeString.h>

//  Parses commands of the form:  SET <name> <value>\n
//  Uses only fixed buffers; no heap allocation; every field length-checked.
//
//  sfLine  - accumulates one line of input (destructively tokenised)
//  sfToken - receives each field; sized equal to sfLine so any field can fit
//  sfName  - holds the validated name; MAX_NAME+1 so isFull() detects an over-long one
const size_t MAX_NAME = 16;
createSafeString(sfLine, 60);
createSafeString(sfToken, 60);
createSafeString(sfName, MAX_NAME + 1);

void setup() {
  Serial.begin(115200);
  SafeString::setOutput(Serial);        // enable error reporting
}

/**
 * Handles one complete command line held in sfLine.
 * Consumes sfLine destructively via nextToken().
 * Reports, rather than ignores, malformed input.
 */
void handleLine() {
  sfLine.trim();

  // field 1: the verb
  if (!sfLine.nextToken(sfToken, " ")) { return; }        // no token -> nothing to do
  if (!sfToken.equalsIgnoreCase("SET")) {
    Serial.print(F("Unknown command: ")); Serial.println(sfToken.c_str());
    return;
  }

  // field 2: the name, length-checked WITHOUT raising an error.
  // sfName has capacity MAX_NAME+1, one more than the longest name accepted, so a full sfName
  // after readFrom() can only mean the input was too long.  Using sfName = sfToken here would
  // raise a SafeString error for what is only a long name, and would leave sfName empty so the
  // message below could not quote it.  See section 4.8.
  if (!sfLine.nextToken(sfToken, " ")) { Serial.println(F("Missing name")); return; }
  sfName.clear();
  sfName.readFrom(sfToken);
  if (sfName.isFull()) {                                  // name longer than MAX_NAME chars
    Serial.print(F("Name too long: ")); Serial.println(sfToken.c_str());
    return;
  }

  // field 3: the value, strictly validated
  if (!sfLine.nextToken(sfToken, " ")) { Serial.println(F("Missing value")); return; }
  int value;
  if (!sfToken.toInt(value)) {
    Serial.print(F("Not an integer: ")); Serial.println(sfToken.c_str());
    return;
  }

  Serial.print(F("SET ")); Serial.print(sfName.c_str());
  Serial.print(F(" = "));  Serial.println(value);
}

void loop() {
  // Non-blocking: append whatever has arrived, act only on a complete line.
  // NOTE: readUntil() also returns true when sfLine fills up without a '\n',
  // so an over-long line is processed as-is rather than overflowing anything.
  if (sfLine.readUntil(Serial, "\n")) {
    handleLine();              // trim() strips the trailing '\n' that readUntil appended
    sfLine.clear();            // discard any unconsumed remainder before the next line
  }
}
```
