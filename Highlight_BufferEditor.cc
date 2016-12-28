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

#include <string.h>

#include "MemLog.hh"
#include "Utilities.hh"
#include "FileBuf.hh"
#include "Highlight_BufferEditor.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

extern const char* EDIT_BUF_NAME;
extern const char* HELP_BUF_NAME;
//extern const char* SRCH_BUF_NAME;
extern const char* MSG__BUF_NAME;
extern const char* SHELL_BUF_NAME;
extern const char* COLON_BUF_NAME;
extern const char* SLASH_BUF_NAME;

Highlight_BufferEditor::Highlight_BufferEditor( FileBuf& rfb )
  : Highlight_Base( rfb )
  , m_state( &ME::Hi_In_None )
{
}

void Highlight_BufferEditor::Run_Range( const CrsPos   st
                                      , const unsigned fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_state = &ME::Hi_In_None;

  unsigned l=st.crsLine;
  unsigned p=st.crsChar;

  while( m_state && l<fn )
  {
    (this->*m_state)( l, p );
  }
}

void Highlight_BufferEditor::Hi_In_None( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( ; l<m_fb.NumLines(); l++ )
  {
    const Line&    lr = m_fb.GetLine( l );
    const unsigned LL = m_fb.LineLen( l );

    if( 0<LL )
    {
      const char c_end = m_fb.Get( l, LL-1 );
      const char* ls = lr.c_str(0);

      if( 0==strncmp( ls, EDIT_BUF_NAME, lr.len() )
       || 0==strncmp( ls, HELP_BUF_NAME, lr.len() )
       || 0==strncmp( ls, MSG__BUF_NAME, lr.len() )
       || 0==strncmp( ls, SHELL_BUF_NAME, lr.len() )
       || 0==strncmp( ls, COLON_BUF_NAME, lr.len() )
       || 0==strncmp( ls, SLASH_BUF_NAME, lr.len() ) )
      {
        for( int k=0; k<LL; k++ )
        {
          m_fb.SetSyntaxStyle( l, k, HI_DEFINE );
        }
      }
      else if( c_end == DIR_DELIM )
      {
        for( int k=0; k<LL; k++ )
        {
          const char C = m_fb.Get( l, k );
          if( C == DIR_DELIM )
            m_fb.SetSyntaxStyle( l, k, HI_CONST );
          else
            m_fb.SetSyntaxStyle( l, k, HI_CONTROL );
        }
      }
      else {
        for( int k=0; k<LL; k++ )
        {
          const char C = m_fb.Get( l, k );
          if( C == DIR_DELIM )
            m_fb.SetSyntaxStyle( l, k, HI_CONST );
        }
      }
    }
    p = 0;
  }
  m_state = 0;
}

