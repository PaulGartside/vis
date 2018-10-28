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

#include <stdio.h>     // memcpy, memset
#include <string.h>    // memcpy, memset
#include <unistd.h>    // write, ioctl[unix], read
#include <signal.h>
#include <stdarg.h>    // va_list, va_start, va_end
#include <sys/ioctl.h> // ioctl

#if defined(OSX)
#  include <termios.h>  // struct termios
#else
#  include <termio.h>  // struct termio
#endif

#include "MemLog.hh"
#include "Utilities.hh"
#include "Vis.hh"
#include "View.hh"
#include "Console.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

#if defined(OSX)
static termios m_origTTyState;
#else
static termio m_origTTyState;
#endif

unsigned gl_bytes_out = 0;

static Vis*     mp_vis   = 0;
static unsigned m_num_rows = 0;
static unsigned m_num_cols = 0;

const int FD_IO = 0; // read/write file descriptor

const char* const STR_CLEAR       = "\E[J";       // "\E[nJ" n=0, clear from cursor to end of screen, n=1, clear from cursor to beginning of screen, n=2, clear screen
const char* const STR_HOME        = "\E[H";       // "\E[H" moves cursor home
const char* const STR_ROW_COL     = "\E[%u;%uH";  // "\E[r;cH" moves cursor to row r and column c
const char* const STR_UP          = "\E[A";
const char* const STR_UP_X        = "\E[%uA";
const char* const STR_DOWN        = "\E[B";
const char* const STR_DOWN_X      = "\E[%uB";
const char* const STR_RIGHT       = "\E[C";
const char* const STR_RIGHT_X     = "\E[%uC";
const char* const STR_LEFT        = "\E[D";
const char* const STR_LEFT_X      = "\E[%uD";
const char* const STR_NORMAL      = "\E[0m";
const char* const STR_NORMAL_DARK = "\E[0;40;37m";
const char* const STR_BRIGHT_BOLD = "\E[1m";
const char* const STR_UNDERLINE   = "\E[4m";
const char* const STR_INVERSE     = "\E[7m";
const char* const STR_DIS_CTL     = "\E[3h"; // Display control characters - not sure what this does

const char* const STR_FG_BLACK   = "\E[30m";
const char* const STR_FG_RED     = "\E[31m";
const char* const STR_FG_GREEN   = "\E[32m";
const char* const STR_FG_YELLOW  = "\E[33m";
const char* const STR_FG_BLUE    = "\E[34m";
const char* const STR_FG_MAGENTA = "\E[35m";
const char* const STR_FG_CYAN    = "\E[36m";
const char* const STR_FG_WHITE   = "\E[37m";

const char* const STR_FG_BB_BLACK   = "\E[30;1m";
const char* const STR_FG_BB_RED     = "\E[31;1m";
const char* const STR_FG_BB_GREEN   = "\E[32;1m";
const char* const STR_FG_BB_YELLOW  = "\E[33;1m";
const char* const STR_FG_BB_BLUE    = "\E[34;1m";
const char* const STR_FG_BB_MAGENTA = "\E[35;1m";
const char* const STR_FG_BB_CYAN    = "\E[36;1m";
const char* const STR_FG_BB_WHITE   = "\E[37;1m";

const char* const STR_BG_BLACK   = "\E[40m";
const char* const STR_BG_RED     = "\E[41m";
const char* const STR_BG_GREEN   = "\E[42m";
const char* const STR_BG_YELLOW  = "\E[43m";
const char* const STR_BG_BLUE    = "\E[44m";
const char* const STR_BG_MAGENTA = "\E[45m";
const char* const STR_BG_CYAN    = "\E[46m";
const char* const STR_BG_WHITE   = "\E[47m";
const char* const STR_BG_DEFAULT = "\E[49m";

const char* const STR_BG_BB_BLACK   = "\E[40;1m";
const char* const STR_BG_BB_RED     = "\E[41;1m";
const char* const STR_BG_BB_GREEN   = "\E[42;1m";
const char* const STR_BG_BB_YELLOW  = "\E[43;1m";
const char* const STR_BG_BB_BLUE    = "\E[44;1m";
const char* const STR_BG_BB_MAGENTA = "\E[45;1m";
const char* const STR_BG_BB_CYAN    = "\E[46;1m";
const char* const STR_BG_BB_WHITE   = "\E[47;1m";
const char* const STR_BG_BB_DEFAULT = "\E[49;1m";

const char* const STR_SCREEN_SAVE    = "\E[?47h";
const char* const STR_SCREEN_RESTORE = "\E[?47l";

const uint8_t LEN_CLEAR       = strlen( STR_CLEAR );
const uint8_t LEN_HOME        = strlen( STR_HOME  );
const uint8_t LEN_UP          = strlen( STR_UP    );
const uint8_t LEN_DOWN        = strlen( STR_DOWN  );
const uint8_t LEN_LEFT        = strlen( STR_LEFT  );
const uint8_t LEN_RIGHT       = strlen( STR_RIGHT );
const uint8_t LEN_NORMAL      = strlen( STR_NORMAL );
const uint8_t LEN_NORMAL_DARK = strlen( STR_NORMAL_DARK );
const uint8_t LEN_BRIGHT_BOLD = strlen( STR_BRIGHT_BOLD );
const uint8_t LEN_UNDERLINE   = strlen( STR_UNDERLINE );
const uint8_t LEN_INVERSE     = strlen( STR_INVERSE );
const uint8_t LEN_DIS_CTL     = strlen( STR_DIS_CTL );

const uint8_t LEN_FG_BLACK   = strlen( STR_FG_BLACK   );
const uint8_t LEN_FG_RED     = strlen( STR_FG_RED     );
const uint8_t LEN_FG_GREEN   = strlen( STR_FG_GREEN   );
const uint8_t LEN_FG_YELLOW  = strlen( STR_FG_YELLOW  );
const uint8_t LEN_FG_BLUE    = strlen( STR_FG_BLUE    );
const uint8_t LEN_FG_MAGENTA = strlen( STR_FG_MAGENTA );
const uint8_t LEN_FG_CYAN    = strlen( STR_FG_CYAN    );
const uint8_t LEN_FG_WHITE   = strlen( STR_FG_WHITE   );

const uint8_t LEN_FG_BB_BLACK   = strlen( STR_FG_BB_BLACK   );
const uint8_t LEN_FG_BB_RED     = strlen( STR_FG_BB_RED     );
const uint8_t LEN_FG_BB_GREEN   = strlen( STR_FG_BB_GREEN   );
const uint8_t LEN_FG_BB_YELLOW  = strlen( STR_FG_BB_YELLOW  );
const uint8_t LEN_FG_BB_BLUE    = strlen( STR_FG_BB_BLUE    );
const uint8_t LEN_FG_BB_MAGENTA = strlen( STR_FG_BB_MAGENTA );
const uint8_t LEN_FG_BB_CYAN    = strlen( STR_FG_BB_CYAN    );
const uint8_t LEN_FG_BB_WHITE   = strlen( STR_FG_BB_WHITE   );

const uint8_t LEN_BG_BLACK   = strlen( STR_BG_BLACK   );
const uint8_t LEN_BG_RED     = strlen( STR_BG_RED     );
const uint8_t LEN_BG_GREEN   = strlen( STR_BG_GREEN   );
const uint8_t LEN_BG_YELLOW  = strlen( STR_BG_YELLOW  );
const uint8_t LEN_BG_BLUE    = strlen( STR_BG_BLUE    );
const uint8_t LEN_BG_MAGENTA = strlen( STR_BG_MAGENTA );
const uint8_t LEN_BG_CYAN    = strlen( STR_BG_CYAN    );
const uint8_t LEN_BG_WHITE   = strlen( STR_BG_WHITE   );

const uint8_t LEN_BG_BB_BLACK   = strlen( STR_BG_BB_BLACK   );
const uint8_t LEN_BG_BB_RED     = strlen( STR_BG_BB_RED     );
const uint8_t LEN_BG_BB_GREEN   = strlen( STR_BG_BB_GREEN   );
const uint8_t LEN_BG_BB_YELLOW  = strlen( STR_BG_BB_YELLOW  );
const uint8_t LEN_BG_BB_BLUE    = strlen( STR_BG_BB_BLUE    );
const uint8_t LEN_BG_BB_MAGENTA = strlen( STR_BG_BB_MAGENTA );
const uint8_t LEN_BG_BB_CYAN    = strlen( STR_BG_BB_CYAN    );
const uint8_t LEN_BG_BB_WHITE   = strlen( STR_BG_BB_WHITE   );

const uint8_t LEN_SCREEN_SAVE    = strlen( STR_SCREEN_SAVE );
const uint8_t LEN_SCREEN_RESTORE = strlen( STR_SCREEN_RESTORE );

Color NORMAL_FG = White;     Color NORMAL_FG_RV = Black;
Color NORMAL_BG = Black;     Color NORMAL_BG_RV = White;

Color STATUS_FG = White;     Color STATUS_FG_RV = Blue;
Color STATUS_BG = Blue;      Color STATUS_BG_RV = White;

Color STAR_FG   = White;     Color STAR_FG_RV   = Red;
Color STAR_BG   = Red;       Color STAR_BG_RV   = White;

Color VISUAL_FG = White;     Color VISUAL_FG_RV = Red;
Color VISUAL_BG = Red;       Color VISUAL_BG_RV = White;

Color BANNER_FG = White;     Color BANNER_FG_RV = Red;
Color BANNER_BG = Red;       Color BANNER_BG_RV = White;

Color COMMENT_FG = Blue;     Color COMMENT_FG_RV = White;
Color COMMENT_BG = Black;    Color COMMENT_BG_RV = Blue;

Color DEFINE_FG  = Magenta;  Color DEFINE_FG_RV  = White;
Color DEFINE_BG  = Black;    Color DEFINE_BG_RV  = Magenta;

Color QUOTE_FG   = Cyan;     Color QUOTE_FG_RV   = Black;
Color QUOTE_BG   = Black;    Color QUOTE_BG_RV   = Cyan;

Color CONTROL_FG = Yellow;   Color CONTROL_FG_RV = Black;
Color CONTROL_BG = Black;    Color CONTROL_BG_RV = Yellow;

Color VARTYPE_FG = Green;    Color VARTYPE_FG_RV = White;
Color VARTYPE_BG = Black;    Color VARTYPE_BG_RV = Green;

Color BORDER_HI_FG = Blue;   Color BORDER_HI_FG_RV = Green;
Color BORDER_HI_BG = Green;  Color BORDER_HI_BG_RV = Blue;

Color NONASCII_FG = Red;     Color NONASCII_FG_RV = Blue;
Color NONASCII_BG = Blue;    Color NONASCII_BG_RV = Red;

Color EMPTY_BG = Black;
Color EMPTY_FG = Red;

// Diff has Blue background:
Color NORMAL_FG_DIFF = White;
Color NORMAL_BG_DIFF = Blue;

Color STAR_FG_DIFF =  Blue;
Color STAR_BG_DIFF =  Red;

Color COMMENT_FG_DIFF = White;
Color COMMENT_BG_DIFF = Blue;

Color DEFINE_FG_DIFF = Magenta;
Color DEFINE_BG_DIFF = Blue;

Color QUOTE_FG_DIFF =  Cyan;
Color QUOTE_BG_DIFF =  Blue;

Color CONTROL_FG_DIFF = Yellow;
Color CONTROL_BG_DIFF = Blue;

Color VARTYPE_FG_DIFF = Green;
Color VARTYPE_BG_DIFF = Blue;

Color VISUAL_FG_DIFF = Blue;
Color VISUAL_BG_DIFF = Red;

Color DELETED_FG_DIFF = White;
Color DELETED_BG_DIFF = Red;

static LinesList* lines__p = 0; // list of screen lines  pending to be written.
static LinesList* lines__w = 0; // list of screen lines  already written.
static LinesList* styles_p = 0; // list of screen styles pending to be written.
static LinesList* styles_w = 0; // list of screen styles already written.
static Line*      touched  = 0; // list of screen lines that have been changed.
static Line*      out_buf  = 0; // output buffer to reduce number of write calls.

#if defined( SUNOS )
uint8_t Byte2out( uint8_t C )
{
  char C_out = '?';

  if(  0 == C
   ||  9 == C   //  9 == '\t'
   || 13 == C ) // 13 == '\r'
  {
    C_out = ' ';
  }
  else if( (  1 <= C && C <= 8  )
        || ( 11 <= C && C <= 31 )
        || (127 <= C && C <= 160) )
  {
    C_out = '?';
  }
  else {
    C_out = C;
  }
  return C_out;
}
#else
// Show escape sequences:
uint8_t Byte2out( uint8_t C )
{
  char C_out = '?';

  if(  0 == C
   ||  9 == C   //  9 == '\t'
   || 13 == C ) // 13 == '\r'
  {
    C_out = ' ';
  }
  else if( (  1 <= C && C <= 8  )
        || ( 11 <= C && C <= 31 )
        || (127 <= C && C <= 255) )
  {
    C_out = '?';
  }
  else {
    C_out = C;
  }
  return C_out;
}
// Escape sequences not shown, but color the console:
//uint8_t Byte2out( uint8_t C )
//{
//  char C_out = '?';
//
//  if(  0 == C
//   ||  9 == C   //  9 == '\t'
//   || 13 == C ) // 13 == '\r'
//  {
//    C_out = ' ';
//  }
//  else if( (  1 <= C && C <= 8  )
//        || ( 11 <= C && C <= 26 )
//        || ( 28 <= C && C <= 31 )
//        || (127 <= C && C <= 255) )
//  {
//    C_out = '?';
//  }
//  else {
//    C_out = C;
//  }
//  return C_out;
//}
#endif

void Screen_Save()
{
  for( unsigned k=0; k<LEN_SCREEN_SAVE; k++ )
  {
    out_buf->push( STR_SCREEN_SAVE[k] );
  }
  Console::Flush();
}

void Screen_Restore()
{
  for( unsigned k=0; k<LEN_SCREEN_RESTORE; k++ )
  {
    out_buf->push( STR_SCREEN_RESTORE[k] );
  }
  for( unsigned k=0; k<LEN_NORMAL; k++ )
  {
    out_buf->push( STR_NORMAL[k] );
  }
  Console::Flush();
}

unsigned PrintB( Line& B )
{
  // Needed only by Win32
  return 0;
}

unsigned PrintC( uint8_t C )
{
  unsigned bytes_written = 1;

  out_buf->push( Byte2out( C ) );

  return bytes_written;
}

void Move_2_Home()
{
  for( unsigned k=0; k<LEN_HOME; k++ )
  {
    out_buf->push( STR_HOME[k] );
  }
}

int Background_2_Code( const Color BG )
{
  int code = 49;

  if     ( BG == Black  ) code = 40;
  else if( BG == Red    ) code = 41;
  else if( BG == Green  ) code = 42;
  else if( BG == Yellow ) code = 43;
  else if( BG == Blue   ) code = 44;
  else if( BG == Magenta) code = 45;
  else if( BG == Cyan   ) code = 46;
  else if( BG == White  ) code = 47;

  return code;
}

int Foreground_2_Code( const Color FG )
{
  int code = 39;

  if     ( FG == Black  ) code = 30;
  else if( FG == Red    ) code = 31;
  else if( FG == Green  ) code = 32;
  else if( FG == Yellow ) code = 33;
  else if( FG == Blue   ) code = 34;
  else if( FG == Magenta) code = 35;
  else if( FG == Cyan   ) code = 36;
  else if( FG == White  ) code = 37;

  return code;
}

void Set_Style( const Color BG, const Color FG, const bool BB )
{
  char s[32];
  const unsigned LEN = sprintf( s, "\E[%i;%i;%im"
                                 , BB ? 1 : 0
                                 , Background_2_Code( BG )
                                 , Foreground_2_Code( FG ) );
  for( unsigned k=0; k<LEN; k++ )
  {
    out_buf->push( s[k] );
  }
}

Color Style_2_BG( const uint8_t S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Color C = Black; // Default

  switch( S )
  {
  case S_NORMAL   : C = NORMAL_BG;    break;
  case S_STATUS   : C = STATUS_BG;    break;
  case S_BORDER   : C = STATUS_BG;    break;
  case S_BORDER_HI: C = BORDER_HI_BG; break;
  case S_BANNER   : C = BANNER_BG;    break;
  case S_STAR     : C = STAR_BG;      break;
  case S_COMMENT  : C = COMMENT_BG;   break;
  case S_DEFINE   : C = DEFINE_BG;    break;
  case S_CONST    : C = QUOTE_BG;     break;
  case S_CONTROL  : C = CONTROL_BG;   break;
  case S_VARTYPE  : C = VARTYPE_BG;   break;
  case S_VISUAL   : C = VISUAL_BG;    break;
  case S_NONASCII : C = NONASCII_BG;  break;
  case S_EMPTY    : C = EMPTY_BG;     break;

  case S_RV_NORMAL   : C = NORMAL_BG_RV;    break;
  case S_RV_STATUS   : C = STATUS_BG_RV;    break;
  case S_RV_BORDER   : C = STATUS_BG_RV;    break;
  case S_RV_BORDER_HI: C = BORDER_HI_BG_RV; break;
  case S_RV_BANNER   : C = BANNER_BG_RV;    break;
  case S_RV_STAR     : C = STAR_BG_RV;      break;
  case S_RV_COMMENT  : C = COMMENT_BG_RV;   break;
  case S_RV_DEFINE   : C = DEFINE_BG_RV;    break;
  case S_RV_CONST    : C = QUOTE_BG_RV;     break;
  case S_RV_CONTROL  : C = CONTROL_BG_RV;   break;
  case S_RV_VARTYPE  : C = VARTYPE_BG_RV;   break;
  case S_RV_VISUAL   : C = VISUAL_BG_RV;    break;
  case S_RV_NONASCII : C = NONASCII_BG_RV;  break;

  case S_DIFF_NORMAL : C = NORMAL_BG_DIFF;  break;
  case S_DIFF_STAR   : C = STAR_BG_DIFF;    break;
  case S_DIFF_COMMENT: C = COMMENT_BG_DIFF; break;
  case S_DIFF_DEFINE : C = DEFINE_BG_DIFF;  break;
  case S_DIFF_CONST  : C = QUOTE_BG_DIFF;   break;
  case S_DIFF_CONTROL: C = CONTROL_BG_DIFF; break;
  case S_DIFF_VARTYPE: C = VARTYPE_BG_DIFF; break;
  case S_DIFF_VISUAL : C = VISUAL_BG_DIFF;  break;
  case S_DIFF_DEL    : C = DELETED_BG_DIFF; break;
  }
  return C;
}

Color Style_2_FG( const uint8_t S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Color C = White; // Default

  switch( S )
  {
  case S_NORMAL   : C = NORMAL_FG;    break;
  case S_STATUS   : C = STATUS_FG;    break;
  case S_BORDER   : C = STATUS_FG;    break;
  case S_BORDER_HI: C = BORDER_HI_FG; break;
  case S_BANNER   : C = BANNER_FG;    break;
  case S_STAR     : C = STAR_FG;      break;
  case S_COMMENT  : C = COMMENT_FG;   break;
  case S_DEFINE   : C = DEFINE_FG;    break;
  case S_CONST    : C = QUOTE_FG;     break;
  case S_CONTROL  : C = CONTROL_FG;   break;
  case S_VARTYPE  : C = VARTYPE_FG;   break;
  case S_VISUAL   : C = VISUAL_FG;    break;
  case S_NONASCII : C = NONASCII_FG;  break;
  case S_EMPTY    : C = EMPTY_FG;     break;

  case S_RV_NORMAL   : C = NORMAL_FG_RV;    break;
  case S_RV_STATUS   : C = STATUS_FG_RV;    break;
  case S_RV_BORDER   : C = STATUS_FG_RV;    break;
  case S_RV_BORDER_HI: C = BORDER_HI_FG_RV; break;
  case S_RV_BANNER   : C = BANNER_FG_RV;    break;
  case S_RV_STAR     : C = STAR_FG_RV;      break;
  case S_RV_COMMENT  : C = COMMENT_FG_RV;   break;
  case S_RV_DEFINE   : C = DEFINE_FG_RV;    break;
  case S_RV_CONST    : C = QUOTE_FG_RV;     break;
  case S_RV_CONTROL  : C = CONTROL_FG_RV;   break;
  case S_RV_VARTYPE  : C = VARTYPE_FG_RV;   break;
  case S_RV_VISUAL   : C = VISUAL_FG_RV;    break;
  case S_RV_NONASCII : C = NONASCII_FG_RV;  break;

  case S_DIFF_NORMAL : C = NORMAL_FG_DIFF;  break;
  case S_DIFF_STAR   : C = STAR_FG_DIFF;    break;
  case S_DIFF_COMMENT: C = COMMENT_FG_DIFF; break;
  case S_DIFF_DEFINE : C = DEFINE_FG_DIFF;  break;
  case S_DIFF_CONST  : C = QUOTE_FG_DIFF;   break;
  case S_DIFF_CONTROL: C = CONTROL_FG_DIFF; break;
  case S_DIFF_VARTYPE: C = VARTYPE_FG_DIFF; break;
  case S_DIFF_VISUAL : C = VISUAL_FG_DIFF;  break;
  case S_DIFF_DEL    : C = DELETED_FG_DIFF; break;
  }
  return C;
}

bool Style_2_BB( const uint8_t S )
{
  return true;
}

void Reset_tty()
{
#if defined(OSX)
  int err = ioctl( FD_IO, TIOCSETA, &m_origTTyState );
#else
  int err = ioctl( FD_IO, TCSETA, &m_origTTyState );
#endif
  if( err ) {
    printf("\nFailed to reset tty.\n"
           "Type \"stty sane\" to reset tty.\n\n");
  }
}

void Sig_Handle_SIGCONT( int signo )
{
#ifdef OSX
  termios t;
#else
  termio t;
#endif

  t.c_cc[VMIN ] = 0;
  t.c_cc[VTIME] = 1;   // reads time out after 100 ms

  t.c_lflag &= ~( ICANON | ECHO | ECHONL | PENDIN );
  t.c_lflag |= ECHOK ;

  t.c_iflag &= ~( IXON | IXANY | IMAXBEL | IGNCR );
  t.c_iflag |= ICRNL;

  t.c_oflag |= OPOST | ONLCR;

  t.c_cflag &= ~( HUPCL );

#ifdef OSX
  ioctl( FD_IO, TIOCSETA, &t );
#else
  ioctl( FD_IO, TCSETA, &t );
#endif

  mp_vis->UpdateAll( false );
}

void Sig_Handle_HW( int signo )
{
  if     ( SIGBUS  == signo ) Log.Log("Received SIGBUS \n");
  else if( SIGIOT  == signo ) Log.Log("Received SIGIOT \n");
  else if( SIGTRAP == signo ) Log.Log("Received SIGTRAP\n");
  else if( SIGSEGV == signo ) Log.Log("Received SIGSEGV\n");
  else if( SIGINT  == signo ) Log.Log("Received SIGINT \n");
  else if( SIGTERM == signo ) Log.Log("Received SIGTERM\n");

  mp_vis->Stop();

  Trace::Print();

  MemMark(__FILE__,__LINE__); delete mp_vis; mp_vis = 0;

  Trace  ::Cleanup();
  Console::Cleanup();

  MemClean();
  Log.Dump();

  exit( signo );
}

void Console::Allocate()
{
  if( 0 == lines__p )
  {
    lines__p = new(__FILE__,__LINE__) LinesList;
    lines__w = new(__FILE__,__LINE__) LinesList;
    styles_p = new(__FILE__,__LINE__) LinesList;
    styles_w = new(__FILE__,__LINE__) LinesList;
    touched  = new(__FILE__,__LINE__) Line();
    out_buf  = new(__FILE__,__LINE__) Line( 16385 );
  }
  Screen_Save();
}

void Console::Cleanup()
{
  Screen_Restore();

  if( 0 != lines__p )
  {
    MemMark(__FILE__,__LINE__); delete lines__p; lines__p = 0;
    MemMark(__FILE__,__LINE__); delete lines__w; lines__w = 0;
    MemMark(__FILE__,__LINE__); delete styles_p; styles_p = 0;
    MemMark(__FILE__,__LINE__); delete styles_w; styles_w = 0;
    MemMark(__FILE__,__LINE__); delete touched ; touched  = 0;
    MemMark(__FILE__,__LINE__); delete out_buf ; out_buf  = 0;
  }
}

void Console::AtExit()
{
  Reset_tty();
}

#if defined(OSX)
bool Console::Set_tty()
{
  termios t;
  if( -1 == ioctl( FD_IO, TIOCGETA, &t ) ) return false;

  memcpy( &m_origTTyState, &t, sizeof(t) );

  t.c_cc[VMIN ] = 0;
  t.c_cc[VTIME] = 1;   // reads time out after 100 ms

  t.c_lflag &= ~( ICANON | ECHO | ECHONL | PENDIN );
  t.c_lflag |= ECHOK ;

  t.c_iflag &= ~( IXON | IXANY | IMAXBEL | IGNCR );
  t.c_iflag |= ICRNL;

  t.c_oflag |= OPOST | ONLCR;

  t.c_cflag &= ~( HUPCL );

  ioctl( FD_IO, TIOCSETA, &t );

  return true;
}
#else
bool Console::Set_tty()
{
  termio t;
  if( -1 == ioctl( FD_IO, TCGETA, &t ) ) return false;

  memcpy( &m_origTTyState, &t, sizeof(t) );

  t.c_cc[VMIN ] = 0;
  t.c_cc[VTIME] = 1;   // reads time out after 100 ms

  t.c_lflag &= ~( ICANON | ECHO | ECHONL | PENDIN );
  t.c_lflag |= ECHOK ;

  t.c_iflag &= ~( IXON | IXANY | IMAXBEL | IGNCR );
  t.c_iflag |= ICRNL;

  t.c_oflag |= OPOST | ONLCR;

  t.c_cflag &= ~( HUPCL );

  ioctl( FD_IO, TCSETA, &t );

  return true;
}
#endif

void Console::SetConsoleCursor()
{
  // Used only in WIN32
}

void Console::SetSignals()
{
  signal( SIGCONT, Sig_Handle_SIGCONT );
  signal( SIGBUS , Sig_Handle_HW );
  signal( SIGIOT , Sig_Handle_HW );
  signal( SIGTRAP, Sig_Handle_HW );
  signal( SIGSEGV, Sig_Handle_HW );
  signal( SIGINT , Sig_Handle_HW );
  signal( SIGTERM, Sig_Handle_HW );
}

void Console::SetVis( Vis* p_vis )
{
  mp_vis = p_vis;
}

bool Console::Update()
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool output_something = false;
  unsigned crs_row = ~0; // Cursor row
  unsigned crs_col = ~0; // Cursor col
  static uint8_t crs_style = S_UNKNOWN;

  for( unsigned row=0; row<m_num_rows; row++ )
  {
    if( touched->get( row ) )
    for( unsigned col=0; col<m_num_cols; col++ )
    {
      // Dont print bottom right cell of screen:
      if( row == m_num_rows-1 && col == m_num_cols-1 ) continue;

      const uint8_t c_p = ((*lines__p)[row])->get( col ); // char pending
      const uint8_t c_w = ((*lines__w)[row])->get( col ); // char written
      const uint8_t s_p = ((*styles_p)[row])->get( col ); // style pending
      const uint8_t s_w = ((*styles_w)[row])->get( col ); // style written

      if( c_p != c_w || s_p != s_w || s_w == S_UNKNOWN )
      {
        if( crs_row != row || crs_col != col )
        {
          Move_2_Row_Col( row, col );
          crs_row = row;
          crs_col = col;
        }
        if( crs_style != s_p )
        {
          Set_Style( Style_2_BG(s_p), Style_2_FG(s_p), Style_2_BB(s_p) );
          crs_style = s_p;
        }
        PrintC( c_p );
        ((*lines__w)[row])->set( col, c_p );
        ((*styles_w)[row])->set( col, s_p );
        crs_col++;
        output_something = true;
      }
    }
    touched->set( row, 0 );
  }
  return output_something;
}

void Console::Refresh()
{
  Trace trace( __PRETTY_FUNCTION__ );

  Invalidate();

  Update();

  mp_vis->CV()->PrintCursor();
}

void Console::Invalidate()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Invalidate all written styles:
  for( unsigned row=0; row<m_num_rows; row++ )
  {
    touched->set( row, 1 );
    for( unsigned col=0; col<m_num_cols; col++ )
    {
      ((*styles_w)[row])->set( col, S_UNKNOWN );
    }
  }
}

void Console::Flush()
{
  // Needed only by UNIX
  const unsigned LEN = out_buf->len();

  if( 0<LEN )
  {
    gl_bytes_out += write( FD_IO, out_buf->c_str(0), LEN );

    out_buf->clear();
  }
}

bool Console::GetWindowSize()
{
  Trace trace( __PRETTY_FUNCTION__ );

  winsize ws;
  int err = ioctl( FD_IO, TIOCGWINSZ, &ws );
  if( err < 0 )
  {
    DBG(__LINE__,"ioctl(%i, TIOCGWINSZ) returned %s", FD_IO, strerror(err) );
    return false;
  }
  else {
    bool changed_size = m_num_rows != ws.ws_row
                     || m_num_cols != ws.ws_col
                     || m_num_rows != lines__p->len();

    m_num_rows = ws.ws_row;
    m_num_cols = ws.ws_col;

    if( changed_size )
    {
      while( lines__p->len() < m_num_rows )
      {
        lines__p->push( mp_vis->BorrowLine(__FILE__,__LINE__) );
        lines__w->push( mp_vis->BorrowLine(__FILE__,__LINE__) );
        styles_p->push( mp_vis->BorrowLine(__FILE__,__LINE__) );
        styles_w->push( mp_vis->BorrowLine(__FILE__,__LINE__) );
      }
      while( m_num_rows < lines__p->len() )
      {
        Line* lp = 0;
        lines__p->pop( lp ); mp_vis->ReturnLine( lp );
        lines__w->pop( lp ); mp_vis->ReturnLine( lp );
        styles_p->pop( lp ); mp_vis->ReturnLine( lp );
        styles_w->pop( lp ); mp_vis->ReturnLine( lp );
      }
      for( unsigned k=0; k<m_num_rows; k++ )
      {
        (*lines__p)[k]->set_len( m_num_cols );
        (*lines__w)[k]->set_len( m_num_cols );
        (*styles_p)[k]->set_len( m_num_cols );
        (*styles_w)[k]->set_len( m_num_cols );
      }
      touched->set_len( m_num_rows );
    }
  }
  return true;
}

unsigned Console::Num_Rows()
{
  return m_num_rows;
}

unsigned Console::Num_Cols()
{
  return m_num_cols;
}

char Console::KeyIn()
{
  Trace trace( __PRETTY_FUNCTION__ );

  static Vis&     vis   = *mp_vis;
  static unsigned count = 0;

  // Ignore read errors, and escaped keys.
  // Return the first single char read.
  char str[16]; // Maximum F1-F12 and arrow key interpretation
  while( read( FD_IO, str, 1 ) != 1 )
  {
    // Try to use less CPU time while waiting:
    if( 0==count ) vis.CheckWindowSize(); // If window has resized, update window
    if( 4==count ) vis.CheckFileModTime();
    if( vis.Shell_Running() ) vis.Update_Shell();

    bool updated_sts_line = vis.Update_Status_Lines();
    bool updated_chg_sts  = vis.Update_Change_Statuses();

    if( updated_sts_line || updated_chg_sts )
    {
      Console::Update();
      vis.PrintCursor();
    }
    count++;
    if( 8==count ) count=0;
  }
  return str[0];
}

void Console::Set_Normal()
{
  for( unsigned k=0; k<LEN_NORMAL; k++ )
  {
    out_buf->push( STR_NORMAL[k] );
  }
}

void Console::NewLine()
{
  PrintC('\n');
}

void Console::Move_2_Row_Col( const unsigned ROW, const unsigned COL )
{
  char buf[32];
  const unsigned LEN = sprintf( buf, STR_ROW_COL, ROW+1, COL+1 );

  for( unsigned k=0; k<LEN; k++ )
  {
    out_buf->push( buf[k] );
  }
}

void Console::Set( const unsigned ROW
                 , const unsigned COL
                 , const uint8_t  C
                 , const Style    S )
{
  ((*lines__p)[ ROW ])->set( COL, C );
  ((*styles_p)[ ROW ])->set( COL, S );
  touched->set( ROW, 1 );
}

void Console::SetS( const unsigned ROW
                  , const unsigned COL
                  , const char*    str
                  , const Style    S )
{
  const unsigned str_len = strlen( str );

  for( unsigned k=0; k<str_len; k++ )
  {
    Set( ROW, COL+k, str[k], S );
  }
}

void Console::Set_Color_Scheme_1()
{
  Trace trace( __PRETTY_FUNCTION__ );

  NORMAL_FG = White;       NORMAL_FG_RV = Black;
  NORMAL_BG = Black;       NORMAL_BG_RV = White;

  STATUS_FG = White;       STATUS_FG_RV = Blue;
  STATUS_BG = Blue;        STATUS_BG_RV = White;

  STAR_FG   = White;       STAR_FG_RV   = Red;
  STAR_BG   = Red;         STAR_BG_RV   = White;

  VISUAL_FG = White;       VISUAL_FG_RV = Red;
  VISUAL_BG = Red;         VISUAL_BG_RV = White;

  BANNER_FG = White;       BANNER_FG_RV = Red;
  BANNER_BG = Red;         BANNER_BG_RV = White;

  COMMENT_FG = Blue;       COMMENT_FG_RV = White;
  COMMENT_BG = Black;      COMMENT_BG_RV = Blue;

  DEFINE_FG  = Magenta;    DEFINE_FG_RV  = White;
  DEFINE_BG  = Black;      DEFINE_BG_RV  = Magenta;

  QUOTE_FG   = Cyan;       QUOTE_FG_RV   = Black;
  QUOTE_BG   = Black;      QUOTE_BG_RV   = Cyan;

  CONTROL_FG = Yellow;     CONTROL_FG_RV = Black;
  CONTROL_BG = Black;      CONTROL_BG_RV = Yellow;

  VARTYPE_FG = Green;      VARTYPE_FG_RV = White;
  VARTYPE_BG = Black;      VARTYPE_BG_RV = Green;

  BORDER_HI_FG = Blue;     BORDER_HI_FG_RV = Green;
  BORDER_HI_BG = Green;    BORDER_HI_BG_RV = White;

  NONASCII_FG = Red;       NONASCII_FG_RV = Blue;
  NONASCII_BG = Blue;      NONASCII_BG_RV = Red;

  EMPTY_FG = Red;
  EMPTY_BG = Black;

  // Diff has Blue background:
  NORMAL_FG_DIFF = White;
  NORMAL_BG_DIFF = Blue;

  STAR_FG_DIFF =  Blue;
  STAR_BG_DIFF =  Red;

  COMMENT_FG_DIFF = White;
  COMMENT_BG_DIFF = Blue;

  DEFINE_FG_DIFF = Magenta;
  DEFINE_BG_DIFF = Blue;

  QUOTE_FG_DIFF =  Cyan;
  QUOTE_BG_DIFF =  Blue;

  CONTROL_FG_DIFF = Yellow;
  CONTROL_BG_DIFF = Blue;

  VARTYPE_FG_DIFF = Green;
  VARTYPE_BG_DIFF = Blue;

  VISUAL_FG_DIFF = Blue;
  VISUAL_BG_DIFF = Red;

  DELETED_FG_DIFF = White;
  DELETED_BG_DIFF = Red;

  Refresh();
}

void Console::Set_Color_Scheme_2()
{
  Trace trace( __PRETTY_FUNCTION__ );

  NORMAL_FG = White;       NORMAL_FG_RV = Black;
  NORMAL_BG = Black;       NORMAL_BG_RV = White;

  STATUS_FG = White;       STATUS_FG_RV = Blue;
  STATUS_BG = Blue;        STATUS_BG_RV = White;

  STAR_FG   = White;       STAR_FG_RV   = Red;
  STAR_BG   = Red;         STAR_BG_RV   = White;

  VISUAL_FG = White;       VISUAL_FG_RV = Red;
  VISUAL_BG = Red;         VISUAL_BG_RV = White;

  BANNER_FG = White;       BANNER_FG_RV = Red;
  BANNER_BG = Red;         BANNER_BG_RV = White;

  COMMENT_FG = Blue;       COMMENT_FG_RV = White;
  COMMENT_BG = Black;      COMMENT_BG_RV = Blue;

  DEFINE_FG  = Magenta;    DEFINE_FG_RV  = White;
  DEFINE_BG  = Black;      DEFINE_BG_RV  = Magenta;

  QUOTE_FG   = Cyan;       QUOTE_FG_RV   = Black;
  QUOTE_BG   = Black;      QUOTE_BG_RV   = Cyan;

  CONTROL_FG = Yellow;     CONTROL_FG_RV = Black;
  CONTROL_BG = Black;      CONTROL_BG_RV = Yellow;

  VARTYPE_FG = Green;      VARTYPE_FG_RV = White;
  VARTYPE_BG = Black;      VARTYPE_BG_RV = Green;

  BORDER_HI_FG = Blue;     BORDER_HI_FG_RV = Green;
  BORDER_HI_BG = Green;    BORDER_HI_BG_RV = White;

  NONASCII_FG = Yellow;    NONASCII_FG_RV = Cyan;
  NONASCII_BG = Cyan;      NONASCII_BG_RV = Yellow;

  EMPTY_FG = Red;
  EMPTY_BG = Blue;

  // Diff has Blue background:
  NORMAL_FG_DIFF = White;
  NORMAL_BG_DIFF = Blue;

  STAR_FG_DIFF =  Blue;
  STAR_BG_DIFF =  Red;

  COMMENT_FG_DIFF = White;
  COMMENT_BG_DIFF = Blue;

  DEFINE_FG_DIFF = Magenta;
  DEFINE_BG_DIFF = Blue;

  QUOTE_FG_DIFF =  Cyan;
  QUOTE_BG_DIFF =  Blue;

  CONTROL_FG_DIFF = Yellow;
  CONTROL_BG_DIFF = Blue;

  VARTYPE_FG_DIFF = Green;
  VARTYPE_BG_DIFF = Blue;

  VISUAL_FG_DIFF = Blue;
  VISUAL_BG_DIFF = Red;

  DELETED_FG_DIFF = White;
  DELETED_BG_DIFF = Red;

  Refresh();
}

void Console::Set_Color_Scheme_3()
{
  Trace trace( __PRETTY_FUNCTION__ );

  NORMAL_FG = Black;       NORMAL_FG_RV = White;
  NORMAL_BG = White;       NORMAL_BG_RV = Black;

  STATUS_FG = White;       STATUS_FG_RV = Blue;
  STATUS_BG = Blue;        STATUS_BG_RV = White;

  STAR_FG   = White;       STAR_FG_RV   = Red;
  STAR_BG   = Red;         STAR_BG_RV   = White;

  VISUAL_FG = Red;         VISUAL_FG_RV = White;
  VISUAL_BG = White;       VISUAL_BG_RV = Red;

  BANNER_FG = White;       BANNER_FG_RV = Red;
  BANNER_BG = Red;         BANNER_BG_RV = White;

  COMMENT_FG = Blue;       COMMENT_FG_RV = White;
  COMMENT_BG = White;      COMMENT_BG_RV = Blue;

  DEFINE_FG  = Magenta;    DEFINE_FG_RV  = White;
  DEFINE_BG  = White;      DEFINE_BG_RV  = Magenta;

  QUOTE_FG   = Black;      QUOTE_FG_RV   = Cyan;
  QUOTE_BG   = Cyan;       QUOTE_BG_RV   = Black;

  CONTROL_FG = Black;      CONTROL_FG_RV = Yellow;
  CONTROL_BG = Yellow;     CONTROL_BG_RV = Black;

  VARTYPE_FG = Black;      VARTYPE_FG_RV = Green;
  VARTYPE_BG = Green;      VARTYPE_BG_RV = Black;

  BORDER_HI_FG = Blue;     BORDER_HI_FG_RV = Green;
  BORDER_HI_BG = Green;    BORDER_HI_BG_RV = White;

  NONASCII_FG = Yellow;    NONASCII_FG_RV = Cyan;
  NONASCII_BG = Cyan;      NONASCII_BG_RV = Yellow;

  EMPTY_BG = White;

  // Diff has Blue background:
  NORMAL_FG_DIFF = White;
  NORMAL_BG_DIFF = Blue;

  STAR_FG_DIFF =  Blue;
  STAR_BG_DIFF =  Red;

  COMMENT_FG_DIFF = White;
  COMMENT_BG_DIFF = Blue;

  DEFINE_FG_DIFF = Magenta;
  DEFINE_BG_DIFF = Blue;

  QUOTE_FG_DIFF =  Cyan;
  QUOTE_BG_DIFF =  Blue;

  CONTROL_FG_DIFF = Yellow;
  CONTROL_BG_DIFF = Blue;

  VARTYPE_FG_DIFF = Green;
  VARTYPE_BG_DIFF = Blue;

  VISUAL_FG_DIFF = Blue;
  VISUAL_BG_DIFF = Red;

  DELETED_FG_DIFF = White;
  DELETED_BG_DIFF = Red;

  Refresh();
}

void Console::Set_Color_Scheme_4()
{
  Trace trace( __PRETTY_FUNCTION__ );

  NORMAL_FG = Black;       NORMAL_FG_RV = White;
  NORMAL_BG = White;       NORMAL_BG_RV = Black;

  STATUS_FG = White;       STATUS_FG_RV = Blue;
  STATUS_BG = Blue;        STATUS_BG_RV = White;

  STAR_FG   = White;       STAR_FG_RV   = Red;
  STAR_BG   = Red;         STAR_BG_RV   = White;

  VISUAL_FG = Red;         VISUAL_FG_RV = White;
  VISUAL_BG = White;       VISUAL_BG_RV = Red;

  BANNER_FG = White;       BANNER_FG_RV = Red;
  BANNER_BG = Red;         BANNER_BG_RV = White;

  COMMENT_FG = Blue;       COMMENT_FG_RV = White;
  COMMENT_BG = White;      COMMENT_BG_RV = Blue;

  DEFINE_FG  = Magenta;    DEFINE_FG_RV  = White;
  DEFINE_BG  = White;      DEFINE_BG_RV  = Magenta;

  QUOTE_FG   = Black;      QUOTE_FG_RV   = Cyan;
  QUOTE_BG   = Cyan;       QUOTE_BG_RV   = Black;

  CONTROL_FG = Black;      CONTROL_FG_RV = Yellow;
  CONTROL_BG = Yellow;     CONTROL_BG_RV = Black;

  VARTYPE_FG = Black;      VARTYPE_FG_RV = Green;
  VARTYPE_BG = Green;      VARTYPE_BG_RV = Black;

  BORDER_HI_FG = Blue;     BORDER_HI_FG_RV = Green;
  BORDER_HI_BG = Green;    BORDER_HI_BG_RV = White;

  NONASCII_FG = Yellow;    NONASCII_FG_RV = Cyan;
  NONASCII_BG = Cyan;      NONASCII_BG_RV = Yellow;

  EMPTY_BG = Black;

  // Diff has Blue background:
  NORMAL_FG_DIFF = White;
  NORMAL_BG_DIFF = Blue;

  STAR_FG_DIFF =  Blue;
  STAR_BG_DIFF =  Red;

  COMMENT_FG_DIFF = White;
  COMMENT_BG_DIFF = Blue;

  DEFINE_FG_DIFF = Magenta;
  DEFINE_BG_DIFF = Blue;

  QUOTE_FG_DIFF =  Cyan;
  QUOTE_BG_DIFF =  Blue;

  CONTROL_FG_DIFF = Yellow;
  CONTROL_BG_DIFF = Blue;

  VARTYPE_FG_DIFF = Green;
  VARTYPE_BG_DIFF = Blue;

  VISUAL_FG_DIFF = Blue;
  VISUAL_BG_DIFF = Red;

  DELETED_FG_DIFF = White;
  DELETED_BG_DIFF = Red;

  Refresh();
}

void Console::Set_Color_Scheme_5()
{
  Trace trace( __PRETTY_FUNCTION__ );

  STATUS_FG = White;
  STATUS_BG = Blue;

  STAR_FG   = White;
  STAR_BG   = Red;

  VISUAL_FG = White;
  VISUAL_BG = Red;

  BANNER_FG = White;
  BANNER_BG = Red;

  COMMENT_FG = Cyan;
  COMMENT_BG = Black;

  DEFINE_FG  = Magenta;
  DEFINE_BG  = Black;

  QUOTE_FG   = Blue;
  QUOTE_BG   = Black;

  CONTROL_FG = Yellow;
  CONTROL_BG = Black;

  VARTYPE_FG = Green;
  VARTYPE_BG = Black;

  Refresh();
}

