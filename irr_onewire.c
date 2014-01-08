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


#include <pthread.h>

#include "irrigate.h"

#include "ownet.h"
#include "atod26.h"
#include "swt3A.h"
#include "findtype.h"

pthread_mutex_t onewire_mutex;
int8_t VI = -1;
uint8_t iocache[MAXDEVICES];
uint8_t famsw[MAXDEVICES][8];
uint8_t famvolt[MAXDEVICES][8];
int8_t portnum = 0;



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
   int16_t i;
   double Vad;
   uint16_t cnt = 0, numgpio = 0, numbm = 0;

   if ((portnum = owAcquireEx (device)) < 0)
   {
      log_printf (LOG_ERR, "Error: failed to acquire port");
      exit (EXIT_FAILURE);
   }

   // find all the GPIO chips and clear them down
   cnt = 0;
   while (1)
   {
      numgpio = FindDevices (portnum, &famsw[0], SSWITCH_FAM, MAXDEVICES);

      if (numgpio == 0)
      {
         if (cnt > 10)
         {
            log_printf (LOG_ERR, "Error: no Switch (GPIO) devices found");
            exit (EXIT_FAILURE);
         }
         else
         {
            cnt++;
         }
      }
      else
         break;
   }

   general_reset (numgpio);

   log_printf (LOG_INFO, "Found %d GPIO chip(s) OK", numgpio);

   // try and find at least one battery monitor chip
   while (1)
   {
      numbm = FindDevices (portnum, &famvolt[0], SBATTERY_FAM, MAXDEVICES);

      if (numbm == 0)
      {
         if (cnt > 10)
         {
            log_printf (LOG_WARNING, "No Temperature Monitor devices found");
            break;
         }
         else
         {
            cnt++;
         }
      }
      else
         break;
   }

   log_printf (LOG_INFO, "Found %d temperature monitor chip(s) OK", numbm);

   // search all DS2438 chips, categorise according to the volts on the Vad pin
   for (i = 0; i < numbm; i++)
   {
      Vad = ReadVad (portnum, famvolt[i]);
      if (debug)
         printf ("Read %2.2f volts on Vad pin\n", Vad);
      if (Vad < 1.0)
      {
         VI = i;                // tap volts == ground, using voltage (current) sensors
         if (debug)
            printf ("Current sensor on address  %s\n", getAddr (famvolt[i]));
      }
      else if (Vad < 3.0)
      {
         *T1 = i;               // tap volts == half supply, just using temp sensor
         if (debug)
            printf ("Temp sensor 1 on address  %s\n", getAddr (famvolt[i]));
      }
      else if (Vad > 4.5)
      {
         *T2 = i;               // tap volts ~= supply, must be the 3rd temp sensor
         if (debug)
            printf ("Temp sensor 2 on address %s\n", getAddr (famvolt[i]));
      }
   }

   if (VI < 0)
   {
      if (monitor)
      {
         log_printf (LOG_ERR, "Current monitor enabled but no current sensor found");
      }
      monitor = FALSE;
   }
   pthread_mutex_init (&onewire_mutex, NULL);

   return numgpio;
}


void general_reset(uint16_t numgpio)
{
   int16_t i;

   for (i = 0; i < numgpio; i++)
   {
      owAccessWrite (portnum, famsw[i], TRUE, 0xff);    // both high = off
      iocache[i] = 0xff;        // both outputs high (inactive)
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
         if (strncmp (chanmap[zone].address, getAddr (famsw[dev]), 16) == 0)
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
   owRelease (portnum);
}

uint16_t
GetCurrent (void)
{
   double i;

   if (VI < 0)
      return 0;
   pthread_mutex_lock (&onewire_mutex);
   i = ReadVad (portnum, famvolt[VI]) * VoltToMilliAmp;
   pthread_mutex_unlock (&onewire_mutex);
// can't read reliably below 1.5 volts (but need down to ~0.1V)
   if (i < 50)
      i = 0;

   return (uint16_t) i;
}

double
GetTemp(uint16_t index)
{
   double temp;

   pthread_mutex_lock (&onewire_mutex);
   temp = ReadTemp (portnum, famvolt[index]);
   pthread_mutex_unlock (&onewire_mutex);
   return temp;
}


void
setGPIOraw(uint8_t index, uint8_t value)
{
   pthread_mutex_lock (&onewire_mutex);
   owAccessWrite (portnum, famsw[index], TRUE, value);
   pthread_mutex_unlock (&onewire_mutex);
}

char *
getGPIOAddr (uint8_t index)
{
   return getAddr (famsw[index]);
}
   
//--------------------------------------------------------------------------
// function - DoOutput
// This routine sets either PIOA or PIOB according to the zone mapping
// in the chanmap array. It references and updates the iocache array to get
// the state of the other port on a device
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
   uint8_t ioval;

   pthread_mutex_lock (&onewire_mutex);
   if (((chanmap[zone].valid & HARDWARE) == 0) && (state != OFF))    // if no hardware and trying to switch on then fail
   {
      pthread_mutex_unlock (&onewire_mutex);
      return FALSE;
   }

   if (chanmap[zone].AorB)
   {
      // operating on PIOA - need current state of B with A bit cleared to activate it
      if (state == ON)
         ioval = iocache[chanmap[zone].dev] & 0xfe;
      else
         ioval = iocache[chanmap[zone].dev] | 0x01;
   }
   else
   {
      // operating on PIOB
      if (state == ON)
         ioval = iocache[chanmap[zone].dev] & 0xfd;
      else
         ioval = iocache[chanmap[zone].dev] | 0x02;
   }

   if (debug)
   {
      printf ("Device %s port %c state %02x ioval %02x cache %02x\n",
              getAddr (famsw[chanmap[zone].dev]),
              chanmap[zone].AorB ? 'A' : 'B', state, ioval, iocache[chanmap[zone].dev]);
   }

   ret = owAccessWrite (portnum, famsw[chanmap[zone].dev], TRUE, ioval);
   if (ret)                     // update cache and output state if write OK
   {
      iocache[chanmap[zone].dev] = ioval;
   }
   else
   {
      log_printf (LOG_ERR, "Failed to switch zone %d %s", zone, state ? "ON" : "OFF");
   }
   pthread_mutex_unlock (&onewire_mutex);
   return ret;
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

