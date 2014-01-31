#include <stdio.h>

#include <unistd.h>
#include <termio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>                    
                    
#include "common.h"

#define MODULE_NAME "comm"

#define SERIAL_DATABITS CS8 // 1 stop bit no parity checking

#include <stdlib.h>
#include <stdint.h>

#include "messages.h"

static bool compare_responce( const unsigned char* input, const unsigned char* orig, unsigned int orig_len, unsigned int msg_len ) ;
static int serial_read(int serial_fd, unsigned char* buffer, unsigned int max_len, unsigned int* red_bytes);

static bool serial_write(int serial_fd,  const unsigned char* message, unsigned int len) ;
static int serial_set_highspeed( int serial_fd , unsigned char* buffer  );
static void print_message( unsigned char* buffer, int len );



///--------------------------------------------------------------------------------------------------------------------
/// Download all datapoints from the device
///--------------------------------------------------------------------------------------------------------------------
int serial_clear_datapoints( int serial_fd, unsigned char* buffer )
{
  if ( !serial_write(serial_fd, msg_clear_samples, 7) != 0)
   {
      ERROR("Serial CLEAR failed at write!");
      return -1;
   }
   
   return 0;
}


float convert_float( const unsigned char* buffer )
{
   uint64_t value = 0;
   value = value + buffer[0];
   value = value + (buffer[1] << 8);
   value = value + (buffer[2] << 16) ;
   value = value + (buffer[3] << 24);
   
   //printf("Map value of %x %x %x %x -> %ld\n", buffer[0], buffer[1], buffer[2], buffer[3], value  );
   return value*0.000001;
}

void convert_time( int32_t* date, const unsigned char* buffer )
{
   // take 1,2,4,8,32 for seconds, 
   date[0] = (buffer[0]  & 0b00111111 );
   // minutes bits 1=1,2=2 comes from buffer 0
   // minutes bits 3=4,4=8,5=16,6=32 comes from buffer 0
   date[1] = ((buffer[0] & 0b11000000 )>>6)   + (( buffer[1] & 0b00001111) << 2 );
   // HOURS: 4=1h,5=2h,6=4h,7=8h
   // bits 1=16h
   date[2] = ((buffer[1] & 0b11110000) >> 4 ) + (( buffer[2] & 0b00000001) << 4 );
   // days:
   // 2=1d,4=2d,8=4d,16=8d,32=16d
   date[3] = ((buffer[2] & 0b00111110) >> 1 ) ;
   // months, 64=1m,128=2m, 1=4m,2=8
   date[4] = ((buffer[2] & 0b11000000) >> 6 ) + ((buffer[3] & 0b00000011) << 2 );
   
   // 4=1y,8=2y,16=4y,32=8y,64=16y,128=32y
   date[5] = 2000 + ((buffer[3] & 0b11111100) >> 2 ) ;
}

unsigned char calculate_entry_checksum( const unsigned char* buffer )
{
   unsigned char sum = 0;
   int loop = 0;
   
   for ( loop = 0; loop < 19; loop ++ )
   {
      sum = sum + buffer[loop];
   }
   sum = sum + 0xBA;
   return sum;
}

///--------------------------------------------------------------------------------------------------------------------
/// Download all datapoints from the device
///--------------------------------------------------------------------------------------------------------------------
int serial_download( int serial_fd, unsigned char* buffer, GPS_points* data )
{
   unsigned int red = 0;
   int loop;
   int ploop;
   
   DEBUG(2,"CALL: download samples ");
   
   memcpy( buffer, msg_download_start, 7 );
   if ( !serial_write(serial_fd, buffer, 7) != 0)
   {
      ERROR("Serial DOWNLOAD start failed at write!");
      return -1;
   }
   
   if ( serial_read( serial_fd, buffer, 0, &red ) != 0 )
   {
      ERROR("Serial DOWNLOAD failed at read!");
      return 1; 
   }
   
   // now we should have responce that has 3 first character indicating that command was understood
   if (!compare_responce( buffer, msg_download_resp_yes, 3, red ))
      return 1; 
   
   // Now seek for character ',' to determine number of datapoints
   unsigned char* entry1 = buffer + 3;

   
   for ( loop = 3; loop < red; loop ++ )
   {
      if ( buffer[loop] == ',' )
         break;
   }
   if ( loop == red )
   {
      DEBUG(3, "cannot find ',' to separate fields");
      return 1;
   }
   
   buffer[loop] = 0x00;
   unsigned char* entry2 = buffer + loop + 1;
   
   // then seek for '*' to end second datafield
   for ( loop = 3; loop < red; loop ++ )
   {
      if ( buffer[loop] == '*' )
         break;
   }
   if ( loop == red )
   {
      DEBUG(3, "cannot find '*' to separate fields");
      return 1;
   }
   buffer[loop] = 0x00;
   
   int number1 = atoi( (const char*)entry1 );
   int number2 = atoi( (const char*)entry2 );
   
   if ( number1 == 0 && number1 == number2 )
   {
      data->npoints = 0;
      return 0;
   }
   else if ( number1 > 0 && number2 == number1 + 1 )
   {
      data->npoints = number1;
   }
   else
   {
      ERROR("Unexpected numbers parsed : %d , %d " , number1, number2 );
      return 1;
   }   
   
   // then download the points
   data->points = (GPS_point*)malloc( data->npoints * sizeof(GPS_point) );
   
   // THE message is: 0x23, 0x23, 0xf7, <ascii index entry> 0x2a <check item> 0x0d, 0x0a
   // g = ( mod(i,10) + 39 ) + (i >= 10).*(49 + floor(mod(i,100)/10) - 1) + ( i>= 100).*(49 + floor(mod(i,1000)/100) - 1) + (i>=1000).*(49 + floor(i/1000) - 1)
   GPS_point* points = data->points;
   
   for ( ploop = 0; ploop < data->npoints; ploop ++ )
   {
      DEBUG(4,"downloading item %d ", ploop );
      memcpy( buffer, msg_download_entry, 3 );
      int len = sprintf( (char*)buffer + 3, "%d*", ploop );
      buffer[ 3 + len + 0] = DOWNLOAD_GET_CHECK( ploop );
      buffer[ 3 + len + 1] = 0x0d;
      buffer[ 3 + len + 2] = 0x0a;
      buffer[ 3 + len + 3] = 0x00;
   
      print_message( buffer, len + 6 );
      
      if (!serial_write( serial_fd, buffer, 3 + len + 3))
      {
         ERROR("Serial DOWNLOAD start failed at write!");
         return -1;
      }
      
      // then download the responce
      int offset = 0;
      int ret    = 0;
      int tries  = 0;
      for ( tries = 0; tries < 10; tries ++ )
      {
         ret = serial_read( serial_fd, buffer + offset , 20 - offset , &red ); 
         if (  ret == -1)
         {
            ERROR("Serial DOWNLOAD READ failed at write!");
            return -1;
         }
         
         if ( ret == 1 )
         {
            offset = offset + red;
         }
         
         if ( ret == 0 || offset >= 20  )
         {
            break;
         }
         
      }
      
      for ( loop = 20; loop < 32; loop ++ )
         buffer[loop]=0x00;
      
      if ( buffer[19] != calculate_entry_checksum( buffer ) )
      {
         ERROR("Serial DOWNLOAD READ failed at CHECKSUM!");
         return 1;
      }
      
      points[ ploop ].longitude = convert_float( &buffer[3] );
      points[ ploop ].latitude  = convert_float( &buffer[7] );
      points[ ploop ].height    = buffer[11] + (buffer[12]<<8);
      convert_time( (int32_t*)&points[ploop].time, &buffer[15] );
      
      //convert_time( points->
      // print_message( buffer, 32 );
      printf("Downloaded entry LON %.06f LAT %.06f HEI %f \n", points[ploop].longitude, points[ploop].latitude, points[ploop].height );
   }

   
   return 0;
}


///--------------------------------------------------------------------------------------------------------------------
/// Query for device sample rate
///--------------------------------------------------------------------------------------------------------------------
int serial_set_sampling( int serial_fd, unsigned char* buffer, int sample )
{
   
   DEBUG(2,"CALL: set sample rate ");
   
   memcpy( buffer, msg_sample_set, 11 );
   unsigned int tens  = sample/10;
   unsigned int units = sample - tens*10;
   
   if ( tens > 9 || units > 9)
   {
      ERROR("Sample rate must be 1 .. 99 \n");
   }
   
   buffer[3] = '0' + tens;
   buffer[4] = '0' + units;
   
   /*
   int loop = 0;
   printf("   set send: " );
   for ( loop = 0; loop < 11; loop ++ )
      printf(" %x ", buffer[loop] );
   printf("\n");
  */
   
   if ( !serial_write(serial_fd, buffer, 11) != 0)
   {
      ERROR("Serial SET raising failed at write!");
      return -1;
   }
   

   
   unsigned int red = 0;
   
   if ( serial_read( serial_fd, buffer, 7, &red ) != 0 )
   {
      ERROR("Serial SET failed at read!");
      return 1; 
   }
   
   if ( !compare_responce( buffer, msg_sample_set_resp, 3, red) )
   {
      return 1;
   }
   
   return 0;
}


///--------------------------------------------------------------------------------------------------------------------
/// Query for device sample rate
///--------------------------------------------------------------------------------------------------------------------
int serial_query_sampling( int serial_fd, unsigned char* buffer, unsigned int* sample_rate )
{
   DEBUG(2,"CALL: query for sample rate ");
   
   memcpy( buffer, msg_sample_query, 7 );
   
   if ( !serial_write(serial_fd, buffer, 7) != 0)
   {
      ERROR("Serial query raising failed at write!");
      return -1;
   }
   

   
   unsigned int red = 0;
   if ( serial_read( serial_fd, buffer, 0, &red ) != 0 )
   {
      ERROR("Serial query failed at read!");
      return 1; 
   }
   
   if ( !compare_responce( buffer, msg_sample_query_resp, 3, red ) )
   {
      return 1;
   }
   int loop = 0;
   
   for ( loop = 3; loop < red; loop ++ )
   {
      if ( buffer[loop] == ',')
         break;
   }
   if ( loop == red )
   {
      DEBUG(3,"cannot find delimiter");
      return 1;
   }
   
   buffer[loop]=0x00;
   
   
   *sample_rate = atoi( (char*)buffer + 3);
   return 0;
}


///--------------------------------------------------------------------------------------------------------------------
/// Query for device sample rate
///--------------------------------------------------------------------------------------------------------------------

#define MODE_SERIAL_TRY_HIGHSPEED 1
#define MODE_SERIAL_DO_RESET      2

bool serial_init_highspeed( const char* device, unsigned char* buffer, int* serial_fd_p )
{
   int serial_fd;
   int tries = 0;
   int mode = MODE_SERIAL_TRY_HIGHSPEED;
   
   for ( tries = 0; tries < 5; tries ++ )
   {
      serial_fd = open( device, O_RDWR | O_NOCTTY | O_NONBLOCK );
      if ( serial_fd <= 0)
      {
         ERROR("cannot open serial port: %s", strerror(errno)  );
         return false;
      }
      
      DEBUG (2,"opened device for fd %d " , serial_fd );
   
      int ret = 0;
      sleep(1);
      
      if ( mode == MODE_SERIAL_TRY_HIGHSPEED ) 
      {
         ret = serial_set_highspeed( serial_fd, buffer );
      }
      else if ( mode == MODE_SERIAL_DO_RESET )
      {
         ret = serial_reset( serial_fd, buffer ) ;
      }
      else
      {
         ERROR("Unknown mode.\n");
         return false;
      }

      
      if ( ret == -1)
      {
        DEBUG(2,"Init failed due system call. Exit. \n");
        close( serial_fd );
        return false;
      }
      
      if  (ret == 0)
      {
         if ( mode == MODE_SERIAL_TRY_HIGHSPEED )
         {
            DEBUG(2, "Device init ok. ");
            break;
         }
         else if ( mode == MODE_SERIAL_DO_RESET )
         {
            close( serial_fd );
            mode = MODE_SERIAL_TRY_HIGHSPEED;
         }
         else
         {
            ERROR("Unknown mode.\n");
            return false;
         }
      }
      else if ( ret == 1 )
      {
         close( serial_fd );
         mode = MODE_SERIAL_DO_RESET;
      }
   }
   
   if ( tries == 5 )
      return false;
   
   *serial_fd_p = serial_fd;
   return true;
}

///--------------------------------------------------------------------------------------------------------------------
/// Set serial options as wanted
///--------------------------------------------------------------------------------------------------------------------
int serial_set( int serial_fd , unsigned int baudrate )
{
   struct termios options;
   
   DEBUG (3, "   set serial device baud %d " , baudrate );

   // Get the current options for the port...
   tcgetattr(serial_fd, &options);

   // Enable the receiver and set local: Do not enable CRTSCTS -- hardware data control
   options.c_cflag = baudrate | SERIAL_DATABITS | CLOCAL | CREAD | NOFLSH ;
   
   // ignore parity errors and CR 
   options.c_iflag = IGNPAR | IGNBRK;
   
   options.c_oflag  = 0; 
   
   // ICANON  : enable canonical input
   // disable all echo functionality, and don't send signals to calling program
   options.c_lflag  = 0 ; // ICANON;   

   if ( tcsetattr(serial_fd, TCSANOW, &options) != 0 )
   {
      ERROR(" Command tcsetattr failed: %s ", strerror(errno ) );
     return -1; 
   }
   
   if ( tcflush(serial_fd, TCIFLUSH) != 0 )
   {
      ERROR(" Command tcflush failed: %s ", strerror(errno ) );
      return -1; 
   }
   return 0;
}

///--------------------------------------------------------------------------------------------------------------------
/// Send reset string to device
///--------------------------------------------------------------------------------------------------------------------
int serial_reset( int serial_fd , unsigned char* buffer  )
{
   DEBUG(2, "CALL: Sending reset to device ..");
   
   if ( serial_set( serial_fd, B115200) != 0 )
      return -1;
   
   memcpy( buffer, msg_reset, 7 );
   if ( !serial_write(serial_fd, buffer, 7) != 0)
   {
      ERROR("Serial reset failed at write!");
      return -1;
   }
   
   unsigned int red = 0;
   if ( serial_read( serial_fd, buffer, 7, &red ) < 0 )
   {
      ERROR("Serial reset failed at read!");
      return 1; 
   }

   if ( serial_set( serial_fd, B9600 ) != 0 )
      return -1;
   
   if ( !compare_responce( buffer, msg_reset_resp, 7, red ) )
   {
      return 1;
   }
   
   return 0;
}

///--------------------------------------------------------------------------------------------------------------------
/// Set device for high speed communication
///--------------------------------------------------------------------------------------------------------------------
int serial_set_highspeed( int serial_fd , unsigned char* buffer  )
{
   
   DEBUG(2, "CALL: Setting device for highspeed communication");
   
   if ( serial_set( serial_fd, B9600) != 0 )
      return -1;
   
   unsigned int red = 0;
   // OK, then write for speed up request
   memcpy( buffer, msg_speedup_write_000, 7 );
   if ( !serial_write(serial_fd, buffer, 7) != 0)
   {
      ERROR("Serial speed raising failed at write!");
      return -1;
   }
   
   
   if ( serial_read( serial_fd, buffer, 7, &red  ) != 0 )
   {
      ERROR("Serial speed raising failed at read!");
      return 1; 
   }
   
   if ( !compare_responce( buffer, msg_speedup_resp_000, 7, red ) )
   {
      return 1;
   }
   
   // Set speed high
   if ( serial_set( serial_fd, B115200) != 0 )
      return -1;
   
   usleep(100000);
   
   memcpy( buffer, msg_speedup_write_001, 7 );
   if ( !serial_write(serial_fd, buffer, 7) != 0)
   {
      ERROR("Serial speed raising failed at write!");
      return -1; 
   }
   
   if ( serial_read( serial_fd, buffer, 8, &red ) != 0 )
   {
      ERROR("Serial speed raising failed at read!");
      return 1; 
   }
   
   if ( !compare_responce( buffer, msg_speedup_resp_001, 8, red ) )
   {
      return 1;
   }
   
   DEBUG(3, "device opened highspeed ok!");
   return 0;
}


///--------------------------------------------------------------------------------------------------------------------
///--------------------------------------------------------------------------------------------------------------------
///--------------------------------------------------------------------------------------------------------------------
bool compare_responce( const unsigned char* input, const unsigned char* orig, unsigned int orig_len, unsigned int msg_len ) 
{
   unsigned int loop;
   unsigned int differ = 0;
   
   if ( orig_len > msg_len ) 
   {
      DEBUG(3, "too short message: %d vs %d" , orig_len , msg_len  );
      return false;
   }
   
   for ( loop = 0; loop < orig_len ; loop ++ )
   {
      if ( orig[loop] != 0x00 && input[loop] != orig[loop] )
      {
         if ( GLOBAL_debug_level >= 4 )
            printf("    responce differs at character %d : %x (%c) vs %x (%c) \n", loop, input[loop], input[loop], orig[loop], orig[loop] );
         differ++;
      }
   }
   
   if ( differ == 0)
   {
      DEBUG(3, "received OK responce.");
   }
   else
   {
      return false;
   }
   
   return true;
}

///--------------------------------------------------------------------------------------------------------------------
///--------------------------------------------------------------------------------------------------------------------
///--------------------------------------------------------------------------------------------------------------------
bool serial_write(int serial_fd,  const unsigned char* message, unsigned int len) 
{
   int ret = 0;
   errno = 0;
   unsigned int loop = 0;
   while ( loop < len )
   {   
      ret = write( serial_fd, (char*)(&message[loop]), len - loop);
      
      if ( ret == 0 )
      {
         DEBUG(3, "couldn't write single byte!");
         sleep(1);
      }
      
      if (ret == -1 && errno == EINTR )
      {
         DEBUG(3, "eintr caught..");
         continue;
      }
      else if ( ret == -1 )
      {
         ERROR("Error writing to serial port: %s ", strerror(errno) );
         return false; 
      }
      
      loop = loop + ret;
      continue;
   }
   
   if ( tcflush(serial_fd, TCIFLUSH) != 0 )
   {
      ERROR(" Command tcflush failed: %s", strerror(errno ) );
      return -1;
   }
   
   return true;   
}  

      
///--------------------------------------------------------------------------------------------------------------------
/// \returns 0 -- success, we red what we wanted
///          1 -- failure, we didn't get enough red before timeout was reached
///         -1 -- failure, system error, bailout
///--------------------------------------------------------------------------------------------------------------------      
int serial_read(int serial_fd, unsigned char* buffer, unsigned int max_len, unsigned int* red_bytes)
{ 
   struct timeval timeout;
   fd_set  fdset_read;
   
   int red = 0;
   int ret = 0;
   int max_fd_num = serial_fd + 1;
   int offset     = 0;
   
   FD_ZERO( &fdset_read );
   
   
   
   bool search_for_end  = false;
   
   if (max_len == 0 )
   {
      search_for_end = true;
      max_len = BUFFER_SIZE;
   }
      
   memset( buffer, 0, max_len );
   
   bool message_start_found = false;
   while( offset < max_len )
   {
      FD_SET(serial_fd,  &fdset_read );
      timeout.tv_sec  = 0;
      timeout.tv_usec = 1000000 * SERIAL_WAIT_FOR_COMM ;
      
      // printf("Doing select with offset %d \n", offset );
      
      ret = select( max_fd_num, &fdset_read, NULL, NULL, &timeout );
      
      // timeout
      if ( ret == 0 )
      {
          DEBUG(4, "read: timeout reached");
          return 1;
      }
      else if ( ret < 0 )
      {
         
         if ( errno == EINTR || errno == EAGAIN )
            continue;
         
         ERROR("Serial is failing: %s", strerror( errno ) );
          return -1; 
      }
      else
      {
         if ( !FD_ISSET( serial_fd,  &fdset_read ) )
         {
            ERROR("Internal error %s:%d" ,__FILE__, __LINE__ );
            return -1;
         }
         
         red = read( serial_fd, &buffer[offset], BUFFER_SIZE - offset );
	 // printf("Red %d bytes\n", red );
         // check also return value of =0, since we did select above
         if ( red <= 0 )
         {
            ERROR("Serial is failing: %s", strerror( errno ) );
            return -1; 
         }
         
         
         if ( GLOBAL_debug_level >= 5 )
         {
            print_message( buffer, offset + red );
         }
         
         // each message must start with 0x23 0x23
         if ( message_start_found == false )
         {
            
            int loop = 0;
            for ( loop = 0; loop < offset + red - 1; loop ++ )
            {
               if ( buffer[loop] == 0x23 && buffer[loop + 1] == 0x23 )
               {
                  message_start_found = true;
                  break;
               }
            }
            
            if ( message_start_found )
            {
               DEBUG(4, "read: msg start found at %d" ,  loop );
               // move data from loop -> 0.
               memmove( &buffer[0], &buffer[loop], red - loop );
               offset = offset + (red - loop);
            }   
            else
            {
               // no message start found, ignore data, except possibly last 0x23
               if ( buffer[ red - 1] == 0x23 )
               {
                  buffer[0] = 0x23;
                  offset    = 1;
               }
               DEBUG(4, "read: no msg start found " );
               continue;
            }
         }
         else
         {
            offset = offset + red;
         }  
         
         // Now we have data available 0 .. offset and message start has been found

         // search for possible terminating \r\n 
         if ( search_for_end == true )
         {
            int loop = 0;
            for ( loop = 0; loop < offset + red - 1; loop ++ )
            {
               if ( buffer[ loop ] == '\r' && buffer[loop+1] == '\n' )
               {
                  DEBUG(4, "read: msg end found at %d ", loop );
                  buffer[ loop + 2 ] = 0x0;
                  *red_bytes = loop + 2;
                  return 0;
               }
            }
            DEBUG(4, "read: no msg end found " );
         }

      }
   }
   
   if ( search_for_end == true )
   {
      DEBUG(4, "read: no msg end found and buffer is FULL" );
      return 1;
   }
   
   *red_bytes = offset;
   
   return 0;
}
      
      
      

void print_message( unsigned char* buffer, int len )
{
   if ( GLOBAL_debug_level >= 4 )
   {
      int tmp_loop = 0;
      printf("     msg:");
      for ( tmp_loop = 0; tmp_loop < len; tmp_loop ++ )
         printf(" %x ", buffer[tmp_loop] );
      printf("\n");
   }
   /*
   printf("     msg: ");
   for ( tmp_loop = 0; tmp_loop < len; tmp_loop ++ )
   {
      if ( buffer[tmp_loop] == '\n' )
         printf("(\\n)");
      else if ( buffer[tmp_loop] == '\r' )
         printf("(\\r)");
      else 
         printf("(%c) ", buffer[tmp_loop] );
   }
   printf("\n");
   */
}
