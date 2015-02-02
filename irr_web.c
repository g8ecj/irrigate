//---------------------------------------------------------------------------
// Copyright (C) 2009-2014 Robin Gilks
//
//
//  irr_web.c   -   This section looks after the interface to the mongoose threaded web server
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


struct mg_server *server;

const char *fmt = "%a, %d %b %Y %H:%M:%S";

static void
send_headers (struct mg_connection *conn)
{
   mg_send_header (conn, "Cache", "no-cache");
   mg_send_header (conn, "Content-Type", "application/x-javascript");
}

static int
refuse (struct mg_connection UNUSED(*conn))
{
   return MG_FALSE;
}

static int
check_authorised(struct mg_connection *conn)
{
   int result = MG_FALSE; // Not authorized
   FILE *fp;
   char pass_file[MAXFILELEN];

   strcpy (pass_file, datapath);
   strncat (pass_file, "/passfile", 10);
    // To populate passwords file, do
    // mongoose -A my_passwords.txt mydomain.com admin admin
   if ((fp = fopen(pass_file, "r")) != NULL) {
      result = mg_authorize_digest(conn, fp);
      fclose(fp);
   }

    return result;
}



/*
 *       webmsg(conn, p->time, p->pri, textpri(p->pri), p->log);
*/

void
sendjsonmsg (struct mg_connection *conn, time_t time, int UNUSED (priority), char * UNUSED (desc), char *msg)
{
   char timestr[64];
   struct json_object *jobj;

//   ctime_r (&time, timestr);
   strftime(timestr, sizeof(timestr), fmt, localtime(&time));

   jobj = json_object_new_object ();
   json_object_object_add (jobj, "Time", json_object_new_string (timestr));
   json_object_object_add (jobj, "-----", json_object_new_string (" "));
   json_object_object_add (jobj, "Log", json_object_new_string (msg));

   mg_printf_data (conn, "%s, ", json_object_to_json_string (jobj));
   json_object_put (jobj);

}

static int
show_jsonlogs (struct mg_connection *conn)
{
   send_headers (conn);

   mg_printf_data (conn, "%s", "{ \"logs\": [ ");

   send_log_msgs (conn, sendjsonmsg);

   mg_printf_data (conn, "%s", " { } ] }");
   return MG_TRUE;
}

void
sendmsg (struct mg_connection *conn, time_t time, int UNUSED (priority), char *desc, char *msg)
{
   char timestr[64];

//   ctime_r (&time, timestr);
   strftime(timestr, sizeof(timestr), fmt, localtime(&time));
   mg_printf_data (conn, "%s irrigate %s: %s<br>", timestr, desc, msg);

}

static int
show_logs (struct mg_connection *conn)
{
   mg_send_header (conn, "Content-Type", "text/html");

   send_log_msgs (conn, sendmsg);
   return MG_TRUE;

}

/*
To track past events we need to populate a dummy chanmap with the following values taken at the time
the event is logged

state IDLE/ERROR
frequency
starttime
period

Should be able to then create everything the same as a future event plus show errors
Errors also have a code to indicate what failed

*/


// create a json object for a zone by decoding the various attributes
struct json_object *
create_json_zone (uint8_t zone, time_t starttime, struct mapstruct *cmap)
{
   struct json_object *jobj;
   struct tm tm;
   time_t t;
   time_t duration;
   uint8_t i, j;

   char descstr[140];
   char startstr[64];
   char endstr[64];
   char ending_ed[64];
   char ing_ed[5];
   char is_was[5];
   char fromday[64] = {0};
   char tmpstr[30] = {0};
   char rptstr[30] = {0};
   char errstr[30] = {0};
   char durstr[30] = {0};

   if (cmap->state == ACTIVE)
   {
      if (starttime > basictime)           // if this instance is in the future then its queued
         strncpy (tmpstr, "queued", 7);
      else
         strncpy (tmpstr, "active", 7);
   }
   else if (cmap->state == ERROR)
   {
      strncpy (tmpstr, "error", 6);
   }
   else if (cmap->state == WASOK)
   {
      strncpy (tmpstr, "completed", 10);
   }
   else if (cmap->state == WASCANCEL)
   {
      strncpy (tmpstr, "cancelled", 10);
   }
   else if (cmap->state == WASFAIL)
   {
      strncpy (tmpstr, "failed", 7);
      switch (cmap->lasterrno)
      {
      case EIO:
         strncpy (errstr, " [Driver failure] ", 19);
         break;
      case ERANGE:
         strncpy (errstr, " [Valve current] ", 18);
         break;
      case ENOTSUP:
         strncpy (errstr, " [Invalid command] ", 20);
         break;
      }
   }
   else if (cmap->frequency > 0)
   {
      strncpy (tmpstr, "repeating", 10);
   }
   else if (cmap->frequency < 0)
   {
      for (i = 0, j = 0; i < 7; i++)
         if (cmap->daylist[i]) j++;
      sprintf (tmpstr, "repeating %d days a week", j);
   }
   else if (cmap->locked)
   {
      strncpy (tmpstr, "locked", 7);
   }
   else if (findonqueue (zone) > 0)
   {
      strncpy (tmpstr, "queued", 7);
   }
   else
   {
      strncpy (tmpstr, "idle", 5);
   }

   switch (cmap->frequency / (60 * 60)) // convert secs to hours as that is what the user inputs
   {
   case 6:
      strncpy (rptstr, "4 times daily ", 15);
      break;
   case 12:
      strncpy (rptstr, "twice daily ", 13);
      break;
   case 24:
      strncpy (rptstr, "daily ", 7);
      break;
   case 48:
      strncpy (rptstr, "every 2 days ", 14);
      break;
   case 72:
      strncpy (rptstr, "every 3 days ", 14);
      break;
   case 96:
      strncpy (rptstr, "every 4 days ", 14);
      break;
   case 168:
      strncpy (rptstr, "weekly ", 8);
      break;
   }

   if (cmap->duration > 0)
   {
      duration = cmap->period;
      if (cmap->duration > 5400)
         sprintf(durstr, "of %.1f hours", duration / 3600.0);
      else
         sprintf(durstr, "of %d minutes", (int)duration / 60);
      t = starttime + duration;
   }
   else
   {
      duration = abs(basictime - starttime);
      sprintf(durstr, "less than %d minutes", (int)duration / 60);
      t = basictime;
   }

   strftime(startstr, sizeof(startstr), fmt, localtime(&starttime));
   strftime(endstr, sizeof(endstr), fmt, localtime(&t));

   localtime_r (&t, &tm);
   if (t < basictime)
      sprintf (ending_ed, "ended at %02d%02d", tm.tm_hour, tm.tm_min);
   else
      sprintf (ending_ed, "ending at %02d%02d", tm.tm_hour, tm.tm_min);


   if (starttime < basictime)
      strncpy (ing_ed, "ed", 3);
   else
      strncpy (ing_ed, "ing", 4);


   // default to not having day
   if (((starttime > basictime) && (starttime < (basictime + 60 * 60 * 24))) 
            || ((starttime + cmap->frequency > basictime) && (starttime + cmap->frequency < (basictime + 60 * 60 * 24))))        // start OR repeat is today
   {
//      fromday[0] = '\0';
   }
   else if (starttime > (basictime + 60 * 60 * 24)) // start more than 24hrs ahead - always have day
   {
      localtime_r (&starttime, &tm);
      sprintf(fromday, "from %s", daystr[tm.tm_wday]);     // use starttime to find day
   }
   else if (starttime + cmap->frequency > (basictime + 60 * 60 * 24))  // repeat sometime more than 24hrs ahead
   {
      t = starttime + cmap->frequency;
      localtime_r (&t, &tm);
      sprintf(fromday, "from %s", daystr[tm.tm_wday]);     // use starttime to find day
   }

   // description is 
   // start{ing/ed}(1) at <time> for a duration of <period> and {is/was}(2) <state>  {and is repeating/""}(3) <rptstr> {from <daystr>}(4)
   // (1) start time < basictime
   // (2) start time + period < basictime
   // (3) frequency > 0
   // (4) infuture, not today

   if ((starttime + cmap->period < basictime) && (cmap->state != ACTIVE))
      strncpy (is_was, "was", 4);
   else
      strncpy (is_was, "is", 3);

   if (strncmp(tmpstr, "idle", 5) == 0)
      sprintf (descstr, "%s - nothing scheduled", cmap->name);
   else if ((cmap->locked) && (cmap->state != ACTIVE))
   {
      t = starttime + cmap->period;
      localtime_r (&t, &tm);
      sprintf (descstr, "%s - locked until %02d%02d", cmap->name, tm.tm_hour, tm.tm_min);
   }
   else
   {
      // note that the following relies on initial null termination of the strings.
      localtime_r (&starttime, &tm);
      sprintf (descstr, "%s - start%s at %02d%02d for a duration %s, %s and %s %s %s%s %s", 
         cmap->name, ing_ed, tm.tm_hour, tm.tm_min, durstr, ending_ed, is_was, tmpstr, errstr, rptstr, fromday);
   }

   jobj = json_object_new_object ();

   json_object_object_add (jobj, "zone", json_object_new_int (zone));
   json_object_object_add (jobj, "status", json_object_new_string (tmpstr));
   json_object_object_add (jobj, "duration", json_object_new_int (duration));
   json_object_object_add (jobj, "start", json_object_new_string (startstr));
   json_object_object_add (jobj, "end", json_object_new_string (endstr));
   json_object_object_add (jobj, "title", json_object_new_string (cmap->name));
   json_object_object_add (jobj, "description", json_object_new_string (descstr));

   return jobj;
}



/*
 * This callback function is attached to the URI "/status"
 * It returns a json object with all the status information
 */
static int
show_status (struct mg_connection *conn)
{
   uint8_t zone;
   struct json_object *jobj, *jzones, *jfrost;
   char str1[80], str2[80];
   time_t uptime = basictime - startuptime;
   struct tm tm;

   send_headers(conn);

   jzones = json_object_new_array ();

   for (zone = 1; zone < REALZONES; zone++)
   {
      // only report status of valid zones - ignore sensors
      if ((chanmap[zone].valid) && ((chanmap[zone].type & ISSENSOR) == 0))
      {

         jobj = create_json_zone (zone, chanmap[zone].starttime, &chanmap[zone]);
         json_object_array_add (jzones, jobj);
      }

   }
   jobj = json_object_new_object ();
   json_object_object_add (jobj, "cmd", json_object_new_string ("status"));
   localtime_r (&basictime, &tm);
   sprintf (str1, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
   json_object_object_add (jobj, "time", json_object_new_string (str1));

   sprintf (str1, "Version %s compiled on %s at %s", VERSION, __DATE__, __TIME__);
   json_object_object_add (jobj, "version", json_object_new_string (str1));

   json_object_object_add (jobj, "flow", json_object_new_int (get_expected_flow()));

   // frost can be on (manual or cold!), armed or off - 4 different outcomes
   // might need to use Tintegral here (or equiv)
   jfrost = json_object_new_object ();
   switch (frost_mode)
   {
   case FROST_MANUAL:
      strcpy (str1, "man");
      strcpy (str2, "Frost Protect - activated by manual override");
      break;
   case FROST_ACTIVE:
      strcpy (str1, "on");
      strcpy (str2, "Frost Protect - activated by low temperature");
      break;
   case FROST_OFF:
      if (frost_armed)
         if (Tintegral > 0)
         {
            strcpy (str1, "run");
            sprintf(str2, "Frost Protect - triggered - %2d/%2d degree minutes...", (int)Tintegral, frostlimit);
         }
         else
         {
            strcpy (str1, "arm");
            strcpy (str2, "Frost Protect - monitoring automatic temperature control");
         }
      else
      {
         strcpy (str1, "off");
         strcpy (str2, "Frost Protect - off");
      }
      break;
   }
   json_object_object_add (jfrost, "status", json_object_new_string (str1));
   json_object_object_add (jfrost, "description", json_object_new_string (str2));
   json_object_object_add (jobj, "frost", jfrost);

   gmtime_r(&uptime, &tm);
   if (tm.tm_yday == 0)
      sprintf (str1, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
   else
      sprintf (str1, "%d %02d:%02d:%02d", tm.tm_yday, tm.tm_hour, tm.tm_min, tm.tm_sec);
   json_object_object_add (jobj, "uptime", json_object_new_string (str1));

   json_object_object_add (jobj, "temperature", json_object_new_int ((int) (temperature * 100)));
   json_object_object_add (jobj, "current", json_object_new_int (GetCurrent()));
   json_object_object_add (jobj, "zones", jzones);

   mg_printf_data (conn, "%s", json_object_to_json_string (jobj));
   json_object_put (jobj);
   return MG_TRUE;
}


/*
 * This callback function is attached to the URI "/timedata"
 * It returns a json object with all the timeline information
 */
static int
show_timedata (struct mg_connection *conn)
{
   uint8_t zone, index, action;
   struct json_object *jobj, *jzones;
   FILE *fd;
   time_t starttime;
   struct mapstruct cmap;
   char tmpstr[80];

   send_headers(conn);

   jzones = json_object_new_array ();

   // note all the currently active zones
   for (zone = 1; zone < REALZONES; zone++)
   {
      if (chanmap[zone].state == ACTIVE)
      {
         jobj = create_json_zone (zone, chanmap[zone].starttime, &chanmap[zone]);
         json_object_array_add (jzones, jobj);
      }
   }

// walk the time queue to find the future active zones
   for (index = 0;;)
   {
      index = walk_queue (index, &zone, &starttime, &action);
      if (index == 0)
         break;
      if ((action == TURNON) || (action == TESTON))
      {
         jobj = create_json_zone (zone, starttime, &chanmap[zone]);
         json_object_array_add (jzones, jobj);
      }
   }

   // read the values from the history file
   if ((fd = open_history ()) != NULL)
   {

      while (read_history (&cmap, fd))
      {
         jobj = create_json_zone (cmap.zone, cmap.starttime, &cmap);
         json_object_array_add (jzones, jobj);
      }
      close_history (fd);
   }

   jobj = json_object_new_object ();
   json_object_object_add (jobj, "cmd", json_object_new_string ("timedata"));

   // define start and end dates 8 days either way from today
   starttime = basictime - (8*24*60*60);
   strftime(tmpstr, sizeof(tmpstr), fmt, localtime(&starttime));
   json_object_object_add (jobj, "mindate", json_object_new_string (tmpstr));
   starttime = basictime + (8*24*60*60);
   strftime(tmpstr, sizeof(tmpstr), fmt, localtime(&starttime));
   json_object_object_add (jobj, "maxdate", json_object_new_string (tmpstr));
   json_object_object_add (jobj, "events", jzones);

   mg_printf_data (conn, "%s", json_object_to_json_string (jobj));
   json_object_put (jobj);
   return MG_TRUE;
}



static int show_sensors(struct mg_connection *conn)
{
   uint8_t zone;
   struct json_object *jobj, *jzones;
   double value;

   send_headers(conn);

   jzones = json_object_new_array ();

   for (zone = 1; zone < REALZONES; zone++)
   {
      if (chanmap[zone].type & ISSENSOR)
      {
         value = GetSensorbyZone(zone);
         jobj = json_object_new_object ();
         json_object_object_add (jobj, "zone", json_object_new_int (zone));
         json_object_object_add (jobj, "status", json_object_new_string ("ready"));
         json_object_object_add (jobj, "name", json_object_new_string (chanmap[zone].name));
         json_object_object_add (jobj, "value", json_object_new_double (value));
         json_object_array_add (jzones, jobj);

      }
   }
   jobj = json_object_new_object ();
   json_object_object_add (jobj, "cmd", json_object_new_string ("sensors"));
   json_object_object_add (jobj, "zones", jzones);

   mg_printf_data (conn, "%s", json_object_to_json_string (jobj));
   json_object_put (jobj);

   return MG_TRUE;

}

#define jjj 1

static int
show_stats (struct mg_connection *conn)
{
   uint8_t zone;
   struct json_object *jobj;
#if jjj > 0
   struct json_object *jzones;

   // tell the browser to expect some javascript
   send_headers(conn);
   jzones = json_object_new_array ();
   // Not implemented yet so in the meantime, just make it print/readable
#else
   mg_send_header (conn, "Content-Type", "text/html");
#endif
   // load stats from file
   read_statistics();

   for (zone = 1; zone < REALZONES; zone++)
   {
      // get stats specifying that we want the human readable stuff included
      if ((jobj = get_statistics(zone, true)) != NULL)
      {
#if jjj > 0
         json_object_array_add (jzones, jobj);
#else
         mg_printf_data (conn, "%s<br>", json_object_to_json_string (jobj));
         json_object_put (jobj);
#endif
      }
   }

#if jjj > 0
   jobj = json_object_new_object ();
   json_object_object_add (jobj, "cmd", json_object_new_string ("statistics"));
   json_object_object_add (jobj, "zones", jzones);

   mg_printf_data (conn, "%s", json_object_to_json_string (jobj));
   json_object_put (jobj);
#endif
   return MG_TRUE;
}


/*
 * This callback is attached to the URI "/set_state"
 * It receives a json object from the client web browser with a command and data
 * The command updates the chanmap and queues any immediate events
 * Persistent data is written at the end
 */
static int
set_state (struct mg_connection *conn)
{
   struct json_object *jobj;
   char cmd[12];
   uint8_t zone, days;
   int8_t i, j;
   int32_t frequency;
   time_t starttime;
   struct tm tm;

   conn->content[conn->content_len] = 0;
   if (debug)
      printf ("Received data: %s\n", conn->content);
   jobj = json_tokener_parse (conn->content);
   if (is_error (jobj))
   {
      log_printf (LOG_NOTICE, "error parsing json object %s\n", conn->content);
      json_object_put (jobj);
      return MG_FALSE;
   }

   zone = json_object_get_int (json_object_object_get (jobj, "zone"));
   strncpy (cmd, json_object_get_string (json_object_object_get (jobj, "cmd")), 10);
   starttime = chanmap[zone].starttime; // save the current start time & freq for a mo
   frequency = chanmap[zone].frequency;
   chanmap[zone].useful = FALSE;        // assume we won't be using it any more

   // all actions require that we cancel any existing activity in the zone
   // we may put something back though

   // see if this is a virtual channel handling groups
   if (chanmap[zone].type & ISGROUP)
   {
      group_cancel (zone, IDLE);
   }
   else if (chanmap[zone].type & ISTEST)
   {
      test_cancel (zone);
   }
   else
   {
      zone_cancel (zone, IDLE);
      chanmap[zone].locked = FALSE;        // manually clearing also unlocks
   }
   // program puts in new values for the zone
   if (strncmp (cmd, "program", 7) == 0)
   {
      // start time will required extra decoding
      starttime = json_object_get_int (json_object_object_get (jobj, "start"));
      // find the start time
      if (starttime == 2400)    // magic number for now
         starttime = basictime;
      else if (starttime == 2500)       // magic number for auto
         // haven't done auto yet - will be based on 2300 - 0700 cheap electric!!
         starttime = basictime + 10;
      else
      {
         localtime_r (&basictime, &tm);
         tm.tm_hour = starttime / 100;
         tm.tm_min = starttime - (tm.tm_hour * 100);    // might allow mins later
         tm.tm_sec = 0;
         starttime = mktime (&tm);
         if (starttime < basictime)     // if time < now then must be tomorrow!!
            starttime += 60 * 60 * 24;
      }
      chanmap[zone].starttime = starttime;
      // duration for the whole of this zone or group members
      chanmap[zone].duration = json_object_get_int (json_object_object_get (jobj, "duration"));
      // use frequency as a repeat
      chanmap[zone].frequency = json_object_get_int (json_object_object_get (jobj, "frequency"));
      if (chanmap[zone].frequency > 1024)
      {
         // find out what day of the week the start time is on
         localtime_r (&starttime, &tm);
         j = tm.tm_wday;
         days = chanmap[zone].frequency & 0x0f;      // see how many days to go in succession
         chanmap[zone].frequency = -1;               // indicate to scheduler that we have a daylist
         for (i = 0; i < 7; i++, j++)
         {
            if (days)
            {
               chanmap[zone].daylist[j % 7] = true;
               --days;
            }
            else
            {
               chanmap[zone].daylist[j % 7] = false;
            }
         }
      }
      else
      {
         chanmap[zone].frequency *= 3600;           // convert hours to seconds
      }
      chanmap[zone].useful = TRUE;      // got some useful data here!!
      chanmap[zone].period = chanmap[zone].duration;
   }
   // delay puts back the values we saved earlier but 24hrs further on
   else if (strncmp (cmd, "delay", 5) == 0)
   {
      starttime += 60 * 60 * 24;        // delay by 24 hrs
      chanmap[zone].starttime = starttime;
      chanmap[zone].frequency = frequency;
      j = chanmap[zone].daylist[6];
      for (i = 6; i > 0; --i)
      {
         chanmap[zone].daylist[i] = chanmap[zone].daylist[i -1];
      }
      chanmap[zone].daylist[0] = j;
      chanmap[zone].useful = TRUE;      // got some useful data here (maybe!!)
   }
   else if (strncmp (cmd, "advance", 7) == 0)
   {
      starttime -= 60 * 60 * 24;        // advance by 24 hrs - if ends up in the past then check_schedule will sort it out
      chanmap[zone].starttime = starttime;
      chanmap[zone].frequency = frequency;
      j = chanmap[zone].daylist[0];
      for (i = 0; i < 7; ++i)
         chanmap[zone].daylist[i] = chanmap[zone].daylist[i + 1];
      chanmap[zone].daylist[6] = j;
      chanmap[zone].useful = TRUE;      // got some useful data here (maybe!!)
   }
   else if (strncmp (cmd, "cancel", 6) == 0)
   {
      // already done
   }
   else
   {
      errno = ENOTSUP;
      log_printf (LOG_ERR, "Invalid command %s", cmd);
   }

   json_object_put (jobj);
   check_schedule (TRUE);       // assume major changes to the schedule
   show_status (conn);
   return MG_TRUE;
}

/*
 * This callback is attached to the URI "/set_frost"
 * It receives a json object from the client web browser with a command and data
 * The command updates the chanmap and queues any immediate events
 * Persistent data is written at the end
 */
static int
set_frost (struct mg_connection *conn)
{
   struct json_object *jobj;
   char cmd[12];

   conn->content[conn->content_len] = 0;
   if (debug)
      printf ("Received data: %s\n", conn->content);
   jobj = json_tokener_parse (conn->content);
   if (is_error (jobj))
   {
      log_printf (LOG_NOTICE, "error parsing json object %s\n", conn->content);
      json_object_put (jobj);
      return MG_FALSE;
   }


   strncpy (cmd, json_object_get_string (json_object_object_get (jobj, "cmd")), 10);
   if (strncmp (cmd, "frost", 5) == 0)
   {
      char mode[7];
      strncpy (mode, json_object_get_string (json_object_object_get (jobj, "mode")), 6);
      if (strncmp (mode, "on", 2) == 0)
      {
         frost_cancel ();       // clear any current activity
         dofrost ();         // before running the frost program
         frost_mode = FROST_MANUAL;     // leave armed state alone
      }
      else if (strncmp (mode, "off", 3) == 0)
      {
         if (frost_mode == FROST_MANUAL)        // cancel when manually activated
         {
            frost_cancel ();
            frost_mode = FROST_OFF;
         }
         else if (frost_mode == FROST_ACTIVE)
         {
            frost_cancel ();
            frost_mode = FROST_OFF;
            frost_armed = FALSE;        // if we're temp triggered then cancel and clear armed state
         }
         else
         {
            frost_armed = FALSE;        // if we're not on at all then clear armed state
            Tintegral = 0;      // stop integration indicator
         }
      }
      else if (strncmp (mode, "arm", 3) == 0)
      {
         if (frost_mode == FROST_MANUAL)        // if arming and already in manual then clear down
         {
            frost_cancel ();
            frost_mode = FROST_OFF;
         }
         frost_armed = TRUE;
      }
      check_schedule (TRUE);
   }
   show_status (conn);
   return MG_TRUE;
}


static const struct web_config
{
   enum mg_event event;
   const char *uri;
   int (*func) (struct mg_connection *);
} web_config[] =
{
   { MG_AUTH,    "/passfile", &refuse},
   { MG_REQUEST, "/status", &show_status},
   { MG_REQUEST, "/timedata", &show_timedata},
   { MG_REQUEST, "/sensors", &show_sensors},
   { MG_REQUEST, "/stats", &show_stats},
   { MG_AUTH,    "/set_state", &check_authorised},
   { MG_REQUEST, "/set_state", &set_state},
   { MG_AUTH,    "/set_frost", &check_authorised},
   { MG_REQUEST, "/set_frost", &set_frost},
   { MG_REQUEST, "/jlogs", &show_jsonlogs},
   { MG_REQUEST, "/logs", &show_logs},
   { 0, NULL, NULL}
};


static int
ev_handler(struct mg_connection *conn, enum mg_event event)
{
   int i;

   if (event == MG_POLL)
      return MG_FALSE;

   for (i = 0; web_config[i].uri != NULL; i++)
   {
      if ((event == web_config[i].event) && (!strcmp (conn->uri, web_config[i].uri)))
      {
         return web_config[i].func (conn);
      }
   }

   // unhandled uri is allowed to be handled by mongoose
   if (event == MG_AUTH)
      return MG_TRUE;
   else
      return MG_FALSE;
}

void
irr_web_poll(void)
{
   mg_poll_server(server, 0);
}


   /*
    * Initialize HTTPD context.
    * Attach a redirect to the status page on web root
    * Put pasword protect on set_state page
    * Set WWW root as defined by command line
    * Start listening on port as defined by command line
    */
void
irr_web_init (void)
{

   server = mg_create_server(NULL, ev_handler);

   mg_set_option (server, "listening_port", httpport);
   mg_set_option (server, "document_root", httproot);
   mg_set_option (server, "auth_domain", "gilks.ath.cx");
   mg_set_option (server, "index_files", "zones.html");
   if (accesslog[0] != '\0')
      mg_set_option (server, "access_log_file", accesslog);

}

