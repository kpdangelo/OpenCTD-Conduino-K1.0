///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
   Conduino_Qduino_CTD_V02

   This sketch is adapted from the open CTD from Oceanography for Everyone
   http://www.oceanographyforeveryone.com/
   https://github.com/OceanographyforEveryone/OpenCTD

   Sensors are
   MS5803 pressure sensor
   Dallas 1-wire temperature sensors
   Conduino conductivity sensor which uses the AD5933 Network Analyzer

   Marco Carminati, Paolo Luzzatto-Fegiz,
   Conduino: Affordable and high-resolution multichannel water conductivity sensor using micro USB connectors,
   In Sensors and Actuators B: Chemical, Volume 251, 2017, Pages 1034-1041, ISSN 0925-4005,
   https://doi.org/10.1016/j.snb.2017.05.184.

   output format on SD card:
   TimeStamp(seconds) Pressure(mbar) Temperature 1 2 3 (deg C) Conductivity (us/cm)

   Kevin D'Angelo
   7/22/18
*/
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <Wire.h>
#include "Qduino.h"
qduino q;
#include <MS5803_14.h>
#include <avr/wdt.h>      // watch dog timer
///////////////////////////////////////////////////////////////////////////////////////////////
//  Sample Interval
//  loop takes about 810ms, so the sample interval needs to be greater than that.
//  The bulk of the time, 622ms, is used to communicate with the 1-wire temperature sensors.
//  watch dog timer is continuously reset while waiting for sample interval.
////////////////////////////////////////////////////////////////////////////////////////////////
#define SampleInterval 1000      // 1000 ms
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Temperature and pressure releated
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//OneWire is used to get data from both temperature and conductivity sensors
#include <OneWire.h>  //CAN WE MAKE THIS LIBRARY SMALLER?

//DallasTemperature is used to query the temeprature sensors
#include <DallasTemperature.h>   //CAN WE MAKE THIS LIBRARY SMALLER?

// For the SD card reader.
#include <SPI.h>
#include <SD.h>

// Declare global variables.
float tempA;
float tempB;
float tempC;

OneWire oneWire(6);        // Define the OneWire port for temperature.
DallasTemperature sensors(&oneWire);  // Define DallasTemperature input based on OneWire.

MS_5803 sensor = MS_5803(4096);     // pressure sensor
double pressure_abs; // define absolute pressure variable.

/////////////////////////////////////////////////////////////////////////////////
//    CONDUINO RELATED
///////////////////////////////////////////////////////////////////////////////
// The user must calibrate the AD5933 system for a known impedance range to determine the gain factor

// polynomial coefficients for conversion from conduino reading to conductivity
// conductivity = CA*X^2 + CB*X + CC, where X is the conduino reading
// Unit 1
//#define CA 0.0851
//#define CB 7.1241
//#define CC 0.2871
// Unit 2
//#define CA 0.0702
//#define CB 7.3119
//#define CC 0.2013
// Unit 3 
//#define CA 0.0851
//#define CB 7.1241
//#define CC 0.2871
// Unit 4
#define CA 0.0745
#define CB 7.3287
#define CC 0.1729

#define AD5933_ADDR 0x0D

double ConduinoReading;
double Conductivity;
double magnitude;
double phase;
double impedance;
double gain = 5.825 * pow(10, -6); //calibrated with 180.5 Ohm resistor on 8/17/18

const float CLOCK_SPEED = 16.776 * pow(10, 6); // AD5933 has internal clock of 16.776 MHz

const int CONTROL_REGISTER[2] =                    { 0x80, 0x81       }; // see mapping below
const int START_FREQUENCY_REGISTER[3] =            { 0x82, 0x83, 0x84 }; // 24 bits for start frequency
const int FREQ_INCREMENT_REGISTER[3] =             { 0x85, 0x86, 0x87 }; // 24 bits for frequency increment
const int NUM_INCREMENTS_REGISTER[2] =             { 0x88, 0x89       }; //  9 bits for # of increments
const int NUM_SETTLING_CYCLES_REGISTER[2] =        { 0x8A, 0x8B       }; //  9 bits + 1 modifier for # of settling times
const int STATUS_REGISTER[1] =                     { 0x8F             }; // see mapping below
const int TEMPERATURE_DATA_REGISTER[2] =           { 0x92, 0x93       }; // 16-bit, twos complement format
const int REAL_DATA_REGISTER[2] =                  { 0x94, 0x95       }; // 16-bit, twos complement format
const int IMAG_DATA_REGISTER[2] =                  { 0x96, 0x97       }; // 16-bit, twos complement format

// control register map (D15 to D12)
const int DEFAULT_VALUE =                 B0000; // initial setting
const int INITIALIZE =                    B0001; // excite the unknown impedance initially
const int START_SWEEP =                   B0010; // begin the frequency sweep
const int INCREMENT =                     B0011; // step to the next frequency point
const int REPEAT =                        B0100; // repeat the current frequency point measurement
const int MEASURE_TEMP =                  B1001; // initiates a temperature reading
const int POWER_DOWN =                    B1010; // VOUT and VIN are connected internally to ground
const int STANDBY =                       B1011; // VOUT and VIN are connected internally to ground

const int BLOCK_WRITE = B1010000;
const int BLOCK_READ =  B1010001;
const int ADDRESS_PTR = 0xB0;// B1011000;

const int validTemperatureMeasurement =  1;
const int validImpedanceData =           2;
const int frequencySweepComplete =       4;

int temperatureData;

//functions declarations
int checkStatus();
boolean measureTemperature();
byte frequencyCode(float, int);
boolean setStartFrequency(float);
boolean setFrequencyIncrement(float);
boolean setNumberOfIncrements(int);
boolean setNumberOfSettlingTimes(int);
void measureImpedance();
void configureAD5933(int, float, float, int);
int getByte(int);
boolean setByte(int, int);
boolean setControlRegister(int);

boolean setControlRegister2();

int reading = 0;
byte rxByte;
int number;
char FirstChar[1];
char SecondChar[1];

int e = 0;
int y;
int ch;
int u;
int real;
int imag ;

long timeStamp;
long time1;
/////////////////////////////////////////////////////////////////////////////////
//    setup()
///////////////////////////////////////////////////////////////////////////////
void setup() {
  Wire.begin();
  Serial.begin(9600);
  wdt_enable(WDTO_2S);      // 2 second watch dog timer reset interval
  q.setup();

  pinMode(8, OUTPUT);   // mux
  pinMode(9, OUTPUT);   // on/off for conduino

  digitalWrite (8, LOW);     // set mux HIGH or LOW, it is not used
  digitalWrite (9, LOW);     // turn off conduino

  if (!SD.begin(4)) {
    q.setRGB(RED);    // SD card bad, turn on the red light
    wdt_reset();      // pet the dog
    delay(1000);
    return;
  }
  else {          // SD card is good, turn on the green light for 1 second
    q.setRGB(GREEN);
    wdt_reset();      // pet the dog
    delay(1000);
    q.ledOff();
  }

  wdt_reset();      // pet the dog

  sensors.begin();    // Initialize the temperature sensors
  sensor.initializeMS_5803();   // Initialize pressure sensor

  time1 = millis();     // initialize time stamp
}
/////////////////////////////////////////////////////////////////////////////////
//    loop() ~ 860ms
///////////////////////////////////////////////////////////////////////////////
void loop() {

  timeStamp += millis() - time1;
  time1 = millis();

  sensor.readSensor();    // read pressure sensor, takes 22ms

  // Read temperature sensors.
  sensors.requestTemperatures();      // takes 622 ms
  tempA = sensors.getTempCByIndex(0);
  tempB = sensors.getTempCByIndex(1);
  tempC = sensors.getTempCByIndex(2); // takes 126ms

  digitalWrite (9, HIGH);   // Conduino on
  delay (1);                // more than enough time to settle
  measureImpedance();       // get measurement from Conduino
  digitalWrite(9, LOW);     // Conduino off

  File dataFile = SD.open("datalog.txt", FILE_WRITE);   // takes ~4ms
  if (dataFile) {
    dataFile.print(timeStamp / 1000); dataFile.print(" ");
    dataFile.print(sensor.pressure()); dataFile.print(" ");
    dataFile.print(tempA); dataFile.print(" ");
    dataFile.print(tempB); dataFile.print(" ");
    dataFile.print(tempC); dataFile.print(" ");
    dataFile.print(Conductivity); dataFile.print(" ");
    dataFile.print(ConduinoReading, 4); dataFile.print(" ");  // raw number from conduino for calibration
    //dataFile.print(temperatureData); dataFile.print(" ");     // temperature from 5933 chip
    dataFile.println("");
    dataFile.close();
  }

  //    Write to serial monitor or not Comment/Uncomment
//      Serial.print(timeStamp / 1000); Serial.print(" ");
//      Serial.print (sensor.pressure()); Serial.print(" ");
//      Serial.print (tempA); Serial.print(" ");
//      Serial.print (tempB); Serial.print (" ");
//      Serial.print (tempC); Serial.print (" ");
//      Serial.print (Conductivity); Serial.print (" ");
//      Serial.print (ConduinoReading, 4); Serial.print(" ");  // for calibration
//      Serial.print(impedance); Serial.print (" ");
//      Serial.print (magnitude);
//      Serial.println("");

  while (millis() - time1 <= SampleInterval) { // wait for Sample Interval
    wdt_reset();              // reset the watchdog timer in case SampleInterval > 2000
  }
}
////////////////////////////////////////////////////////////////////////
//  Conduino Related Subroutines
////////////////////////////////////////////////////////////////////////
// message sequence:
// to set the memory address:
// 1. slave address
// 2. pointer command
// 3. register address to point to

// to write data (address already set):
// 1. slave address
// 2. block write
// 3. number bytes write
// 4. byte 0, byte 1, byte 2, etc.

// to read data (address is already set):
// 1. slave address with read bit high
// 2. register data is returned

// to read a block of data (address is already set):
// 1. slave address
// 2. block read
// 3. number of bytes
// 4. slave address (read bit high)
// 5. byte 0, byte 1, byte 2, etc.

// control register map (D10 to D9)
// ranges = {2.0 V p-p, 200 mV p-p, 400 mV p-p, 1.0 V p-p}

// control register map (D11, D8 to D0)
// D11 = no operation
// D8 = PGA gain (0 = x5, 1 = x1) // amplifies the response signal into the ADC
// D7 = reserved, set to 0
// D6 = reserved, set to 0
// D5 = reserved, set to 0
// D4 = reset // interrupts a frequency sweep
// D3 = external system clock, set to 1; internal system clock, set to 0
// D2 = reserved, set to 0
// D1 = reserved, set to 0
// D0 = reserved, set to 0

// number of increments register
// 0x88: D15 to D9 -- don't care
// 0x89: D8 -- number of increments bit 1
// 0x89: D7 to D0 -- number of increments bits 2 through 9; 9-bit integer number stored in binary format

// number of settling times
// 0x8A: D15 to D11 -- don't care
// 0x8A: D10 to D9 -- 2-bit decode
//        0 0 = default
//        0 1 = # of cycles x 2
//        1 0 = reserved
//        1 1 = # of cycles x 4
// 0x8A: D8 -- MSB number of settling times
// 0x8B: D7 to D0 -- number of settling times; 9-bit integer number stored in binary format

const int OUTPUT_VOLTAGE[4] = {
  B00, B01, B10, B11
};

/////////////////////////////////////////////////////////////////////////////////
//    Measure Impedance
///////////////////////////////////////////////////////////////////////////////
void measureImpedance() {

  real = 0;
  imag = 0;
  e = 0;
  ControlReset();
  configureAD5933(145, // Conduino network analyzer, number of settling times
                  90000, // start frequency (Hz)
                  1, // frequency increment (Hz)
                  0); // number of increments

  //0.Inizialize bit D11,D10,D9,D8
  setControlRegister2(4);   // range=4, 200mv signal
  //  setControlRegister2(3);   // range=3, 400mv signal
  //  setControlRegister2(1);   // range=1, 2v signal

  // 1. place AD5933 in standby mode
  setControlRegister(STANDBY);
  // 2. initialize with start frequency
  setControlRegister(INITIALIZE);
  // add a delay here
  delay(10);

  // 3. start frequency sweep
  setControlRegister(START_SWEEP);

  while (checkStatus() < validImpedanceData) {    // wait for DFT to complete
    wdt_reset();
  }
  real = getByte(REAL_DATA_REGISTER[0]) << 8;
  real |= getByte(REAL_DATA_REGISTER[1]);

  imag = getByte(IMAG_DATA_REGISTER[0]) << 8;
  imag |= getByte(IMAG_DATA_REGISTER[1]);

  magnitude = sqrt(pow(double(real), 2) + pow(double(imag), 2));
  phase = atan(double(imag) / double(real)) * 360 / (2 * 3.14) + 47;
  impedance = 1 / (gain * magnitude);
  ConduinoReading = (129 / impedance);
  Conductivity = CA * ConduinoReading * ConduinoReading + CB * ConduinoReading + CC; // ms/cm
  Conductivity = Conductivity * 1000;  // us/cm

  setControlRegister(INCREMENT);

  //  measureTemperature();           // keep track of AD5933 internal temperature
  // 8. power-down mode
  setControlRegister(POWER_DOWN);
}
/////////////////////////////////////////////////////////////////////////////////
//    check status register, used in polling
///////////////////////////////////////////////////////////////////////////////
int checkStatus() {
  return (getByte(STATUS_REGISTER[0]) & 7);
}
/////////////////////////////////////////////////////////////////////////////////
//    Measure internal temperature of network analyzer chip
///////////////////////////////////////////////////////////////////////////////
boolean measureTemperature() {
  setControlRegister(MEASURE_TEMP);
  delay(10); // wait for 10 ms
  if (checkStatus() & 1 == validTemperatureMeasurement) {

    // temperature is available
    temperatureData = getByte(TEMPERATURE_DATA_REGISTER[0]) << 8;
    temperatureData |= getByte(TEMPERATURE_DATA_REGISTER[1]);
    temperatureData &= 0x3FFF; // remove first two bits

    if (temperatureData & 0x2000 == 1) { // negative temperature
      temperatureData -= 0x4000;
    }
    temperatureData /= 32;
    setControlRegister(POWER_DOWN);
    return true;
  }
  else {
    return false;
  }
}
/////////////////////////////////////////////////////////////////////////////////
//    start frequency and frequency increment formula
///////////////////////////////////////////////////////////////////////////////
byte frequencyCode(float freqInHz, int byteNum) {
  long value = long((freqInHz / (CLOCK_SPEED / 4)) * pow(2, 27));

  byte code[3];

  code[0] = (value & 0xFF0000) >> 0x10;
  code[1] = (value & 0x00FF00) >> 0x08;
  code[2] = (value & 0x0000FF);

  return code[byteNum];
}
/////////////////////////////////////////////////////////////////////////////////
//    start frequency and frequency increment formula:
///////////////////////////////////////////////////////////////////////////////
boolean setStartFrequency(float freqInHz) {
  boolean statusValue;

  for (int n = 0; n < 3; n++) {
    statusValue = setByte(START_FREQUENCY_REGISTER[n], frequencyCode(freqInHz, n));
  }

  return statusValue;
}
/////////////////////////////////////////////////////////////////////////////////
//    start frequency and frequency increment formula:
///////////////////////////////////////////////////////////////////////////////
boolean setFrequencyIncrement(float freqInHz) {
  boolean statusValue;
  for (int n = 0; n < 3; n++) {
    statusValue = setByte(FREQ_INCREMENT_REGISTER[n], frequencyCode(freqInHz, n));
  }
  return statusValue;
}
/////////////////////////////////////////////////////////////////////////////////
//    start frequency and frequency increment formula:
///////////////////////////////////////////////////////////////////////////////
boolean setNumberOfIncrements(int n) {

  boolean i2cStatus;

  int numIncrements = min(n, 511);

  i2cStatus = setByte(NUM_INCREMENTS_REGISTER[0], numIncrements >> 8);
  i2cStatus = setByte(NUM_INCREMENTS_REGISTER[1], numIncrements & 255);

  return i2cStatus;
}
/////////////////////////////////////////////////////////////////////////////////
//    start frequency and frequency increment formula:
///////////////////////////////////////////////////////////////////////////////
boolean setNumberOfSettlingTimes(int n) {
  int decode;
  int numSettlingTimes = min(n, 2047);

  if (n > 1023) {
    decode = 3;
    numSettlingTimes /= 4;
  }
  else if (n > 511) {
    decode = 1;
    numSettlingTimes /= 2;
  }
  else {
    decode = 0;
    numSettlingTimes = n;
  }

  boolean i2cStatus;

  i2cStatus = setByte(NUM_SETTLING_CYCLES_REGISTER[0], (numSettlingTimes >> 8) + (decode << 1));
  i2cStatus = setByte(NUM_SETTLING_CYCLES_REGISTER[1], numSettlingTimes & 255);

  return i2cStatus;
}
/////////////////////////////////////////////////////////////////////////////////
//    Configure network analyzer register settings
///////////////////////////////////////////////////////////////////////////////
void configureAD5933(int settlingTimes, float startFreq, float freqIncr, int numIncr) {
  setNumberOfSettlingTimes(settlingTimes);
  setStartFrequency(startFreq);
  setFrequencyIncrement(freqIncr);
  setNumberOfIncrements(numIncr);
}
///////////////////////////////////////////////////////////////////////////////
// How to write or read a byte:
// 1. master device asserts a start condition on SDA
// 2. master sends the 7-bit slave address followed by the write bit (low)
// 3. addressed slave device asserts an acknowledge on SDA
// 4. The master send a register address or pointer command code (1011 0000)
// 5. the slave asserts an acknowledge on SDA
// 6. the master sends a data byte or register address to where the pointer is to point
// 7. the slave asserts an acknowledge on SDA
// 8. the master asserts a stop condition on SDA to end the transaction
/////////////////////////////////////////////////////////////////////////////////
//    data I/O
///////////////////////////////////////////////////////////////////////////////
int getByte(int address) {
  int rxByte;
  Wire.beginTransmission(AD5933_ADDR);
  Wire.write(ADDRESS_PTR);
  Wire.write(address);
  int i2cStatus = Wire.endTransmission();

  Wire.requestFrom(AD5933_ADDR, 1);

  if (1 <= Wire.available()) {
    rxByte = Wire.read();
  }
  else {
    rxByte = -1;
  }
  return rxByte;
}
/////////////////////////////////////////////////////////////////////////////////
//    data I/O
///////////////////////////////////////////////////////////////////////////////
boolean setByte(int address, int value) {

  Wire.beginTransmission(AD5933_ADDR);
  Wire.write(address);
  Wire.write(value);
  int i2cStatus = Wire.endTransmission();

  if (i2cStatus)
    return false;
  else
    return true;
}
/////////////////////////////////////////////////////////////////////////////////
//    control register
///////////////////////////////////////////////////////////////////////////////
boolean setControlRegister(int code) {

  int rxByte = getByte(CONTROL_REGISTER[0]);

  rxByte &= 0x0F; // clear upper four bits
  rxByte |= code << 4; // set to 1011

  boolean s = setByte(CONTROL_REGISTER[0], rxByte);

  delay(1);

  return s;
}
////////////////////////////////////////////////////////////////////////////
//  reset bit of control register
///////////////////////////////////////////////////////////////////////
void ControlReset() {
  setByte(CONTROL_REGISTER[1], 0x10);

}
/////////////////////////////////////////////////////////////////////////////////
//    control register, here you can set the magnitude of the stimulus signal
///////////////////////////////////////////////////////////////////////////////
boolean setControlRegister2(int range) {

  int rxByte = getByte(CONTROL_REGISTER[0]);

  rxByte &= 0xF0; // clear lower four bits
  switch (range) {
    case 1:
      rxByte |= B00000001; // set to 11 PGA gain=x1, Range1: 2v
      break;
    case 2:
      rxByte |= B00000111; // set to 11 PGA gain=x1, Range2: 1v
      break;
    case 3:
      rxByte |= B00000101;  // set to 10 PGA gain=x1, range3: 400mv
      break;
    case 4:
      rxByte |= B00000011; // set to 01 PGA gain=x1, Range4: 200mv,
      break;
  }
  boolean s = setByte(CONTROL_REGISTER[0], rxByte);
  delay(10);
  return s;
}

