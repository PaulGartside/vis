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

#include "Types.hh"

const char BS  =   8; // Backspace
const char ESC =  27; // Escape
const char DEL = 127; // Delete

class Vis;

class Console
{
public:
  static void Allocate();
  static void Cleanup();
  static void AtExit();
  static bool Set_tty();
  static void SetConsoleCursor(); // Win32
  static void SetSignals();       // Unix
  static void SetVis( Vis* p_vis );

  static bool Update();
  static void Refresh();
  static void Invalidate();
  static void Flush();

  static bool     GetWindowSize();
  static unsigned Num_Rows();
  static unsigned Num_Cols();

  static char KeyIn();

  static void Set_Normal();
  static void NewLine();
  static void Move_2_Row_Col( const unsigned ROW, const unsigned COL ); // Not used
  static void Set( const unsigned ROW, const unsigned COL, const uint8_t C, const Style S );
  static void SetS( const unsigned ROW, const unsigned COL, const char* str, const Style S );

  static void Set_Color_Scheme_1();
  static void Set_Color_Scheme_2();
  static void Set_Color_Scheme_3();
  static void Set_Color_Scheme_4();
  static void Set_Color_Scheme_5();
};

#endif

