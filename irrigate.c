//---------------------------------------------------------------------------
// Copyright (C) 2009-2014 Robin Gilks
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


// Include files
#include <signal.h>
#include "irrigate.h"


time_t basictime;
time_t startuptime;

struct mg_server *server;

uint8_t frost_mode = FROST_OFF;
bool frost_armed = FALSE;
bool controlC = FALSE;
int16_t interrupt = 0;
double temperature = 0;
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
   uint8_t zone;
   uint8_t action;
   uint16_t numdev;
   time_t lastsec = 0, lastmin = 0, frosttime = 0;

   basictime = time (NULL);
   startuptime = basictime;

   maps_init();

   openlog ("irrigate", LOG_PERROR, LOG_USER);

   log_printf (LOG_NOTICE, "Irrigation Controller Version %s Copyright (c) 2009-2015 Robin Gilks", VERSION);

   parseArguments (argc, argv);

   if (signal (SIGINT, sig_ctlc) == SIG_ERR)
   {
      log_printf (LOG_EMERG, "Error: could not setup int handler");
      exit (EXIT_FAILURE);
   }
   if (signal (SIGTERM, sig_ctlc) == SIG_ERR)
   {
      log_printf (LOG_EMERG, "Error: could not setup term handler");
      exit (EXIT_FAILURE);
   }
   if (signal (SIGHUP, sig_hup) == SIG_ERR)
   {
      log_printf (LOG_EMERG, "Error: could not setup hup handler");
      exit (EXIT_FAILURE);
   }
   if (signal (SIGUSR1, sig_usr) == SIG_ERR)
   {
      log_printf (LOG_EMERG, "Error: could not setup usr1 handler");
      exit (EXIT_FAILURE);
   }
   if (signal (SIGUSR2, sig_usr) == SIG_ERR)
   {
      log_printf (LOG_EMERG, "Error: could not setup usr2 handler");
      exit (EXIT_FAILURE);
   }

   log_printf (LOG_INFO, "Opening device %s", device);


   // start up all the one-wire stuff
   numdev = irr_onewire_init ();

   if (!readchanmap ())
   {
      if (config)
      {
         createchanmap (numdev);
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

   // match up the addresses to zones
   irr_onewire_match (numdev);


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
         log_printf (LOG_EMERG, "Failed to run in background");
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

   irr_web_init ();

   // read scheduling info from file (persistent data)
   read_schedule ();
   insert (basictime, RESCHEDULE, NOACTION);    // check it over immediately...

/* The Big Loop */
   do
   {
      // poll the web server
      irr_web_poll();

      // all actions are synchronised by the seconds count of the timer
      basictime = time (NULL);
      // every time round the loop, check the head of the timer queue and see if anything needs processing
      // things are put on the queue mostly by the doaction() and set_state() functions
      while ((zone = peek_queue (&action)) > 0)
      {
         delete (zone);         // remove from the time queue
         // the zone may be a virtual one that determines some special action
         if (zone == FROST_LOAD)
         {
            dofrost ();
         }
         else if (zone == RESCHEDULE)
         {
            check_schedule (FALSE);
            update_statistics ();
            clean_history ();
            insert (basictime + 3600, RESCHEDULE, NOACTION);    // do again every hour...
         }
         else if (zone == RESET)
         {
            doreset();
         }
         else if (chanmap[zone].type & ISGROUP)
         {
            dogroup (zone, action);
         }
         else if (chanmap[zone].type & ISTEST)
         {
            dotest (zone, action);
         }
         else
         {
            doaction (zone, action);
         }
      }


      // manage the pumps
      dopumps();

      // a couple of things we do every second
      if (basictime >= lastsec + 1)
      {
         lastsec = basictime;

         // poll the current being drawn by the valves and see if its as expected
         if (!check_current())
         {
            errno = ERANGE;
            log_printf (LOG_ERR, "Polled current out of spec. Doing emergency shutdown");
            emergency_off(ERROR);
         }

      }

      /* only read and check the temperature every minute */
      if (basictime >= lastmin + 60)
      {
         lastmin = basictime;

         temperature = GetTemp ();

         if ((temperature != -999) && (frost_armed))    // only process low temperature if have sensors and we are armed
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
                  dofrost ();
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
         testcurrent = 4;
#endif
         log_printf (LOG_NOTICE,
                  "Temperature = %2.2fC, integral value = %2.2f, current = %d",
                  temperature, Tintegral, GetCurrent());
         interrupt = 0;
      }
      if (interrupt == SIGUSR2)
      {
#ifdef PC
         extern uint16_t testcurrent;
         testcurrent = 0;
         update_statistics ();
//#else
#endif
//         dump_log_msgs();
         print_chanmap ();
         print_pumpmap ();
         print_sensormap ();
         print_queue ();
//         check_schedule (FALSE);

         interrupt = 0;
      }
   }
   while (!controlC);

   update_statistics ();

   doreset();

   if (debug > 1)
      print_chanmap ();

   irr_onewire_stop ();

   mg_destroy_server(&server);

   return 0;
}
