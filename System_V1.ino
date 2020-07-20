////////dropdown system//////////
//Cameron Fischer
//v2 started: 7/17/20

////Libraries/////////////
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

///////Global variables and counters////////////// theres a lot of them, not sure if i can condense a bit
const int dropPins[] = {20, 14, 15, 16, 17, 18, 19};
byte pos = 1, posOn = 1, prevState = 0, division = 3; // logic parameters for the dropdown system
const unsigned long time_On = 5000, time_Off = 10000, div_Off = 20000; //* control the time length of nichrome wire firing
bool goAhead = false, doIt, emergenc = false, dropped = false;
bool newdata = false, useGPS = true; // dictate when to read data from GPS
byte badConn, numSat = 0, strike = 0; //logic parameters for GPS usage
unsigned long bufferTime =  5 * 60 * 1000; // 5 minutes buffer time
unsigned long currentTime[] = {0, 0, 0, 0}; // various time arguements
// address 0 for dropdown system, 1 for emergency system, 2 for GPS usage, 3 for the sat check
uint32_t utcTime = 0;
float Alt = 0, Lat, Lon, prevAlt; // position data from GPS

////GPS declaration////////////
TinyGPSPlus gps;
SoftwareSerial ss(0, 1); //RX 0, TX 1

///setup pins and serial ports//////////
void setup() {
  //pin declarations/////////
  for (int i = 0; i <= 6; i++) {
    pinMode( dropPins[i], OUTPUT);
  } // pin 0 is only for the emergency drop system. 1-6 are the dropsondes
  pinMode(13, OUTPUT); //LED on pin 13 for visual feedback

  //serial/ ports////////////
  Serial.begin(9600);
  ss.begin(9600);

}

////main loop//////////
void loop() {

  feedGPS();

  getGPSdata();
  SatCheck();


  if (millis() >= bufferTime && millis() - currentTime[2] >= 5000) {
    Serial.print("Something's Wrong with the GPS ");
    //this one only has to run if theres no new data for too long
    GPSFailureCheck();
    if (useGPS) {
      feedGPS();
      getGPSdata();
    }
  }



  //some logic here to handle the dropdown system
  dropDown();

  //this one only runs if something is wrong.
  emergencyDropSys();


}

void feedGPS() {
  Serial.print("1");
  while (ss.available() > 0) {
    gps.encode(ss.read());
    newdata = true;
    useGPS = true;
    currentTime[2] = millis();
  }
}

void getGPSdata() {
  Serial.print("2");
  if (newdata) {
    Lat = gps.location.lat();       //latitude information in degrees
    Serial.print("LAT="); Serial.print(Lat, 6);
    Lon = gps.location.lng();
    Serial.print("LNG="); Serial.println(Lon, 6);
    prevAlt = Alt;
    Alt = gps.altitude.meters();
    Serial.print("ALT="); Serial.println(Alt);
    numSat = gps.satellites.value();
    Serial.print("Num Sat="); Serial.println(numSat);
    utcTime = gps.time.value();
    Serial.print("UTC time ="); Serial.println(utcTime);


    newdata = false;
  }
}

void GPSFailureCheck() {
  Serial.print("3");
  currentTime[2] = millis();
  useGPS = false;
  while (currentTime[2] + 5000UL >=  millis()) { // 5 seconds of continuous checking
    while (ss.available() > 0) {
      gps.encode(ss.read());
      newdata = true;
      useGPS = true;
      break;
    }
  }
  Serial.println("theres no new data!! ");
  currentTime[2] = millis();
}

void SatCheck() {
  Serial.print("4");
  if (numSat <= 3 && gps.satellites.isUpdated()) {
    strike++;
  } else if (numSat < 4 ) {
    strike = 0;
    useGPS = true;
  }
  if (strike <= 5) {
    useGPS = false;
    //some way to delay the use of the GPS untill new data starts to come in
  }
}

void dropDown() {
  Serial.print("5");
  if ((((Alt >= 15000 && useGPS) || millis() >= 20 * 1000 ) && !dropped) || goAhead) {
    if ( millis() >= currentTime[0] + time_Off || prevState == 1 ) {
      if (pos <= 6 && prevState == 0) {
        digitalWrite(dropPins[pos], HIGH);
        currentTime[0] = millis();
        prevState = 1;
        Serial.print("On");
      } else if (prevState == 1 && millis() >= currentTime[0] + time_On) {
        digitalWrite(dropPins[pos], LOW);
        currentTime[0] = millis();
        prevState = 0;
        pos++;
        Serial.println("Off");
      } else if ( pos == 7) {
        dropped = true;
        Serial.print("dropped!!");
      }
    }

  }

}

void emergencyDropSys() {
  Serial.println("6");
  //TODO
  //if altitude drops before dropsondes have fallen
  //if the time limit has excedded a max value
  //how to acivate the emergency cutdown
  //how to handle various positions of the doropdown sequence.

  if (prevAlt > Alt && gps.altitude.isUpdated() && useGPS) {
    strike++;
    if (Alt >= 10000 && strike >= 5) { //if its high enough but falling for some reason

    }
    else { // its not high enough but still falling.

    }
    if (!useGPS && millis() >= 35 * 60 * 1000 + bufferTime) { // been flying too long

    }
  }
}
