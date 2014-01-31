#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>

extern int GLOBAL_debug_level;

void print_debug ( int level, const char* module, const char* file, int linenum, const char* format, ... );
void print_error ( const char* module, const char* format, ... );

#define ERROR(  ... ) print_error(MODULE_NAME, ## __VA_ARGS__ )
#define DEBUG(lvl,  ... ) print_debug(lvl, MODULE_NAME, __FILE__, __LINE__, ## __VA_ARGS__ )

#define BUFFER_SIZE 1024
/// How many seconds to wait
#define SERIAL_WAIT_FOR_COMM 0.1 

typedef struct
{
   float longitude;
   float latitude;
   float height;
   
   int32_t time[6]; // 0 = seconds, 1=minutes,2=hours,3=days,4=months,5=years
} GPS_point;
   
typedef struct
{
   unsigned int npoints;
   GPS_point* points;
} GPS_points;

/// ---------- IMPLEMENTED IN serial.cc ---------------
int serial_reset( int serial_fd , unsigned char* buffer  );
bool serial_init_highspeed( const char* device, unsigned char* buffer, int* serial_fd_p );

int serial_query_sampling( int serial_fd, unsigned char* buffer, unsigned int* sample_rate );
int serial_set_sampling ( int serial_fd, unsigned char* buffer, int sampling );
int serial_download ( int serial_fd, unsigned char* buffer, GPS_points* points );
int serial_clear_datapoints( int serial_fd, unsigned char* buffer );

/// ---------- IMPLEMENTED IN datafile.cc ---------------
bool GPS_points_init( GPS_points* points );
bool GPS_points_write( GPS_points* points, const char* filename );
bool GPS_points_free( GPS_points* points );

#endif