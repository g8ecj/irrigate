//---------------------------------------------------------------------------
// Copyright (C) 2009-2014 Robin Gilks
//
//
//  irr_onewire.c   -   This section looks after the 1-wire interface
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


#include <owcapi.h>

#include "irrigate.h"


char devcopy[MAXDEVICES][16];
uint8_t numgpio = 0;
uint8_t numvolt = 0;
uint8_t numtemp = 0;

// define the following if using a version of OWFS prior to 2.9p3
// which has a problem with big endian handling of aggregate PIO devices (e.g. DS2408, DS2413)
#define OW_ENDIAN_BUG 1

#ifdef PC
uint16_t testcurrent = 0;


/* these are a set of dummy functions that are used when compiled for a PC
 * that has no 1-wire devices attached */
uint16_t
irr_onewire_init (void)
{
   return TRUE;
}

void
irr_onewire_stop (void)
{
}

bool
//DoOutput (uint8_t UNUSED (zone), uint8_t UNUSED (state))
DoOutput (uint8_t zone, uint8_t state)
{
   chanmap[zone].output = state;     // the real code doesn't do this - we have to save state so we can fake the current
   return TRUE;
}

void general_reset(void)
{
}

void
irr_onewire_match (uint16_t UNUSED (numgpio))
{
   int i;
   for (i = 1; i < REALZONES; i++)
      if (chanmap[i].valid & CONFIGURED)
         chanmap[i].valid |= HARDWARE;  // got real hardware here
}

uint16_t
GetCurrent (void)
{
   int zone;
   uint16_t mycurrent = 0;

   for (zone = 1; zone < REALZONES; zone++)
   {
      if (debug > 0)
      {
         if (((chanmap[zone].output == ON) || (chanmap[zone].output == TEST)) && (zone != 3))      // fail zone 3 for testing
            mycurrent += chanmap[zone].current;
      }
      else
      {
         if ((chanmap[zone].output == ON) || (chanmap[zone].output == TEST))
            mycurrent += chanmap[zone].current;
      }
   }
   if (testcurrent)
      return 88;
   else
      return mycurrent;
}

double
GetTemp(void)
{
   return 0;
}

void
setGPIOraw(uint8_t UNUSED (index), uint8_t UNUSED (value))
{
}

char *
getGPIOAddr (uint8_t UNUSED (index))
{
   return "1122334455667788";
}

#else

/*
 * The real code follows now!!
 */


// this function relies on chanmap_read having already been called to load up the 
// chanmap with details of what devices should be present
uint16_t
irr_onewire_init (void)
{
   uint8_t numdev = 0;
   int16_t family;
   char * tokenstring;
   size_t s ;
   char seps[] = ",";
   char* token;
   char val[10];

   if (OW_init(device))
   {
      log_printf (LOG_EMERG, "Error: failed to acquire port");
      exit (EXIT_FAILURE);
   }

   OW_set_error_print("2");
   sprintf(val, "%d", debug);
   OW_set_error_level(val);
// get a list of the top of the 1-wire tree to see what devices there are
   OW_get("/",&tokenstring,&s) ;

   token = strtok (tokenstring, seps);
   while (token != NULL)
   {
// keep a copy of the addresses of the devices we find and catagorise them
      if (token[2] == '.')
      {
         token[15] = '\0';
         if (debug)
            log_printf (LOG_INFO, "Device %s", token);
         strncpy(devcopy[numdev++], token, 16);
         family = strtol(token, NULL, 16);
         switch (family)
         {
         case DS2413_FAMILY_CODE:
            numgpio++;
            break;
         case DS2438_FAMILY_CODE:
            numvolt++;
            break;
         case DS18S20_FAMILY_CODE:
         case DS18B20_FAMILY_CODE:
         case DS1822_FAMILY_CODE:
            numtemp++;
            break;
         }
      }
      token = strtok (NULL, seps);
   }
   free(tokenstring);

   if (numdev == 0)
   {
      log_printf (LOG_EMERG, "Error: nothing found on 1-wire bus - check cables?");
      exit (EXIT_FAILURE);
   }

   log_printf (LOG_INFO, "Found %d GPIO chip(s) OK", numgpio);
   log_printf (LOG_INFO, "Found %d voltage monitor chip(s) OK", numvolt);
   log_printf (LOG_INFO, "Found %d temperature monitor chip(s) OK", numtemp);

   // clear all hardware outputs down
   general_reset ();


   return numdev;
}

void irr_onewire_match (uint16_t numdev)
{
   uint8_t dev, zone;
   // match the values I've read from the file with the devices I scanned and match them up
   for (zone = 1; zone < REALZONES; zone++)
   {
      for (dev = 0; dev < numdev; dev++)
      {
         if (strncasecmp (chanmap[zone].address, devcopy[dev], 15) == 0)
         {
            if (debug)
            {
               if (chanmap[zone].type & ISSENSOR)
               {
                  printf ("Match at zone %d (%s) device %d address %s sensor type %s\n", 
                       zone, chanmap[zone].name, dev, chanmap[zone].address, sensornames[sensormap[chanmap[zone].link].type]);
               }
               else
               {
                  printf ("Match at zone %d (%s) device %d port %c 1-wire address %s\n", 
                       zone, chanmap[zone].name, dev, chanmap[zone].AorB ? 'A' : 'B', chanmap[zone].address);
               }
            }
            chanmap[zone].valid |= HARDWARE;       // got real hardware here
            chanmap[zone].dev = dev;
         }
      }
   }
}

#if OW_ENDIAN_BUG
ssize_t OW_pio_workaround(uint8_t zone, uint8_t state)
{
   static uint8_t iocache[MAXDEVICES] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
   char path[32];
   char val[10];
   uint8_t ioval, index = chanmap[zone].dev;
   ssize_t ret;

   sprintf(path, "/%s/PIO.BYTE", chanmap[zone].address);

   if (chanmap[zone].AorB)
   {
      // operating on PIOA - need current state of B with A bit cleared to activate it
      if (state == OFF)
         ioval = iocache[index] & 0xfe;
      else
         ioval = iocache[index] | 0x01;
   }
   else
   {
      // operating on PIOB
      if (state == OFF)
         ioval = iocache[index] & 0xfd;
      else
         ioval = iocache[index] | 0x02;
   }

   sprintf(val, "%d", ioval);

   ret = OW_put(path, val, strlen(val));
   // if the write worked then update the cache
   if (ret >=0)
      iocache[index] = ioval;

   return ret;
}
#endif


void general_reset(void)
{
   int16_t i;
   char path[32];
   char val[10] = "0,0";

   for (i = 1; i < REALZONES; i++)
   {
      if (chanmap[i].type & ISOUTPUT)
      {
         sprintf(path, "/%s/PIO.ALL", chanmap[i].address);
         OW_put(path, val, strlen(val)) ;
      }
   }
}




void
irr_onewire_stop (void)
{
   OW_finish() ;
}


// using the configured sensor mapping, average the readings from all the
// sensors of the type requested
double
GetSensor(eSENSOR type)
{
   uint8_t sensor, num = 0;
   double value = 0;
   ssize_t ret;
   char path[32];
   char * tokenstring;
   size_t s;


   for (sensor = 0; sensormap[sensor].zone; sensor++)
   {
      if (sensormap[sensor].type == type)
      {
         sprintf(path, sensormap[sensor].path, chanmap[sensormap[sensor].zone].address);
         ret = OW_get(path,&tokenstring,&s);
         if (ret < 0)
         {
            num = 0;        // flag that we failed to get data
            break;
         }
         else
         {
            value += atof(tokenstring);
            free(tokenstring);
            num++;
         }
      }
   }
   if (num > 0)
      return value / num;
   else
      return -999;

}


// read the volts from the current transformer interface, convert volts to current.
uint16_t
GetCurrent (void)
{
   double milliamps;

   milliamps = GetSensor(eCURRENT) * VoltToMilliAmp;
// can't read reliably below 1.5 volts (but need down to ~0.1V)
   if (milliamps < 50)
      milliamps = 0;

   return (uint16_t) milliamps;
}

double
GetTemp(void)
{
   double temp;

   temp = GetSensor(eEXTTEMP);

   return temp;
}

// get/set the time in the DS2438 that handles the current transformer interface
time_t
GetTime (void)
{
   double time;

   time = GetSensor(eGETTIME);

   return (time_t)time;
}


void
SetTime(void)
{
   char path[32];
   uint8_t sensor;

   for (sensor = 0; sensormap[sensor].zone; sensor++)
   {
      if (sensormap[sensor].type == eSETTIME)
      {
         sprintf(path, sensormap[sensor].path, chanmap[sensormap[sensor].zone].address);
         OW_put(path, "", 0) ;
         break;
      }
   }
}


void
setGPIOraw(uint8_t index, uint8_t value)
{
   char path[32];
   char val[10];

   sprintf(path, "/%s/PIO.BYTE", devcopy[index]);
   sprintf(val, "%d", value);
   OW_put(path, val, strlen(val)) ;

}

char *
getGPIOAddr (uint8_t index)
{
   return devcopy[index];
}
   
//--------------------------------------------------------------------------
// function - DoOutput
// This routine sets either PIOA or PIOB according to the zone mapping
// in the chanmap array. 
//
// 'zone'  - Number from 1...max zone
// 'state'    - TRUE if trying to activate a port (set low)
//
// Returns: TRUE(1):    If set is successful
//          FALSE(0):   If set is not successful
//

bool
DoOutput (uint8_t zone, uint8_t state)
{
   bool ret = TRUE;
   char path[32];
   char val[10];

   if (((chanmap[zone].valid & HARDWARE) == 0) && (state != OFF))    // if no hardware and trying to switch on then fail
   {
      return FALSE;
   }

   if (chanmap[zone].AorB)
   {
      // operating on PIOA
      sprintf(path, "/%s/PIO.A", devcopy[chanmap[zone].dev]);
   }
   else
   {
      // operating on PIOB
      sprintf(path, "/%s/PIO.B", devcopy[chanmap[zone].dev]);
   }

   sprintf(val, "%d", state == OFF ? 0 : 1);
#if OW_ENDIAN_BUG
   ret = OW_pio_workaround(zone, state);
#else
   ret = OW_put(path, val, strlen(val));
#endif

   if (debug)
   {
      printf ("Device %s port %c state %02x\n", devcopy[chanmap[zone].dev],
              chanmap[zone].AorB ? 'A' : 'B', state);
   }

   if (ret < 0)
   {
      errno = EIO;
      log_printf (LOG_ERR, "Failed to switch zone %d %s", zone, state ? "ON" : "OFF");
   }

   return (ret < 0 ? FALSE : TRUE);
}

#endif       /* PC */

//--------------------------------------------------------------------------
// function - check_current
// This routine checks the current drawn by all the valves active
// Only active if 'monitor' is set
//
// Returns: TRUE(1):    If set is successful
//          FALSE(0):   If set is not successful
//
bool 
check_current(void)
{
   uint16_t expected_current = 0, minC, maxC, actual_current;

   bool ret = TRUE;

   if (monitor)
   {

      expected_current = get_expected_current();

      // allow 20% slack in current that is read
      minC = expected_current * 0.8;
      maxC = expected_current * 1.2;

      actual_current = GetCurrent ();
      if (((actual_current > maxC) || (actual_current < minC)) && (expected_current > 0))
      {
         log_printf (LOG_ERR, "Current draw %u - expecting from %u to %u", actual_current, minC, maxC);
         ret = FALSE;
      }
   }

   return ret;
}



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
bool
SetOutput (uint8_t zone, uint8_t state)
{
   bool ret = TRUE;

   ret = DoOutput (zone, state);
   if (ret)
   {
      // keep track of the actual I/O state so we know what current to expect
      chanmap[zone].output = state;
      usleep (100000);          // wait 100mS settling time
      ret = check_current ();
   }
   if (ret)
   {
      // all OK - nothing else to do here!!
   }
   else
   {
      // force this last zone off
      DoOutput(zone, OFF);
      chanmap[zone].output = OFF;
      errno = ERANGE;
      log_printf (LOG_ERR, "Switching %s zone %u current out of spec.", state ? "ON" : "OFF", zone);
   }
   return ret;
}

