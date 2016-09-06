////////////////////////////////////////////////////////////////////////////////
// VI-Simplified (vis) C++ Implementation                                     //
// Copyright (c) 05 Sep 2016 Paul J. Gartside                                 //
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
#include "Highlight_CMake.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

Highlight_CMake::Highlight_CMake( FileBuf& rfb )
  : Highlight_Base( rfb )
  , m_state( &ME::Hi_In_None )
{
}

void Highlight_CMake::Run_Range( const CrsPos st, const unsigned fn )
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

void Highlight_CMake::Hi_In_None( unsigned& l, unsigned& p )
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
      else if( 0==strncmp( s, "`",  1 ) ) { m_state = &ME::Hi_96_Quote; }
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
            || s[0]=='/' || s[0]=='|'
            || s[0]=='@' || s[0]=='^' ) { m_fb.SetSyntaxStyle( l, p, HI_CONTROL ); }

      else if( s[0] < 32 || 126 < s[0] ) { m_fb.SetSyntaxStyle( l, p, HI_NONASCII ); }
      else if( LL-1 == p && s[0]=='\\')  { m_fb.SetSyntaxStyle( l, p, HI_DEFINE ); }

      if( &ME::Hi_In_None != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_CMake::Hi_In_Comment( unsigned& l, unsigned& p )
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

void Highlight_CMake::Hi_SingleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_fb.SetSyntaxStyle( l, p, HI_CONST ); p++;

  for( ; l<m_fb.NumLines(); l++ )
  {
    const unsigned LL = m_fb.LineLen( l );

    bool slash_escaped = false;
    for( ; p<LL; p++ )
    {
      // c0 is ahead of c1: c1,c0
      const char c1 = p ? m_fb.Get( l, p-1 ) : m_fb.Get( l, p );
      const char c0 = p ? m_fb.Get( l, p   ) : 0;

      if( (c1=='\'' && c0==0   )
       || (c1!='\\' && c0=='\'')
       || (c1=='\\' && c0=='\'' && slash_escaped) )
      {
        m_fb.SetSyntaxStyle( l, p, HI_CONST ); p++;
        m_state = &ME::Hi_In_None;
      }
      else {
        if( c1=='\\' && c0=='\\' ) slash_escaped = true;
        else                       slash_escaped = false;

        m_fb.SetSyntaxStyle( l, p, HI_CONST );
      }
      if( &ME::Hi_SingleQuote != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_CMake::Hi_DoubleQuote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_fb.SetSyntaxStyle( l, p, HI_CONST ); p++;

  for( ; l<m_fb.NumLines(); l++ )
  {
    const unsigned LL = m_fb.LineLen( l );

    bool slash_escaped = false;
    for( ; p<LL; p++ )
    {
      // c0 is ahead of c1: c1,c0
      const char c1 = p ? m_fb.Get( l, p-1 ) : m_fb.Get( l, p );
      const char c0 = p ? m_fb.Get( l, p   ) : 0;

      if( (c1=='\"' && c0==0   )
       || (c1!='\\' && c0=='\"')
       || (c1=='\\' && c0=='\"' && slash_escaped) )
      {
        m_fb.SetSyntaxStyle( l, p, HI_CONST ); p++;
        m_state = &ME::Hi_In_None;
      }
      else {
        if( c1=='\\' && c0=='\\' ) slash_escaped = true;
        else                       slash_escaped = false;

        m_fb.SetSyntaxStyle( l, p, HI_CONST );
      }
      if( &ME::Hi_DoubleQuote != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_CMake::Hi_96_Quote( unsigned& l, unsigned& p )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_fb.SetSyntaxStyle( l, p, HI_CONST ); p++;

  for( ; l<m_fb.NumLines(); l++ )
  {
    const unsigned LL = m_fb.LineLen( l );

    bool slash_escaped = false;
    for( ; p<LL; p++ )
    {
      // c0 is ahead of c1: c1,c0
      const char c1 = p ? m_fb.Get( l, p-1 ) : m_fb.Get( l, p );
      const char c0 = p ? m_fb.Get( l, p   ) : 0;

      if( (c1=='`'  && c0==0  )
       || (c1!='\\' && c0=='`')
       || (c1=='\\' && c0=='`' && slash_escaped) )
     //|| (c1=='\'' && c0==0   )
     //|| (c1!='\\' && c0=='\'')
     //|| (c1=='\\' && c0=='\'' && slash_escaped) )
      {
        m_fb.SetSyntaxStyle( l, p, HI_CONST ); p++;
        m_state = &ME::Hi_In_None;
      }
      else {
        if( c1=='\\' && c0=='\\' ) slash_escaped = true;
        else                       slash_escaped = false;

        m_fb.SetSyntaxStyle( l, p, HI_CONST );
      }
      if( &ME::Hi_96_Quote != m_state ) return;
    }
    p = 0;
  }
  m_state = 0;
}

void Highlight_CMake::Hi_NumberBeg( unsigned& l, unsigned& p )
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

void Highlight_CMake::Hi_NumberIn( unsigned& l, unsigned& p )
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

void Highlight_CMake::Hi_NumberHex( unsigned& l, unsigned& p )
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

void Highlight_CMake::Hi_NumberFraction( unsigned& l, unsigned& p )
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

void Highlight_CMake::Hi_NumberExponent( unsigned& l, unsigned& p )
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
  { "add_compile_options"          , HI_CONTROL },
  { "add_custom_command"           , HI_CONTROL },
  { "add_custom_target"            , HI_CONTROL },
  { "add_definitions"              , HI_CONTROL },
  { "add_dependencies"             , HI_CONTROL },
  { "add_executable"               , HI_CONTROL },
  { "add_library"                  , HI_CONTROL },
  { "add_subdirectory"             , HI_CONTROL },
  { "add_test"                     , HI_CONTROL },
  { "aux_source_directory"         , HI_CONTROL },
  { "break"                        , HI_CONTROL },
  { "build_command"                , HI_CONTROL },
  { "cmake_host_system_information", HI_CONTROL },
  { "cmake_minimum_required"       , HI_CONTROL },
  { "cmake_parse_arguments"        , HI_CONTROL },
  { "cmake_policy"                 , HI_CONTROL },
  { "configure_file"               , HI_CONTROL },
  { "continue"                     , HI_CONTROL },
  { "create_test_sourcelist"       , HI_CONTROL },
  { "define_property"              , HI_CONTROL },
  { "elseif"                       , HI_CONTROL },
  { "else"                         , HI_CONTROL },
  { "enable_language"              , HI_CONTROL },
  { "enable_testing"               , HI_CONTROL },
  { "endforeach"                   , HI_CONTROL },
  { "endfunction"                  , HI_CONTROL },
  { "endif"                        , HI_CONTROL },
  { "endmacro"                     , HI_CONTROL },
  { "endwhile"                     , HI_CONTROL },
  { "execute_process"              , HI_CONTROL },
  { "export"                       , HI_CONTROL },
  { "file"                         , HI_CONTROL },
  { "find_file"                    , HI_CONTROL },
  { "find_library"                 , HI_CONTROL },
  { "find_package"                 , HI_CONTROL },
  { "find_path"                    , HI_CONTROL },
  { "find_program"                 , HI_CONTROL },
  { "fltk_wrap_ui"                 , HI_CONTROL },
  { "foreach"                      , HI_CONTROL },
  { "function"                     , HI_CONTROL },
  { "get_cmake_property"           , HI_CONTROL },
  { "get_directory_property"       , HI_CONTROL },
  { "get_filename_component"       , HI_CONTROL },
  { "get_property"                 , HI_CONTROL },
  { "get_source_file_property"     , HI_CONTROL },
  { "get_target_property"          , HI_CONTROL },
  { "get_test_property"            , HI_CONTROL },
  { "if"                           , HI_CONTROL },
  { "include_directories"          , HI_CONTROL },
  { "include_external_msproject"   , HI_CONTROL },
  { "include_regular_expression"   , HI_CONTROL },
  { "include"                      , HI_CONTROL },
  { "install"                      , HI_CONTROL },
  { "link_directories"             , HI_CONTROL },
  { "link_libraries"               , HI_CONTROL },
  { "list"                         , HI_CONTROL },
  { "load_cache"                   , HI_CONTROL },
  { "macro"                        , HI_CONTROL },
  { "mark_as_advanced"             , HI_CONTROL },
  { "math"                         , HI_CONTROL },
  { "message"                      , HI_CONTROL },
  { "option"                       , HI_CONTROL },
  { "project"                      , HI_CONTROL },
  { "qt_wrap_cpp"                  , HI_CONTROL },
  { "qt_wrap_ui"                   , HI_CONTROL },
  { "remove_definitions"           , HI_CONTROL },
  { "return"                       , HI_CONTROL },
  { "separate_arguments"           , HI_CONTROL },
  { "set_directory_properties"     , HI_CONTROL },
  { "set_property"                 , HI_CONTROL },
  { "set"                          , HI_CONTROL },
  { "set_source_files_properties"  , HI_CONTROL },
  { "set_target_properties"        , HI_CONTROL },
  { "set_tests_properties"         , HI_CONTROL },
  { "site_name"                    , HI_CONTROL },
  { "source_group"                 , HI_CONTROL },
  { "string"                       , HI_CONTROL },
  { "target_compile_definitions"   , HI_CONTROL },
  { "target_compile_features"      , HI_CONTROL },
  { "target_compile_options"       , HI_CONTROL },
  { "target_include_directories"   , HI_CONTROL },
  { "target_link_libraries"        , HI_CONTROL },
  { "target_sources"               , HI_CONTROL },
  { "try_compile"                  , HI_CONTROL },
  { "try_run"                      , HI_CONTROL },
  { "unset"                        , HI_CONTROL },
  { "variable_watch"               , HI_CONTROL },
  { "while"                        , HI_CONTROL },
  { 0 }
};

void Highlight_CMake::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Hi_FindKey_In_Range( HiPairs, st, fn );
}

