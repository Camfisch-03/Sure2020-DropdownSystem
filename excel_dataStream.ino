////Libraries=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>


// Program variables ----------------------------------------------------------
int exampleVariable = 0;
int sensorPin = A0;

// Serial data variables ------------------------------------------------------
//Incoming Serial Data Array
const byte kNumberOfChannelsFromExcel = 10;

// Comma delimiter to separate consecutive data if using more than 1 sensor
const char kDelimiter = ',';
// Interval between serial writes
const int kSerialInterval = 1000; //edit this for how often excel samples for new data (milliseconds)
// Timestamp to track serial interval
unsigned long serialPreviousTime;
float GPSdata[] = {0, 0, 0, 0, 0, 0, 0}, temp = 0;
byte Lat = 0, Lon = 1, Alt = 2, prevAlt = 3, numSat = 4;
uint32_t utcTime;
char* arr[kNumberOfChannelsFromExcel];


TinyGPSPlus gps;
SoftwareSerial ss(0, 1); //RX 0, TX 1

// SETUP ----------------------------------------------------------------------
void setup() {
  // Initialize Serial Communication
  Serial.begin(9600);
  ss.begin(9600);

}

// START OF MAIN LOOP ---------------------------------------------------------
void loop()
{
  FeedGPS();

  if (gps.location.isUpdated()) {
    processSensors();
  }
  // Read Excel variables from serial port (Data Streamer)
  processIncomingSerial();

  // Process and send data to Excel via serial port (Data Streamer)
  processOutgoingSerial();


  // Compares STR1 to STR2 returns 0 if true.
  //   if ( strcmp ("Apple", arr[0]) == 0){
  //       Serial.println("working");
  //   }
}

void FeedGPS() { // see if the GPS has any new data =-=-=-=-=-=-=-=-=--=-=-=-=-=-=
  //Serial.print("2");
  while (ss.available() > 0) {  //while there is new data.
    gps.encode(ss.read());  //send that new data to the GPS encoder to be read in getGPSdata()
  }
}

// SENSOR INPUT CODE-----------------------------------------------------------
void processSensors()
{
  GPSdata[Lat] = gps.location.lat(); //latitude
  GPSdata[Lon] = gps.location.lng(); //longitude
  GPSdata[Alt] = gps.altitude.meters();
  GPSdata[numSat] = gps.satellites.value();  //number of satalites used for calculations
  utcTime = gps.time.value(); //global standard time

}

// Add any specialized methods and processing code below


// OUTGOING SERIAL DATA PROCESSING CODE----------------------------------------
void sendDataToSerial()
{
  // Send data out separated by a comma (kDelimiter)
  // Repeat next 2 lines of code for each variable sent:

  Serial.print(GPSdata[Lat], 8);
  Serial.print(kDelimiter);
  Serial.print(GPSdata[Lon], 8);
  Serial.print(kDelimiter);
  Serial.print(GPSdata[Alt]);
  Serial.print(kDelimiter);
  Serial.print(GPSdata[numSat]);
  Serial.print(kDelimiter);
  Serial.print(utcTime);
  Serial.print(kDelimiter);

  Serial.println(); // Add final line ending character only once
}

//-----------------------------------------------------------------------------
// DO NOT EDIT ANYTHING BELOW THIS LINE
//-----------------------------------------------------------------------------

// OUTGOING SERIAL DATA PROCESSING CODE----------------------------------------
void processOutgoingSerial()
{
  // Enter into this only when serial interval has elapsed
  if ((millis() - serialPreviousTime) > kSerialInterval)
  {
    // Reset serial interval timestamp
    serialPreviousTime = millis();
    sendDataToSerial();
  }
}

// INCOMING SERIAL DATA PROCESSING CODE----------------------------------------
void processIncomingSerial()
{
  if (Serial.available()) {
    parseData(GetSerialData());
  }
}

// Gathers bytes from serial port to build inputString
char* GetSerialData()
{
  static char inputString[64]; // Create a char array to store incoming data
  memset(inputString, 0, sizeof(inputString)); // Clear the memory from a pervious reading
  while (Serial.available()) {
    Serial.readBytesUntil('\n', inputString, 64); //Read every byte in Serial buffer until line end or 64 bytes
  }
  return inputString;
}

// Seperate the data at each delimeter
void parseData(char data[])
{
  char *token = strtok(data, ","); // Find the first delimeter and return the token before it
  int index = 0; // Index to track storage in the array
  while (token != NULL) { // Char* strings terminate w/ a Null character. We'll keep running the command until we hit it
    arr[index] = token; // Assign the token to an array
    token = strtok(NULL, ","); // Conintue to the next delimeter
    index++; // incremenet index to store next value
  }
}
