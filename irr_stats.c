//---------------------------------------------------------------------------
// Copyright (C) 2009-2014 Robin Gilks
//
//
//  irr_stats.c   -   This section looks after the statistics of water delivered, pump run time
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


#include <stdarg.h>
#include <string.h>

#define SYSLOG_NAMES
#include "irrigate.h"



// linked list of log messages
typedef struct logstruct
{
   struct logstruct *next;      // the next object in the list
   int pri;
   time_t time;
   char *log;
} LS;

LS *loghead = NULL;

#define LOG_MAXLEN 250
#define MAXLOG 100



/*
 * Decode a priority into textual information like emerg/notice etc
 */
char *
textpri (int pri)
{
   static char res[20];
   CODE *c_pri;

   for (c_pri = prioritynames; c_pri->c_name && !(c_pri->c_val == LOG_PRI (pri)); c_pri++);

   snprintf (res, sizeof (res), "%s", c_pri->c_name);

   return res;
}



/*
 * every hour update the time spent with the well pump on (assuming it has been on at all)
 * and count how much water has been put on each zone (based on config file values)
 */
void
update_statistics (void)
{
   struct json_object *jobj;
   char statsfile[MAXFILELEN];
   uint8_t zone, changes = 0;
   FILE *fd;
   char *input;

   strcpy (statsfile, datapath);
   strncat (statsfile, "/statistics", 12);

   // start off by reading the last values from file
   fd = fopen (statsfile, "r");
   if (fd != NULL)
   {
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
            // make sure I actually read a zone
            if (zone > 0)
            {
               if (chanmap[zone].type & ISPUMP)
                  pumpmap[get_pump_by_zone(zone)].pumpingtime += json_object_get_double (json_object_object_get (jobj, "pumptime")) * 3600;
               else
                  chanmap[zone].totalflow += json_object_get_double (json_object_object_get (jobj, "totalflow"));
            }
         }
         json_object_put (jobj);
      }
      free (input);
      fclose (fd);
   }

   // scan zones to see if we are making any changes to the stats
   // we must do this seperately otherwise we only see the zones already in the stats file.
   for (zone = 1; zone < REALZONES; zone++)
   {
      if (chanmap[zone].type & ISPUMP)
      {
         if (pumpmap[get_pump_by_zone(zone)].pumpingtime > 0)
            changes++;
      }
      else
      {
         if (chanmap[zone].totalflow > 0)
             changes++;
      }
   }

   // nothing to do if no changes!!
   if (changes == 0)
      return;

   // update the records in the file and clear the current accumulated stats
   // Note: this means we rewrite the whole file even if only one zone changes.
   fd = fopen (statsfile, "w");
   if (fd == NULL)
   {
      printf ("Can't open statistics file [%s]\n", statsfile);
      return;                   // fatal error
   }
   for (zone = 1; zone < REALZONES; zone++)
   {
      if ((chanmap[zone].type & ISPUMP) && (pumpmap[get_pump_by_zone(zone)].pumpingtime > 0))
      {
         jobj = json_object_new_object ();
         json_object_object_add (jobj, "zone", json_object_new_int (chanmap[zone].zone));
         json_object_object_add (jobj, "name", json_object_new_string (chanmap[zone].name));
         json_object_object_add (jobj, "pumptime", json_object_new_double ((double)pumpmap[get_pump_by_zone(zone)].pumpingtime / 3600.0));
         fputs (json_object_to_json_string (jobj), fd);
         fputc ('\n', fd);
         json_object_put (jobj);
         pumpmap[get_pump_by_zone(zone)].pumpingtime = 0;   // reset count
      }
      else if (chanmap[zone].totalflow > 0)
      {
         jobj = json_object_new_object ();
         json_object_object_add (jobj, "zone", json_object_new_int (chanmap[zone].zone));
         json_object_object_add (jobj, "name", json_object_new_string (chanmap[zone].name));
         json_object_object_add (jobj, "totalflow", json_object_new_double (chanmap[zone].totalflow));
         fputs (json_object_to_json_string (jobj), fd);
         fputc ('\n', fd);
         json_object_put (jobj);
         chanmap[zone].totalflow = 0;   // reset count
      }
   }

   fclose (fd);
}

void
dump_log_msgs (void)
{
   LS *p;
   struct tm tm;
   char timestamp[32];

   p = loghead;
   while (p != NULL)
   {
      localtime_r (&p->time, &tm);
      strftime (timestamp, 32, "%b %d %H:%M:%S", &tm);
      printf ("%s irrigate %s: %s\r\n", timestamp, textpri (p->pri), p->log);
      p = p->next;
   }
}

void
send_log_msgs (struct mg_connection *conn, msgfunc_t webmsg)
{

   LS *p;

   p = loghead;
   while (p != NULL)
   {
      webmsg (conn, p->time, p->pri, textpri (p->pri), p->log);
      p = p->next;
   }
}


void
add_log_msg (char *msg, time_t time, int priority)
{
   int16_t len = strlen (msg), count = 1;
   LS *n, *p;

   /* create a new node */
   n = (LS *) malloc (sizeof (LS));
   n->log = malloc (len + 1);   // allow room for terminator
   /* save data into new node */
   strncpy (n->log, msg, len + 1);
   n->time = time;
   n->pri = priority;
   n->next = NULL;

   /* first, we handle the case where `data' should be the first element */
   if (loghead == NULL)
   {
      /* apparently this !IS! the first element */
      /* now data should [be|becomes] the first element */
      loghead = n;
   }
   else
   {
      p = loghead;
      while (p->next != NULL)
      {
         p = p->next;
         count++;
      }
      p->next = n;
   }
   // we only keep the last 'n' log messages 
   if (count >= MAXLOG)
   {
      p = loghead->next;
      free (loghead->log);
      free (loghead);
      loghead = p;
   }
}



void
log_printf (int priority, const char *format, ...)
{
   va_list ap;
   char message[LOG_MAXLEN];


   va_start (ap, format);
   vsnprintf (message, LOG_MAXLEN, format, ap);
   va_end (ap);

   // put into internal log buffer
   add_log_msg (message, basictime, priority);

   syslog (priority, "%s", message);
}


// open history file for reading
FILE *
open_history (void)
{
   FILE *fd;
   char histfile[MAXFILELEN];

   strcpy (histfile, datapath);
   strncat (histfile, "/history", 9);

   // read the values from the history file
   fd = fopen (histfile, "r");
   return fd;

}


// read an item from the history file and fill in the chanmap structure from the data
int
read_history (struct mapstruct *cmap, FILE * fd)
{
   char *input;
   int ret = TRUE;
   struct json_object *jobj;

   input = malloc (1024);
   if (fgets (input, 1024, fd) != NULL)
   {
      jobj = json_tokener_parse (input);
      if (is_error (jobj))
      {
         printf ("error parsing json object %s\n", input);
         ret = FALSE;
      }
      else
      {
         cmap->zone = json_object_get_int (json_object_object_get (jobj, "zone"));
         if (cmap->zone > 0)
         {
            cmap->state = json_object_get_int (json_object_object_get (jobj, "result"));
            cmap->lasterrno = json_object_get_int (json_object_object_get (jobj, "errno"));
            cmap->starttime = json_object_get_int (json_object_object_get (jobj, "time"));
            cmap->period = json_object_get_int (json_object_object_get (jobj, "period"));
            cmap->duration = cmap->period;
            strcpy(cmap->name, chanmap[cmap->zone].name);
            // can't repeat or be locked in the past
            cmap->frequency = 0;
            cmap->locked = 0;
         }
      }
      json_object_put (jobj);
   }
   else
   {
      ret = FALSE;
   }

   free (input);

   return ret;
}


// close the history file
void
close_history (FILE * fd)
{
   fclose (fd);
}


// append data to the history file
void
write_history (uint8_t zone, time_t endtime, time_t starttime, uint8_t result)
{
   struct json_object *jobj;
   char histfile[MAXFILELEN];
   FILE *fd;

   strcpy (histfile, datapath);
   strncat (histfile, "/history", 9);

   // start off by reading the last values from file
   fd = fopen (histfile, "a");
   if (fd != NULL)
   {
      jobj = json_object_new_object ();
      json_object_object_add (jobj, "zone", json_object_new_int (zone));
      json_object_object_add (jobj, "time", json_object_new_int (starttime));
      json_object_object_add (jobj, "period", json_object_new_int (endtime - starttime));
      json_object_object_add (jobj, "result", json_object_new_int (result));
      json_object_object_add (jobj, "errno", json_object_new_int (errno));
      fputs (json_object_to_json_string (jobj), fd);
      fputc ('\n', fd);
      json_object_put (jobj);
      errno = 0;
   }
   fclose (fd);
}


// expire entries more than 1 week old from the history file
void
clean_history (void)
{
   struct json_object *jobj, *jarray;
   char histfile[MAXFILELEN];
   char *input;
   FILE *fd;
   time_t starttime;
   int i, changes = FALSE;

   strcpy (histfile, datapath);
   strncat (histfile, "/history", 9);

   jarray = json_object_new_array ();
   // read the values from the history file
   fd = fopen (histfile, "r");
   if (fd != NULL)
   {
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
            starttime = json_object_get_int (json_object_object_get (jobj, "time"));
            if (starttime > basictime - (60 * 60 * 24 * 8))     // less than a week old
            {
               json_object_array_add (jarray, jobj);    // add to array to save it
            }
            else
            {
               json_object_put (jobj);  // else junk it
               changes = TRUE;
            }
         }
      }
      fclose (fd);
      free (input);
   }

   if (changes)
   {
      // put back those values young enough to still be in the array
      fd = fopen (histfile, "w");
      if (fd != NULL)
      {
         for (i = 0; i < json_object_array_length (jarray); i++)
         {
            jobj = json_object_array_get_idx (jarray, i);
            fputs (json_object_to_json_string (jobj), fd);
            fputc ('\n', fd);
            json_object_put (jobj);
         }
         fclose (fd);
      }
   }
   json_object_put (jarray);
}
