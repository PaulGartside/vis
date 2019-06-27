////////////////////////////////////////////////////////////////////////////////
// VI-Simplified (vis) C++ Implementation                                     //
// Copyright (c) 26 Jun 2019 Paul J. Gartside                                 //
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
#include "Highlight_MIB.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

Highlight_MIB::Highlight_MIB( FileBuf& rfb )
  : Highlight_Base( rfb )
  , m_state( &ME::Hi_In_None )
{
}

void Highlight_MIB::Run_Range( const CrsPos st, const unsigned fn )
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

bool Quote_Start( const char qt
                , const char c2
                , const char c1
                , const char c0 )
{
  return (c1==0    && c0==qt ) //< Quote at beginning of line
      || (c1!='\\' && c0==qt ) //< Non-escaped quote
      || (c2=='\\' && c1=='\\' && c0==qt ); //< Escaped escape before quote
}
static bool OneControl( const char c0 )
{
  return c0=='(' || c0==')'
      || c0=='{' || c0=='}'
      || c0==',' || c0==';'
      || c0=='|';
}

void Highlight_MIB::Hi_In_None( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  for( ; l<m_fb.NumLines(); l++ )
  {
    const unsigned LL = m_fb.LineLen( l );
    const Line&    lr = m_fb.GetLine( l );

    for( ; p<LL; p++ )
    {
      m_fb.ClearSyntaxStyles( l, p );

      // c0 is ahead of c1 is ahead of c2: (c2,c1,c0)
      const char c2 = 1<p ? m_fb.Get( l, p-2 ) : 0;
      const char c1 = 0<p ? m_fb.Get( l, p-1 ) : 0;
      const char c0 =       m_fb.Get( l, p   );

      if     ( c1 == '-' && c0=='-'       ) { m_state = &ME::Hi_In_Comment; }
      else if( Quote_Start('\'',c2,c1,c0) ) { m_state = &ME::Hi_SingleQuote; }
      else if( Quote_Start('\"',c2,c1,c0) ) { m_state = &ME::Hi_DoubleQuote; }
      else if( Quote_Start('`' ,c2,c1,c0) ) { m_state = &ME::Hi_96_Quote; }
      else if( !IsIdent(c1) && isdigit(c0)) { m_state = &ME::Hi_NumberBeg; }

      else if( c2==':' && c1==':' && c0=='=' )
      {
        m_fb.SetSyntaxStyle( l, p-2, HI_CONTROL );
        m_fb.SetSyntaxStyle( l, p-1, HI_CONTROL );
        m_fb.SetSyntaxStyle( l, p  , HI_VARTYPE );
      }
      else if( OneControl( c0 ) )
      {
        m_fb.SetSyntaxStyle( l, p, HI_CONTROL );
      }
      else if( c0=='.' )
      {
        m_fb.SetSyntaxStyle( l, p, HI_CONST );
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

void Highlight_MIB::Hi_In_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = m_fb.LineLen( l );

  if( 0 < p ) p--;

  for( ; p<LL; p++ )
  {
    m_fb.SetSyntaxStyle( l, p, HI_COMMENT );
  }
  p=0; l++;
  m_state = &ME::Hi_In_None;
}

void Highlight_MIB::Hi_SingleQuote( unsigned& l, unsigned& p )
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

void Highlight_MIB::Hi_DoubleQuote( unsigned& l, unsigned& p )
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

void Highlight_MIB::Hi_96_Quote( unsigned& l, unsigned& p )
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

void Highlight_MIB::Hi_NumberBeg( unsigned& l, unsigned& p )
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

void Highlight_MIB::Hi_NumberIn( unsigned& l, unsigned& p )
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

void Highlight_MIB::Hi_NumberHex( unsigned& l, unsigned& p )
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

void Highlight_MIB::Hi_NumberFraction( unsigned& l, unsigned& p )
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

void Highlight_MIB::Hi_NumberExponent( unsigned& l, unsigned& p )
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
  { "OBJECT IDENTIFIER", HI_VARTYPE },
  { "MODULE-COMPLIANCE", HI_VARTYPE },
  { "MANDATORY-GROUPS" , HI_CONTROL },

  { "SYNTAX"     , HI_CONTROL },
  { "UNITS"      , HI_CONTROL },
  { "MAX-ACCESS" , HI_CONTROL },
  { "BEGIN"      , HI_CONTROL },
  { "FROM"       , HI_CONTROL },
  { "END"        , HI_CONTROL },
  { "IMPORTS"    , HI_CONTROL },
  { "OBJECT-TYPE", HI_CONTROL },
  { "STATUS"     , HI_CONTROL },
  { "INDEX"      , HI_CONTROL },
  { "DEFVAL"     , HI_CONTROL },
  { "DISPLAY-HINT", HI_CONTROL },
  { "DESCRIPTION", HI_CONTROL },
  { "DEFINITIONS", HI_CONTROL },
  { "REVISION"   , HI_CONTROL },
  { "OBJECTS"    , HI_CONTROL },
  { "OBJECT-GROUP", HI_CONTROL },
  { "OBJECT"     , HI_CONTROL },
  { "REFERENCE"  , HI_VARTYPE },
  { "WRITE-SYNTAX", HI_CONTROL },
  { "NOTIFICATIONS", HI_CONTROL },
  { "LAST-UPDATED", HI_CONTROL },
  { "ORGANIZATION", HI_CONTROL },
  { "CONTACT-INFO", HI_CONTROL },

  { "Counter32"  , HI_VARTYPE },
  { "Counter64"  , HI_VARTYPE },
  { "INTEGER"    , HI_VARTYPE },
  { "Integer32"  , HI_VARTYPE },
  { "IpAddress"  , HI_VARTYPE },
  { "InetAddress", HI_VARTYPE },
  { "InetAddressType", HI_VARTYPE },
  { "Unsigned32" , HI_VARTYPE },
  { "Gauge32"    , HI_VARTYPE },
  { "InterfaceIndex", HI_VARTYPE },
  { "MacAddress" , HI_VARTYPE },
  { "TimeTicks"  , HI_VARTYPE },
  { "TimeStamp"  , HI_VARTYPE },
//{ "Timeout"    , HI_VARTYPE },
  { "SnmpAdminString", HI_VARTYPE },
  { "BITS"       , HI_VARTYPE },
  { "OCTET"      , HI_VARTYPE },
  { "SIZE"       , HI_VARTYPE },
  { "STRING"     , HI_VARTYPE },
  { "SEQUENCE OF", HI_VARTYPE },
  { "SEQUENCE"   , HI_CONTROL },
  { "IDENTIFIER" , HI_VARTYPE },
  { "MIN-ACCESS" , HI_VARTYPE },
  { "MODULE-IDENTITY", HI_VARTYPE },
  { "MODULE"     , HI_VARTYPE },
  { "TEXTUAL-CONVENTION", HI_VARTYPE },
  { "NOTIFICATION-GROUP", HI_VARTYPE },
  { "NOTIFICATION-TYPE", HI_VARTYPE },
  { "GROUP"      , HI_VARTYPE },
  { "RowStatus", HI_VARTYPE },
  { "TruthValue", HI_VARTYPE },
  { "current"    , HI_VARTYPE },
  { "read-only"  , HI_VARTYPE },
  { "read-create", HI_VARTYPE },
  { "read-write" , HI_VARTYPE },
  { "not-accessible" , HI_VARTYPE },
//{"", HI_VARTYPE },
  { 0 }
};

// Find keys starting on st up to but not including fn line
void Highlight_MIB::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Hi_FindKey_In_Range( HiPairs, st, fn );
}

