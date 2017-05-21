////////////////////////////////////////////////////////////////////////////////
// VI-Simplified (vis) C++ Implementation                                     //
// Copyright (c) 02 Nov 2016 Paul J. Gartside                                 //
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

#include <string.h>    // memcpy, memset
#include <unistd.h>    // write, ioctl[unix], read
#include <fcntl.h>     // fcntl
#include <errno.h>
#ifndef WIN32
#include <sys/wait.h>
#endif

#include "String.hh"
#include "MemLog.hh"
#include "Utilities.hh"
#include "FileBuf.hh"
#include "View.hh"
#include "Vis.hh"
#include "Shell.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

static const size_t BUF_SIZE = 1024;

struct Shell::Data
{
  Data( Shell& parent
      , Vis& vis );

  ~Data();

  Shell&   self;
  Vis&     vis;
  View*    view;
  FileBuf* pfb;
  String   cmd;
  String   cmd_copy;
  String   cmd_part;
  int      fd;
  bool     fd_blocking;
  pid_t    child_pid;
  bool     running;  // true if running a shell command
  char     buffer[BUF_SIZE];

  const Line divider;
};

Shell::Data::Data( Shell& parent
                 , Vis& vis )
  : self( parent )
  , vis( vis )
  , view( 0 )
  , pfb( 0 )
  , cmd()
  , cmd_copy()
  , fd( 0 )
  , fd_blocking( true )
  , child_pid( 0 )
  , running( false )
  , divider( 40, '#')
{
}

Shell::Data::~Data()
{
}

#ifndef WIN32

bool Blocking_Cmd( Shell::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool blocking_cmd = false;

  m.cmd_copy = m.cmd; 
  bool ok = m.cmd_copy.split( " ", m.cmd_part );
  if( ok && 0 < m.cmd_part.len() )
  {
    const unsigned LEN = m.cmd_part.len();

    // Editor commands running in this console must be run in blocking mode,
    // or this instance of vis will and the child editor will be clobbering
    // each other
    if( m.cmd_part.ends_with("vis")
     || m.cmd_part.ends_with("vis.new")
     || m.cmd_part.ends_with("vim")
     || m.cmd_part.ends_with("vi" ) )
    {
      blocking_cmd = true;
    }
  }
  return blocking_cmd;
}

// On success, returns non-zero file descriptor and fills in child_pid.
// On failure, returns zero.
int Pipe_Fork_Exec( Shell::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  int fd = 0;
  int pfd[2];

  if( pipe( pfd ) < 0 ) ; // pipe() error, drop out
  else {
    if( !Blocking_Cmd( m ) )
    {
      m.fd_blocking = 0 == fcntl( pfd[0], F_SETFL, O_NONBLOCK )
                    ? false : true;
    }
    const pid_t pid = fork();
    if( pid < 0 ) ; // fork() error, drop out
    else if( 0 < pid ) // Parent
    {
      close( pfd[1] ); // Close parent write end of pipe
      fd = pfd[0];
      m.child_pid = pid;
    }
    else //( 0 == pid ) // Child
    {
      close( pfd[0] ); // Close child read end of pipe

      // Put stdout and stderr onto child write end of pipe:
      if( STDOUT_FILENO != pfd[1] ) dup2( pfd[1], STDOUT_FILENO );
      if( STDERR_FILENO != pfd[1] ) dup2( pfd[1], STDERR_FILENO );

      if( STDOUT_FILENO != pfd[1] && STDERR_FILENO != pfd[1] )
      {
        // Only close pfd[1] if is not equal to STDOUT and STDERR, or else
        // when it is closed either STDOUT and STDERR will also be closed.
        // The execl() command below will send its output to stdout
        // and stderr, which are tied to write end of pipe.
        close( pfd[1] );
      }
      ExecShell( m.cmd.c_str() );
      // If we get here, execl() failed, so make child return with 127
      _exit( 127 );
    }
  }
  return fd;
}

// Returns child termination status on success, -1 on error
int CloseFd_WaitPID( int fd, const pid_t child_pid )
{
  // Default to error value:
  int child_termination_sts = -1;

  // my_popen_r was never called:
  if( child_pid <= 0 ) ; // Invalid child_pid, drop out
  else {
    if( close( fd ) < 0 ) ; // fd was not open, drop out
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

bool Get_Shell_Cmd( Shell::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool got_command = false;

  if( 0 < m.pfb->NumLines() )
  {
    const unsigned LAST_LINE = m.pfb->NumLines()-1;

    // Find first line, which is line below last line matching '^[ ]*#':
    bool found_first_line = false;
    int first_line = 0;
    for( int l=LAST_LINE; !found_first_line && 0<=l; l-- )
    {
      const unsigned LL = m.pfb->LineLen( l );
      unsigned first_non_white = 0;
      while( first_non_white<LL && IsSpace( m.pfb->Get( l, first_non_white ) ) ) first_non_white++;
      if( first_non_white<LL && '#' == m.pfb->Get( l, first_non_white ) )
      {
        found_first_line = true;
        first_line = l+1;
      }
    }
    if( first_line <= LAST_LINE )
    {
      m.cmd.clear();
      // Concatenate all command lines into String cmd:
      for( unsigned k=first_line; k<=LAST_LINE; k++ )
      {
        const unsigned LL = m.pfb->LineLen( k );
        for( unsigned p=0; p<LL; p++ )
        {
          const uint8_t C = m.pfb->Get( k, p );
          if( C == '#' ) break; //< Ignore # to end of line
          m.cmd.push( C );
        }
        if( 0<LL && k<LAST_LINE ) m.cmd.push(' ');
      }
      m.cmd.trim(); //< Remove leading and ending spaces

      got_command = m.cmd.len() ? true : false;
    }
  }
  return got_command;
}

void Print_Divider_Move_Cursor_2_Bottom( Shell::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Add ###################################### followed by empty line
  m.pfb->PushLine( m.divider );
  m.pfb->PushLine();

  // Move cursor to bottom of file
  const unsigned NUM_LINES = m.pfb->NumLines();
  m.view->GoToCrsPos_NoWrite( NUM_LINES-1, 0 );
  m.pfb->Update();
}

// Returns true of ran command, else false
void Run_Shell_Start( Shell::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Add ######################################
  m.pfb->PushLine( m.divider );

  m.fd = Pipe_Fork_Exec( m );
  if( 0 == m.fd )
  {
    m.vis.Window_Message("\nPipe_Fork_Exec( %s ) failed\n\n", m.cmd.c_str() );

    Print_Divider_Move_Cursor_2_Bottom( m );
  }
  else {
    m.pfb->PushLine();
    // Move cursor to bottom of file
    const unsigned NUM_LINES = m.pfb->NumLines();
    m.view->GoToCrsPos_NoWrite( NUM_LINES-1, 0 );
    m.running = true;
    m.pfb->Update();
  }
}

void Join_Print_Exit_Status( Shell::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const int exit_val = CloseFd_WaitPID( m.fd, m.child_pid );
  m.running = false;

  char exit_msg[128];
  sprintf( exit_msg, "Exit_Value=%i", exit_val );

  // Append exit_msg:
  if( 0<m.pfb->LineLen( m.pfb->NumLines()-1 ) ) m.pfb->PushLine();
  const unsigned EXIT_MSG_LEN = strlen( exit_msg );
  for( unsigned k=0; k<EXIT_MSG_LEN; k++ ) m.pfb->PushChar( exit_msg[k] );
}

void Run_Blocking( Shell::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  double T1 = GetTimeSeconds();
  char C = 0;
  // 1==success, 0==EOF, -1==Error. If not 1, drop out.
  while( 0 < read( m.fd, &C, 1 ) )
  {
    if( '\n' != C ) m.pfb->PushChar( C );
    else {
      m.pfb->PushLine();
      double T2 = GetTimeSeconds();
      if( 0.5 < (T2-T1) ) {
        T1 = T2;
        // Move cursor to bottom of file
        const unsigned NUM_LINES = m.pfb->NumLines();
        m.view->GoToCrsPos_NoWrite( NUM_LINES-1, 0 );
        m.pfb->Update();
      }
    }
  }
  Join_Print_Exit_Status( m );

  Print_Divider_Move_Cursor_2_Bottom( m );
}

void Run_Non_Blocking( Shell::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const ssize_t bytes_read = read( m.fd, m.buffer, BUF_SIZE );

  if( 0 < bytes_read )
  {
    for( size_t k=0; k<bytes_read; k++ )
    {
      const int C = m.buffer[k];

      if( '\n' != C ) m.pfb->PushChar( C );
      else {
        m.pfb->PushLine();

        // Move cursor to bottom of file
        const unsigned NUM_LINES = m.pfb->NumLines();
        m.view->GoToCrsPos_NoWrite( NUM_LINES-1, 0 );
        m.pfb->Update();
      }
    }
  }
  else {
    if( bytes_read < 0 && EAGAIN == errno )
    {
      // No data currently available, but EOF not yet reached,
      // so just return and try again later
    }
    else {
      // 0==EOF, or -1==Error, either way, drop out
      Join_Print_Exit_Status( m );

      Print_Divider_Move_Cursor_2_Bottom( m );
    }
  }
}

#endif

Shell::Shell( Vis& vis )
  : m( *new(__FILE__, __LINE__) Data( *this, vis ) )
{
}

Shell::~Shell()
{
  MemMark(__FILE__,__LINE__); delete &m;
}

void Shell::Run()
{
  Trace trace( __PRETTY_FUNCTION__ );

#ifndef WIN32
  m.view = m.vis.CV(); 
  m.pfb  = m.view->GetFB();

  bool ok = Get_Shell_Cmd( m );

  if( ok )
  {
    Run_Shell_Start( m );

    if( m.running && m.fd_blocking )
    {
      Run_Blocking( m );
    }
  }
  else {
    m.view->PrintCursor();
  }
#endif
}

bool Shell::Running() const
{
  return m.running;
}

void Shell::Update()
{
  Run_Non_Blocking( m );
}

