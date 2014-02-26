//---------------------------------------------------------------------------
// Copyright (C) 2009-2010 Robin Gilks
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


int8_t VI = -1;
char famgpio[MAXDEVICES][16];
uint8_t numgpio = 0;
char famvolt[MAXDEVICES][16];
uint8_t numvolt = 0;
char famtemp[MAXDEVICES][16];
uint8_t numtemp = 0;



#ifdef PC
uint16_t testcurrent = 0;


/* these are a set of dummy functions that are used when compiled for a PC
 * that has no 1-wire devices attached */
uint16_t
irr_onewire_init (int16_t *T1, int16_t *T2)
{
   *T1 = 1;
   *T2 = 2;
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

void general_reset(uint16_t UNUSED (numgpio))
{
}

void
irr_match (uint16_t UNUSED (numgpio))
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
GetTemp(uint16_t UNUSED (index))
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
 


uint16_t
irr_onewire_init (int16_t * T1, int16_t * T2)
{
   uint8_t i = 0;
   int16_t family;
   char * tokenstring;
   size_t s ;
   char seps[] = ",";
   char* tokens[MAXDEVICES];
   char path[64];
   double Vad;
   char val[10];

   if (OW_init(device))
   {
      log_printf (LOG_ERR, "Error: failed to acquire port");
      exit (EXIT_FAILURE);
   }

   OW_set_error_print("2");
   sprintf(val, "%d", debug);
   OW_set_error_level(val);
// get a list of the top of the 1-wire tree to see what devices there are
   OW_get("/",&tokenstring,&s) ;

   tokens[i] = strtok (tokenstring, seps);
   while (tokens[i] != NULL)
   {
// keep a copy of the addresses of the devices we find and catagorise them
      if (tokens[i][2] == '.')
      {
         tokens[i][15] = '\0';
         family = strtol(tokens[i], NULL, 16);
         switch (family)
         {
         case SWITCH_FAM:
            strncpy(famgpio[numgpio++], tokens[i], 16);
            break;
         case VOLTS_FAM:
            strncpy(famvolt[numvolt++], tokens[i], 16);
            break;
         case TEMP_FAM:
            strncpy(famtemp[numtemp++], tokens[i], 16);
            break;
         }
      }
      i++;
      tokens[i] = strtok (NULL, seps);
   }
   free(tokenstring);

   if (numgpio + numvolt + numtemp == 0)
   {
      log_printf (LOG_ERR, "Error: nothing found on 1-wire bus - check cables?");
      exit (EXIT_FAILURE);
   }

   log_printf (LOG_INFO, "Found %d GPIO chip(s) OK", numgpio);
   log_printf (LOG_INFO, "Found %d voltage monitor chip(s) OK", numvolt);
   log_printf (LOG_INFO, "Found %d temperature monitor chip(s) OK", numtemp);

   general_reset (numgpio);

   // search all DS2438 chips, categorise according to the volts on the Vad pin
   for (i = 0; i < numvolt; i++)
   {

      sprintf(path, "/%s/VAD", famvolt[i]);
      OW_get(path,&tokenstring,&s) ;
      Vad = atoi(tokenstring);
      if (debug)
         printf ("Read %2.2f volts on Vad pin\n", Vad);
      if (Vad < 1.0)
      {
         VI = i;                // tap volts == ground, using voltage (current) sensors
         if (debug)
            printf ("Current sensor on address  %s\n", famvolt[i]);
      }
      else if (Vad < 3.0)
      {
         *T1 = i;               // tap volts == half supply, just using temp sensor
         if (debug)
            printf ("Temp sensor 1 on address  %s\n", famvolt[i]);
      }
      else if (Vad > 4.5)
      {
         *T2 = i;               // tap volts ~= supply, must be the 3rd temp sensor
         if (debug)
            printf ("Temp sensor 2 on address %s\n", famvolt[i]);
      }
      free(tokenstring);
   }

   if (VI < 0)
   {
      if (monitor)
      {
         log_printf (LOG_ERR, "Current monitor enabled but no current sensor found");
      }
      monitor = FALSE;
   }

   return numgpio;
}

ssize_t my_OW_put(uint8_t index, bool AorB, uint8_t state)
{
   static uint8_t iocache[MAXDEVICES] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
   char path[32];
   char val[10];
   uint8_t ioval;
   ssize_t ret;

   sprintf(path, "/%s/PIO.BYTE", famgpio[index]);

   if (AorB)
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


void general_reset(uint16_t numgpio)
{
   int16_t i;
   char path[32];
   char val[10] = "0,0";

   for (i = 0; i < numgpio; i++)
   {
      sprintf(path, "/%s/PIO.ALL", famgpio[i]);
      OW_put(path, val, strlen(val)) ;
   }
}



void
irr_match (uint16_t numgpio)
{
   int dev, zone;

   // match the values I've read from the file with the devices I scanned and match them up
   for (zone = 1; zone < REALZONES; zone++)
   {
      for (dev = 0; dev < numgpio; dev++)
      {
         if (strncasecmp (chanmap[zone].address, famgpio[dev], 15) == 0)
         {
            if (debug)
               printf ("Match at zone %d device %d port %C 1-wire address %s\n", zone, dev,
                       chanmap[zone].AorB ? 'A' : 'B', chanmap[zone].address);
            chanmap[zone].valid |= HARDWARE;       // got real hardware here
            chanmap[zone].dev = dev;
         }
      }
   }
}

void
irr_onewire_stop (void)
{
   OW_finish() ;
}

uint16_t
GetCurrent (void)
{
   char path[32];
   char * tokenstring;
   double Vad;
   size_t s ;

   if (VI < 0)
      return 0;
   sprintf(path, "/uncached/%s/VAD", famvolt[VI]);
   OW_get(path,&tokenstring,&s) ;
   Vad = atof(tokenstring) * VoltToMilliAmp;
   free(tokenstring);
// can't read reliably below 1.5 volts (but need down to ~0.1V)
   if (Vad < 50)
      Vad = 0;

   return (uint16_t) Vad;
}

double
GetTemp(uint16_t index)
{
   double temp;
   char path[32];
   char * tokenstring;
   size_t s ;

   sprintf(path, "/%s/temperature", famvolt[index]);
   OW_get(path,&tokenstring,&s) ;
   temp = atof(tokenstring);
   free(tokenstring);
   return temp;
}


void
setGPIOraw(uint8_t index, uint8_t value)
{
   char path[32];
   char val[10];

   sprintf(path, "/%s/PIO.BYTE", famgpio[index]);
   sprintf(val, "%d", value);
   OW_put(path, val, strlen(val)) ;

}

char *
getGPIOAddr (uint8_t index)
{
   return famgpio[index];
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
      sprintf(path, "/%s/PIO.A", famgpio[chanmap[zone].dev]);
   }
   else
   {
      // operating on PIOB
      sprintf(path, "/%s/PIO.B", famgpio[chanmap[zone].dev]);
   }

   sprintf(val, "%d", state == OFF ? 0 : 1);
//   ret = my_OW_put(path, val, strlen(val));
   ret = my_OW_put(chanmap[zone].dev, chanmap[zone].AorB, state);

   if (debug)
   {
      printf ("Device %s port %c state %02x\n",famgpio[chanmap[zone].dev],
              chanmap[zone].AorB ? 'A' : 'B', state);
   }

   if (ret < 0)
      log_printf (LOG_ERR, "Failed to switch zone %d %s", zone, state ? "ON" : "OFF");

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
         log_printf (LOG_ERR,
                  "Current draw %u - expecting from %u to %u", actual_current, minC, maxC);
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
      log_printf (LOG_ERR,
               "Switching %s zone %u current out of spec.", state ? "ON" : "OFF", zone);
   }
   return ret;
}

