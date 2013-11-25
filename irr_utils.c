//---------------------------------------------------------------------------
// Copyright (C) 2009-2010 Robin Gilks
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
   boolean done = FALSE;
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

