# SafeString — AI Reference Guide for Safe C-String Handling

**Library:** SafeString v4.1.44 by Matthew Ford, Forward Computing and Control Pty. Ltd.
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

## 4. Error handling — how to know something went wrong

### 4.1 Turn on messages

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

### 4.2 Check the flags

| Call | Scope | Note |
|---|---|---|
| `sfStr.hasError()` | this SafeString | **reading clears the flag** |
| `SafeString::errorDetected()` | any SafeString anywhere | static; **reading clears the flag** |

Both are "sticky until read". Read them at a point where you can act on the result:

```cpp
sfCmd = userInput;                      // may not fit
if (sfCmd.hasError()) {
  // handle: input too long / invalid — sfCmd is unchanged and still valid
}
```

### 4.3 A real error message

```
Error: msgStr.concat() needs capacity of 8 for the first 3 chars of the input.
        Input arg was '598'
        msgStr cap:5 len:5 'A0 = '
```

The variable name (`msgStr`) comes from the `#name` stringification inside the creation macros.

### 4.4 Inspect a SafeString at runtime

```cpp
sfStr.debug();                          // full dump: name, capacity, length, contents
sfStr.debug("after parse: ");           // with a title (const char*, F(".."), or SafeString)
sfStr.debug(false);                     // suppress contents
Serial.println(sfStr.debug());          // debug() returns "" so this composes cleanly
```

`debug()` is always compiled in and is *not* affected by `setVerbose()`; it still needs `setOutput()` to
have somewhere to print.

### 4.5 Globals constructed before `setup()`

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

### 4.6 Compile-time error control

`SafeString.h` defines `SSTRING_DEBUG`. Commenting it out removes all error-message code **and** the RAM
holding SafeString names — smaller binary, no diagnostics. Normally leave it defined and control output
at runtime with `setOutput()` / `turnOutputOff()`.

---

## 5. Mapping unsafe C-string idioms to SafeString

This is the practical core of the library. `sf` denotes a SafeString.

| Unsafe C | Why it's unsafe | SafeString replacement |
|---|---|---|
| `strcpy(dst, src)` | no bound at all | `sfDst = src;` |
| `strncpy(dst, src, n)` | may leave `dst` unterminated; truncates silently | `sfDst = src;` then check `hasError()` |
| `strcat(dst, src)` | no bound | `sfDst += src;` / `sfDst.concat(src);` |
| `strncat(dst, src, n)` | easy off-by-one on the size argument | `sfDst.concat(src, n);` — **stricter**: raises an error and appends nothing if `strlen(src) < n`, where `strncat` would stop at the `'\0'` |
| `sprintf(buf, "%d", v)` | unbounded | `sf = v;` or `sf.print(v);` |
| `snprintf(buf, n, ...)` | truncates silently | `sf.clear(); sf.print(a); sf += b;` + `hasError()` |
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

---

## 6. Complete API reference

`sf`, `sfOther`, `token`, `result` are SafeStrings. Return type `SafeString&` means the call returns
`*this` so calls can be cascaded: `sf.clear().concat("a").concat(1);`

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
forbids by design. To cascade operators, parenthesise: `(sf += 'a') += 5;`

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
  value fit `abs(width)`. *(Through v4.1.43 the comments in `SafeString.h` and `SafeString.cpp` said
  "< 7", i.e. 6, contradicting the code; corrected in V4.1.44.)*
- A negative `decs` raises an error.
- If it still won't fit with `decs == 0`, an error is raised.
- Pass `decs = 0` to format `int`/`long` values.
- Special values are **not** errors — they print as the literal text `nan`, `inf`, `ovf` or `-ovf`
  (`ovf`/`-ovf` at `d >= 4294967039.0` / `d <= -4294967039.0`, threshold empirically).
   Nothing sets the error flag, so a `nan` reaches your output silently.

### 6.4 Comparison

`compareTo(SafeString&|const char*)` → `-1` / `0` / `+1`.
`equals(SafeString&|const char*|char)`, `equalsIgnoreCase(...)`,
`equalsConstantTime(SafeString&)` (timing-attack-resistant, for comparing secrets).
Operators `==`, `!=`, `<`, `>`, `<=`, `>=` against `SafeString&` and `const char*` (plus `==`/`!=` against
`char`). All return `unsigned char`.

### 6.5 Prefix / suffix tests

```cpp
sf.startsWith(c | "str" | sfOther, fromIndex = 0);
sf.startsWithIgnoreCase(c | "str" | sfOther, fromIndex = 0);
sf.endsWith(c | "str" | sfOther);
sf.endsWithCharFrom("\r\n");    // true if the last char is any one of these
```

`fromIndex > length()` raises an error and returns false. `fromIndex == length()` or `-1` returns false
without error.

### 6.6 Searching — all return `int`, `-1` when not found

```cpp
sf.indexOf(char | "str" | sfOther, fromIndex = 0);
sf.lastIndexOf(char | "str" | sfOther);              // searches backwards from the end
sf.lastIndexOf(char | "str" | sfOther, fromIndex);   // backwards from fromIndex, inclusive
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
sf.replace(findChar, "replacement" | sfReplace);
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

> **Upstream boundary bugs, fixed in V4.1.44.** Through v4.1.43 the conversions used `strtol`'s
> *return value* as their overflow test, which cannot distinguish an overflow from a valid input at the
> limit. Two consequences, both fixed here by testing `errno == ERANGE` instead:
>
> - **`toLong` / `toUnsignedLong` / the bin-oct-hex variants / `toInt64_t` rejected valid limits.** On a
>   32-bit `long`, `"2147483647"`, `"-2147483648"` and `"4294967295"` returned **false**. They now parse
>   correctly.
> - **`toInt()` returned a wrong value on 32-bit cores.** Its guard is `result > INT_MAX`, but where
>   `sizeof(int) == sizeof(long)` (ESP32, ESP8266, RP2040, SAMD) `INT_MAX == LONG_MAX`, so it could never
>   fire: `"99999999999999".toInt(i)` returned **`true`** with `i = 2147483647`. It now returns false.
>   AVR was unaffected, its `int` being 16-bit.
>
> **On an unpatched upstream library, both bugs are live** — treat `toInt()` on 32-bit boards as unable
> to detect overflow, and range-check large values yourself with `strtol` + `errno`.

### 6.11 UTF-8 (v4.1.42+)

```cpp
int i = sf.utf8index(endIdx);       // largest index <= endIdx that does not split a code point
int j = sf.utf8nextIndex(startIdx); // start of the next code point (startIdx+1 .. startIdx+4)

sf.substring(result, 0, sf.utf8index(endIdx));                     // safe truncation
sf.substring(result, sf.utf8index(idx), sf.utf8nextIndex(idx));    // extract one code point
```

For pure ASCII these reduce to `endIdx` and `idx+1`. `utf8index`: `endIdx > length()` → clamped + error;
`-1` → treated as `length()`, no error. `utf8nextIndex`: `startIdx > length()` → `(unsigned int)-1` +
error; `startIdx == length()` or `-1` → `(unsigned int)-1`, no error.

---

## 7. Tokenising

Three mechanisms with genuinely different semantics. Pick deliberately.

### 7.1 `stoken()` — non-destructive, index-driven

The source SafeString is **unchanged**; you walk it with the returned index. The end of the SafeString is
*always* a delimiter, so the final unterminated token is always returned.

```cpp
int stoken(SafeString& token, unsigned int fromIndex,
           char delimiter | const char* delimiters | SafeString& delimiters,
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

Removes the returned token (and leading delimiters) from the source, which is what lets you keep feeding
a fixed-size buffer from a stream.

```cpp
unsigned char nextToken(SafeString& token,
                        char delimiter | const char* delimiters | SafeString& delimiters,
                        bool returnEmptyFields = false,
                        bool returnLastNonDelimitedToken = true,
                        bool firstToken = false);

unsigned char firstToken(SafeString& token, delimiters, bool returnLastNonDelimitedToken = true);
```

- The **delimiter itself is left in the source**, so you can test which one ended the token — check it
  *before* the next call, which strips leading delimiters.
- `returnLastNonDelimitedToken = true` (the default since V4.0.4) returns a trailing unterminated token.
  Set it to **`false` when feeding from a stream**, so a partial token stays buffered until its delimiter
  arrives.
- `firstToken()` exists so a leading delimiter can yield an empty first field; only use it for the first
  call, because the previous delimiter is still sitting at the front afterwards.
- **Upstream bug, fixed in V4.1.44:** through v4.1.43 the `SafeString` overload was declared
  `firstToken(SafeString& token, SafeString delimiters, …)` — delimiters **by value**. Because the copy
  constructor is private (§10.1), calling it failed to compile with
  `error: 'SafeString::SafeString(const SafeString&)' is private within this context`. The local
  `SafeString/src/SafeString.h` now takes `SafeString& delimiters`. On an **unpatched** upstream copy,
  pass the delimiters as a `const char*` or `char` instead. The `char` and `const char*` overloads were
  always fine, and `nextToken()` takes `SafeString&` correctly in all versions.
- If `token` lacks capacity, the call still returns true with an empty token and the source token is
  still consumed — again, no infinite loops.

### 7.3 `readUntilToken()` — non-blocking, straight from a Stream

```cpp
bool skipToDelimiter = false;
cSF(sfInput, 32);
cSF(sfToken, 32);

if (sfInput.readUntilToken(Serial, sfToken, ",\r\n", skipToDelimiter, false /*echo*/, 0 /*timeout ms*/)) {
  // sfToken holds one complete token, delimiter not included
}
```

- Non-blocking: returns immediately when there is nothing to read.
- `sfInput.capacity()` must be **at least one more than the largest expected token**. Longer runs of
  chars are discarded and produce an empty token at the next delimiter.
- `skipToDelimiter` is a `bool&` holding state between calls — set it true to discard everything up to
  the next delimiter.
- `timeout_ms != 0` returns the buffered text as a token when input goes quiet.
- Prefer `SafeStringReader` (§9), which wraps all of this.

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

Use these to move data between buffers of mismatched size in chunks.

### 8.2 Non-blocking Stream reads

```cpp
unsigned char read(Stream& input);                              // read whatever is available now
unsigned char readUntil(Stream& input, char | const char* | SafeString& delimiters);
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
  }
  // the rest of loop() keeps running at full speed while the user types
}
```

`SafeStringReader` **is a** `SafeString`, so every method above (`==`, `toInt`, `startsWith`, …) works on
the token directly.

| Method | Purpose |
|---|---|
| `connect(Stream&)` | choose the input stream; clears the read count |
| `read()` | non-blocking; `true` when a delimited token is available. Always clears itself first |
| `getDelimiter()` | `int` delimiter that ended the token; `-1` on timeout or error. Valid only right after `read()` returns true |
| `echoOn()` / `echoOff()` | echo received chars back to the stream (default off) |
| `setTimeout(ms)` | return buffered text as a token after `ms` of silence (default 0 = never) |
| `returnEmptyTokens(bool = true)` | emit an empty token per delimiter instead of collapsing runs |
| `flushInput()` | discard buffered input **and** the Stream RX buffer, then skip to the next delimiter |
| `skipToDelimiter()` | discard the token currently being assembled |
| `isSkippingToDelimiter()` | true while discarding |
| `getReadCount()` | chars read since `connect()` — useful for HTTP `Content-Length` |
| `end()` | return any final token, flush the input buffer, disconnect, clear the read count. **Does not** reset echo or the timeout — those two lines are commented out in `SafeStringReader.cpp`. Re-apply `echoOff()` / `setTimeout()` yourself after re-connecting. (The header comment claimed otherwise through v4.1.43; corrected in this copy) |
| `debugInputBuffer(...)` | dump the partially-assembled input buffer |

Flush idiom:

```cpp
sfReader.setTimeout(1000);
sfReader.flushInput();
sfReader.setTimeout(0);
```

Note that one `read()` call transfers at most `size + 1` characters from the stream, so a small reader
drains a port slowly. See **§17** if input is arriving in bursts or faster than that.

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
matrix in **§23**.

| Class | Creation | Purpose |
|---|---|---|
| `SafeStringReader` | `createSafeStringReader(name, size, delims, …)` | non-blocking tokenised Stream input (§9) |
| `SafeStringStream` | `SafeStringStream ss(sfData);` then `ss.begin(baud)` | a `Stream` that replays a SafeString at a simulated baud rate, for repeatable testing — **see §16** |
| `BufferedOutput` | `createBufferedOutput(name, size, mode, allOrNothing)` | non-blocking `print()` — **see §14** |
| `BufferedInput` | `createBufferedInput(name, size)` | extra RX buffering — **see §15** |
| `SerialComs` | `SerialComs coms(sendSize, receiveSize);` | framed, checksummed messages between Arduinos — **see §14** |
| `millisDelay` | `millisDelay d;` | non-blocking delay — **see §13** |
| `loopTimerClass` | `loopTimer` (predefined instance) | max/average `loop()` latency — **see §13** |
| `PinFlasher` | `PinFlasher f(pin);` | non-blocking pin flashing — **see §15** |

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
  Guard clause in the source: if `repeat()` is called while the delay has *not* actually expired
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

Aim for a loop running faster than 500 Hz — a maximum latency under about 1000 µs. A `5sec max` in the
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

## 14. BufferedOutput and SerialComs

### 14.1 BufferedOutput — non-blocking `print()`

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

#### Creation

```cpp
createBufferedOutput(name, size, mode);                  // allOrNothing = true
createBufferedOutput(name, size, mode, allOrNothing);
```

> **`mode` is required — the 2-argument form does not compile.** `BufferedOutput.h` documents
> "takes 2, 3 or 4 arguments" and describes `createBufferedOutput(name, size)` as defaulting to
> `BLOCK_IF_FULL`, but the macro ends in a bare `__VA_ARGS__`, so omitting `mode` expands to
> `BufferedOutput name(size, buf,  );` and fails with
> `error: expected primary-expression before ')' token`. The constructor has no default for `mode`
> either. Always pass a mode. (Verified by compiling all three arities; the header comment is
> corrected in this copy.)

The macro declares a `uint8_t name_OUTPUT_BUFFER[size+4]` — the extra 4 bytes hold the drop mark. Buffer
size is capped at 32766; sizes under 8 (or a `NULL` buffer) fall back to an internal 8-byte buffer.

| Mode | Behaviour when the buffer fills |
|---|---|
| `BLOCK_IF_FULL` | blocks until space frees up. **Not recommended** — it reintroduces exactly the stall you are trying to remove. Useful only in testing, to guarantee every character is seen |
| `DROP_UNTIL_EMPTY` | drop all further output until the buffer has *completely* emptied. Keeps consecutive prints intact so surviving output stays readable. `availableForWrite()` returns 0 for the whole drop period |
| `DROP_IF_FULL` | drop only until there is space again |

Whenever characters are dropped, `~~` (followed by CR NL) is inserted into the output so the gap is
visible.

**`allOrNothing`** (default `true`): if a whole `print(...)` will not fit, none of it is written — you
never see half a line. Set `false` to emit the part that fits. Ignored in `BLOCK_IF_FULL` mode. The
setting reverts to the constructor value after each `write(buf, size)`.

#### Connecting — and the trap

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
is rejected with the same `while(1)` message — 13000000 itself is accepted, despite the message text
saying `< 13000000`.

#### API

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

#### It is a Stream, so it composes

Because `BufferedOutput` is a `Stream`, a `SafeStringReader` can read *through* it — reads go directly to
the underlying serial port while the reader's echo goes out through the buffer:

```cpp
bufferedOut.connect(Serial);
sfReader.connect(bufferedOut);      // echo is buffered, so echoing never blocks
sfReader.echoOn();
```

`loopTimer` can also print through it: `loopTimer.check(bufferedOut);`

**§20** covers this pairing in detail, including echo ordering and when it is worth doing.

#### The full non-blocking stack

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

### 14.2 SerialComs — reliable messages between two boards

`SerialComs` sends and receives whole lines of text between two Arduinos (or an Arduino and a PC) over a
serial link, with a checksum and flow control, so a message either arrives intact or not at all.

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

#### The protocol (from `SerialComs.cpp`)

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
  *Note: through v4.1.43 the comment block in `SerialComs.cpp` and the field declaration in
  `SerialComs.h` both still said 250 ms — stale since v4.1.5. Both are corrected in this copy.*

#### API

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

**§18** covers combining `SerialComs` with a separate `SafeStringReader` for local console input, and why
the link stream must not be shared. **§22** covers `BufferedOutput` — which must never carry the link
itself, but is the right home for the protocol debug output.

#### Rules

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
7. Debug and error output goes to **`SafeString::setOutput(...)`**, deliberately *not* to the stream
   passed to `connect()` — you cannot debug onto the link itself. Comment the `setOutput()` line out once
   the link is working, to silence the protocol chatter.

---

## 15. PinFlasher and BufferedInput

Two smaller classes: `PinFlasher` is built on `millisDelay` (§13.1), and `BufferedInput` is the input-side
counterpart to `BufferedOutput` (§14.1).

### 15.1 PinFlasher — non-blocking pin flashing

Flashes an output pin without blocking, and — the point of the class — lets you drive it from state logic
that runs every loop without having to track whether anything changed.

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

#### The two magic values

| Constant | Value | Meaning |
|---|---|---|
| `PIN_ON` | `-1` | hold the pin **on** (stops flashing) |
| `PIN_OFF` | `0` | hold the pin **off** (stops flashing) |

Any other value is a half-period in ms. "On" and "off" are logical — with `invert = true`, on drives the
pin LOW.

#### API

| Method | Behaviour |
|---|---|
| `PinFlasher(int pin = -1, bool invert = false)` | records the pin; **does not touch the hardware yet** |
| `update()` | advance the flash state. **Call every `loop()`** |
| `setOnOff(unsigned long onOff_ms)` | equal on and off times (50% duty, period = 2×). `PIN_ON` / `PIN_OFF` hold the pin |
| `setOnAndOff(unsigned long on_ms, unsigned long off_ms)` | independent on and off times. `PIN_ON` and `PIN_OFF` are **invalid here and silently ignored** |
| `setPin(int pin)` | change pins: stops flashing, sets the new pin to output and off, returns the previous pin to `INPUT`. Passing the *same* pin is ignored and does not disturb flashing. Any negative value means "no pin" |
| `invertOutput()` | flip the on/off polarity, keeping the current logical state. Returns the new setting (`true` = on is LOW) |
| `~PinFlasher()` | returns the pin to `INPUT` |

#### Why calls are idempotent

`setOnOff()` and `setOnAndOff()` **do nothing if the requested timing already matches the current
setting** — they just call `update()` and return. This is deliberate: you can call them unconditionally
from `loop()` based on program state, and the flashing continues undisturbed rather than restarting its
phase every pass. There is no need to track "did the mode change?" yourself.

#### Two behaviours worth knowing

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

### 15.2 BufferedInput — extra RX buffering

A hardware serial RX buffer is small (64 bytes on an UNO). If `loop()` occasionally takes longer than the
time it takes for that buffer to fill, incoming characters are lost. `BufferedInput` sits in front of the
stream and drains it into a larger buffer of your choosing.

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

The macro declares `uint8_t name_INPUT_BUFFER[size]` (no padding, unlike `createBufferedOutput`). Size is
capped at 32766; sizes under 8 or a `NULL` buffer fall back to an internal 8-byte buffer.

| Method | Purpose |
|---|---|
| `connect(Stream& stream)` | choose the input stream and clear the buffer |
| `nextByteIn()` | move everything currently available from the stream into the buffer. **Call every `loop()`**, and more often in long loops. Most other methods also call it |
| `available()` / `read()` / `peek()` | read from the buffer |
| `write(...)` | passes **straight through** to the underlying stream — output is not buffered |
| `getSize()` | buffer size |
| `maxStreamAvailable()` | high-water mark of the *stream's* `available()` seen so far; **reading resets it to 0** |
| `maxBufferUsed()` | high-water mark of this buffer's fill level; **reading resets it to 0** |

#### Sizing the buffer

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

#### Notes and traps

- **`nextByteIn()` before `connect()` blocks for 5 seconds.** It prints
  `"BufferedInput Error: need to call connect(..) first in setup()"` to `SafeString::Output` and then
  calls `delay(5000)`. In a loop that becomes a 5-second stall per iteration, which reads as a hang.
- **Buffering is not a substitute for a fast loop.** `BufferedInput` absorbs *bursts*; it cannot help if
  the average consumption rate is below the incoming data rate. Use `loopTimer` (§13.2) to find what is
  blocking instead.
- **Often unnecessary.** `SafeStringReader` (§9) already reads non-blockingly every loop. Reach for
  `BufferedInput` only when something in the loop is unavoidably slow and measurement shows characters
  are being lost. **§17 covers pairing the two in detail**, including the reason a fast loop can still
  drop input.
- Through v4.1.43 the usage comment at the top of `BufferedInput.h` was a stale copy of
  `BufferedOutput`'s, referring to `createBufferedOutput` and `nextByteOut()`. It is rewritten in this
  copy; on an unpatched upstream copy ignore it — the correct names are `createBufferedInput` and
  `nextByteIn()`.

---

## 16. SafeStringStream — automated testing of input code

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

### 16.1 Construction and begin()

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

### 16.2 What the two modes actually model

This distinction is the whole point of the class:

- **`begin(0)` — infinite baud.** `available()` returns the full remaining length, so a single pass reads
  everything. Use it to exercise parsing logic quickly.
- **`begin(9600)` — realistic.** Bytes trickle from the data SafeString into the RX buffer at the
  simulated rate, and `available()` reports **only what is in the RX buffer** — 8 bytes by default, not
  the whole string. This is what catches the bugs: parsers that assume a whole line arrives at once,
  tokenisers that mishandle a token split across `loop()` calls, and code that is too slow to keep up.

```cpp
// works only at infinite baud rate
while (sfStream.available()) { Serial.print((char)sfStream.read()); }
```

### 16.3 Reading consumes, writing appends

- **Reading removes characters from the underlying SafeString.** After draining the stream, the data
  SafeString is empty. To re-run a test, re-assign the data.
- **Writing to the stream appends to the underlying SafeString**, so you can inject more test data at
  runtime. If the SafeString is full the write is discarded — it never blocks — and an error is raised.
- `availableForWrite()` reports the underlying SafeString's free space.

### 16.4 RxBufferOverflow() — the assertion worth making

```cpp
size_t dropped = sfStream.RxBufferOverflow();   // count since the last call; reading resets it to 0
```

When the RX buffer is full and another byte is due, the oldest byte is discarded and this counter
increments. That is precisely a hardware RX overrun. A non-zero value means **your code did not read fast
enough at that baud rate and would lose data on a real port** — so check it in tests rather than trusting
that the parse looked right.

### 16.5 The echo feedback loop

If the code under test echoes — `sfReader.echoOn()`, or `readUntilToken(..., echoInput = true)` — the
echoed characters are *written back into the data SafeString*, because writing appends. The test data
then repeats forever.

This is a trap and a technique. The `SafeStringStream_testdata.ino` example relies on it deliberately to
loop its test input indefinitely. If you did not intend it, turn echo off or write the echo to a
different stream.

### 16.6 Swapping test input for live input

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
`sfStr.read(...)` and `sfStr.readUntil(...)`. **§19** covers pairing it with `SafeStringReader` as a test
harness, including RX buffer sizing and the error paths worth exercising.

---

## 17. Using BufferedInput with SafeStringReader

`SafeStringReader` (§9) is already non-blocking, so `BufferedInput` (§15.2) is not needed most of the
time. This section covers the specific case where it *is* needed, and how to tell.

### 17.1 Why a fast loop can still lose characters

`SafeStringReader::read()` does not drain the port. Inside `readUntilTokenInternal()` the read loop is
bounded:

```cpp
while (input.available() && (len < capacity()) && (noCharsRead < capacity())) { … }
```

So **one `read()` call transfers at most `capacity()` characters** from the stream, where `capacity()` is
the reader's internal input buffer — `size + 1` for `createSafeStringReader(name, size, …)`. (A call may
run the loop twice, for at most `2 × capacity()`, when it has just switched to skip-to-delimiter mode.)

Concretely: `createSafeStringReader(sfReader, 5, " ,\r\n")` pulls **at most 6 characters per `read()`
call**. If `read()` is called once per `loop()` and the port delivers more than 6 characters in that
time, the hardware RX buffer — 64 bytes on an UNO — fills and the excess is silently lost. The loop can
be fast and the sketch can still drop input.

This is the situation `BufferedInput` addresses: a small reader on a fast or bursty stream, or a loop
with an occasional long pass.

### 17.2 The wiring

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

### 17.3 What this fixes, and what it does not

| Problem | Does BufferedInput help? |
|---|---|
| Bursts of input larger than the hardware RX buffer | **Yes** — that is exactly what it absorbs |
| An occasional slow pass through `loop()` | **Yes**, if the buffer is sized for the worst pass |
| Input arriving faster than `size + 1` chars per loop, sustained | **No** — your buffer fills instead of the hardware one, just more slowly |
| `loop()` blocked for long periods by `delay()` or blocking prints | **No** — fix the blocking (§13.2, §14.1) |

For a sustained rate mismatch the real fixes are to **increase the reader's `size`** (each `read()` then
transfers more), or to **call `sfReader.read()` more than once per loop**. `BufferedInput` buys headroom,
not throughput.

### 17.4 Sizing the buffer by measurement

`maxStreamAvailable()` and `maxBufferUsed()` (§15.2) exist for exactly this decision. Both are
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
| `maxBufferUsed()` reaches the buffer size | the buffer is full and characters are being dropped | increase the `createBufferedInput` size |
| `maxStreamAvailable()` approaches the hardware RX size (64 on an UNO) while the buffer still has room | the hardware buffer is nearly overflowing before you drain it | call `nextByteIn()` **more often**, not just with a bigger buffer |
| both comfortably below their limits | sized correctly | — |

Note the second row: a bigger `BufferedInput` does nothing if the hardware buffer overflows before
`nextByteIn()` runs. Frequency and size are separate knobs and the two statistics distinguish them.

### 17.5 Echo through BufferedInput

`SafeStringReader` echoes by writing back to the stream it was connected to — so with this wiring the
echo goes to `bufferedIn`. `BufferedInput::write()` passes straight through to the underlying stream
(and conveniently calls `nextByteIn()` on the way), so echo still reaches Serial and still works.

But that pass-through write is a plain `Serial.write()`, so **echo can block** once the TX buffer fills —
reintroducing the stall you were avoiding. Options:

1. `sfReader.echoOff()` — simplest, if echo is not required.
2. Chain both buffers: `bufferedIn.connect(bufferedOut); bufferedOut.connect(Serial);` with
   `sfReader.connect(bufferedIn)`. Reads pass through `BufferedOutput` to Serial (§14.1) and echo writes
   land in the output buffer, so both directions are buffered. This requires **both**
   `bufferedIn.nextByteIn()` and `bufferedOut.nextByteOut()` every loop. The pass-through behaviour of
   both classes makes this work, though the library ships no example of the chain — verify it on your
   hardware.
3. Connect the reader to `bufferedOut` instead (§14.1). Echo is buffered, but input is not — the right
   choice when echo is the bottleneck rather than input.

### 17.6 Complete example

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
  }

  if (reportDelay.justFinished()) { // is the buffering actually adequate?
    reportDelay.repeat();
    Serial.print(F("maxStreamAvailable: ")); Serial.println(bufferedIn.maxStreamAvailable());
    Serial.print(F("maxBufferUsed: "));      Serial.println(bufferedIn.maxBufferUsed());
  }
}
```

Use `SafeStringStream` (§16) at a realistic baud rate to reproduce the fast-input condition on the bench,
and check `RxBufferOverflow()` to confirm nothing was dropped.

---

## 18. Using SerialComs with SafeStringReader

### 18.1 `textReceived` already *is* a SafeStringReader

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

### 18.2 One reader per stream — never share the link

The internal reader has already called `connect()` on the stream you passed to `coms.connect()`.
Attaching **your own** `SafeStringReader` to that same stream gives two consumers racing for the same
bytes: each steals characters the other needs, `XON` delimiters go missing, checksums fail, and the link
drops.

> **Rule:** the stream passed to `coms.connect()` belongs to `SerialComs` alone. A separate
> `SafeStringReader` must read from a *different* stream.

### 18.3 The normal layout: link on one port, console on another

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

  SafeString::setOutput(Serial);   // SafeString errors + SerialComs debug
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
      coms.textToSend = sfCmd;                  // SafeString assignment, length-checked
      if (coms.textToSend.hasError()) {
        bufferedOut.println(F("command too long to send"));
      }
    } else {
      bufferedOut.println(F("link busy or down — command dropped"));
    }
  }
}
```

Note `coms.textToSend = sfCmd;` — a plain SafeString assignment between two SafeStrings, bounds-checked
like any other (§6.2).

### 18.4 Parsing a received message

`textReceived` is a SafeString, so tokenise it with the §7 methods. The checksum has **already been
stripped** — `checkCheckSum()` calls `msg.removeLast(2)` before verifying — so you see only your own
message text.

```cpp
if (!coms.textReceived.isEmpty()) {
  cSF(sfToken, 60);                                   // capacity >= textReceived's
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

### 18.5 Matching capacities

Three sizes have to line up, and mismatches fail quietly at the boundary rather than loudly:

| Relationship | Consequence if wrong |
|---|---|
| local reader `size` ≤ `sendSize` | a console command longer than `sendSize` fails the assignment to `textToSend` — all-or-nothing, so **nothing** is queued. Check `hasError()` |
| this side's `sendSize` == other side's `receiveSize` | a message longer than the far side's `receiveSize` overflows its reader and is discarded there |
| token SafeString capacity ≥ `textReceived` capacity | `nextToken()` returns true with an empty token (§7.2) |

### 18.6 Gotchas

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
  transmitted**. Guard with `coms.isConnected()` before loading a message (as in §18.3), or be prepared
  to resend — `SerialComs` provides no delivery acknowledgement above the checksum.
- **Handle `textReceived` in the same loop pass.** The next `sendAndReceive()` clears it.
- **Only load `textToSend` when it is empty**, or you append to a message still waiting to go out.
- **Echo and debug share the console.** `SafeString::setOutput(Serial)` sends SerialComs protocol
  chatter to the same port your reader echoes to, so the two interleave. Comment out `setOutput()` once
  the link works, or route the reader's echo through `BufferedOutput` and accept the mixing during
  development.
- **A failed `connect()` is not fatal but is not harmless.** If allocation failed, `getTextToSend()` and
  `getTextReceived()` return the `SerialComs` object itself — a SafeString with a **1-byte static buffer
  and zero capacity**. So `coms.textToSend.print(...)` after a failed `connect()` raises capacity errors
  instead of crashing, and nothing is ever sent. Check the return of `connect()`.

---

## 19. Using SafeStringStream with SafeStringReader

`SafeStringStream` (§16) is a `Stream`, so a `SafeStringReader` connects to it exactly as it would to
`Serial`. This is the library's unit-testing arrangement: the same reader code, driven from canned data
at a controlled rate, with repeatable results.

### 19.1 The wiring

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

### 19.2 Echo must be off, or the test repeats forever

This is the trap that costs the most time. `SafeStringReader` echoes by writing back to the stream it
was connected to; writing to a `SafeStringStream` **appends to the data SafeString** (§16.3). So with
`sfReader.echoOn()`, every character read is written straight back onto the end of the test data and the
input never ends.

```cpp
sfReader.echoOff();                   // canned-data tests
```

Turn it off unless you deliberately want an endless loop of the same input — which is precisely what
`SafeStringStream_testdata.ino` does to keep its demo running. If you need to *see* the input during a
test, print it yourself from the token handler instead of enabling echo.

### 19.3 Size the RX buffer to match the real hardware

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

Combined with the bound from §17.1 — one `read()` call transfers at most `size + 1` characters — this
is how you find out, on the bench, whether a small reader can keep up with a given baud rate.

### 19.4 Testing the paths that are awkward to trigger by hand

This is the real payoff: the error branches inside `readUntilTokenInternal()` are hard to produce
reliably by typing, and trivial to produce from canned data.

| Path to exercise | Test data / setup |
|---|---|
| **Token longer than the reader** — sets the error flag, discards the input and skips to the next delimiter | a word longer than `size`, e.g. `"looooooooooooong stop\n"` with `size` 5 |
| **Empty tokens** between consecutive delimiters | `"start,,stop\n"`, with and without `sfReader.returnEmptyTokens()` |
| **The timeout path** — the last un-delimited token is returned when input goes quiet | `sfReader.setTimeout(100);` and test data **not** ending in a delimiter |
| **Which delimiter terminated a token** | mixed delimiters, checking `sfReader.getDelimiter()` right after `read()` returns true |
| **Slow arrival / split tokens** | a low `begin(baud)` so tokens straddle several `loop()` passes |
| **Data ≥ 0x80** (UTF-8) | any high-byte content — fixed in v4.1.10 |

The timeout case is worth spelling out, because it is deterministic here and not on a live port: once
the canned data is exhausted no further characters ever arrive, so the timeout fires exactly once and
returns whatever was buffered. That makes "does my code handle an unterminated final token?" a
repeatable test.

### 19.5 Running several cases in one sketch

Reading **consumes** the data SafeString, so each case needs fresh data. `begin(sf, baud)` replaces it:

```cpp
sfTestData = F("next test case\n");
sfStream.begin(sfTestData, 1200);
```

To also clear any partial token left in the reader between cases, reset it:

```cpp
sfReader.end();                 // return any final token, flush the input buffer, disconnect
sfReader.connect(sfStream);     // re-attach, clearing the read count
sfReader.echoOff();             // end() does NOT reset echo or timeout — see §9
sfReader.setTimeout(100);
```

There is no assertion framework, so a harness is a table of cases plus printed results you compare
against expected output that you capture yourself on a known-good run. (The `.txt` files alongside the
example sketches are **not** that — they are the one-line Arduino IDE example descriptions.)

### 19.6 A minimal harness

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
  sfReader.connect(sfStream);
  sfReader.echoOff();
  sfReader.setTimeout(100);        // so the last un-delimited token is returned
}

void setup() {
  Serial.begin(115200);
  SafeString::setOutput(Serial);   // show the library's own error messages
  runCase(cases[caseIdx]);
}

void loop() {
  if (sfReader.read()) {
    Serial.print(F("token: '")); Serial.print(sfReader.c_str());
    Serial.print(F("' delim: ")); Serial.println(sfReader.getDelimiter());
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

Leave `SafeString::setOutput(Serial)` on in tests — the `"!! Error: … input length exceeds capacity"`
message *is* the expected result for the over-long case, not noise.

### 19.7 Switching back to the live port

Keep the swap to a single `#define` so the code under test is provably identical in both modes (§16.6):

```cpp
#ifdef TEST_DATA
  sfReader.connect(sfStream);
#else
  sfReader.connect(Serial);
#endif
```

---

## 20. Using BufferedOutput with SafeStringReader

`BufferedOutput` (§14.1) is a `Stream`, and its `read()`, `available()` and `peek()` pass **straight
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

### 20.1 Why: echo is output, and output blocks

`echoOn()` is the reason to do this. Echoing doubles your outbound traffic at precisely the moment input
is arriving, and the reader echoes with a plain `input.print((char)c)`. Against a raw `Serial` that call
blocks as soon as the TX buffer fills — so turning echo on can reintroduce exactly the stall
`SafeStringReader` exists to avoid. Routed through `BufferedOutput`, the echo lands in the ring buffer and
returns immediately.

If you are not using `echoOn()`, connecting the reader to `bufferedOut` buys you nothing over
`connect(Serial)` except the extra release points in §20.3.

### 20.2 It also fixes echo ordering

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

### 20.3 Free release points

`BufferedOutput::read()`, `available()` and `peek()` each call `nextByteOut()` before delegating. Since
the reader polls the stream every `loop()`, buffered output gets released on every poll as well as at
your explicit `nextByteOut()` call. In a loop with a slow section this measurably improves output
smoothness at no cost.

Re-entrancy is not a concern: since v4.1.42 `nextByteOut()` holds a per-instance `inNextByteOut` flag and
returns immediately if called recursively.

You still need `bufferedOut.nextByteOut()` at the top of `loop()` — the reader's polling is a supplement,
not a replacement, and it stops happening if you ever guard `sfReader.read()` behind a condition.

### 20.4 Echo can be dropped

Echo is ordinary buffered output, so it obeys the buffer's mode (§14.1). In `DROP_UNTIL_EMPTY` or
`DROP_IF_FULL`, a burst of input can fill the buffer and the echo of subsequent characters is discarded,
marked with `~~` in the console. The input itself is unaffected — only the echo of it is lost — but a
user watching their typing vanish will report it as dropped input.

If echo fidelity matters, size the buffer for the worst-case burst (echo volume equals input volume) or
accept the gaps. `BLOCK_IF_FULL` guarantees complete echo but reinstates the blocking, defeating the
purpose.

### 20.5 Input is *not* buffered by this arrangement

The reader still reads directly from the port, and still transfers at most `size + 1` characters per
`read()` call (§17.1). `BufferedOutput` does nothing for input.

Choosing between the three arrangements:

| Wiring | Input | Echo / output |
|---|---|---|
| `sfReader.connect(Serial)` | unbuffered | unbuffered — echo can block |
| `sfReader.connect(bufferedOut)` | unbuffered | **buffered**, correctly ordered |
| `sfReader.connect(bufferedIn)` (§17) | **buffered** | passes through to Serial — echo can block |
| `bufferedIn` → `bufferedOut` → `Serial` (§17.5) | **buffered** | **buffered** — needs both `nextByteIn()` and `nextByteOut()` |

Pick by which side is actually hurting: `loopTimer` (§13.2) tells you whether the loop is blocking, and
`BufferedInput`'s statistics (§17.4) tell you whether input is being lost.

### 20.6 Complete example

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
  }
}
```

Note `SafeString::setOutput(Serial)` rather than `setOutput(bufferedOut)`: error messages are diagnostics
you want unconditionally, and routing them through a dropping buffer risks losing the one message that
explains a failure. The cost is that error text can appear out of order relative to buffered output —
an acceptable trade for diagnostics.

---

## 21. Using SafeStringStream with BufferedOutput

There are two quite different arrangements here. The first is the common one; the second is a test
technique with real caveats.

### 21.1 Coexisting — the usual case

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
  sfReader.echoOff();               // required — see §19.2
}

void loop() {
  bufferedOut.nextByteOut();
  if (sfReader.read()) { bufferedOut.println(sfReader.c_str()); }
}
```

This is what the library's own example sketches do. If that is what you need, stop here.

### 21.2 Capturing output into a SafeString

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
  captureStream.begin(9600);                // begin() first — see §21.3
  bufferedOut.connect(captureStream, 9600); // pass a baud rate — see §21.3
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

> This capture arrangement is derived from the classes' documented behaviour — `BufferedOutput` writes to
> any `Stream`, and `SafeStringStream::write()` appends to its SafeString — but the library ships no
> example of it. Verify it behaves as you expect before relying on it.

### 21.3 Traps

- **Pass an explicit baud rate to `connect()`.** `bufferedOut.connect(stream)` with no baud rate falls
  back to the stream's `availableForWrite()`, which for a `SafeStringStream` is the free space in its
  SafeString. If that is ≤ 2 at connect time — a small or already-full capture buffer — `BufferedOutput`
  enters its infinite `while(1)` "availableForWrite() returns 0" loop (§14.1) and the sketch appears
  hung. `connect(captureStream, 9600)` skips the `availableForWrite()` path entirely and uses its timer
  instead.
- **`begin()` before `connect()`.** `SafeStringStream::write()` reaches `releaseNextByte()`, which before
  `begin()` prints an error and then `delay(5000)` (§16.1). Call `captureStream.begin(baud)` first.
- **The capture SafeString fills.** Once full, further writes are rejected with capacity errors (all-or-
  nothing, §1) and the output is lost. Size it for the batch, drain it periodically with `clear()`, or
  watch `hasError()`.
- **Never use one `SafeStringStream` for both input and capture.** Writing appends to the very SafeString
  being read, which is the echo feedback loop of §16.5 — the captured output becomes new input and the
  test never terminates. Use two separate `SafeStringStream` objects over two separate SafeStrings.
- **`'\0'` is dropped.** `SafeString::write(0)` refuses the byte and raises an error, so binary output
  cannot be captured this way — text only.
- **Nothing is captured until `nextByteOut()` runs.** The buffer releases on that call (and on
  `read`/`available`/`peek`, §20.3), so a capture check placed before it in `loop()` reads stale content.

### 21.4 Which stream to test against

| Goal | Arrangement |
|---|---|
| test input parsing | `SafeStringStream` → `SafeStringReader` (§19) |
| test what your code prints | print into a SafeString directly — it is a `Print` |
| test the buffer's drop/throttle behaviour | `BufferedOutput` → `SafeStringStream` → SafeString (§21.2) |
| see output while testing input | `BufferedOutput` → `Serial`, alongside the input stream (§21.1) |

---

## 22. Using SerialComs with BufferedOutput

`BufferedOutput` is a `Stream`, so `coms.connect(bufferedOut)` compiles and appears to work. **Do not do
it.** This section explains why, and where `BufferedOutput` does belong in a `SerialComs` sketch.

### 22.1 Never put the link through a dropping BufferedOutput

`SerialComs` runs a framed protocol: `<messageText><2 hex checksum><XON>` (§14.2). Every byte is
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

### 22.2 `BLOCK_IF_FULL` is not a fix either

`BLOCK_IF_FULL` genuinely never writes drop marks — the source comments the branch `// may block but no
drop marks here` — so it does not corrupt the protocol. But it spins in `delay(1)` until space frees up,
which stalls `loop()`, and `sendAndReceive()` must be called every loop for the protocol to work. Adding
latency and stalls to a link with a 5-second connection timeout and strict turn-taking trades a
corruption bug for a timing one.

> **Rule:** give `SerialComs` the raw stream. `coms.connect(Serial1)`, never `coms.connect(bufferedOut)`.

### 22.3 Where BufferedOutput does belong: the console

The genuine pairing is `BufferedOutput` on the *other* port — and specifically for `SerialComs`' own
debug output, which is the thing most likely to break the link.

`SerialComs.cpp` defines `#define DEBUG SafeString::Output` and prints on **every message in both
directions**:

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

### 22.4 Console buffer size and mode

- Use a **drop** mode. `BLOCK_IF_FULL` on the console reintroduces the stall and can time out the link.
- Size it for a whole message plus your prefix: with the default `receiveSize` of 60, an 80–100 byte
  buffer holds one forwarded message and its label without dropping.
- `DROP_UNTIL_EMPTY` keeps surviving lines readable; `DROP_IF_FULL` keeps more total text but fragments
  it. For message logs, prefer `DROP_UNTIL_EMPTY`.

### 22.5 Complete arrangement

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

Once the link is working, comment out `SafeString::setOutput(...)` to silence the protocol chatter
entirely (§14.2) — the quietest option is always the safest for link timing. Keep `loopTimer`
(§13.2) in during commissioning to confirm nothing in the loop is long enough to threaten the
turn-taking.

---

## 23. Pairings: quick verdicts

Every combination of the library's nine classes, with a verdict. Use this to answer "can I combine X and
Y?" before writing code.

**"No interaction"** means exactly that: the two classes never touch. Using both is just calling two
independent things from `loop()`, and each one's own rules apply unchanged — there is no combined
behaviour to learn and nothing extra to configure.

| Pairing | Verdict |
|---|---|
| SafeString × SafeStringReader | `SafeStringReader` **is a** `SafeString` — use the token directly (§9) |
| SafeString × SafeStringStream | `SafeStringStream` adapts a SafeString into a `Stream`: reading replays it, writing appends (§16) |
| SafeString × BufferedOutput | Print a SafeString to it (SafeString is `Printable`). A SafeString **cannot** be a `connect()` target — it is a `Print`, not a `Stream` (§21.2) |
| SafeString × BufferedInput | `sfStr.read(bufferedIn)` and `readUntil()` treat it as any other Stream (§8.2) |
| SafeString × SerialComs | `textToSend` / `textReceived` **are** SafeStrings; the whole §6 API applies (§14.2, §18) |
| SafeString × millisDelay | No interaction |
| SafeString × loopTimer | No interaction |
| SafeString × PinFlasher | No interaction |
| SafeStringReader × SafeStringStream | Canned-input test harness. **Echo must be off** or the test never ends (§19) |
| SafeStringReader × BufferedOutput | Buffers the reader's *echo* and fixes echo/output ordering (§20) |
| SafeStringReader × BufferedInput | Buffers *input*; needed because one `read()` takes at most `size + 1` chars (§17) |
| SafeStringReader × SerialComs | **Separate streams only** — the link belongs to SerialComs' internal reader (§18) |
| SafeStringReader × millisDelay | `setTimeout()` for token timeout, `millisDelay` for inactivity (§13.3) |
| SafeStringReader × loopTimer | Use it to confirm the reader keeps up; no direct interaction (§17.3, §20.5) |
| SafeStringReader × PinFlasher | No interaction |
| SafeStringStream × BufferedOutput | Captures output into a SafeString. Pass an explicit baud rate to `connect()` (§21) |
| SafeStringStream × BufferedInput | **Useful for testing** — reproduce the burst that overflows the buffer (§23.1) |
| SafeStringStream × SerialComs | **Don't** — the protocol needs a live responding peer (§23.1) |
| SafeStringStream × millisDelay | No interaction |
| SafeStringStream × loopTimer | No interaction |
| SafeStringStream × PinFlasher | No interaction |
| BufferedOutput × BufferedInput | Chain `bufferedIn` → `bufferedOut` → `Serial` to buffer both directions; both `nextByte…()` calls required (§17.5) |
| BufferedOutput × SerialComs | **Never on the link** — drop marks corrupt the frame. Correct for the console and debug output (§22) |
| BufferedOutput × loopTimer | `loopTimer.check(bufferedOut)` works, but changes what `max - prt` means (§23.1) |
| BufferedOutput × millisDelay | No interaction |
| BufferedOutput × PinFlasher | No interaction |
| BufferedInput × SerialComs | **Safe**, unlike BufferedOutput — it never drops or injects on the paths SerialComs uses (§23.1) |
| BufferedInput × millisDelay | No interaction |
| BufferedInput × loopTimer | No interaction — though `loopTimer` is how you discover you need BufferedInput |
| BufferedInput × PinFlasher | No interaction |
| SerialComs × millisDelay | It already owns one for its 5 s timeout — use `isConnected()`, not your own timer (§23.1) |
| SerialComs × loopTimer | Use it to confirm `sendAndReceive()` runs often enough (§22.5) |
| SerialComs × PinFlasher | No interaction |
| millisDelay × loopTimer | `loopTimer` uses a `millisDelay` internally for its 5 s report period. Nothing to do (§13.2) |
| millisDelay × PinFlasher | `PinFlasher` inherits `millisDelay` **protected** — the timing methods are not public (§15.1) |
| loopTimer × PinFlasher | No interaction |

### 23.1 Notes on the five that need more than a line

**SafeStringStream × BufferedInput — validating buffer size on the bench.**
`BufferedInput`'s sizing statistics (§17.4) only tell you something once real traffic has stressed them.
`SafeStringStream` at a realistic baud rate reproduces that traffic deterministically: feed a burst,
call `nextByteIn()` at your loop's real rate, and read `maxBufferUsed()`. Give the stream its own RX
buffer sized like the hardware UART (§19.3), or the 8-byte default makes the test pessimistic.

**SafeStringStream × SerialComs — not a viable test.**
`SerialComs` is a turn-taking protocol: the controller prompts, the peer answers, each `XON` hands over
the right to send (§14.2). A `SafeStringStream` replays a fixed script — it cannot compute a checksum
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
§22 bans `BufferedOutput` from the link because it drops protocol bytes and injects `~~\r\n` drop marks.
`BufferedInput` does neither: its `write()` passes straight through to the stream untouched, and
`nextByteIn()` only moves as many bytes as currently fit (`if (rb_avail < avail) avail = rb_avail;`), so
it never discards what it has accepted and never inserts anything. Putting it in front of a `SerialComs`
link is therefore sound, and adds receive depth if the loop has an unavoidably slow section. It is rarely
needed — messages are bounded by `receiveSize` (60 by default) against a typical 64-byte UART buffer —
so fix a slow loop first. The library ships no example of this arrangement; verify it on your hardware.

**SerialComs × millisDelay — do not add a reconnect timer.**
`SerialComs` already contains a `millisDelay` driving its 5-second connection timeout, and re-prompts the
peer itself when the link goes quiet (§14.2). Poll `coms.isConnected()` for link state rather than timing
it yourself. Use your own `millisDelay` for application concerns the protocol knows nothing about — how
often to *generate* a message, or how long to wait before alarming on a persistently dead link — and load
`textToSend` only when connected, or the message can be discarded unsent (§18.6).

---

## 24. Rules checklist for generated code

1. Call `SafeString::setOutput(Serial);` early in `setup()` during development.
2. Create every SafeString with `cSF` / `cSFA` / `cSFP` / `cSFPS`. Never call the constructor directly.
3. Prefer `cSFA` over `cSFP`; prefer `cSFPS` over `cSFP` whenever the buffer size is known.
4. Size the buffer for the **worst case** input, then check `hasError()` — do not assume it fits.
5. Declare SafeString function parameters as `SafeString&`. Never return a SafeString.
6. Use `+=` / `-=` / `concat()` / `prefix()`. There is no `+`. Parenthesise to cascade operators.
7. Give `token`/`result` SafeStrings a capacity **≥** the source SafeString's capacity.
8. Check the boolean return of every `toInt` / `toLong` / `toFloat` / `toDouble` before using the value.
9. Remember `substring(result, begin, end)` excludes `end`.
10. Treat `c_str()` as read-only, and never let it outlive the underlying buffer.
11. Never store `'\0'` in a SafeString.
12. For stream input use `SafeStringReader`; if you tokenise a stream manually with `nextToken()`, pass
    `returnLastNonDelimitedToken = false`.
13. Do not `free()` a buffer that a SafeString still wraps, and do not return references to SafeStrings
    that wrap stack buffers.
14. Check `SafeString::errorDetected()` in `setup()` to catch construction errors in globals.
15. Do not mix `strcpy`/`strcat`/`sprintf` with SafeString on the same buffer — wrap it once and stay
    inside the SafeString API.

---

## 25. Worked example — parse and validate a delimited command line

```cpp
#include <SafeString.h>

//  Parses commands of the form:  SET <name> <value>\n
//  Uses only fixed buffers; no heap allocation; every field length-checked.
//
//  sfLine  - accumulates one line of input (destructively tokenised)
//  sfToken - receives each field; sized equal to sfLine so any field can fit
createSafeString(sfLine, 60);
createSafeString(sfToken, 60);
createSafeString(sfName, 16);

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

  // field 2: the name, length-checked against sfName's capacity
  if (!sfLine.nextToken(sfToken, " ")) { Serial.println(F("Missing name")); return; }
  sfName = sfToken;
  if (sfName.hasError()) {                                // name longer than 16 chars
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

---

## 26. Version notes worth knowing

- **v4.1.42** — added `utf8index()` / `utf8nextIndex()`; `BufferedOutput` gained a per-instance lock
  preventing recursive `nextByteOut()` calls.
- **v4.1.41** — `millisDelay` (V1.1.0): fixed `repeat()` after `stop()` / `finish()`. On earlier versions
  `repeat()` following an early termination could compute a wrapped start time.
- **v4.1.40** — `PinFlasher` gained independent on and off times (`setOnAndOff()`).
- **v4.1.39 / .37 / .36** — `int64_t` support: `+=` / `-=`, `print(int64_t)`, `toInt64_t()`.
- **v4.1.34** — `setLength()` was removed; it no longer exists. Use `removeFrom()` / `removeLast()` / `keepLast()`.
- **v4.1.29** — `SafeString.h` no longer defines the `F()` macro or `__FlashStringHelper`; the board core
  must supply them.
- **v4.1.19** — `PinFlasher::setOutput()` made overridable to support a WS2812 flasher subclass.
- **v4.1.12** — `PinFlasher` added.
- **v4.1.10** — `SafeStringStream` fixed to handle data ≥ 0x80 (bytes are now read as `unsigned char`).
- **v4.1.5** — `SerialComs` connection timeout set to 5 seconds (the source comments were not updated
  until the local fix in this copy).
- **v4.1.2** — fixed `SerialComs` when a message times out without a delimiter.
- **v4.1.0** — fixed-width formatting `print(value, decs, width)`; `SerialComs` added.
- **v4.0.4** — `nextToken()` returns the last unterminated token **by default**; `returnEmptyTokens`
  option added.
- **v4.0.0** — `indexOf()` and friends return `int` and `-1` when not found (previously unsigned).
- **v3.1.0** — `hasError()` added.
- **V2** — `substring()` `endIdx` became **exclusive** (it was inclusive in V1).
