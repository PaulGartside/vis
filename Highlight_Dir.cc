////////////////////////////////////////////////////////////////////////////////
// VI-Simplified (vis) C++ Implementation                                     //
// Copyright (c) 24 May 2016 Paul J. Gartside                                 //
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
#include "Highlight_Dir.hh"

Highlight_Dir::Highlight_Dir( FileBuf& rfb )
  : Highlight_Base( rfb )
  , m_state( &ME::Hi_In_None )
{
}

void Highlight_Dir::Run_Range( const CrsPos   st
                             , const unsigned fn )
{
  m_state = &ME::Hi_In_None;

  unsigned l=st.crsLine;
  unsigned p=st.crsChar;

  while( m_state && l<fn )
  {
    (this->*m_state)( l, p );
  }
}

void Highlight_Dir::Hi_In_None( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<m_fb.NumLines(); l++ )
  {
    const Line&    lr = m_fb.GetLine( l );
    const unsigned LL = m_fb.LineLen( l );

    if( 0<LL )
    {
      const char c_end = m_fb.Get( l, LL-1 );

      if( c_end == DIR_DELIM )
      {
        for( int k=0; k<LL-1; k++ )
        {
          const char C = m_fb.Get( l, k );
          if( C == '.' )
            m_fb.SetSyntaxStyle( l, k, HI_VARTYPE );
          else
            m_fb.SetSyntaxStyle( l, k, HI_CONTROL );
        }
        m_fb.SetSyntaxStyle( l, LL-1, HI_CONST );
      }
      else if( 1<LL )
      {
        const char c0 = m_fb.Get( l, 0 );
        const char c1 = m_fb.Get( l, 1 );

        if( c0=='.' && c1=='.' )
        {
          m_fb.SetSyntaxStyle( l, 0, HI_DEFINE );
          m_fb.SetSyntaxStyle( l, 1, HI_DEFINE );
        }
        else {
          bool found_sym_link = false;
          for( int k=0; k<LL; k++ )
          {
            const char C0 = 0<k ? m_fb.Get( l, k-1 ) : 0;
            const char C1 =       m_fb.Get( l, k );
            if( C1 == '.' )
            {
              m_fb.SetSyntaxStyle( l, k, HI_VARTYPE );
            }
            else if( C0 == '-' && C1 == '>' )
            {
              found_sym_link = true;
              // -> means symbolic link
              m_fb.SetSyntaxStyle( l, k-1, HI_DEFINE );
              m_fb.SetSyntaxStyle( l, k  , HI_DEFINE );
            }
            else if( found_sym_link && C1 == DIR_DELIM )
            {
              m_fb.SetSyntaxStyle( l, k, HI_CONST );
            }
          }
        }
      }
    }
    p = 0;
  }
  m_state = 0;
}

