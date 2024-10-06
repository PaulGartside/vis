////////////////////////////////////////////////////////////////////////////////
// VI-Simplified (vis) C++ Implementation                                     //
// Copyright (c) 02 Jul 2024 Paul J. Gartside                                 //
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
#include <string.h>
#include <strings.h>

#include "Utilities.hh"
#include "FileBuf.hh"
#include "MemLog.hh"
#include "Highlight_Go.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

Highlight_Go::Highlight_Go( FileBuf& rfb )
  : Highlight_Base( rfb )
  , m_state( &ME::Hi_In_None )
{
}

void Highlight_Go::Run_Range( const CrsPos st, const unsigned fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_state = &ME::Hi_In_None;

  unsigned l=st.crsLine;
  unsigned p=st.crsChar;

  while( m_state && l<fn )
  {
    (this->*m_state)( l, p );
  }
  Find_Styles_Keys_In_Range( st, fn );
}

static bool Quote_Start( const char qt
                       , const char c2
                       , const char c1
                       , const char c0 )
{
  return (c1==0    && c0==qt ) //< Quote at beginning of line
      || (c1!='\\' && c0==qt ) //< Non-escaped quote
      || (c2=='\\' && c1=='\\' && c0==qt ); //< Escaped escape before quote
}
static bool OneVarType( const char c0 )
{
  return (c0=='&')
      || (c0=='.' || c0=='*')
      || (c0=='[' || c0==']');
}
static bool OneControl( const char c0 )
{
  return c0=='=' || c0=='^' || c0=='~'
      || c0==':' || c0=='%'
      || c0=='+' || c0=='-'
      || c0=='<' || c0=='>'
      || c0=='!' || c0=='?'
      || c0=='(' || c0==')'
      || c0=='{' || c0=='}'
      || c0==',' || c0==';'
      || c0=='/' || c0=='|';
}
static bool TwoControl( const char c1, const char c0 )
{
  return (c1=='=' && c0=='=')
      || (c1=='&' && c0=='&')
      || (c1=='|' && c0=='|')
      || (c1=='|' && c0=='=')
      || (c1=='&' && c0=='=')
      || (c1=='!' && c0=='=')
      || (c1=='+' && c0=='=')
      || (c1=='-' && c0=='=');
}

void Highlight_Go::Hi_In_None( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<m_fb.NumLines(); l++ )
  {
    const unsigned LL = m_fb.LineLen( l );

    for( ; p<LL; p++ )
    {
      m_fb.ClearSyntaxStyles( l, p );

      // c0 is ahead of c1 is ahead of c2: (c2,c1,c0)
      const char c2 = (1<p) ? m_fb.Get( l, p-2 ) : 0;
      const char c1 = (0<p) ? m_fb.Get( l, p-1 ) : 0;
      const char c0 =         m_fb.Get( l, p );

      if     ( c1=='/' && c0 == '/' ) { p--; m_state = &ME::Hi_BegCPP_Comment; }
      else if( c1=='/' && c0 == '*' ) { p--; m_state = &ME::Hi_BegC_Comment; }
      else if(            c0 == '#' ) { m_state = &ME::Hi_In_Define; }
      else if( Quote_Start('\'',c2,c1,c0) ) { m_state = &ME::Hi_In_SingleQuote; }
      else if( Quote_Start('\"',c2,c1,c0) ) { m_state = &ME::Hi_In_DoubleQuote; }
      else if( Quote_Start('`' ,c2,c1,c0) ) { m_state = &ME::Hi_In_Back__Quote; }
      else if( !IsIdent( c1 )
             && isdigit(c0)){ m_state = &ME::Hi_NumberBeg; }

      else if( (c1==':' && c0==':')
            || (c1=='-' && c0=='>') )
      {
        m_fb.SetSyntaxStyle( l, p-1, HI_VARTYPE );
        m_fb.SetSyntaxStyle( l, p  , HI_VARTYPE );
      }
      else if( TwoControl( c1, c0 ) )
      {
        m_fb.SetSyntaxStyle( l, p-1, HI_CONTROL );
        m_fb.SetSyntaxStyle( l, p  , HI_CONTROL );
      }
      else if( OneVarType( c0 ) )
      {
        m_fb.SetSyntaxStyle( l, p, HI_VARTYPE );
      }
      else if( OneControl( c0 ) )
      {
        m_fb.SetSyntaxStyle( l, p, HI_CONTROL );
      }
      else if( c0 < 32 || 126 < c0 )
      {
        m_fb.SetSyntaxStyle( l, p, HI_NONASCII );
      }
      if( &ME::Hi_In_None != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_Go::Hi_In_Define( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = m_fb.LineLen( l );

  char ce = 0; // character at end of line
  for( ; p<LL; p++ )
  {
    // c0 is ahead of c1: (c1,c0)
    const char c1 = p ? m_fb.Get( l, p-1 ) : 0;
    const char c0 =     m_fb.Get( l, p );

    if( c1=='/' && c0=='/' )
    {
      m_state = &ME::Hi_BegCPP_Comment;
      p--;
    }
    else if( c1=='/' && c0=='*' )
    {
      m_state = &ME::Hi_BegC_Comment;
      p--;
    }
    else {
      m_fb.SetSyntaxStyle( l, p, HI_DEFINE );
    }
    if( &ME::Hi_In_Define != m_state ) return;
    ce = c0;
  }
  p=0; l++;

  if( ce != '\\' )
  {
    m_state = &ME::Hi_In_None;
  }
}

void Highlight_Go::Hi_BegC_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m_fb.SetSyntaxStyle( l, p, HI_COMMENT );
  p++;
  m_state = &ME::Hi_In_C_Comment;
}

void Highlight_Go::Hi_In_C_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<m_fb.NumLines(); l++ )
  {
    const unsigned LL = m_fb.LineLen( l );

    for( ; p<LL; p++ )
    {
      // c0 is ahead of c1: (c1,c0)
      const char c1 = p ? m_fb.Get( l, p-1 ) : 0;
      const char c0 =     m_fb.Get( l, p );

      if( c1=='*' && c0=='/' )
      {
        m_state = &ME::Hi_EndC_Comment;
      }
      else m_fb.SetSyntaxStyle( l, p, HI_COMMENT );

      if( &ME::Hi_In_C_Comment != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_Go::Hi_EndC_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m_fb.SetSyntaxStyle( l, p, HI_COMMENT );
  p++;
  m_state = &ME::Hi_In_None;
}

void Highlight_Go::Hi_BegCPP_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m_fb.SetSyntaxStyle( l, p, HI_COMMENT );
  p++;
  m_state = &ME::Hi_In_CPP_Comment;
}

void Highlight_Go::Hi_In_CPP_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = m_fb.LineLen( l );

  for( ; p<LL; p++ )
  {
    m_fb.SetSyntaxStyle( l, p, HI_COMMENT );
  }
  p--;
  m_state = &ME::Hi_EndCPP_Comment;
}

void Highlight_Go::Hi_EndCPP_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m_fb.SetSyntaxStyle( l, p, HI_COMMENT );
  p=0; l++;
  m_state = &ME::Hi_In_None;
}

void Highlight_Go::Hi_In_SingleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m_fb.SetSyntaxStyle( l, p, HI_CONST );
  p++;
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
        p++;
        m_state = &ME::Hi_In_None;
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

void Highlight_Go::Hi_In_DoubleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m_fb.SetSyntaxStyle( l, p, HI_CONST );
  p++;
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
        p++;
        m_state = &ME::Hi_In_None;
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

void Highlight_Go::Hi_In_Back__Quote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m_fb.SetSyntaxStyle( l, p, HI_CONST );
  p++;
  for( ; l<m_fb.NumLines(); l++ )
  {
    const unsigned LL = m_fb.LineLen( l );

    for( ; p<LL; p++ )
    {
      // c0 is ahead of c1: (c1,c0)
      const char c0 = m_fb.Get( l, p );

      if( c0=='`' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_CONST );
        p++;
        m_state = &ME::Hi_In_None;
      }
      else {
        m_fb.SetSyntaxStyle( l, p, HI_CONST );
      }
      if( &ME::Hi_In_Back__Quote != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_Go::Hi_NumberBeg( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m_fb.SetSyntaxStyle( l, p, HI_CONST );

  const char c1 = m_fb.Get( l, p );
  p++;
  m_state = &ME::Hi_NumberIn;

  const unsigned LL = m_fb.LineLen( l );

  if( '0' == c1 && (p+1)<LL )
  {
    const char c0 = m_fb.Get( l, p );

    if( 'x' == c0 || 'X' == c0 )
    {
      m_fb.SetSyntaxStyle( l, p, HI_CONST );
      m_state = &ME::Hi_NumberHex;
      p++;
    }
  }
}

// Need to add highlighting for:
//   L = long
//   U = unsigned
//   UL = unsigned long
//   ULL = unsigned long long
//   F = float
// at the end of numbers
void Highlight_Go::Hi_NumberIn( unsigned& l, unsigned& p )
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
        const char c0 = m_fb.Get( l, p );
        if( '+' == c0 || '-' == c0 ) {
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
    else if( c1=='L' || c1=='F' || c1=='U' )
    {
      m_state = &ME::Hi_NumberTypeSpec;
    }
    else if( c1=='\'' && (p+1)<LL )
    {
      // ' is followed by another digit on line
      const char c0 = m_fb.Get( l, p+1 );

      if( isdigit( c0 ) )
      {
        m_fb.SetSyntaxStyle( l, p  , HI_CONST );
        m_fb.SetSyntaxStyle( l, p+1, HI_CONST );
        p += 2;
      }
      else {
        m_state = &ME::Hi_In_None;
      }
    }
    else {
      m_state = &ME::Hi_In_None;
    }
  }
}

void Highlight_Go::Hi_NumberHex( unsigned& l, unsigned& p )
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

void Highlight_Go::Hi_NumberFraction( unsigned& l, unsigned& p )
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
        const char c0 = m_fb.Get( l, p );
        if( '+' == c0 || '-' == c0 ) {
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

void Highlight_Go::Hi_NumberExponent( unsigned& l, unsigned& p )
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

void Highlight_Go::Hi_NumberTypeSpec( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned LL = m_fb.LineLen( l );

  if( p < LL )
  {
    const char c0 = m_fb.Get( l, p );

    if( c0=='L' )
    {
      m_fb.SetSyntaxStyle( l, p, HI_VARTYPE );
      p++;
      m_state = &ME::Hi_In_None;
    }
    else if( c0=='F' )
    {
      m_fb.SetSyntaxStyle( l, p, HI_VARTYPE );
      p++;
      m_state = &ME::Hi_In_None;
    }
    else if( c0=='U' )
    {
      m_fb.SetSyntaxStyle( l, p, HI_VARTYPE ); p++;
      if( p<LL ) {
        const char c1 = m_fb.Get( l, p );
        if( c1=='L' ) { // UL
          m_fb.SetSyntaxStyle( l, p, HI_VARTYPE ); p++;
          if( p<LL ) {
            const char c2 = m_fb.Get( l, p );
            if( c2=='L' ) { // ULL
              m_fb.SetSyntaxStyle( l, p, HI_VARTYPE ); p++;
            }
          }
        }
      }
      m_state = &ME::Hi_In_None;
    }
  }
}

static HiKeyVal HiPairs[] =
{
  // Keywords
  { "break"              , HI_CONTROL },
  { "case"               , HI_CONTROL },
  { "chan"               , HI_CONTROL },
  { "continue"           , HI_CONTROL },
  { "default"            , HI_CONTROL },
  { "defer"              , HI_CONTROL },
  { "else"               , HI_CONTROL },
  { "fallthrough"        , HI_CONTROL },
  { "for"                , HI_CONTROL },
  { "func"               , HI_CONTROL },
  { "if"                 , HI_CONTROL },
  { "go"                 , HI_CONTROL },
  { "goto"               , HI_CONTROL },
  { "range"              , HI_CONTROL },
  { "return"             , HI_CONTROL },
  { "select"             , HI_CONTROL },
  { "switch"             , HI_CONTROL },

  // Built in functions
  { "append"             , HI_CONTROL },
  { "cap"                , HI_CONTROL },
  { "close"              , HI_CONTROL },
  { "complex"            , HI_CONTROL },
  { "copy"               , HI_CONTROL },
  { "delete"             , HI_CONTROL },
  { "imag"               , HI_CONTROL },
  { "len"                , HI_CONTROL },
  { "make"               , HI_CONTROL },
  { "new"                , HI_CONTROL },
  { "panic"              , HI_CONTROL },
  { "real"               , HI_CONTROL },
  { "recover"            , HI_CONTROL },

  // Types
  { "bool"               , HI_VARTYPE },
  { "byte"               , HI_VARTYPE },
  { "complex128"         , HI_VARTYPE },
  { "complex64"          , HI_VARTYPE },
  { "const"              , HI_VARTYPE },
  { "error"              , HI_VARTYPE },
  { "float32"            , HI_VARTYPE },
  { "float64"            , HI_VARTYPE },
  { "int"                , HI_VARTYPE },
  { "int8"               , HI_VARTYPE },
  { "int16"              , HI_VARTYPE },
  { "int32"              , HI_VARTYPE },
  { "int64"              , HI_VARTYPE },
  { "interface"          , HI_VARTYPE },
  { "map"                , HI_VARTYPE },
  { "package"            , HI_VARTYPE },
  { "struct"             , HI_VARTYPE },
  { "type"               , HI_VARTYPE },
  { "var"                , HI_VARTYPE },
  { "rune"               , HI_VARTYPE },
  { "string"             , HI_VARTYPE },
  { "uint8"              , HI_VARTYPE },
  { "uint16"             , HI_VARTYPE },
  { "uint32"             , HI_VARTYPE },
  { "uint64"             , HI_VARTYPE },
  { "uintptr"            , HI_VARTYPE },

  // Constants
  { "false"              , HI_CONST   },
  { "iota"               , HI_CONST   },
  { "nil"                , HI_CONST   },
  { "true"               , HI_CONST   },

  { "import"             , HI_DEFINE  },
  { 0 }
};

void Highlight_Go::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Hi_FindKey_In_Range( HiPairs, st, fn );
}

