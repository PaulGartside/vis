////////////////////////////////////////////////////////////////////////////////
// VI-Simplified (vis) C++ Implementation                                     //
// Copyright (c) 13 Aug 2016 Paul J. Gartside                                 //
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
#include "Highlight_Make.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

Highlight_Make::Highlight_Make( FileBuf& rfb )
  : Highlight_Base( rfb )
  , m_state( &ME::Hi_In_None )
{
}

void Highlight_Make::Run_Range( const CrsPos st, const unsigned fn )
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

void Highlight_Make::Hi_In_None( unsigned& l, unsigned& p )
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
      else if( 0==strncmp( s, "`" , 1 ) ) { m_state = &ME::Hi_96_Quote; }
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

      else if( s[0]=='~' || s[0]=='@'
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
      else if( LL-1 == p && s[0]=='\\')  { m_fb.SetSyntaxStyle( l, p, HI_DEFINE ); }

      if( &ME::Hi_In_None != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_Make::Hi_In_Comment( unsigned& l, unsigned& p )
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

void Highlight_Make::Hi_SingleQuote( unsigned& l, unsigned& p )
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
      if( &ME::Hi_SingleQuote != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_Make::Hi_DoubleQuote( unsigned& l, unsigned& p )
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
      if( &ME::Hi_DoubleQuote != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_Make::Hi_96_Quote( unsigned& l, unsigned& p )
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

      if( (c1==0    && c0=='`')
       || (c1!='\\' && c0=='`')
       || (c1=='\\' && c0=='`' && slash_escaped) )
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
      if( &ME::Hi_96_Quote != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_Make::Hi_NumberBeg( unsigned& l, unsigned& p )
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

void Highlight_Make::Hi_NumberIn( unsigned& l, unsigned& p )
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

void Highlight_Make::Hi_NumberHex( unsigned& l, unsigned& p )
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

void Highlight_Make::Hi_NumberFraction( unsigned& l, unsigned& p )
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

void Highlight_Make::Hi_NumberExponent( unsigned& l, unsigned& p )
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
  { "if"                 , HI_CONTROL },
  { "fi"                 , HI_CONTROL },
  { "else"               , HI_CONTROL },
  { "elsif"              , HI_CONTROL },
  { "for"                , HI_CONTROL },
  { "done"               , HI_CONTROL },
  { "while"              , HI_CONTROL },
  { "do"                 , HI_CONTROL },
  { "return"             , HI_CONTROL },
  { "switch"             , HI_CONTROL },
  { "case"               , HI_CONTROL },
  { "break"              , HI_CONTROL },
  { "then"               , HI_CONTROL },
  { "bg"                 , HI_CONTROL },
  { "bind"               , HI_CONTROL },
  { "builtin"            , HI_CONTROL },
  { "caller"             , HI_CONTROL },
  { "cd"                 , HI_CONTROL },
  { "command"            , HI_CONTROL },
  { "compgen"            , HI_CONTROL },
  { "complete"           , HI_CONTROL },
  { "compopt"            , HI_CONTROL },
  { "continue"           , HI_CONTROL },
  { "echo"               , HI_CONTROL },
  { "enable"             , HI_CONTROL },
  { "eval"               , HI_CONTROL },
  { "exec"               , HI_CONTROL },
  { "exit"               , HI_CONTROL },
  { "export"             , HI_CONTROL },
  { "fc"                 , HI_CONTROL },
  { "fg"                 , HI_CONTROL },
  { "hash"               , HI_CONTROL },
  { "help"               , HI_CONTROL },
  { "history"            , HI_CONTROL },
  { "jobs"               , HI_CONTROL },
  { "kill"               , HI_CONTROL },
  { "logout"             , HI_CONTROL },
  { "popd"               , HI_CONTROL },
  { "printf"             , HI_CONTROL },
  { "pushd"              , HI_CONTROL },
  { "pwd"                , HI_CONTROL },
  { "return"             , HI_CONTROL },
  { "set"                , HI_CONTROL },
  { "shift"              , HI_CONTROL },
  { "shopt"              , HI_CONTROL },
  { "source"             , HI_CONTROL },
  { "suspend"            , HI_CONTROL },
  { "test"               , HI_CONTROL },
  { "times"              , HI_CONTROL },
  { "trap"               , HI_CONTROL },
  { "ulimit"             , HI_CONTROL },
  { "umask"              , HI_CONTROL },
  { "unalias"            , HI_CONTROL },
  { "unset"              , HI_CONTROL },
  { "wait"               , HI_CONTROL },
  { "PHONY"              , HI_CONTROL },
  { "INTERMEDIATE"       , HI_CONTROL },
  { "SECONDARY"          , HI_CONTROL },
  { "PRECIOUS"           , HI_CONTROL },
  { "DELETE_ON_ERROR"    , HI_CONTROL },
  { "EXPORT_ALL_VARIABLES",HI_CONTROL },

  { "declare"            , HI_VARTYPE },
  { "dirs"               , HI_VARTYPE },
  { "disown"             , HI_VARTYPE },
  { "getopts"            , HI_VARTYPE },
  { "let"                , HI_VARTYPE },
  { "local"              , HI_VARTYPE },
  { "mapfile"            , HI_VARTYPE },
  { "read"               , HI_VARTYPE },
  { "readonly"           , HI_VARTYPE },
  { "type"               , HI_VARTYPE },
  { "typeset"            , HI_VARTYPE },

  { "false"              , HI_CONST   },
  { "true"               , HI_CONST   },

  { "alias"              , HI_DEFINE  },
  { "include"            , HI_DEFINE  },
  { 0 }
};

void Highlight_Make::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Hi_FindKey_In_Range( HiPairs, st, fn );
}

