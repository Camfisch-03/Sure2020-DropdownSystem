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
long currentTime[] = {0, 0, 0, 0}; // Time stamps for:
const byte Drop = 0, preflight = 3;  // GPS systems, Dropdown system, FlightTime
float prevAlt = 0, currentAlt = 0;  // previaous altitude and current altitude for somparison
uint32_t utcTime; // coordinated universal time
int dropPin[] = {20, 14, 15, 16, 17, 18, 19}; // which pins are attached to the nichrome system
byte pos = 1, prevState = 0;  // dropdown system place markers
unsigned long time_On = 10000, time_Off[] = {0, 20000, 20000, 40000, 20000, 20000, 60000}; // dropdown delay times
//time on for how long to have nichrome hot for, time off for spacend between dropsonde deployments
int  bad_sat_count = 0, ascent_count = 0; // count for satCheck, count for ascending updates
bool begun_flight = false, begun_drop = false, dropped = false; // has the main flight started? has the dropdown system started?
bool useGPS = true, emergency_Status = false, pass = false, ended = false;  //pass is for saveData, ended is for ECutDown
unsigned long maxFlightTime = 2700000, dropFlightTime = 2100000;  //max is for emergency stsyem start, drop is sropdown system start
int DataAddr = 14; // address to store data, increment by 3 after each flight. reset all addresses after 1000
// address is for FlighTtime, address - 1 is for pos, address - 2 is for begun_drop


////GPS declaration-----------------------------------------------------------------------------------
TinyGPSPlus gps;  // GPS object is named gps

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
  pinMode(11, INPUT);
  for (unsigned int i = 0; i < (sizeof(dropPin) / sizeof(dropPin[0])); i++) //cycle through pin array
  {
    digitalWrite(dropPin[i], LOW);  //set pins to off
  } //end for

  for (unsigned int i = 0; i < (sizeof(dropPin) / sizeof(dropPin[0])); i++) //cycle through pin array
  {
    digitalWrite(dropPin[i], LOW);  // make sure pins are off
  }

  // memory checks from previous flight data
  if (EEPROM.read(DataAddr - 1) == 0)  //if this address has never been used before
  {
    EEPROM.write(DataAddr - 1, pos); //reassign it to the starting value for pos
  } //end if

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
  Serial.println("Begin");
  while (!begun_flight)
  {
    //receive any incoming data
    FeedGPS();

    satCheck();

    // ccheck to see if altitude data has been updated
    if (gps.altitude.isUpdated())
    {
      //is altitude above minalt
      if (gps.altitude.meters() >= minAlt)
      {
        ascent_count++; //incerment good check
        Serial.print("ascent count is:  "); Serial.println(ascent_count);
      }
      //if altitude below minalt
      else
      {
        ascent_count = 0; //reset check
        Serial.println("ascent count is 0");
      }
      prevAlt = currentAlt; // used for change in altitude
      currentAlt = gps.altitude.meters(); // update altitude
    } // end if
    //if above minalt for 5 updates in a row
    if(digitalRead(11) == HIGH)
    {
      Serial.println("continue on ahead");
      ascent_count = 5;
    }
    if (ascent_count >= 5)
    {
      begun_flight = true;  //start the main flight
    }
  if(useGPS)
  {
    blink(2000); //blink at rate of 2 seconds to signify working
  }
  else
  {
    blink(500);
  }
    
  } // end while(!begun_flight)

  delay(500);
  // mainflight setup--------------------------------------------------------------------------------
  if (ascent_count >= 1) // if preflight system wasnt skipped over
  {
    currentTime[preflight] = millis(); // used for the FlightTime calculation
    Serial.println("buisness as usual");
  } //end if
  else
  {
    begun_drop = EEPROM.read(DataAddr - 2); //assign the previous value of begun_drop to the current variable
    Serial.println("Skipped preflight");
  }//end else
  digitalWrite(13, LOW); // stop blinking as flight has begin

  Serial.println("main flight beginning now");
  
  // primary functionality, once flight begins -------------------------------------------------------
  while (1 == 1)
  {
    //look for incoming info from the GPS
    FeedGPS();

    // check the status of satellites
    satCheck();

    //check to see if dropsondes can be released
    dropDown_System();

    // runs 4 times per second (ideally)
    if (FlightTime() % 250UL == 0 || emergency_Status)
    {
      //checks for emergencies, and if it can handle them
      emergency_System();
    }

    //save flight time + pos to memory at address DataAddr
    saveData(DataAddr);

    if (gps.altitude.isUpdated())
    {
      prevAlt = currentAlt; // used for change in altitude
      currentAlt = gps.altitude.meters(); // update altitude
    }


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
  { //while there is new data
    gps.encode(HWS.read());  //send that new data to the GPS encoder to be read in getGPSdata()

  }//end while

  //neither location nor altitude have been updated in 5 sec or more
  if (gps.altitude.age() >= 5000UL && gps.location.age() >= 5000UL)
  {
    Serial.println("no new data for a while");
    useGPS = false; // preemptively declare GPS to be not working
    delay(50); //just so this doesnt run thousands of time per second, main ysstem doesnt need this. 
  }
  // the position data has been updated very recently
  else if ( gps.altitude.age() < 5UL && gps.location.age() < 5UL)
  {
    useGPS = true;
    //Serial.print("coo");
  } // end else

}//end FeedGPS


//checks to see how many satellites are present-------------------------------------------------------
void satCheck()
{
  if (gps.satellites.isUpdated())
  {
    Serial.print(gps.satellites.value()); Serial.print(" Satellites in view, ");
    if (gps.satellites.value() <= 3)
    {
      bad_sat_count++;
      Serial.print("bad check # "); Serial.println(bad_sat_count);
    } // end if
    else
    {
      bad_sat_count = 0;
      useGPS = true;
      Serial.println("good");
    } // end else

  } // end if
  if (bad_sat_count >= 5)
  {
    useGPS = false;
  } // end if

}//end satCheck



//activate the dropdown system----------------------------------------------------------------------
void dropDown_System()
{
  if (!dropped)
  {
    //valid GPS data is above maxAlt, or flight time exceeds maxflighttime
    if (((useGPS && gps.altitude.meters() >= maxAlt && gps.altitude.isValid() ) || (FlightTime() >= dropFlightTime && !useGPS)))
    {
      begun_drop = true;
    }
    if (begun_drop || emergency_Status || digitalRead(11) == HIGH)
    {
      if (!begun_drop)
      {
        begun_drop = true;
      }

      // was off for the specified time, now it should be on
      if (pos <= 6 && prevState == 0 && FlightTime() >= currentTime[Drop] + time_Off[pos - 1])
      {
        digitalWrite(dropPin[pos], HIGH);
        if (pos == 1)
        {
          Serial.println("begin drop down sequence");
          EEPROM.write(DataAddr - 2, begun_drop); // assign current statud of dropdown system to memory
        }
        Serial.print( pos ); Serial.println("is Hot");
        prevState = 1;  // system is currently on
        currentTime[Drop] = FlightTime(); // reassign drop time for comparisons
      } //end if

      // was on for specified amount of time, now it should be off
      else if (pos <= 6 && prevState == 1 && FlightTime() >= currentTime[Drop] + time_On)
      {
        digitalWrite(dropPin[pos], LOW);
        prevState = 0;  //system is now off
        Serial.print( pos ); Serial.println("is Hot");
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
  }

}// end dropDown_System


// handes various situations classified as emergencies-----------------------------------------------
void emergency_System()
{
  // flighttime has exceeded maxflighttime and dropsondes havent been drployed yet
  if (millis() - currentTime[preflight] >= maxFlightTime && !dropped) 
  {
    Serial.println("max flight time exceeded without a drop");
    if (!useGPS) // GPS is giving bad signal
    {
      emergency_Status = true;  //declare an emergency
      Serial.println("good to drop");
      dropDown_System();  //send system to start dropdown function
    } //end if
    else // GPS is good, but haven't deployed dropsondes yet
    {
      if (gps.altitude.meters() >= prevAlt && gps.altitude.isValid()) // still going up
      {
        maxFlightTime += 30 * 1000UL; // give it some more time (30 seconds)
        Serial.println("wait a bit");
      } //end if
      else  // falling
      {
        if (gps.altitude.meters() >= 10000 && gps.altitude.isValid()) // high enough to drop and still get data
        {
          emergency_Status = true; // there is a viable emergency
          Serial.println("good to go");
          dropDown_System();  //start the dropdown system
        }   //end if

        // not high enough to get decent data
        else if (gps.altitude.meters() < 10000 && millis() - currentTime[preflight] >= maxFlightTime) 
        { // this is kind of the worst scenario.
          maxFlightTime += 30 * 1000UL; // hope this is a mistake and just wait a minute. maybe something will change??
          Serial.println("wait some more");

        }// end else if

      }// end else

    }// end elseif

  }// end if

  if (gps.speed.mps() >= 100 && gps.speed.isValid()) //if lateral speed is greater than 100 meters/second..? something like that?
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
  } // end if
  // off for the second half of the period
  else
  {
    digitalWrite(LED, LOW);
  } // end else

}//end blink


// save some data for possible later use-------------------------------------------------------------
void saveData(int adr) // input an address to save data to and near
{
  if (FlightTime() % 30000UL == 0 && !pass && FlightTime() <= 2 * 60 * 60 * 1000UL) // runs every 30 seconds up till 2 hours
  {
    EEPROM.write(adr, (byte)((FlightTime() / 30000UL))); //save flight time to memory
    Serial.print(FlightTime()); Serial.println(" saved to memory");
    // add 1 every 30 seconds. (EEPROM can only store bytes)
    pass = true;  //makes this run only once per time cycle
  } //end if
  // at some other time step from the first if
  else if ( FlightTime() % 30000UL == 1)
  {
    pass = false; //reset for use in previous if
  }//end elseif

  if (EEPROM.read(adr - 1) != pos) //if stored value of pos is not equal to current value of pos.
  {
    EEPROM.write(adr - 1, pos); // reasign stored value to be current value
    Serial.print(pos); Serial.println("saved to memory");
  }

}//end saveData


//handles the Flight Termination system--------------------------------------------------------------
void ECutDown()
{
  // once all dropsondes have been deployed
  if ((pos == 7 || false) && !ended)
  {
    //change that false to something that will make this system run under certain conditions without pos == 7 being true
    Serial.println("the beginning of the end");
    digitalWrite(dropPin[0], HIGH); // turn pin 20 on
    delay(15000); //wait 15 seconds
    digitalWrite(dropPin[0], LOW);  // turn pin 20 off
    ended = true; //end this system
    Serial.println("the end");
  }// end if
}// end ECutDown
