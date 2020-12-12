// Serial_GPS_buffered_continuous.ino
//
// This example reads GPS data from a SafeStringStream continuously
//
// Only the $GPRMC message is parsed.
//
// download and install the SafeString library from
// www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
#include "SafeString.h"
#include "SafeStringStream.h"
#include "BufferedOutput.h"

const int TESTING_BAUD_RATE = 9600; // how fast to release the data from sfStream to be read by this sketch
SafeStringStream sfStream;
cSF(sfTestData, 120);

char delimiters[] = "\n"; // space dot comma CR NL are cmd delimiters

const size_t maxMsgLength = 80; // length of largest command to be recognized, can handle longer input but will not tokenize it.
createSafeString(input, maxMsgLength + 1); //  to read input cmd, large enough to hold longest cmd + leading and trailing delimiters
createSafeString(token, maxMsgLength + 1); // for parsing, capacity should be >= input
bool skipToDelimiter = false; // bool variable to hold the skipToDelimiter state across calls to readUntilToken()
// set skipToDelimiter = true to skip initial data upto first delimiter.
// skipToDelimiter = true can be set at any time to skip to next delimiter.

createBufferedOutput(output, 100, DROP_UNTIL_EMPTY); // create an extra output buffer

// UTC date/time
int year;
int month;
int day;
int hour;
int minute;
float seconds;
int latDegs = 0; float latMins = 0.0;
int longDegs = 0; float longMins = 0.0;
float speed = 0.0;
float angle = 0.0;


void setup() {
  Serial.begin(9600);    // Open serial communications and wait a few seconds
  for (int i = 10; i > 0; i--) {
    Serial.print(' '); Serial.print(i);
    delay(500);
  }
  Serial.println();
  Serial.print("Automated Serial testing at "); Serial.println(TESTING_BAUD_RATE);

  output.connect(Serial); // connect the output buffer to the Serial, this flushes Serial
  SafeString::setOutput(output); // enable error messages and debug() output to be sent to output

  sfTestData = F(
                 "$GPRMC,194509.000,A,4042.6142,N,07400.4168,W,2.03,221.11,160412,,,A*77\n"
                 "$GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48\n"
               ); // initialized the test data
  Serial.println("Test Data:-");
  Serial.println(sfTestData);
  Serial.println();
  sfStream.begin(sfTestData, TESTING_BAUD_RATE); // NOTE: do this last !!! or will miss first few chars of sfTestData
}

bool checkSum(SafeString &msg) {
  size_t idxStar = msg.indexOf('*');
  cSF(sfCheckSumHex, 2);
  msg.substring(sfCheckSumHex, idxStar + 1, idxStar + 3); // next 2 chars
  long sum = 0;
  if (!sfCheckSumHex.hexToLong(sum)) {
    return false; // not a valid hex number
  }
  for (size_t i = 1; i < idxStar; i++) { // skip the $ and the *checksum
    sum ^= msg[i];
  }
  return (sum == 0);
}

void parseTime(SafeString &timeField) {
  float timef = 0.0;
  if (!timeField.toFloat(timef)) { // an empty field is not a valid float
    return; // not valid skip the rest
  }
  uint32_t time = timef;
  hour = time / 10000;
  minute = (time % 10000) / 100;
  seconds = fmod(timef, 100.0);
}

void parseDegMin(SafeString &degField, int &d, float& m, int degDigits) {
  cSF(sfSub, 10); // temp substring
  degField.substring(sfSub, 0, degDigits); // degs, 2 for lat, 3 for long
  int degs = 0;
  if (!sfSub.toInt(degs)) {
    return; // invalid
  }
  float mins = 0.0;
  degField.substring(sfSub, degDigits, degField.length()); // mins
  if (!sfSub.toFloat(mins)) {
    return; // invalid
  }
  // both deg/mins valid update returns
  d = degs;
  m = mins;
}

void parseDate(SafeString &dateField) {
  long lDate = 0;
  if (!dateField.toLong(lDate)) {
    return; // invalid
  }
  day = lDate / 10000;
  month = (lDate % 10000) / 100;
  year = (lDate % 100);
}
/**
   Fields: (note fields can be empty)
    123519.723   Fix taken at 12:35:19,723 UTC
    A            Status A=active or V=Void.
    4807.038,N   Latitude 48 deg 07.038' N
    01131.000,E  Longitude 11 deg 31.000' E
    022.4        Speed over the ground in knots
    084.4        Track angle in degrees True
    230394       Date 23rd of March 1994
    003.1,W      Magnetic Variation
*/
// just leaves existing values unchanged if new ones are not valid invalid
// returns true if new Active msg
bool parseGPRMC(SafeString &msg) {
  //msg.debug();

  cSF(sfField, 11); // temp SafeString to received fields, max field len is <11;
  char delims[] = ",*"; // fields delimited by , or *
  bool returnEmptyFields = true; // return empty field for ,,
  size_t idx = 0;
  idx = msg.stoken(sfField, idx, delims, returnEmptyFields);
  if (sfField != "$GPRMC") {  // first field should be $GPRMC else called with wrong msg
    return false;
  }

  cSF(sfTimeField, 11); // temp SafeString to hold time for later passing, after checking 'A'
  idx = msg.stoken(sfTimeField, idx, delims, returnEmptyFields); // time, keep for later

  idx = msg.stoken(sfField, idx, delims, returnEmptyFields); // A / V
  if (sfField != 'A') {
    return false; // not active
  }
  // else A so update time
  parseTime(sfTimeField);

  idx = msg.stoken(sfField, idx, delims, returnEmptyFields); // Lat
  parseDegMin(sfField, latDegs, latMins, 2);

  idx = msg.stoken(sfField, idx, delims, returnEmptyFields); // N / S or empty
  if (sfField == 'S') {
    latDegs = -latDegs;
  }
  idx = msg.stoken(sfField, idx, delims, returnEmptyFields); // Long
  parseDegMin(sfField, longDegs, longMins, 3);

  idx = msg.stoken(sfField, idx, delims, returnEmptyFields); // N / S or empty
  if (sfField == 'W') {
    longDegs = -longDegs;
  }
  idx = msg.stoken(sfField, idx, delims, returnEmptyFields); // speed
  if (!sfField.toFloat(speed) ) {
    // invalid, speed not changed
  }
  idx = msg.stoken(sfField, idx, delims, returnEmptyFields); // track angle true
  if (!sfField.toFloat(angle) ) {
    // invalid, angle not changed
  }
  idx = msg.stoken(sfField, idx, delims, returnEmptyFields); // date
  parseDate(sfField);

  idx = msg.stoken(sfField, idx, delims, returnEmptyFields); // magnetic variation
  // skip parsing this for now
  return true;
}

void printPosition() {
  cSF(results, 50);
  results.concat(F("20")).concat(year).concat('/').concat(month).concat('/').concat(day).concat(F("  "))
  .concat(hour).concat(':').concat(minute).concat(':').concat(seconds).concat(F("  "))
  .concat(longDegs).concat(' ').concat(longMins).concat(F("', ")).concat(latDegs).concat(' ').concat(latMins).concat(F("'")).newline();
  output.clearSpace(results.length()); // only clears space in the extra buffer not the Serial tx buffer
  output.print(results);
}

void loop() {
  output.nextByteOut();
  if (input.readUntilToken(sfStream, token, delimiters, skipToDelimiter, true)) { // echo on write back chars read to end of sfStream
    token.debug("readUntilToken() => ");
    token.trim(); // remove and leading/trailing white space
    if (token.startsWith("$GPRMC,")) {  // this is the one we want
      if (checkSum(token)) { // if the check sum is OK
        if (parseGPRMC(token)) {
          printPosition(); // print new data
        }
      } else {
        output.print("bad checksum : "); output.println(token);
      }
    } // else ignore
  } // else token is empty
}
