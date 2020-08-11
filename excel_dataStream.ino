////Libraries------------------------------------------------------------------
#include <TinyGPS++.h>
#include <EEPROM.h>
#define HWS Serial1 //pins 0 for RX and 1 for TX

// Serial data variables ------------------------------------------------------
const byte kNumberOfChannelsFromExcel = 10;
const char kDelimiter = ',';  // Comma delimiter to separate consecutive data if using more than 1 sensor
const int kSerialInterval = 1000; //edit this for how often excel samples for new data (milliseconds)
unsigned long serialPreviousTime; // Timestamp to track serial interval
float GPSdata[] = {0, 0, 0, 0, 0, 0, 0};
byte Lat = 0, Lon = 1, Alt = 2, prevAlt = 3, numSat = 4, speed = 5;
uint32_t utcTime;
char* arr[kNumberOfChannelsFromExcel];

//setup gps ports ------------------------------------------------------------
TinyGPSPlus gps;

// SETUP ----------------------------------------------------------------------
void setup() {
  // Initialize Serial Communication
  Serial.begin(9600);
  HWS.begin(9600);

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

}

// see if the GPS has any new data --------------------------------------------
void FeedGPS() { 
  //Serial.print("2");
  while (HWS.available() > 0) {  //while there is new data.
    gps.encode(HWS.read());  //send that new data to the GPS encoder to be read in getGPSdata()
  }
}

// SENSOR INPUT CODE-----------------------------------------------------------
void processSensors()
{
  GPSdata[Lat] = gps.location.lat(); //latitude
  GPSdata[Lon] = gps.location.lng(); //longitude
  GPSdata[Alt] = gps.altitude.meters();
  GPSdata[numSat] = gps.satellites.value();  //number of satalites used for calculations
  GPSdata[speed] = gps.speed.mps(); //speed in meters per second
  utcTime = gps.time.value(); //global standard time

}


// OUTGOING SERIAL DATA PROCESSING CODE----------------------------------------
void sendDataToSerial()
{
 //add a delimeter print after each piece of data
  Serial.print(GPSdata[Lat], 8);
  Serial.print(kDelimiter);
  Serial.print(GPSdata[Lon], 8);
  Serial.print(kDelimiter);
  Serial.print(GPSdata[Alt]);
  Serial.print(kDelimiter);
  Serial.print(GPSdata[numSat]);
  Serial.print(kDelimiter);
  Serial.print(GPSdata[speed]);
  Serial.print(kDelimiter);
  Serial.print(utcTime);
  Serial.print(kDelimiter);

  Serial.println(); // Add final line ending character only once
}


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
