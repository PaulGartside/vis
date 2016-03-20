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

#ifndef __CONSOLE_HH__
#define __CONSOLE_HH__

#ifdef WIN32
#include <windows.h>
#include <tchar.h>
#endif

#include "Types.hh"

typedef unsigned char  uint8_t;

const char BS  =   8; // Backspace
const char ESC =  27; // Escape
const char DEL = 127; // Delete

class Console
{
public:
  static void Allocate();
  static void Cleanup();
  static void Move_2_Home();
  static void Move_2_Row_Col( const unsigned ROW, const unsigned COL );
  static void Screen_Save();
  static void Screen_Restore();
  static void Set( const unsigned ROW, const unsigned COL, const uint8_t C, const Style S );
  static void SetS( const unsigned ROW, const unsigned COL, const char* str, const Style S );
  static bool Update();
  static void Invalidate();
  static void Refresh();
  static void Flush();
  static void NewLine();
  static void Set_Normal();

  static void Set_Color_Scheme_1();
  static void Set_Color_Scheme_2();
  static void Set_Color_Scheme_3();
  static void Set_Color_Scheme_4();
  static void Set_Color_Scheme_5();

  static void AtExit();
  static bool Set_tty();
#ifndef WIN32
  static void Reset_tty();
  static void Sig_Handle_SIGCONT( int signo );
  static void Sig_Handle_HW     ( int signo );
#endif
  static bool GetWindowSize();
  static void SetConsoleCursor();
  static void SetSignals();

  static unsigned Num_Rows();
  static unsigned Num_Cols();

  static char KeyIn();

private:
  static Color Style_2_BG( const uint8_t S );
  static Color Style_2_FG( const uint8_t S );
  static bool  Style_2_BB( const uint8_t S );

  static void Set_Style( const Color BG, const Color FG, const bool BB=false );

  static uint8_t  Byte2out( uint8_t C );
  static unsigned PrintC( const uint8_t C );
  static unsigned PrintB( Line& B );

  static int Background_2_Code( const Color BG );
  static int Foreground_2_Code( const Color FG );

  static unsigned num_rows;
  static unsigned num_cols;

#ifdef WIN32
  static HANDLE m_stdin;
  static HANDLE m_stdout;
  static HANDLE m_stderr;
  static DWORD  orig_console_mode;
#endif
};

#endif

