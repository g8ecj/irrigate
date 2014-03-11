//---------------------------------------------------------------------------
// Copyright (C) 2009-2014 Robin Gilks
//
//
//  irr_queue.c   -   This section looks after the queuing functions related to the timers
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


TQ *tqhead = NULL;

// insert an object into the timer queue based on the time to start
void
insert (time_t starttime, uint8_t zone, uint8_t action)
{
   TQ *n, *p, *q;
   time_t t;
   uint8_t i, z, a;

   // if the event is already queued then don't duplicate it
   for (i = 0;;)
   {
      i = walk_queue (i, &z, &t, &a);
      if (i == 0)
         break;
      if ((zone == z) && (starttime == t) && (action == a))
         return;
   }

   /* create a new node */
   n = (TQ *) malloc (sizeof (TQ));
   /* save data into new node */
   n->zone = zone;
   n->starttime = starttime;
   n->action = action;

   /* first, we handle the case where `data' should be the first element */
   if (tqhead == NULL || tqhead->starttime > starttime)
   {
      /* apparently this !IS! the first element */
      /* now data should [be|becomes] the first element */
      n->next = tqhead;
      tqhead = n;
      return;
   }

   /* search the linked list for the right location */
   p = tqhead;
   q = tqhead->next;
   while (q != NULL)
   {
      if (q->starttime > starttime)
      {
         p->next = n;
         n->next = q;
         return;
      }
      else
      {
         p = q;
         q = q->next;
      }
   }
   p->next = n;
   n->next = q;
   return;
}


// get the time of next expiry of a zone on the queue
time_t
findonqueue (uint8_t zone)
{
   TQ *q;

   for (q = tqhead; q != NULL; q = q->next)
   {
      if (q->zone == zone)      // found the correct node?
      {
         return q->starttime;    // return the time it will expire
      }
   }
   return 0;
}

bool
delete (uint8_t zone)
{
   TQ *p = NULL;
   TQ *q;

   for (q = tqhead; q != NULL; p = q, q = q->next)
   {
      if (q->zone == zone)      // found the correct node?
      {
         if (p == NULL)         // case of 1st pass thru the loop so we are going to delete the head
         {
            //no previous so set it
            p = q->next;
         }
         else
         {
            p->next = q->next;  // skip over the one we are deleting
            p = tqhead;
         }
         free (q);
         tqhead = p;
         return TRUE;
      }
   }
   return FALSE;                // not found 
}

uint8_t
walk_queue(uint8_t index, uint8_t * zone, time_t * starttime, uint8_t *action)
{
   TQ *p;
   uint8_t ret = index + 1;

   p = tqhead;
   while ((p != NULL) && (index-- > 0))
      p = p->next;

   if (p != NULL)
   {
      if (zone != NULL)
         *zone = p->zone;
      if (starttime != NULL)
         *starttime = p->starttime;
      if (action != NULL)
         *action = p->action;
   }
   else
      ret = 0;
   return ret;

}

void
print_queue (void)
{
   struct tm tm;
   char actions[][9] = {
      "None",
      "Turn ON",
      "Turn OFF",
      "Cancel",
      "Test ON",
      "Test OFF",
      "Unlock"
   };
   uint8_t index, zone, action;
   time_t starttime;

   for(index = 0;;)
   {
      index = walk_queue(index, &zone, &starttime, &action);
      if (index == 0)
         break;
      localtime_r (&starttime, &tm);
      log_printf (LOG_DEBUG, "Queue: zone %d, %s %02d:%02d:%02d (%lu), period %d, action = %s\n", zone,
              daystr[tm.tm_wday], tm.tm_hour, tm.tm_min, tm.tm_sec, chanmap[zone].starttime, chanmap[zone].period, actions[action]);
   };

}


uint8_t
dequeuezone (uint8_t * action)
{
   uint8_t zone = 0;
   if (tqhead && tqhead->starttime <= basictime)
   {
      zone = tqhead->zone;
      *action = tqhead->action;
   }
   return zone;
}

