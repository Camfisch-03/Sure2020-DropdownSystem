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
bool begun = false, dropped = false, newData = false, useGPS = true, emergency_Status = false;
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
  while (!begun) { // preflight procedure, only ends once flight has begin
    pre_Flight_System();

    if ((GPSdata[Alt] > GPSdata[prevAlt] && gps.altitude.isUpdated()) || GPSdata[Alt] >= 500) { // some condition so signal the beginning of the flight
                          // no clue if the .isUpdated() function wroks or if it even does what i think
                          //im gonna leave this how it is for now, but ill come back and edit how the flight system should begin
      count++;
      if (count >= 5) { //if the GPS is reading an increase in altitude for 5 consecutive updates
        begun = true;
      }
    } else if (GPSdata[prevAlt] > GPSdata[Alt] ) { //if the GPS altitude if falling
      count = 0;
    }
  }
  
  currentTime[6] = millis(); // used for the flightTime function
  digitalWrite(13, LOW); // stop blinking as flight has begin
  count = 0; // reset counter for later use
  begun = false; //reset for later use

  while (1 == 1) { // primary functionality, once flight begins =-=-=-=-=-=-=-=
    FeedGPS();
    getGPSdata();
    
    if (FlightTime() - currentTime[GPS] > 5000UL) { // 5 seconds go by with no new GPS data
      GPSFailureCheck();
    }

    dropDown_System();

    if(FlightTime()%1000UL == 0 || emergency_Status) // runs once a second.
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
    satCheck();
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
    //Serial.println("not enough satalites for accurate positioning");
    useGPS = false;
  }
  
}

void GPSFailureCheck() { // check to see if there is a problem with the GPS=-=-=-=-=-=-=-=-
  //Serial.print("5");
  currentTime[GPS] = FlightTime(); //set a counter to measure the 5 seconds with
  useGPS = false; // preemptivly declare the GPS to be not working
  while (FlightTime() - currentTime[GPS] <= 5000UL && !begun) { // 5 seconds of continuous checking for new data and the dropdown system hasnt stareed yet
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
  if ((((GPSdata[Alt] >= 15000 && useGPS) || (FlightTime() >= 35 * 60 * 1000UL && !useGPS) || (emergency_Status || begun)) && !dropped ) {
    //for this to work a bunch of conditions must be met, either one at a time or together. GPS beats flight time, emergency beats GPS, dropped beats emergency
    if(!begun)
      begun = true; // once its started, it will go all the way through
    if (pos <= 6 && prevState == 0 && FlightTime() >= currentTime[Drop] + time_Off[pos - 1]) { // was off for specified time, now it should be on
      //if it was off and the interval between uses is great enough
      digitalWrite(dropPin[pos], HIGH);
      prevState = 1;
      currentTime[Drop] = FlightTime();
    } else if (pos <= 6 && prevState == 1 && FlightTime() >= currentTime[Drop] + time_On) { // was on, nw it should be off
      //if it was on and the interval between uses is great enough
      digitalWrite(dropPin[pos], LOW);
      prevState = 0;
      pos++;
      currentTime[Drop] = FlightTime();
    } else if (pos == 7) { //all 6 dropsondes have been deployed
      //Serial.println("all dropsondes deployed");
      dropped = true;
      begun = false;
    }
  }
}

void emergency_System() { // handes various situations classified as emergencies =-=-=-=-=-=-=
  //this can definitly be tidied up quite a bit, i just wanted to see most possibilities laid out
  //Serial.println("7");
  if(millis() - currentTime[6] >= overFlowTime || emergencyStsaus){ // if flight time has exceeded the max desired time, or an emergency is already declared
    if(!useGPS && !dropped){ // GPS is givinga bad signal and dropsondes havent been dropped yet. (shouldnt technically reach here, but still)
      emergency_Status = true;
      dropDown_System();
    } else if(useGPS && !dropped){ // GPS is good, but we havent dropped for some reason
      if(GPSdata[Alt] >= GPSdata[prevAlt]){ // were still going up
        overFlowTime += 60*1000UL; // give it some more time
      }else{ // were falling
        if(GPSdata[Alt] >= 10000){ // but were high enough to drop and still get some data
          emergency_Status = true;
          dropDown_System();
        }else{ // were not high enough to even get a decent amount of data
          overFlowTime += 60*1000UL; // hope this is a error and just wait a bit. maybe something will change??
          //this is kinda the worst scinario. 
        }
      }
    }
  }
  if(false){ // i feel like i need a case independent of time if the balloon has dropped too far too fast to just be wind gusts. 
    
  }
  
}


void blink(unsigned int per) {//a visual display that things are working=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  //Serial.println("8");
  if (millis() % per > per / 2)
    digitalWrite(13, HIGH);
  else if (millis() % per <= per / 2)
    digitalWrite(13, LOW);
}
