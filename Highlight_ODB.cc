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
#include "Highlight_ODB.hh"

static HiKeyVal HiPairs[] =
{
  { "at"        , HI_CONTROL },
  { "cd"        , HI_CONTROL },
  { "ce"        , HI_CONTROL },
  { "delay"     , HI_CONTROL },
  { "echo"      , HI_CONTROL },
  { "fb"        , HI_CONTROL },
  { "flush"     , HI_CONTROL },
  { "fs"        , HI_CONTROL },
  { "fw"        , HI_CONTROL },
  { "h"         , HI_CONTROL },
  { "help"      , HI_CONTROL },
  { "ib"        , HI_CONTROL },
  { "info"      , HI_CONTROL },
  { "lr"        , HI_CONTROL },
  { "ls"        , HI_CONTROL },
  { "mkdir"     , HI_CONTROL },
  { "pwd"       , HI_CONTROL },
  { "quit"      , HI_CONTROL },
  { "refresh"   , HI_CONTROL },
  { "sb"        , HI_CONTROL },
  { "ss"        , HI_CONTROL },
  { "sw"        , HI_CONTROL },
  { "symlnk"    , HI_CONTROL },
  { "symlnkinfo", HI_CONTROL },
  { "v"         , HI_CONTROL },
  { "wait"      , HI_CONTROL },
  { "x"         , HI_CONTROL },
  { "inc"       , HI_DEFINE  },
  { 0 }
};

Highlight_ODB::Highlight_ODB( FileBuf& rfb )
  : Highlight_Base( rfb )
  , hi_state( &ME::Hi_In_None )
{
}

void Highlight_ODB::Run_Range( const CrsPos st, const unsigned fn )
{
  hi_state = &ME::Hi_In_None;
  unsigned l=st.crsLine;
  unsigned p=st.crsChar;

  while( hi_state && l<fn )
  {
    (this->*hi_state)( l, p );
  }
  Find_Styles_Keys_In_Range( st, fn );
}

void Highlight_ODB::Hi_In_None( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<fb.NumLines(); l++ )
  {
    const unsigned LL = fb.LineLen( l );

    for( ; p<LL; p++ )
    {
      fb.ClearSyntaxStyles( l, p );

      const char c1 = (1<p) ? fb.Get( l, p-2 ) : 0;
      const char c2 = (0<p) ? fb.Get( l, p-1 ) : 0;
      const char c3 =         fb.Get( l, p );

      if( (c1!='=' && c2=='=' && c3!='=')
       || (c1!='/' && c2=='/' && c3!='/') )
                               { fb.SetSyntaxStyle( l, p-1, HI_CONTROL ); }
      if( c2=='/' && c3=='/' ) { hi_state = &ME::Hi_BegCPP_Comment; p--; }
      else if( !IsIdent(c2)
            &&  isdigit(c3)  ) { hi_state = &ME::Hi_NumberBeg; }

      if( &ME::Hi_In_None != hi_state ) return;
    }
    p = 0;
  }
  hi_state = 0;
}

void Highlight_ODB::Hi_BegCPP_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_COMMENT );
  p++;
  hi_state = &ME::Hi_In_CPP_Comment;
}

void Highlight_ODB::Hi_In_CPP_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = fb.LineLen( l );

  for( ; p<LL; p++ )
  {
    fb.SetSyntaxStyle( l, p, HI_COMMENT );
  }
  p--;
  hi_state = &ME::Hi_EndCPP_Comment;
}

void Highlight_ODB::Hi_EndCPP_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_COMMENT );
  p=0; l++;
  hi_state = &ME::Hi_In_None;
}

void Highlight_ODB::Hi_NumberBeg( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_CONST );

  const char c1 = fb.Get( l, p );
  p++;
  hi_state = &ME::Hi_NumberIn;

  const unsigned LL = fb.LineLen( l );
  if( '0' == c1 && (p+1)<LL )
  {
    const char c2 = fb.Get( l, p );
    if( 'x' == c2 ) {
      fb.SetSyntaxStyle( l, p, HI_CONST );
      hi_state = &ME::Hi_NumberHex;
      p++;
    }
  }
}

void Highlight_ODB::Hi_NumberIn( unsigned& l, unsigned& p )
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
        const char c2 = fb.Get( l, p );
        if( '+' == c2 || '-' == c2 ) {
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

void Highlight_ODB::Hi_NumberHex( unsigned& l, unsigned& p )
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

void Highlight_ODB::Hi_NumberFraction( unsigned& l, unsigned& p )
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
        const char c2 = fb.Get( l, p );
        if( '+' == c2 || '-' == c2 ) {
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

void Highlight_ODB::Hi_NumberExponent( unsigned& l, unsigned& p )
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

void Highlight_ODB::Find_Styles_Keys()
{
  Hi_FindKey( HiPairs );
}

void Highlight_ODB::Find_Styles_Keys_In_Range( const CrsPos   st
                                             , const unsigned fn )
{
  Hi_FindKey_In_Range( HiPairs, st, fn );
}

