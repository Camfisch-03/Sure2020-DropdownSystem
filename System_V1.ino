////////dropdown system////////
//Cameron Fischer
//v2, started: 7/20/20

//TODO//----------------------------------------------------------------------------------------------
// work more on the emergency system and all of its functions
// fully implement the flight termination system


////Libraries and declarations------------------------------------------------------------------------
#include <TinyGPS++.h>  // for parsing data from the GPS
#include <EEPROM.h> // for writing data to non-volatile memory

#define HWS Serial1 // Hardware Serial port 1 (pin 0 for RX and 1 for TX)
#define LED 13  // LED for visual feedback
#define minAlt 500  // altitude main flight starts at
#define maxAlt 15000  // altitude dropdown system starts at

//Declare global Variables----------------------------------------------------------------------------
long currentTime[] = {0, 0, 0, 0, 0, 0, 0}; // Time stamps for:
const byte GPS = 0, Drop = 1, preflight = 6;  // GPS systems, Dropdown system, FlightTime
float GPSdata[] = {0, 0, 0, 0, 0, 0, 0};  //various GPS data fields
const byte Lat = 0, Lon = 1, Alt = 2, prevAlt = 3, numSat = 4, speed = 5; //latitude, longitude, altitude, previous altitude, number of satalites
uint32_t utcTime; // coordinated universal time
int dropPin[] = {20, 14, 15, 16, 17, 18, 19}; // which pins are attached to the nichrome system
byte pos = 1, prevState = 0;  // dropdown system place markers
unsigned long time_On = 10000, time_Off[] = {0, 20000, 20000, 40000, 20000, 20000, 40000}; // dropdown delay times
int  bad_sat_count = 0, ascent_count = 0; // count for satCheck, count for ascending updates
bool begun_flight = false, begun_drop, dropped = false; // has the main flight started? has the dropdown system started?
bool useGPS = true, emergency_Status = false, pass = false, ended = false;  //pass is for saveData, ended is for ECutDown
unsigned long maxFlightTime = 2700000, dropFlightTime = 2100000;  //max is for emergency stsyem start, drop is sropdown system start
int DataAddr = 11; // address to store data, occasionally increment this by 3 to avoid damage to the teensy
// address is for FlighTtime, address - 1 is for pos, address - 2 is for begun_drop


////GPS declaration-----------------------------------------------------------------------------------
TinyGPSPlus gps;  // GPS is named gps

//Setup----------------------------------------------------------------------------------------------
void setup() 
{
  // declare serial ports baud rate
  Serial.begin(9600);
  HWS.begin(9600);

  // pin declarations
  for (unsigned int i = 0; i < (sizeof(dropPin) / sizeof(dropPin[0])); i++) //cycle through pin array
  { 
    pinMode(dropPin[i], OUTPUT);  //set pins to output
  } //end for
  // pin 0 is only for the flight termination system. 1-6 are the dropsondes, hence why pos starts at 1.
  pinMode(LED, OUTPUT); //LED on pin 13 for some visual feedback

  // memory checks from previous flight data
  if (EEPROM.read(DataAddr - 1) == 0)  //if this address has never been used before
  {
    EEPROM.write(DataAddr - 1, pos); //reassign it to the starting value for pos
  }//end if

  if (EEPROM.read(DataAddr) > 0)  //if a previous flight time has been saved
  {
    currentTime[preflight] = -1 * (EEPROM.read(DataAddr) * 30000L); // negative to add to millis in FlightTime()
    begun_flight = true; // skip the preflight system.
    pos = EEPROM.read(DataAddr - 1); //assign the previous value of pos to the current pos
  }//end if
} // end setup


//main loop/ preflight-------------------------------------------------------------------------------
void loop() {
  // perflight procedure
  while (!begun_flight) 
  { 
    //receive any incoming data
    FeedGPS(); 

    // check to see if main flight should start
    if ((GPSdata[Alt] > GPSdata[prevAlt] && gps.altitude.isUpdated() && GPSdata[numSat] > 3) || GPSdata[Alt] >= minAlt) 
    { //if the GPS is ascending, or above minAlt
      ascent_count++; //number of ascending Alt updates
      if (ascent_count >= 45)  //balloon was ascedding for 45 updates in a row. instantly if Alt is above minAlt.
      {
        begun_flight = true; //end preflight system and begin main flight
      }//end if
    } //end if
    else if (GPSdata[prevAlt] > GPSdata[Alt] && gps.altitude.isUpdated() && GPSdata[Alt] < minAlt) //the GPS is falling
    { 
      ascent_count = 0; //reassign ascent_count back to 0 and restart the count
    } // end elseif

    // parse GPS data
    getGPSdata(); 
    
    blink(2000); //blink at a rate of 2 seconds to signify were working
  }// end while(!begun_flight)

  // mainflight setup
  if (ascent_count >= 1) // if preflight system wasnt skipped over
  {
    currentTime[preflight] = millis(); // used for the FlightTime calculation
  } //end if
  else 
  {
    begun_drop = EEPROM.read(DataAddr - 2); //assign the previous value of begun_drop to the current variable
  }//end else
  
  digitalWrite(13, LOW); // stop blinking as flight has begin
  currentTime[GPS] = FlightTime(); // reset GPS time for continuity's sake. and GPSFailureCheck


  // primary functionality, once flight begins -------------------------------------------------------
  while (1 == 1) 
  {
    //look for incoming info from the GPS
    FeedGPS();  

    //check to see if dropsondes can be released
    dropDown_System(); 

    // runs 4 times a second (ideally)
    if (FlightTime() % 250UL == 0 || emergency_Status) 
    {
      //checks for emergencies, and if it can handle them
      emergency_System(); 
    }

    //parse data into usable chunks
    getGPSdata(); 

    //save flight time and pos to memory
    saveData(DataAddr); 
  }//ens while(1==1)

}//end loop


// returns the flight time in millis------------------------------------------------------------------
unsigned long FlightTime() 
{
  unsigned long t = millis() - currentTime[preflight];  //total time - time of preflight system
  return t; //returns the time since start of the flight
}//end FlightTime


// see if the GPS has any new data--------------------------------------------------------------------
void FeedGPS() 
{
  while (HWS.available() > 0) 
  {  //while there is new data
    gps.encode(HWS.read());  //send that new data to the GPS encoder to be read in getGPSdata()

  }//end while

  //neither location nor altitude have been updated in 5 sec
  if (gps.altitude.age() >= 5000UL && gps.location.age() >= 5000UL) 
  { 
    useGPS = false; //don't use GPS for dropdown process
    GPSFailureCheck();  //send system to try and amend GPS issue
  }

}//end FeedGPS


// parse any new data the GPS has--------------------------------------------------------------------
void getGPSdata() 
{
  // if the location and altitude have been updated
  if (gps.location.isUpdated() && gps.altitude.isUpdated()) 
  { 
    satCheck(); // check the status of satellites
    GPSdata[Lat] = gps.location.lat(); //update latitude
    GPSdata[Lon] = gps.location.lng(); //update longitude
    GPSdata[prevAlt] = GPSdata[Alt]; // used for change in altitude
    GPSdata[Alt] = gps.altitude.meters(); // update altitude
    GPSdata[numSat] = gps.satellites.value();  //update number of satellites used for calculations
    utcTime = gps.time.value(); //update global standard time
    GPSdata[speed] = gps.speed.mps(); //update the lateral speed
    currentTime[GPS] = FlightTime();  //assign FlightTime to GPS time variable for comparisons.
  }// end if
}// end getGPSdata


//checks to see how many satellites are present-------------------------------------------------------
void satCheck() 
{
  if (GPSdata[numSat] <= 3 && gps.satellites.isUpdated()) // GPS needs at least 4 satalites
  { 
    bad_sat_count++;  //increment the number of times a bad check was made
  } //end if
  else if (GPSdata[numSat] > 3) // if there are more than 3 satellites in the check
  { 
    bad_sat_count = 0;  //reset the bad_sat_count
    useGPS = true; // things are going smoothly, so use the GPS.
  }//end elseif
  if (bad_sat_count >= 5) // 5 updates with less that 4 sats
  { 
    useGPS = false; // don't rely on GPS for now
  }//end if

}//end satCheck


// check to see if there is a problem with the GPS----------------------------------------------------
void GPSFailureCheck() 
{
  currentTime[GPS] = FlightTime(); // set a counter to measure change in time
  useGPS = false; // preemptively declare GPS to be not working
  
  // a few seconds of searching, only if dropdown system isnt running
  while (FlightTime() - currentTime[GPS] <= 5000UL && !begun_drop) 
  { 
    ascent_count = 0; // for use in prefight system as another point to check
    blink(500); // fairly rapid blinking so signify an error
    while (HWS.available() > 0) // if there is new data
    { 
      gps.encode(HWS.read());  //send data to encoder to be parsed
      break; //not concerned with recieving all data here
    }//end while
  }//end while
  digitalWrite(13, LOW); // stop the blinking
  currentTime[GPS] = FlightTime();  // reset GPS time stamp
}//end GPSFailureCheck


//activate the dropdown system----------------------------------------------------------------------
void dropDown_System() 
{
  if (((GPSdata[Alt] >= maxAlt && useGPS && gps.altitude.isValid()) || (FlightTime() >= 35 * 60 * 1000UL && !useGPS) || (emergency_Status || begun_drop)) && !dropped ) 
  { // for this to work a bunch of conditions must be met, either one at a time or together. GPS beats flight time, emergency beats GPS, dropped beats emergency
    if (!begun_drop) // if this is the first time getting into this system
    {
      begun_drop = true; // once its started, it will go all the way through without interruption
    }
    
    // was off for specified time, now it should be on
    if (pos <= 6 && prevState == 0 && FlightTime() >= currentTime[Drop] + time_Off[pos - 1]) 
    { 
      digitalWrite(dropPin[pos], HIGH);
      if (pos == 1) 
      {
        EEPROM.write(DataAddr - 2, begun_drop); // assign current statud of dropdown system to memory
      }
      prevState = 1;  // system is currently on
      currentTime[Drop] = FlightTime(); // reassign drop time for comparisons
    } //end if

    // was on for a certain amount of time, now it should be off
    else if (pos <= 6 && prevState == 1 && FlightTime() >= currentTime[Drop] + time_On) 
    { 
      digitalWrite(dropPin[pos], LOW);
      prevState = 0;  //system is now off
      pos++;  //go to e next dropsonde
      currentTime[Drop] = FlightTime(); // reassign drop time for comparisons
    } //end elseif

    // all 6 dropsondes have been deployed
    else if (pos == 7 && FlightTime() >= currentTime[Drop] + time_Off[pos - 1]) 
    { 
      dropped = true; // the moment this becomes true, the dropdown_system cannot run again.
      begun_drop = false; // reset for use in other systems
      ECutDown(); // activate cutdown system to end the flight here
      EEPROM.write(DataAddr - 2, begun_drop); //assign current statud of dropdown system to memory

    }// end elseif

  }// end if

}// end dropDown_System


// handes various situations classified as emergencies-----------------------------------------------
void emergency_System() 
{
  if (millis() - currentTime[preflight] >= maxFlightTime) // flighttime has exceeded maxflighttime, or an emergency is declared
  { 
    if (!useGPS && !dropped) // GPS is giving bad signal, dropsondes haven't been dropped yet
    { 
      emergency_Status = true;  //declare an emergency
      dropDown_System();  //send system to start dropdown function
    } //end if
    else if (useGPS && !dropped) // GPS is good, but haven't deployed dropsondes yet
    { 
      if (GPSdata[Alt] >= GPSdata[prevAlt] && gps.altitude.isValid()) // still going up
      { 
        maxFlightTime += 30 * 1000UL; // give it some more time (30 seconds)
      } //end if
      else  // falling
      {
        if (GPSdata[Alt] >= 10000 && gps.altitude.isValid()) // high enough to drop and still get data. *height subject to change.*
        { 
          emergency_Status = true; // there is a viable emergency
          dropDown_System();  //start the dropdown system
        }   //end if
        else if (GPSdata[Alt] < 10000 && millis() - currentTime[preflight] >= maxFlightTime) // not high enough
        { // this is kind of the worst scenario.
          maxFlightTime += 30 * 1000UL; // hope this is a mistake and just wait a minute. maybe something will change??
          
        }// end else if

      }// end else

    }// end elseif

  }// end if

  if (GPSdata[speed] >= 100 && gps.speed.isValid()) //if lateral speed is greater than 100 meters/second..? something like that?
  { 
    if (false) // some other check to make
    { 
      ECutDown(); // stop the flight here?
    }
  }//end if

  if (false) // I feel like i still need something else to go here, independent of flight time.
  { 
    // maybe if it's simply falling too fast?
  }// end if

}// end emergency_System


// a visual display that things are working-----------------------------------------------------------
void blink(unsigned int per)  // per is the period of blinking
{ 
  // on for the first half of the period
  if (millis() % per >= per / 2)
  {  
    digitalWrite(LED, HIGH);
  }
  // off for the second half of the period
  else if (millis() % per < per / 2) 
  {
    digitalWrite(LED, LOW);
  }

}//end blink


// save some data for possible later use-------------------------------------------------------------
void saveData(int adr) // input an address to save data to and near
{  
  if (FlightTime() % 30000UL == 0 && !pass && FlightTime() <= 2 * 60 * 60 * 1000UL) // runs every 30 seconds up till 2 hours
  { 
    EEPROM.write(adr, (byte)((FlightTime() / 30000UL))); //save flight time to memory
    // add 1 every 30 seconds. (EEPROM can only store bytes)
    pass = true;  //makes this run only once per time cycle
  } //end if
  else if ( FlightTime() % 30000UL != 0) //
  { 
    pass = false; //reset for use in previous if
  }//end elseif

  if (EEPROM.read(adr - 1) != pos) //if stored value of pos is not equal to current value of pos.
  {
    EEPROM.write(adr - 1, pos); // reasign stored value to be current value
  }

}//end saveData


//handles the Flight Termination system--------------------------------------------------------------
void ECutDown() 
{
  // once all dropsondes have been deployed
  if ((pos == 7 || false) && !ended) 
  { 
    //change that false to something that will make this system run under certain conditions without pos == 7 being true
    digitalWrite(dropPin[0], HIGH); // turn pin 20 on
    delay(15000); //wait 15 seconds
    digitalWrite(dropPin[0], LOW);  // turn pin 20 off
    ended = true; //end this system
  }// end if
}// end ECutDown
