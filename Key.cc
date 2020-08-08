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

#include <unistd.h>    // write, ioctl[unix], read

#include "Utilities.hh"
#include "Console.hh"
#include "Key.hh"

Key::Key()
  : save_2_dot_buf_n( false )
  , save_2_dot_buf_l( false )
  , save_2_vis_buf  ( false )
  , save_2_map_buf  ( false )
  , get_from_dot_buf_n( false )
  , get_from_dot_buf_l( false )
  , get_from_map_buf( false )
  , dot_buf_n()
  , dot_buf_l()
  , vis_buf()
  , map_buf()
  , dot_buf_index_n( 0 )
  , dot_buf_index_l( 0 )
  , map_buf_index( 0 )
{
}

char Key::In()
{
  Trace trace( __PRETTY_FUNCTION__ );

  char C = 0;

  if     ( get_from_map_buf   ) C = In_MapBuf();
  else if( get_from_dot_buf_n ) C = In_DotBuf_n();
  else if( get_from_dot_buf_l ) C = In_DotBuf_l();
  else                          C = Console::KeyIn();

  if( save_2_map_buf   ) map_buf.push( C );
  if( save_2_dot_buf_n ) dot_buf_n.push( C );
  if( save_2_dot_buf_l ) dot_buf_l.push( C );
  if( save_2_vis_buf   ) vis_buf.push( C );

  return C;
}

char Key::In_DotBuf_n()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned DOT_BUF_LEN = dot_buf_n.len();

  ASSERT( __LINE__, dot_buf_index_n < DOT_BUF_LEN
                  ,"dot_buf_index_n < DOT_BUF_LEN" );

  const uint8_t C = dot_buf_n.get( dot_buf_index_n++ );

  if( DOT_BUF_LEN <= dot_buf_index_n )
  {
    get_from_dot_buf_n = false;
    dot_buf_index_n    = 0;
  }
  return C;
}

char Key::In_DotBuf_l()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned DOT_BUF_LEN = dot_buf_l.len();

  ASSERT( __LINE__, dot_buf_index_l < DOT_BUF_LEN
                  ,"dot_buf_index_l < DOT_BUF_LEN" );

  const uint8_t C = dot_buf_l.get( dot_buf_index_l++ );

  if( DOT_BUF_LEN <= dot_buf_index_l )
  {
    get_from_dot_buf_l = false;
    dot_buf_index_l    = 0;
  }
  return C;
}

char Key::In_MapBuf()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned MAP_BUF_LEN = map_buf.len();

  ASSERT( __LINE__, map_buf_index < MAP_BUF_LEN
                  ,"map_buf_index < MAP_BUF_LEN" );

  const uint8_t C = map_buf.get( map_buf_index++ );

  if( MAP_BUF_LEN <= map_buf_index )
  {
    get_from_map_buf = false;
    map_buf_index    = 0;
  }
  return C;
}

