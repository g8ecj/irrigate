//---------------------------------------------------------------------------
// Copyright (C) 2009-2010 Robin Gilks
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
#include <pthread.h>

#define MAX_OPTIONS 40


const char *fmt = "%a, %d %b %Y %H:%M:%S %z";

static void
send_headers (struct mg_connection *conn)
{
   mg_send_header (conn, "Cache", "no-cache");
   mg_send_header (conn, "Content-Type", "application/x-javascript");
}


static int
check_authorised(struct mg_connection *conn)
{
   FILE *fp;
   int authorized = 0;
   char pass_file[MAXFILELEN];

   strcpy (pass_file, datapath);
   strncat (pass_file, "/passfile", 10);

   if (pass_file != NULL && (fp = fopen(pass_file, "r")) != NULL) {
      authorized = mg_authorize_digest(conn, fp);
      fclose(fp);
   }

   if (!authorized) 
   {
      mg_send_digest_auth_request(conn);
      return 1;
   }
  return 0;
}


/*
 *       webmsg(conn, p->time, p->pri, textpri(p->pri), p->log);
*/

void
sendjsonmsg (struct mg_connection *conn, time_t time, int priority, char *desc, char *msg)
{
   char timestr[64];
   struct json_object *jobj;

//   ctime_r (&time, timestr);
   strftime(timestr, sizeof(timestr), fmt, localtime(&time));

   jobj = json_object_new_object ();
   json_object_object_add (jobj, "time", json_object_new_string (timestr));
   json_object_object_add (jobj, "priority", json_object_new_int (priority));
   json_object_object_add (jobj, "desc", json_object_new_string (desc));
   json_object_object_add (jobj, "log", json_object_new_string (msg));

   mg_printf_data (conn, "%s", json_object_to_json_string (jobj));
   json_object_put (jobj);

}

static int
show_jsonlogs (struct mg_connection *conn)
{
   send_headers (conn);

   mg_printf_data (conn, "%s", "{ \"logs\":[ ");

   send_log_msgs (conn, sendjsonmsg);

   mg_printf_data (conn, "%s", " ] }");
   return 1;
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
   return 1;

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

   char descstr[140];
   char startstr[64];
   char endstr[64];
   char fromday[64];
   char colour[10];
   char ing_ed[10];
   char is_was[10];
   char tmpstr[30];
   char rptstr[30];

   if (cmap->state == ACTIVE)
   {
      if (starttime > basictime)        // more than one instance queued, only the 1st is active
         strncpy (colour, "green", 6);
      else
         strncpy (colour, "red", 4);
      strncpy (tmpstr, "active", 7);
   }
   else if (cmap->state == ERROR)
   {
      strncpy (colour, "fuchsia", 8);
      strncpy (tmpstr, "error", 6);
   }
   else if (cmap->state == WASOK)
   {
      strncpy (colour, "green", 6);
      strncpy (tmpstr, "doneok", 7);
   }
   else if (cmap->state == WASCANCEL)
   {
      strncpy (colour, "blue", 5);
      strncpy (tmpstr, "cancelled", 10);
   }
   else if (cmap->state == WASFAIL)
   {
      strncpy (colour, "fuchsia", 8);
      strncpy (tmpstr, "failed", 7);
   }
   else if (cmap->frequency)
   {
      strncpy (colour, "cyan", 5);
      strncpy (tmpstr, "repeating", 10);
   }
   else if (cmap->locked)
   {
      strncpy (colour, "blue", 5);
      strncpy (tmpstr, "locked", 7);
   }
   else if (findonqueue (zone) > 0)
   {
      strncpy (colour, "green", 6);
      strncpy (tmpstr, "queued", 7);
   }
   else if (cmap->useful)
   {
      strncpy (colour, "green", 6);
      strncpy (tmpstr, "queued", 7);
   }
   else
   {
      strncpy (colour, "white", 6);
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
   default:
      strncpy (rptstr, "", 2);
      break;
   }

   strftime(startstr, sizeof(startstr), fmt, localtime(&starttime));
   t = starttime + cmap->period;
   strftime(endstr, sizeof(endstr), fmt, localtime(&t));

   if (((starttime > basictime) 
         && (starttime < (basictime + 60 * 60 * 24))) 
            || ((starttime + cmap->frequency > basictime) 
               && (starttime + cmap->frequency < (basictime + 60 * 60 * 24))))        // start OR repeat is today
   {
      fromday[0] = '\0';
   }
   else if (starttime > (basictime + 60 * 60 * 24)) // start more than 24hrs ahead - always have day
   {
      sprintf(fromday, "from %s", daystr[tm.tm_wday]);     // use starttime to find day
   }
   else if (starttime + cmap->frequency > (basictime + 60 * 60 * 24))  // repeat sometime more than 24hrs ahead
   {
      t = starttime + cmap->frequency;
      localtime_r (&t, &tm);
      sprintf(fromday, "from %s", daystr[tm.tm_wday]);     // use starttime to find day
   }
   else
   {
      fromday[0] = '\0';
   }
   // description is 
   // start{ing/ed}(1) at <time> for a duration of <period> and {is/was}(2) <state>  {and is repeating/""}(3) <rptstr> {from <daystr>}(4)
   // (1) start time < basictime
   // (2) start time + period < basictime
   // (3) frequency > 0
   // (4) infuture, not today
   localtime_r (&starttime, &tm);
   if (starttime < basictime)
      strncpy (ing_ed, "ed", 3);
   else
      strncpy (ing_ed, "ing", 4);

   if (starttime + cmap->period < basictime)
      strncpy (is_was, "was", 4);
   else
      strncpy (is_was, "is", 3);

   if (strncmp(tmpstr, "idle", 5) == 0)
      sprintf (descstr, "%s - nothing scheduled", cmap->name);
   else if (cmap->frequency == 0)
      sprintf (descstr, "%s - start%s at %02d%02d for a duration of %3d minutes and %s %s %s", 
         cmap->name, ing_ed, tm.tm_hour, tm.tm_min, cmap->duration / 60, is_was, tmpstr, fromday);
   else
      sprintf (descstr, "%s - start%s at %02d%02d for a duration of %3d minutes and %s %s %s %s", 
         cmap->name, ing_ed, tm.tm_hour, tm.tm_min, cmap->duration / 60, is_was, tmpstr, rptstr, fromday);

   jobj = json_object_new_object ();

   json_object_object_add (jobj, "zone", json_object_new_int (zone));
   json_object_object_add (jobj, "status", json_object_new_string (tmpstr));
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
   struct json_object *jobj, *jzones;
   char tmpstr[80];

   struct tm tm;

   send_headers(conn);

   jzones = json_object_new_array ();

   for (zone = 1; zone < REALZONES; zone++)
   {
      if (chanmap[zone].valid)
      {

         jobj = create_json_zone (zone, chanmap[zone].starttime, &chanmap[zone]);
         json_object_array_add (jzones, jobj);
      }
   }
   jobj = json_object_new_object ();
   json_object_object_add (jobj, "cmd", json_object_new_string ("status"));
   localtime_r (&basictime, &tm);
   sprintf (tmpstr, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
   json_object_object_add (jobj, "time", json_object_new_string (tmpstr));

   sprintf (tmpstr, "Version %s compiled on %s at %s", VERSION, __DATE__, __TIME__);
   json_object_object_add (jobj, "version", json_object_new_string (tmpstr));

   json_object_object_add (jobj, "temp_sensor_1", json_object_new_int (temperature1 * 100));
   json_object_object_add (jobj, "temp_sensor_2", json_object_new_int (temperature2 * 100));
   json_object_object_add (jobj, "flow", json_object_new_int (get_expected_flow()));

   // frost can be on (manual or cold!), armed or off - 4 different outcomes
   // might need to use Tintegral here (or equiv)
   switch (frost_mode)
   {
   case FROST_MANUAL:
      strncpy (tmpstr, "man", 4);
      break;
   case FROST_ACTIVE:
      strncpy (tmpstr, "on", 3);
      break;
   case FROST_OFF:
      if (frost_armed)
         if (Tintegral > 0)
            strncpy (tmpstr, "run", 4);
         else
            strncpy (tmpstr, "arm", 4);
      else
         strncpy (tmpstr, "off", 4);
      break;
   }
   json_object_object_add (jobj, "frost", json_object_new_string (tmpstr));
   json_object_object_add (jobj, "temperature", json_object_new_int ((int) (temperature * 100)));
   json_object_object_add (jobj, "current", json_object_new_int (GetCurrent()));
   json_object_object_add (jobj, "zones", jzones);

   mg_printf_data (conn, "%s", json_object_to_json_string (jobj));
   json_object_put (jobj);
   return 1;
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

   starttime = basictime - (7*24*60*60);
   strftime(tmpstr, sizeof(tmpstr), fmt, localtime(&starttime));
   json_object_object_add (jobj, "mindate", json_object_new_string (tmpstr));
   starttime = basictime + (7*24*60*60);
   strftime(tmpstr, sizeof(tmpstr), fmt, localtime(&starttime));
   json_object_object_add (jobj, "maxdate", json_object_new_string (tmpstr));
   json_object_object_add (jobj, "events", jzones);

   mg_printf_data (conn, "%s", json_object_to_json_string (jobj));
   json_object_put (jobj);
   return 1;
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
   uint8_t zone;
   int32_t frequency;
   time_t starttime;
   struct tm tm;

   if (check_authorised(conn))
      return 1;

   conn->content[conn->content_len] = 0;
   if (debug)
      printf ("Received data: %s\n", conn->content);
   jobj = json_tokener_parse (conn->content);
   if (is_error (jobj))
   {
      sprintf (logbuf, "error parsing json object %s\n", conn->content);
      syslog (LOG_NOTICE, "%s", logbuf);
      json_object_put (jobj);
      return 0;
   }

   zone = json_object_get_int (json_object_object_get (jobj, "zone"));
   strncpy (cmd, json_object_get_string (json_object_object_get (jobj, "cmd")), 10);
   starttime = chanmap[zone].starttime; // save the current start time & freq for a mo
   frequency = chanmap[zone].frequency;
   chanmap[zone].useful = FALSE;        // assume we won't be using it any more
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
      // all other actions require that we cancel any existing activity in the zone
      zone_cancel (zone, IDLE);
      chanmap[zone].locked = FALSE;        // manually clearing also unlocks
   }
   // program puts in new values for the zone
   if (strncmp (cmd, "program", 7) == 0)
   {
      // start time will required extra decoding
      starttime = json_object_get_int (json_object_object_get (jobj, "start"));
      // duration for the whole of this zone or group members
      chanmap[zone].duration = json_object_get_int (json_object_object_get (jobj, "duration"));
      // use frequency as a repeat
      chanmap[zone].frequency = json_object_get_int (json_object_object_get (jobj, "frequency")) * 60 * 60;     // convert hours to seconds
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
      chanmap[zone].useful = TRUE;      // got some useful data here!!
      chanmap[zone].period = chanmap[zone].duration;
//      insert (starttime, zone, TURNON);
   }
   // delay puts back the values we saved earlier but 24hrs further on
   else if (strncmp (cmd, "delay", 5) == 0)
   {
      starttime += 60 * 60 * 24;        // delay by 24 hrs
      chanmap[zone].starttime = starttime;
      chanmap[zone].frequency = frequency;
      chanmap[zone].useful = TRUE;      // got some useful data here (maybe!!)
   }
   else if (strncmp (cmd, "advance", 7) == 0)
   {
      starttime -= 60 * 60 * 24;        // advance by 24 hrs - if ends up in the past then check_schedule will sort it out
      chanmap[zone].starttime = starttime;
      chanmap[zone].frequency = frequency;
      chanmap[zone].useful = TRUE;      // got some useful data here (maybe!!)
   }
   else if (strncmp (cmd, "cancel", 6) == 0)
   {
      // already done
   }
   else
   {
      log_printf (LOG_ERR, "Invalid command %s", cmd);
   }

   json_object_put (jobj);
   check_schedule (TRUE);       // assume major changes to the schedule
   show_status (conn);
   return 1;
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

   if (check_authorised(conn))
      return 1;

   conn->content[conn->content_len] = 0;
   if (debug)
      printf ("Received data: %s\n", conn->content);
   jobj = json_tokener_parse (conn->content);
   if (is_error (jobj))
   {
      sprintf (logbuf, "error parsing json object %s\n", conn->content);
      syslog (LOG_NOTICE, "%s", logbuf);
      json_object_put (jobj);
      return 0;
   }


   strncpy (cmd, json_object_get_string (json_object_object_get (jobj, "cmd")), 10);
   if (strncmp (cmd, "frost", 5) == 0)
   {
      char mode[7];
      strncpy (mode, json_object_get_string (json_object_object_get (jobj, "mode")), 6);
      if (strncmp (mode, "on", 2) == 0)
      {
         frost_cancel ();       // clear any current activity
         frost_load ();         // before running the frost program
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
   return 1;
}


   /*
    * Initialize HTTPD context.
    * Attach a redirect to the status page on web root
    * Put pasword protect on set_state page
    * Set WWW root as defined by command line
    * Start listening on port as defined by command line
    */
void
irr_web_init (struct mg_server *server)
{

   mg_set_option (server, "listening_port", httpport);
   mg_set_option (server, "document_root", httproot);
   mg_set_option (server, "auth_domain", "gilks.ath.cx");
   mg_set_option (server, "index_files", "zones.html");
   if (accesslog[0] != '\0')
      mg_set_option (server, "access_log_file", accesslog);

   mg_add_uri_handler(server, "/status", show_status);
   mg_add_uri_handler(server, "/timedata", show_timedata);
   mg_add_uri_handler(server, "/set_state", set_state);
   mg_add_uri_handler(server, "/set_frost", set_frost);
   mg_add_uri_handler(server, "/jlogs", show_jsonlogs);
   mg_add_uri_handler(server, "/logs", show_logs);

}

