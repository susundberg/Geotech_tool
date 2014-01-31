

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.h"
#define MODULE_NAME "main"

///-------------------------------------------------------------------------------------
int GLOBAL_debug_level = 2;


///-------------------------------------------------------------------------------------
/// LOCAL HEADERS
///-------------------------------------------------------------------------------------
typedef struct
{
 const char* device;
 unsigned int param_int;
 const char* param_str;
 unsigned int mode;
} Setup;

bool get_runmode_etc( int argc, char** argv, Setup* setup);

#define MODE_RESET    1
#define MODE_QUERY    2
#define MODE_SET      3
#define MODE_DOWNLOAD 4
#define MODE_CLEAR    5


///-------------------------------------------------------------------------------------
///-------------------------------------------------------------------------------------
void usage()
{
      printf("usage: ./geotech <device> <mode> <param>, where mode is one of following:\n");
      printf("       reset -- send reset pulse to device \n");
      printf("       query -- query device for the sampling rate\n");
      printf("       set   -- set the device sampling rate given in <param>\n");
      printf("       download -- download all data points from the device, save in GPX format to file <param>\n");
      printf("       clear -- clear all data points from the device\n");
      exit(1);
}



///-------------------------------------------------------------------------------
///-------------------------------------------------------------------------------
int main(int argc, char** argv)
{
    Setup setup;
   
   int tries ;
   
   int serial_fd = 0;
   int ret;
   unsigned char* buffer = NULL;
   
   if (!get_runmode_etc( argc, argv, &setup))
      return -1;
   
   // buffer must be +1 sized for seeking \r\n.
   buffer = (unsigned char*) malloc( BUFFER_SIZE + 1);
   if ( buffer == NULL )
   {
      ERROR("Out of memory!");
      return false; 
   }
   
   if ( setup.mode != MODE_RESET )
   {
      if ( serial_init_highspeed( setup.device, buffer,  &serial_fd ) != true )
      {
         free(buffer);
         return 1;
      }
   }
   
   if ( setup.mode == MODE_QUERY )
   {
      unsigned int sample = 0;
      if (serial_query_sampling( serial_fd, buffer, &sample ) != 0)
      {
         ERROR("Query failed!\n");
      }
      else
      {
         printf("---------------------------------------------------------------------------------------\n");
         printf("  SAMPLING STEP: %02d s \n", sample );
         printf("---------------------------------------------------------------------------------------\n");
      }
   }   
   else if ( setup.mode == MODE_SET )
   {
      if (serial_set_sampling( serial_fd, buffer, setup.param_int ) != 0)
      {
         ERROR("Set failed!\n");
      }
      else
      {
         printf("---------------------------------------------------------------------------------------\n");
         printf("  SAMPLING STEP SET TO : %02d s \n", setup.param_int );
         printf("---------------------------------------------------------------------------------------\n");
      }
   }  
   else if ( setup.mode == MODE_DOWNLOAD )
   {
      GPS_points datapoints;
      
      GPS_points_init( &datapoints );
      
      if (serial_download( serial_fd, buffer, &datapoints ) != 0)
      {
         ERROR("Download failed!\n");
      }
      else
      {
         printf("---------------------------------------------------------------------------------------\n");
         printf("  DOWNLOAD DONE: %d datapoints aquired. Saving to file '%s'\n", datapoints.npoints, setup.param_str );
         printf("---------------------------------------------------------------------------------------\n");
      }
      
      if (!GPS_points_write( &datapoints, setup.param_str ))
         return 1;
      
      GPS_points_free( &datapoints );
   }  
   else if ( setup.mode == MODE_CLEAR )
   {
      if (serial_clear_datapoints( serial_fd, buffer ) != 0)
      {
         ERROR("CLEAR failed!\n");
      }
      else
      {
         printf("---------------------------------------------------------------------------------------\n");
         printf("  DEVICE CLEARED \n");
         printf("---------------------------------------------------------------------------------------\n");
      }
   }
          
   serial_reset( serial_fd, buffer ) ;
   close( serial_fd );
   free(buffer),
   exit(0);
}




///-------------------------------------------------------------------------------
///-------------------------------------------------------------------------------
bool get_runmode_etc( int argc, char** argv, Setup* setup)
{
   if (argc < 3)
      usage();
   
   setup->device = argv[1];
   
   if (strcasecmp("reset", argv[2] ) == 0 )
   {
      if ( argc != 3 )
         usage();

      setup->mode = MODE_RESET;
   }
   else if (strcasecmp("query", argv[2] ) == 0 )
   {
      if ( argc != 3 )
         usage();
      
      setup->mode = MODE_QUERY;
   }
   else if (strcasecmp("set", argv[2] ) == 0 )
   {
      setup->mode = MODE_SET;
      
      if ( argc != 4 )
         usage();
      
      setup->param_int = atoi( argv[3] );
   }
   else if (strcasecmp("download", argv[2] ) == 0 )
   {
      setup->mode = MODE_DOWNLOAD;
      
      if ( argc != 4 )
         usage();
      
      setup->param_str = argv[3] ;
   }   
   else if (strcasecmp("clear", argv[2] ) == 0 )
   {
      setup->mode = MODE_CLEAR;
      
      if ( argc != 3 )
         usage();
      
      setup->param_str = argv[3] ;
   }  
   else
   {
      ERROR("Unkown mode: %s " , argv[2] );
      return false;
   }
   return true;
}   