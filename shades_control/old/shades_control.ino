/*
  shades rf control
  
*/

#include <Wire.h>
#include <RCSwitch.h>
#include <RTClib.h>
#include <MemoryFree.h>


#define DEBUG true

#define TIME_HEADER         "T"   // Header tag for serial time sync message
#define TIME_REQUEST        7     // ASCII bell character requests a time sync message 
#define SHADE_ACTION_HEADER "S"


//const unsigned long PROGMEM DEFAULT_TIME  = 1522910280;   // 04/05/2018 @ 6:38am (UTC)
//const unsigned long PROGMEM DEFAULT_TIME  = 1522843800;   // 04/04/2018 @ 12:10pm (UTC)
//const unsigned long PROGMEM DEFAULT_TIME  = 1522867800;   // 04/04/2018 @ 6:50pm (UTC)
//const unsigned long PROGMEM DEFAULT_TIME  = 1522868390;   // 04/04/2018 @ 6:59:50pm (UTC)
//const unsigned long PROGMEM DEFAULT_TIME  = 3600;         // 1am 1st Jan 1970
//const unsigned long PROGMEM DEFAULT_TIME  = 0;            // 0am 1st Jan 1970
//const unsigned long PROGMEM DEFAULT_TIME  = 1526241600;   // 2018-05-13T20:00:00+00:00 in ISO 8601
RTC_DS1307 RTC;

const int PROGMEM rf_length = 24;
const int PROGMEM rf_enable_pin = 2;
const int PROGMEM timeBtnPin = 4;                 // time setting button pin number
const int PROGMEM timeBtnSettingModeDur = 2;      // time button press duration (s) to enter time setting mode
unsigned long timeBtnPressedAt = 0;       // when was the time button started being pressed
//DateTime timeBtnPressedAt = DateTime(0);  // (RTC) when was the time button started being pressed
const int PROGMEM timeSettingStep = 3600;         // each time button action will cause time to be set to this amount of seconds more
const int  PROGMEM timeBtn, PROGMEM timeBtnReleasedAfterSetting;

//const char button_a = F("010101010101010100110000");
//const char button_b = F("010101010101010100001100");
//const char button_c = F("010101010101010111000000");
//const char* button_d = F("010101010101010100000011");
//const char* BRING_UP_CODE    = F("010101010101010100110000");
//const char* BRING_DOWN_CODE  = F("010101010101010100001100");
//const char* STOP_CODE        = F("010101010101010111000000");


// actions definitions
const int PROGMEM OPEN = 1, PROGMEM CLOSE = 2, PROGMEM STOP = 3;  // open and close values also mean their field position in shades table.


// shades definitions
struct Shade{
  int shade_id;      // first digit means the floor number; second and third mean shade number on that floor
  int up_duration;   // duration in seconds
  int down_duration; // duration in seconds
  unsigned long bring_up_code;
  unsigned long bring_down_code;
  unsigned long stop_code;
};
const int PROGMEM shade_list_size = 12;

// shade IDs
const int PROGMEM KITCHEN01   = 101;
const int PROGMEM KITCHEN02   = 102;
const int PROGMEM ATELIER     = 103;
const int PROGMEM LIVING01    = 104;
const int PROGMEM LIVING02    = 105;
const int PROGMEM LIVING03    = 106;
const int PROGMEM SUITE01     = 201;
const int PROGMEM SUITE02     = 202;
const int PROGMEM OFFICE      = 203;
const int PROGMEM ROOM_TI     = 204;
const int PROGMEM WC          = 205;
const int PROGMEM ROOM_GUGA   = 206;

// shade initialization
const Shade shades[shade_list_size] PROGMEM = {
//        id, up duration, down duration,   up code, down code, stop code
  {KITCHEN01,          20,            15,   5592393,   5592394,   5592395},
  {KITCHEN02,          20,            15,   5592401,   5592402,   5592403},
  {  ATELIER,          20,            15,   5592409,   5592410,   5592411},
  //{ LIVING01,          20,            20,   5592368,   5592418,   5592419},
  { LIVING01,          20,            23,   5592368,   5592332,   5592512},  //remote fob a, b and c button codes
  { LIVING02,          20,            20,   5592425,   5592426,   5592427},
  { LIVING03,          20,            20,   5592433,   5592434,   5592435},
  {  SUITE01,          20,            15,   5592457,   5592458,   5592459},
  {  SUITE02,          20,            15,   5592465,   5592466,   5592467},
  {   OFFICE,          20,            15,   5592473,   5592474,   5592475},
  {  ROOM_TI,          25,            15,   5592481,   5592482,   5592483},
  {       WC,          20,            15,   5592489,   5592490,   5592491},
  {ROOM_GUGA,          20,            15,   5592497,   5592498,   5592499}
};


// scheduling definitions
struct Schedule {
  unsigned long  timestamp;
  int            shade_id;
  int            duration_factor;
  int            action;
  int            hours;
  int            season;
  bool           weekend;
  bool           working_day;
};
Schedule schdl;

// schedule initialization
int last_min_performed=-1;
unsigned long t_now, t_previous;
const int PROGMEM action_list_size          = 50;  // items
const int PROGMEM schedule_list_size        = 100; // items
const int PROGMEM schedule_check_period     = 10;  // seconds
const int PROGMEM schedule_check_tolerance  = 2;   // seconds

const Schedule PROGMEM schedules[schedule_list_size] = {
//timestamp,  shade id, dur fact, action, hours, season, weekend, work day 
// {        0, KITCHEN01,        1,   OPEN,  700,      0,       0,        0},
// {        0, KITCHEN01,        1,  CLOSE, 2100,      0,       0,        0},
// {        0, KITCHEN02,        1,   OPEN,  702,      0,       0,        0},
// {        0, KITCHEN02,        2,  CLOSE, 2102,      0,       0,        0},
// {        0,   ATELIER,        1,   OPEN,  640,      0,       0,        0},
// {        0,   ATELIER,        1,  CLOSE, 2104,      0,       0,        0},
// {        0,  LIVING01,        1,   OPEN,  640,      0,       0,        0},
// {        0,  LIVING01,        1,  CLOSE, 2120,      0,       0,        0},
// {        0,  LIVING02,        1,   OPEN,  640,      0,       0,        0},
// {        0,  LIVING02,        1,  CLOSE, 1900,      0,       0,        0},
// {        0,  LIVING03,        1,   OPEN,  640,      0,       0,        0},
// {        0,  LIVING03,        1,  CLOSE, 2106,      0,       0,        0},
// {        0,   SUITE01,        1,   OPEN,  640,      0,       0,        0},
// {        0,   SUITE01,        1,  CLOSE, 2108,      0,       0,        0},
// {        0,   SUITE02,        1,   OPEN,  640,      0,       0,        0},
// {        0,   SUITE02,        1,  CLOSE, 2110,      0,       0,        0},
// {        0,    OFFICE,        1,   OPEN,  640,      0,       0,        0},
// {        0,    OFFICE,        1,  CLOSE, 2112,      0,       0,        0},
 {        0,   ROOM_TI,        1,   OPEN,  700,      0,       0,        0},
 {        0,   ROOM_TI,        1,  CLOSE, 1955,      0,       0,        0}
// {        0,        WC,        1,   OPEN,  640,      0,       0,        0},
// {        0,        WC,        1,  CLOSE, 2114,      0,       0,        0},
// {        0, ROOM_GUGA,        1,   OPEN,  640,      0,       0,        0},
// {        0, ROOM_GUGA,        1,  CLOSE, 2116,      0,       0,        0}
};


// actions definition
struct Action {
  int           shade_id;
  unsigned long start_time;
  int           task;
  int           duration;
  int           started=0;
};

int ii = 0, tsk_count = 0;

static Action running_actions[action_list_size];
Action action;


// RF definitions
RCSwitch mySwitch = RCSwitch();


// SETUP = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
void setup() {
  // communication with PC = = = = = = = = = = = = = = = = = = = = = = = = 
  Serial.begin(9600);

  // time keeping  = = = = = = = = = = = = = = = = = = = = = = = = = = = = 
//  setTime(DEFAULT_TIME);  
  Wire.begin();
  RTC.begin();
  digitalClockDisplay();
  pinMode(13, OUTPUT); digitalWrite(13, HIGH); delay(3000); digitalWrite(13, LOW);
//  setSyncProvider(requestSync);  //set function to call when sync required


  // touch button for setting up time: 12am, 1pm, 2pm, 3pm .. 11pm
//  pinMode(timeBtnPin, INPUT);
  

 // managing shades = = = = = = = = = = = = = = = = = = = = = =
  t_previous = RTC.now().unixtime();


  // Transmitter is connected to Arduino Pin #10  
  mySwitch.enableTransmit(rf_enable_pin);

  // Optional set pulse length.
  // mySwitch.setPulseLength(320);
  
  // Optional set protocol (default is 1, will work for most outlets)
  // mySwitch.setProtocol(2);
  
  // Optional set number of transmission repetitions.
  mySwitch.setRepeatTransmit(15);
  
};



// MAIN PROGRAM = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =
void loop() {

  // check if time touch button pressed
//  timeBtn = digitalRead(timeBtnPin);
//  //timeBtn = LOW;  // disabling the touch button
//  if (timeBtn == HIGH)
//  {
//    if (DEBUG) {Serial.println(String(F("timeBtnPressedAt: "))+timeBtnPressedAt);}
//    if (timeBtnPressedAt == 0) timeBtnPressedAt = now();
//    if(now()-timeBtnPressedAt >= timeBtnSettingModeDur && timeBtnReleasedAfterSetting)
//    {
////      //setTime(now()+3600); // advance 1 hour from now
////      if (hour() < 23)
////        setTime(hour()+1, 00, 00, 1, 1, 1970);  // advance to the zeroed next hour
////      else
////        setTime(0);
//              
//      timeBtnReleasedAfterSetting = 0;
////      digitalClockDisplay();
//    }
//  } else
//  {
//    timeBtnPressedAt = 0;
//    timeBtnReleasedAfterSetting = 1;
//  }
  
  // time keeping
  t_now = RTC.now().unixtime();
  if (Serial.available() > 0) {
    processSyncMessage();
  }
//  if (timeStatus()!= timeNotSet && t_now % 10 == 0) {
//    //digitalClockDisplay();  
//  }
//  if (timeStatus() == timeSet) {
//    digitalWrite(13, HIGH); // LED on if synced
//  } else {
//    digitalWrite(13, LOW);  // LED off if needs refresh
//  }
  //  digitalClockDisplay();


  // managing shades
  if ((t_now - t_previous) > schedule_check_period)
  {
    if(DEBUG){digitalClockDisplay();Serial.print(F(" freeMemory()=")); Serial.println(freeMemory());}
    if(freeMemory()<790) {digitalWrite(13, HIGH);}
    // it's time to check the schedule
//    Serial.println(F("checking the schedule..."));
    ii = 0;
    while(ii < schedule_list_size)
    {
      // read the schedule information into temp schedule variable 
      schdl.timestamp       = (unsigned long)pgm_read_word(&(schedules[ii].timestamp));
      schdl.shade_id        = (int)pgm_read_word(&(schedules[ii].shade_id));
      schdl.duration_factor = (int)pgm_read_word(&(schedules[ii].duration_factor));
      schdl.action          = (int)pgm_read_word(&(schedules[ii].action));
      schdl.hours           = (int)pgm_read_word(&(schedules[ii].hours));
      schdl.season          = (int)pgm_read_word(&(schedules[ii].season));
      schdl.weekend         = (bool)pgm_read_word(&(schedules[ii].weekend));
      schdl.working_day     = (bool)pgm_read_word(&(schedules[ii].working_day));
//      if(schdl.hours > 0 && DEBUG)
//      {
//        printSchedule(schdl);
//        Serial.print(hour()); Serial.print(F(" ")); Serial.println(minute());
//      }
// 20200222 - hbertini - debugging morning hours not triggering the shades:
      Serial.print(F("schdl.hours=")); Serial.println(schdl.hours);
      Serial.print(F("RTC.now().hour()*100=")); Serial.println(RTC.now().hour()*100);
// 20200222 - end debugging
      if (schdl.shade_id > 0 && schdl.hours == (RTC.now().hour()*100 + RTC.now().minute()) && RTC.now().minute() > last_min_performed)
      {
        Serial.print(F("Adding schedule to task list: ")); printSchedule(schdl);
        action.shade_id   = schdl.shade_id;
        action.start_time = t_now;
        action.task       = schdl.action;
        action.duration   = getShadeDuration(schdl.shade_id, schdl.action)/schdl.duration_factor;
        addRunningAction(action);
        tsk_count = tsk_count + 1;
      }
      ii = ii+1;
      Serial.flush();
    }
    if (tsk_count > 0){
      last_min_performed = RTC.now().minute();
    }
    t_previous = t_now;
  } else {
    // it's too soon to check the schedule
//    Serial.println(F("too soon to check the schedule..."));
  }



  // managing actions
  performRunningActions();

//  delay(1000);
}



// MANAGING SHADES = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = 
//  char bring_up_code[rf_length];
//  char bring_down_code[rf_length];
//  char stop_code[rf_length];
void performAction(int shade_id, int act)
{
  unsigned long cmd;
  cmd = getShadeActionRFCode(shade_id, act);
  Serial.println(String(F("SENDING ACTION RF "))+act+F(" to shade ")+shade_id);
  //Serial.println(cmd);
  //Serial.println(String(cmd, BIN).c_str());
  mySwitch.send(cmd, 24);
  delay(10);
}


int getShadeDuration(int s_id, int act)
{
  int i = 0, ret = 0;
  while (i < shade_list_size)
  {
    if((int)pgm_read_word(&(shades[i].shade_id)) == s_id)
    {
      //Serial.println(String(F("found shade")));
      if (act == OPEN) 
      {
        ret = (int)pgm_read_word(&(shades[i].up_duration));  
      }
      else
      {
        ret = (int)pgm_read_word(&(shades[i].down_duration));
      }
      i = shade_list_size;
    }
    else
    {
      //Serial.println(String(F("shade not found yet..."))+s_id);
      ret = 0;
    }
    i = i + 1;
  }
  return ret;
}

unsigned long getShadeActionRFCode(int s_id, int act)
{
  int i = 0;
  unsigned long cmd;
  while (i < shade_list_size)
  {
    if((int)pgm_read_word(&(shades[i].shade_id)) == s_id)
    {
      //Serial.println(String(F("found shade")));
      if (act == OPEN) 
      {
        cmd = (unsigned long)pgm_read_dword(&(shades[i].bring_up_code));  
      }
      else if (act == CLOSE)
      {
        cmd = (unsigned long)pgm_read_dword(&(shades[i].bring_down_code));
      }
      else 
      {
        cmd = (unsigned long)pgm_read_dword(&(shades[i].stop_code));
      }
      i = shade_list_size;
    }
    else
    {
      //Serial.println(String(F("shade not found yet..."))+s_id);
      cmd = 0;
    }
    //Serial.print(F("from table: ")); Serial.println(cmd);
    i = i + 1;
  }
  //String(cmd, BIN).c_str();
  //Serial.print(F("rfCode: ")); Serial.println(ret);
  return cmd;
}


void printSchedule(Schedule s)
{
  Serial.print(String(F("ts: "))+schdl.timestamp+F("; "));
  Serial.print(String(F("id: "))+schdl.shade_id+F("; "));
  Serial.print(String(F("dur fac: "))+schdl.duration_factor+F("; "));
  Serial.print(String(F("act: "))+schdl.action+F("; "));
  Serial.print(String(F("hrs: "))+schdl.hours+F("; "));
  Serial.print(String(F("sesn: "))+schdl.season+F("; "));
  Serial.print(String(F("wknd: "))+schdl.weekend+F("; "));
  Serial.println(String(F("wrk day: "))+schdl.working_day+F("; "));
  Serial.flush();
}



// MANAGING RUNNING ACTIONS = = = = = = = = = = = = = = = = = = = = = = = = 
void addRunningAction(Action act)
{
  Serial.println(String(F("act: "))+act.shade_id+F(" ")+act.start_time+F(" ")+act.task+F(" ")+act.duration);
  int j=0;
  // find a free slot in the actions list and add the new task
  while (j < action_list_size)
  {
    if (DEBUG){Serial.println(String(F("action list size: "))+action_list_size+F("; action slot attempt: ")+j);}
    if(running_actions[j].shade_id == 0)
    {
      running_actions[j].shade_id   = act.shade_id;
      running_actions[j].start_time = act.start_time;
      running_actions[j].task       = act.task;
      running_actions[j].duration   = act.duration;
      if (DEBUG){Serial.println(String(F("added running action in slot "))+j+": "+running_actions[j].shade_id+F(" ")+running_actions[j].start_time+F("; tsk: ")+running_actions[j].task+F("; d: ")+running_actions[j].duration);};
      j = action_list_size;
    }
    j = j + 1;
  }
}


void performRunningActions()
{
  int i=0;
  while (i < action_list_size)
  {
    int s= running_actions[i].shade_id;
    if(running_actions[i].shade_id > 0 && running_actions[i].started == 0)
    {
      if (DEBUG){Serial.print(F("triggering shade ")); Serial.println(running_actions[i].shade_id);};
      // the action was not triggered yet
      performAction(running_actions[i].shade_id, running_actions[i].task);
      running_actions[i].started = 1; // mark this task as triggered
    }
    else if (running_actions[i].started == 1)
    {
      // the action was triggered already,
      // so let's check if closure (STOP) action is to be taken
      if(RTC.now().unixtime() - running_actions[i].start_time >= running_actions[i].duration)
      {
        if (DEBUG){Serial.print(F("stopping shade ")); Serial.println(running_actions[i].shade_id);};
        //the time to wait for shade action to complete has expired.
        //sending STOP command to shade, 
        performAction(running_actions[i].shade_id, STOP);

        //and clearing this record from running_actions table
        running_actions[i].shade_id   = 0;
        running_actions[i].start_time = 0;
        running_actions[i].task       = STOP;
        running_actions[i].duration   = 0;
        running_actions[i].started    = 0;
      }
    }
    i = i + 1;
  }
}



// TIME KEEPING = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = 
void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(RTC.now().hour());
  printDigits(RTC.now().minute());
  printDigits(RTC.now().second());
  Serial.print(F(" "));
  Serial.print(RTC.now().day());
  Serial.print(F(" "));
  Serial.print(RTC.now().month());
  Serial.print(F(" "));
  Serial.print(RTC.now().year()); 
  Serial.println(); 
//  int i = 0;
//  while(i < RTC.now().hour())
//  {
//    digitalWrite(13, LOW);
//    delay(100);
//    digitalWrite(13, HIGH);
//    delay(200);
//    i = i + 1;
//  }
  digitalWrite(13, LOW);
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(F(":"));
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}


void processSyncMessage() {
  int shade_control, m, h;
  unsigned long pctime;
  //const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013

  Serial.println(F("Processing manual message from serial"));

  if (Serial.find(SHADE_ACTION_HEADER)) {
    Serial.println(F("Manual shade command received"));
    // SERIAL COMMAND FORMAT: SIIIA, where S is the header, III is the shade id, and A is the action 1=OPEN, 2=CLOSE, 3=STOP
    // EXAMPLE SERIAL COMMAND: S1041
    shade_control = Serial.parseInt();
    action.shade_id = shade_control/10;
    action.start_time = RTC.now().unixtime();
    action.task = shade_control % 10;
    action.duration = getShadeDuration(action.shade_id, action.task);
    action.started = 0;
    addRunningAction(action);
  }
//  if(Serial.find(TIME_HEADER)) {
//      digitalClockDisplay();
//    pctime = Serial.parseInt();  // HHMM
//    Serial.println(F("Manual time setting received"));
//    //if( pctime >= DEFAULT_TIME - 86400) { // check the integer is a valid time
//    h=pctime/100;m=pctime % 100;
//      setTime(h, m, 0, day(), month(), year()); // Sync Arduino clock to the time received on the serial port
//      digitalClockDisplay();
//    //}
//  }
  
}
//time_t requestSync()
//{
//  Serial.write(TIME_REQUEST);  
//  return 0; // the time will be sent later in response to serial mesg
//}

//unsigned long now(){
//  return (unsigned long) DateTime().unixtime();
//}
