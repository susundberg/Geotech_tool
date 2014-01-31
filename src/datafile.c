

#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define MODULE_NAME "datafile"

///--------------------------------------------------------------------------------------------------------------------
///--------------------------------------------------------------------------------------------------------------------
bool GPS_points_init( GPS_points* points )
{
   points->npoints = 0;
   points->points  = NULL;
   return true;
}

///--------------------------------------------------------------------------------------------------------------------
///--------------------------------------------------------------------------------------------------------------------
bool GPS_points_free( GPS_points* points )
{
   free( points->points  );
   points->npoints = 0;
   return true;
}

///--------------------------------------------------------------------------------------------------------------------
///--------------------------------------------------------------------------------------------------------------------
bool GPS_points_write( GPS_points* data, const char* filename )
{
   GPS_point* points = data->points;
   
   FILE* fid = fopen( filename, "wb" );
   if (fid == NULL )
   {
      printf("Error! Cannot open file '%s' for writing: %s \n", filename, strerror(errno ));
      return false;
   }
   int ploop=0;
   
   fprintf(fid, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n" );
   fprintf(fid, "<gpx xmlns=\"http://www.topografix.com/GPX/1/1\" creator=\"MapSource 6.15.7\" version=\"1.1\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd\">\n");
   fprintf(fid, "<metadata>\n");
   fprintf(fid, "<text>Geotech GPS receiver, data downloaded with geotech_tool </text> \n");
   fprintf(fid, "</metadata>\n");

   fprintf(fid, " <trk>\n");
   fprintf(fid, "  <name>Route1</name>\n");
   fprintf(fid, "  <trkseg>\n");
   for ( ploop = 0; ploop < data->npoints; ploop ++ )
   {
      fprintf(fid, "  <trkpt lat=\"%.06f\" lon=\"%.06f\"> \n", points[ploop].latitude, points[ploop].longitude );
      fprintf(fid, "     <ele> %.06f</ele> \n",  points[ploop].height );
      fprintf(fid, "     <time>%04d-%02d-%02dT%02d:%02d:%02dZ</time> \n",  points[ploop].time[5], points[ploop].time[4], points[ploop].time[3], 
                                                                      points[ploop].time[2], points[ploop].time[1], points[ploop].time[0] );
     //    <ele>39.000000</ele>
     //    <time>2012-04-01T13:38:47Z</time>
      fprintf(fid, "  </trkpt>\n");
      /// fprintf(fid, "p%04d, %.03f, %0.6f, %0.6f \n", ploop, points->height[ploop], points->latitude[ploop], points->longitude[ploop] );
   }
   fprintf(fid, "  </trkseg>\n");
   fprintf(fid, " </trk>\n");
   fprintf(fid, "</gpx>\n");
   
   fclose( fid );
   return true;

}
