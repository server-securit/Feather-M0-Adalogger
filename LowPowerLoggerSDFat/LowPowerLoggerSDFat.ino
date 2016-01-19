/*
 Low Power logger for Feather M0 Adalogger

 Uses internal RTC and interrupts to put M0 into deep sleep.
 Output logged to uSD at intervals set

 Created by:  Jonathan Davies
 Date:        01/01/16
 Version:     0.3
 Changes: Limited number of file flushes to reduce power
 
*/

////////////////////////////////////////////////////////////
// #define ECHO_TO_SERIAL // Allows serial output if uncommented
////////////////////////////////////////////////////////////

#include <RTCZero.h>
#include <SPI.h>
// #include <SD.h>
#include <SdFat.h>
SdFat SD;

#define cardSelect 4  // Set the pin used for uSD
#define RED 13 // Red LED on Pin #13
#define GREEN 8 // Green LED on Pin #8
#define VBATPIN A7    // Battery Voltage on Pin A7

#ifdef ARDUINO_SAMD_ZERO
   #define Serial SerialUSB   // re-defines USB serial from M0 chip so it appears as regular serial
#endif

extern "C" char *sbrk(int i); //  Used by FreeRAm Function

//////////////// Key Settings ///////////////////

#define SampleIntSec 60 // RTC - Sample interval in seconds
#define SamplesPerCycle 60  // Number of samples to buffer before uSD card flush is called

const int SampleIntSeconds = 500;   //Simple Delay used for testing, ms i.e. 1000 = 1 sec

/* Change these values to set the current initial time */
const byte hours = 10;
const byte minutes = 11;
const byte seconds = 0;
/* Change these values to set the current initial date */
const byte day = 05;
const byte month = 01;
const byte year = 16;

/////////////// Global Objects ////////////////////
RTCZero rtc;    // Create RTC object
File logfile;   // Create file object
char filename[15];
  
float measuredvbat;   // Variable for battery voltage
int NextAlarmSec; // Variable to hold next alarm time in seconds
unsigned int CurrentCycleCount;  // Num of smaples in current cycle, before uSD flush call



//////////////    Setup   ///////////////////
void setup() {

  //Set board LED pins as output
  pinMode(13, OUTPUT);
  pinMode(8, OUTPUT);

  //  Setup RTC and set time
  rtc.begin();    // Start the RTC in 24hr mode
  rtc.setTime(hours, minutes, seconds);   // Set the time
  rtc.setDate(day, month, year);    // Set the date

  strcpy(filename, "ANALOG00.CSV");
  CreateFile();   


}  

/////////////////////   Loop    //////////////////////
void loop() {

  
  blink(GREEN,2);             // Quick blink to show we have a pulse
  CurrentCycleCount += 1;     //  Increment samples in current uSD flush cycle

  #ifdef ECHO_TO_SERIAL
    SerialOutput();           // Only logs to serial if ECHO_TO_SERIAL is uncommented at start of code
  #endif
  
  SdOutput();                 // Output to uSD card

  // Code to limit the number of power hungry writes to the uSD
  if( CurrentCycleCount >= SamplesPerCycle ) {
    logfile.flush();
    CurrentCycleCount = 0;
    #ifdef ECHO_TO_SERIAL
      Serial.println("logfile.flush() called");
    #endif
  }

  
  ///////// Interval Timing and Sleep Code ////////////////
  //delay(SampleIntSeconds);   // Simple delay for testing only interval set by const in header

  NextAlarmSec = (NextAlarmSec + SampleIntSec) % 60;   // i.e. 65 becomes 5
  rtc.setAlarmSeconds(NextAlarmSec); // RTC time to wake, currently seconds only
  rtc.enableAlarm(rtc.MATCH_SS); // Match seconds only
  rtc.attachInterrupt(alarmMatch); // Attaches function to be called, currently blank
  delay(5); // Brief delay prior to sleeping not really sure its required
  
  rtc.standbyMode();    // Sleep until next alarm match
  
  // Code re-starts here after sleep !

}

///////////////   Functions   //////////////////

// Debbugging output of time/date and battery voltage
void SerialOutput() {

  //Serial.print(CurrentCycleCount);
  //Serial.print(":");
  //Serial.print(freeram ());
  //Serial.print("-");
  Serial.print(rtc.getDay());
  Serial.print("/");
  Serial.print(rtc.getMonth());
  Serial.print("/");
  Serial.print(rtc.getYear()+2000);
  Serial.print(" ");
  Serial.print(rtc.getHours());
  Serial.print(":");
  if(rtc.getMinutes() < 10)
    Serial.print('0');      // Trick to add leading zero for formatting
  Serial.print(rtc.getMinutes());
  Serial.print(":");
  if(rtc.getSeconds() < 10)
    Serial.print('0');      // Trick to add leading zero for formatting
  Serial.print(rtc.getSeconds());
  Serial.print(",");
  Serial.println(BatteryVoltage ());   // Print battery voltage  
}

// Print data and time followed by battery voltage to SD card
void SdOutput() {

  // Formatting for file out put dd/mm/yyyy hh:mm:ss, [sensor output]
  
  logfile.print(rtc.getDay());
  logfile.print("/");
  logfile.print(rtc.getMonth());
  logfile.print("/");
  logfile.print(rtc.getYear()+2000);
  logfile.print(" ");
  logfile.print(rtc.getHours());
  logfile.print(":");
  if(rtc.getMinutes() < 10)
    logfile.print('0');      // Trick to add leading zero for formatting
  logfile.print(rtc.getMinutes());
  logfile.print(":");
  if(rtc.getSeconds() < 10)
    logfile.print('0');      // Trick to add leading zero for formatting
  logfile.print(rtc.getSeconds());
  logfile.print(",");
  logfile.println(BatteryVoltage ());   // Print battery voltage
}

// Write data header.
void writeHeader() {
  logfile.println("DD:MM:YYYY hh:mm:ss, Battery Voltage");
}

// blink out an error code
void error(uint8_t errno) {
  while(1) {
    uint8_t i;
    for (i=0; i<errno; i++) {
      digitalWrite(13, HIGH);
      delay(100);
      digitalWrite(13, LOW);
      delay(100);
    }
    for (i=errno; i<10; i++) {
      delay(200);
    }
  }
}

// blink out an error code, Red on pin #13 or Green on pin #8
void blink(uint8_t LED, uint8_t flashes) {
  uint8_t i;
  for (i=0; i<flashes; i++) {
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
    delay(200);
  }
}

// Measure battery voltage using divider on Feather M0 - Only works on Feathers !!
float BatteryVoltage () {
  measuredvbat = analogRead(VBATPIN);   //Measure the battery voltage at pin A7
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  return measuredvbat;
}


void CreateFile()
{
    #ifdef ECHO_TO_SERIAL
    while (! Serial); // Wait until Serial is ready
    Serial.begin(115200);
    Serial.println("\r\nFeather M0 Analog logger");
  #endif
  
  // see if the card is present and can be initialized:
  if (!SD.begin(cardSelect)) {
    Serial.println("Card init. failed! or Card not present");
    error(2);     // Two red flashes means no card or card init failed.
  }
  
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = '0' + i/10;
    filename[7] = '0' + i%10;
    // create if does not exist, do not open existing, write, sync after write
    if (! SD.exists(filename)) {
      break;
    }
  }  

  logfile = SD.open(filename, FILE_WRITE);
  if( ! logfile ) {
    Serial.print("Couldnt create "); 
    Serial.println(filename);
    error(3);
  }
  Serial.print("Writing to "); 
  Serial.println(filename);
  Serial.println("Logging ....");
}

void alarmMatch() // Do something when interrupt called
{
  
}

// Small ARM free RAM function for use during debugging
int freeram () {
  char stack_dummy = 0;
  return &stack_dummy - sbrk(0);
}
