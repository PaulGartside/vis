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

#include "Utilities.hh"
#include "FileBuf.hh"
#include "Highlight_Text.hh"

Highlight_Text::Highlight_Text( FileBuf& rfb )
  : Highlight_Base( rfb )
  , hi_state( &ME::Hi_In_None )
{
}

void Highlight_Text::Run_Range( const CrsPos   st
                              , const unsigned fn )
{
  hi_state = &ME::Hi_In_None;

  unsigned l=st.crsLine;
  unsigned p=st.crsChar;

  while( hi_state && l<fn )
  {
    (this->*hi_state)( l, p );
  }
}

void Highlight_Text::Hi_In_None( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<fb.NumLines(); l++ )
  {
    const unsigned LL = fb.LineLen( l );

    for( ; p<LL; p++ )
    {
      fb.ClearSyntaxStyles( l, p );

      const char C = fb.Get( l, p );

      if( C=='#' )
      {
        hi_state = &ME::Hi_In_Define;
      }
      else if( C < 32 || 126 < C )
      {
        fb.SetSyntaxStyle( l, p, HI_NONASCII );
      }
      if( &ME::Hi_In_None != hi_state ) return;
    }
    p = 0;
  }
  hi_state = 0;
}

void Highlight_Text::Hi_In_Define( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = fb.LineLen( l );

  for( ; p<LL; p++ )
  {
    fb.SetSyntaxStyle( l, p, HI_DEFINE );
  }
  p=0; l++;
  hi_state = &ME::Hi_In_None;
}

