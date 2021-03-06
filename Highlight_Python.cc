////////////////////////////////////////////////////////////////////////////////
// VI-Simplified (vis) C++ Implementation                                     //
// Copyright (c) 04 Jan 2017 Paul J. Gartside                                 //
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
#include "Highlight_Python.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

Highlight_Python::Highlight_Python( FileBuf& rfb )
  : Highlight_Base( rfb )
  , m_state( &ME::Hi_In_None )
{
}

void Highlight_Python::Run_Range( const CrsPos st, const unsigned fn )
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

void Highlight_Python::Hi_In_None( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<m_fb.NumLines(); l++ )
  {
    const unsigned LL = m_fb.LineLen( l );
    const Line&    lr = m_fb.GetLine( l );

    for( ; p<LL; p++ )
    {
      m_fb.ClearSyntaxStyles( l, p );

      const char* s = lr.c_str( p );

      if     ( 0==strncmp( s, "#" , 1 ) ) { m_state = &ME::Hi_In_Comment; }
      else if( 0==strncmp( s, "\'", 1 ) ) { m_state = &ME::Hi_SingleQuote; }
      else if( 0==strncmp( s, "\"", 1 ) ) { m_state = &ME::Hi_DoubleQuote; }
      else if( 0<p && !IsIdent((s-1)[0])
                   &&  isdigit( s   [0]) ){ m_state = &ME::Hi_NumberBeg; }

      else if( p<LL-1
            && (0==strncmp( s, "::", 2 )
             || 0==strncmp( s, "->", 2 )) ) { m_fb.SetSyntaxStyle( l, p++, HI_VARTYPE );
                                              m_fb.SetSyntaxStyle( l, p  , HI_VARTYPE ); }
      else if( p<LL-1
            &&( 0==strncmp( s, "==", 2 )
             || 0==strncmp( s, "&&", 2 )
             || 0==strncmp( s, "||", 2 )
             || 0==strncmp( s, "|=", 2 )
             || 0==strncmp( s, "&=", 2 )
             || 0==strncmp( s, "!=", 2 )
             || 0==strncmp( s, "+=", 2 )
             || 0==strncmp( s, "-=", 2 )) ) { m_fb.SetSyntaxStyle( l, p++, HI_CONTROL );
                                              m_fb.SetSyntaxStyle( l, p  , HI_CONTROL ); }

      else if( s[0]=='$' ) { m_fb.SetSyntaxStyle( l, p, HI_DEFINE ); }
      else if( s[0]=='&'
            || s[0]=='.' || s[0]=='*'
            || s[0]=='[' || s[0]==']' ) { m_fb.SetSyntaxStyle( l, p, HI_VARTYPE ); }

      else if( s[0]=='~'
            || s[0]=='=' || s[0]=='^'
            || s[0]==':' || s[0]=='%'
            || s[0]=='+' || s[0]=='-'
            || s[0]=='<' || s[0]=='>'
            || s[0]=='!' || s[0]=='?'
            || s[0]=='(' || s[0]==')'
            || s[0]=='{' || s[0]=='}'
            || s[0]==',' || s[0]==';'
            || s[0]=='/' || s[0]=='|' ) { m_fb.SetSyntaxStyle( l, p, HI_CONTROL ); }

      else if( s[0] < 32 || 126 < s[0] ) { m_fb.SetSyntaxStyle( l, p, HI_NONASCII ); }

      if( &ME::Hi_In_None != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_Python::Hi_In_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = m_fb.LineLen( l );

  for( ; p<LL; p++ )
  {
    m_fb.SetSyntaxStyle( l, p, HI_COMMENT );
  }
  p=0; l++;
  m_state = &ME::Hi_In_None;
}

void Highlight_Python::Hi_SingleQuote( unsigned& l, unsigned& p )
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
      const char c0 =     m_fb.Get( l, p   );

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
      if( &ME::Hi_SingleQuote != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_Python::Hi_DoubleQuote( unsigned& l, unsigned& p )
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
      const char c0 =     m_fb.Get( l, p   );

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
      if( &ME::Hi_DoubleQuote != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_Python::Hi_NumberBeg( unsigned& l, unsigned& p )
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
    if( 'x' == c0 ) {
      m_fb.SetSyntaxStyle( l, p, HI_CONST );
      m_state = &ME::Hi_NumberHex;
      p++;
    }
  }
}

void Highlight_Python::Hi_NumberIn( unsigned& l, unsigned& p )
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
    else {
      m_state = &ME::Hi_In_None;
    }
  }
}

void Highlight_Python::Hi_NumberHex( unsigned& l, unsigned& p )
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

void Highlight_Python::Hi_NumberFraction( unsigned& l, unsigned& p )
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

void Highlight_Python::Hi_NumberExponent( unsigned& l, unsigned& p )
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

static HiKeyVal HiPairs[] =
{
  { "and"         , HI_CONTROL },
  { "break"       , HI_CONTROL },
  { "continue"    , HI_CONTROL },
  { "elif"        , HI_CONTROL },
  { "else"        , HI_CONTROL },
  { "except"      , HI_CONTROL },
  { "finally"     , HI_CONTROL },
  { "for"         , HI_CONTROL },
  { "if"          , HI_CONTROL },
  { "in"          , HI_CONTROL },
  { "is"          , HI_CONTROL },
  { "not"         , HI_CONTROL },
  { "or"          , HI_CONTROL },
  { "pass"        , HI_CONTROL },
  { "raise"       , HI_CONTROL },
  { "return"      , HI_CONTROL },
  { "try"         , HI_CONTROL },
  { "while"       , HI_CONTROL },

  { "as"          , HI_VARTYPE },
  { "class"       , HI_VARTYPE },
  { "def"         , HI_VARTYPE },
  { "del"         , HI_VARTYPE },
  { "global"      , HI_VARTYPE },
  { "int"         , HI_VARTYPE },
  { "with"        , HI_VARTYPE },

  // Built in functions:
  { "abs"         , HI_VARTYPE },
  { "all"         , HI_VARTYPE },
  { "any"         , HI_VARTYPE },
  { "ascii"       , HI_VARTYPE },
  { "bin"         , HI_VARTYPE },
  { "bool"        , HI_VARTYPE },
  { "bytearrary"  , HI_VARTYPE },
  { "bytes"       , HI_VARTYPE },
  { "callable"    , HI_VARTYPE },
  { "chr"         , HI_VARTYPE },
  { "classmethod" , HI_VARTYPE },
  { "compile"     , HI_VARTYPE },
  { "complex"     , HI_VARTYPE },
  { "delattr"     , HI_VARTYPE },
  { "dict"        , HI_VARTYPE },
  { "dir"         , HI_VARTYPE },
  { "divmod"      , HI_VARTYPE },
  { "enumerate"   , HI_VARTYPE },
  { "eval"        , HI_VARTYPE },
  { "exec"        , HI_VARTYPE },
  { "filter"      , HI_VARTYPE },
  { "float"       , HI_VARTYPE },
  { "format"      , HI_VARTYPE },
  { "frozenset"   , HI_VARTYPE },
  { "getattr"     , HI_VARTYPE },
  { "globals"     , HI_VARTYPE },
  { "hasattr"     , HI_VARTYPE },
  { "hash"        , HI_VARTYPE },
  { "help"        , HI_VARTYPE },
  { "hex"         , HI_VARTYPE },
  { "id"          , HI_VARTYPE },
  { "input"       , HI_VARTYPE },
  { "int"         , HI_VARTYPE },
  { "isinstance"  , HI_VARTYPE },
  { "issubclass"  , HI_VARTYPE },
  { "iter"        , HI_VARTYPE },
  { "len"         , HI_VARTYPE },
  { "list"        , HI_VARTYPE },
  { "locals"      , HI_VARTYPE },
  { "map"         , HI_VARTYPE },
  { "max"         , HI_VARTYPE },
  { "memoryview"  , HI_VARTYPE },
  { "min"         , HI_VARTYPE },
  { "next"        , HI_VARTYPE },
  { "object"      , HI_VARTYPE },
  { "oct"         , HI_VARTYPE },
  { "open"        , HI_VARTYPE },
  { "ord"         , HI_VARTYPE },
  { "pow"         , HI_VARTYPE },
  { "print"       , HI_VARTYPE },
  { "property"    , HI_VARTYPE },
  { "range"       , HI_VARTYPE },
  { "repr"        , HI_VARTYPE },
  { "reversed"    , HI_VARTYPE },
  { "round"       , HI_VARTYPE },
  { "set"         , HI_VARTYPE },
  { "setattr"     , HI_VARTYPE },
  { "slice"       , HI_VARTYPE },
  { "sorted"      , HI_VARTYPE },
  { "staticmethod", HI_VARTYPE },
  { "str"         , HI_VARTYPE },
  { "sum"         , HI_VARTYPE },
  { "super"       , HI_VARTYPE },
  { "tuple"       , HI_VARTYPE },
  { "type"        , HI_VARTYPE },
  { "vars"        , HI_VARTYPE },
  { "zip"         , HI_VARTYPE },

  { "assert"      , HI_DEFINE  },
  { "from"        , HI_DEFINE  },
  { "import"      , HI_DEFINE  },
  { "__import__"  , HI_DEFINE  },
  { "__name__"    , HI_DEFINE  },

  { "False"       , HI_CONST   },
  { "None"        , HI_CONST   },
  { "True"        , HI_CONST   },
  { 0 }
};

void Highlight_Python::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Hi_FindKey_In_Range( HiPairs, st, fn );
}

