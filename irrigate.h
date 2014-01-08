//---------------------------------------------------------------------------
// Copyright (C) 2009-2010 Robin Gilks
//
//
//  irrigate.h   -   Function prototypes, global variables and #defines!!
//
//             0.70 - split into several files
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA




#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <syslog.h>
#include <json/json.h>

#include "mongoose.h"

#define VERSION "2.30"

#define UNUSED(arg) (arg) __attribute__ ((unused))

#if !defined boolean
typedef bool boolean;
#endif

// Constant definitions
#define SWITCH_FAMILY      0x3A
#define MAXDEVICES         18

#define MAXDEVLEN 80
#define MAXFILELEN 80

#define VIRTUALZONES 10
//#define REALZONES (MAXDEVICES * 2)
#define REALZONES 100
#define MAXZONES (REALZONES + VIRTUALZONES)
/* enumerate virtual zones */
#define FROST_LOAD   (REALZONES + 1)
#define RESCHEDULE   (REALZONES + 2)
#define RESET        (REALZONES + 3)


#define FROST_TIME 60
#define VALVE_OVERLAP 1
#define VoltToMilliAmp 275

// times used in action stuff
#define TEST_TIME 5
#define FAILED_RETRY 30
#define ONTIME 10800
#define SLACK_TIME 5

/*
 Must put some info in here!!
*/



struct mapstruct
{
   uint8_t zone;                // replicates the index into the map array
   uint16_t valid;              // bit ORed for validation of the zone in various modes
   uint8_t useful;              // whether the zone has useful scheduling data in it
   uint8_t output;              // track the actual state of the output. One of OFF, ON, TEST
                                // + used for user input while configuring the hardware
   uint8_t dev;                 // device index into the address array
   uint8_t AorB;                // whether this zone is on port A (TRUE) or B of the IO chip
   char address[17];            // 8 byte chip address in hex - derived from famsw[dev]? includes null terminator
   char name[33];               // up to 32 chars free format name string
   uint8_t state;               // if currently on or off - used when toggling an object
   int16_t duration;            // total of how long a zone is active for
   uint16_t period;             // the working value of how long to be on for on this pass
   int32_t frequency;           // how often the zone is activated
   uint8_t locked;              // > 0 if locked (only really applies to a pump)
   uint16_t flow;               // nominal flow rate
   time_t starttime;            // when this zone will start
   time_t actualstart;          // when this zone actually started (may have been delayed by a power reset)
   double totalflow;            // total flow seen on this zone during this run
   uint16_t type;               // bit ORed for pump, frost bits
   uint16_t group;              // what groups the zone is in
   uint16_t current;            // expected current draw in mA
};

// what 'state' we are in
#define IDLE      0
#define ACTIVE    1
#define ERROR     2
// List of actions saved in the history file used to provide the retrospective timeline display
// follow on from state values!!
#define WASOK      3
#define WASCANCEL  4
#define WASFAIL    5

// what an output can do
#define OFF       0
#define ON        1
#define TEST      2


// what 'type' attributes a zone has
#define ISPUMP   1
#define ISFROST  2
#define ISGROUP  4              // first bit of several groups possible
#define ISDPFEED 8
#define ISTEST   16
#define ISSPARE  64

#define FROST_OFF    1
#define FROST_MANUAL 2
#define FROST_ACTIVE 3

// what validation bits are used
#define HARDWARE 1              // the hardware actually exists for this zone
#define CONFIGURED 2            // exists in the config file so operate on it



// ordered time queue
typedef struct tqueue
{
   time_t starttime;            // when this object is acted on
   uint8_t zone;                // the object number
   uint8_t action;              // what action is to happen on expiry
   struct tqueue *next;         // the next object in the list
} TQ;

//actions
#define NOACTION 0
#define TURNON   1
#define TURNOFF  2
#define CANCEL   3
#define TESTON   4
#define TESTOFF  5
#define UNLOCK   6


// globals that all in this module can see
extern time_t basictime;
extern time_t ampstime;
extern struct mapstruct chanmap[];
extern int8_t wellzone;
extern int16_t wellmaxflow;
extern int16_t wellmaxstarts;
extern int8_t dpfeed;
extern int16_t dpstart;
extern int16_t dpend;
extern uint8_t frost_mode;
extern boolean frost_armed;
extern boolean monitor;
extern int16_t interrupt;
extern uint32_t welltime;
extern double temperature;
extern double temperature1, temperature2;
extern double Tintegral;
extern char daystr[][10];

// config items
extern boolean debug;
extern boolean timestamp;
extern boolean fileflag;
extern boolean background;
extern boolean config;
extern char device[];
extern char configfile[];
extern char datapath[];
extern char logbuf[];
extern char datestring[];
extern char httpport[];
extern char httproot[];
extern char accesslog[];
extern char errorlog[];
extern double Tthreshold;
extern uint16_t frostlimit;
extern uint8_t testmode;

// prototypes
void insert (time_t starttime, uint8_t zone, uint8_t action);
boolean delete (uint8_t zone);
time_t findonqueue (uint8_t zone);
uint8_t dequeuezone(uint8_t *action);
uint8_t walk_queue(uint8_t index, uint8_t * zone, time_t * starttime, uint8_t *action);



/* 
* action routines
*/
void doaction (uint8_t zone, uint8_t action);

/* load all zones with an action to test the solenoid continuity (except the well pump) */
void test_load (uint8_t zone, uint8_t action);
void test_cancel (uint8_t zone);

/* load up 'ISFROST' zones to run in a 'round-robin' with 1 minute duration with 1 second overlap */
void frost_load (void);

/* cancel the frost protect program by removing the virtual zone that keeps it going and remove any currently active */
void frost_cancel (void);

/* load up all the zones in a group to run required duration starting at defined time but load up pump to max flow */
void dogroup (uint8_t group, uint8_t action);

// clear all aspects of a zone - its timer queue entries, frequency etc
// always try and set output off, if it was active then adjust flow and log it
void zone_cancel (uint8_t zone, uint8_t state);

// cancel all the zones in a group and the virtual channel controlling them
void group_cancel (uint8_t group, uint8_t state);

// switch on the pump, update the state
void well_on (uint8_t zone);

// switch off the pump, update the state and say the zone is locked (busy)
// queue an unlock for later
void well_off (uint8_t zone);

// just mark the well zone as no longer locked
void well_unlock (uint8_t zone);

void manage_pumps(void);

// find a domestic feed valve and return the zone its in (0 if not found)
uint8_t find_dpfeed(void);

// switch off all active zones and put into error state
void emergency_off(uint8_t newstate);

void doreset (uint16_t numgpio);

/**
 parseArguments
 * Parse command line arguments.
 */
void parseArguments (int argc, char **argv);


// read from a file to populate the chanmap array of mapstruct entries
// return TRUE if we got some good config data
// return FALSE if no file or something wrong with it
boolean readchanmap (void);

// create a load of mapstruct entries from interactive user input
// this function only gets the 1-wire relevant data (address and port)
void createchanmap (int numdev);

// save the chanmap mapstruct array to a file
// this will require editing by hand to create the display map and other parameters
boolean savechanmap (void);

void irr_web_init(void);
void irr_web_stop(void);


/*
  Each time the schedule is updated, dump the current state to a file in case
  we get a restart and we need to pick up where we left off
  */
boolean write_schedule (void);

/*
  Read the current scheduling data from a file and populate the channel map
  Used at program startup to recover data
  */
boolean read_schedule (void);

/*
  Iterate the chanmap and insert items into the time queue that haven't yet expired
  This function will only schedule items up to 24 hours ahead so it should be called 
  at least once a day as well as on startup
  */
void check_schedule (boolean changes);


/*
* one wire interface
*/

uint16_t irr_onewire_init(int16_t *T1, int16_t *T2);

void general_reset(uint16_t numgpio);

void irr_onewire_stop(void);

void irr_match(uint16_t numgpio);

uint16_t GetCurrent (void);

double GetTemp(uint16_t index);

void setGPIOraw(uint8_t index, uint8_t value);

char * getGPIOAddr (uint8_t index);

//--------------------------------------------------------------------------
// function - SetOutput
// This routine uses 'DoOutput' and 'check_current' to do the heavy lifting
//
// 'zone'  - Number from 1...max zone
// 'state'    - TRUE if trying to activate a port (set low)
//
// Returns: TRUE(1):    If set is successful
//          FALSE(0):   If set is not successful
//
boolean SetOutput (uint8_t zone, uint8_t state);

//--------------------------------------------------------------------------
// function - check_current
// This routine checks the current drawn by all the valves active
// Only active if 'monitor' is set
//
// Returns: TRUE(1):    If set is successful
//          FALSE(0):   If set is not successful
//
boolean check_current(void);

/*
* diagnostic and user I/O functions
*/

void print_chanmap (void);

void print_queue (void);


/**
 * Retrieve user input from the console.
 *
 * min  minimum number to accept
 * max  maximum number to accept
 *
 * @return numeric value entered from the console.
 */
int getNumber (int min, int max);

char * getAddr (uint8_t * SNum);

// convert clock hours to time based on current seconds count
time_t hours2time(uint16_t decahours);

/*
 * every hour update the time spent with the well pump on (assuming it has been on at all)
 * and count how much water has been put on each zone (based on config file values)
 */
void update_statistics (void);
void clean_history (void);
FILE * open_history(void);
int read_history(struct mapstruct * cmap,  FILE *fd);
void write_history (uint8_t zone, time_t endtime, time_t starttime, uint8_t result);
void close_history (FILE *fd);

void log_printf(int severity, const char *format, ...);

void dump_log_msgs(void);

typedef void (*msgfunc_t)(struct mg_connection *conn, time_t time, int priority, char * desc, char * msg);

void send_log_msgs(struct mg_connection *conn, msgfunc_t sendmsg);

// get expected solenoid current by adding up all the active valves
uint16_t get_expected_current(void);
// get expected flow rate by adding up all the active zones
uint16_t get_expected_flow(void);


