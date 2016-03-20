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
#include <string.h>
#include <strings.h>

#include "Utilities.hh"
#include "FileBuf.hh"
#include "MemLog.hh"
#include "Highlight_Bash.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

Highlight_Bash::Highlight_Bash( FileBuf& rfb )
  : Highlight_Base( rfb )
  , hi_state( &ME::Hi_In_None )
{
}

void Highlight_Bash::Run_Range( const CrsPos st, const unsigned fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  hi_state = &ME::Hi_In_None;

  unsigned l=st.crsLine;
  unsigned p=st.crsChar;

  while( hi_state && l<fn )
  {
    (this->*hi_state)( l, p );
  }
  Find_Styles_Keys_In_Range( st, fn );
}

void Highlight_Bash::Hi_In_None( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<fb.NumLines(); l++ )
  {
    const unsigned LL = fb.LineLen( l );
    const Line&    lr = fb.GetLine( l );

    for( ; p<LL; p++ )
    {
      fb.ClearSyntaxStyles( l, p );

      const char* s = lr.c_str( p );

      if     ( 0==strncmp( s, "#" , 1 ) ) { hi_state = &ME::Hi_In_Comment; }
      else if( 0==strncmp( s, "\'", 1 ) ) { hi_state = &ME::Hi_BegSingleQuote; }
      else if( 0==strncmp( s, "\"", 1 ) ) { hi_state = &ME::Hi_BegDoubleQuote; }
      else if( 0<p && !IsIdent((s-1)[0])
                   &&  isdigit( s   [0]) ){ hi_state = &ME::Hi_NumberBeg; }

      else if( p<LL-1
            && (0==strncmp( s, "::", 2 )
             || 0==strncmp( s, "->", 2 )) ) { fb.SetSyntaxStyle( l, p++, HI_VARTYPE );
                                              fb.SetSyntaxStyle( l, p  , HI_VARTYPE ); }
      else if( p<LL-1
            &&( 0==strncmp( s, "==", 2 )
             || 0==strncmp( s, "&&", 2 )
             || 0==strncmp( s, "||", 2 )
             || 0==strncmp( s, "|=", 2 )
             || 0==strncmp( s, "&=", 2 )
             || 0==strncmp( s, "!=", 2 )
             || 0==strncmp( s, "+=", 2 )
             || 0==strncmp( s, "-=", 2 )) ) { fb.SetSyntaxStyle( l, p++, HI_CONTROL );
                                              fb.SetSyntaxStyle( l, p  , HI_CONTROL ); }

      else if( s[0]=='$' ) { fb.SetSyntaxStyle( l, p, HI_DEFINE ); }
      else if( s[0]=='&'
            || s[0]=='.' || s[0]=='*'
            || s[0]=='[' || s[0]==']' ) { fb.SetSyntaxStyle( l, p, HI_VARTYPE ); }

      else if( s[0]=='~'
            || s[0]=='=' || s[0]=='^'
            || s[0]==':' || s[0]=='%'
            || s[0]=='+' || s[0]=='-'
            || s[0]=='<' || s[0]=='>'
            || s[0]=='!' || s[0]=='?'
            || s[0]=='(' || s[0]==')'
            || s[0]=='{' || s[0]=='}'
            || s[0]==',' || s[0]==';'
            || s[0]=='/' || s[0]=='|' ) { fb.SetSyntaxStyle( l, p, HI_CONTROL ); }

      else if( s[0] < 32 || 126 < s[0] ) { fb.SetSyntaxStyle( l, p, HI_NONASCII ); }

      if( &ME::Hi_In_None != hi_state ) return;
    }
    p = 0;
  }
  hi_state = 0;
}

void Highlight_Bash::Hi_In_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = fb.LineLen( l );

  for( ; p<LL; p++ )
  {
    fb.SetSyntaxStyle( l, p, HI_COMMENT );
  }
  p=0; l++;
  hi_state = &ME::Hi_In_None;
}

void Highlight_Bash::Hi_BegSingleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_CONST );
  p++;
  hi_state = &ME::Hi_In_SingleQuote;
}

void Highlight_Bash::Hi_In_SingleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<fb.NumLines(); l++ )
  {
    const unsigned LL = fb.LineLen( l );

    bool slash_escaped = false;
    for( ; p<LL; p++ )
    {
      // c0 is ahead of c1: c1,c0
      const char c1 = p ? fb.Get( l, p-1 ) : fb.Get( l, p );
      const char c0 = p ? fb.Get( l, p   ) : 0;

      if( (c1=='\'' && c0==0   )
       || (c1!='\\' && c0=='\'')
       || (c1=='\\' && c0=='\'' && slash_escaped) )
      {
        hi_state = &ME::Hi_EndSingleQuote;
      }
      else {
        if( c1=='\\' && c0=='\\' ) slash_escaped = true;
        else                       slash_escaped = false;

        fb.SetSyntaxStyle( l, p, HI_CONST );
      }
      if( &ME::Hi_In_SingleQuote != hi_state ) return;
    }
    p = 0;
  }
  hi_state = 0;
}

void Highlight_Bash::Hi_EndSingleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_CONST );
  p++; //p++;
  hi_state = &ME::Hi_In_None;
}

void Highlight_Bash::Hi_BegDoubleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_CONST );
  p++;
  hi_state = &ME::Hi_In_DoubleQuote;
}

void Highlight_Bash::Hi_In_DoubleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<fb.NumLines(); l++ )
  {
    const unsigned LL = fb.LineLen( l );

    bool slash_escaped = false;
    for( ; p<LL; p++ )
    {
      // c0 is ahead of c1: c1,c0
      const char c1 = p ? fb.Get( l, p-1 ) : fb.Get( l, p );
      const char c0 = p ? fb.Get( l, p   ) : 0;

      if( (c1=='\"' && c0==0   )
       || (c1!='\\' && c0=='\"')
       || (c1=='\\' && c0=='\"' && slash_escaped) )
      {
        hi_state = &ME::Hi_EndDoubleQuote;
      }
      else {
        if( c1=='\\' && c0=='\\' ) slash_escaped = true;
        else                       slash_escaped = false;

        fb.SetSyntaxStyle( l, p, HI_CONST );
      }
      if( &ME::Hi_In_DoubleQuote != hi_state ) return;
    }
    p = 0;
  }
  hi_state = 0;
}

void Highlight_Bash::Hi_EndDoubleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_CONST );
  p++; //p++;
  hi_state = &ME::Hi_In_None;
}

void Highlight_Bash::Hi_NumberBeg( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_CONST );

  const char c1 = fb.Get( l, p );
  p++;
  hi_state = &ME::Hi_NumberIn;

  const unsigned LL = fb.LineLen( l );
  if( '0' == c1 && (p+1)<LL )
  {
    const char c0 = fb.Get( l, p );
    if( 'x' == c0 ) {
      fb.SetSyntaxStyle( l, p, HI_CONST );
      hi_state = &ME::Hi_NumberHex;
      p++;
    }
  }
}

void Highlight_Bash::Hi_NumberIn( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = fb.LineLen( l );
  if( LL <= p ) hi_state = &ME::Hi_In_None;
  else {
    const char c1 = fb.Get( l, p );

    if( '.'==c1 )
    {
      fb.SetSyntaxStyle( l, p, HI_CONST );
      hi_state = &ME::Hi_NumberFraction;
      p++;
    }
    else if( 'e'==c1 || 'E'==c1 )
    {
      fb.SetSyntaxStyle( l, p, HI_CONST );
      hi_state = &ME::Hi_NumberExponent;
      p++;
      if( p<LL )
      {
        const char c0 = fb.Get( l, p );
        if( '+' == c0 || '-' == c0 ) {
          fb.SetSyntaxStyle( l, p, HI_CONST );
          p++;
        }
      }
    }
    else if( isdigit(c1) )
    {
      fb.SetSyntaxStyle( l, p, HI_CONST );
      p++;
    }
    else {
      hi_state = &ME::Hi_In_None;
    }
  }
}

void Highlight_Bash::Hi_NumberHex( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = fb.LineLen( l );
  if( LL <= p ) hi_state = &ME::Hi_In_None;
  else {
    const char c1 = fb.Get( l, p );
    if( isxdigit(c1) )
    {
      fb.SetSyntaxStyle( l, p, HI_CONST );
      p++;
    }
    else {
      hi_state = &ME::Hi_In_None;
    }
  }
}

void Highlight_Bash::Hi_NumberFraction( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = fb.LineLen( l );
  if( LL <= p ) hi_state = &ME::Hi_In_None;
  else {
    const char c1 = fb.Get( l, p );
    if( isdigit(c1) )
    {
      fb.SetSyntaxStyle( l, p, HI_CONST );
      p++;
    }
    else if( 'e'==c1 || 'E'==c1 )
    {
      fb.SetSyntaxStyle( l, p, HI_CONST );
      hi_state = &ME::Hi_NumberExponent;
      p++;
      if( p<LL )
      {
        const char c0 = fb.Get( l, p );
        if( '+' == c0 || '-' == c0 ) {
          fb.SetSyntaxStyle( l, p, HI_CONST );
          p++;
        }
      }
    }
    else {
      hi_state = &ME::Hi_In_None;
    }
  }
}

void Highlight_Bash::Hi_NumberExponent( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = fb.LineLen( l );
  if( LL <= p ) hi_state = &ME::Hi_In_None;
  else {
    const char c1 = fb.Get( l, p );
    if( isdigit(c1) )
    {
      fb.SetSyntaxStyle( l, p, HI_CONST );
      p++;
    }
    else {
      hi_state = &ME::Hi_In_None;
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

  { "alias"              , HI_DEFINE  },
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
  { "declare"            , HI_VARTYPE },
  { "dirs"               , HI_VARTYPE },
  { "disown"             , HI_VARTYPE },
  { "echo"               , HI_CONTROL },
  { "enable"             , HI_CONTROL },
  { "eval"               , HI_CONTROL },
  { "exec"               , HI_CONTROL },
  { "exit"               , HI_CONTROL },
  { "export"             , HI_CONTROL },
  { "false"              , HI_CONST   },
  { "fc"                 , HI_CONTROL },
  { "fg"                 , HI_CONTROL },
  { "getopts"            , HI_VARTYPE },
  { "hash"               , HI_CONTROL },
  { "help"               , HI_CONTROL },
  { "history"            , HI_CONTROL },
  { "jobs"               , HI_CONTROL },
  { "kill"               , HI_CONTROL },
  { "let"                , HI_VARTYPE },
  { "local"              , HI_VARTYPE },
  { "logout"             , HI_CONTROL },
  { "mapfile"            , HI_VARTYPE },
  { "popd"               , HI_CONTROL },
  { "printf"             , HI_CONTROL },
  { "pushd"              , HI_CONTROL },
  { "pwd"                , HI_CONTROL },
  { "read"               , HI_VARTYPE },
  { "readonly"           , HI_VARTYPE },
  { "return"             , HI_CONTROL },
  { "set"                , HI_CONTROL },
  { "shift"              , HI_CONTROL },
  { "shopt"              , HI_CONTROL },
  { "source"             , HI_CONTROL },
  { "suspend"            , HI_CONTROL },
  { "test"               , HI_CONTROL },
  { "times"              , HI_CONTROL },
  { "trap"               , HI_CONTROL },
  { "true"               , HI_CONST   },
  { "type"               , HI_VARTYPE },
  { "typeset"            , HI_VARTYPE },
  { "ulimit"             , HI_CONTROL },
  { "umask"              , HI_CONTROL },
  { "unalias"            , HI_CONTROL },
  { "unset"              , HI_CONTROL },
  { "wait"               , HI_CONTROL },
  { 0 }
};

void Highlight_Bash::Find_Styles_Keys()
{
  Trace trace( __PRETTY_FUNCTION__ );

  Hi_FindKey( HiPairs );
}

void Highlight_Bash::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Hi_FindKey_In_Range( HiPairs, st, fn );
}

