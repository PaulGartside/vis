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

#include <ctype.h>

#include "Utilities.hh"
#include "FileBuf.hh"
#include "Highlight_XML.hh"

static HiKeyVal HiPairs[] =
{
  // HTML tags:
  { "xml"     , HI_CONTROL },
  { "version" , HI_CONTROL },
  { "encoding", HI_CONTROL },
  { 0 }
};

Highlight_XML::Highlight_XML( FileBuf& rfb )
  : Highlight_Base( rfb )
  , m_state( &ME::Hi_In_None )
  , m_qtXSt( &ME::Hi_In_None )
{
}

void Highlight_XML::Run_Range( const CrsPos st, const unsigned fn )
{
  m_state = &ME::Hi_In_None;

  unsigned l=st.crsLine;
  unsigned p=st.crsChar;

  while( m_state && l<fn )
  {
    (this->*m_state)( l, p );
  }
  Find_Styles_Keys_In_Range( st, fn );
}

void Highlight_XML::Hi_In_None( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<m_fb.NumLines(); l++ )
  {
    const unsigned LL = m_fb.LineLen( l );

    for( ; p<LL; p++ )
    {
      m_fb.ClearSyntaxStyles( l, p );

      // c0 is ahead of c1 is ahead of c2: (c3,c2,c1,c0)
      const char c3 = (2<p) ? m_fb.Get( l, p-3 ) : 0;
      const char c2 = (1<p) ? m_fb.Get( l, p-2 ) : 0;
      const char c1 = (0<p) ? m_fb.Get( l, p-1 ) : 0;
      const char c0 =         m_fb.Get( l, p );

      if( c1=='<' && c0!='!' && c0!='/')
      {
        m_fb.SetSyntaxStyle( l, p-1, HI_DEFINE );
        m_state = &ME::Hi_OpenTag_ElemName;
      }
      else if( c1=='<' && c0=='/')
      {
        m_fb.SetSyntaxStyle( l, p-1, HI_DEFINE ); //< '<'
        m_fb.SetSyntaxStyle( l, p  , HI_DEFINE ); //< '/'
        p++; // Move past '/'
        m_state = &ME::Hi_OpenTag_ElemName;
      }
      else if( c3=='<' && c2=='!' && c1=='-' && c0=='-' )
      {
        m_fb.SetSyntaxStyle( l, p-3, HI_COMMENT ); //< '<'
        m_fb.SetSyntaxStyle( l, p-2, HI_COMMENT ); //< '!'
        m_fb.SetSyntaxStyle( l, p-1, HI_COMMENT ); //< '-'
        m_fb.SetSyntaxStyle( l, p  , HI_COMMENT ); //< '-'
        p++; // Move past '-'
        m_state = &ME::Hi_Comment;
      }
    //else if( c0=='\'')
    //{
    //  m_fb.SetSyntaxStyle( l, p, HI_CONST );
    //  p++; // Move past '\''
    //  m_state = &ME::Hi_In_SingleQuote;
    //  m_qtXSt = &ME::Hi_In_None;
    //}
    //else if( c0=='\"')
    //{
    //  m_fb.SetSyntaxStyle( l, p, HI_CONST );
    //  p++; // Move past '\"'
    //  m_state = &ME::Hi_In_DoubleQuote;
    //  m_qtXSt = &ME::Hi_In_None;
    //}
      else if( !IsIdent( c1 ) && isdigit( c0 ) )
      {
        m_state = &ME::Hi_NumberBeg;
      }
      else {
        ; //< No syntax highlighting on content outside of <>tags
      }

      if( &ME::Hi_In_None != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_XML::Hi_OpenTag_ElemName( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found_elem_name = false;

  for( ; l<m_fb.NumLines(); l++ )
  {
    const int LL = m_fb.LineLen( l );

    for( ; p<LL; p++ )
    {
      const char c0 = m_fb.Get( l, p );

      if( c0=='>' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
        p++; // Move past '>'
        m_state = &ME::Hi_In_None;
      }
      else if( c0=='/' || c0=='?' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
      }
      else if( !found_elem_name )
      {
        if( IsXML_Ident( c0 ) )
        {
          found_elem_name = true;
          m_fb.SetSyntaxStyle( l, p, HI_CONTROL );
        }
        else if( c0==' ' || c0=='\t' )
        {
          m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
        }
        else {
          m_fb.SetSyntaxStyle( l, p, HI_NONASCII );
        }
      }
      else if( found_elem_name )
      {
        if( IsXML_Ident( c0 ) )
        {
          m_fb.SetSyntaxStyle( l, p, HI_CONTROL );
        }
        else if( c0==' ' || c0=='\t' )
        {
          m_fb.SetSyntaxStyle( l, p, HI_CONTROL );
          p++; //< Move past white space
          m_state = &ME::Hi_OpenTag_AttrName;
        }
        else {
          m_fb.SetSyntaxStyle( l, p, HI_NONASCII );
        }
      }
      else {
        m_fb.SetSyntaxStyle( l, p, HI_COMMENT );
      }
      if( &ME::Hi_OpenTag_ElemName != m_state ) return;
    }
    p = 0;
  }
}

void Highlight_XML::Hi_OpenTag_AttrName( unsigned& l, unsigned& p )
{
  bool found_attr_name = false;
  bool past__attr_name = false;

  for( ; l<m_fb.NumLines(); l++ )
  {
    const int LL = m_fb.LineLen( l );

    for( ; p<LL; p++ )
    {
      // c0 is ahead of c1 is ahead of c2: (c2,c1,c0)
      const char c0 = m_fb.Get( l, p );

      if( c0=='>' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
        p++; // Move past '>'
        m_state = &ME::Hi_In_None;
      }
      else if( c0=='/' || c0=='?' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
      }
      else if( !found_attr_name )
      {
        if( IsXML_Ident( c0 ) )
        {
          found_attr_name = true;
          m_fb.SetSyntaxStyle( l, p, HI_VARTYPE );
        }
        else if( c0==' ' || c0=='\t' )
        {
          m_fb.SetSyntaxStyle( l, p, HI_CONTROL );
        }
        else {
          m_fb.SetSyntaxStyle( l, p, HI_NONASCII );
        }
      }
      else if( found_attr_name && !past__attr_name )
      {
        if( IsXML_Ident( c0 ) )
        {
          m_fb.SetSyntaxStyle( l, p, HI_VARTYPE );
        }
        else if( c0==' ' || c0=='\t' )
        {
          past__attr_name = true;
          m_fb.SetSyntaxStyle( l, p, HI_CONTROL );
        }
        else if( c0=='=' )
        {
          past__attr_name = true;
          m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
          p++; //< Move past '='
          m_state = &ME::Hi_OpenTag_AttrVal;
        }
        else {
          m_fb.SetSyntaxStyle( l, p, HI_NONASCII );
        }
      }
      else if( found_attr_name && past__attr_name )
      {
        if( c0=='=' )
        {
          m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
          p++; //< Move past '='
          m_state = &ME::Hi_OpenTag_AttrVal;
        }
        else if( c0==' ' || c0=='\t' )
        {
          m_fb.SetSyntaxStyle( l, p, HI_VARTYPE );
        }
        else {
          m_fb.SetSyntaxStyle( l, p, HI_NONASCII );
        }
      }
      else {
        m_fb.SetSyntaxStyle( l, p, HI_COMMENT );
      }
      if( &ME::Hi_OpenTag_AttrName != m_state ) return;
    }
    p = 0;
  }
}

void Highlight_XML::Hi_OpenTag_AttrVal( unsigned& l, unsigned& p )
{
  for( ; l<m_fb.NumLines(); l++ )
  {
    const int LL = m_fb.LineLen( l );

    for( ; p<LL; p++ )
    {
      // c0 is ahead of c1 is ahead of c2: (c2,c1,c0)
      const char c0 = m_fb.Get( l, p );

      if( c0=='>' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
        p++; // Move past '>'
        m_state = &ME::Hi_In_None;
      }
      else if( c0=='/' || c0=='?' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
      }
      else if( c0=='\'' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_CONST );
        p++; // Move past '\''
        m_state = &ME::Hi_In_SingleQuote;
        m_qtXSt = &ME::Hi_OpenTag_AttrName;
      }
      else if( c0=='\"' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_CONST );
        p++; //< Move past '\"'
        m_state = &ME::Hi_In_DoubleQuote;
        m_qtXSt = &ME::Hi_OpenTag_AttrName;
      }
      else if( c0==' ' || c0=='\t' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
      }
      else {
        m_fb.SetSyntaxStyle( l, p, HI_NONASCII );
      }
      if( &ME::Hi_OpenTag_AttrVal != m_state ) return;
    }
    p = 0;
  }
}

void Highlight_XML::Hi_CloseTag( unsigned& l, unsigned& p )
{
  for( ; l<m_fb.NumLines(); l++ )
  {
    const int LL = m_fb.LineLen( l );

    for( ; p<LL; p++ )
    {
      // c0 is ahead of c1 is ahead of c2: (c2,c1,c0)
      const char c0 = m_fb.Get( l, p );

      if( c0=='>' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
        p++; // Move past '>'
        m_state = &ME::Hi_In_None;
      }
      else if( c0=='/' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
      }
      else if( IsXML_Ident( c0 ) )
      {
        m_fb.SetSyntaxStyle( l, p, HI_CONTROL );
      }
      else if( c0==' ' || c0=='\t' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
      }
      else {
        m_fb.SetSyntaxStyle( l, p, HI_NONASCII );
      }
      if( &ME::Hi_CloseTag != m_state ) return;
    }
    p = 0;
  }
}

void Highlight_XML::Hi_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<m_fb.NumLines(); l++ )
  {
    const unsigned LL = m_fb.LineLen( l );

    for( ; p<LL; p++ )
    {
      const char c1 = (1<p) ? m_fb.Get( l, p-2 ) : 0;
      const char c2 = (0<p) ? m_fb.Get( l, p-1 ) : 0;
      const char c3 =         m_fb.Get( l, p   );

      if( c1=='-' && c2=='-' && c3=='>' )
      {
        m_fb.SetSyntaxStyle( l, p-2, HI_COMMENT ); //< '-'
        m_fb.SetSyntaxStyle( l, p-1, HI_COMMENT ); //< '-'
        m_fb.SetSyntaxStyle( l, p  , HI_COMMENT ); //< '>'
        p++; // Move past '>'
        m_state = &ME::Hi_In_None;
      }
      else m_fb.SetSyntaxStyle( l, p, HI_COMMENT );

      if( &ME::Hi_Comment != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_XML::Hi_In_SingleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<m_fb.NumLines(); l++ )
  {
    const unsigned LL = m_fb.LineLen( l );

    bool slash_escaped = false;
    for( ; p<LL; p++ )
    {
      // c0 is ahead of c1: (c1,c0)
      const char c1 = p ? m_fb.Get( l, p-1 ) : 0;
      const char c0 =     m_fb.Get( l, p );

      if( (c1==0    && c0=='\'')
       || (c1!='\\' && c0=='\'')
       || (c1=='\\' && c0=='\'' && slash_escaped) )
      {
        m_fb.SetSyntaxStyle( l, p, HI_CONST );
        p++; // Move past '\''
        m_state = m_qtXSt;
      }
      else {
        if( c1=='\\' && c0=='\\' ) slash_escaped = !slash_escaped;
        else                       slash_escaped = false;

        m_fb.SetSyntaxStyle( l, p, HI_CONST );
      }
      if( &ME::Hi_In_SingleQuote != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_XML::Hi_In_DoubleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<m_fb.NumLines(); l++ )
  {
    const unsigned LL = m_fb.LineLen( l );

    bool slash_escaped = false;
    for( ; p<LL; p++ )
    {
      // c0 is ahead of c1: (c1,c0)
      const char c1 = p ? m_fb.Get( l, p-1 ) : 0;
      const char c0 =     m_fb.Get( l, p );

      if( (c1==0    && c0=='\"')
       || (c1!='\\' && c0=='\"')
       || (c1=='\\' && c0=='\"' && slash_escaped) )
      {
        m_fb.SetSyntaxStyle( l, p, HI_CONST );
        p++; //< Move past '\"'
        m_state = m_qtXSt;
      }
      else {
        if( c1=='\\' && c0=='\\' ) slash_escaped = !slash_escaped;
        else                       slash_escaped = false;

        m_fb.SetSyntaxStyle( l, p, HI_CONST );
      }
      if( &ME::Hi_In_DoubleQuote != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_XML::Hi_NumberBeg( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m_fb.SetSyntaxStyle( l, p, HI_CONST );

  const char c1 = m_fb.Get( l, p );
  p++;
  m_state = &ME::Hi_NumberIn;

  const unsigned LL = m_fb.LineLen( l );
  if( '0' == c1 && (p+1)<LL )
  {
    const char c2 = m_fb.Get( l, p );
    if( 'x' == c2 ) {
      m_fb.SetSyntaxStyle( l, p, HI_CONST );
      m_state = &ME::Hi_NumberHex;
      p++;
    }
  }
}

void Highlight_XML::Hi_NumberIn( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = m_fb.LineLen( l );
  if( LL <= p ) m_state = &ME::Hi_In_None;
  else {
    const char c1 = m_fb.Get( l, p );

    if( '.'==c1 )
    {
      m_fb.SetSyntaxStyle( l, p, HI_CONST );
      m_state = &ME::Hi_NumberFraction;
      p++;
    }
    else if( 'e'==c1 || 'E'==c1 )
    {
      m_fb.SetSyntaxStyle( l, p, HI_CONST );
      m_state = &ME::Hi_NumberExponent;
      p++;
      if( p<LL )
      {
        const char c2 = m_fb.Get( l, p );
        if( '+' == c2 || '-' == c2 ) {
          m_fb.SetSyntaxStyle( l, p, HI_CONST );
          p++;
        }
      }
    }
    else if( isdigit(c1) )
    {
      m_fb.SetSyntaxStyle( l, p, HI_CONST );
      p++;
    }
    else {
      m_state = &ME::Hi_In_None;
    }
  }
}

void Highlight_XML::Hi_NumberHex( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = m_fb.LineLen( l );
  if( LL <= p ) m_state = &ME::Hi_In_None;
  else {
    const char c1 = m_fb.Get( l, p );
    if( isxdigit(c1) )
    {
      m_fb.SetSyntaxStyle( l, p, HI_CONST );
      p++;
    }
    else {
      m_state = &ME::Hi_In_None;
    }
  }
}

void Highlight_XML::Hi_NumberFraction( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = m_fb.LineLen( l );
  if( LL <= p ) m_state = &ME::Hi_In_None;
  else {
    const char c1 = m_fb.Get( l, p );
    if( isdigit(c1) )
    {
      m_fb.SetSyntaxStyle( l, p, HI_CONST );
      p++;
    }
    else if( 'e'==c1 || 'E'==c1 )
    {
      m_fb.SetSyntaxStyle( l, p, HI_CONST );
      m_state = &ME::Hi_NumberExponent;
      p++;
      if( p<LL )
      {
        const char c2 = m_fb.Get( l, p );
        if( '+' == c2 || '-' == c2 ) {
          m_fb.SetSyntaxStyle( l, p, HI_CONST );
          p++;
        }
      }
    }
    else {
      m_state = &ME::Hi_In_None;
    }
  }
}

void Highlight_XML::Hi_NumberExponent( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = m_fb.LineLen( l );
  if( LL <= p ) m_state = &ME::Hi_In_None;
  else {
    const char c1 = m_fb.Get( l, p );
    if( isdigit(c1) )
    {
      m_fb.SetSyntaxStyle( l, p, HI_CONST );
      p++;
    }
    else {
      m_state = &ME::Hi_In_None;
    }
  }
}

void Highlight_XML::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Hi_FindKey_In_Range( HiPairs, st, fn );
}

