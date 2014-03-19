//---------------------------------------------------------------------------
// Copyright (C) 2009-2014 Robin Gilks
//
//
//  irr_utils.c   -   This section looks after the various utility functions
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


#include "irrigate.h"



/**
 * Retrieve user input from the console.
 *
 * min  minimum number to accept
 * max  maximum number to accept
 *
 * @return numeric value entered from the console.
 */
int
getNumber (int min, int max)
{
   int value = min, cnt;
   bool done = FALSE;
   do
   {
      cnt = scanf ("%d", &value);
      if (cnt > 0 && (value > max || value < min))
      {
         printf ("Value (%d) is outside of the limits (%d,%d)\n", value, min, max);
         printf ("Try again:\n");
      }
      else
         done = TRUE;

   }
   while (!done);

   return value;
}


char *
getAddr (uint8_t * SNum)
{
   static char printbuf[200];
   sprintf (printbuf, "%02x%02x%02x%02x%02x%02x%02x%02x", SNum[0], SNum[1],
            SNum[2], SNum[3], SNum[4], SNum[5], SNum[6], SNum[7]);
   return printbuf;
}


// convert clock hours to time based on current seconds count
time_t
hours2time(uint16_t decahours)
{
   time_t starttime;
   struct tm tm;


   localtime_r (&basictime, &tm);
   tm.tm_hour = decahours / 100;
   tm.tm_min = decahours - (tm.tm_hour * 100); // might allow mins later
   tm.tm_sec = 0;
   starttime = mktime (&tm);

   return starttime;
}

// get expected solenoid current by adding up all the active valves
uint16_t get_expected_current(void)
{
   uint16_t current = 0;
   uint8_t zone;

   for (zone = 1; zone < REALZONES; zone++)
   {
      // we're only interested in zones that are active
      if ((chanmap[zone].output == ON) || (chanmap[zone].output == TEST))
         current += chanmap[zone].current;
   }

   return current;
}


// get expected flow rate by adding up all the active zones
// skip pumps
uint16_t get_expected_flow(void)
{
   uint16_t flow = 0;
   uint8_t zone;

   for (zone = 1; zone < REALZONES; zone++)
   {
      // we're only interested in zones that are active but not the well pump or any other feed
      if ((chanmap[zone].output == ON) && ((chanmap[zone].type & (ISPUMP | ISDPFEED)) == 0))
         flow += chanmap[zone].flow;
   }

   return flow;
}

// get maximum flow rate of the pumping system by finding the highest capacity pump value
uint16_t get_maximum_flow(void)
{

   static uint16_t maxflow = 0;
   uint8_t pump;

   if (maxflow == 0)
   {
      for (pump = 0; pump < MAXPUMPS; pump++)
      {
         // we're only interested in pumps
         if (pumpmap[pump].maxflow > maxflow)
         {
            maxflow = pumpmap[pump].maxflow;
            printf ("Flow %d\n", maxflow);
         }
      }
   }
   return maxflow;
}


// get the pump number fro mthe zone number
uint8_t get_pump_by_zone(uint8_t zone)
{

   uint8_t pump;

   for (pump = 1; pump < MAXPUMPS; pump++)
   {
      // we're only interested in pumps
      if (pumpmap[pump].zone == zone)
         return pump;
   }
   return 0;
}


