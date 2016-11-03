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

bool IsDir( const char* fname )
{
  // If lstat fails, is_dir is false
  struct stat sbuf ;

  int err = my_stat( fname, sbuf );

  const bool is_dir = 0==err && S_ISDIR( sbuf.st_mode );

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

//void Normalize_Full_Path( char* path )
//{
//  if( 0 != path )
//  {
//    int PATH_LEN = strlen( path );
//    if( 1 < PATH_LEN )
//    {
//      if( '/' == path[PATH_LEN-2]
//       && '.' == path[PATH_LEN-1] )
//      {
//        path[PATH_LEN-1] = 0;
//        PATH_LEN--;
//      }
//      char slash_dot_dot[8];
//      sprintf( slash_dot_dot, "%c..", DIR_DELIM );
//
//      // Remove all 'parent/..' occurences
//      while( char* slash2_ptr = strstr( path, slash_dot_dot ) )
//      {
//        if( 3 == PATH_LEN )
//        {
//          path[0] = DIR_DELIM;
//          path[1] = 0;
//        }
//        else {
//          // Search for beginning of parent:
//          int slash1_idx = slash2_ptr - path - 1;
//          for( ; -1 < slash1_idx; slash1_idx-- )
//          {
//            if( path[slash1_idx] == DIR_DELIM ) break;
//          }
//          int end_idx = slash2_ptr + 2 - path;
//          if( end_idx < PATH_LEN-1 && '/' == path[end_idx+1] ) end_idx++;
//          const int RM_LEN = end_idx - slash1_idx;
//          for( int k=slash1_idx+1; k<=(PATH_LEN-RM_LEN); k++ )
//          {
//            path[k] = path[k+RM_LEN];
//          }
//          PATH_LEN -= RM_LEN;
//        }
//      }
//    }
//  }
//}

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

//void Normalize_Full_Path( char* path )
//{
//  if( 0 != path )
//  {
//    Remove_dot_at_end( path );
//    Remove_parent_slash_dot_dot( path );
//    Remove_dot_slash( path );
//    Remove_slash_slash( path );
//
//  }
//}

void Normalize_Full_Path( char* path )
{
  if( 0 != path )
  {
    Remove_dot_at_end( path );
    Remove_slash_slash( path );
    Remove_parent_slash_dot_dot( path );
    Remove_dot_slash( path );

  }
}

// Finds full name of file or directory of in_out_fname passed in
// relative to current directory, and places result in in_out_fname.
// If successful, as a side effect, currect directory is changed to
// that of new in_out_fname.
// Returns true on success, false on failure.
//
//bool FindFullFileName( String& in_out_fname )
//{
//  EnvKeys2Vals( in_out_fname );
//  const char* in_fname = in_out_fname.c_str();
//  const unsigned FILE_NAME_LEN = 1024;
//  char cwd[ FILE_NAME_LEN ];
//
//  // 1. lstat( in_fname ), set is_dir.
//  // If lstat fails, is_dir is false, and assume in_fname will be a new file.
//  struct stat sbuf ;
//  int err = my_stat( in_fname, sbuf );
//  const bool is_dir = ( 0==err && S_ISDIR( sbuf.st_mode ) ) ? true : false;
//
//  if( is_dir ) // in_fname is name of dir:
//  {
//    // 1. chdir  - Change dir to in_fname
//    int err = chdir( in_fname );
//    if( err ) return false;
//    // 2. getcwd - Get the current working directory and put into cwd
//    if( ! getcwd( cwd, FILE_NAME_LEN ) ) return false;
//    // 3. make sure cwd ends with a '/'
//    const int F_NAME_LEN = strlen( cwd );
//    if( DIR_DELIM != cwd[ F_NAME_LEN-1 ] )
//    {
//      cwd[ F_NAME_LEN ] = DIR_DELIM;
//      cwd[ F_NAME_LEN+1 ] = 0;
//    }
//    // 4. copy cwd int in_out_fname
//    in_out_fname = cwd;
//  }
//  else {
//    // 1. seperate in_fname into f_name_tail and f_name_head
//    String f_name_head;
//    String f_name_tail;
//
//    GetFnameHeadAndTail( in_out_fname, f_name_head, f_name_tail );
//
//    // 2. chdir  - Change dir to f_name_tail
//    if( f_name_tail.len() )
//    {
//      int err = chdir( f_name_tail.c_str() );
//      if( err ) return false;
//    }
//    // 3. getcwd - Get the current working directory and put into f_name_tail
//    if( ! getcwd( cwd, FILE_NAME_LEN ) ) return false;
//    f_name_tail = cwd;
//    // 4. concatenate f_name_tail and f_name_head into in_out_fname
//    sprintf( cwd, "%s%c%s", f_name_tail.c_str(), DIR_DELIM, f_name_head.c_str() );
//    in_out_fname = cwd;
//  }
//  return true;
//}

// Finds full name of file or directory of in_out_fname passed in
// relative to current directory, and places result in in_out_fname.
// If successful, as a side effect, currect directory is changed to
// that of new in_out_fname.
// Returns true on success, false on failure.
//
//bool FindFullFileName( String& in_out_fname )
//{
//  EnvKeys2Vals( in_out_fname );
//  const char* in_fname = in_out_fname.c_str();
//  const unsigned FILE_NAME_LEN = 1024;
//  char cwd[ FILE_NAME_LEN ];
//
//  if( IsDir( in_fname ) ) // in_fname is name of dir:
//  {
//    // 1. chdir  - Change dir to in_fname
//    int err = chdir( in_fname );
//    if( err ) return false;
//    // 2. getcwd - Get the current working directory and put into cwd
//    if( ! getcwd( cwd, FILE_NAME_LEN ) ) return false;
//    // 3. make sure cwd ends with a '/'
//    const int F_NAME_LEN = strlen( cwd );
//    if( DIR_DELIM != cwd[ F_NAME_LEN-1 ] )
//    {
//      cwd[ F_NAME_LEN ] = DIR_DELIM;
//      cwd[ F_NAME_LEN+1 ] = 0;
//    }
//    // 4. copy cwd int in_out_fname
//    in_out_fname = cwd;
//  }
//  else { // IsDir is false, assume in_fname will be a new file.
//    // 1. seperate in_fname into f_name_tail and f_name_head
//    String f_name_head;
//    String f_name_tail;
//
//    GetFnameHeadAndTail( in_out_fname, f_name_head, f_name_tail );
//
//    // 2. chdir  - Change dir to f_name_tail
//    if( 0<f_name_tail.len() )
//    {
//      int err = chdir( f_name_tail.c_str() );
//      if( err ) return false;
//    }
//    // 3. getcwd - Get the current working directory and put into f_name_tail
//    if( ! getcwd( cwd, FILE_NAME_LEN ) ) return false;
//    f_name_tail = cwd;
//    // 4. concatenate f_name_tail and f_name_head into in_out_fname
//    sprintf( cwd, "%s%c%s", f_name_tail.c_str(), DIR_DELIM, f_name_head.c_str() );
//    in_out_fname = cwd;
//  }
//  return true;
//}

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
      // in_out_fname is a already full path, so just return
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

void GetFnameHeadAndTail( const String& in_fname, String& head, String& tail )
{
  head.clear();
  tail.clear();

  // This const_cast is okay because we are not changing in_fname_cp:
  char* in_fname_cp = CCast<char*>(in_fname.c_str());
  char* const last_slash = strrchr( in_fname_cp, DIR_DELIM );
  if( last_slash )
  {
    for( char* cp = last_slash + 1; *cp; cp++ ) head.push( *cp );
    for( char* cp = in_fname_cp; cp < last_slash; cp++ ) tail.push( *cp );
  }
  else {
    // No tail, all head:
    for( char* cp = in_fname_cp; *cp; cp++ ) head.push( *cp );
  }
}

String GetFnameHead( const char* in_full_fname )
{
  String head;

  // This const_cast is okay because we are not changing in_fname_cp:
  char* in_fname_cp = CCast<char*>(in_full_fname);
  char* const last_slash = strrchr( in_fname_cp, DIR_DELIM );
  if( last_slash )
  {
    for( char* cp = last_slash + 1; *cp; cp++ ) head.push( *cp );
  }
  else {
    // No tail, all head:
    for( char* cp = in_fname_cp; *cp; cp++ ) head.push( *cp );
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
    for( char* cp = in_fname_cp; cp<last_slash; cp++ ) tail.push( *cp );
  }
  return tail;
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

double GetTimeSeconds()
{
  timeval tv;

  gettimeofday( &tv, 0 );

  return tv.tv_sec + double(tv.tv_usec)/1e6;
}
#endif

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

ConstCharList* Trace::mp_Call_Stack = 0;

void Trace::Allocate()
{
  mp_Call_Stack = new(__FILE__,__LINE__) ConstCharList(__FILE__,__LINE__);
}

void Trace::Cleanup()
{
  MemMark(__FILE__,__LINE__); delete mp_Call_Stack; mp_Call_Stack = 0;
}

Trace::Trace( const char* func_name )
{
  mp_Call_Stack->push(__FILE__,__LINE__, func_name );
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

