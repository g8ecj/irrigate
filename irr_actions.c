//---------------------------------------------------------------------------
// Copyright (C) 2009-2014 Robin Gilks
//
//
//  irr_actions.c   -   This section looks after the actions that occur to start and stop watering
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


void
doaction (uint8_t zone, uint8_t action)
{
   double flow;

   switch (action)
   {
   case TURNON:
      // switch on and make another entry for the off time
      if (SetOutput (zone, ON))
      {
         insert (basictime + chanmap[zone].period, zone, TURNOFF);
         chanmap[zone].starttime = basictime;   // make sure start time reflects what the zone is actually doing, not what is queued
         chanmap[zone].state = ACTIVE;
         log_printf (LOG_NOTICE, "switch ON zone %d (%s) for %d minutes", zone, chanmap[zone].name, chanmap[zone].period / 60);
         chanmap[zone].actualstart = basictime;
         chanmap[zone].lastrun = basictime;
      }
      else
      {
         chanmap[zone].state = ERROR;   // report the error to the GUI
         log_printf (LOG_ERR, "switch ON *failed* in zone %d (%s) - switching OFF", zone, chanmap[zone].name);
         while (delete (zone));         // remove any further events on this zone
         chanmap[zone].locked = TRUE;
         insert (basictime + FAILED_RETRY, zone, UNLOCK);
         chanmap[zone].actualstart = basictime;
         write_history (zone, basictime + 1, basictime, WASFAIL);
      }
      break;

   case TURNOFF:
      // switch off
      flow = (basictime - chanmap[zone].actualstart) * chanmap[zone].flow / 60;   // convert secs to mins * l/min
      chanmap[zone].totalflow += (flow / 1000);   // add up the number of cubic metres
      chanmap[zone].lastdur = basictime - chanmap[zone].actualstart;
      if (SetOutput (zone, OFF))
      {
         chanmap[zone].state = IDLE;
         // if there is another entry on the queue then set the start time for it
         // otherwise we'd use the old (incorrect) time
         chanmap[zone].starttime = findonqueue (zone);

         log_printf (LOG_NOTICE, "switch OFF zone %d (%s)", zone, chanmap[zone].name);
         write_history (zone, basictime, chanmap[zone].actualstart, WASOK);
      }
      else
      {
         chanmap[zone].state = ERROR;
         write_history (zone, basictime, chanmap[zone].actualstart, WASFAIL);
      }
      break;

   case CANCEL:                // used from the event queue to cancel zones
      if ((chanmap[zone].output == ON) || (chanmap[zone].output == TEST))       // attempt switchoff if active 
      {
         chanmap[zone].lastdur = basictime - chanmap[zone].actualstart;
         flow = (basictime - chanmap[zone].actualstart) * chanmap[zone].flow / 60;        // convert secs to mins * l/min
         chanmap[zone].totalflow += (flow / 1000);      // add up the number of cubic metres
         SetOutput (zone, OFF);
         log_printf (LOG_WARNING, "switch OFF (cancel) zone %d (%s)", zone, chanmap[zone].name);
         write_history (zone, basictime, chanmap[zone].actualstart, WASCANCEL);
      }
      break;

   case TESTON:
      // switch on and make another entry for the off time
      if (SetOutput (zone, TEST))
      {
         insert (basictime + chanmap[zone].period, zone, TESTOFF);
         chanmap[zone].starttime = basictime;   // make sure start time reflects what the zone is actually doing, not what is queued
         chanmap[zone].state = ACTIVE;
         log_printf (LOG_NOTICE, "testing zone %d (%s)", zone, chanmap[zone].name);
         chanmap[zone].actualstart = basictime;
         chanmap[zone].lastrun = basictime;
      }
      else
      {
         chanmap[zone].state = ERROR;   // report the error to the GUI
         log_printf (LOG_ERR, "test on zone %d (%s) *failed* - switching OFF", zone, chanmap[zone].name);
         chanmap[zone].actualstart = basictime;
         write_history (zone, basictime + 1, basictime, WASFAIL);
      }
      break;

   case TESTOFF:
      // switch off from test mode, don't update flows or stats, should never repeat (one shot!!)
      if (SetOutput (zone, OFF))
      {
         chanmap[zone].state = IDLE;
         log_printf (LOG_NOTICE, "test complete OK on zone %d (%s)", zone, chanmap[zone].name);
         write_history (zone, basictime, chanmap[zone].actualstart, WASOK);
      }
      else
      {
         chanmap[zone].state = ERROR;
         write_history (zone, basictime, chanmap[zone].actualstart, WASFAIL);
      }

      break;

   case PUMPOFF:
      pump_off(zone);
      break;

   case UNLOCK:
      chanmap[zone].locked = FALSE;
      log_printf (LOG_NOTICE, "UNLOCK %s", chanmap[zone].name);
      break;

   default:
      break;
   }
}


// clear all aspects of a zone - its timer queue entries etc
// always try and set output off, if it was active then adjust flow and log it
// do this by queueing a CANCEL event that gets executed in the main thread
void
zone_cancel (uint8_t zone, uint8_t state)
{

   while (delete (zone));
   insert (1, zone, CANCEL);                    // ensure it goes to the front of the time queue
   chanmap[zone].state = state;
   chanmap[zone].frequency = 0;                 // ensure it never repeats
   chanmap[zone].starttime = 0;                 // and that it gets removed from the schedule on the next check_schedule
}


/* load all zones with an action to test the solenoid continuity (except the well pump) */
void
dotest (uint8_t testzone, uint8_t action)
{

   uint8_t zone;
   time_t start = basictime + 2;    // start after the reset has occured

   if (action == TURNOFF)           // use this to toggle the display status
   {
      chanmap[testzone].state = IDLE;
      if ((findonqueue (testzone) == 0) && (chanmap[testzone].frequency == 0))
      {
         chanmap[testzone].starttime = 0;
      }
      return;
   }
   else if (action == CANCEL)
   {
      test_cancel (testzone);
      return;
   }

   // the starttime has already passed so set it to NOW
   chanmap[testzone].starttime = basictime;


   for (zone = 1; zone < REALZONES; zone++)
   {
      if ((chanmap[zone].type & ISPUMP) != 0)
      {
         if (chanmap[zone].state == ACTIVE)
            pump_off(get_pump_by_zone(zone));
         while (delete (zone));                          // remove the queued up unlock command
         chanmap[zone].locked = TRUE;
      }
      // already know its not a pump, check for group, is valid and has a flow associated with it (i.e. not a spare)
      else if (((chanmap[zone].type & (ISGROUP | ISTEST)) == 0) && (chanmap[zone].valid) && (chanmap[zone].flow > 0))
      {
         chanmap[zone].duration = chanmap[testzone].period;
         chanmap[zone].period = chanmap[testzone].period;
         insert (start, zone, TESTON);
         chanmap[zone].starttime = start;
         start += chanmap[testzone].period;
      }
   }
   chanmap[testzone].state = ACTIVE;                     // say we're active
   chanmap[testzone].period = start - chanmap[testzone].starttime;
   insert (start, testzone, TURNOFF);                    // switch off display at the end

   for (zone = 1; zone < REALZONES; zone++)
   {
      if ((chanmap[zone].type & ISPUMP) != 0)
      {
         chanmap[zone].starttime = basictime;            // have a base time to know when unlock occurs
         insert (start, zone, UNLOCK);                   // then allow well to start again
         chanmap[zone].period = chanmap[testzone].period;
      }
   }

}

/* check for any zones having a test on/off command queued and any found then reset everything */
void
test_cancel (uint8_t testzone)
{
   uint8_t zone;

   // TODO - scan for existing test commands
   while (delete (testzone));
   emergency_off(IDLE);
   chanmap[testzone].state = IDLE;
   chanmap[testzone].starttime = 0;
   chanmap[testzone].frequency = 0;
   // handle unlocking of the pumps
   for (zone = 1; zone < REALZONES; zone++)
   {
      if ((chanmap[zone].type & ISPUMP) != 0)
         pump_off(zone);
   }

}



/* load up 'ISFROST' zones to run in a 'round-robin' with 1 minute duration with 1 second overlap */
void
dofrost (void)
{

   uint8_t zone;
   time_t start = basictime;

   for (zone = 1; zone < REALZONES; zone++)
   {
      if (chanmap[zone].type & ISFROST)
      {
         chanmap[zone].duration = FROST_TIME;   // used by status display
         chanmap[zone].period = FROST_TIME;
         insert (start, zone, TURNON);
         chanmap[zone].starttime = start;
         start += FROST_TIME - VALVE_OVERLAP;
      }
   }
   insert (start, FROST_LOAD, NOACTION);
}

/* cancel the frost protect program by removing the virtual zone that keeps it going and remove any currently active */
void
frost_cancel (void)
{
   uint8_t zone;
   uint8_t group = 0;

   while (delete (FROST_LOAD)); // stop it firing again

   for (zone = 1; zone < REALZONES; zone++)
   {
      if (chanmap[zone].type & ISFROST)
      {
         zone_cancel (zone, IDLE);
         if (chanmap[zone].group)       // see if a member of a group
            group = chanmap[zone].group;

         if (group)                   // cancel the whole group if one is a member
         {
            for (zone = 1; zone < REALZONES; zone++)
            {
               // we're only interested in zones in this group
               if ((chanmap[zone].group == group) && (chanmap[zone].type & ISGROUP))
               {
                  group_cancel (zone, IDLE);
                  break;
               }
            }
         }
      }
   }
}


/* load up all the zones in a group to run required duration starting at defined time but load up pump to max flow */
void
dogroup (uint8_t group, uint8_t action)
{
   uint8_t zone, pass, numpass, z = 0;
   uint16_t newflow = 0, flow = 0, pumpnomflow;
   time_t start;

   if (action == TURNOFF)       // use this to toggle the display status
   {
      chanmap[group].state = IDLE;
      if ((findonqueue (group) == 0) && (chanmap[group].frequency == 0))
      {
         chanmap[group].starttime = 0;
      }
      return;
   }
   else if (action == CANCEL)
   {
      log_printf (LOG_WARNING, "Unexpected group cancel event");
      group_cancel (group, ERROR);
      return;
   }

   pumpnomflow = get_nominal_flow();

   // the starttime has already passed so set it to NOW
   chanmap[group].starttime = basictime;

   // scan for all group members, get an example zone and the total flow for the group
   for (zone = 1; zone < REALZONES; zone++)
   {
      // we're only interested in zones in this group
      if ((chanmap[zone].group & chanmap[group].group) && ((chanmap[zone].type & ISGROUP) == 0))
      {
         z = zone;              // save one of the zones
         newflow += chanmap[zone].flow;
      }
   }

   start = chanmap[group].starttime;

   // if the total flow rate is less than the pump output then no need for multiple passes
   // we assume that all zones in a group have the same flow rate
   if (newflow < pumpnomflow)
   {
      numpass = 1;
   }
   else
   {
      numpass = pumpnomflow / chanmap[z].flow;    // how many zones can run simultaneously
   }

   for (pass = 0; pass < numpass; pass++)
   {
      for (zone = 1; zone < REALZONES; zone++)
      {
         // we're only interested in zones in this group
         if ((chanmap[zone].group & chanmap[group].group) && ((chanmap[zone].type & ISGROUP) == 0))
         {
            // if this zone will tip us over the max flow of the pump then schedule ahead
            if ((flow + chanmap[zone].flow) > pumpnomflow)
            {
               start += (chanmap[group].duration / numpass);
               flow = 0;
            }
            chanmap[zone].period = (chanmap[group].duration / numpass) + 1;     // overlap so pump doesn't restart
            chanmap[zone].duration = (chanmap[group].duration / numpass) + 1;   // used by the status display so keep up to date
            insert (start, zone, TURNON);
            chanmap[zone].frequency = 0;
            flow += chanmap[zone].flow;
            if (pass == 0)
               chanmap[zone].starttime = start; // only set the start time on the first pass otherwise we overwrite it!!
         }
      }
   }
   chanmap[group].state = ACTIVE;       // say we're active
   chanmap[group].period = start - chanmap[group].starttime + (chanmap[group].duration / numpass);
   insert (basictime + chanmap[group].period, group, TURNOFF);  // switch off display at the end
}


// cancel all the zones in a group and the virtual channel controlling them
void
group_cancel (uint8_t group, uint8_t state)
{
   uint8_t zone;

   while (delete (group));
   for (zone = 1; zone < REALZONES; zone++)
   {
      // we're only interested in zones in this group
      if ((chanmap[zone].group & chanmap[group].group) && ((chanmap[zone].type & ISGROUP) == 0))
      {
         zone_cancel (zone, state);
      }
   }
   chanmap[group].state = state;
   chanmap[group].frequency = 0;
   chanmap[group].starttime = 0;
}

void
dopumps (void)
{
// scan through the list of pumps looking for one that suits the current flow rate
// having found it, make sure its allowed to run at the current time
// if not allowed by time then ignore it
// if no pumps running by the end of the scan look for a pump that has the ISSTOCK attribute
//   and turn it on if the time limits allow it

   uint16_t totalflow = get_expected_flow();
   uint8_t pump;

   // scan the list of pumps
   for (pump = 0; pumpmap[pump].zone; pump++)
   {
      // see if within the range of operation of this pump
      if ((totalflow >= pumpmap[pump].minflow) && (totalflow <= pumpmap[pump].maxflow) && (chanmap[pumpmap[pump].zone].locked == FALSE))
      {
         pump_on(pumpmap[pump].zone);
      }
      // stock water flag is only meaningful if there is no other demand
      else if ((chanmap[pumpmap[pump].zone].type & ISSTOCK) && (totalflow == 0))
      {
         // see if we are allowed to run it at this time of day
         if ((basictime > hours2time (pumpmap[pump].start)) && (basictime < hours2time (pumpmap[pump].end)))
         {
            pump_on(pumpmap[pump].zone);
         }
         else
         {
            pump_off(pumpmap[pump].zone);
         }
      }
      else
      {
         pump_off(pumpmap[pump].zone);
      }
   }

}


// switch on the pump, update the state
void
pump_on (uint8_t zone)
{
   uint8_t pump = get_pump_by_zone(zone);

   if ((chanmap[zone].locked) || (chanmap[zone].state == ACTIVE))
      return;                   // belt and braces

   if (SetOutput (zone, ON))
   {
      chanmap[zone].state = ACTIVE;
      chanmap[zone].duration = 0;
      chanmap[zone].period = 0; // we don't really know the end time
      chanmap[zone].starttime = basictime;      // say it starts now!!
      log_printf (LOG_NOTICE, "switch ON %s", chanmap[zone].name);
      chanmap[zone].actualstart = basictime;
      chanmap[zone].lastrun = basictime;
      insert (basictime + pumpmap[pump].maxrun, zone, PUMPOFF);
   }
   else
   {
      chanmap[zone].state = ERROR;      // report the error to the GUI
      write_history (zone, basictime + 1, basictime, WASFAIL);
      // set lock if failure to prevent retries happening too fast
      chanmap[zone].locked = TRUE;
      chanmap[zone].starttime = basictime;
      chanmap[zone].period = FAILED_RETRY;
      chanmap[zone].duration = FAILED_RETRY;
      insert (basictime + FAILED_RETRY, zone, UNLOCK);
   }
}

// switch off the pump, update the state and say the zone is locked (busy)
// queue an unlock for later
void
pump_off (uint8_t zone)
{
   uint16_t locktime;
   uint8_t pump = get_pump_by_zone(zone);

   if (chanmap[zone].state == ACTIVE)
   {
      if (SetOutput (zone, OFF))
      {
         pumpmap[pump].pumpingtime += (basictime - chanmap[zone].actualstart);
         chanmap[zone].totalflow = 0;      // don't record pump flow, its the time that is important
         log_printf (LOG_NOTICE, "switch OFF %s and LOCK", chanmap[zone].name);
         write_history (zone, basictime, chanmap[zone].actualstart, WASOK);
      }
      else
      {
         chanmap[zone].state = ERROR;
         write_history (zone, basictime, chanmap[zone].actualstart, WASFAIL);
      }
      while (delete (zone));                // remove any outstanding 'off' action
      chanmap[zone].lastdur = basictime - chanmap[zone].actualstart;
      chanmap[zone].state = IDLE;
      chanmap[zone].locked = TRUE;
      chanmap[zone].starttime = basictime;
   }
   locktime = 3600 / pumpmap[pump].maxstarts;   // convert starts per hour to lockout time
   chanmap[zone].period = locktime;
   chanmap[zone].duration = locktime;
   // if its locked and an unlock hasn't yet been queueud then do one now
   if ((chanmap[zone].locked) && (findonqueue(zone) == 0))
      insert (basictime + locktime, zone, UNLOCK);
}




// switch off all active zones and put into error state
void
emergency_off (uint8_t newstate)
{
   uint8_t zone;

   for (zone = 1; zone < REALZONES; zone++)
   {
      // if something queued up then cancel it before we do owt else with the queue
      if (findonqueue (zone) > 0)
         delete (zone);

      // if a pump then do the unlock timing etc
      if ((chanmap[zone].type & ISPUMP) != 0)
         pump_off(zone);

      // if switched on and got real hardware here then switch off 
      else if (chanmap[zone].state == ACTIVE)
      {
         if (chanmap[zone].type & ISGROUP)
         {
            group_cancel (zone, newstate);
         }
         else
         {
            zone_cancel (zone, newstate);  // all those switched due to error condition mark as such
         }
      }
   }
   // schedule a general reset AFTER we have tried to clean things up
   insert (basictime + 1, RESET, NOACTION);

}


void
doreset(void)
{
   uint8_t zone;

   // belt and braces - hardware cleardown and ensure the pump manager doesn't go off
   general_reset ();
   for (zone = 1; zone < REALZONES; zone++)
      chanmap[zone].output = OFF;
   log_printf (LOG_NOTICE, "Done general reset");

}

