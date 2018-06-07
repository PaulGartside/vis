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

// JavaScript or CSS edges
struct Edges
{
  Edges()
  {}
  Edges( const unsigned l, const unsigned p )
  {
    beg.crsLine = l;
    beg.crsChar = p;
    end.crsLine = 0;
    end.crsChar = 0;
  } 
  // cp is between beg and end
  bool contains( CrsPos cp )
  {
    bool cp_past_beg_edge = beg.crsLine < cp.crsLine
                         || ( beg.crsLine == cp.crsLine
                           && beg.crsChar < cp.crsChar );

    // end == 0 means end edge is somewhere ahead of cp
    bool cp_before_end_edge = ( 0 == end.crsLine && 0 == end.crsChar )
                           || ( cp.crsLine < end.crsLine
                             || ( cp.crsLine == end.crsLine
                               && cp.crsChar < end.crsChar ) );

    return cp_past_beg_edge && cp_before_end_edge;
  }
  // cp is less than beg, or
  // beg is greater then or equal to cp
  bool ge( CrsPos cp )
  {
    // beg == null means beg edge is somewhere ahead of cp
    bool cp_before_beg_edge = cp.crsLine < beg.crsLine
                           || ( cp.crsLine == beg.crsLine
                             && cp.crsChar < beg.crsChar );
    return cp_before_beg_edge;
  }
  void Print(const char* label)
  {
    Log.Log("%s: beg=(%u,%u), end=(%u,%u)\n"
           , label
           , beg.crsLine+1, beg.crsChar+1
           , end.crsLine+1, end.crsChar+1 );
  }
  CrsPos beg;
  CrsPos end;
};

const char* Highlight_HTML::m_HTML_Tags[] =
{
  "DOCTYPE",
  "abbr"    , "address"   , "area"      , "article" ,
  "aside"   , "audio"     , "a"         , "base"    ,
  "bdi"     , "bdo"       , "blockquote", "body"    ,
  "br"      , "button"    , "b"         , "canvas"  ,
  "caption" , "cite"      , "code"      , "col"     ,
  "colgroup", "datalist"  , "dd"        , "del"     ,
  "details" , "dfn"       , "dialog"    , "div"     ,
  "dl"      , "dt"        , "em"        , "embed"   ,
  "fieldset", "figcaption", "figure"    , "footer"  ,
  "form"    , "h1"        , "h2"        , "h3"      ,
  "h4"      , "h5"        , "h6"        , "head"    ,
  "header"  , "hr"        , "html"      , "ifname"  ,
  "img"     , "input"     , "ins"       , "i"       ,
  "kbd"     , "keygen"    , "label"     , "legend"  ,
  "link"    , "li"        , "main"      , "map"     ,
  "mark"    , "menu"      , "menuitem"  , "meta"    ,
  "meter"   , "nav"       , "noscript"  , "object"  ,
  "ol"      , "optgroup"  , "option"    , "p"       ,
  "param"   , "picture"   , "pre"       , "progress",
  "q"       , "rp"        , "rt"        , "ruby"    ,
  "samp"    , "script"    , "section"   , "select"  ,
  "small"   , "source"    , "span"      , "strong"  ,
  "style"   , "sub"       , "summary"   , "sup"     ,
  "s"       , "table"     , "tbody"     , "td"      ,
  "textarea", "tfoot"     , "thread"    , "th"      ,
  "time"    , "title"     , "tr"        , "track"   ,
  "ul"      , "u"         , "var"       , "video"   ,
  "wbr"     ,
};

Highlight_HTML::Highlight_HTML( FileBuf& rfb )
  : Highlight_Base( rfb )
  , m_state( St_In_None )
  , m_qtXSt( St_In_None )
  , m_ccXSt( St_JS_None )
  , m_numXSt( St_In_None )
  , m_l(0)
  , m_p(0)
  , m_OpenTag_was_script(false)
  , m_OpenTag_was_style(false)
  , m_JS_edges()
  , m_CS_edges()
{
}

void Highlight_HTML::Run_Range( const CrsPos st, const unsigned fn )
{
  m_state = Run_Range_Get_Initial_State( st );

  m_l = st.crsLine;
  m_p = st.crsChar;

  while( St_Done != m_state && m_l<fn )
  {
    const bool state_was_JS = JS_State( m_state );
    const unsigned st_l = m_l;
    const unsigned st_p = m_p;

    Run_State();

    if( state_was_JS )
    {
      CrsPos st = { st_l, st_p };

      Find_Styles_Keys_In_Range( st, m_l+1 );
    }
  }
}

// Find keys starting from st up to but not including fn line
void Highlight_HTML::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Hi_FindKey_In_Range( m_JS_HiPairs, st, fn );
}

Highlight_HTML::Hi_State
Highlight_HTML::Run_Range_Get_Initial_State( const CrsPos st )
{
  Hi_State initial = St_In_None;

  if( Get_Initial_State( st, m_JS_edges, m_CS_edges ) )
  {
    initial = St_JS_None;
  }
  else if( Get_Initial_State( st, m_CS_edges, m_JS_edges ) )
  {
    initial = St_CS_None;
  }
  return initial;
}

bool Highlight_HTML::Get_Initial_State( const CrsPos st
                                      , Array_t<Edges> edges_1
                                      , Array_t<Edges> edges_2 )
{
  bool found_containing_edges = false;

  for( unsigned k=0; k<edges_1.len(); k++ )
  {
    if( !found_containing_edges )
    {
      Edges edges = edges_1[k];
      if( edges.contains( st ) )
      {
        found_containing_edges = true;
      }
    }
    else {
      // Since a change was made at st, all the following edges
      // have been invalidated, so remove all following elements
      edges_1.remove( k );
      k--; //< Since the current element was just removed, stay on k
    }
  }
  if( found_containing_edges )
  {
    // Remove all CS_edges past st:
    for( unsigned k=0; k<edges_2.len(); k++ )
    {
      Edges edges = edges_2[k];
      if( edges.ge( st ) )
      {
        edges_2.remove( k );
        k--; //< Since the current element was just removed, stay on k
      }
    }
  }
  return found_containing_edges;
}

bool Highlight_HTML::JS_State( const Hi_State state )
{
  return state == St_JS_None
      || state == St_JS_Define
      || state == St_JS_SingleQuote
      || state == St_JS_DoubleQuote
      || state == St_JS_C_Comment
      || state == St_JS_CPP_Comment
      || state == St_JS_NumberBeg
      || state == St_JS_NumberDec
      || state == St_JS_NumberHex
      || state == St_JS_NumberFraction
      || state == St_JS_NumberExponent
      || state == St_JS_NumberTypeSpec;
}

void Highlight_HTML::Run_State()
{
  switch( m_state )
  {
  case St_In_None         : Hi_In_None         (); break;
  case St_XML_Comment     : Hi_XML_Comment     (); break;
  case St_CloseTag        : Hi_CloseTag        (); break;
  case St_NumberBeg       : Hi_NumberBeg       (); break;
  case St_NumberHex       : Hi_NumberHex       (); break;
  case St_NumberDec       : Hi_NumberDec       (); break;
  case St_NumberExponent  : Hi_NumberExponent  (); break;
  case St_NumberFraction  : Hi_NumberFraction  (); break;
  case St_NumberTypeSpec  : Hi_NumberTypeSpec  (); break;
  case St_OpenTag_ElemName: Hi_OpenTag_ElemName(); break;
  case St_OpenTag_AttrName: Hi_OpenTag_AttrName(); break;
  case St_OpenTag_AttrVal : Hi_OpenTag_AttrVal (); break;
  case St_SingleQuote     : Hi_SingleQuote     (); break;
  case St_DoubleQuote     : Hi_DoubleQuote     (); break;

  case St_JS_None          : Hi_JS_None       (); break;
  case St_JS_Define        : Hi_JS_Define     (); break;
  case St_JS_SingleQuote   : Hi_SingleQuote   (); break;
  case St_JS_DoubleQuote   : Hi_DoubleQuote   (); break;
  case St_JS_C_Comment     : Hi_C_Comment     (); break;
  case St_JS_CPP_Comment   : Hi_JS_CPP_Comment(); break;
  case St_JS_NumberBeg     : Hi_NumberBeg     (); break;
  case St_JS_NumberDec     : Hi_NumberDec     (); break;
  case St_JS_NumberHex     : Hi_NumberHex     (); break;
  case St_JS_NumberFraction: Hi_NumberFraction(); break;
  case St_JS_NumberExponent: Hi_NumberExponent(); break;
  case St_JS_NumberTypeSpec: Hi_NumberTypeSpec(); break;

  case St_CS_None          : Hi_CS_None       (); break;
  case St_CS_C_Comment     : Hi_C_Comment     (); break;
  case St_CS_SingleQuote   : Hi_SingleQuote   (); break;
  case St_CS_DoubleQuote   : Hi_DoubleQuote   (); break;

  default:
    m_state = St_In_None;
  }
}

void Highlight_HTML::Hi_In_None()
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( ; m_l<m_fb.NumLines(); m_l++ )
  {
    const unsigned LL = m_fb.LineLen( m_l );
    const Line&    lr = m_fb.GetLine( m_l );

    for( ; m_p<LL; m_p++ )
    {
      m_fb.ClearSyntaxStyles( m_l, m_p );

      // c0 is ahead of c1 is ahead of c2: (c3,c2,c1,c0)
      const char c3 = (2<m_p) ? m_fb.Get( m_l, m_p-3 ) : 0;
      const char c2 = (1<m_p) ? m_fb.Get( m_l, m_p-2 ) : 0;
      const char c1 = (0<m_p) ? m_fb.Get( m_l, m_p-1 ) : 0;
      const char c0 =           m_fb.Get( m_l, m_p );

      if( c1=='<' && c0!='!' && c0!='/')
      {
        m_fb.SetSyntaxStyle( m_l, m_p-1, HI_DEFINE );
        m_state = St_OpenTag_ElemName;
      }
      else if( c1=='<' && c0=='/')
      {
        m_fb.SetSyntaxStyle( m_l, m_p-1, HI_DEFINE ); //< '<'
        m_fb.SetSyntaxStyle( m_l, m_p  , HI_DEFINE ); //< '/'
        m_p++; // Move past '/'
        m_state = St_CloseTag;
      }
      else if( c3=='<' && c2=='!' && c1=='-' && c0=='-')
      {
        m_fb.SetSyntaxStyle( m_l, m_p-3, HI_COMMENT ); //< '<'
        m_fb.SetSyntaxStyle( m_l, m_p-2, HI_COMMENT ); //< '!'
        m_fb.SetSyntaxStyle( m_l, m_p-1, HI_COMMENT ); //< '-'
        m_fb.SetSyntaxStyle( m_l, m_p  , HI_COMMENT ); //< '-'
        m_p++; // Move past '-'
        m_state = St_XML_Comment;
      }
      else if( c3=='<' && c2=='!' && c1=='D' && c0=='O')
      {
        // <!DOCTYPE html>   
        m_fb.SetSyntaxStyle( m_l, m_p-3, HI_DEFINE ); //< '<'
        m_fb.SetSyntaxStyle( m_l, m_p-2, HI_DEFINE ); //< '!'
        m_p--; // Move back to 'D'
        m_state = St_OpenTag_ElemName;
      }
      else if( !IsIdent( c1 )
            && isdigit( c0 ) )
      {
        m_state = St_NumberBeg;
        m_numXSt = St_In_None;
      }
      else {
        ; //< No syntax highlighting on content outside of <>tags
      }
      if( St_In_None != m_state ) return;
    }
    m_p = 0;
  }
  m_state = St_Done;
}

void Highlight_HTML::Hi_XML_Comment()
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( ; m_l<m_fb.NumLines(); m_l++ )
  {
    const unsigned LL = m_fb.LineLen( m_l );
    for( ; m_p<LL; m_p++ )
    {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_COMMENT );

      // c0 is ahead of c1 is ahead of c2: (c2,c1,c0)
      const char c2 = (1<m_p) ? m_fb.Get( m_l, m_p-2 ) : 0;
      const char c1 = (0<m_p) ? m_fb.Get( m_l, m_p-1 ) : 0;
      const char c0 =           m_fb.Get( m_l, m_p );

      if( c2=='-' && c1=='-' && c0=='>' )
      {
        m_p++; // Move past '>'
        m_state = St_In_None;
      }
      if( St_XML_Comment != m_state ) return;
    }
    m_p = 0;
  }
  m_state = St_Done;
}

void Highlight_HTML::Hi_CloseTag()
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found_elem_name = false;

  for( ; m_l<m_fb.NumLines(); m_l++ )
  {
    const Line* lp = m_fb.GetLineP( m_l );
    const unsigned LL = m_fb.LineLen( m_l );
    for( ; m_p<LL; m_p++ )
    {
      const char c0 = m_fb.Get( m_l, m_p );

      if( c0=='>' )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
        m_p++; // Move past '>'
        m_state = St_In_None;
      }
      else if( c0=='/' || c0=='?' )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
      }
      else if( !found_elem_name )
      {
        // Returns non-zero if lp has HTTP tag at m_p:
        const unsigned tag_len = Has_HTTP_Tag_At( lp, m_p );
        if( 0<tag_len )
        {
          found_elem_name = true;
          for( unsigned k=0; k<tag_len; k++, m_p++ )
          {
            m_fb.SetSyntaxStyle( m_l, m_p, HI_CONTROL );
          }
          m_p--;
        }
        else if( c0==' ' || c0=='\t' )
        {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
        }
        else {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_NONASCII );
        }
      }
      else //( found_elem_name )
      {
        if( c0==' ' || c0=='\t' )
        {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_CONTROL );
        }
        else {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_NONASCII );
        }
      }
      if( St_CloseTag != m_state ) return;
    }
    m_p = 0;
  }
  m_state = St_Done;
}

void Highlight_HTML::Hi_NumberBeg()
{
  m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );

  const char c1 = m_fb.Get( m_l, m_p );
  m_p++; //< Move past first digit
  m_state = St_JS_NumberBeg == m_state
          ? St_JS_NumberDec
          : St_NumberDec;

  const unsigned LL = m_fb.LineLen( m_l );
  if( '0' == c1 && (m_p+1)<LL )
  {
    const char c0 = m_fb.Get( m_l, m_p );
    if( 'x' == c0 ) {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
      m_state = St_JS_NumberBeg == m_state
              ? St_JS_NumberHex
              : St_NumberHex;
      m_p++; //< Move past 'x'
    }
  }
}

void Highlight_HTML::Hi_NumberHex()
{
  const unsigned LL = m_fb.LineLen( m_l );
  if( LL <= m_p ) m_state = m_numXSt;
  else {
    const char c1 = m_fb.Get( m_l, m_p );
    if( isxdigit(c1) )
    {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
      m_p++;
    }
    else {
      m_state = m_numXSt;
    }
  }
}

void Highlight_HTML::Hi_NumberDec()
{
  const unsigned LL = m_fb.LineLen( m_l );
  if( LL <= m_p ) m_state = m_numXSt;
  else {
    const char c1 = m_fb.Get( m_l, m_p );

    if( '.'==c1 )
    {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
      m_state = St_JS_NumberDec == m_state
              ? St_JS_NumberFraction
              : St_NumberFraction;
      m_p++;
    }
    else if( 'e'==c1 || 'E'==c1 )
    {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
      m_state = St_JS_NumberDec == m_state
              ? St_JS_NumberExponent
              : St_NumberExponent;
      m_p++;
      if( m_p<LL )
      {
        const char c0 = m_fb.Get( m_l, m_p );
        if( '+' == c0 || '-' == c0 ) {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
          m_p++;
        }
      }
    }
    else if( isdigit(c1) )
    {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
      m_p++;
    }
    else if( c1=='L' || c1=='F' || c1=='U' )
    {
      m_state = St_JS_NumberDec == m_state
              ? St_JS_NumberTypeSpec
              : St_NumberTypeSpec;
    }
    else {
      m_state = m_numXSt;
    }
  }
}

void Highlight_HTML::Hi_NumberExponent()
{
  const unsigned LL = m_fb.LineLen( m_l );
  if( LL <= m_p ) m_state = m_numXSt;
  else {
    const char c1 = m_fb.Get( m_l, m_p );
    if( isdigit(c1) )
    {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
      m_p++;
    }
    else {
      m_state = m_numXSt;
    }
  }
}

void Highlight_HTML::Hi_NumberFraction()
{
  const unsigned LL = m_fb.LineLen( m_l );
  if( LL <= m_p ) m_state = m_numXSt;
  else {
    const char c1 = m_fb.Get( m_l, m_p );
    if( isdigit(c1) )
    {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
      m_p++;
    }
    else if( 'e'==c1 || 'E'==c1 )
    {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
      m_state = St_JS_NumberFraction == m_state
              ? St_JS_NumberExponent
              : St_NumberExponent;
      m_p++;
      if( m_p<LL )
      {
        const char c0 = m_fb.Get( m_l, m_p );
        if( '+' == c0 || '-' == c0 ) {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
          m_p++;
        }
      }
    }
    else {
      m_state = m_numXSt;
    }
  }
}

void Highlight_HTML::Hi_NumberTypeSpec()
{
  const unsigned LL = m_fb.LineLen( m_l );

  if( m_p < LL )
  {
    const char c0 = m_fb.Get( m_l, m_p );

    if( c0=='L' )
    {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_VARTYPE );
      m_p++;
      m_state = m_numXSt;
    }
    else if( c0=='F' )
    {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_VARTYPE );
      m_p++;
      m_state = m_numXSt;
    }
    else if( c0=='U' )
    {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_VARTYPE ); m_p++;
      if( m_p<LL ) {
        const char c1 = m_fb.Get( m_l, m_p );
        if( c1=='L' ) { // UL
          m_fb.SetSyntaxStyle( m_l, m_p, HI_VARTYPE ); m_p++;
          if( m_p<LL ) {
            const char c2 = m_fb.Get( m_l, m_p );
            if( c2=='L' ) { // ULL
              m_fb.SetSyntaxStyle( m_l, m_p, HI_VARTYPE ); m_p++;
            }
          }
        }
      }
      m_state = m_numXSt;
    }
  }
}

void Highlight_HTML::Hi_OpenTag_ElemName()
{
  m_OpenTag_was_script = false;
  m_OpenTag_was_style  = false;
  bool found_elem_name = false;

  for( ; m_l<m_fb.NumLines(); m_l++ )
  {
    const Line* lp = m_fb.GetLineP( m_l );
    const unsigned LL = m_fb.LineLen( m_l );
    for( ; m_p<LL; m_p++ )
    {
      const char c0 = m_fb.Get( m_l, m_p );

      if( c0=='>' )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
        m_p++; // Move past '>'
        m_state = St_In_None;
        if( m_OpenTag_was_script )
        {
          m_state = St_JS_None;
          m_JS_edges.push( Edges( m_l, m_p ) );
        }
        else if( m_OpenTag_was_style )
        {
          m_state = St_CS_None;
          m_CS_edges.push( Edges( m_l, m_p ) );
        }
      }
      else if( c0=='/' || c0=='?' )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
      }
      else if( !found_elem_name )
      {
        // Returns non-zero if lp has HTTP tag at m_p:
        const unsigned tag_len = Has_HTTP_Tag_At( lp, m_p );
        if( 0<tag_len )
        {
          if( 0==strncasecmp( lp->c_str(m_p), "script", 6 ) )
          {
            m_OpenTag_was_script = true;
          }
          else if( 0==strncasecmp( lp->c_str(m_p), "style", 5 ) )
          {
            m_OpenTag_was_style = true;
          }
          found_elem_name = true;
          for( unsigned k=0; k<tag_len; k++, m_p++ )
          {
            m_fb.SetSyntaxStyle( m_l, m_p, HI_CONTROL );
          }
          m_p--;
        }
        else if( c0==' ' || c0=='\t' )
        {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
        }
        else {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_NONASCII );
        }
      }
      else //( found_elem_name )
      {
        if( c0==' ' || c0=='\t' )
        {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_CONTROL );
          m_p++; //< Move past white space
          m_state = St_OpenTag_AttrName;
        }
        else {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_NONASCII );
        }
      }
      if( St_OpenTag_ElemName != m_state ) return;
    }
    m_p = 0;
  }
  m_state = St_Done;
}

void Highlight_HTML::Hi_OpenTag_AttrName()
{
  bool found_attr_name = false;
  bool past__attr_name = false;

  for( ; m_l<m_fb.NumLines(); m_l++ )
  {
    const unsigned LL = m_fb.LineLen( m_l );

    for( ; m_p<LL; m_p++ )
    {
      // c0 is ahead of c1 is ahead of c2: (c2,c1,c0)
      const char c0 = m_fb.Get( m_l, m_p );

      if( c0=='>' )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
        m_p++; // Move past '>'
        m_state = m_OpenTag_was_style
                ? St_CS_None
                : St_In_None;
      }
      else if( c0=='/' || c0=='?' )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
      }
      else if( !found_attr_name )
      {
        if( IsXML_Ident( c0 ) )
        {
          found_attr_name = true;
          m_fb.SetSyntaxStyle( m_l, m_p, HI_VARTYPE );
        }
        else if( c0==' ' || c0=='\t' )
        {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_CONTROL );
        }
        else {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_NONASCII );
        }
      }
      else if( found_attr_name && !past__attr_name )
      {
        if( IsXML_Ident( c0 ) )
        {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_VARTYPE );
        }
        else if( c0==' ' || c0=='\t' )
        {
          past__attr_name = true;
          m_fb.SetSyntaxStyle( m_l, m_p, HI_CONTROL );
        }
        else if( c0=='=' )
        {
          past__attr_name = true;
          m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
          m_p++; //< Move past '='
          m_state = St_OpenTag_AttrVal;
        }
        else {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_NONASCII );
        }
      }
      else //( found_attr_name && past__attr_name )
      {
        if( c0=='=' )
        {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
          m_p++; //< Move past '='
          m_state = St_OpenTag_AttrVal;
        }
        else if( c0==' ' || c0=='\t' )
        {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_VARTYPE );
        }
        else {
          m_fb.SetSyntaxStyle( m_l, m_p, HI_NONASCII );
        }
      }
      if( St_OpenTag_AttrName != m_state ) return;
    }
    m_p = 0;
  }
  m_state = St_Done;
}

void Highlight_HTML::Hi_OpenTag_AttrVal()
{
  for( ; m_l<m_fb.NumLines(); m_l++ )
  {
    const unsigned LL = m_fb.LineLen( m_l );

    for( ; m_p<LL; m_p++ )
    {
      // c0 is ahead of c1 is ahead of c2: (c2,c1,c0)
      const char c0 = m_fb.Get( m_l, m_p );

      if( c0=='>' )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
        m_p++; // Move past '>'
        m_state = St_In_None;
        m_state = m_OpenTag_was_style
                ? St_CS_None
                : St_In_None;
      }
      else if( c0=='/' || c0=='?' )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
      }
      else if( c0=='\'' )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
        m_p++; // Move past '\''
        m_state = St_SingleQuote;
        m_qtXSt = St_OpenTag_AttrName;
      }
      else if( c0=='\"' )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
        m_p++; //< Move past '\"'
        m_state = St_DoubleQuote;
        m_qtXSt = St_OpenTag_AttrName;
      }
      else if( isdigit( c0 ) )
      {
        m_state = St_NumberBeg;
        m_numXSt = St_OpenTag_AttrName;
      }
      else if( c0==' ' || c0=='\t' )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
      }
      else {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_NONASCII );
      }
      if( St_OpenTag_AttrVal != m_state ) return;
    }
    m_p = 0;
  }
  m_state = St_Done;
}

void Highlight_HTML::Hi_SingleQuote()
{
  bool exit = false;
  for( ; m_l<m_fb.NumLines(); m_l++ )
  {
    const unsigned LL = m_fb.LineLen( m_l );

    bool slash_escaped = false;
    for( ; m_p<LL; m_p++ )
    {
      const char c1 = 0<m_p ? m_fb.Get( m_l, m_p-1 ) : m_fb.Get( m_l, m_p );
      const char c0 = 0<m_p ? m_fb.Get( m_l, m_p   ) : 0;

      if( (c1=='\'' && c0==0   )
       || (c1!='\\' && c0=='\'')
       || (c1=='\\' && c0=='\'' && slash_escaped) )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
        m_p++; // Move past '\''
        m_state = m_qtXSt;
        exit = true;
      }
      else {
        if( c1=='\\' && c0=='\\' ) slash_escaped = true;
        else                       slash_escaped = false;

        m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
      }
      if( exit ) return;
    }
    m_p = 0;
  }
  m_state = St_Done;
}

void Highlight_HTML::Hi_DoubleQuote()
{
  bool exit = false;
  for( ; m_l<m_fb.NumLines(); m_l++ )
  {
    const unsigned LL = m_fb.LineLen( m_l );

    bool slash_escaped = false;
    for( ; m_p<LL; m_p++ )
    {
      const char c1 = 0<m_p ? m_fb.Get( m_l, m_p-1 ) : m_fb.Get( m_l, m_p );
      const char c0 = 0<m_p ? m_fb.Get( m_l, m_p   ) : 0;

      if( (c1=='\"' && c0==0   )
       || (c1!='\\' && c0=='\"')
       || (c1=='\\' && c0=='\"' && slash_escaped) )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
        m_p++; //< Move past '\"'
        m_state = m_qtXSt;
        exit = true;
      }
      else {
        if( c1=='\\' && c0=='\\' ) slash_escaped = true;
        else                       slash_escaped = false;

        m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
      }
      if( exit ) return;
    }
    m_p = 0;
  }
  m_state = St_Done;
}

bool JS_OneVarType( const char c0 )
{
  return c0=='&'
      || c0=='.' || c0=='*'
      || c0=='[' || c0==']';
}

bool JS_OneControl( const char c0 )
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

bool JS_TwoControl( const char c1, const char c0 )
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

void Highlight_HTML::Hi_JS_None()
{
  for( ; m_l<m_fb.NumLines(); m_l++ )
  {
    const Line* lp = m_fb.GetLineP( m_l );
    const unsigned LL = m_fb.LineLen( m_l );

    for( ; m_p<LL; m_p++ )
    {
      m_fb.ClearSyntaxStyles( m_l, m_p );

      // c0 is ahead of c1 is ahead of c2: (c2,c1,c0)
      const char c2 = (1<m_p) ? m_fb.Get( m_l, m_p-2 ) : 0;
      const char c1 = (0<m_p) ? m_fb.Get( m_l, m_p-1 ) : 0;
      const char c0 =           m_fb.Get( m_l, m_p );

      if     ( c1=='/' && c0 == '/' ) { m_p--; m_state = St_JS_CPP_Comment; }
      else if( c1=='/' && c0 == '*' )
      {
        m_p--;
        m_state = St_JS_C_Comment;
        m_ccXSt = St_JS_None;
      }
      else if( c0 == '#' ) { m_state = St_JS_Define; }
      else if( c0 == '\'')
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
        m_p++; //< Move past '\"'
        m_state = St_JS_SingleQuote;
        m_qtXSt = St_JS_None;
      }
      else if( c0 == '\"')
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
        m_p++; //< Move past '\"'
        m_state = St_JS_DoubleQuote;
        m_qtXSt = St_JS_None;
      }
      else if( !IsIdent( c1 )
             && isdigit(c0))
      {
        m_state = St_NumberBeg;
        m_numXSt = St_JS_None;
      }
      else if( (c1==':' && c0==':')
            || (c1=='-' && c0=='>') )
      {
        m_fb.SetSyntaxStyle( m_l, m_p-1, HI_VARTYPE );
        m_fb.SetSyntaxStyle( m_l, m_p  , HI_VARTYPE );
      }
      else if( c1=='<' && c0=='/' && m_p+7<LL )
      {
        if( 0==strncasecmp( lp->c_str(m_p-1), "</script", 8 ) )
        {
          m_fb.SetSyntaxStyle( m_l, m_p-1, HI_DEFINE );
          m_fb.SetSyntaxStyle( m_l, m_p  , HI_DEFINE );
          m_p++; // Move past '/'
          m_state = St_CloseTag;
          if( 0<m_JS_edges.len() ) { //< Should always be true
            Edges* edges = m_JS_edges.get( m_JS_edges.len()-1 );
            edges->end.crsLine = m_l;
            edges->end.crsChar = m_p-1;
          }
        }
      }
      else if( JS_TwoControl( c1, c0 ) )
      {
        m_fb.SetSyntaxStyle( m_l, m_p-1, HI_CONTROL );
        m_fb.SetSyntaxStyle( m_l, m_p  , HI_CONTROL );
      }
      else if( JS_OneVarType( c0 ) )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_VARTYPE );
      }
      else if( JS_OneControl( c0 ) )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_CONTROL );
      }
      else if( c0 < 32 || 126 < c0 )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_NONASCII );
      }
      if( St_JS_None != m_state ) return;
    }
    m_p = 0;
  }
  m_state = St_Done;
}

void Highlight_HTML::Hi_JS_Define()
{
  const unsigned LL = m_fb.LineLen( m_l );

  for( ; m_p<LL; m_p++ )
  {
    const char c1 = 0<m_p ? m_fb.Get( m_l, m_p-1 ) : m_fb.Get( m_l, m_p );
    const char c0 = 0<m_p ? m_fb.Get( m_l, m_p   ) : 0;

    if( c1=='/' && c0=='/' )
    {
      m_fb.SetSyntaxStyle( m_l, m_p-1, HI_COMMENT );
      m_fb.SetSyntaxStyle( m_l, m_p  , HI_COMMENT );
      m_p++;
      m_state = St_JS_CPP_Comment;
    }
    else if( c1=='/' && c0=='*' )
    {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_COMMENT );
      m_fb.SetSyntaxStyle( m_l, m_p, HI_COMMENT );
      m_p++;
      m_state = St_JS_C_Comment;
      m_ccXSt = St_JS_None;
    }
    else {
      m_fb.SetSyntaxStyle( m_l, m_p, HI_DEFINE );
    }
    if( St_JS_Define != m_state ) return;
  }
  m_p=0; m_l++;
  m_state = St_JS_None;
}
void Highlight_HTML::Hi_C_Comment()
{
  bool exit = false;
  for( ; m_l<m_fb.NumLines(); m_l++ )
  {
    const unsigned LL = m_fb.LineLen( m_l );

    for( ; m_p<LL; m_p++ )
    {
      const char c1 = 0<m_p ? m_fb.Get( m_l, m_p-1 ) : m_fb.Get( m_l, m_p );
      const char c0 = 0<m_p ? m_fb.Get( m_l, m_p   ) : 0;

      m_fb.SetSyntaxStyle( m_l, m_p, HI_COMMENT );

      if( c1=='*' && c0=='/' )
      {
        m_p++; //< Move past '/'
        m_state = m_ccXSt;
        exit = true;
      }
      if( exit ) return;
    }
    m_p = 0;
  }
  m_state = St_Done;
}
void Highlight_HTML::Hi_JS_CPP_Comment()
{
  const unsigned LL = m_fb.LineLen( m_l );

  for( ; m_p<LL; m_p++ )
  {
    m_fb.SetSyntaxStyle( m_l, m_p, HI_COMMENT );
  }
  m_l++;
  m_p=0;
  m_state = St_JS_None;
}

bool CS_OneVarType( const char c0 )
{
  return c0=='*' || c0=='#';
}
bool CS_OneControl( const char c0 )
{
  return c0=='.' || c0=='-' || c0==','
      || c0==':' || c0==';'
      || c0=='{' || c0=='}';
}

void Highlight_HTML::Hi_CS_None()
{
  for( ; m_l<m_fb.NumLines(); m_l++ )
  {
    const Line* lp = m_fb.GetLineP( m_l );
    const unsigned LL = m_fb.LineLen( m_l );

    for( ; m_p<LL; m_p++ )
    {
      m_fb.ClearSyntaxStyles( m_l, m_p );

      // c0 is ahead of c1 is ahead of c2: (c2,c1,c0)
      const char c2 = (1<m_p) ? m_fb.Get( m_l, m_p-2 ) : 0;
      const char c1 = (0<m_p) ? m_fb.Get( m_l, m_p-1 ) : 0;
      const char c0 =           m_fb.Get( m_l, m_p );

      if( c1=='/' && c0 == '*' )
      {
        m_p--;
        m_state = St_CS_C_Comment;
        m_ccXSt = St_CS_None;
      }
      else if( c0 == '\'')
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
        m_p++; //< Move past '\"'
        m_state = St_CS_SingleQuote;
        m_qtXSt = St_CS_None;
      }
      else if( c0 == '\"')
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_CONST );
        m_p++; //< Move past '\"'
        m_state = St_CS_DoubleQuote;
        m_qtXSt = St_CS_None;
      }
      else if( !IsIdent( c1 )
             && isdigit( c0 ))
      {
        // FIXME: For CSS, the following extensions should be highlighted:
        //        px, pt, %, in, em
        m_state = St_NumberBeg;
        m_numXSt = St_CS_None;
      }
      else if( c1=='<' && c0=='/' && m_p+6<LL )
      {
        if( 0==strncasecmp( lp->c_str(m_p-1), "</style", 7 ) )
        {
          m_fb.SetSyntaxStyle( m_l, m_p-1, HI_DEFINE );
          m_fb.SetSyntaxStyle( m_l, m_p  , HI_DEFINE );
          m_p++; // Move past '/'
          m_state = St_CloseTag;
          if( 0<m_CS_edges.len() ) { //< Should always be true
            Edges* edges = m_CS_edges.get( m_CS_edges.len()-1 );
            edges->end.crsLine = m_l;
            edges->end.crsChar = m_p-1;
          }
        }
      }
      else if( CS_OneVarType( c0 ) )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_VARTYPE );
      }
      else if( CS_OneControl( c0 ) )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_CONTROL );
      }
      else if( c0 < 32 || 126 < c0 )
      {
        m_fb.SetSyntaxStyle( m_l, m_p, HI_NONASCII );
      }
      if( St_CS_None != m_state ) return;
    }
    m_p = 0;
  }
  m_state = St_Done;
}

unsigned Highlight_HTML::Has_HTTP_Tag_At( const Line* lp, unsigned pos )
{
  if( IsXML_Ident( lp->get( pos ) ) )
  {
    const unsigned num_HTML_Tags = sizeof(m_HTML_Tags)/sizeof(char*);

    for( unsigned k=0; k<num_HTML_Tags; k++ )
    {
      const char* tag = m_HTML_Tags[k];
      const size_t tag_len = strlen(tag);

      if( 0==strncasecmp( lp->c_str(pos), tag, tag_len ) )
      {
        return tag_len;
      }
    }
  }
  return 0;
}

const char* Highlight_HTML::State_2_str( const Hi_State state )
{
  switch( state )
  {
  case St_In_None         : return "St_In_None"         ;
  case St_OpenTag_ElemName: return "St_OpenTag_ElemName";
  case St_OpenTag_AttrName: return "St_OpenTag_AttrName";
  case St_OpenTag_AttrVal : return "St_OpenTag_AttrVal ";
  case St_CloseTag        : return "St_CloseTag"        ;
  case St_XML_Comment     : return "St_XML_Comment"     ;
  case St_SingleQuote     : return "St_SingleQuote"     ;
  case St_DoubleQuote     : return "St_DoubleQuote"     ;
  case St_NumberBeg       : return "St_NumberBeg"       ;
  case St_NumberDec       : return "St_NumberDec"       ;
  case St_NumberHex       : return "St_NumberHex"       ;
  case St_NumberExponent  : return "St_NumberExponent"  ;
  case St_NumberFraction  : return "St_NumberFraction"  ;
  case St_NumberTypeSpec  : return "St_NumberTypeSpec"  ;

  case St_JS_None          : return "St_JS_None"          ;
  case St_JS_Define        : return "St_JS_Define"        ;
  case St_JS_C_Comment     : return "St_JS_C_Comment"     ;
  case St_JS_CPP_Comment   : return "St_JS_CPP_Comment"   ;
  case St_JS_SingleQuote   : return "St_JS_SingleQuote"   ;
  case St_JS_DoubleQuote   : return "St_JS_DoubleQuote"   ;
  case St_JS_NumberBeg     : return "St_JS_NumberBeg"     ;
  case St_JS_NumberDec     : return "St_JS_NumberDec"     ;
  case St_JS_NumberHex     : return "St_JS_NumberHex"     ;
  case St_JS_NumberExponent: return "St_JS_NumberExponent";
  case St_JS_NumberFraction: return "St_JS_NumberFraction";
  case St_JS_NumberTypeSpec: return "St_JS_NumberTypeSpec";

  case St_CS_None       : return "St_CS_None"       ;
  case St_CS_C_Comment  : return "St_CS_C_Comment"  ;
  case St_CS_SingleQuote: return "St_CS_SingleQuote";
  case St_CS_DoubleQuote: return "St_CS_DoubleQuote";

  case St_Done: return "St_Done";
  }
  return "Unknown";
}

HiKeyVal Highlight_HTML::m_JS_HiPairs[] =
{
  // Keywords:
  { "break"     , HI_CONTROL },
  { "break"     , HI_CONTROL },
  { "catch"     , HI_CONTROL },
  { "case"      , HI_CONTROL },
  { "continue"  , HI_CONTROL },
  { "debugger"  , HI_CONTROL },
  { "default"   , HI_CONTROL },
  { "delete"    , HI_CONTROL },
  { "do"        , HI_CONTROL },
  { "else"      , HI_CONTROL },
  { "finally"   , HI_CONTROL },
  { "for"       , HI_CONTROL },
  { "function"  , HI_CONTROL },
  { "if"        , HI_CONTROL },
  { "in"        , HI_CONTROL },
  { "instanceof", HI_CONTROL },
  { "new"       , HI_VARTYPE },
  { "return"    , HI_CONTROL },
  { "switch"    , HI_CONTROL },
  { "throw"     , HI_CONTROL },
  { "try"       , HI_CONTROL },
  { "typeof"    , HI_VARTYPE },
  { "var"       , HI_VARTYPE },
  { "void"      , HI_VARTYPE },
  { "while"     , HI_CONTROL },
  { "with"      , HI_CONTROL },

  // Keywords in strict mode:
  { "implements", HI_CONTROL },
  { "interface" , HI_CONTROL },
  { "let"       , HI_VARTYPE },
  { "package"   , HI_DEFINE  },
  { "private"   , HI_CONTROL },
  { "protected" , HI_CONTROL },
  { "public"    , HI_CONTROL },
  { "static"    , HI_VARTYPE },
  { "yield"     , HI_CONTROL },

  // Constants:
  { "false", HI_CONST },
  { "null" , HI_CONST },
  { "true" , HI_CONST },

  // Global variables and functions:
  { "arguments"         , HI_VARTYPE },
  { "Array"             , HI_VARTYPE },
  { "Boolean"           , HI_VARTYPE },
  { "Date"              , HI_CONTROL },
  { "decodeURI"         , HI_CONTROL },
  { "decodeURIComponent", HI_CONTROL },
  { "encodeURI"         , HI_CONTROL },
  { "encodeURIComponent", HI_CONTROL },
  { "Error"             , HI_VARTYPE },
  { "eval"              , HI_CONTROL },
  { "EvalError"         , HI_CONTROL },
  { "Function"          , HI_CONTROL },
  { "Infinity"          , HI_CONST   },
  { "isFinite"          , HI_CONTROL },
  { "isNaN"             , HI_CONTROL },
  { "JSON"              , HI_CONTROL },
  { "Math"              , HI_CONTROL },
  { "NaN"               , HI_CONST   },
  { "Number"            , HI_VARTYPE },
  { "Object"            , HI_VARTYPE },
  { "parseFloat"        , HI_CONTROL },
  { "parseInt"          , HI_CONTROL },
  { "RangeError"        , HI_VARTYPE },
  { "ReferenceError"    , HI_VARTYPE },
  { "RegExp"            , HI_CONTROL },
  { "String"            , HI_VARTYPE },
  { "SyntaxError"       , HI_VARTYPE },
  { "TypeError"         , HI_VARTYPE },
  { "undefined"         , HI_CONST   },
  { "URIError"          , HI_VARTYPE },
};

