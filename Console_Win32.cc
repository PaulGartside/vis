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

#undef UNICODE
#undef _UNICODE
#undef _MBCS

// Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
//#include <SDKDDKVer.h>
#include <windows.h>
#include <tchar.h>

#include <stdio.h>     // memcpy, memset
#include <string.h>    // memcpy, memset
#include <unistd.h>    // write, ioctl[unix], read
#include <signal.h>
#include <stdarg.h>    // va_list, va_start, va_end

#include "MemLog.hh"
#include "Utilities.hh"
#include "Vis.hh"
#include "View.hh"
#include "Console.hh"

extern Vis* gl_pVis;
extern MemLog<MEM_LOG_BUF_SIZE> Log;

// Diff has Blue background:

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

Color BORDER_HI_FG = White;  Color BORDER_HI_FG_RV = Green;
Color BORDER_HI_BG = Green;  Color BORDER_HI_BG_RV = White;

Color NONASCII_FG = Red;     Color NONASCII_FG_RV = Blue;
Color NONASCII_BG = Blue;    Color NONASCII_BG_RV = Red;

Color EMPTY_BG = Black;
Color EMPTY_FG = Red;

// Diff has Blue background:
Color NORMAL_DIFF_FG = White;
Color NORMAL_DIFF_BG = Blue;

Color STAR_DIFF_FG =  Blue;
Color STAR_DIFF_BG =  Red;

Color COMMENT_DIFF_FG = White;
Color COMMENT_DIFF_BG = Blue;

Color DEFINE_DIFF_FG = Magenta;
Color DEFINE_DIFF_BG = Blue;

Color QUOTE_DIFF_FG =  Cyan;
Color QUOTE_DIFF_BG =  Blue;

Color CONTROL_DIFF_FG = Yellow;
Color CONTROL_DIFF_BG = Blue;

Color VARTYPE_DIFF_FG = Green;
Color VARTYPE_DIFF_BG = Blue;

Color VISUAL_DIFF_FG = Blue;
Color VISUAL_DIFF_BG = Red;

Color DELETED_DIFF_FG = White;
Color DELETED_DIFF_BG = Red;

unsigned Console::num_rows = 0;
unsigned Console::num_cols = 0;

static LinesList* lines__p = 0; // list of screen lines  pending to be written.
static LinesList* lines__w = 0; // list of screen lines  already written.
static LinesList* styles_p = 0; // list of screen styles pending to be written.
static LinesList* styles_w = 0; // list of screen styles already written.
static Line*      touched  = 0; // list of screen lines that have been changed.
static Line*      out_buf  = 0; // output buffer to reduce number of write calls.

HANDLE Console::m_stdin = 0;
HANDLE Console::m_stdout = 0;
HANDLE Console::m_stderr = 0;
DWORD  Console::orig_console_mode = 0;

uint8_t Console::Byte2out( uint8_t C )
{
  char C_out = '?';

  if(  0 == C
   ||  9 == C   //  9 == '\t'
   || 13 == C ) // 13 == '\r'
  {
    C_out = ' ';
  }
  else if( (  1 <= C && C <= 8  )
        || ( 11 <= C && C <= 31 ) )
//      || (127 <= C && C <= 160) )
  {
    C_out = '?';
  }
  else if( 146 == C ) C_out = '\'';
  else if( 147 == C
        || 148 == C ) C_out = '"';
  else if( 150 == C ) C_out = '-';
  else {
    C_out = C;
  }
  return C_out;
}

void Console::Allocate()
{
  if( 0 == lines__p )
  { 
    lines__p = new(__FILE__,__LINE__) LinesList;
    lines__w = new(__FILE__,__LINE__) LinesList;
    styles_p = new(__FILE__,__LINE__) LinesList;
    styles_w = new(__FILE__,__LINE__) LinesList;
    touched  = new(__FILE__,__LINE__) Line(__FILE__,__LINE__);
    out_buf  = new(__FILE__,__LINE__) Line(__FILE__,__LINE__, 16385 );
  }
}

void Console::Cleanup()
{
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

unsigned Console::PrintC( uint8_t C )
{
  unsigned bytes_written = 1;

  char C_out = Byte2out( C );

  WriteConsole( m_stdout, &C_out, 1, NULL, NULL );

  return bytes_written;
}

unsigned Console::PrintB( Line& B )
{
  const unsigned LEN = B.len();

  if( LEN )
  {
    WriteConsole( m_stdout, B.c_str(0), LEN, NULL, NULL );

    B.clear();
  }
  return LEN;
}

void Console::Flush()
{
  // Needed only by UNIX
}

void Console::Move_2_Home()
{
  COORD pos = { 0, 0 };

  BOOL ok = SetConsoleCursorPosition( m_stdout, pos );

  ASSERT( __LINE__, ok, "ok" );
}

void Console::Move_2_Row_Col( const unsigned ROW, const unsigned COL )
{
  COORD pos = { COL, ROW };

  BOOL ok = SetConsoleCursorPosition( m_stdout, pos );

  ASSERT( __LINE__, ok, "ok" );
}

void Console::Set( const unsigned ROW
                 , const unsigned COL
                 , const uint8_t  C
                 , const Style    S )
{
  (*lines__p)[ ROW ]->set( COL, C );
  (*styles_p)[ ROW ]->set( COL, S );
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

bool Console::Update()
{
  // Use out_buf to reduce kernel write calls, to improve scrolling speed.
  // out_buf accumulates as many characters as possible before a write is
  // necessary, which is when the cursor changes position or the style
  // changes, or when changing to a new row.
  out_buf->clear(); // Should already be cleared, do it again just in case

  bool output_something = false;
  unsigned crs_row   = ~0; // Cursor row
  unsigned crs_col   = ~0; // Cursor col
  static uint8_t crs_style = S_UNKNOWN;

  for( unsigned row=0; row<num_rows; row++ )
  {
    if( touched->get(row) )
    for( unsigned col=0; col<num_cols; col++ )
    {
      if( row == num_rows-1 && col == num_cols-1 ) continue;

      const uint8_t c_p = (*lines__p)[row]->get(col); // char pending
      const uint8_t c_w = (*lines__w)[row]->get(col); // char written
      const uint8_t s_p = (*styles_p)[row]->get(col); // style pending
      const uint8_t s_w = (*styles_w)[row]->get(col); // style written

      if( c_p != c_w || s_p != s_w || s_w == S_UNKNOWN )
      {
        if( crs_row != row || crs_col != col )
        {
          PrintB( *out_buf ); // clears out_buf
          Move_2_Row_Col( row, col );
          crs_row = row;
          crs_col = col;
        }
        if( crs_style != s_p )
        {
          PrintB( *out_buf ); // clears out_buf
          Set_Style( Style_2_BG(s_p), Style_2_FG(s_p), Style_2_BB(s_p) );
          crs_style = s_p;
        }
        out_buf->push(__FILE__,__LINE__, Byte2out( c_p ) );
        (*lines__w)[row]->set( col, c_p );
        (*styles_w)[row]->set( col, s_p );
        crs_col++;
        output_something = true;
      }
      else {
        PrintB( *out_buf ); // clears out_buf
      }
    }
    PrintB( *out_buf ); // clears out_buf
    touched->set( row, 0 );
  }
  return output_something;
}

void Console::Invalidate()
{
  // Invalidate all written styles:
  for( unsigned row=0; row<num_rows; row++ )
  {
    touched->set( row, 1 );
    for( unsigned col=0; col<num_cols; col++ )
    {
      (*styles_w)[row]->set( col, S_UNKNOWN );
    }
  }
}

void Console::Refresh()
{
  Invalidate();
 
  Update();

  gl_pVis->CV()->PrintCursor();
}

void Console::NewLine()
{
  PrintC('\n');
}

void Console::Set_Color_Scheme_1()
{
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

  BORDER_HI_FG = White;    BORDER_HI_FG_RV = Green;
  BORDER_HI_BG = Green;    BORDER_HI_BG_RV = White;

  NONASCII_FG = Red;       NONASCII_FG_RV = Blue;
  NONASCII_BG = Blue;      NONASCII_BG_RV = Red;

  EMPTY_FG = Red;
  EMPTY_BG = Black;

  // Diff has Blue background:
  NORMAL_DIFF_FG = White;
  NORMAL_DIFF_BG = Blue;

  STAR_DIFF_FG =  Blue;
  STAR_DIFF_BG =  Red;

  COMMENT_DIFF_FG = White;
  COMMENT_DIFF_BG = Blue;

  DEFINE_DIFF_FG = Magenta;
  DEFINE_DIFF_BG = Blue;

  QUOTE_DIFF_FG =  Cyan;
  QUOTE_DIFF_BG =  Blue;

  CONTROL_DIFF_FG = Yellow;
  CONTROL_DIFF_BG = Blue;

  VARTYPE_DIFF_FG = Green;
  VARTYPE_DIFF_BG = Blue;

  VISUAL_DIFF_FG = Blue;
  VISUAL_DIFF_BG = Red;

  DELETED_DIFF_FG = White;
  DELETED_DIFF_BG = Red;

  Refresh();
}

void Console::Set_Color_Scheme_2()
{
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

  BORDER_HI_FG = White;    BORDER_HI_FG_RV = Green;
  BORDER_HI_BG = Green;    BORDER_HI_BG_RV = White;

  NONASCII_FG = Yellow;    NONASCII_FG_RV = Cyan;
  NONASCII_BG = Cyan;      NONASCII_BG_RV = Yellow;

  EMPTY_FG = Red;
  EMPTY_BG = Blue;

  // Diff has Blue background:
  NORMAL_DIFF_FG = White;
  NORMAL_DIFF_BG = Blue;

  STAR_DIFF_FG =  Blue;
  STAR_DIFF_BG =  Red;

  COMMENT_DIFF_FG = White;
  COMMENT_DIFF_BG = Blue;

  DEFINE_DIFF_FG = Magenta;
  DEFINE_DIFF_BG = Blue;

  QUOTE_DIFF_FG =  Cyan;
  QUOTE_DIFF_BG =  Blue;

  CONTROL_DIFF_FG = Yellow;
  CONTROL_DIFF_BG = Blue;

  VARTYPE_DIFF_FG = Green;
  VARTYPE_DIFF_BG = Blue;

  VISUAL_DIFF_FG = Blue;
  VISUAL_DIFF_BG = Red;

  DELETED_DIFF_FG = White;
  DELETED_DIFF_BG = Red;

  Refresh();
}

void Console::Set_Color_Scheme_3()
{
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

  BORDER_HI_FG = White;    BORDER_HI_FG_RV = Green;
  BORDER_HI_BG = Green;    BORDER_HI_BG_RV = White;

  NONASCII_FG = Yellow;    NONASCII_FG_RV = Cyan;
  NONASCII_BG = Cyan;      NONASCII_BG_RV = Yellow;

  EMPTY_BG = White;

  // Diff has Blue background:
  NORMAL_DIFF_FG = White;
  NORMAL_DIFF_BG = Blue;

  STAR_DIFF_FG =  Blue;
  STAR_DIFF_BG =  Red;

  COMMENT_DIFF_FG = White;
  COMMENT_DIFF_BG = Blue;

  DEFINE_DIFF_FG = Magenta;
  DEFINE_DIFF_BG = Blue;

  QUOTE_DIFF_FG =  Cyan;
  QUOTE_DIFF_BG =  Blue;

  CONTROL_DIFF_FG = Yellow;
  CONTROL_DIFF_BG = Blue;

  VARTYPE_DIFF_FG = Green;
  VARTYPE_DIFF_BG = Blue;

  VISUAL_DIFF_FG = Blue;
  VISUAL_DIFF_BG = Red;

  DELETED_DIFF_FG = White;
  DELETED_DIFF_BG = Red;

  Refresh();
}

void Console::Set_Color_Scheme_4()
{
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

  BORDER_HI_FG = White;    BORDER_HI_FG_RV = Green;
  BORDER_HI_BG = Green;    BORDER_HI_BG_RV = White;

  NONASCII_FG = Yellow;    NONASCII_FG_RV = Cyan;
  NONASCII_BG = Cyan;      NONASCII_BG_RV = Yellow;

  EMPTY_BG = Black;

  // Diff has Blue background:
  NORMAL_DIFF_FG = White;
  NORMAL_DIFF_BG = Blue;

  STAR_DIFF_FG =  Blue;
  STAR_DIFF_BG =  Red;

  COMMENT_DIFF_FG = White;
  COMMENT_DIFF_BG = Blue;

  DEFINE_DIFF_FG = Magenta;
  DEFINE_DIFF_BG = Blue;

  QUOTE_DIFF_FG =  Cyan;
  QUOTE_DIFF_BG =  Blue;

  CONTROL_DIFF_FG = Yellow;
  CONTROL_DIFF_BG = Blue;

  VARTYPE_DIFF_FG = Green;
  VARTYPE_DIFF_BG = Blue;

  VISUAL_DIFF_FG = Blue;
  VISUAL_DIFF_BG = Red;

  DELETED_DIFF_FG = White;
  DELETED_DIFF_BG = Red;

  Refresh();
}

void Console::Set_Color_Scheme_5()
{
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

Color Console::Style_2_BG( const uint8_t S )
{
  Color c = Black; // Default

  switch( S )
  {
  case S_NORMAL   : c = NORMAL_BG;    break;
  case S_STATUS   : c = STATUS_BG;    break;
  case S_BORDER   : c = STATUS_BG;    break;
  case S_BORDER_HI: c = BORDER_HI_BG; break;
  case S_BANNER   : c = BANNER_BG;    break;
  case S_STAR     : c = STAR_BG;      break;
  case S_COMMENT  : c = COMMENT_BG;   break;
  case S_DEFINE   : c = DEFINE_BG;    break;
  case S_CONST    : c = QUOTE_BG;     break;
  case S_CONTROL  : c = CONTROL_BG;   break;
  case S_VARTYPE  : c = VARTYPE_BG;   break;
  case S_VISUAL   : c = VISUAL_BG;    break;
  case S_NONASCII : c = NONASCII_BG;  break;
  case S_EMPTY    : c = EMPTY_BG;     break;

  case S_RV_NORMAL   : c = NORMAL_BG_RV;    break;
  case S_RV_STATUS   : c = STATUS_BG_RV;    break;
  case S_RV_BORDER   : c = STATUS_BG_RV;    break;
  case S_RV_BORDER_HI: c = BORDER_HI_BG_RV; break;
  case S_RV_BANNER   : c = BANNER_BG_RV;    break;
  case S_RV_STAR     : c = STAR_BG_RV;      break;
  case S_RV_COMMENT  : c = COMMENT_BG_RV;   break;
  case S_RV_DEFINE   : c = DEFINE_BG_RV;    break;
  case S_RV_CONST    : c = QUOTE_BG_RV;     break;
  case S_RV_CONTROL  : c = CONTROL_BG_RV;   break;
  case S_RV_VARTYPE  : c = VARTYPE_BG_RV;   break;
  case S_RV_VISUAL   : c = VISUAL_BG_RV;    break;
  case S_RV_NONASCII : c = NONASCII_BG_RV;  break;

  case S_DIFF_NORMAL : c = NORMAL_DIFF_BG;  break;
  case S_DIFF_STAR   : c = STAR_DIFF_BG;    break;
  case S_DIFF_COMMENT: c = COMMENT_DIFF_BG; break;
  case S_DIFF_DEFINE : c = DEFINE_DIFF_BG;  break;
  case S_DIFF_CONST  : c = QUOTE_DIFF_BG;   break;
  case S_DIFF_CONTROL: c = CONTROL_DIFF_BG; break;
  case S_DIFF_VARTYPE: c = VARTYPE_DIFF_BG; break;
  case S_DIFF_VISUAL : c = VISUAL_DIFF_BG;  break;
  case S_DIFF_DEL    : c = DELETED_DIFF_BG; break;
  }
  return c;
}

Color Console::Style_2_FG( const uint8_t S )
{
  Color c = White; // Default

  switch( S )
  {
  case S_NORMAL   : c = NORMAL_FG;    break;
  case S_STATUS   : c = STATUS_FG;    break;
  case S_BORDER   : c = STATUS_FG;    break;
  case S_BORDER_HI: c = BORDER_HI_FG; break;
  case S_BANNER   : c = BANNER_FG;    break;
  case S_STAR     : c = STAR_FG;      break;
  case S_COMMENT  : c = COMMENT_FG;   break;
  case S_DEFINE   : c = DEFINE_FG;    break;
  case S_CONST    : c = QUOTE_FG;     break;
  case S_CONTROL  : c = CONTROL_FG;   break;
  case S_VARTYPE  : c = VARTYPE_FG;   break;
  case S_VISUAL   : c = VISUAL_FG;    break;
  case S_NONASCII : c = NONASCII_FG;  break;
  case S_EMPTY    : c = EMPTY_FG;     break;

  case S_RV_NORMAL   : c = NORMAL_FG_RV;    break;
  case S_RV_STATUS   : c = STATUS_FG_RV;    break;
  case S_RV_BORDER   : c = STATUS_FG_RV;    break;
  case S_RV_BORDER_HI: c = BORDER_HI_FG_RV; break;
  case S_RV_BANNER   : c = BANNER_FG_RV;    break;
  case S_RV_STAR     : c = STAR_FG_RV;      break;
  case S_RV_COMMENT  : c = COMMENT_FG_RV;   break;
  case S_RV_DEFINE   : c = DEFINE_FG_RV;    break;
  case S_RV_CONST    : c = QUOTE_FG_RV;     break;
  case S_RV_CONTROL  : c = CONTROL_FG_RV;   break;
  case S_RV_VARTYPE  : c = VARTYPE_FG_RV;   break;
  case S_RV_VISUAL   : c = VISUAL_FG_RV;    break;
  case S_RV_NONASCII : c = NONASCII_FG_RV;  break;

  case S_DIFF_NORMAL : c = NORMAL_DIFF_FG;  break;
  case S_DIFF_STAR   : c = STAR_DIFF_FG;    break;
  case S_DIFF_COMMENT: c = COMMENT_DIFF_FG; break;
  case S_DIFF_DEFINE : c = DEFINE_DIFF_FG;  break;
  case S_DIFF_CONST  : c = QUOTE_DIFF_FG;   break;
  case S_DIFF_CONTROL: c = CONTROL_DIFF_FG; break;
  case S_DIFF_VARTYPE: c = VARTYPE_DIFF_FG; break;
  case S_DIFF_VISUAL : c = VISUAL_DIFF_FG;  break;
  case S_DIFF_DEL    : c = DELETED_DIFF_FG; break;
  }
  return c;
}

bool Console::Style_2_BB( const uint8_t S )
{
  return true;
}

void Console::Set_Normal()
{
}

void Console::Set_Style( const Color BG, const Color FG, const bool BB )
{
  WORD attributes = Background_2_Code( BG )
                  | Foreground_2_Code( FG );

  BOOL ok = SetConsoleTextAttribute( m_stdout, attributes );

  ASSERT( __LINE__, ok, "ok" );
}

int Console::Background_2_Code( const Color BG )
{
  int code = 0;

  if     ( BG == Black  ) code = 0;
  else if( BG == Red    ) code = BACKGROUND_RED;
  else if( BG == Green  ) code = BACKGROUND_GREEN;
  else if( BG == Yellow ) code = BACKGROUND_RED   | BACKGROUND_GREEN;
  else if( BG == Blue   ) code = BACKGROUND_BLUE;
  else if( BG == Magenta) code = BACKGROUND_RED   | BACKGROUND_BLUE;
  else if( BG == Cyan   ) code = BACKGROUND_GREEN | BACKGROUND_BLUE;
  else if( BG == White  ) code = BACKGROUND_RED   | BACKGROUND_GREEN | BACKGROUND_BLUE;

  if( code ) code |= BACKGROUND_INTENSITY ;

  return code;
}

int Console::Foreground_2_Code( const Color FG )
{
  int code = 0;

  if     ( FG == Black  ) code = 0;
  else if( FG == Red    ) code = FOREGROUND_RED;
  else if( FG == Green  ) code = FOREGROUND_GREEN;
  else if( FG == Yellow ) code = FOREGROUND_RED   | FOREGROUND_GREEN;
  else if( FG == Blue   ) code = FOREGROUND_BLUE;
  else if( FG == Magenta) code = FOREGROUND_RED   | FOREGROUND_BLUE;
  else if( FG == Cyan   ) code = FOREGROUND_GREEN | FOREGROUND_BLUE;
  else if( FG == White  ) code = FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_BLUE;

  if( code ) code |= FOREGROUND_INTENSITY ;

  return code;
}

void Console::AtExit()
{
  // Only needed by Unix
}

bool Console::Set_tty()
{
  m_stdin  = CreateFile( "CONIN$"
                       , GENERIC_READ | GENERIC_WRITE
                       , FILE_SHARE_READ
                       , NULL
                       , OPEN_EXISTING
                       , NULL
                       , NULL );
  ASSERT( __LINE__, INVALID_HANDLE_VALUE != m_stdin, "Failed to get m_stdint");

  m_stdout = CreateFile( "CONOUT$"
                       , GENERIC_READ | GENERIC_WRITE
                       , FILE_SHARE_WRITE
                       , NULL
                       , OPEN_EXISTING
                       , NULL
                       , NULL );
  ASSERT( __LINE__, INVALID_HANDLE_VALUE != m_stdout, "Failed to get m_stdout");

  m_stderr = GetStdHandle( STD_ERROR_HANDLE );
  ASSERT( __LINE__, INVALID_HANDLE_VALUE != m_stderr, "Failed to get m_stderr");

  //////////////////////////////////////////////////////////////////////////////
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  BOOL ok = GetConsoleScreenBufferInfo( m_stdout, &csbi );
  ASSERT( __LINE__, ok, "ok" );

  num_rows = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
  num_cols = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
  //////////////////////////////////////////////////////////////////////////////

  BOOL
  okay = GetConsoleMode( m_stdin, &orig_console_mode );
  ASSERT( __LINE__, okay, "GetConsoleMode() failed" );

  if( okay )
  {
  //DWORD fdwMode = 0;
    DWORD fdwMode = ENABLE_WINDOW_INPUT;
    okay = SetConsoleMode( m_stdin, fdwMode );
    ASSERT( __LINE__, okay, "SetConsoleMode() failed" );
  }
  return okay;
}

bool Console::GetWindowSize()
{
  Trace trace( __PRETTY_FUNCTION__ );

  CONSOLE_SCREEN_BUFFER_INFO csbi;
  BOOL okay = GetConsoleScreenBufferInfo( m_stdout, &csbi );
  ASSERT( __LINE__, okay, "okay" );

  const unsigned n_num_rows = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
  const unsigned n_num_cols = csbi.srWindow.Right  - csbi.srWindow.Left + 1;

  bool changed_size = n_num_rows != num_rows
                   || n_num_cols != num_cols
                   || n_num_rows != lines__p->len();

  num_rows = n_num_rows;
  num_cols = n_num_cols;

  if( changed_size )
  {
    while( lines__p->len() < num_rows )
    {
      lines__p->push( gl_pVis->BorrowLine(__FILE__,__LINE__) );
      lines__w->push( gl_pVis->BorrowLine(__FILE__,__LINE__) );
      styles_p->push( gl_pVis->BorrowLine(__FILE__,__LINE__) );
      styles_w->push( gl_pVis->BorrowLine(__FILE__,__LINE__) );
    }
    while( num_rows < lines__p->len() )
    {
      Line* lp = 0;
      lines__p->pop( lp ); gl_pVis->ReturnLine( lp );
      lines__w->pop( lp ); gl_pVis->ReturnLine( lp );
      styles_p->pop( lp ); gl_pVis->ReturnLine( lp );
      styles_w->pop( lp ); gl_pVis->ReturnLine( lp );
    }
    for( unsigned k=0; k<num_rows; k++ )
    {
      (*lines__p)[k]->set_len(__FILE__,__LINE__, num_cols );
      (*lines__w)[k]->set_len(__FILE__,__LINE__, num_cols );
      (*styles_p)[k]->set_len(__FILE__,__LINE__, num_cols );
      (*styles_w)[k]->set_len(__FILE__,__LINE__, num_cols );
    }
    touched->set_len(__FILE__,__LINE__, num_rows );
  }
  return true;
}

void Console::SetConsoleCursor()
{
  CONSOLE_CURSOR_INFO CCI = { 100, true };
  BOOL ok = SetConsoleCursorInfo( m_stdout, &CCI );
  ASSERT( __LINE__, ok, "ok" );
}

void Console::SetSignals()
{
  // Only needed by UNIX
}

unsigned Console::Num_Rows()
{
  return num_rows;
}

unsigned Console::Num_Cols()
{
  return num_cols;
}

char Console::KeyIn()
{
  CHAR c = 0;

  static unsigned count = 0;

  bool read_key = false;
  while( !read_key  )
  {
    DWORD rval = WaitForSingleObject( m_stdin, 50 );
    ASSERT( __LINE__, WAIT_FAILED != rval, "WaitForSingleObject() failed" );

    if( WAIT_OBJECT_0 == rval )
    {
      DWORD cNumRead;
      INPUT_RECORD input_record[1];
      BOOL ok = ReadConsoleInput( m_stdin       // input buffer handle
                                , input_record  // buffer to read into
                                , 1             // size of read buffer
                                , &cNumRead );  // number of records read
      ASSERT( __LINE__, ok, "ReadConsoleInput() failed" );

      for( DWORD i = 0; i < cNumRead; i++ )
      {
        const WORD et = input_record[i].EventType;

        if( KEY_EVENT == et )
        {
          const KEY_EVENT_RECORD ker = input_record[i].Event.KeyEvent;

          if( ker.bKeyDown )
          {
            c = ker.uChar.AsciiChar;

            if( c ) read_key = true;
          }
        }
      }
    }
    if( !read_key )
    {
      // Try to use less CPU time while waiting:
      if( 0==count ) gl_pVis->CheckWindowSize(); // If window has resized, update window
      if( 4==count ) gl_pVis->CheckFileModTime();

      bool updated_sts_line = gl_pVis->Update_Status_Lines();
      bool updated_chg_sts  = gl_pVis->Update_Change_Statuses();

      if( updated_sts_line || updated_chg_sts )
      {
        Console::Update();
        gl_pVis->CV()->PrintCursor();
        Console::Flush();
      }
      count++;
      if( 8==count ) count=0;
    }
  }
  return c;
}

