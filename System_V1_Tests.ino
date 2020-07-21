
////Libraries=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

//Declare global Variables=-=-=-=-=-=-=-=-=--=-=-=-=-=-=
unsigned long currentTime[] = {0, 0, 0, 0, 0, 0, 0};
byte GPS = 0, Drop = 1;
unsigned int count = 0;
const int dropPin[] = {20, 14, 15, 16, 17, 18, 19};
byte pos = 1, prevState = 0;
bool flight_begun = false, dropped = false, newData = false, useGPS = true, emergency_Status = false;
unsigned long time_On = 5000, time_Off[] = {0, 5000, 500, 5000, 500, 5000, 0}; // edit here for delay times in millis
unsigned long overFlowTime = 45 * 60 * 1000;
float GPSdata[] = {0, 0, 0, 0, 0, 0, 0};
byte Lat = 0, Lon = 1, Alt = 2, prevAlt = 3, numSat = 4;
uint32_t utcTime;




////GPS declaration=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
TinyGPSPlus gps;
SoftwareSerial ss(0, 1); //RX 0, TX 1



void setup() {  //Setup=-=-=-=-=-=-=-=-=-==-=-=-=-=-=-=-=-=-=-=-=-=

  //pin declarations//////
  for (int i = 0; i <= 6; i++) {
    pinMode(dropPin[i], OUTPUT);
  } // pin 0 is only for the emergency drop system. 1-6 are the dropsondes, hence why pos = 1.
  pinMode(13, OUTPUT); //LED on pin 13 for some visual feedback
  pinMode(11, INPUT);
  //serial/ ports//////
  Serial.begin(9600);
  ss.begin(9600);

}


void loop() {
  while (!flight_begun) {
    pre_Flight_System();
    while (digitalRead(11) == HIGH) {
      newData = true;
    }
    if (millis() >= 10000) {
      flight_begun = true;
    }
    currentTime[6] = millis();
  }
  Serial.println("flight has begun!");

  while (1) {
    //Serial.print("1");
    dropDown_System();
    blink(1000);


  }
}

void dropDown_System() { //activate the dropdown system =-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  //Serial.print("6");
  if ((((GPSdata[Alt] >= 15000 && useGPS) || FlightTime() >= 10 * 1000) || emergency_Status) && !dropped) {
    //for this to work, both the atitude mush be above 15 km and gps working, or flight time exceeds 35 min,
    //and the dropsondes havent been dropped yet, or the emergency override says to go ahead
    if (pos <= 6 && prevState == 0 && millis() >= currentTime[Drop] + time_Off[pos - 1]) {
      //if it was off and the interval between uses is great enough
      digitalWrite(dropPin[pos], HIGH);
      prevState = 1;
      Serial.print("Slot "); Serial.print(pos); Serial.print(" ON for "); Serial.print(time_On); Serial.println(" Milliseconds");
      currentTime[Drop] = millis();
    } else if (pos <= 6 && prevState == 1 && millis() >= currentTime[Drop] + time_On) {
      //if it was on and the interval between uses is great enough
      digitalWrite(dropPin[pos], LOW);
      prevState = 0;
      Serial.print("OFF for "); Serial.print(time_Off[pos]); Serial.println(" Milliseconds");
      pos++;
      currentTime[Drop] = millis();
    } else if (pos == 7) { // if all 6 dropsondes have been deployed
      Serial.println("all dropsondes deployed");
      dropped = true;
      Serial.print("LAT = "); Serial.println(GPSdata[Lat], 6);
      Serial.print("LNG = "); Serial.println(GPSdata[Lon], 6);
      Serial.print("ALT = "); Serial.println(GPSdata[Alt]);
      Serial.print("Num  Sat = "); Serial.println(GPSdata[numSat]);
      Serial.print("UTC time = "); Serial.println(utcTime);
      Serial.println("Cool ");
    }
  }
}

void blink(unsigned int per) {//a visual display that things are working=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  //Serial.println("8");
  if (millis() % per > per / 2)
    digitalWrite(13, HIGH);
  if (millis() % per <= per / 2)
    digitalWrite(13, LOW);
}

void pre_Flight_System() { //stuff to do before the flight begins =-=-=-=-=-=-=-=-=-=-=-=-=
  //Serial.print("1");
  FeedGPS();
  getGPSdata();
  blink(2000);  //just a simple slow blink so signify its on and running.

}


unsigned long FlightTime() {// returns the flight time in millis =-=-=-=-=-=-=-=-=-=-=-=-=-=
  unsigned long t = millis() - currentTime[6];
  return t;
}

void FeedGPS() { // see if the GPS has any new data =-=-=-=-=-=-=-=-=--=-=-=-=-=-=
  //Serial.print("2");
  while (ss.available() > 0) {  //while there is new data
    gps.encode(ss.read());  //send that new data to the GPS encoder to be read in getGPSdata()
    newData = true; //there is new data
    useGPS = true; // the GPS is working properly
    currentTime[GPS] = FlightTime();
  }
}


void getGPSdata() { // parse any new data the GPS has =-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  //Serial.print("3");
  if (newData) { // if there is new data
    GPSdata[Lat] = gps.location.lat(); //latitude
    Serial.print("\nLAT = "); Serial.println(GPSdata[Lat], 6);
    GPSdata[Lon] = gps.location.lng(); //longitude
    Serial.print("LNG = "); Serial.println(GPSdata[Lon], 6);
    GPSdata[prevAlt] = GPSdata[Alt]; // used for change in altitude
    GPSdata[Alt] = gps.altitude.meters(); // altitude
    Serial.print("ALT = "); Serial.println(GPSdata[Alt]);
    GPSdata[numSat] = gps.satellites.value();  //number of satalites used for calculations
    Serial.print("Num  Sat =  "); Serial.println(GPSdata[numSat]);
    utcTime = gps.time.value(); //global standard time
    Serial.print("UTC time =  "); Serial.println(utcTime);
    Serial.println(" ");
    newData = false; // all new data has been parsed
  }
}
