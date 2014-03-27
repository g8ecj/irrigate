//---------------------------------------------------------------------------
// Copyright (C) 2009-2014 Robin Gilks
//
//
//  irr_chanmap.c   -   This section looks after reading, creating and writing the channel map
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



// read from a file to populate the chanmap array of mapstruct entries
// return TRUE if we got some good config data
// return FALSE if no file or something wrong with it
bool
readchanmap (void)
{
   uint8_t zone, group;
   bool ret = FALSE;
   int offset;
   char *p;
   char *input;
   int8_t pump;

   FILE *fd;
   struct json_object *jobj, *jlist, *jtmp;

   fd = fopen (configfile, "r");
   if (fd == NULL)
      return FALSE;

   input = malloc (1024);
   while (fgets (input, 1024, fd) != NULL)
   {
      if ((p = strchr (input, '{')) == NULL)    // ignore lines that don't have a '{' in them!
         continue;
      offset = p - input;
      jobj = json_tokener_parse (&input[offset]);
      if (is_error (jobj))
      {
         printf ("error parsing json object %s\n", &input[offset]);
      }
      else
      {
         zone = json_object_get_int (json_object_object_get (jobj, "zone"));
         if ((zone > 0) && (zone <= REALZONES))
         {
            chanmap[zone].zone = zone;
            chanmap[zone].type = 0;
            chanmap[zone].flow = json_object_get_int (json_object_object_get (jobj, "flow"));
            chanmap[zone].current = json_object_get_int (json_object_object_get (jobj, "current"));
            chanmap[zone].type |= json_object_get_boolean (json_object_object_get (jobj, "ispump")) ? ISPUMP : 0;
            chanmap[zone].type |= json_object_get_boolean (json_object_object_get (jobj, "isfrost")) ? ISFROST : 0;
            chanmap[zone].type |= json_object_get_boolean (json_object_object_get (jobj, "isstock")) ? ISSTOCK : 0;
            chanmap[zone].type |= json_object_get_boolean (json_object_object_get (jobj, "istest")) ? ISTEST : 0;
            chanmap[zone].type |= json_object_get_boolean (json_object_object_get (jobj, "isspare")) ? ISSPARE : 0;
            if (chanmap[zone].type & ISPUMP)
            {
               // find a few pump slot
               for (pump = 0; pump < MAXPUMPS; pump++)
               {
                  if (pumpmap[pump].zone == 0)
                  {
                     // found a free one!!
                     pumpmap[pump].zone = zone;
                     pumpmap[pump].minflow = json_object_get_int (json_object_object_get (jobj, "minflow"));
                     pumpmap[pump].nomflow = json_object_get_int (json_object_object_get (jobj, "nomflow"));
                     pumpmap[pump].maxflow = json_object_get_int (json_object_object_get (jobj, "maxflow"));
                     pumpmap[pump].maxstarts = json_object_get_int (json_object_object_get (jobj, "maxstarts"));
                     pumpmap[pump].maxrun = json_object_get_int (json_object_object_get (jobj, "maxrun"));
                     pumpmap[pump].start = json_object_get_int (json_object_object_get (jobj, "start"));
                     pumpmap[pump].end = json_object_get_int (json_object_object_get (jobj, "end"));
                     break;
                  }
               }
            }

            chanmap[zone].AorB = json_object_get_boolean (json_object_object_get (jobj, "aorb"));
            // careful with string input in case its not actually there!!
            jtmp = json_object_object_get (jobj, "address");
            if (jtmp)
            {
               strncpy (chanmap[zone].address, json_object_get_string (jtmp), 17);
               json_object_put (jtmp);
            }
            jtmp = json_object_object_get (jobj, "name");
            if (jtmp)
            {
               strncpy (chanmap[zone].name, json_object_get_string (jtmp), 33);
               json_object_put (jtmp);
            }
            // if a zone has the group attribute then it controls the list of zones in that group
            group = json_object_get_int (json_object_object_get (jobj, "group"));
            if (group > 0)
            {
               int i;
               for (i = 1; i < REALZONES; i++)
               {
                  // see if we have already used this group
                  if ((chanmap[i].type & ISGROUP) && (chanmap[i].group == (1 << (group -1))))
                  {
                     log_printf(LOG_ERR, "Duplicate group %d", group);
                     return FALSE;
                  }
               }

               jlist = json_object_object_get (jobj, "zones");
               for (i = 0; i < json_object_array_length (jlist); i++)
               {
                  chanmap[json_object_get_int (json_object_array_get_idx (jlist, i))].group |= 1 << (group - 1);
               }
               chanmap[zone].group = 1 << (group - 1);
               chanmap[zone].type |= ISGROUP;
            }
            chanmap[zone].valid |= CONFIGURED;
         }
         else
         {
            log_printf(LOG_ERR, "Invalid zone number %d", zone);
            return FALSE;
         }
         ret = TRUE;            // got something useful
      }
      json_object_put (jobj);
   }
   free (input);
   fclose (fd);

   if (debug > 1)
      print_chanmap ();
   return ret;
}

// create a load of mapstruct entries from interactive user input
// this function only gets the 1-wire relevant data (address and port)
// its up to the user to define how the zones get displayed on the UI
void
createchanmap (int numdev)
{
   int n, dev, output;

   printf ("\t\tThere are %d zones to be entered\n", numdev * 2);
   printf ("As each zone is activated in turn - enter the zone number\n");

   /* Starting from the top... */
   output = 1;
   do
   {
      dev = (output - 1) / 2;
      setGPIOraw(dev, (output & 1) ? 0xfe : 0xfd);    // starts at pioA; then pioB
      printf ("Enter zone number (output %d of %d) [0 to end]: ", output, numdev * 2);
      n = getNumber (0, REALZONES);
      setGPIOraw(dev, 0xff);
      if (n == 0)
         break;
      if (chanmap[n].zone > 0)
      {
         printf ("Output %d has already been assigned zone number %d\n", chanmap[n].output, n);
         continue;
      }
      chanmap[n].valid |= CONFIGURED;
      chanmap[n].zone = n;
      chanmap[n].output = output;
      chanmap[n].AorB = (output & 1) ? TRUE : FALSE;
      strncpy (chanmap[n].address, getGPIOAddr (dev), 16);
      if (debug > 1)
         printf ("Output %d zone %d port %c device %d\n", output, n, chanmap[n].AorB ? 'A' : 'B', dev);
      output++;

   }
   while (output <= numdev * 2);
}

// save the chanmap mapstruct array to a file
// this will require editing by hand to create the display map and other parameters
bool
savechanmap (void)
{
   int i;
   FILE *fd;
   struct json_object *jobj;

   fd = fopen (configfile, "w");
   if (fd == NULL)
      return FALSE;

   for (i = 1; i < REALZONES; i++)
   {
      if (chanmap[i].valid)
      {
         jobj = json_object_new_object ();
         json_object_object_add (jobj, "zone", json_object_new_int (chanmap[i].zone));
         json_object_object_add (jobj, "aorb", json_object_new_boolean (chanmap[i].AorB));
         json_object_object_add (jobj, "address", json_object_new_string (chanmap[i].address));
         fputs (json_object_to_json_string (jobj), fd);
         fputc ('\n', fd);
         json_object_put (jobj);
      }
   }
   fclose (fd);
   return TRUE;
}

void
print_chanmap (void)
{
   int zone;
   char state[][8] = {
      "Idle",
      "Active",
      "Error"
   };
   struct tm tm;

   for (zone = 1; zone < MAXZONES; zone++)
   {
      if (chanmap[zone].valid)
      {
         localtime_r (&(chanmap[zone].starttime), &tm);
         log_printf(LOG_DEBUG, 
             "zone %d, %s name %s, port %c, add %s, flow %d, start %s %02d:%02d:%02d (%lu), dur %d, per %d, freq %d val %d use %d group %02x type %02x tf %2.5f lck %d\n",
             chanmap[zone].zone, state[chanmap[zone].state], chanmap[zone].name,
             chanmap[zone].AorB ? 'A' : 'B', chanmap[zone].address, chanmap[zone].flow,
             daystr[tm.tm_wday], tm.tm_hour, tm.tm_min, tm.tm_sec, chanmap[zone].starttime,
             chanmap[zone].duration, chanmap[zone].period, chanmap[zone].frequency,
             chanmap[zone].valid, chanmap[zone].useful, chanmap[zone].group, chanmap[zone].type,
             chanmap[zone].totalflow, chanmap[zone].locked);
      }
   }
}


