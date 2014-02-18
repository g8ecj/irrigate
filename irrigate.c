//---------------------------------------------------------------------------
// Copyright (C) 2009-2010 Robin Gilks
//
//
//  irrigate.c   -   This program controls a number of valves and pumps that constitutes
//                   an irrigation system.
//
//  History:   0.10 - First release. 
//             0.20 - frost protect, repeat, persistent data
//             0.30 - improved display for future events, 24hr delay
//             0.40 - shake a whole load of bugs out, clean schedule file correctly
//             0.50 - monitor current drawn by valves
//             0.60 - statistics gathering
//             0.70 - split into several files
//             2.10 - sum polled current expectation on each check
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

// TODO - requires state machine to handle requests for zones to go active that fail due to open/short circuit solenoid valves
//      - this needs to reflect into the demand for the well pump, the GUI and the syslog data

// Include files
#include <signal.h>
#include "irrigate.h"


// globals that all in this module can see
struct mapstruct chanmap[MAXZONES];

time_t basictime;

struct mg_server *server;

int8_t wellzone = -1;
int16_t wellmaxflow = 0;
int16_t wellmaxstarts = 0;
int8_t dpfeed = -1;
int16_t dpstart = 0700;
int16_t dpend = 2100;
uint32_t welltime = 0;
uint8_t frost_mode = FROST_OFF;
bool frost_armed = FALSE;
bool controlC = FALSE;
int16_t interrupt = 0;
double temperature = 0;
double temperature1 = 0, temperature2 = 0;
double Tintegral = 0;

char daystr[][10] = {
   "Sunday",
   "Monday",
   "Tuesday",
   "Wednesday",
   "Thursday",
   "Friday",
   "Saturday"
};




static void
sig_hup (int UNUSED (signo))
{

}

static void
sig_usr (int signo)
{
   interrupt = signo;
}

static void
sig_ctlc (int UNUSED (signo))
{
   controlC = TRUE;
}



//--------------------------------------------------------------------------
// This is the begining of the program that controls the different zones
int
main (int argc, char **argv)
{
   int i;                       //loop counter
   uint8_t zone;
   uint8_t action;
   uint16_t numgpio;
   int16_t T1 = -1, T2 = -1;
   time_t lastsec = 0, lastmin = 0, frosttime = 0;

   basictime = time (NULL);

   for (i = 0; i < MAXZONES; i++)
   {
      chanmap[i].zone = 0;
      chanmap[i].valid = 0;
      chanmap[i].state = IDLE;
      chanmap[i].output = 0;
      chanmap[i].starttime = 0;
      chanmap[i].duration = 0;
      chanmap[i].totalflow = 0;
      chanmap[i].locked = FALSE;
      chanmap[i].name[0] = '\0';
   }

   openlog ("irrigate", LOG_PERROR, LOG_USER);

   log_printf (LOG_NOTICE, "Irrigation Controller Version %s Copyright (c) 2009-2014 Robin Gilks", VERSION);

   parseArguments (argc, argv);

   if (signal (SIGINT, sig_ctlc) == SIG_ERR)
   {
      log_printf (LOG_ERR, "Error: could not setup int handler");
      exit (EXIT_FAILURE);
   }
   if (signal (SIGTERM, sig_ctlc) == SIG_ERR)
   {
      log_printf (LOG_ERR, "Error: could not setup term handler");
      exit (EXIT_FAILURE);
   }
   if (signal (SIGHUP, sig_hup) == SIG_ERR)
   {
      log_printf (LOG_ERR, "Error: could not setup hup handler");
      exit (EXIT_FAILURE);
   }
   if (signal (SIGUSR1, sig_usr) == SIG_ERR)
   {
      log_printf (LOG_ERR, "Error: could not setup usr1 handler");
      exit (EXIT_FAILURE);
   }
   if (signal (SIGUSR2, sig_usr) == SIG_ERR)
   {
      log_printf (LOG_ERR, "Error: could not setup usr2 handler");
      exit (EXIT_FAILURE);
   }

   log_printf (LOG_INFO, "Opening device %s", device);

   // start up all the one-wire stuff
   numgpio = irr_onewire_init (&T1, &T2);

   if (!readchanmap ())
   {
      if (config)
      {
         createchanmap (numgpio);
         savechanmap ();
         exit (EXIT_SUCCESS);
      }
      else
      {
         printf ("Error: no valid config file. Rerun with the -c (--config) switch to build configuration file\n");
         exit (EXIT_FAILURE);
      }
   }
   else
   {
      if (config)
      {
         printf
            ("Error: a valid configuration file already exists at %s\nDelete this first before running with the --config option\n",
             configfile);
         exit (EXIT_FAILURE);
      }
   }

   // match up the 1-wire addresses to zones
   irr_match (numgpio);

   if (background)
   {
      /* Our process ID */
      pid_t pid;

      closelog ();
      openlog ("irrigate", 0, LOG_USER);        // don't try and talk to stderr if in the background

      /* Fork off the parent process */
      pid = fork ();
      if (pid < 0)
      {
         log_printf (LOG_ERR, "Failed to run in background");
         exit (EXIT_FAILURE);
      }
      /* If we got a good PID, then we can exit the parent process. */
      if (pid > 0)
      {
         log_printf (LOG_INFO, "Running in background as pid %d", pid);
         exit (EXIT_SUCCESS);
      }
      /* Change the file mode mask */
      umask (0);
      /* Close out the standard file descriptors */
      close (STDIN_FILENO);
      close (STDOUT_FILENO);
      close (STDERR_FILENO);
   }

   server = mg_create_server(NULL);
   irr_web_init (server);


   // read scheduling info from file (persistent data)
   read_schedule ();
   insert (basictime, RESCHEDULE, NOACTION);    // check it over immediately...

/* The Big Loop */
   do
   {
      // poll the web server
      mg_poll_server(server, 0);

      // all actions are synchronised by the seconds count of the timer
      basictime = time (NULL);
      // every time round the loop, check the head of the timer queue and see if anything needs processing
      // things are put on the queue mostly by the doaction() and set_state() functions
      while ((zone = dequeuezone (&action)) > 0)
      {
         delete (zone);         // remove from the time queue
         // the zone may be a virtual one that determines some special action
         if (zone == FROST_LOAD)
         {
            frost_load ();
         }
         else if (zone == RESCHEDULE)
         {
            check_schedule (FALSE);
            update_statistics ();
            clean_history ();
            chanmap[zone].period = 3600;
            insert (basictime + 3600, RESCHEDULE, NOACTION);    // do again every hour...
         }
         else if (zone == RESET)
         {
            doreset(numgpio);
         }
         else if (chanmap[zone].type & ISGROUP)
         {
            dogroup (zone, action);
         }
         else if (chanmap[zone].type & ISTEST)
         {
            test_load (zone, action);
         }
         else
         {
            doaction (zone, action);
         }
      }


      // manage the pumps
      manage_pumps();

      // a couple of things we do every second
      if (basictime >= lastsec + 1)
      {
         lastsec = basictime;

         // poll the current being drawn by the valves and see if its as expected
         if (!check_current())
         {
            log_printf (LOG_ERR,
                     "Polled current out of spec. Doing emergency shutdown");
            emergency_off(ERROR);
         }

      }

      /* only read and check the temperature every minute */
      if (basictime >= lastmin + 60)
      {
         int num = 0;
         double Ttotal = 0;
         lastmin = basictime;

         if (T1 >= 0)
         {
            temperature1 = GetTemp (T1);
            if (temperature1 != -999)
            {
               Ttotal += temperature1;
               num++;
            }
         }
         if (T2 >= 0)
         {
            temperature2 = GetTemp (T2);
            if (temperature2 != -999)
            {
               Ttotal += temperature2;
               num++;
            }
         }
         if (num)               // only if any temperature sensors present
         {
            temperature = Ttotal / num;
         }

         if ((num) && (frost_armed))    // only process low temperature if have sensors and we are armed
         {
            // calculate degree-minutes as we go under a threshold - if we have a frost limit defined
            // its OK to carry on increasing the integrated temperaure-time value as we only come out if above the threshold
            if ((temperature < Tthreshold) && (frostlimit > 0))
            {
               // the bigger the difference, the quicker we go into frost protect mode
               if (Tintegral == 0)
               {
                  log_printf (LOG_NOTICE, "Below frost threshold - starting temperature integration at %2.2fC", temperature);
                  frosttime = basictime;
               }
               Tintegral += (Tthreshold - temperature);
               // only start frost protection mode if not already active
               if ((Tintegral > frostlimit) && (frost_mode == FROST_OFF))
               {
                  log_printf (LOG_NOTICE,
                           "Starting frost protect program - temperature is %2.2fC, integration time is %ld minutes",
                           temperature, (basictime - frosttime) / 60);
                  frost_cancel ();      // clear any current activity in the frost zones
                  frost_load ();
                  frost_mode = FROST_ACTIVE;
               }
            }
            else
            {
               // if now above threshold and activated by integrating temperature over time - turn it off and cancel integration
               if (temperature > Tthreshold)
               {
                  if (frost_mode == FROST_ACTIVE)
                  {
                     log_printf (LOG_NOTICE, "Stopping frost protect program - temperature is %2.2fC", temperature);
                     frost_cancel ();
                     frost_mode = FROST_OFF;
                  }
                  Tintegral = 0;
               }
            }
         }
      }


      usleep (100);

      if (interrupt == SIGUSR1)
      {
#ifdef PC
         extern uint16_t testcurrent;
         testcurrent = 1;
#endif
         log_printf (LOG_NOTICE,
                  "Temperatures T1 = %2.2fC, T2 = %2.2fC, integral value = %2.2f, current = %d",
                  temperature1, temperature2, Tintegral, GetCurrent());
         interrupt = 0;
      }
      if (interrupt == SIGUSR2)
      {
#ifdef PC
         extern uint16_t testcurrent;
         testcurrent = 0;
//#else
//         dump_log_msgs();
         print_chanmap ();
         print_queue ();
         update_statistics ();
         check_schedule (FALSE);
#endif

         interrupt = 0;
      }
   }
   while (!controlC);

   doreset(numgpio);

   if (debug > 1)
      print_chanmap ();

   irr_onewire_stop ();

   update_statistics ();
   mg_destroy_server(&server);

   return 0;
}
