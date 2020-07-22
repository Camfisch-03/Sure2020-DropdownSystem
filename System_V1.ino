////////dropdown system////////
//Cameron Fischer
//v2, started: 7/20/20

//TODO//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//tidy up the emergency system ,as well as fugure out more of what is has to do
//do some tests to make sure at least the concepts work
//just generally tidy everything up
//i have a lot to do, i just dont know what it is yet


////Libraries=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

//Declare global Variables=-=-=-=-=-=-=-=-=--=-=-=-=-=-=
unsigned long currentTime[] = {0, 0, 0, 0, 0, 0, 0};
byte GPS = 0, Drop = 1, count = 0;
const int dropPin[] = {20, 14, 15, 16, 17, 18, 19};
byte pos = 1, prevState = 0;
bool flight_begun = false, dropped = false, newData = false, useGPS = true, emergency_Status = false;
unsigned long time_On = 10000, time_Off[] = {0, 20000, 20000, 40000, 20000, 20000, 0}; // edit here for delay times in millis leave the first and last 0s alone
unsigned long overFlowTime = 45*60*1000;
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

  //serial/ ports//////
  Serial.begin(9600);
  ss.begin(9600);
}


void loop() {////main loop=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  while (!flight_begun) { // preflight procedure, only ends once flight has begin
    pre_Flight_System();

    if (GPSdata[Alt] > GPSdata[prevAlt] && gps.altitude.isUpdated()) { // some condition so signal the beginning of the flight
                          // no clue if the .isUpdated() function wroks or if it even does what i think
      count++;
      if (count >= 5) { //if the GPS is reading an increase in altitude for 5 consecutive updates
        flight_begun = true;
      }
    } else if (GPSdata[prevAlt] > GPSdata[Alt] ) { 
      count = 0;
    }
    currentTime[6] = millis(); // used later for the flightTime function
  }

  digitalWrite(13, LOW); // stop blinking as flight has begin
  count = 0; // reset counter for later use

  while (1 == 1) { // primary functionality, once flight begins =-=-=-=-=-=-=-=
    FeedGPS();
    getGPSdata();
    satCheck();
    
    if (FlightTime() - currentTime[GPS] > 5000UL) { // 5 seconds go by with no new GPS data
      GPSFailureCheck();
    }
    if((FlightTime() % 60*1000UL) == 0 && useGPS){// every minute the gps is functioning properly
      for(int i = 0; i <= 5; i++){
        EEPROM.write(i, GPSdata[i]);
      }
    }

    dropDown_System();

    //it should be ok to run this every cycle, or maybe i can just run it once per second untill something happens?
    emergency_System();

  }

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

void satCheck(){ //checks to see how many satalites are present and if the GPS data is reliable
  //Serial.print("4");
  if(GPSdata[numSat] <=3 && gps.satellites.isUpdated()){ // a GPS needs at least 3 satalites to work, but is only accurate with 4 or more
    count++;
  }else if(GPSdata[numSat] >3){
    count = 0;
  }
  if(count >= 5){ // is 5 GPS updates go by and there are less that 4 satalites for each, dont rely on the GPS.
    useGPS = false;
  }
  
}

void GPSFailureCheck() { // check to see if there is a problem with the GPS=-=-=-=-=-=-=-=-
  //Serial.print("5");
  currentTime[GPS] = FlightTime(); //set a counter to measure the 5 seconds with
  useGPS = false; // preemptivly declare the GPS to be not working
  while (FlightTime() - currentTime[GPS] <= 5000UL) { // 5 seconds of continuous checking for new data
    blink(500); //a rapid blinking
    while (ss.available() > 0) { // only true if new data is recieved
      gps.encode(ss.read());
      newData = true;
      useGPS = true; // redeclare the GPS to be working
    }
  }
  digitalWrite(13, LOW);
  currentTime[GPS] = FlightTime();
}

void dropDown_System() { //activate the dropdown system =-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  //Serial.print("6");
  if ((((GPSdata[Alt] >= 15000 && useGPS) || FlightTime() >= 35 * 60 * 1000UL) || emergency_Status) && !dropped ) {
    //for this to work, both the atitude mush be above 15 km and gps working, or flight time exceeds 35 min,
    //and the dropsondes havent been dropped yet, or the emergency override says to go ahead
    if (pos <= 6 && prevState == 0 && FlightTime() >= currentTime[Drop] + time_Off[pos - 1]) {
      //if it was off and the interval between uses is great enough
      digitalWrite(dropPin[pos], HIGH);
      prevState = 1;
      currentTime[Drop] = FlightTime();
    } else if (pos <= 6 && prevState == 1 && FlightTime() >= currentTime[Drop] + time_On) {
      //if it was on and the interval between uses is great enough
      digitalWrite(dropPin[pos], LOW);
      prevState = 0;
      pos++;
      currentTime[Drop] = FlightTime();
    } else if (pos == 7) { // if all 6 dropsondes have been deployed
      //Serial.println("all dropsondes deployed");
      dropped = true;
    }
  }
}

void emergency_System() { // handes various situations classified as emergencies =-=-=-=-=-=-=
  //this can definitly be tidied up quite a bit, i just wanted to see most possibilities laid out
  //one hope of mine, which i havent tested yet, is if this is activated in the middle of the dropdown process, it shoudne actually affect anything.
  //Serial.println("7");
  if (millis() - currentTime[6] >= overFlowTime || emergency_Status) { // only true after 45 min of flight or an emergency is already declared
    if (!useGPS) {
      if (!dropped) {//time limit expired, GPS isnt working, and the Dropsondes havent been deployed yet.
        emergency_Status = true;
        dropDown_System();
      } else {
        //things apear to be working ok...?
        // time is passed, GPS isnt working, but dropsondes have been deployed. maybe activate pin 20?
        //did we ever decide if we wanted a parachute?
      }
    } else {
      if (!dropped) {
        //time is passed. but the dropsondes havent been deployed, even though the GPS is working...
        if (GPSdata[Alt] > GPSdata[prevAlt]) {
          //were still rising, so we can wait a bit
          overFlowTime += 60*1000UL; // adds a minute to the initial if statement
        } else if (GPSdata[Alt] < GPSdata[prevAlt] && GPSdata[Alt] >= 10000) { //were falling too soon but still high enough, go ahead and drop
          emergency_Status = true;
          dropDown_System();
        } else if (GPSdata[Alt] < GPSdata[prevAlt] && GPSdata[Alt] <= 10000) { //were falling too soon, but not high enough to drop
          //idk what to do here? it might not be salvagable, or maybe just wait a bit? 
        }
      } 
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
