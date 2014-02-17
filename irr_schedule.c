//---------------------------------------------------------------------------
// Copyright (C) 2009-2010 Robin Gilks
//
//
//  irr_schedule.c   -   This section looks after the current and forward schedule and maintains it in a file
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


/*
  Each time the schedule is updated, dump the current state to a file in case
  we get a restart and we need to pick up where we left off
  */
bool
write_schedule (void)
{
   uint8_t zone;
   FILE *fd;
   struct json_object *jobj;
   char schedulefile[MAXFILELEN];

   strcpy (schedulefile, datapath);
   strncat (schedulefile, "/schedule", 9);
   fd = fopen (schedulefile, "w");
   if (fd == NULL)
      return FALSE;

   for (zone = 1; zone < REALZONES; zone++)
   {
      if ((chanmap[zone].valid) && (chanmap[zone].useful))      // valid zone that has useful data in it
      {
         jobj = json_object_new_object ();
         json_object_object_add (jobj, "zone", json_object_new_int (chanmap[zone].zone));
         json_object_object_add (jobj, "name", json_object_new_string (chanmap[zone].name));
         json_object_object_add (jobj, "starttime", json_object_new_int (chanmap[zone].starttime));
         json_object_object_add (jobj, "duration", json_object_new_int (chanmap[zone].duration));
         json_object_object_add (jobj, "frequency", json_object_new_int (chanmap[zone].frequency));
         fputs (json_object_to_json_string (jobj), fd);
         fputc ('\n', fd);
         json_object_put (jobj);
      }
   }
   if (frost_armed)
   {
      jobj = json_object_new_object ();
      json_object_object_add (jobj, "frost_armed", json_object_new_boolean (TRUE));
      fputs (json_object_to_json_string (jobj), fd);
      fputc ('\n', fd);
      json_object_put (jobj);
   }
   fclose (fd);
   return TRUE;
}


/*
  Read the current scheduling data from a file and populate the channel map
  Used at program startup to recover data
  */
bool
read_schedule (void)
{
   uint8_t zone;
   bool ret = FALSE;
   FILE *fd;
   struct json_object *jobj;
   char schedulefile[MAXFILELEN];
   char *input;

   strcpy (schedulefile, datapath);
   strncat (schedulefile, "/schedule", 9);
   fd = fopen (schedulefile, "r");
   if (fd == NULL)
      return FALSE;
   input = malloc (1024);
   while (fgets (input, 1024, fd) != NULL)
   {
      jobj = json_tokener_parse (input);
      if (is_error (jobj))
      {
         printf ("error parsing json object %s\n", input);
      }
      else
      {
         zone = json_object_get_int (json_object_object_get (jobj, "zone"));
         if (zone > 0)
         {
            chanmap[zone].starttime = json_object_get_int (json_object_object_get (jobj, "starttime"));
            chanmap[zone].duration = json_object_get_int (json_object_object_get (jobj, "duration"));
            chanmap[zone].frequency = json_object_get_int (json_object_object_get (jobj, "frequency"));
            chanmap[zone].useful = TRUE;        // assume useful until I try a check_schedule
            chanmap[zone].period = chanmap[zone].duration;
         }
         else if (json_object_get_boolean (json_object_object_get (jobj, "frost_armed")))
            frost_armed = TRUE;

         json_object_put (jobj);
         ret = TRUE;            // got something useful
      }
   }
   free (input);
   fclose (fd);

   if (debug > 1)
      print_chanmap ();
   return ret;
}

/*
  Iterate the chanmap and insert items into the time queue that haven't yet expired
  This function will only schedule items up to 24 hours ahead so it should be called 
  at least once a day as well as on startup
  */
void
check_schedule (bool changes)
{
   uint8_t zone, updatestart;
   int32_t timediff;
   time_t future;


   for (zone = 1; zone < REALZONES; zone++)
   {
      chanmap[zone].period = chanmap[zone].duration;    // assume we'll be doing something with this zone
      // first check the case where everything sums to be in the past and not repeating
      if (((chanmap[zone].starttime + chanmap[zone].duration) < basictime) && (chanmap[zone].frequency == 0))
      {
         if (chanmap[zone].useful)      // if it was in the schedule
            changes = TRUE;     // its gone from it now
         chanmap[zone].useful = FALSE;
         chanmap[zone].frequency = 0;
         continue;
      }

      if (chanmap[zone].useful)
      {
         future = chanmap[zone].starttime;
         updatestart = 0;
         while (future < (basictime + (60 * 60 * 24 * 8)))      // within the next week
         {
            if ((future + chanmap[zone].duration) >= basictime) // see if this instance is now or in the future
            {
               timediff = (future + chanmap[zone].frequency) - basictime;
               // adjust starttime that might be in the past (but only once!!)
               if ((timediff > 0) && (updatestart == 0))
               {
                  chanmap[zone].starttime = future;
                  updatestart++;
               }

               timediff = basictime - future;
               if ((timediff > 0) && (chanmap[zone].state != ACTIVE))   // start time has already passed and not yet started
               {
                  chanmap[zone].period -= timediff;
               }

               if ((chanmap[zone].state != ACTIVE) || (findonqueue (zone) == 0)) // not started or queued yet
               {
                  insert (future, zone, TURNON);
                  changes = TRUE;       // always update starttime value in schedule file if its repeating
               }
            }
            future += chanmap[zone].frequency;
            if (chanmap[zone].frequency == 0)
               break;
         }
      }

   }
   if (changes)
      write_schedule ();        // any changes get written to schedule file
}
