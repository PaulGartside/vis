////////////////////////////////////////////////////////////////////////////////
// VI-Simplified (vis) C++ Implementation                                     //
// Copyright (c) 07 Sep 2015 Paul J. Gartside                                 //
////////////////////////////////////////////////////////////////////////////////
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without  limitation //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
////////////////////////////////////////////////////////////////////////////////

#include <ctype.h>     // is(alnum|punct|space|print|lower...)
#include <unistd.h>    // write, ioctl[unix], read
#include <string.h>    // memcpy, memset
#include <sys/stat.h>  // lstat
#include <sys/types.h>
#include <sys/time.h>  // gettimeofday
#ifndef WIN32
#include <sys/wait.h>
#endif
#include <stdlib.h>    // exit
#include <dirent.h>
#include <errno.h>

#include "Types.hh"
#include "MemLog.hh"
#include "Console.hh"
#include "FileBuf.hh"
#include "String.hh"
#include "Utilities.hh"

extern const char* PROG_NAME;
extern MemLog<MEM_LOG_BUF_SIZE> Log;
extern ConstCharList* gl_pCall_Stack;

void DIE( const char* msg )
{
  printf("%s: %s\n", PROG_NAME, msg );
  Log.Dump();
  exit( 0 );
}

void DBG( const int line, const char* msg, ... )
{
  if( !msg )
  {
    printf( "%s(%i)\n", __FILE__, line );
  }
  else {
    char out[1024];

    va_list list;
    va_start( list, msg );
    vsprintf( out, msg, list);
    va_end( list );

    printf( "%s(%i): %s\n", __FILE__, line, out );
  }
  fflush( stdout );
}

void ASSERT( const int line, bool condition, const char* msg, ... )
{
  if( !condition )
  {
    Trace::Print();

    char msg_buf[ 1024 ];
    va_list argp;
    va_start( argp, msg );
    vsprintf( msg_buf, msg, argp );
    va_end( argp );

    DBG( line, "ASSERT: %s", msg_buf );
    DIE( "Exiting because of ASSERT" );
  }
}

unsigned Min( const unsigned a, const unsigned b )
{
  return a < b ? a : b;
}

unsigned Max( const unsigned a, const unsigned b )
{
  return a > b ? a : b;
}

unsigned LLM1( const unsigned LL )
{
  return 0 < LL ? LL-1 : 0;
}

void Swap( unsigned& A, unsigned& B )
{
  unsigned T = B;
  B = A;
  A = T;
}

void Safe_Strcpy( char* dst, const char* src, const size_t dst_size )
{
  strncpy( dst, src, dst_size-1 );

  dst[ dst_size-1 ] = 0;
}

void RemoveSpaces( char* cp )
{
  if( cp )
  {
    unsigned len = strlen( cp );

    ASSERT( __LINE__, len < 1024, "len < 1024" );

    for( unsigned k=0; k<len; k++ )
    {
      if( IsSpace( cp[k] ) )
      {
        for( unsigned i=k; i<len; i++ )
        {
          cp[i] = cp[i+1];
        }
        len--;
        k--; // Since we just shifted down over current char,
      }      // recheck current char
    }
  }
}

void Shift( char* cp, const unsigned SHIFT_LEN )
{
  if( cp )
  {
    const unsigned cp_len = strlen( cp );

    ASSERT( __LINE__, cp_len < 1024, "len < 1024" );

    if( cp_len < SHIFT_LEN )
    {
      cp[0] = 0;
    }
    else {
      for( unsigned k=SHIFT_LEN; k<=cp_len; k++ )
      {
        cp[k-SHIFT_LEN] = cp[k];
      }
    }
  }
}

// Return 0 on success, -1 on failure
int my_stat( const char* fname, struct stat& sbuf )
{
#if defined( WIN32 )
  return ::stat( fname, &sbuf );
#else
  return ::lstat( fname, &sbuf );
#endif
}

bool FileExists( const char* fname )
{
  struct stat sbuf;

  int err = my_stat( fname, sbuf );

  const bool exists = 0 == err;

  return exists;
}

size_t FileSize( const char* fname )
{
  size_t file_size = 0;

  struct stat sbuf;

  int err = my_stat( fname, sbuf );

  const bool exists = 0 == err;

  if( exists )
  {
    file_size = sbuf.st_size;
  }
  return file_size;
}

bool IsReg( const char* fname )
{
  struct stat sbuf;

  int err = my_stat( fname, sbuf );

  const bool exists = 0 == err;

  const bool is_reg = exists && S_ISREG( sbuf.st_mode );

  return is_reg;
}

bool IsDir( const char* fname )
{
  // If lstat fails, is_dir is false
  struct stat sbuf ;

  int err = my_stat( fname, sbuf );

  const bool exists = 0 == err;

  const bool is_dir = exists && S_ISDIR( sbuf.st_mode );

  return is_dir;
}

double ModificationTime( const char* fname )
{
  double mod_time = 0;

  struct stat sbuf;
  int err = my_stat( fname, sbuf );
  if( 0 == err )
  {
#if defined( WIN32 )
    mod_time = sbuf.st_mtime;
#elif defined( OSX )
    mod_time = sbuf.st_mtimespec.tv_sec
             + static_cast<double>(sbuf.st_mtimespec.tv_nsec)/1e9;
#else
    mod_time = sbuf.st_mtim.tv_sec
             + static_cast<double>(sbuf.st_mtim.tv_nsec)/1e9;
#endif
  }
  return mod_time;
}

// Changes 'path/.' to 'path/'
void Remove_dot_at_end( char* path )
{
  if( 0 != path )
  {
    int PATH_LEN = strlen( path );
    if( 2 < PATH_LEN )
    {
      if( DIR_DELIM == path[PATH_LEN-2]
       && '.'       == path[PATH_LEN-1] )
      {
        path[PATH_LEN-1] = 0;
      }
    }
  }
}

// Removes './' from path
void Remove_dot_slash( char* path )
{
  if( 0 != path )
  {
    int PATH_LEN = strlen( path );
    if( 1 < PATH_LEN )
    {
      for( int k=1; k<PATH_LEN; k++ )
      {
        if( '.' == path[k-1]
         && '/' == path[k  ] )
        {
          //< Shift from '/' to end back to '.'
        //Shift( path+k-1, 1 );
          for( unsigned i=k; i<=PATH_LEN; i++ )
          {
            path[i-1] = path[i];
          }
          PATH_LEN -= 1;
        }
      }
    }
  }
}

// Changes '//' to '/'
void Remove_slash_slash( char* path )
{
  if( 0 != path )
  {
    int PATH_LEN = strlen( path );
    if( 1 < PATH_LEN )
    {
      for( int k=1; k<PATH_LEN; k++ )
      {
        if( '/' == path[k-1]
         && '/' == path[k  ] )
        {
          //< Shift from second '/' to end back to first '/'
        //Shift( path+k-1, 1 );
          for( unsigned i=k; i<=PATH_LEN; i++ )
          {
            path[i-1] = path[i];
          }
          PATH_LEN -= 1;
        }
      }
    }
  }
}

// Remove all 'parent/..' occurences
void Remove_parent_slash_dot_dot( char* path )
{
  if( 0 != path )
  {
    int PATH_LEN = strlen( path );
    if( 2 < PATH_LEN )
    {
      char slash_dot_dot[8];
      sprintf( slash_dot_dot, "%c..", DIR_DELIM );

      // Remove all 'parent/..' occurences
      while( char* slash2_ptr = strstr( path, slash_dot_dot ) )
      {
        if( 3 == PATH_LEN )
        {
          path[0] = DIR_DELIM;
          path[1] = 0;
        }
        else {
          // Search for beginning of parent:
          int slash1_idx = slash2_ptr - path - 1;
          for( ; -1 < slash1_idx; slash1_idx-- )
          {
            if( path[slash1_idx] == DIR_DELIM ) break;
          }
          int end_idx = slash2_ptr + 2 - path;
          if( end_idx < PATH_LEN-1 && '/' == path[end_idx+1] ) end_idx++;
          const int RM_LEN = end_idx - slash1_idx;
          for( int k=slash1_idx+1; k<=(PATH_LEN-RM_LEN); k++ )
          {
            path[k] = path[k+RM_LEN];
          }
          PATH_LEN -= RM_LEN;
        }
      }
    }
  }
}

void Normalize_Full_Path( char* path )
{
  if( 0 != path )
  {
    Remove_dot_at_end( path );
    Remove_slash_slash( path );
    Remove_parent_slash_dot_dot( path );
    Remove_dot_slash( path );
    Remove_slash_slash( path );
  }
}

// Finds full name of file or directory of in_out_fname passed in
// relative to the current directory, and places result in in_out_fname.
// The full name found does not need to exist to return success.
// Returns true on success, false on failure.
//
bool FindFullFileNameRel2CWD( String& in_out_fname )
{
  EnvKeys2Vals( in_out_fname );

  const char* in_fname = in_out_fname.c_str();

  if( 0<in_out_fname.len() && DIR_DELIM == in_fname[0] )
  {
    // in_out_fname is a already full path, so just return
    return true;
  }
  const unsigned BUF_LEN = 1024;
  char buf[ BUF_LEN ];
  if( getcwd( buf, BUF_LEN ) )
  {
    const int CWD_LEN = strlen( buf );
    if( CWD_LEN < BUF_LEN )
    {
      const int REMAINDER = BUF_LEN - CWD_LEN;
      int rval = snprintf( buf+CWD_LEN
                         , REMAINDER
                         , "%c%s", DIR_DELIM, in_fname );

      if( 0 < rval && rval < REMAINDER )
      {
        Normalize_Full_Path( buf );
        in_out_fname = buf;
        return true;
      }
    }
  }
  return false;
}

// Finds full name of file or directory of in_out_fname passed in
// relative to rel_2_path, and places result in in_out_fname.
// The full name found does not need to exist to return success.
// Returns true on success, false on failure.
//
bool FindFullFileNameRel2( const char* rel_2_path, String& in_out_fname )
{
  if( 0==rel_2_path || 0==strcmp(rel_2_path,"") )
  {
    return FindFullFileNameRel2CWD( in_out_fname );
  }
  else {
    EnvKeys2Vals( in_out_fname );

    const char* in_fname = in_out_fname.c_str();

    if( 0<in_out_fname.len() && DIR_DELIM == in_fname[0] )
    {
      // in_out_fname is already a full path, so just return
      return true;
    }
    const unsigned BUF_LEN = 1024;
    char buf[ BUF_LEN ];
    Safe_Strcpy( buf, rel_2_path, BUF_LEN );
    const int REL_PATH_LEN = strlen( buf );
    if( REL_PATH_LEN < BUF_LEN )
    {
      const int REMAINDER = BUF_LEN - REL_PATH_LEN;
      int rval = snprintf( buf+REL_PATH_LEN
                         , REMAINDER
                         , "%c%s", DIR_DELIM, in_fname );

      if( 0 < rval && rval < REMAINDER )
      {
        Normalize_Full_Path( buf );
        in_out_fname = buf;
        return true;
      }
    }
  }
  return false;
}

void GetFnameHeadAndTail( const char* in_fname, String& head, String& tail )
{
  head.clear();
  tail.clear();

  // This const_cast is okay because we are not changing in_fname_nc:
  char* in_fname_nc = CCast<char*>(in_fname);
  char* const last_slash = strrchr( in_fname_nc, DIR_DELIM );
  if( last_slash )
  {
    for( const char* cp = last_slash + 1; *cp; cp++ ) head.push( *cp );
    for( const char* cp = in_fname; cp < last_slash; cp++ ) tail.push( *cp );
  }
  else {
    // No tail, all head:
    for( const char* cp = in_fname; *cp; cp++ ) head.push( *cp );
  }
}

void GetFnameHeadAndTail( const String& in_fname, String& head, String& tail )
{
  GetFnameHeadAndTail( in_fname.c_str(), head, tail );
}

String GetFnameHead( const char* in_full_fname )
{
  String head;

  // This const_cast is okay because we are not changing in_fname_cp:
  char* in_fname_cp = CCast<char*>(in_full_fname);
  char* const last_slash = strrchr( in_fname_cp, DIR_DELIM );
  if( last_slash )
  {
    for( const char* cp = last_slash + 1; *cp; cp++ ) head.push( *cp );
  }
  else {
    // No tail, all head:
    for( const char* cp = in_full_fname; *cp; cp++ ) head.push( *cp );
  }
  return head;
}

String GetFnameTail( const char* in_full_fname )
{
  String tail;

  // This const_cast is okay because we are not changing in_fname_cp:
  char* in_fname_cp = CCast<char*>(in_full_fname);
  char* const last_slash = strrchr( in_fname_cp, DIR_DELIM );
  if( last_slash )
  {
    for( const char* cp = in_full_fname; cp<last_slash; cp++ ) tail.push( *cp );
  }
  return tail;
}

void Append_Dir_Delim( String& in_dir )
{
  if( (0 < in_dir.len())
   && (DIR_DELIM != in_dir.get_end()) )
  {
    in_dir.push( DIR_DELIM );
  }
}

const char* DirDelimStr()
{
  static char DIR_DELIM_STR[4];
  static bool initialized = false;

  if( !initialized )
  {
    initialized = true;
    sprintf( DIR_DELIM_STR, "%c", DIR_DELIM );
  }
  return DIR_DELIM_STR;
}

void EnvKeys2Vals( String& in_out_fname )
{
  String& s = in_out_fname;

  // Replace ~/ with $HOME/
  if( 1<s.len() && '~' == s.get(0) && '/' == s.get(1) )
  {
    s.replace( "~", "$HOME" );
  }
  for( unsigned k=0; k<s.len()-1; k++ )
  {
    if( '$' == s.get(k) )
    {
      String env_key_s = "";
      for( k++; k<s.len() && IsWord_Ident( s.get(k) ); k++ )
      {
        env_key_s.push( s.get(k) );
      }
      if( env_key_s.len() )
      {
        const char* env_val_c = getenv( env_key_s.c_str() );
        if( env_val_c )
        {
          String env_val_s( env_val_c );
          env_key_s.insert( 0, '$' );
          while( s.replace( env_key_s, env_val_s ) ) ;
        }
      }
    }
  }
}

#ifndef WIN32
// On success, returns non-zero fd and fills in child_pid.
// On failure, returns zero.
FILE* POpenRead( const char* cmd, pid_t& child_pid )
{
  FILE* fp = 0;
  int pfd[2];

  if( pipe( pfd ) < 0 ) ; // pipe() error, drop out
  else {
    const pid_t pid = fork();
    if( pid < 0 ) ; // fork() error, drop out
    else if( 0 < pid ) // Parent
    {
      close( pfd[1] ); // Close parent write end of pipe
      fp = fdopen( pfd[0], "r" );
      if( fp ) child_pid = pid;
    }
    else //( 0 == pid ) // Child
    {
      close( pfd[0] ); // Close child read end of pipe
      // Put stdout and stderr onto child write end of pipe:
      if( STDOUT_FILENO != pfd[1] ) dup2( pfd[1], STDOUT_FILENO );
      if( STDERR_FILENO != pfd[1] ) dup2( pfd[1], STDERR_FILENO );
      // Close child write file descriptor for pipe because it will not
      // be used.  The execl() command below will send its output to
      // stdout and stderr, which are still tied to write end of pipe.
      close( pfd[1] );

      ExecShell( cmd );
      // If we get here, execl() failed, so make child return with 127
      _exit( 127 );
    }
  }
  return fp;
}

// Returns child termination status on success, -1 on error
int PClose( FILE* fp, const pid_t child_pid )
{
  // Default to error value:
  int child_termination_sts = -1;

  // my_popen_r was never called:
  if( child_pid <= 0 ) ; // Invalid child_pid, drop out
  else {
    if( fclose( fp ) == EOF ) ; // fp was not open, drop out
    else {
      bool child_done = false;
      while( !child_done )
      {
        int err = waitpid( child_pid, &child_termination_sts, 0 );
        if( 0 != err && EINTR == errno )
        {
          ; // waitpid() interrupted, keep waiting
        }
        else {
          // waitpid successful, or had error, either way, drop out
          child_done = true;
        }
      }
    }
  }
  return child_termination_sts;
}

void ExecShell( const char* cmd )
{
  const char*           shell_prog = getenv("VIS_SHELL");
  if( 0 == shell_prog ) shell_prog = getenv("SHELL");
  if( 0 == shell_prog ) shell_prog = "/bin/bash";

  execl( shell_prog, shell_prog, "-c", cmd, (char*) 0 );
}

#endif

double GetTimeSeconds()
{
#if defined( WIN32 )
  return time();
#else
  timeval tv;

  gettimeofday( &tv, 0 );

  return tv.tv_sec + double(tv.tv_usec)/1e6;
#endif
}

bool IsWord_Ident( const int C )
{
  return isalnum( C ) || C == '_';
}

bool IsWord_NonIdent( const int C )
{
  return !IsSpace( C ) && !IsWord_Ident( C );
}

bool IsSpace( const int C )
{
  return C == ' ' || C == '\t' || C == '\n' || C == '\r';
}

bool NotSpace( const int C )
{
  return !IsSpace( C );
}

bool IsIdent( const int C )
{
  return isalnum( C ) || C == '_';
}

bool IsXML_Ident( const char C )
{
  return isalnum( C )
      || C == '_'
      || C == '-'
      || C == '.'
      || C == ':';
}

bool IsFileNameChar( const int C )
{
  return      '$' == C
      ||      '+' == C
      ||      '-' == C
      ||      '.' == C
      || DIR_DELIM== C
      ||    ( '0' <= C && C <= '9' )
      ||    ( 'A' <= C && C <= 'Z' )
      ||      '_' == C
      ||    ( 'a' <= C && C <= 'z' )
      ||      '~' == C
#ifdef WIN32
      ||      ' ' == C
#endif
      ;
}

bool IsEndOfLineDelim( const int C )
{
  if( C == '\n' ) return true;
#ifdef WIN32
  if( C == '\r' ) return true;
#endif
  return false;
}

char MS_Hex_Digit( const char Ci )
{
  const uint8_t I = (Ci >> 4) & 0xF;

  char Co = '*';

  switch( I )
  {
  case  0: Co = '0'; break;
  case  1: Co = '1'; break;
  case  2: Co = '2'; break;
  case  3: Co = '3'; break;
  case  4: Co = '4'; break;
  case  5: Co = '5'; break;
  case  6: Co = '6'; break;
  case  7: Co = '7'; break;
  case  8: Co = '8'; break;
  case  9: Co = '9'; break;
  case 10: Co = 'A'; break;
  case 11: Co = 'B'; break;
  case 12: Co = 'C'; break;
  case 13: Co = 'D'; break;
  case 14: Co = 'E'; break;
  case 15: Co = 'F'; break;
  }
  return Co;
}

char LS_Hex_Digit( const char Ci )
{
  const uint8_t I = Ci & 0xF;

  char Co = '*';

  switch( I )
  {
  case  0: Co = '0'; break;
  case  1: Co = '1'; break;
  case  2: Co = '2'; break;
  case  3: Co = '3'; break;
  case  4: Co = '4'; break;
  case  5: Co = '5'; break;
  case  6: Co = '6'; break;
  case  7: Co = '7'; break;
  case  8: Co = '8'; break;
  case  9: Co = '9'; break;
  case 10: Co = 'A'; break;
  case 11: Co = 'B'; break;
  case 12: Co = 'C'; break;
  case 13: Co = 'D'; break;
  case 14: Co = 'E'; break;
  case 15: Co = 'F'; break;
  }
  return Co;
}

bool IsHexDigit( const char C )
{
  return isdigit( C )
      || C == 'A' || C == 'a'
      || C == 'B' || C == 'b'
      || C == 'C' || C == 'C'
      || C == 'D' || C == 'd'
      || C == 'E' || C == 'e'
      || C == 'F' || C == 'f';
}

int Hex_Char_2_Int_Val( const char C )
{
  int I = 0;

  switch( C )
  {
  case '0': I = 0; break;
  case '1': I = 1; break;
  case '2': I = 2; break;
  case '3': I = 3; break;
  case '4': I = 4; break;
  case '5': I = 5; break;
  case '6': I = 6; break;
  case '7': I = 7; break;
  case '8': I = 8; break;
  case '9': I = 9; break;
  case 'A':
  case 'a': I = 10; break;
  case 'B':
  case 'b': I = 11; break;
  case 'C':
  case 'c': I = 12; break;
  case 'D':
  case 'd': I = 13; break;
  case 'E':
  case 'e': I = 14; break;
  case 'F':
  case 'f': I = 15; break;
  }
  return I;
}

char Hex_Chars_2_Byte( const char C1, const char C2 )
{
  return (char)( Hex_Char_2_Int_Val( C1 )*16
               + Hex_Char_2_Int_Val( C2 ) );
}

bool line_start_or_prev_C_non_ident( const Line& line
                                   , const unsigned p )
{
  if( 0==p ) return true; // p is on line start

  // At this point 0 < p
  const uint8_t C = line.get( p-1 );
  if( !isalnum( C ) && C!='_' )
  {
    // C is not an identifier
    return true;
  }
  // On identifier
  return false;
}

bool line_end_or_non_ident( const Line& line
                          , const unsigned LL
                          , const unsigned p )
{
  if( p == LL-1 ) return true; // p is on line end

  if( p < LL-1 )
  {
    // At this point p should always be less than LL-1,
    // but put the check in above just to be safe.
    // The check above could also be implemented as an ASSERT.
    const uint8_t C = line.get(p+1);
    if( !isalnum( C ) && C!='_' )
    {
      // C is not an identifier
      return true;
    }
  }
  // C is an identifier
  return false;
}

// Given full path names of files, return true if the two
// files are the same
bool Files_Are_Same( const char* fname_s, const char* fname_l )
{
  bool files_are_same = false;

  if( IsReg( fname_s )
   && IsReg( fname_l ) )
  {
    const size_t len_s = FileSize( fname_s );
    const size_t len_l = FileSize( fname_l );

    if( len_s == len_l )
    {
      FILE* fp_s = fopen( fname_s, "rb" );
      FILE* fp_l = fopen( fname_l, "rb" );

      if( fp_s && fp_l )
      {
        for( bool done = false; !done; )
        {
          const int C_s = fgetc( fp_s );
          const int C_l = fgetc( fp_l );

          if( EOF == C_s && EOF == C_l )
          {
            done = true;
            files_are_same = true;
          }
          else if( C_s != C_l )
          {
            done = true; // Files are different
          }
          else {
            // Keep reading till end of files or difference
          }
        }
      }
      fclose( fp_s );
      fclose( fp_l );
    }
  }
  return files_are_same;
}

// Given FileBuf of files, return true if the two files are the same
bool Files_Are_Same( const FileBuf& fb_s, const FileBuf& fb_l )
{
  bool files_are_same = false;

  if( fb_s.IsRegular() && fb_l.IsRegular() )
  {
    const unsigned num_lines_s = fb_s.NumLines();
    const unsigned num_lines_l = fb_l.NumLines();

    if( num_lines_s == num_lines_l )
    {
      files_are_same = true;

      for( unsigned k=0; files_are_same && k<num_lines_s; k++ )
      {
        const Line& l_s = fb_s.GetLine( k );
        const Line& l_l = fb_l.GetLine( k );

        if( !l_s.eq( l_l ) )
        {
          files_are_same = false;
        }
      }
    }
  }
  return files_are_same;
}

bool Line_Has_Regex( const Line& line, const String& regex )
{
  // Find the patterns for the line:
  const unsigned LL = line.len();

        bool     slash    = true;
        unsigned star_len = regex.len();
  const char*    star_str = regex.c_str();
  if( 4<regex.len()
   && regex.has_at("\\b", 0)
   && regex.ends_with("\\b") )
  {
    star_str += 2;
    star_len -= 4;
    slash     = false;
  }
  if( star_len<=LL )
  {
    for( unsigned p=0; p<LL; p++ )
    {
      bool matches = slash || line_start_or_prev_C_non_ident( line, p );
      for( unsigned k=0; matches && (p+k)<LL && k<star_len; k++ )
      {
        if( star_str[k] != line.get(p+k) ) matches = false;
        else {
          if( k+1 == star_len ) // Found pattern
          {
            matches = slash || line_end_or_non_ident( line, LL, p+k );

            if( matches ) return true;
          }
        }
      }
    }
  }
  return false;
}

ConstCharList* Trace::mp_Call_Stack = 0;

void Trace::Allocate()
{
  mp_Call_Stack = new(__FILE__,__LINE__) ConstCharList;
}

void Trace::Cleanup()
{
  MemMark(__FILE__,__LINE__); delete mp_Call_Stack; mp_Call_Stack = 0;
}

Trace::Trace( const char* func_name )
{
  mp_Call_Stack->push( func_name );
}

Trace::~Trace()
{
  mp_Call_Stack->pop();
}

void Trace::Print()
{
  if( mp_Call_Stack->len() )
  {
    Console::Set_Normal();
    Console::Move_2_Row_Col( Console::Num_Rows()-1, 0 );

    // Put curson on a new line:
    Console::NewLine();
    Console::Flush();

    printf("\n");
    for( unsigned k=0; k<mp_Call_Stack->len(); k++ )
    {
      printf( "Call Stack: %s\n", (*mp_Call_Stack)[k] );
    }
    mp_Call_Stack->clear();
  }
}

