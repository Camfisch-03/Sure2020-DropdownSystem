////////dropdown system////////
//Cameron Fischer
//v2, started: 7/20/20

//TODO//------------------------------------------------------------------------
//tidy up the emergency system ,as well as fugure out more of what is has to do
//just generally tidy everything up


////Libraries-------------------------------------------------------------------
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

//Declare global Variables------------------------------------------------------
long currentTime[] = {0, 0, 0, 0, 0, 0, 0};
byte GPS = 0, Drop = 1, count = 0, count1 = 0;
const int dropPin[] = {20, 14, 15, 16, 17, 18, 19}, minAlt = 260; //minalt is the altitude the main fligh system starts at
byte pos = 1, prevState = 0;
bool begun = false, dropped = false, newData = false, useGPS = true, emergency_Status = false, pass = false, ended = false;
unsigned long time_On = 10000, time_Off[] = {0, 20000, 20000, 40000, 20000, 20000, 10000}; // edit here for delay times in millis leave the first and last 0s alone
unsigned long maxFlightTme = 45 * 60 * 1000;
float GPSdata[] = {0, 0, 0, 0, 0, 0, 0};
byte Lat = 0, Lon = 1, Alt = 2, prevAlt = 3, numSat = 4, speed = 5;
uint32_t utcTime;
int FTaddr = 2; // occasionally increment this by 3 to avoid dammage to the teensy
float tempAlt = 0;
//unsigned long numPass = 0;


////GPS declaration--------------------------------------------------------------
TinyGPSPlus gps;
SoftwareSerial ss(0, 1); //RX 0, TX 1 (connect GPS TX to teenxy RX and vice versa)


//Setup------------------------------------------------------------------------
void setup() {

  //pin declarations//////
  for (int i = 0; i <= 6; i++) {  //define the pin mode for the nichrome pins
    pinMode(dropPin[i], OUTPUT);
  } //end for
  // pin 0 is only for the emergency drop system. 1-6 are the dropsondes, hence why pos = 1.
  pinMode(13, OUTPUT); //LED on pin 13 for some visual feedback
  pinMode(11, INPUT); //button for me to interact with system

  //serial/ ports//////
  Serial.begin(9600);
  ss.begin(9600);

  //read from memory to see if there was a previous flight.
  //if (EEPROM.read(FTaddr - 1) == 0) { //if previous position is 0 (meaning it has never been declared)
  //this should only ever be true the first time a specific addredd is accessed.
  //   EEPROM.write(FTaddr - 1, 1); //reasign it to 1, because this is the starting value for pos.
  //}//end if
  // if (EEPROM.read(FTaddr) != 0) { //if previous flight time is a value other than 0
  //  currentTime[6] = -1 * (EEPROM.read(FTaddr) * 30000L); // negative to add to millis rather than detract in FlightTime function
  //  begun = true; // skip the preflight system.
  //  pos = EEPROM.read(FTaddr - 1); //assign any pervious value of pos to the current pos
  //}//end if
} // end setup


////main loop, preflight system-------------------------------------------------
void loop() {
  while (!begun) { // preflight procedure, only ends once flight has begin
    FeedGPS();  //recieve any incoming data
    //Serial.print(gps.location.isUpdated());
    if ( digitalRead(11) == HIGH) { //artifically make the system start
      count1 = 30;
      begun = true;
      Serial.println("Begun");
      digitalWrite(20, HIGH);//turn on an LED to signify the beginning
    }
    if ((GPSdata[Alt] > GPSdata[prevAlt] && GPSdata[prevAlt] != tempAlt) || GPSdata[Alt] >= 600) { // some condition so signal the beginning of the flight
      //im gonna leave this how it is for now, but ill come back and edit how the flight system should begin
      tempAlt = GPSdata[prevAlt];
      count1++;
      Serial.print(count1); Serial.println(" / 30");
      if (count1 >= 30) { //if the GPS is reading an increase in altitude for 15 consecutive updates, or practically instantly if the alt is above 500 m
        //15 should be too many to simply be a mistake or error on the GPS's side, if its being heald steady.
        begun = true;
        Serial.println("Begun");
        digitalWrite(20, HIGH);//turn on an LED to signify the beginning
      }//end if

    } //end if

    else if (GPSdata[prevAlt] > GPSdata[Alt] && GPSdata[prevAlt] != tempAlt && GPSdata[Alt] < 600 && digitalRead(11) == LOW) { //if the GPS altitude if falling or remaining stagnant.
      tempAlt = GPSdata[prevAlt];
      count1 = 0;
      Serial.print(count1); Serial.println(" / 30");

    } // end elseif

    getGPSdata();  //parse the data into usable chunks
    //this has to be here so isUpdated() shows true when there is an update. has to do with when i call a field.
    blink(2000); //blink at a rate of 2 seconds to signify System is working and isnt stuck.
  }// end while


  //

  delay(1000);
  if (count1 >= 1) // only set right after preflight system runs, otherwise set during setup (count is only incremented in preflight)
    currentTime[6] = millis(); // used for the FlightTime function
  digitalWrite(13, LOW); // stop blinking as flight has begin
  digitalWrite(20, LOW); // turn the LED off
  count = 0; // reset counter for later use
  begun = false; //reset for later use
  currentTime[GPS] = FlightTime(); // reassign GPS time for easier system integration
  //numPass = 0;


  // primary functionality, once flight begins -----------------------------------
  while (1 == 1) {

    if (FlightTime() - currentTime[GPS] > 5000UL) { // 5 seconds go by with no new GPS data. i could also say gps.location.age() >= 5000UL
      GPSFailureCheck();
    }// end if

    FeedGPS();
    getGPSdata();

    dropDown_System(); // this should be good to run every time.

    if (FlightTime() % 1000UL == 0 || emergency_Status) // runs 1/1000 as often as dropdown_System and feedGPS
      emergency_System();

    // if (FlightTime() % 30000UL == 29999UL && !pass && FlightTime() <= 2 * 60 * 60 * 1000UL) { // runs once every 30 seconds up till 2 hours of flight have elapsed
    //saveData(FTaddr);
    //pass = true;
    //} //end if

    //else if ( FlightTime() % 60000UL == 0) { // should make the loop above run once and only once per time interval.
    //pass = false;

    //}//end elseif
    //Serial.print("Time: "); Serial.print(millis()); Serial.print(" pass number: "); Serial.println(numPass);
    //numPass++;
  } //end while

}//end loop


// returns the flight time in millis-----------------------------------------
unsigned long FlightTime() {
  unsigned long t = millis() - currentTime[6];
  return t;
}//end FlightTime


// see if the GPS has any new data --------------------------------------------
void FeedGPS() {
  //Serial.print("2");
  while (ss.available() > 0) {  //while there is new data
    gps.encode(ss.read());  //send that new data to the GPS encoder to be read in getGPSdata()
    newData = true; //there is new data
    //useGPS = true; // the GPS is working properly, but here isnt the place to be setting/resetting this.
  }//end while

}//end FeedGPS


// parse any new data the GPS has -------------------------------------------
void getGPSdata() {
  //Serial.print("3");
  if (gps.location.isUpdated() && gps.altitude.isUpdated()) { // has the location been updated
    satCheck(); // check the ststus of satalites
    GPSdata[Lat] = gps.location.lat(); //update latitude
    Serial.print("\nLAT = "); Serial.println(GPSdata[Lat], 6);
    GPSdata[Lon] = gps.location.lng(); //update longitude
    Serial.print("LNG = "); Serial.println(GPSdata[Lon], 6);
    GPSdata[prevAlt] = GPSdata[Alt]; // used for change in altitude
    GPSdata[Alt] = gps.altitude.meters(); // update altitude
    Serial.print("ALT = "); Serial.println(GPSdata[Alt]);
    GPSdata[numSat] = gps.satellites.value();  //update number of satalites used for calculations
    Serial.print("Num  Sat =  "); Serial.println(GPSdata[numSat]);
    utcTime = gps.time.value(); //update global standard time
    Serial.print("UTC time =  "); Serial.println(utcTime);
    GPSdata[speed] = gps.speed.mps();
    Serial.print("Speed =   "); Serial.print(GPSdata[speed]); Serial.println(" m/s");
    //Serial.println(" ");
    newData = false; // all new data has been parsed
    currentTime[GPS] = FlightTime();
    if (count1 >= 30){ //ronly runs after the preflight system is over. 
      Serial.print("Flight Time =   "); Serial.println(FlightTime());
    }//end if
  }// end if
}// end getGPSdata


//checks to see how many satalites are present----------------------------------
void satCheck() {
  //Serial.print("4");
  Serial.print(GPSdata[numSat]);
  Serial.print(" satalites in view, ");
  if (GPSdata[numSat] <= 3 && gps.satellites.isUpdated()) { // a GPS needs at least 3 satalites to work, but is only accurate with 4 or more
    count++;  //increment the number of bad checks
    Serial.println("bad");
  } //end if
  else if (GPSdata[numSat] > 3) { // if there are more then 3 satalites in the check
    count = 0;  //reset the count
    Serial.println("Good");
    useGPS = true; // thigs are going smoothly, so use the GPS.
  }//end elseif
  if (count >= 5) { // if 5 GPS updates go by and there are less that 4 satalites for each, dont rely on the GPS.
    //Serial.println("not enough satalites for accurate positioning");
    useGPS = false; // for now, don't rely on the GPS for positioning untill the system resets.
    Serial.println("Dont rely on  GPS");
  }//end if

}//end satCheck


// check to see if there is a problem with the GPS------------------------------
void GPSFailureCheck() {
  //Serial.print("5");
  currentTime[GPS] = FlightTime(); //set a counter to measure the 5 seconds with
  useGPS = false; // preemptivly declare the GPS to be not working
  while (FlightTime() - currentTime[GPS] <= 5000UL && !begun) { // 5 seconds of continuous checking for new data and the dropdown system hasnt started yet (or ended)
    blink(500); //a fairly rapid blinking so signify an error
    while (ss.available() > 0) { // only true if new data is recieved
      gps.encode(ss.read());
      newData = true;
      //useGPS = true; // redeclare the GPS to be working
    }//end while
  }//end while
  digitalWrite(13, LOW); // to stop the blinking
  currentTime[GPS] = FlightTime();
}//end GPSFailureCheck


//activate the dropdown system ------------------------------------------------
void dropDown_System() {
  //Serial.print("6");
  if (((GPSdata[Alt] >= minAlt && useGPS) || (FlightTime() >= 35 * 60 * 1000UL && !useGPS) || (emergency_Status || begun)) && !dropped ) {
    //for this to work a bunch of conditions must be met, either one at a time or together. GPS beats flight time, emergency beats GPS, dropped beats emergency
    if (!begun)
      begun = true; // once its started, it will go all the way through without interuption
    if (pos <= 6 && prevState == 0 && FlightTime() >= currentTime[Drop] + time_Off[pos - 1]) { // was off for specified time, now it should be on
      //if it was off and the interval between uses is great enough
      digitalWrite(dropPin[pos], HIGH);
      prevState = 1;
      currentTime[Drop] = FlightTime();
    } //end if
    else if (pos <= 6 && prevState == 1 && FlightTime() >= currentTime[Drop] + time_On) { // was on, now it should be off
      //if it was on and the interval between uses is great enough
      digitalWrite(dropPin[pos], LOW);
      prevState = 0;
      pos++;
      currentTime[Drop] = FlightTime();
    } //end elseif
    else if (pos == 7 && FlightTime() >= currentTime[Drop] + time_Off[pos - 1]) { //all 6 dropsondes have been deployed
      //Serial.println("all dropsondes deployed");
      dropped = true; // the moment this becomes true, the dropdown_system cannot run again.
      begun = false; // reset for use in other systems
      ECutDown(); // activate cutdown system to end the flight here
    }//end elseif
  }//end if
}//end dropDown_System



// handes various situations classified as emergencies ------------------------
void emergency_System() {
  //this can definitly be tidied up quite a bit, i just wanted to see most possibilities laid out
  //Serial.println("7");

  if (millis() - currentTime[6] >= maxFlightTme || emergency_Status) { // if flight time has exceeded the max desired time, or an emergency is already declared
    if (!useGPS && !dropped) { // GPS is giving a bad signal and dropsondes havent been dropped yet. (shouldnt technically reach here, but still)
      emergency_Status = true;
      dropDown_System();
      ECutDown();
    } //end if
    else if (useGPS && !dropped) { // GPS is good, but we havent dropped for some reason
      if (GPSdata[Alt] >= GPSdata[prevAlt]) { // we're still going up
        maxFlightTme += 30 * 1000UL; // give it some more time (30 seconds)
      } //end if
      else { // were falling
        if (GPSdata[Alt] >= 10000) { // but were high enough to drop and still get some data. *height subject to change.*
          emergency_Status = true; // there is a viable emergency
          dropDown_System();  //start the dropdown system
          ECutDown();//initiate the emergency cutdown to terminate flight. after all dropsondes have been released.
        }   //end if
        else { // were not high enough to even get a decent amount of data
          maxFlightTme += 60 * 1000UL; // hope this is a mistake and just wait a minute. maybe something will change??
          //this is kind of the worst scinario.
        }//end else
      }//end else
    }//end elseif
  }//end if
  if (false) { // i feel like i need a case independent of time if the balloon has dropped too far too fast to just be wind gusts or a data error

  }
  if (false) { // maybe something like the lateral movement is way too high. something like "if the payload got hit by a plane"
    //maybe use pin 20 for emergency cut down here?
  }

}// end emergency_System


//a visual display that things are working-------------------------------------
void blink(unsigned int per) {
  //Serial.println("8");
  if (millis() % per >= per / 2)
    digitalWrite(13, HIGH);
  else if (millis() % per < per / 2)
    digitalWrite(13, LOW);
}//end blink


// save some data for possible later use--------------------------------------
void saveData(int adr) {

  //EEPROM.write(adr, ((int)(FlightTime() / 30000UL))); // divide flight time by 30000 because EEPROM can only store 1 byte. this means add 1 every 30 seconds from the beginning of the flight.
  //EEPROM.write(adr - 1, pos); // should usually be a 1 unless the dropdown system has started.

}//end saveData


//handles the emercency cut down system.--------------------------------------
void ECutDown() {
  if (pos == 7 && !ended) { // if all of the dropsondes have been deployed. activate the emergency cut down system. make sure the fight ends now
    digitalWrite(dropPin[0], HIGH); // turn pin 20 on (emergency sut down system)
    Serial.println("\nThe Beginning of the End");
    delay(15000); //wait 15 seconds
    digitalWrite(dropPin[0], LOW);  // turn pin 20 off
    ended = true; //end this system
  }//end if
}//end ECutDown
