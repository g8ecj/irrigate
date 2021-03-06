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


char sensornames[][9] = 
{
   "",
   "CURRENT",
   "EXTTEMP",
   "INTTEMP",
   "SETTIME",
   "GETTIME"
};


// globals that all in this module can see
struct mapstruct chanmap[MAXZONES];
struct pumpstruct pumpmap[MAXPUMPS];
struct sensorstruct sensormap[MAXSENSORS];


void
maps_init(void)
{
   uint8_t i, j;

   for (i = 0; i < MAXZONES; i++)
   {
      chanmap[i].zone = 0;
      chanmap[i].valid = 0;
      chanmap[i].state = IDLE;
      chanmap[i].output = 0;
      chanmap[i].starttime = 0;
      chanmap[i].lastrun = 0;
      chanmap[i].lastdur = 0;
      chanmap[i].duration = 0;
      chanmap[i].totalflow = 0;
      chanmap[i].locked = FALSE;
      chanmap[i].name[0] = '\0';
      for (j = 0; j < 7; j++)
         chanmap[i].daylist[j] = false;
   }

   for (i = 0; i < MAXPUMPS; i++)
   {
      pumpmap[i].zone = 0;
      pumpmap[i].pumpingtime = 0;
   }

   for (i = 0; i < MAXSENSORS; i++)
   {
      sensormap[i].zone = 0;
   }




}


// read from a file to populate the chanmap array of mapstruct entries
// return TRUE if we got some good config data
// return FALSE if no file or something wrong with it
bool
readchanmap (void)
{
   uint8_t zone = 0, group = 0, sensor, pump, i;
   bool ret = FALSE;
   int offset;
   char *p;
   char *input;
   char type[12];

   FILE *fd;
   struct json_object *jobj, *jlist, *jtmp;

   fd = fopen (configfile, "r");
   if (fd == NULL)
      return FALSE;

   input = malloc (1024);
   while (fgets (input, 1024, fd) != NULL)
   {
      zone = 0;
      group = 0;
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
         if (json_object_object_get_ex (jobj, "zone", &jtmp))
            zone = json_object_get_int (jtmp);
         if ((zone > 0) && (zone <= REALZONES))
         {
            chanmap[zone].zone = zone;
            chanmap[zone].type = 0;
            if (json_object_object_get_ex (jobj, "flow", &jtmp))
               chanmap[zone].flow = json_object_get_int (jtmp);
            if (json_object_object_get_ex (jobj, "current", &jtmp))
               chanmap[zone].current = json_object_get_int (jtmp);
            if (json_object_object_get_ex (jobj, "ispump", &jtmp))
               chanmap[zone].type |= json_object_get_boolean (jtmp) ? ISPUMP : 0;
            if (json_object_object_get_ex (jobj, "isfrost", &jtmp))
               chanmap[zone].type |= json_object_get_boolean (jtmp) ? ISFROST : 0;
            if (json_object_object_get_ex (jobj, "isstock", &jtmp))
               chanmap[zone].type |= json_object_get_boolean (jtmp) ? ISSTOCK : 0;
            if (json_object_object_get_ex (jobj, "istest", &jtmp))
               chanmap[zone].type |= json_object_get_boolean (jtmp) ? ISTEST : 0;
            if (json_object_object_get_ex (jobj, "issensor", &jtmp))
               chanmap[zone].type |= json_object_get_boolean (jtmp) ? ISSENSOR : 0;
            if (json_object_object_get_ex (jobj, "isoutput", &jtmp))
               chanmap[zone].type |= json_object_get_boolean (jtmp) ? ISOUTPUT : 0;
            if (json_object_object_get_ex (jobj, "isspare", &jtmp))
               chanmap[zone].type |= json_object_get_boolean (jtmp) ? ISSPARE : 0;

            if (chanmap[zone].type & ISPUMP)
            {
               // find a free pump slot
               for (pump = 0; pump < MAXPUMPS; pump++)
               {
                  if (pumpmap[pump].zone == 0)
                  {
                     pumpmap[pump].zone = zone;
                     if (json_object_object_get_ex (jobj, "minflow", &jtmp))
                        pumpmap[pump].minflow = json_object_get_int (jtmp);
                     if (json_object_object_get_ex (jobj, "nomflow", &jtmp))
                        pumpmap[pump].nomflow = json_object_get_int (jtmp);
                     if (json_object_object_get_ex (jobj, "maxflow", &jtmp))
                        pumpmap[pump].maxflow = json_object_get_int (jtmp);
                     if (json_object_object_get_ex (jobj, "maxstarts", &jtmp))
                        pumpmap[pump].maxstarts = json_object_get_int (jtmp);
                     if (json_object_object_get_ex (jobj, "maxrun", &jtmp))
                        pumpmap[pump].maxrun = json_object_get_int (jtmp);
                     if (json_object_object_get_ex (jobj, "start", &jtmp))
                        pumpmap[pump].start = json_object_get_int (jtmp);
                     if (json_object_object_get_ex (jobj, "end", &jtmp))
                        pumpmap[pump].end = json_object_get_int (jtmp);
                     pumpmap[pump].pumpingtime = 0;   // reset count
                     chanmap[zone].link = pump;         // cross link refs
                     break;
                  }
               }
            }

            if (chanmap[zone].type & ISSENSOR)
            {
               // find a free sensor slot
               for (sensor = 0; sensor < MAXSENSORS; sensor++)
               {
                  if (sensormap[sensor].zone == 0)
                  {
                     // found a free one!!
                     sensormap[sensor].zone = zone;
                     // extract the type string
                     if (json_object_object_get_ex (jobj, "type", &jtmp))
                        strncpy (type, json_object_get_string (jtmp), 12);

                     // then try and match it (case insensitive)
                     for (i = 0; i < eMAXSENSE; i++)
                     {
                        if (strncasecmp(type, sensornames[i], 12) == 0)
                        {
                           sensormap[sensor].type = i;
                           break;
                        }
                     }
                     if (i == eMAXSENSE)
                     {
                        log_printf (LOG_INFO, "Invalid sensor type %s", type);
                        sensormap[sensor].zone = 0;           // invalid sensor type
                        break;
                     }
                     chanmap[zone].link = sensor;         // cross link refs
                     if (json_object_object_get_ex (jobj, "path", &jtmp))
                        strncpy (sensormap[sensor].path, json_object_get_string (jtmp), 33);

                     break;
                  }
               }
            }

            if (json_object_object_get_ex (jobj, "aorb", &jtmp))
               chanmap[zone].AorB = json_object_get_boolean (jtmp);
            // careful with string input in case its not actually there!!
            if (json_object_object_get_ex (jobj, "address", &jtmp))
               strncpy (chanmap[zone].address, json_object_get_string (jtmp), 17);
            if (json_object_object_get_ex (jobj, "name", &jtmp))
               strncpy (chanmap[zone].name, json_object_get_string (jtmp), 33);

            // if a zone has the group attribute then it controls the list of zones in that group
            if (json_object_object_get_ex (jobj, "group", &jtmp))
               group = json_object_get_int (jtmp);
            if ((group > 0) && (group <= MAXGROUPS))
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

               if (json_object_object_get_ex (jobj, "zones", &jlist))
               {
                  for (i = 0; i < json_object_array_length (jlist); i++)
                  {
                     chanmap[json_object_get_int (json_object_array_get_idx (jlist, i))].group |= 1 << (group - 1);
                  }
                  chanmap[zone].group = 1 << (group - 1);
                  chanmap[zone].type |= ISGROUP;
               }
            }
            chanmap[zone].valid |= CONFIGURED;
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
             "zone %d, %s name %s, port %c, addr %s, flow %d, start %s %02d:%02d:%02d (%lu), dur %d, per %d, freq %d valid %d use %d group %02x type %02x tf %2.5f lock %d last %lu days [%d %d %d %d %d %d %d]\n",
             chanmap[zone].zone, state[chanmap[zone].state], chanmap[zone].name,
             chanmap[zone].AorB ? 'A' : 'B', chanmap[zone].address, chanmap[zone].flow,
             daystr[tm.tm_wday], tm.tm_hour, tm.tm_min, tm.tm_sec, chanmap[zone].starttime,
             chanmap[zone].duration, chanmap[zone].period, chanmap[zone].frequency,
             chanmap[zone].valid, chanmap[zone].useful, chanmap[zone].group, chanmap[zone].type,
             chanmap[zone].totalflow, chanmap[zone].locked, chanmap[zone].lastrun,
             chanmap[zone].daylist[0], chanmap[zone].daylist[1], chanmap[zone].daylist[2], chanmap[zone].daylist[3], chanmap[zone].daylist[4], chanmap[zone].daylist[5], chanmap[zone].daylist[6]);
      }
   }
}

void
print_pumpmap (void)
{
   int pump;

   for (pump = 0; pumpmap[pump].zone; pump++)
   {
      log_printf(LOG_DEBUG, "pump %d, zone %d, minflow %d, nom %d, max %d, starts %d, run %d, start, %d end %d, time %lu\n",
         pump, pumpmap[pump].zone, pumpmap[pump].minflow, pumpmap[pump].nomflow, pumpmap[pump].maxflow, 
         pumpmap[pump].maxstarts, pumpmap[pump].maxrun, pumpmap[pump].start, pumpmap[pump].end, pumpmap[pump].pumpingtime);
   }
}


void
print_sensormap (void)
{
   int sensor;
   for (sensor = 0; sensormap[sensor].zone; sensor++)
   {
      log_printf(LOG_DEBUG, "sensor %d, name \"%s\", type %s, address \"%s\", path \"%s\"\n",
         sensor, chanmap[sensormap[sensor].zone].name, sensornames[sensormap[sensor].type], chanmap[sensormap[sensor].zone].address, sensormap[sensor].path);
   }
}




