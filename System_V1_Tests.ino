////////dropdown system////////
//Cameron Fischer
//v2, started: 7/20/20

//TODO//----------------------------------------------------------------------------------------------
//work more on the emergency system and all of its functions
//got memory function working
//fully implemet the flight termination system


////Libraries-----------------------------------------------------------------------------------------
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

//Declare global Variables----------------------------------------------------------------------------
long currentTime[] = {0, 0, 0, 0, 0, 0, 0}; //a few time variables
byte GPS = 0, Drop = 1, count = 0, count1 = 0;  //name for time variables
const int dropPin[] = {20, 14, 15, 16, 17, 18, 19}; //which pins are attached to the nichrome system
const int minAlt = 260; //minalt is the altitude the main fligh system starts at
byte pos = 1, prevState = 0;
bool begun = false, dropped = false, newData = false, useGPS = true, emergency_Status = false, pass = false, ended = false;
unsigned long time_On = 10000, time_Off[] = {0, 20000, 20000, 40000, 20000, 20000, 40000}; // edit here for delay times in millis leave the first 0 alone.
unsigned long maxFlightTime = 45 * 60 * 1000;  //flight time before emergency system runs
float GPSdata[] = {0, 0, 0, 0, 0, 0, 0};  //various GPS data fields
byte Lat = 0, Lon = 1, Alt = 2, prevAlt = 3, numSat = 4, speed = 5; //name of GPS data fields
uint32_t utcTime; //GPS time variable
int FTaddr = 11; // occasionally increment this by 3 to avoid dammage to the teensy
float tempAlt = 0;  //a temporaty alt for comparison
//unsigned long numPass = 0;


////GPS declaratio-----------------------------------------------------------------------------------
TinyGPSPlus gps;  //GPS is named gps
SoftwareSerial ss(0, 1); //RX 0, TX 1 (attach GPS TX to teensy RX and vice versa)

//Setup----------------------------------------------------------------------------------------------
void setup() {

  //serial/ ports//////
  Serial.begin(9600);
  ss.begin(9600);

  //pin declarations//////
  for (int i = 0; i <= 6; i++) {  //cycle through pin array
    pinMode(dropPin[i], OUTPUT);  //set pins to output
  } //end for
  // pin 0 is only for the emergency drop system. 1-6 are the dropsondes, hence why pos = 1.
  pinMode(13, OUTPUT); //LED on pin 13 for some visual feedback
  pinMode(11, INPUT); //for interactions with the system.


  if (EEPROM.read(FTaddr - 1) == 0) { //if previous position is 0 (meaning it has never been declared)
    EEPROM.write(FTaddr - 1, pos); //reasign it to 1, because this is the starting value for pos.
    Serial.println("Set initial pos to memory");
  }//end if

  if (EEPROM.read(FTaddr) > 0) { //if previous flight time is a value greater than 0
    currentTime[6] = -1 * (EEPROM.read(FTaddr) * 30000L); // negative to add to millis rather than detract
    begun = true; // skip the preflight system.
    pos = EEPROM.read(FTaddr - 1); //assign any pervious value of pos to the current pos
  }//end if
} // end setup


//main loop/ preflight-------------------------------------------------------------------------------
void loop() {
  delay(500); //idk
  while (!begun) { // preflight procedure, only ends once flight has begin
    FeedGPS();//recieve any incoming data

    if ((GPSdata[Alt] > GPSdata[prevAlt] && GPSdata[prevAlt] != tempAlt) || GPSdata[Alt] >= minAlt) { //if the GPS is ascending, or above minAlt
      tempAlt = GPSdata[prevAlt]; //assign previous alt to temp alt for comparison, to make the above statement only work once per update
      count1++; //increment count1
      Serial.println(count1);
      if (count1 >= 100) { //if the GPS is reading an increase in altitude for 30 consecutive updates, or practically instantly if the alt is above minAlt
        begun = true;//start the main flight system

      }//end if
    } //end if
    else if (GPSdata[prevAlt] > GPSdata[Alt] && GPSdata[prevAlt] != tempAlt && GPSdata[Alt] < minAlt) { //if GPS is falling
      tempAlt = GPSdata[prevAlt];
      count1 = 0; //reasign count1 back to 0 and restart the count
      Serial.println(count1);
    } // end elseif
    if (digitalRead(11) == HIGH) {
      count1 = 30;
      begun = true;
      digitalWrite(20, HIGH);
    }
    getGPSdata();  //parse the data into usable chunks
    blink(2000); //blink at a rate of 2 seconds to signify were working

  }// end while(!begun)


  if (count1 >= 1) {// only set right after preflight system runs, otherwise set during setup
    delay(750);  //long enough to see the LED on and/or release the button
    currentTime[6] = millis(); // used for the FlightTime function
    begun = false; //reset for later use
  } else {
    Serial.println("loading previous flight time");
    Serial.print(-1 * currentTime[6]); Serial.println(" into the flight");
    Serial.println("Skipped preFlight");
    begun = EEPROM.read(FTaddr - 2);
  }


  digitalWrite(13, LOW); // stop blinking as flight has begin
  digitalWrite(20, LOW);
  currentTime[GPS] = FlightTime(); // reset GPS time for continuity's sake. and GPSFailureCheck
  Serial.println("Flight Begun");


  // primary functionality, once flight begins -------------------------------------------------------
  while (1 == 1) {
    FeedGPS();  //look for incoming info from the GPS

    if (FlightTime() - currentTime[GPS] > 5000UL) { // 5 seconds go by with no new GPS data
      GPSFailureCheck();  //check to see if i can fix the issue
    }// end if

    dropDown_System(); //check to see if dropsondes can be released
    //should be good ro run everytime

    if (FlightTime() % 1000UL == 0 || emergency_Status) // runs 1/1000 as often as dropdown_System and feedGPS
      emergency_System(); //checks for emergencies, and if it can handle them

    getGPSdata(); //parse data into usable chunks

    saveData(FTaddr); //save flighttime and pos to memory

  }//ens while(1==1)

}//end loop


// returns the flight time in millis------------------------------------------------------------------
unsigned long FlightTime() {
  unsigned long t = millis() - currentTime[6];  //total time - time of preflight system
  return t; //returns the time since start of the flight
}//end FlightTime


// see if the GPS has any new data--------------------------------------------------------------------
void FeedGPS() {
  while (ss.available() > 0) {  //while there is new data
    gps.encode(ss.read());  //send that new data to the GPS encoder to be read in getGPSdata()
    newData = true; //there is new data (unused var)

  }//end while
  if (gps.altitude.age() >= 5000UL && gps.location.age() >= 5000UL) { //if neither altitude or location have been updated within 5 seconds
    useGPS = false; //dont use GPS for dropdown process
    GPSFailureCheck();  //send system to try and amend GPS issue
  }

}//end FeedGPS


// parse any new data the GPS has--------------------------------------------------------------------
void getGPSdata() {
  if (gps.location.isUpdated() && gps.altitude.isUpdated()) { // if the location and altitude have been updated
    satCheck(); // check the ststus of satalites
    GPSdata[Lat] = gps.location.lat(); //update latitude
    Serial.print("Lat =   "); Serial.println(GPSdata[Lat], 6);
    GPSdata[Lon] = gps.location.lng(); //update longitude
    Serial.print("Lon =   "); Serial.println(GPSdata[Lon], 6);
    GPSdata[prevAlt] = GPSdata[Alt]; // used for change in altitude
    GPSdata[Alt] = gps.altitude.meters(); // update altitude
    Serial.print("Alt =   "); Serial.println(GPSdata[Alt]);
    GPSdata[numSat] = gps.satellites.value();  //update number of satalites used for calculations
    utcTime = gps.time.value(); //update global standard time
    Serial.print("UTC Time =   "); Serial.println(utcTime);
    GPSdata[speed] = gps.speed.mps(); //update the lateral speed
    Serial.print("speed =   "); Serial.print(GPSdata[speed]); Serial.println(" m/s\n");
    currentTime[GPS] = FlightTime();  //assign FlightTime to GPS time variable for comparisons.
    newData = false; // all new data has been parsed (unused var)
  }// end if
}// end getGPSdata


//checks to see how many satalites are present-------------------------------------------------------
//also checks the age and validity of location + altitude
void satCheck() {
  Serial.print(GPSdata[numSat]); Serial.print(" Satalites in view, ");
  if (GPSdata[numSat] <= 3 && gps.satellites.isUpdated()) { // a GPS can work with 3 satalites, but is only accurate with 4 or more
    Serial.print("Bad ");
    count++;  //increment the number of times a bad check was made
    Serial.println(count);
  } //end if
  else if (GPSdata[numSat] > 3) { // if there are more then 3 satalites in the check
    Serial.print("Good");
    count = 0;  //reset the count
    useGPS = true; // thigs are going smoothly, so use the GPS.
  }//end elseif
  if (count >= 5) { // if 5 GPS updates go by and there are less that 4 satalites for each, dont rely on the GPS.
    useGPS = false; // for now, don't rely on the GPS for positioning
  }//end if
  Serial.println("");

}//end satCheck


// check to see if there is a problem with the GPS----------------------------------------------------
void GPSFailureCheck() {

  currentTime[GPS] = FlightTime(); //set a counter to measure the 5 seconds with
  useGPS = false; // preemptivly declare the GPS to be not working
  while (FlightTime() - currentTime[GPS] <= 5000UL && !begun) { // 5 seconds of continuous checking for new data and the dropdown system hasnt started yet (or ended)
    blink(500); //a fairly rapid blinking so signify an error
    while (ss.available() > 0) { // only true if new data is recieved
      gps.encode(ss.read());  //send data to encoder to be parsed later
      newData = true; //(unused var)
    }//end while
  }//end while
  digitalWrite(13, LOW); // to stop the blinking
  currentTime[GPS] = FlightTime();
}//end GPSFailureCheck


//activate the dropdown system----------------------------------------------------------------------
void dropDown_System() {

  if (((GPSdata[Alt] >= 15000 && useGPS && gps.altitude.isValid()) || (FlightTime() >= 35 * 60 * 1000UL && !useGPS) || (emergency_Status || begun || digitalRead(11) == HIGH)) && !dropped ) {
    //for this to work a bunch of conditions must be met, either one at a time or together. GPS beats flight time, emergency beats GPS, dropped beats emergency
    if (!begun) //if this is the first time getting into this system
      begun = true; // once its started, it will go all the way through without interuption

    if (pos <= 6 && prevState == 0 && FlightTime() >= currentTime[Drop] + time_Off[pos - 1]) { // was off for specified time, now it should be on
      //if it was off and the interval between uses is great enough
      digitalWrite(dropPin[pos], HIGH);
      if (pos == 1) { // if this is the first time through
        EEPROM.write(FTaddr - 2, begun); //save the value of begun
      }//end if
      Serial.print("Dropping "); Serial.println(pos);
      prevState = 1;  //system is currently on
      currentTime[Drop] = FlightTime(); //reasign drop time for comparisons

    } //end if

    else if (pos <= 6 && prevState == 1 && FlightTime() >= currentTime[Drop] + time_On) { // was on, now it should be off
      //if it was on and the interval between uses is great enough
      digitalWrite(dropPin[pos], LOW);
      prevState = 0;  //system is now off
      pos++;
      currentTime[Drop] = FlightTime();
    } //end elseif

    else if (pos == 7 && FlightTime() >= currentTime[Drop] + time_Off[pos - 1]) { //all 6 dropsondes have been deployed
      dropped = true; // the moment this becomes true, the dropdown_system cannot run again.
      begun = false; // reset for use in other systems
      ECutDown(); // activate cutdown system to end the flight here
      EEPROM.write(FTaddr - 2, begun); //save the value of begun

    }//end elseif

  }//end if

}//end dropDown_System


// handes various situations classified as emergencies-----------------------------------------------
void emergency_System() {

  if (millis() - currentTime[6] >= maxFlightTime) { // if flight time has exceeded the max desired time
    if (!useGPS && !dropped) { // GPS is giving a bad signal and dropsondes havent been dropped yet. (shouldnt technically reach here, but still)
      emergency_Status = true;  //declare an emergency
      dropDown_System();  //send system to start dropsown function
    } //end if
    else if (useGPS && !dropped) { // GPS is good, but we havent dropped for some reason
      if (GPSdata[Alt] >= GPSdata[prevAlt] && gps.altitude.isValid()) { // we're still going up
        maxFlightTime += 30 * 1000UL; // give it some more time (30 seconds)
      } //end if
      else { // were falling
        if (GPSdata[Alt] >= 10000 && gps.altitude.isValid()) { // but were high enough to drop and still get some data. *height subject to change.*
          emergency_Status = true; // there is a viable emergency
          dropDown_System();  //start the dropdown system
        }   //end if
        else if (GPSdata[Alt] < 10000 && millis() - currentTime[6] >= maxFlightTime) { // were not high enough to even get a decent amount of data
          maxFlightTime += 30 * 1000UL; // hope this is a mistake and just wait a minute. maybe something will change??
          //this is kind of the worst scinario.
        }//end else if

      }//end else

    }//end elseif

  }//end if

  if (GPSdata[speed] >= 100 && gps.speed.isValid()) { //if lateral speed is greater than 100 meters/second..? something like that?
    if (false) { // some other check to make
      ECutDown(); //stop the flight here?
    }
  }//end if

  if (false) { // if fell like i still need something else to go here, independent of flight time.
    //maybe if its simply falling too fast?
  }//end if

}// end emergency_System


//a visual display that things are working-----------------------------------------------------------
void blink(unsigned int per) {  //per is the period of blinking

  if (millis() % per >= per / 2)  //on for the first half of the period
    digitalWrite(13, HIGH);
  else if (millis() % per < per / 2)  //off for the second half of the period
    digitalWrite(13, LOW);

}//end blink


// save some data for possible later use-------------------------------------------------------------
void saveData(int adr) {  //takes in an address from global variables
  if (FlightTime() % 30000UL == 0UL && !pass && FlightTime() <= 2 * 60 * 60 * 1000UL) { // runs once every 30 seconds up till 2 hours of flight have elapsed
    EEPROM.write(adr, (byte)((FlightTime() / 30000UL))); // divide flight time by 30000 because EEPROM can only store 1 byte. this means add 1 every 30 seconds from the beginning of the flight.
    Serial.print("Flight time written to memory: "); Serial.println(FlightTime());
    pass = true;  //makes this run only once per time cycle
  } //end if
  else if ( FlightTime() % 30000UL == 1) { // if time is an integer multiple of 30 seconds
    pass = false; //reset for use in previous if

  }//end elseif

  if (EEPROM.read(adr - 1) != pos) { //if the stored value of pos is not equal to the current value of pos.
    EEPROM.write(adr - 1, pos); // should usually be a 1 unless the dropdown system has started.
    Serial.print("Pos written to memory: "); Serial.println(pos);
  }

}//end saveData


//handles the Flight Termination system--------------------------------------------------------------
void ECutDown() {
  if ((pos == 7 || false) && !ended) { // if all of the dropsondes have been deployed. activate the emergency cut down system. make sure the fight ends now
    //change that false to something that will make this system run under certain conditions without pos == 7 being true
    Serial.println("The beginning of the End");
    digitalWrite(dropPin[0], HIGH); // turn pin 20 on (emergency sut down system)
    delay(15000); //wait 15 seconds
    digitalWrite(dropPin[0], LOW);  // turn pin 20 off
    ended = true; //end this system
    pos = 8;
  }//end if
}//end ECutDown
