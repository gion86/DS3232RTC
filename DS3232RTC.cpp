// Arduino DS3232RTC Library
// https://github.com/JChristensen/DS3232RTC
// Copyright (C) 2018 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html
//
// Arduino library for the Maxim Integrated DS3232 and DS3231
// Real-Time Clocks.
// Requires PJRC's improved version of the Arduino Time Library,
// https://playground.arduino.cc/Code/Time
// https://github.com/PaulStoffregen/Time
//
// For AVR architecture, a DS3232RTC object named RTC is instantiated
// by the library and I2C initialization occurs in the constructor;
// this is for backwards compatibility.
// For other architectures, the user needs to instantiate a DS3232RTC
// object and optionally initialize the I2C bus by calling
// DS3232RTC::begin(). The constructor has an optional bool parameter
// to indicate whether I2C initialization should occur in the
// constructor; this parameter defaults to true if not given.

/*----------------------------------------------------------------------*
 * Arduino DS3232RTC Library v1.1                                       *
 * Gionata Boccalini                                                    *
 *                                                                      *
 * - Changed include to use Wire, master/slave I2C library for          *
 *   Atmel micros, with or without I2C hardware module.                 *
 * - Added checkCon() fuction to check the I2C connection.              *
 *----------------------------------------------------------------------*/

#include <DS3232RTC.h>

// DS3232 I2C Address
#define RTC_ADDR 0x68

// DS3232 Register Addresses
#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x01
#define RTC_HOURS 0x02
#define RTC_DAY 0x03
#define RTC_DATE 0x04
#define RTC_MONTH 0x05
#define RTC_YEAR 0x06
#define ALM1_SECONDS 0x07
#define ALM1_MINUTES 0x08
#define ALM1_HOURS 0x09
#define ALM1_DAYDATE 0x0A
#define ALM2_MINUTES 0x0B
#define ALM2_HOURS 0x0C
#define ALM2_DAYDATE 0x0D
#define RTC_CONTROL 0x0E
#define RTC_STATUS 0x0F
#define RTC_AGING 0x10
#define RTC_TEMP_MSB 0x11
#define RTC_TEMP_LSB 0x12
#define SRAM_START_ADDR 0x14    // first SRAM address
#define SRAM_SIZE 236           // number of bytes of SRAM

// Alarm mask bits
#define A1M1 7
#define A1M2 7
#define A1M3 7
#define A1M4 7
#define A2M2 7
#define A2M3 7
#define A2M4 7

// Control register bits
#define EOSC 7
#define BBSQW 6
#define CONV 5
#define RS2 4
#define RS1 3
#define INTCN 2
#define A2IE 1
#define A1IE 0

// Status register bits
#define OSF 7
#define BB32KHZ 6
#define CRATE1 5
#define CRATE0 4
#define EN32KHZ 3
#define BSY 2
#define A2F 1
#define A1F 0

// Other
#define DS1307_CH 7                // for DS1307 compatibility, Clock Halt bit in Seconds register
#define HR1224 6                   // Hours register 12 or 24 hour mode (24 hour mode==0)
#define CENTURY 7                  // Century bit in Month register
#define DYDT 6                     // Day/Date flag bit in alarm Day/Date registers

byte DS3232RTC::errCode;           // for debug

// Constructor.
#ifdef TWDR
DS3232RTC::DS3232RTC(TwoWire &bus) : busI2C(bus)
{
}
#else
DS3232RTC::DS3232RTC(USIWire &bus) : busI2C(bus)
{
}
#endif

// Verifies the I2C connection by calling beginTransmission() and endTransmission().
// Returns the I2C status (zero if successful).
byte DS3232RTC::checkCon() {
  busI2C.beginTransmission(RTC_ADDR);
  return busI2C.endTransmission();
}

// Read the current time from the RTC and return it as a time_t
// value. Returns a zero value if an I2C error occurred (e.g. RTC
// not present).
time_t DS3232RTC::get()
{
    struct tm tm;
    
    if ( read(&tm) ) return 0;
    return( mk_gmtime(&tm) );
}

// Set the RTC to the given time_t value and clear the
// oscillator stop flag (OSF) in the Control/Status register.
// Returns the I2C status (zero if successful).
byte DS3232RTC::set(time_t t)
{
    struct tm tm;

    gmtime_r(&t, &tm);
    return ( write(&tm) );
}


// Reads the current time from the RTC and return it in a tm
// structure. Returns the I2C status (zero if successful).
byte DS3232RTC::read(struct tm *tm)
{
    busI2C.beginTransmission(RTC_ADDR);
    busI2C.write((uint8_t)RTC_SECONDS);
    if ( byte e = busI2C.endTransmission() ) { errCode = e; return e; }
    //request 7 bytes (secs, min, hr, dow, date, mth, yr)
    busI2C.requestFrom(RTC_ADDR, 7);
    tm->tm_sec = bcd2dec(busI2C.read() & ~_BV(DS1307_CH));
    tm->tm_min = bcd2dec(busI2C.read());
    tm->tm_hour = bcd2dec(busI2C.read() & ~_BV(HR1224));    // assumes 24hr clock
    tm->tm_wday = busI2C.read();
    tm->tm_mday = bcd2dec(busI2C.read());
    tm->tm_mon = bcd2dec(busI2C.read() & ~_BV(CENTURY));  // don't use the Century bit
    tm->tm_year = bcd2dec(busI2C.read());                 // TODO tmYearToY2k 
    tm->tm_isdst = 0; // TODO  ???
    return 0;
}

// Set the RTC time from a tm structure and clear the
// oscillator stop flag (OSF) in the Control/Status register.
// Returns the I2C status (zero if successful).
byte DS3232RTC::write(struct tm *tm)       // TODO reference
{
    busI2C.beginTransmission(RTC_ADDR);
    busI2C.write((uint8_t)RTC_SECONDS);
    busI2C.write(dec2bcd(tm->tm_sec));
    busI2C.write(dec2bcd(tm->tm_min));
    busI2C.write(dec2bcd(tm->tm_hour));         //sets 24 hour format (Bit 6 == 0)
    busI2C.write(tm->tm_wday);
    busI2C.write(dec2bcd(tm->tm_mday));
    busI2C.write(dec2bcd(tm->tm_mon));
    busI2C.write(dec2bcd(tm->tm_year));     // TODO tmYearToY2k 
    byte ret = busI2C.endTransmission();
    uint8_t s = readRTC(RTC_STATUS);        //read the status register
    writeRTC( RTC_STATUS, s & ~_BV(OSF) );  //clear the Oscillator Stop Flag
    return ret;
}

// Write multiple bytes to RTC RAM.
// Valid address range is 0x00 - 0xFF, no checking.
// Number of bytes (nBytes) must be between 1 and 31 (Wire library
// limitation).
// Returns the I2C status (zero if successful).
byte DS3232RTC::writeRTC(byte addr, byte *values, byte nBytes)
{
    busI2C.beginTransmission(RTC_ADDR);
    busI2C.write(addr);
    for (byte i=0; i<nBytes; i++) busI2C.write(values[i]);
    return busI2C.endTransmission();
}

// Write a single byte to RTC RAM.
// Valid address range is 0x00 - 0xFF, no checking.
// Returns the I2C status (zero if successful).
byte DS3232RTC::writeRTC(byte addr, byte value)
{
    return ( writeRTC(addr, &value, 1) );
}

// Read multiple bytes from RTC RAM.
// Valid address range is 0x00 - 0xFF, no checking.
// Number of bytes (nBytes) must be between 1 and 32 (Wire library
// limitation).
// Returns the I2C status (zero if successful).
byte DS3232RTC::readRTC(byte addr, byte *values, byte nBytes)
{
    busI2C.beginTransmission(RTC_ADDR);
    busI2C.write(addr);
    if ( byte e = busI2C.endTransmission() ) return e;
    busI2C.requestFrom( (uint8_t)RTC_ADDR, nBytes );
    for (byte i=0; i<nBytes; i++) values[i] = busI2C.read();
    return 0;
}

// Read a single byte from RTC RAM.
// Valid address range is 0x00 - 0xFF, no checking.
byte DS3232RTC::readRTC(byte addr)
{
    byte b;

    readRTC(addr, &b, 1);
    return b;
}

// Set an alarm time. Sets the alarm registers only.  To cause the
// INT pin to be asserted on alarm match, use alarmInterrupt().
// This method can set either Alarm 1 or Alarm 2, depending on the
// value of alarmType (use a value from the ALARM_TYPES_t enumeration).
// When setting Alarm 2, the seconds value must be supplied but is
// ignored, recommend using zero. (Alarm 2 has no seconds register.)
void DS3232RTC::setAlarm(ALARM_TYPES_t alarmType, byte seconds, byte minutes, byte hours, byte daydate)
{
    uint8_t addr;

    seconds = dec2bcd(seconds);
    minutes = dec2bcd(minutes);
    hours = dec2bcd(hours);
    daydate = dec2bcd(daydate);
    if (alarmType & 0x01) seconds |= _BV(A1M1);
    if (alarmType & 0x02) minutes |= _BV(A1M2);
    if (alarmType & 0x04) hours |= _BV(A1M3);
    if (alarmType & 0x10) daydate |= _BV(DYDT);
    if (alarmType & 0x08) daydate |= _BV(A1M4);

    if ( !(alarmType & 0x80) )  // alarm 1
    {
        addr = ALM1_SECONDS;
        writeRTC(addr++, seconds);
    }
    else
    {
        addr = ALM2_MINUTES;
    }
    writeRTC(addr++, minutes);
    writeRTC(addr++, hours);
    writeRTC(addr++, daydate);
}

// Set an alarm time. Sets the alarm registers only. To cause the
// INT pin to be asserted on alarm match, use alarmInterrupt().
// This method can set either Alarm 1 or Alarm 2, depending on the
// value of alarmType (use a value from the ALARM_TYPES_t enumeration).
// However, when using this method to set Alarm 1, the seconds value
// is set to zero. (Alarm 2 has no seconds register.)
void DS3232RTC::setAlarm(ALARM_TYPES_t alarmType, byte minutes, byte hours, byte daydate)
{
    setAlarm(alarmType, 0, minutes, hours, daydate);
}

// Enable or disable an alarm "interrupt" which asserts the INT pin
// on the RTC.
void DS3232RTC::alarmInterrupt(byte alarmNumber, bool interruptEnabled)
{
    uint8_t controlReg, mask;

    controlReg = readRTC(RTC_CONTROL);
    mask = _BV(A1IE) << (alarmNumber - 1);
    if (interruptEnabled)
        controlReg |= mask;
    else
        controlReg &= ~mask;
    writeRTC(RTC_CONTROL, controlReg);
}

// Returns true or false depending on whether the given alarm has been
// triggered, and resets the alarm flag bit.
bool DS3232RTC::alarm(byte alarmNumber)
{
    uint8_t statusReg, mask;

    statusReg = readRTC(RTC_STATUS);
    mask = _BV(A1F) << (alarmNumber - 1);
    if (statusReg & mask)
    {
        statusReg &= ~mask;
        writeRTC(RTC_STATUS, statusReg);
        return true;
    }
    else
    {
        return false;
    }
}

// Enable or disable the square wave output.
// Use a value from the SQWAVE_FREQS_t enumeration for the parameter.
void DS3232RTC::squareWave(SQWAVE_FREQS_t freq)
{
    uint8_t controlReg;

    controlReg = readRTC(RTC_CONTROL);
    if (freq >= SQWAVE_NONE)
    {
        controlReg |= _BV(INTCN);
    }
    else
    {
        controlReg = (controlReg & 0xE3) | (freq << RS1);
    }
    writeRTC(RTC_CONTROL, controlReg);
}

// Returns the value of the oscillator stop flag (OSF) bit in the
// control/status register which indicates that the oscillator is or    *
// was stopped, and that the timekeeping data may be invalid.
// Optionally clears the OSF bit depending on the argument passed.
bool DS3232RTC::oscStopped(bool clearOSF)
{
    uint8_t s = readRTC(RTC_STATUS);    // read the status register
    bool ret = s & _BV(OSF);            // isolate the osc stop flag to return to caller
    if (ret && clearOSF)                // clear OSF if it's set and the caller wants to clear it
    {
        writeRTC( RTC_STATUS, s & ~_BV(OSF) );
    }
    return ret;
}

// Returns the temperature in Celsius times four.
int DS3232RTC::temperature()
{
    union int16_byte {
        int i;
        byte b[2];
    } rtcTemp;

    rtcTemp.b[0] = readRTC(RTC_TEMP_LSB);
    rtcTemp.b[1] = readRTC(RTC_TEMP_MSB);
    return rtcTemp.i / 64;
}

// Decimal-to-BCD conversion
uint8_t DS3232RTC::dec2bcd(uint8_t n)
{
    return n + 6 * (n / 10);
}

// BCD-to-Decimal conversion
uint8_t __attribute__ ((noinline)) DS3232RTC::bcd2dec(uint8_t n)
{
    return n - 6 * (n >> 4);
}
