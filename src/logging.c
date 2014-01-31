#include "common.h"

#include <stdio.h>
#include <stdarg.h>

///----------------------------------------------------------------------------
///----------------------------------------------------------------------------
void print_raw( FILE* out, const char* prefix, const char* module, const char* file, long line , const char* format, va_list param_list )
{
  if ( file == NULL )
  {
      fprintf(  out , "%s (%s): "   , prefix, module );
  }
  else
  {
     fprintf(  out , "%s (%s:%ld): ", prefix, file, line);
  }
  
  vfprintf( out , format  , param_list );
  fprintf( out , "\n" );
}

///----------------------------------------------------------------------------
///----------------------------------------------------------------------------
void print_error( const char* module, const char* format, ... )
{
   va_list param_list;

   va_start( param_list, format );
   print_raw( stdout, "Error", module, NULL, 0, format, param_list );
   va_end( param_list );
}

///----------------------------------------------------------------------------
///----------------------------------------------------------------------------
void print_debug( int level, const char* module, const char* file, int linenum, const char* format, ... )
{
   
   if ( level > GLOBAL_debug_level )
      return ;
      
   va_list param_list;

   va_start( param_list, format );
   print_raw( stdout, "Debug", module, file, linenum, format, param_list );
   va_end( param_list );
}