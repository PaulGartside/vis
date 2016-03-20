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

#include "Utilities.hh"
#include "MemLog.hh"
#include "FileBuf.hh"
#include "Highlight_HTML.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

static HiKeyVal HiPairs[] =
{
  // Keywords:
  { "break"              , HI_CONTROL },
  { "break"              , HI_CONTROL },
  { "catch"              , HI_CONTROL },
  { "continue"           , HI_CONTROL },
  { "debugger"           , HI_CONTROL },
  { "default"            , HI_CONTROL },
  { "delete"             , HI_CONTROL },
  { "do"                 , HI_CONTROL },
  { "else"               , HI_CONTROL },
  { "finally"            , HI_CONTROL },
  { "for"                , HI_CONTROL },
  { "function"           , HI_CONTROL },
  { "if"                 , HI_CONTROL },
  { "in"                 , HI_CONTROL },
  { "instanceof"         , HI_CONTROL },
  { "new"                , HI_VARTYPE },
  { "return"             , HI_CONTROL },
  { "switch"             , HI_CONTROL },
  { "throw"              , HI_CONTROL },
  { "try"                , HI_CONTROL },
  { "typeof"             , HI_VARTYPE },
  { "var"                , HI_VARTYPE },
  { "void"               , HI_VARTYPE },
  { "while"              , HI_CONTROL },
  { "with"               , HI_CONTROL },

  // Keywords in strict mode:
  { "implements"         , HI_CONTROL },
  { "interface"          , HI_CONTROL },
  { "let"                , HI_VARTYPE },
  { "package"            , HI_DEFINE },
  { "private"            , HI_CONTROL },
  { "protected"          , HI_CONTROL },
  { "public"             , HI_CONTROL },
  { "static"             , HI_VARTYPE },
  { "yield"              , HI_CONTROL },

  // Constants:
  { "false"              , HI_CONST   },
  { "null"               , HI_CONST   },
  { "true"               , HI_CONST   },

  // Global variables and functions:
  { "arguments"          , HI_VARTYPE   },
  { "Array"              , HI_VARTYPE   },
  { "Boolean"            , HI_VARTYPE   },
  { "Date"               , HI_CONTROL   },
  { "decodeURI"          , HI_CONTROL   },
  { "decodeURIComponent" , HI_CONTROL   },
  { "encodeURI"          , HI_CONTROL   },
  { "encodeURIComponent" , HI_CONTROL   },
  { "Error"              , HI_VARTYPE   },
  { "eval"               , HI_CONTROL   },
  { "EvalError"          , HI_CONTROL   },
  { "Function"           , HI_CONTROL   },
  { "Infinity"           , HI_CONST     },
  { "isFinite"           , HI_CONTROL   },
  { "isNaN"              , HI_CONTROL   },
  { "JSON"               , HI_CONTROL   },
  { "Math"               , HI_CONTROL   },
  { "NaN"                , HI_CONST     },
  { "Number"             , HI_VARTYPE   },
  { "Object"             , HI_VARTYPE   },
  { "parseFloat"         , HI_CONTROL   },
  { "parseInt"           , HI_CONTROL   },
  { "RangeError"         , HI_VARTYPE   },
  { "ReferenceError"     , HI_VARTYPE   },
  { "RegExp"             , HI_CONTROL   },
  { "String"             , HI_VARTYPE   },
  { "SyntaxError"        , HI_VARTYPE   },
  { "TypeError"          , HI_VARTYPE   },
  { "undefined"          , HI_CONST     },
  { "URIError"           , HI_VARTYPE   },

  { 0 }
};

static HiKeyVal TagPairs[] =
{
  // HTML tags:
//{ "DOCTYPE", HI_DEFINE  },
  { "a"       , HI_VARTYPE },
  { "b"       , HI_VARTYPE },
  { "body"    , HI_VARTYPE },
  { "br"      , HI_VARTYPE },
  { "button"  , HI_VARTYPE },
  { "canvas"  , HI_VARTYPE },
  { "circle"  , HI_VARTYPE },
  { "defs"    , HI_VARTYPE },
  { "div"     , HI_VARTYPE },
  { "fieldset", HI_VARTYPE },
  { "figure"  , HI_VARTYPE },
  { "filter"  , HI_VARTYPE },
  { "font"    , HI_VARTYPE },
  { "form"    , HI_VARTYPE },
  { "g"       , HI_VARTYPE },
  { "head"    , HI_VARTYPE },
  { "html"    , HI_VARTYPE },
  { "h1"      , HI_VARTYPE },
  { "h2"      , HI_VARTYPE },
  { "h3"      , HI_VARTYPE },
  { "h4"      , HI_VARTYPE },
  { "h5"      , HI_VARTYPE },
  { "h6"      , HI_VARTYPE },
  { "i"       , HI_VARTYPE },
  { "input"   , HI_VARTYPE },
  { "li"      , HI_VARTYPE },
  { "line"    , HI_VARTYPE },
  { "meta"    , HI_VARTYPE },
  { "ol"      , HI_VARTYPE },
  { "output"  , HI_VARTYPE },
  { "p"       , HI_VARTYPE },
  { "pre"     , HI_VARTYPE },
  { "span"    , HI_VARTYPE },
  { "strong"  , HI_VARTYPE },
  { "style"   , HI_VARTYPE },
  { "script"  , HI_VARTYPE },
  { "svg"     , HI_VARTYPE },
  { "table"   , HI_VARTYPE },
  { "td"      , HI_VARTYPE },
  { "text"    , HI_VARTYPE },
  { "textarea", HI_VARTYPE },
  { "title"   , HI_VARTYPE },
  { "th"      , HI_VARTYPE },
  { "tr"      , HI_VARTYPE },
  { "tt"      , HI_VARTYPE },
  { "ul"      , HI_VARTYPE },
  { 0 }
};

Highlight_HTML::Highlight_HTML( FileBuf& rfb )
  : Highlight_Base( rfb )
  , hi_state( &ME::Hi_In_None )
{
}

void Highlight_HTML::Run()
{
  hi_state = &ME::Hi_In_None;
  unsigned l=0;
  unsigned p=0;

  for( unsigned k=0; k<999 && hi_state; k++ )
  {
    (this->*hi_state)( l, p );
  }
  Find_Styles_Keys();
}

void Highlight_HTML::Run_Range( const CrsPos st, const unsigned fn )
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

void Highlight_HTML::Hi_In_None( unsigned& l, unsigned& p )
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

      if     ( p<LL-3 && 0==strncmp( s, "<!--", 4 ) ) hi_state = &ME::Hi_Comment;
      else if( p<LL-1 && 0==strncmp( s, "</"  , 2 ) ) hi_state = &ME::Hi_Tag_Close;
      else if(           0==strncmp( s, "<"   , 1 ) ) hi_state = &ME::Hi_Tag_Open;

      if( &ME::Hi_In_None != hi_state ) return;
    }
    p = 0;
  }
  hi_state = 0;
}

void Highlight_HTML::Hi_Comment( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );

  fb.SetSyntaxStyle( l, p, HI_COMMENT ); p++;
  fb.SetSyntaxStyle( l, p, HI_COMMENT ); p++;
  fb.SetSyntaxStyle( l, p, HI_COMMENT ); p++;
  fb.SetSyntaxStyle( l, p, HI_COMMENT ); p++;

  for( ; l<fb.NumLines(); l++ )
  {
    const unsigned LL = fb.LineLen( l );
    const Line&    lr = fb.GetLine( l );

    for( ; p<LL; p++ )
    {
      const char* s = lr.c_str( p );

      if( 0==strncmp( s, "-->", 3 ) )
      {
        fb.SetSyntaxStyle( l, p, HI_COMMENT ); p++;
        fb.SetSyntaxStyle( l, p, HI_COMMENT ); p++;
        fb.SetSyntaxStyle( l, p, HI_COMMENT ); p++;

        hi_state = &ME::Hi_In_None;
      }
      else fb.SetSyntaxStyle( l, p, HI_COMMENT );

      if( &ME::Hi_Comment != hi_state ) return;
    }
    p = 0;
  }
  hi_state = 0;
}

void Highlight_HTML::Hi_Tag_Open( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );

  fb.SetSyntaxStyle( l, p, HI_DEFINE ); p++; // Set '<' to HI_DEFINE

  const unsigned LL = fb.LineLen( l );
  const Line&    lr = fb.GetLine( l );

  for( unsigned h=0; TagPairs[h].key; h++ )
  {
    bool matches = true;
    const char*    tag     = TagPairs[h].key;
    const uint8_t  HI_TYPE = TagPairs[h].val;
    const unsigned TAG_LEN = strlen( tag );

    for( unsigned k=0; matches && (p+TAG_LEN)<LL && k<TAG_LEN; k++ )
    {
      if( tag[k] != tolower( lr.get(p+k) ) ) matches = false;
      else {
        if( k+1 == TAG_LEN ) // Found tag
        {
          const uint8_t NC = lr.get(p+k+1); // NC = next char
          matches = ( NC == '>' || NC == ' ' );
          if( matches )
          {
            for( unsigned m=p; m<p+TAG_LEN; m++ ) fb.SetSyntaxStyle( l, m, HI_TYPE );
            p += TAG_LEN;

            if( NC == '>' )
            {
              fb.SetSyntaxStyle( l, p, HI_DEFINE ); // Set '>' to HI_DEFINE
              hi_state = &ME::Hi_In_None;
            }
            else {
              hi_state = &ME::Hi_Tag_In;
            }
            p++;
            return;
          }
        }
      }
    }
  }
  // Did not find tag, to go back to none state:
  hi_state = &ME::Hi_In_None;
}

// <tag_name Hi_Tag_In> or
// <tag_name Hi_Tag_In/>
void Highlight_HTML::Hi_Tag_In( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( ; l<fb.NumLines(); l++ )
  {
    const unsigned LL = fb.LineLen( l );
    const Line&    lr = fb.GetLine( l );

    for( ; p<LL; p++ )
    {
      const char* s = lr.c_str( p );

      if( 0==strncmp( s, "/>", 2 ) )
      {
        fb.SetSyntaxStyle( l, p, HI_DEFINE ); p++; // Set '/' to HI_DEFINE
        fb.SetSyntaxStyle( l, p, HI_DEFINE ); p++; // Set '>' to HI_DEFINE
        hi_state = &ME::Hi_In_None;
        return;
      }
      else if( 0==strncmp( s, ">", 1 ) )
      {
        fb.SetSyntaxStyle( l, p, HI_DEFINE ); p++; // Set '>' to HI_DEFINE
        hi_state = &ME::Hi_In_None;
        return;
      }
    }
    p = 0;
  }
  hi_state = 0;
}

void Highlight_HTML::Hi_Tag_Close( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );

  fb.SetSyntaxStyle( l, p, HI_DEFINE ); p++; // Set '<' to HI_DEFINE
  fb.SetSyntaxStyle( l, p, HI_DEFINE ); p++; // Set '/' to HI_DEFINE

  const unsigned LL = fb.LineLen( l );
  const Line&    lr = fb.GetLine( l );

  for( unsigned h=0; TagPairs[h].key; h++ )
  {
    bool matches = true;
    const char*    tag     = TagPairs[h].key;
    const uint8_t  HI_TYPE = TagPairs[h].val;
    const unsigned TAG_LEN = strlen( tag );

    for( unsigned k=0; matches && (p+TAG_LEN)<LL && k<TAG_LEN; k++ )
    {
      if( tag[k] != tolower( lr.get(p+k) ) ) matches = false;
      else {
        if( k+1 == TAG_LEN ) // Found tag
        {
          const uint8_t NC = lr.get(p+k+1); // NC = next char
          matches = ( NC == '>' );
          if( matches )
          {
            for( unsigned m=p; m<p+TAG_LEN; m++ ) fb.SetSyntaxStyle( l, m, HI_TYPE );
            p += TAG_LEN;

            fb.SetSyntaxStyle( l, p, HI_DEFINE ); // Set '>' to HI_DEFINE
            hi_state = &ME::Hi_In_None;
            p++;
            return;
          }
        }
      }
    }
  }
  // Did not find tag, to go back to none state:
  hi_state = &ME::Hi_In_None;
}

void Highlight_HTML::Hi_BegSingleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );

  fb.SetSyntaxStyle( l, p, HI_CONST );
  p++;
  hi_state = &ME::Hi_In_SingleQuote;
}

void Highlight_HTML::Hi_In_SingleQuote( unsigned& l, unsigned& p )
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

void Highlight_HTML::Hi_EndSingleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );

  fb.SetSyntaxStyle( l, p, HI_CONST );
  p++; //p++;
  hi_state = &ME::Hi_In_None;
}

void Highlight_HTML::Hi_BegDoubleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );
//Log.Log("%s\n", __FUNCTION__);
  fb.SetSyntaxStyle( l, p, HI_CONST );
  p++;
  hi_state = &ME::Hi_In_DoubleQuote;
}

void Highlight_HTML::Hi_In_DoubleQuote( unsigned& l, unsigned& p )
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

void Highlight_HTML::Hi_EndDoubleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );

  fb.SetSyntaxStyle( l, p, HI_CONST );
  p++; //p++;
  hi_state = &ME::Hi_In_None;
}

void Highlight_HTML::Hi_NumberBeg( unsigned& l, unsigned& p )
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

void Highlight_HTML::Hi_NumberIn( unsigned& l, unsigned& p )
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

void Highlight_HTML::Hi_NumberHex( unsigned& l, unsigned& p )
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

void Highlight_HTML::Hi_NumberFraction( unsigned& l, unsigned& p )
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

void Highlight_HTML::Hi_NumberExponent( unsigned& l, unsigned& p )
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

void Highlight_HTML::Find_Styles_Keys()
{
  Hi_FindKey( HiPairs );
}

void Highlight_HTML::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Hi_FindKey_In_Range( HiPairs, st, fn );
}

