//---------------------------------------------------------------------------
// Copyright (C) 2009-2014 Robin Gilks
//
//
//  irr_parse.c   -   This section looks after parsing the command line arguments
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



#include <getopt.h>

#include "irrigate.h"


// config items
int16_t debug = 0;
bool timestamp = FALSE;
bool fileflag = FALSE;
bool background = FALSE;
bool config = FALSE;
bool monitor = FALSE;
char device[MAXDEVLEN] = "/dev/ttyUSB0";
char configfile[MAXFILELEN] = "/www/zones.conf";
char datapath[MAXFILELEN] = "/www";
char httpport[10] = "8080";
char httproot[MAXFILELEN] = "/www";
char accesslog[MAXFILELEN] = "";
double Tthreshold = -0.5;
uint16_t frostlimit = 60;




int
modify_passwords_file (const char *fname, const char *domain, const char *user, const char *pass)
{
   int found;
   char line[512], u[512], d[512], ha1[33], tmp[1024];
   FILE *fp, *fp2;

   found = 0;
   fp = fp2 = NULL;

   // Regard empty password as no password - remove user record.
   if (pass != NULL && pass[0] == '\0')
   {
      pass = NULL;
   }

   (void) snprintf (tmp, sizeof (tmp), "%s.tmp", fname);

   // Create the file if does not exist
   if ((fp = fopen (fname, "a+")) != NULL)
   {
      fclose (fp);
   }

   // Open the given file and temporary file
   if ((fp = fopen (fname, "r")) == NULL)
   {
      return 0;
   }
   else if ((fp2 = fopen (tmp, "w+")) == NULL)
   {
      fclose (fp);
      return 0;
   }

   // Copy the stuff to temporary file
   while (fgets (line, sizeof (line), fp) != NULL)
   {
      if (sscanf (line, "%[^:]:%[^:]:%*s", u, d) != 2)
      {
         continue;
      }

      if (!strcmp (u, user) && !strcmp (d, domain))
      {
         found++;
         if (pass != NULL)
         {
            mg_md5 (ha1, user, ":", domain, ":", pass, NULL);
            fprintf (fp2, "%s:%s:%s\n", user, domain, ha1);
         }
      }
      else
      {
         fprintf (fp2, "%s", line);
      }
   }

   // If new user, just add it
   if (!found && pass != NULL)
   {
      mg_md5 (ha1, user, ":", domain, ":", pass, NULL);
      fprintf (fp2, "%s:%s:%s\n", user, domain, ha1);
   }

   // Close files
   fclose (fp);
   fclose (fp2);

   // Put the temp file in place of real file
   remove (fname);
   rename (tmp, fname);

   return 1;
}




/**
 parseArguments
 * Parse command line arguments.
 */
void
parseArguments (int argc, char **argv)
{
   int c;

   static struct option long_options[] = {
      {"accesslog",  required_argument, NULL, 'a'},
      {"background", no_argument, NULL,       'b'},
      {"config",     no_argument, NULL,       'c'},
      {"datapath",   required_argument, NULL, 'd'},
      {"file",       required_argument, NULL, 'f'},
      {"help",       no_argument, NULL,       'h'},
      {"limit",      required_argument, NULL, 'l'},
      {"monitor",    no_argument, NULL,       'm'},
      {"port",       required_argument, NULL, 'p'},
      {"root",       required_argument, NULL, 'r'},
      {"serialport", required_argument, NULL, 's'},
      {"threshold",  required_argument, NULL, 't'},
      {"version",    no_argument, NULL,       'v'},
      {"debug",      required_argument, NULL,       'x'},
      {NULL, 0, NULL, 0}
   };

  // Edit passwords file if -A option is specified
   if (argc > 1 && !strcmp (argv[1], "-A"))
   {
      if (argc != 6)
      {
         printf("%s -A <htpasswd_file> <realm> <user> <passwd>\n", argv[0]);
         exit (EXIT_FAILURE);
      }
      exit (modify_passwords_file (argv[2], argv[3], argv[4], argv[5]) ? EXIT_SUCCESS : EXIT_FAILURE);
   }

   while (TRUE)
   {
      c = getopt_long (argc, argv, "a:bcd:f:hl:mp:r:s:t:vx:", long_options, NULL);
      if (c == -1)
         break;

      switch (c)
      {
      case 'a':
         strncpy (accesslog, optarg, MAXFILELEN);
         log_printf (LOG_INFO, "Using %s for access log file", accesslog);
         break;
      case 'b':
         background = TRUE;
         break;
      case 'c':
         config = TRUE;
         break;
      case 'd':
         strncpy (datapath, optarg, MAXFILELEN);
         log_printf (LOG_INFO, "Using %s for persistent schedule and statistics data", datapath);
         break;
      case 'f':
         strncpy (configfile, optarg, MAXFILELEN);
         log_printf (LOG_INFO, "Getting config from %s", configfile);
         break;
      case 'h':
         printf ("usage: %s [bcdfhlprstv]\n"
                 "\n"
                 "-a <file>, --file      access log file name\n"
                 "-b,        --background run in daemon mode\n"
                 "-c,        --config    configure zone mapping\n"
                 "-d <dir>,  --datapath  data file directory\n"
                 "-f <file>, --file      config file name\n"
                 "-l <int>,  --limit     degree-minutes before frost program runs\n"
                 "-m,        --monitor   check current drawn by solenoid valves\n"
                 "-p <int>,  --port      http port number\n"
                 "-r <dir>,  --root      http root directory\n"
                 "-s <dev>,  --serialport serial port device\n"
                 "-t <int>,  --threshold temperature below which frost protect operates\n"
                 "-v,        --version   useful version information\n"
                 "-x         --debug     debug mode\n"
                 "-h,        --help      this help screen\n", argv[0]);
         printf(" or: %s -A <htpasswd_file> <realm> <user> <passwd>\n", argv[0]);

         exit (0);
      case 'l':
         frostlimit = atoi (optarg);
         if (frostlimit > 360)
         {
            printf ("Warning: Maximum frostlimit is 360 degree minutes. Input was %d\n", frostlimit);
         }
         log_printf (LOG_NOTICE, "Frost protection active after %d degree-minutes", frostlimit);
         break;
      case 'm':
         monitor = TRUE;
         log_printf (LOG_NOTICE, "Current monitor of valves enabled");
         break;
      case 'p':
         strncpy (httpport, optarg, 10);
         break;
      case 'r':
         strncpy (httproot, optarg, MAXFILELEN);
         break;
      case 's':
         strncpy (device, optarg, MAXDEVLEN);
         break;
      case 't':
         Tthreshold = atof (optarg);
         if ((Tthreshold < -10) || (Tthreshold > 0))
         {
            printf ("Warning: threshold range is -10 to 0. Input was %2.2f\n", Tthreshold);
         }
         log_printf (LOG_NOTICE, "Frost protection threshold temperature is %2.2f", Tthreshold);
         break;
      case 'v':
         printf ("Irrigation Controller Version %s Copyright (c) 2009-2014 Robin Gilks\n", VERSION);
         printf ("Released under GPL Version 2\n");
         break;
      case 'x':
         debug = atoi (optarg);
         if ((debug < 0) || (debug > 6))
         {
            printf ("Invalid debug level set - resetting\n");
            debug = 0;
         }
         break;
      case '?':
         printf ("Unknown option \"%s\". Use -h for help\n", argv[optind - 1]);
         exit (EXIT_FAILURE);
      default:
         break;
      }
   }
   optind = 1;
}
