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

#ifndef __MEM_LOG__HH__
#define __MEM_LOG__HH__

#include <stdarg.h>    // va_list, va_start, va_end
#include <stdio.h>     // printf, stderr, FILE, fopen, fclose

const unsigned MEM_LOG_BUF_SIZE = 102400;

template <int SIZE>
class MemLog
{
public:
  MemLog();

  // Returns true if message was logged
  bool Log( const char* msg, ... );

  void Dump();

private:
  unsigned m_offset;
  char     m_buffer[SIZE];
};

template <int SIZE>
MemLog<SIZE>::MemLog() : m_offset( 0 )
{
  m_buffer[0] = 0;
}

template <int SIZE>
bool MemLog<SIZE>::Log( const char* msg, ... )
{
  if( !msg ) return false;

  char buf[1024];

  va_list list;
  va_start( list, msg );
  const int LEN = vsprintf( buf, msg, list);
  va_end( list );

  if( m_offset+LEN < SIZE )
  {
    m_offset += sprintf( m_buffer + m_offset, "%s", buf );

    return true;
  }
  return false;
}

template <int SIZE>
void MemLog<SIZE>::Dump()
{
  printf( "%s", m_buffer );

  m_buffer[0] = 0;
  m_offset    = 0;
}

#endif

