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

  // IC types
  { "Composite", HI_VARTYPE },
  { "Boolean"  , HI_VARTYPE },
  { "Double"   , HI_VARTYPE },
  { "Enum"     , HI_VARTYPE },
  { "File"     , HI_VARTYPE },
  { "Integer"  , HI_VARTYPE },
  { "String"   , HI_VARTYPE },
  { "True"     , HI_VARTYPE },
  { "False"    , HI_VARTYPE },
  { "Value"    , HI_VARTYPE },

  // IC stuff
  { "access"     , HI_CONTROL },
  { "default"    , HI_CONTROL },
  { "description", HI_CONTROL },
  { "DeviceMax"  , HI_CONTROL },
  { "filename"   , HI_CONTROL },
  { "frequency"  , HI_CONTROL },
  { "min"        , HI_CONTROL },
  { "max"        , HI_CONTROL },
  { "name"       , HI_CONTROL },
  { "PruneNode"  , HI_CONTROL },
  { "SystemTop"  , HI_CONTROL },

  // L3 IF Tester stuff
  { "snmp_tester"            , HI_CONTROL },
  { "targets"                , HI_CONTROL },
  { "agent"                  , HI_CONTROL },
  { "ip"                     , HI_CONTROL },
  { "port"                   , HI_CONTROL },
  { "community"              , HI_CONTROL },
  { "targets"                , HI_CONTROL },
  { "tests"                  , HI_CONTROL },
  { "get_test"               , HI_CONTROL },
  { "set_test"               , HI_CONTROL },
  { "oid"                    , HI_CONTROL },
  { "operation_delay"        , HI_CONTROL },
  { "validation"             , HI_CONTROL },
  { "type"                   , HI_CONTROL },
  { "value"                  , HI_CONTROL },
  { "validate_delay"         , HI_CONTROL },
  { "run_test_group"         , HI_CONTROL },
  { "exclude_list"           , HI_CONTROL },
  { "exclude"                , HI_CONTROL },
  { "user_name"              , HI_CONTROL },
  { "authentication_protocol", HI_CONTROL },
  { "authentication_password", HI_CONTROL },
  { "privacy_protocol"       , HI_CONTROL },
  { "privacy_password"       , HI_CONTROL },
  { "include"                , HI_CONTROL },
  { "test_grouping"          , HI_CONTROL },
  { "group name"             , HI_CONTROL },

  { 0 }
};

Highlight_XML::Highlight_XML( FileBuf& rfb )
  : Highlight_Base( rfb )
  , hi_state( &ME::Hi_In_None )
{
}

void Highlight_XML::Run()
{
  hi_state = &ME::Hi_In_None;
  unsigned l=0;
  unsigned p=0;

  while( hi_state ) (this->*hi_state)( l, p );

  Find_Styles_Keys();
}

void Highlight_XML::Run_Range( const CrsPos st, const unsigned fn )
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

void Highlight_XML::Hi_In_None( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<fb.NumLines(); l++ )
  {
    const unsigned LL = fb.LineLen( l );

    for( ; p<LL; p++ )
    {
      fb.ClearSyntaxStyles( l, p );

      const char c1 = (2<p) ? fb.Get( l, p-3 ) : 0;
      const char c2 = (1<p) ? fb.Get( l, p-2 ) : 0;
      const char c3 = (0<p) ? fb.Get( l, p-1 ) : 0;
      const char c4 =         fb.Get( l, p );

      // This if block is extra:
      if( (c2!='=' && c3=='=' && c4!='=') ) { fb.SetSyntaxStyle( l, p-1, HI_DEFINE ); }
      else if(        c3=='/' && c4=='>'  ) { fb.SetSyntaxStyle( l, p-1, HI_DEFINE );
                                              fb.SetSyntaxStyle( l, p  , HI_DEFINE ); }
      else if(        c3=='<' && c4=='/'  ) { fb.SetSyntaxStyle( l, p-1, HI_DEFINE );
                                              fb.SetSyntaxStyle( l, p  , HI_DEFINE ); }
      else if(                   c4=='<'
                              || c4=='>'  ) { fb.SetSyntaxStyle( l, p  , HI_DEFINE ); }

      if     ( c1=='<' && c2=='!'
            && c3=='-' && c4=='-' ) { hi_state = &ME::Hi_Beg_Comment; p-=3; }
      else if(            c4=='\'') { hi_state = &ME::Hi_BegSingleQuote; }
      else if(            c4=='\"') { hi_state = &ME::Hi_BegDoubleQuote; }
      else if( !IsIdent(c3) && isdigit(c4) )
                                    { hi_state = &ME::Hi_NumberBeg; }

      if( &ME::Hi_In_None != hi_state ) return;
    }
    p = 0;
  }
  hi_state = 0;
}

void Highlight_XML::Hi_Beg_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_COMMENT );
  p++;
  hi_state = &ME::Hi_In__Comment;
}

void Highlight_XML::Hi_In__Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<fb.NumLines(); l++ )
  {
    const unsigned LL = fb.LineLen( l );

    for( ; p<LL; p++ )
    {
      const char c1 = (1<p) ? fb.Get( l, p-2 ) : 0;
      const char c2 = (0<p) ? fb.Get( l, p-1 ) : 0;
      const char c3 =         fb.Get( l, p   );

      if( c1=='-' && c2=='-' && c3=='>' )
      {
        hi_state = &ME::Hi_End_Comment;
      }
      else fb.SetSyntaxStyle( l, p, HI_COMMENT );

      if( &ME::Hi_In__Comment != hi_state ) return;
    }
    p = 0;
  }
  hi_state = 0;
}

void Highlight_XML::Hi_End_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_COMMENT ); p+=3;

  hi_state = &ME::Hi_In_None;
}

void Highlight_XML::Hi_BegSingleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_CONST );
  p++;
  hi_state = &ME::Hi_In_SingleQuote;
}

void Highlight_XML::Hi_In_SingleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<fb.NumLines(); l++ )
  {
    const unsigned LL = fb.LineLen( l );

    bool slash_escaped = false;
    for( ; p<LL; p++ )
    {
      const char c1 = p ? fb.Get( l, p-1 ) : fb.Get( l, p );
      const char c2 = p ? fb.Get( l, p   ) : 0;

      if( (c1!='\\' && c2=='\'')
       || (c1=='\\' && c2=='\'' && slash_escaped) )
      {
        hi_state = &ME::Hi_EndSingleQuote;
      }
      else {
        if( c1=='\\' && c2=='\\' ) slash_escaped = true;
        else                       slash_escaped = false;

        fb.SetSyntaxStyle( l, p, HI_CONST );
      }
      if( &ME::Hi_In_SingleQuote != hi_state ) return;
    }
    p = 0;
  }
  hi_state = 0;
}

void Highlight_XML::Hi_EndSingleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_CONST );
  p++; //p++;
  hi_state = &ME::Hi_In_None;
}

void Highlight_XML::Hi_BegDoubleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_CONST );
  p++;
  hi_state = &ME::Hi_In_DoubleQuote;
}

void Highlight_XML::Hi_In_DoubleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<fb.NumLines(); l++ )
  {
    const unsigned LL = fb.LineLen( l );

    bool slash_escaped = false;
    for( ; p<LL; p++ )
    {
      const char c1 = p ? fb.Get( l, p-1 ) : fb.Get( l, p );
      const char c2 = p ? fb.Get( l, p   ) : 0;

      if( (c1!='\\' && c2=='\"')
       || (c1=='\\' && c2=='\"' && slash_escaped) )
      {
        hi_state = &ME::Hi_EndDoubleQuote;
      }
      else {
        if( c1=='\\' && c2=='\\' ) slash_escaped = true;
        else                       slash_escaped = false;

        fb.SetSyntaxStyle( l, p, HI_CONST );
      }
      if( &ME::Hi_In_DoubleQuote != hi_state ) return;
    }
    p = 0;
  }
  hi_state = 0;
}

void Highlight_XML::Hi_EndDoubleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  fb.SetSyntaxStyle( l, p, HI_CONST );
  p++; //p++;
  hi_state = &ME::Hi_In_None;
}

void Highlight_XML::Hi_NumberBeg( unsigned& l, unsigned& p )
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

void Highlight_XML::Hi_NumberIn( unsigned& l, unsigned& p )
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

void Highlight_XML::Hi_NumberHex( unsigned& l, unsigned& p )
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

void Highlight_XML::Hi_NumberFraction( unsigned& l, unsigned& p )
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

void Highlight_XML::Hi_NumberExponent( unsigned& l, unsigned& p )
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

void Highlight_XML::Find_Styles_Keys()
{
  Hi_FindKey( HiPairs );
}

void Highlight_XML::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Hi_FindKey_In_Range( HiPairs, st, fn );
}

