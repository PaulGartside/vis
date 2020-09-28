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

#include <ctype.h>     // is(alnum|punct|space|print|lower...)
#include <string.h>    // memcpy, memset
#include <unistd.h>    // readlink
#include <sys/stat.h>  // lstat
#include <stdio.h>     // printf, stderr, FILE, fopen, fclose
#include <dirent.h>

#ifdef USE_REGEX
#include <regex>
#endif

#include "String.hh"
#include "ChangeHist.hh"
#include "Console.hh"
#include "Key.hh"
#include "Utilities.hh"
#include "MemLog.hh"
#include "View.hh"
#include "LineView.hh"
#include "Vis.hh"
#include "FileBuf.hh"
#include "Highlight_Bash.hh"
#include "Highlight_BufferEditor.hh"
#include "Highlight_CMake.hh"
#include "Highlight_CPP.hh"
#include "Highlight_Dir.hh"
#include "Highlight_HTML.hh"
#include "Highlight_IDL.hh"
#include "Highlight_Java.hh"
#include "Highlight_JS.hh"
#include "Highlight_Make.hh"
#include "Highlight_MIB.hh"
#include "Highlight_ODB.hh"
#include "Highlight_Python.hh"
#include "Highlight_SQL.hh"
#include "Highlight_STL.hh"
#include "Highlight_Swift.hh"
#include "Highlight_TCL.hh"
#include "Highlight_Text.hh"
#include "Highlight_XML.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

extern const unsigned USER_FILE;  // First user file

extern const char* EDIT_BUF_NAME;
extern const char* HELP_BUF_NAME;
extern const char* MSG__BUF_NAME;
extern const char* SHELL_BUF_NAME;
extern const char* COLON_BUF_NAME;
extern const char* SLASH_BUF_NAME;

struct FileBuf::Data
{
  Data( FileBuf& parent
      , Vis& vis
      , const char* const FILE_NAME
      , const bool MUTABLE
      , const File_Type FT );

  Data( FileBuf& parent
      , Vis& vis
      , const char* const FILE_NAME
      , const FileBuf& rfb );

  ~Data();

  FileBuf&        self;
  Vis&            vis;
  Highlight_Base* pHi;
  ChangeHist      history;
  String          path_name; // Full path and filename = dir_name + file_name
  String          dir_name;  // Directory = path_name - file_name, (for directories this is the same a path_name)
  String          file_name; // Filename = path_name - dir_name, (for directories this is empty)
  const bool      is_dir;
  const bool      is_regular;
  double          mod_time;
  double          foc_time;
  bool            changed_externally;
  ViewList        views;     // List of views that display this file
  LineView*       line_view;
#ifdef USE_REGEX
  std::cmatch     cm;
#endif
  String          regex;
  boolList        lineRegexsValid;

  bool       save_history;
  unsList    lineOffsets; // absolute byte offset of beginning of line in file
  LinesList  lines;    // list of file lines.
  LinesList  styles;   // list of file styles.
  unsigned   hi_touched_line; // Line before which highlighting is valid
  bool       LF_at_EOF; // Line feed at end of file
  File_Type  file_type;
  const bool m_mutable; // mutable is used by preprocessor, so use m_mutable instead
  Line       line_buf;
  Encoding   decoding;
  Encoding   encoding;
};

FileBuf::Data::Data( FileBuf& parent
                   , Vis& vis
                   , const char* const FILE_NAME
                   , const bool MUTABLE
                   , const File_Type FT )
  : self( parent )
  , vis( vis )
  , pHi( 0 )
  , history( vis, parent )
  , path_name( FILE_NAME )
  , dir_name( "" )
  , file_name( "" )
  , is_dir( ::IsDir( path_name.c_str() ) )
  , is_regular( ::IsReg( path_name.c_str() ) )
  , mod_time( 0 )
  , foc_time( 0 )
  , changed_externally( false )
  , views()
  , line_view( 0 )
#ifdef USE_REGEX
  , cm()
#endif
  , regex()
  , lineRegexsValid()
  , save_history( false )
  , lineOffsets()
  , lines()
  , styles()
  , hi_touched_line( 0 )
  , LF_at_EOF( true )
  , file_type( FT )
  , m_mutable( MUTABLE )
  , decoding( ENC_BYTE )
  , encoding( ENC_BYTE )
{
  if( is_dir )
  {
    dir_name = path_name;
    if( DIR_DELIM != path_name.get_end() ) path_name.push( DIR_DELIM );
  }
  else {
    dir_name = GetFnameTail( path_name.c_str() );
    file_name = GetFnameHead( path_name.c_str() );
  }
}

FileBuf::Data::Data( FileBuf& parent
                   , Vis& vis
                   , const char* const FILE_NAME
                   , const FileBuf& rfb )
  : self( parent )
  , vis( vis )
  , path_name( FILE_NAME )
  , dir_name( "" )
  , file_name( "" )
  , is_dir( rfb.m.is_dir )
  , is_regular( rfb.m.is_regular )
  , mod_time( rfb.m.mod_time )
  , foc_time( rfb.m.foc_time )
  , changed_externally( rfb.m.changed_externally )
  , views()
  , line_view( 0 )
#ifdef USE_REGEX
  , cm()
#endif
  , regex()
  , lineRegexsValid()
  , history( vis, parent )
  , save_history( is_dir ? false : true )
  , lineOffsets ( rfb.m.lineOffsets )
  , lines()
  , styles()
  , hi_touched_line( 0 )
  , LF_at_EOF( rfb.m.LF_at_EOF )
  , file_type( rfb.m.file_type )
  , pHi( 0 )
  , m_mutable( true )
  , decoding( rfb.m.decoding )
  , encoding( rfb.m.encoding )
{
  if( is_dir )
  {
    dir_name = path_name;
    if( DIR_DELIM != path_name.get_end() ) path_name.push( DIR_DELIM );
  }
  else {
    dir_name = GetFnameTail( path_name.c_str() );
    file_name = GetFnameHead( path_name.c_str() );
  }
}

FileBuf::Data::~Data()
{
}

bool Find_File_Type_Bash( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".sh"      )
   || m.path_name.ends_with(".sh.new"  )
   || m.path_name.ends_with(".sh.old"  )
   || m.path_name.ends_with(".bash"    )
   || m.path_name.ends_with(".bash.new")
   || m.path_name.ends_with(".bash.old")
   || m.path_name.ends_with(".alias"   )
   || m.path_name.ends_with(".bashrc"  )
   || m.path_name.ends_with(".profile" )
   || m.path_name.ends_with(".bash_profile")
   || m.path_name.ends_with(".bash_logout" ) )
  {
    m.file_type = FT_BASH;
    m.pHi = new(__FILE__,__LINE__) Highlight_Bash( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_CPP( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".h"      )
   || m.path_name.ends_with(".h.new"  )
   || m.path_name.ends_with(".h.old"  )
   || m.path_name.ends_with(".c"      )
   || m.path_name.ends_with(".c.new"  )
   || m.path_name.ends_with(".c.old"  )
   || m.path_name.ends_with(".hh"     )
   || m.path_name.ends_with(".hh.new" )
   || m.path_name.ends_with(".hh.old" )
   || m.path_name.ends_with(".cc"     )
   || m.path_name.ends_with(".cc.new" )
   || m.path_name.ends_with(".cc.old" )
   || m.path_name.ends_with(".hpp"    )
   || m.path_name.ends_with(".hpp.new")
   || m.path_name.ends_with(".hpp.old")
   || m.path_name.ends_with(".cpp"    )
   || m.path_name.ends_with(".cpp.new")
   || m.path_name.ends_with(".cpp.old")
   || m.path_name.ends_with(".cxx"    )
   || m.path_name.ends_with(".cxx.new")
   || m.path_name.ends_with(".cxx.old") )
  {
    m.file_type = FT_CPP;
    m.pHi = new(__FILE__,__LINE__) Highlight_CPP( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_IDL( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".idl"    )
   || m.path_name.ends_with(".idl.new")
   || m.path_name.ends_with(".idl.old")
   || m.path_name.ends_with(".idl.in"    )
   || m.path_name.ends_with(".idl.in.new")
   || m.path_name.ends_with(".idl.in.old") )
  {
    m.file_type = FT_IDL;
    m.pHi = new(__FILE__,__LINE__) Highlight_IDL( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_Java( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".java"    )
   || m.path_name.ends_with(".java.new")
   || m.path_name.ends_with(".java.old") )
  {
    m.file_type = FT_JAVA;
    m.pHi = new(__FILE__,__LINE__) Highlight_Java( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_HTML( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".htm"     )
   || m.path_name.ends_with(".htm.new" )
   || m.path_name.ends_with(".htm.old" )
   || m.path_name.ends_with(".html"    )
   || m.path_name.ends_with(".html.new")
   || m.path_name.ends_with(".html.old") )
  {
    m.file_type = FT_HTML;
    m.pHi = new(__FILE__,__LINE__) Highlight_HTML( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_XML( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".xml"       )
   || m.path_name.ends_with(".xml.new"   )
   || m.path_name.ends_with(".xml.old"   )
   || m.path_name.ends_with(".xml.in"    )
   || m.path_name.ends_with(".xml.in.new")
   || m.path_name.ends_with(".xml.in.old") )
  {
    m.file_type = FT_XML;
    m.pHi = new(__FILE__,__LINE__) Highlight_XML( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_JS( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".js"    )
   || m.path_name.ends_with(".js.new")
   || m.path_name.ends_with(".js.old") )
  {
    m.file_type = FT_JS;
    m.pHi = new(__FILE__,__LINE__) Highlight_JS( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_Make( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".Make"       )
   || m.path_name.ends_with(".make"       )
   || m.path_name.ends_with(".Make.new"   )
   || m.path_name.ends_with(".make.new"   )
   || m.path_name.ends_with(".Make.old"   )
   || m.path_name.ends_with(".make.old"   )
   || m.path_name.ends_with("Makefile"    )
   || m.path_name.ends_with("makefile"    )
   || m.path_name.ends_with("Makefile.new")
   || m.path_name.ends_with("makefile.new")
   || m.path_name.ends_with("Makefile.old")
   || m.path_name.ends_with("makefile.old") )
  {
    m.file_type = FT_MAKE;
    m.pHi = new(__FILE__,__LINE__) Highlight_Make( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_MIB( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".mib"    )
   || m.path_name.ends_with(".mib.new")
   || m.path_name.ends_with(".mib.old")
   || m.path_name.ends_with("-MIB.txt")
   || m.path_name.ends_with("-MIB.txt.new")
   || m.path_name.ends_with("-MIB.txt.old") )
  {
    m.file_type = FT_MIB;
    m.pHi = new(__FILE__,__LINE__) Highlight_MIB( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_CMake( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".cmake"    )
   || m.path_name.ends_with(".cmake.new")
   || m.path_name.ends_with(".cmake.old")
   || m.path_name.ends_with("CMakeLists.txt"    )
   || m.path_name.ends_with("CMakeLists.new.txt")
   || m.path_name.ends_with("CMakeLists.old.txt") )
  {
    m.file_type = FT_CMAKE;
    m.pHi = new(__FILE__,__LINE__) Highlight_CMake( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_Python( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".py"    )
   || m.path_name.ends_with(".py.new")
   || m.path_name.ends_with(".py.old") )
  {
    m.file_type = FT_PY;
    m.pHi = new(__FILE__,__LINE__) Highlight_Python( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_SQL( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".sql"    )
   || m.path_name.ends_with(".sql.new")
   || m.path_name.ends_with(".sql.old") )
  {
    m.file_type = FT_SQL;
    m.pHi = new(__FILE__,__LINE__) Highlight_SQL( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_STL( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".stl"    )
   || m.path_name.ends_with(".stl.new")
   || m.path_name.ends_with(".stl.old")
   || m.path_name.ends_with(".ste"    )
   || m.path_name.ends_with(".ste.new")
   || m.path_name.ends_with(".ste.old") )
  {
    m.file_type = FT_STL;
    m.pHi = new(__FILE__,__LINE__) Highlight_STL( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_ODB( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".odb"    )
   || m.path_name.ends_with(".odb.new")
   || m.path_name.ends_with(".odb.old") )
  {
    m.file_type = FT_ODB;
    m.pHi = new(__FILE__,__LINE__) Highlight_ODB( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_Swift( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".swift"    )
   || m.path_name.ends_with(".swift.new")
   || m.path_name.ends_with(".swift.old") )
  {
    m.file_type = FT_SWIFT;
    m.pHi = new(__FILE__,__LINE__) Highlight_Swift( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_TCL( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.path_name.ends_with(".tcl"    )
   || m.path_name.ends_with(".tcl.new")
   || m.path_name.ends_with(".tcl.old") )
  {
    m.file_type = FT_TCL;
    m.pHi = new(__FILE__,__LINE__) Highlight_TCL( m.self );
    return true;
  }
  return false;
}

void Find_File_Type_Suffix( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.is_dir )
  {
    m.file_type = FT_DIR;
    m.pHi = new(__FILE__,__LINE__) Highlight_Dir( m.self );
  }
  else if( Find_File_Type_Bash  ( m )
        || Find_File_Type_CPP   ( m )
        || Find_File_Type_IDL   ( m )
        || Find_File_Type_Java  ( m )
        || Find_File_Type_HTML  ( m )
        || Find_File_Type_XML   ( m )
        || Find_File_Type_JS    ( m )
        || Find_File_Type_Make  ( m )
        || Find_File_Type_MIB   ( m )
        || Find_File_Type_CMake ( m )
        || Find_File_Type_ODB   ( m )
        || Find_File_Type_Python( m )
        || Find_File_Type_SQL   ( m )
        || Find_File_Type_STL   ( m )
        || Find_File_Type_Swift ( m )
        || Find_File_Type_TCL   ( m ) )
  {
    // File type found
  }
  else {
    // File type NOT found based on suffix.
    // File type will be found in Find_File_Type_FirstLine()
  }
}

void Find_File_Type_FirstLine( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.self.NumLines();

  if( 0 < NUM_LINES )
  {
    const Line* lp0 = m.self.GetLineP( 0 );

    const unsigned LL0 = lp0->len();
    if( 0 < LL0 )
    {
      const char* s = lp0->c_str( 0 );

      static const char* p1 = "#!/bin/bash";
      static const char* p2 = "#!/bin/sh";
      static const char* p3 = "#!/usr/bin/bash";
      static const char* p4 = "#!/usr/bin/sh";
      static const unsigned p1_len = strlen( p1 );
      static const unsigned p2_len = strlen( p2 );
      static const unsigned p3_len = strlen( p3 );
      static const unsigned p4_len = strlen( p4 );

      if( (p1_len<=LL0 && 0==strncmp( s, p1, p1_len ))
       || (p2_len<=LL0 && 0==strncmp( s, p2, p2_len ))
       || (p1_len<=LL0 && 0==strncmp( s, p3, p3_len ))
       || (p2_len<=LL0 && 0==strncmp( s, p4, p4_len )) )
      {
        m.file_type = FT_BASH;
        m.pHi = new(__FILE__,__LINE__) Highlight_Bash( m.self );
      }
    }
  }
  if( FT_UNKNOWN == m.file_type )
  {
    // File type NOT found, so default to TEXT
    m.file_type = FT_TEXT;
    m.pHi = new(__FILE__,__LINE__) Highlight_Text( m.self );
  }
}

bool SavingHist( FileBuf::Data& m )
{
  return m.m_mutable && m.save_history;
}

void ChangedLine( FileBuf::Data& m, const unsigned line_num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // HVLO = Highest valid line offset
  unsigned HVLO = 0;

  if( 0<line_num && 0<m.lineOffsets.len() )
  {
    HVLO = Min( line_num-1, m.lineOffsets.len()-1 );
  }
  m.lineOffsets.set_len( HVLO );

  m.hi_touched_line = Min( m.hi_touched_line, line_num );
}

void SwapLines( FileBuf::Data& m
              , const unsigned l_num_1
              , const unsigned l_num_2 )
{
  Trace trace( __PRETTY_FUNCTION__ );
  bool ok =  m.lines.swap( l_num_1, l_num_2 )
         && m.styles.swap( l_num_1, l_num_2 );

  ASSERT( __LINE__, ok, "ok" );

  if( SavingHist( m ) ) m.history.Save_SwapLines( l_num_1, l_num_2 );

  ChangedLine( m, Min( l_num_1, l_num_2 ) );
}

void ReadExistingFile( FileBuf::Data& m, FILE* fp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, fp, "fp" );

  Line* lp = 0;
  int C = 0;
  while( EOF != (C = fgetc( fp )) )
  {
    if( 0==lp ) lp = m.vis.BorrowLine( __FILE__,__LINE__ );

    if( '\n' == C )
    {
      m.self.PushLine( lp ); //< FileBuf::lines takes ownership of lp
      lp = 0;
      m.LF_at_EOF = true;
    }
    else {
      bool ok = lp->push( C );
      if( !ok ) DIE("Line.push() failed");
      m.LF_at_EOF = false;
    }
  }
  if( lp ) m.self.PushLine( lp ); //< FileBuf::lines takes ownership of lp

  m.save_history = true;

  if( FT_UNKNOWN == m.file_type )
  {
    Find_File_Type_FirstLine( m );
  }
}

// Add byte C to the end of line l_num
//
void Append_DirDelim( FileBuf::Data& m, const unsigned l_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  m.lines.len(), "l_num < m.lines.len()" );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* lp =  m.lines[ l_num ];
  Line* sp = m.styles[ l_num ];

  if( !lp->ends_with( DIR_DELIM ) )
  {
    bool ok = lp->push( DIR_DELIM )
           && sp->push( 0 );

    ASSERT( __LINE__, ok, "ok" );

    ChangedLine( m, l_num );

    if( SavingHist( m ) ) m.history.Save_InsertChar( l_num, lp->len()-1 );
  }
}

// Add symbolic link info, i.e., -> symbolic_link_path, to file name
//
void ReadExistingDir_AddLink( FileBuf::Data& m
                            , const String& dir_path_fname
                            , const unsigned LINE_NUM )
{
#ifndef WIN32
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned mbuf_sz = 1024;
  char mbuf[ 1024 ];
  int rval = readlink( dir_path_fname.c_str(), mbuf, mbuf_sz );
  if( 0 < rval )
  {
    m.self.PushChar( LINE_NUM, ' ');
    m.self.PushChar( LINE_NUM, '-');
    m.self.PushChar( LINE_NUM, '>');
    m.self.PushChar( LINE_NUM, ' ');

    for( unsigned k=0; k<rval; k++ )
    {
      m.self.PushChar( LINE_NUM, mbuf[k] );
    }
    if( rval < 1024 )
    {
      mbuf[ rval ] = 0;

      if( IsDir( mbuf ) ) Append_DirDelim( m, LINE_NUM );
    }
  }
#endif
}

void ReadExistingDir_Sort( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.self.NumLines();

  // Sort lines (file names), least to greatest:
  for( unsigned i=NUM_LINES-1; 0<i; i-- )
  {
    for( unsigned k=0; k<i; k++ )
    {
      const Line& l_0 = *m.lines[k  ];
      const Line& l_1 = *m.lines[k+1];

      if( l_0.gt( l_1 ) ) SwapLines( m, k, k+1 );
    }
  }

  // Move non-directory files to end:
  for( unsigned i=NUM_LINES-1; 0<i; i-- )
  {
    for( unsigned k=0; k<i; k++ )
    {
      const String& sl = m.lines[k]->toString();

      if( (sl != "..") && (sl != ".") )
      {
        Line* lp1 = m.lines[k  ];
        Line* lp2 = m.lines[k+1];

        if( !lp1->ends_with( DIR_DELIM )
         &&  lp2->ends_with( DIR_DELIM ) ) SwapLines( m, k, k+1 );
      }
    }
  }
}

void ReadExistingDir( FileBuf::Data& m, DIR* dp, String dir_path )
{
  Trace trace( __PRETTY_FUNCTION__ );
  // Make sure dir_path ends in '/'
  if( DIR_DELIM != dir_path.get_end() ) dir_path.push( DIR_DELIM );

  String dir_path_fname;
  while( dirent* de = readdir( dp ) )
  {
    const char*    fname     = de->d_name;
    const unsigned fname_len = strlen( fname );

    // Dont add "." to list of files
    if( fname_len && strcmp(".", fname) )
    {
      const unsigned LINE_NUM = m.self.NumLines();
      m.self.PushLine();

      for( unsigned k=0; k<fname_len; k++ )
      {
        m.self.PushChar( LINE_NUM, fname[k] );
      }
      bool IS_DIR = 0; // Line added is name of directory, but not '..'
      bool IS_LNK = 0; // Line added is a symbolic link
      // Put a slash on the end of the fname if it is a directory, and not '..':
      if( strcmp( "..", fname ) )
      {
        dir_path_fname = dir_path;
        dir_path_fname += fname;
        struct stat stat_buf ;
        int err = my_stat( dir_path_fname.c_str(), stat_buf );
        IS_DIR = 0==err && S_ISDIR( stat_buf.st_mode );
#ifndef WIN32
        IS_LNK = 0==err && S_ISLNK( stat_buf.st_mode );
#endif
        if     ( IS_DIR ) Append_DirDelim( m, LINE_NUM );
        else if( IS_LNK ) ReadExistingDir_AddLink( m, dir_path_fname, LINE_NUM );
      }
    }
  }
  ReadExistingDir_Sort( m );
}

// Get byte on line l_num at end of line
//
uint8_t GetEnd( FileBuf::Data& m, const unsigned l_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.lines.len(), "l_num < m.lines.len()" );

  Line* lp = m.lines[ l_num ];

  ASSERT( __LINE__, lp->len(), "lp->len()" );

  return lp->get(lp->len()-1);
}

void Adjust_View_topLine( FileBuf::Data& m
                        , View_IF* pV
                        , const unsigned l_num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned top_line = pV->GetTopLine();

  if( l_num < top_line )
  {
    // Line removed was above views top line, so decrement views
    // top line so the view will appear to be in the same position:
    pV->SetTopLine( top_line-1 );
  }
  if( m.lines.len() <= pV->CrsLine() )
  {
    // Cursor was on last line before a line was removed, so
    // decrement cursor line by either decrementing cursor row,
    // or decrementing top line
    const unsigned crs_row  = pV->GetCrsRow();
    const unsigned top_line = pV->GetTopLine();

    // Only one line is removed at a time, so just decrementing should work:
    if     ( 0 < crs_row  ) pV->SetCrsRow( crs_row-1 );
    else if( 0 < top_line ) pV->SetTopLine( top_line-1 );
  }
}

void RemoveLine_Adjust_Views_topLines( FileBuf::Data& m, const unsigned l_num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned w=0; w<MAX_WINS && w<m.views.len(); w++ )
  {
    View* pV = m.views[w];

    Adjust_View_topLine( m, pV, l_num );
  }
  if( 0 != m.line_view )
  {
    LineView* pV = m.line_view;

    Adjust_View_topLine( m, pV, l_num );
  }
}

CrsPos Update_Styles_Find_St( FileBuf::Data& m, const unsigned first_line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Default start position is beginning of file
  CrsPos st = { 0, 0 };

  // 1. Find first position without a style before first line
  bool done = false;
  for( int l=first_line-1; !done && 0<=l; l-- )
  {
    const int LL = m.self.LineLen( l );
    for( int p=LL-1; !done && 0<=p; p-- )
    {
      const uint8_t S = m.styles[l]->get(p);
      if( !S ) {
        st.crsLine = l;
        st.crsChar = p;
        done = true;
      }
    }
  }
  return st;
}

// Find m.styles starting at st up to but not including fn line number
void Find_Styles_In_Range( FileBuf::Data& m, const CrsPos st, const int fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pHi should have already have been allocated, but just in case
  // check and allocate here if needed.
  if( 0 == m.pHi )
  {
    m.file_type = FT_TEXT;
    m.pHi = new(__FILE__,__LINE__) Highlight_Text( m.self );
  }
  m.pHi->Run_Range( st, fn );
}

// Clear all m.styles includeing star and syntax
void ClearAllStyles( FileBuf::Data& m
                   , const unsigned l_num
                   , const unsigned c_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* sp = m.styles[ l_num ];

  ASSERT( __LINE__, c_num < sp->len(), "c_num < sp->len()" );

  sp->set( c_num, 0 );
}

// Leave syntax m.styles unchanged, and set star style
void Set__StarStyle( FileBuf::Data& m
                   , const unsigned l_num
                   , const unsigned c_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* sp = m.styles[ l_num ];

  ASSERT( __LINE__, c_num < sp->len(), "c_num < sp->len()" );

  sp->set( c_num, sp->get( c_num ) | HI_STAR );
}

// Leave syntax m.styles unchanged, and clear star style
void ClearStarStyle( FileBuf::Data& m
                   , const unsigned l_num
                   , const unsigned c_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* sp = m.styles[ l_num ];

  ASSERT( __LINE__, c_num < sp->len(), "c_num < sp->len()" );

  // Clear only star
  sp->set( c_num, sp->get( c_num ) & ~HI_STAR );
}

Line* Append_hex_2_line( FileBuf::Data& m
                     //, LinesList& n_lines
                       , Array_t<Line*>& n_lines
                       , Line* n_line
                       , const uint8_t C )
{
  Trace trace( __PRETTY_FUNCTION__ );

  char C1 = MS_Hex_Digit( C );
  char C2 = LS_Hex_Digit( C );

  n_line->push(' ');
  n_line->push(C1);
  n_line->push(C2);

  if( 47 < n_line->len() )
  {
    n_lines.push( n_line );
    n_line = m.vis.BorrowLine( __FILE__,__LINE__ );
  }
  return n_line;
}

void Replace_current_file( FileBuf::Data& m
                       //, LinesList& n_lines
                         , Array_t<Line*>& n_lines
                         , const Encoding enc
                         , const bool LF_at_EOF )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.save_history = false;

  m.self.ClearLines();

  for( unsigned k=0; k<n_lines.len(); k++ )
  {
    // FileBuf takes responsibility for deleting line:
    m.self.PushLine( *n_lines.get(k) );
  }
  m.save_history = true;

  m.decoding = enc;
  m.encoding = enc;
  m.LF_at_EOF = LF_at_EOF;
}

bool BYTE_to_HEX( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool ok = true;

  Array_t<Line*> n_lines;
  Line* n_line = m.vis.BorrowLine( __FILE__,__LINE__ );

  const unsigned NUM_LINES = m.lines.len();

  for( unsigned k=0; k<NUM_LINES; k++ )
  {
    Line* l = m.lines[k];
    const int LL = l->len();

    for( int i=0; i<LL; i++ )
    {
      n_line = Append_hex_2_line( m, n_lines, n_line, l->get(i) );
    }
    if( k<NUM_LINES-1 || m.LF_at_EOF )
    {
      n_line = Append_hex_2_line( m, n_lines, n_line, '\n' );
    }
  }
  if( 0 < n_line->len() ) n_lines.push( n_line );

  Replace_current_file( m, n_lines, ENC_HEX, false );
  m.self.Set_File_Type("text");
  return ok;
}

bool HEX_to_BYTE_check_format( FileBuf::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  bool ok = true;

  const int NUM_LINES = m.lines.len();
  // Make sure lines are all 48 characters long, except last line,
  // which must be a multiple of 3 characters long
  for( unsigned k=0; ok && k<NUM_LINES; k++ )
  {
    Line* l = m.lines[k];
    const unsigned LL = l->len();
    if( k<(NUM_LINES-1) && LL != 48 )
    {
      ok = false;
      m.vis.Window_Message("Line: %u is %u characters, not 48", (k+1), LL);
    }
    else { // Last line
      if( 48 < LL || 0!=(LL%3) )
      {
        ok = false;
        m.vis.Window_Message("Line: %u is %u characters, not multiple of 3", (k+1), LL);
      }
    }
    // Make sure lines are: ' XX XX'
    for( int i=0; ok && i<LL; i+=3 )
    {
      const char C0 = l->get(i);
      const char C1 = l->get(i+1);
      const char C2 = l->get(i+2);

      if( C0 != ' ' )
      {
        ok = false;
        m.vis.Window_Message("Expected space on Line: %u pos: %u", (k+1), (i+1));
      }
      else if( !IsHexDigit( C1 ) )
      {
        ok = false;
        m.vis.Window_Message("Expected hex digit on Line: %u pos: %u : %c", (k+1), (i+2), C1);
      }
      else if( !IsHexDigit( C2 ) )
      {
        ok = false;
        m.vis.Window_Message("Expected hex digit on Line: %u pos: %u : %c", (k+1), (i+3), C2);
      }
    }
  }
  return ok;
}

bool HEX_to_BYTE_get_lines( FileBuf::Data& m
                          , Array_t<Line*>& n_lines
                          , bool& LF_at_EOF )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool ok = HEX_to_BYTE_check_format(m);
  if( ok )
  {
    Line* n_line = m.vis.BorrowLine( __FILE__,__LINE__ );

    for( int k=0; k<m.lines.len(); k++ )
    {
      Line* l = m.lines[k];
      const int LL = l->len();
      for( int i=0; i<LL; i+=3 )
      {
        const char C1 = l->get(i+1);
        const char C2 = l->get(i+2);
        const char C  = Hex_Chars_2_Byte( C1, C2 );

        if('\n' == C)
        {
          n_lines.push( n_line );
          n_line = m.vis.BorrowLine( __FILE__,__LINE__ );
          LF_at_EOF = true;
        }
        else {
          n_line->push( C );
          LF_at_EOF = false;
        }
      }
    }
    if( 0 < n_line->len() ) n_lines.push( n_line );
  }
  return ok;
}

bool HEX_to_BYTE( FileBuf::Data& m )
{
  Array_t<Line*> n_lines;
  bool LF_at_EOF = true;

  bool ok = HEX_to_BYTE_get_lines( m, n_lines, LF_at_EOF );
  if( ok )
  {
    Replace_current_file( m, n_lines, ENC_BYTE, LF_at_EOF );
    Find_File_Type_Suffix( m );
  }
  return ok;
}

FileBuf::FileBuf( Vis& vis
                , const char* const FILE_NAME
                , const bool MUTABLE
                , const File_Type FT )
  : m( *new(__FILE__, __LINE__)
        Data( *this, vis, FILE_NAME, MUTABLE, FT )
     )
{
  // Absolute byte offset of beginning of first line in file is always zero:
  m.lineOffsets.push( 0 );

  if( FT == FT_BUFFER_EDITOR )
  {
    m.file_type = FT_BUFFER_EDITOR;
    m.pHi = new(__FILE__,__LINE__) Highlight_BufferEditor( *this );
  }
  else if( FT_UNKNOWN == m.file_type ) Find_File_Type_Suffix( m );

  m.vis.Add_FileBuf_2_Lists_Create_Views( this, m.path_name.c_str() );
}

FileBuf::FileBuf( Vis& vis
                , const char* const FILE_NAME
                , const FileBuf& rfb )
  : m( *new(__FILE__, __LINE__)
        Data( *this, vis, FILE_NAME, rfb )
     )
{
  Find_File_Type_Suffix( m );

  const unsigned NUM_LINES = rfb.m.lines.len();

  // Reserve space:
  m.lines.inc_cap( NUM_LINES );
  m.styles.inc_cap( NUM_LINES );
  m.lineRegexsValid.inc_cap( NUM_LINES );

  // Add elements:
  for( unsigned k=0; k<NUM_LINES; k++ )
  {
    m.lines.push( m.vis.BorrowLine(__FILE__,__LINE__, *(rfb.m.lines[k]) ) );
  }
  for( unsigned k=0; k<NUM_LINES; k++ )
  {
    m.styles.push( m.vis.BorrowLine(__FILE__,__LINE__, *(rfb.m.styles[k]) ) );
  }
  for( unsigned k=0; k<NUM_LINES; k++ )
  {
    m.lineRegexsValid.push( false );
  }
  m.mod_time = ModificationTime( m.path_name.c_str() );

  m.vis.Add_FileBuf_2_Lists_Create_Views( this, m.path_name.c_str() );
}

FileBuf::~FileBuf()
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* p_line = 0;
  while( 0<m.lines.len() )
  {
    m.lines.pop( p_line );
    m.vis.ReturnLine( p_line );
  }
  while( 0<m.styles.len() )
  {
    m.styles.pop( p_line );
    m.vis.ReturnLine( p_line );
  }
  MemMark(__FILE__,__LINE__); delete m.pHi;
  MemMark(__FILE__,__LINE__); delete &m;
}

bool FileBuf::IsDir() const { return m.is_dir; }
bool FileBuf::IsRegular() const { return m.is_regular; }
double FileBuf::GetModTime() const { return m.mod_time; }
void FileBuf::SetModTime( const double mt ) { m.mod_time = mt; }
void FileBuf::SetFocTime( const double ft ) { m.foc_time = ft; }
bool FileBuf::GetChangedExternally() const { return m.changed_externally; }
void FileBuf::SetChangedExternally() { m.changed_externally = true; }
const char* FileBuf::GetPathName() const { return m.path_name.c_str(); }
const char* FileBuf::GetDirName() const { return m.dir_name.c_str(); }
const char* FileBuf::GetFileName() const { return m.file_name.c_str(); }
File_Type   FileBuf::GetFileType() const { return m.file_type; }

Encoding FileBuf::GetDecoding() const
{
  return m.decoding;
}

bool FileBuf::SetDecoding( const Encoding dec )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool ok = true;
  if( dec != m.decoding )
  {
    if( m.decoding == ENC_BYTE )
    {
      if( dec == ENC_HEX ) ok = BYTE_to_HEX(m);
      else ok = false;
    }
    else if( dec == ENC_BYTE )
    {
      if( m.decoding == ENC_HEX ) ok = HEX_to_BYTE(m);
      else ok = false;
    }
    else ok = false;
  }
  return ok;
}

Encoding FileBuf::GetEncoding() const
{
  return m.encoding;
}

void FileBuf::SetEncoding( const Encoding E )
{
  m.encoding = E;
}

void FileBuf::Set_File_Type( const char* syn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.is_dir )
  {
    bool found_syntax_type = true;
    Highlight_Base* p_new_Hi = 0;

    if( 0==strcmp( syn, "sh")
     || 0==strcmp( syn, "bash") )
    {
      m.file_type = FT_BASH;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_Bash( *this );
    }
    else if( 0==strcmp( syn, "cmake") )
    {
      m.file_type = FT_CMAKE;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_CMake( *this );
    }
    else if( 0==strcmp( syn, "c")
          || 0==strcmp( syn, "cpp") )
    {
      m.file_type = FT_CPP;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_CPP( *this );
    }
    else if( 0==strcmp( syn, "htm")
          || 0==strcmp( syn, "html") )
    {
      m.file_type = FT_HTML;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_HTML( *this );
    }
    else if( 0==strcmp( syn, "idl") )
    {
      m.file_type = FT_IDL;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_IDL( *this );
    }
    else if( 0==strcmp( syn, "java") )
    {
      m.file_type = FT_JAVA;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_Java( *this );
    }
    else if( 0==strcmp( syn, "js") )
    {
      m.file_type = FT_JS;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_JS( *this );
    }
    else if( 0==strcmp( syn, "make") )
    {
      m.file_type = FT_MAKE;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_Make( *this );
    }
    else if( 0==strcmp( syn, "mib") )
    {
      m.file_type = FT_MIB;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_MIB( *this );
    }
    else if( 0==strcmp( syn, "odb") )
    {
      m.file_type = FT_ODB;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_ODB( *this );
    }
    else if( 0==strcmp( syn, "py") )
    {
      m.file_type = FT_PY;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_Python( *this );
    }
    else if( 0==strcmp( syn, "sql") )
    {
      m.file_type = FT_SQL;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_SQL( *this );
    }
    else if( 0==strcmp( syn, "ste")
          || 0==strcmp( syn, "stl") )
    {
      m.file_type = FT_STL;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_STL( *this );
    }
    else if( 0==strcmp( syn, "swift") )
    {
      m.file_type = FT_SWIFT;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_Swift( *this );
    }
    else if( 0==strcmp( syn, "tcl") )
    {
      m.file_type = FT_TCL;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_TCL( *this );
    }
    else if( 0==strcmp( syn, "text") )
    {
      m.file_type = FT_TEXT;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_Text( *this );
    }
    else if( 0==strcmp( syn, "xml") )
    {
      m.file_type = FT_XML;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_XML( *this );
    }
    else {
      found_syntax_type = false;
    }
    if( found_syntax_type )
    {
      if( 0 != m.pHi )
      {
        MemMark(__FILE__,__LINE__);
        delete m.pHi;
      }
      m.pHi = p_new_Hi;

      m.hi_touched_line = 0;

      Update();
    }
  }
}

void FileBuf::AddView( View* v )
{
  m.views.push( v );
}

void FileBuf::AddView( LineView* v )
{
  m.line_view = v;
}

void FileBuf::ReadFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.is_dir )
  {
    // Directory
    DIR* dp = opendir( m.path_name.c_str() );
    if( dp ) {
      ReadExistingDir( m, dp, m.path_name.c_str() );
      closedir( dp );
    }
  }
  else {
    // Regular file
    FILE* fp = fopen( m.path_name.c_str(), "rb" );
    if( fp )
    {
      ReadExistingFile( m, fp );
      fclose( fp );
    }
    else {
      // File does not exist, so add an empty line:
      PushLine();
    }
    m.save_history = true;
  }
  m.mod_time = ModificationTime( m.path_name.c_str() );
  m.changed_externally = false;
}

void FileBuf::ReReadFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Can only re-read user files
  if( USER_FILE <= m.vis.Buf2FileNum( this ) )
  {
    ClearChanged();
    ClearLines();

    m.save_history = false; //< Gets turned back on in ReadFile()

    ReadFile();

    // To be safe, put cursor at top,left of each view of this file:
    for( unsigned w=0; w<MAX_WINS; w++ )
    {
      View* const pV = m.views[w];

      pV->Check_Context();
    }
    m.save_history    = true;
    m.hi_touched_line = 0;
  }
}

bool FileBuf::Sort()
{
  return m.vis.GetSortByTime()
       ? BufferEditor_SortTime()
       : BufferEditor_SortName();
}

// Return true if l_0 (dname,fname)
// is greater then l_1 (dname,fname)
bool BufferEditor_SortName_Swap( const Line& l_0, const Line& l_1 )
{
  bool swap = false;

  const String& l_0_s = l_0.toString();
  const String& l_1_s = l_1.toString();

  // Tail is the directory name:
  const String& l_0_dn = GetFnameTail( l_0_s.c_str() );
  const String& l_1_dn = GetFnameTail( l_1_s.c_str() );

  const int dn_compare = l_0_dn.compareTo( l_1_dn );

  if( 0<dn_compare )
  {
    // l_0 dname is greater than l_1 dname
    swap = true;
  }
  else if( 0==dn_compare )
  {
    // l_0 dname == l_1 dname
    // Head is the file name:
    const String& l_0_fn = GetFnameHead( l_0_s.c_str() );
    const String& l_1_fn = GetFnameHead( l_1_s.c_str() );

    if( 0<l_0_fn.compareTo( l_1_fn ) )
    {
      // l_0 fname is greater than l_1 fname
      swap = true;
    }
  }
  return swap;
}

// Move largest file name to bottom.
// Files are grouped under directories.
// Returns true if any lines were swapped.
bool FileBuf::BufferEditor_SortName()
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool changed = false;
  const unsigned NUM_LINES = NumLines();

  const unsigned NUM_BUILT_IN_FILES = USER_FILE;
  const unsigned FNAME_START_CHAR   = 0;

  // Sort lines (file names), least to greatest:
  for( unsigned i=NUM_LINES-1; NUM_BUILT_IN_FILES<i; i-- )
  {
    for( unsigned k=NUM_BUILT_IN_FILES; k<i; k++ )
    {
      const Line& l_0 = *m.lines[ k   ];
      const Line& l_1 = *m.lines[ k+1 ];

      if( BufferEditor_SortName_Swap( l_0, l_1 ) )
      {
        SwapLines( m, k, k+1 );
        changed = true;
      }
    }
  }
  return changed;
}

bool FileBuf::BufferEditor_SortTime()
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool changed = false;
  const unsigned NUM_BUILT_IN_FILES = USER_FILE;

  // Sort lines (file names), least to greatest:
  for( unsigned i=NumLines()-1; NUM_BUILT_IN_FILES<i; i-- )
  {
    for( unsigned k=NUM_BUILT_IN_FILES; k<i; k++ )
    {
      const Line& l_0 = *m.lines[ k   ];
      const Line& l_1 = *m.lines[ k+1 ];

      FileBuf* f_0 = m.vis.GetFileBuf( l_0.toString() );
      FileBuf* f_1 = m.vis.GetFileBuf( l_1.toString() );

      // Move oldest files to the bottom.
      // f_0 has older time than f_1, so move it down:
      if( f_0 && f_1 && f_0->m.foc_time < f_1->m.foc_time )
      {
        SwapLines( m, k, k+1 );
        changed = true;
      }
    }
  }
  return changed;
}

void FileBuf::ReadString( const char* const STR )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;

  unsigned index = 0;
  int C = 0;
  while( 0 != (C=STR[index++]) )
  {
    if( 0==lp ) lp = m.vis.BorrowLine( __FILE__,__LINE__ );

    if( '\n' == C )
    {
      PushLine( lp ); //< FileBuf::lines takes ownership of lp
      lp = 0;
      m.LF_at_EOF = true;
    }
    else {
      bool ok = lp->push( C );
      if( !ok ) DIE("Line.push() failed");
      m.LF_at_EOF = false;
    }
  }
  if( lp ) PushLine( lp ); //< FileBuf::lines takes ownership of lp
}

void FileBuf::ReadArray( const Line& line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;
  const unsigned LL = line.len();

  for( unsigned k=0; k<LL; k++ )
  {
    if( 0==lp ) lp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const int C = line.get( k );

    if( '\n' == C )
    {
      PushLine( lp ); //< FileBuf::lines takes ownership of lp
      lp = 0;
      m.LF_at_EOF = true;
    }
    else {
      bool ok = lp->push( C );
      if( !ok ) DIE("Line.push() failed");
      m.LF_at_EOF = false;
    }
  }
  if( lp ) PushLine( lp ); //< FileBuf::lines takes ownership of lp
}

void Write_p( FileBuf::Data& m
            , const Array_t<Line*>& l_lines
            , const bool l_LF_at_EOF )
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==m.path_name.len() )
  {
    // No file name message:
    m.vis.CmdLineMessage("No file name to write to");
  }
  else {
    FILE* fp = fopen( m.path_name.c_str(), "wb" );

    if( !fp ) {
      // Could not open file for writing message:
      m.vis.Window_Message("\nCould not open:\n\n%s\n\nfor writing\n\n"
                           , m.path_name.c_str() );
    }
    else {
      const unsigned NUM_LINES = l_lines.len();

      for( unsigned k=0; k<NUM_LINES; k++ )
      {
        const unsigned LL = l_lines[k]->len();
        for( unsigned i=0; i<LL; i++ )
        {
          int c = l_lines[k]->get(i);
          fputc( c, fp );
        }
        if( k<NUM_LINES-1 || l_LF_at_EOF )
        {
          fputc( '\n', fp );
        }
      }
      fclose( fp );

      m.mod_time = ModificationTime( m.path_name.c_str() );
      m.changed_externally = false;

      m.history.Clear();
      // Wrote to file message:
      m.vis.CmdLineMessage("\"%s\" written", m.path_name.c_str() );
    }
  }
}

void FileBuf::Write()
{
  if( ENC_BYTE == m.encoding )
  {
    Write_p( m, m.lines, m.LF_at_EOF );
  }
  else if( ENC_HEX == m.encoding )
  {
    Array_t<Line*> n_lines;
    bool LF_at_EOF = true;

    if( HEX_to_BYTE_get_lines( m, n_lines, LF_at_EOF ) )
    {
      Write_p( m, n_lines, LF_at_EOF );
    }
  }
  else {
    m.vis.Window_Message("\nUnhandled Encoding: %s\n\n"
                        , Encoding_Str( m.encoding ) );
  }
}

// Return number of lines in file
//
unsigned FileBuf::NumLines() const
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.lines.len();
}

// Returns length of line line_num
//
unsigned FileBuf::LineLen( const unsigned line_num ) const
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( line_num < m.lines.len() )
  {
    return m.lines[ line_num ]->len();
  }
  return 0;
}

// Get byte on line l_num at position c_num
//
uint8_t FileBuf::Get( const unsigned l_num, const unsigned c_num ) const
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( l_num < m.lines.len() )
  {
    Line* lp = m.lines[ l_num ];

    if( c_num < lp->len() )
    {
      return lp->get(c_num);
    }
  }
  return ' ';
}

// Set byte on line l_num at position c_num
//
void FileBuf::Set( const unsigned l_num
                 , const unsigned c_num
                 , const uint8_t  C
                 , const bool     continue_last_update )
{
  Trace trace( __PRETTY_FUNCTION__ );

  ASSERT( __LINE__, l_num < m.lines.len(), "l_num < m.lines.len()" );

  Line* lp = m.lines[ l_num ];

  ASSERT( __LINE__, c_num < lp->len(), "c_num < lp->len()" );

  const uint8_t old_C = lp->get(c_num);

  if( old_C != C )
  {
    lp->set( c_num, C );

    if( SavingHist( m ) )
    {
      m.history.Save_Set( l_num, c_num, old_C, continue_last_update );
    }
    // Did not call ChangedLine(), so need to set m.hi_touched_line here:
    m.hi_touched_line = Min( m.hi_touched_line, l_num );
    m.lineRegexsValid.set( l_num, false );
  }
}

bool FileBuf::Has_LF_at_EOF() { return m.LF_at_EOF; }

// Return reference to line l_num
//
const Line& FileBuf::GetLine( const unsigned l_num ) const
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.lines.len(), "l_num < m.lines.len()" );

  const Line* lp  = m.lines[ l_num ];
  ASSERT( __LINE__, lp, "m.lines[ %u ]", l_num );

  return *lp;
}

// Return reference to line l_num
//
const Line& FileBuf::GetStyle( const unsigned l_num ) const
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* lp  = m.styles[ l_num ];
  ASSERT( __LINE__, lp, "m.styles[ %u ]", l_num );

  return *lp;
}

// Put a copy of line l_num into l
//
void FileBuf::GetLine( const unsigned l_num, Line& l ) const
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.lines.len(), "l_num < m.lines.len()" );

//l = *(m.lines[ l_num ]);
  l.copy( *(m.lines[ l_num ]) );
}

const Line* FileBuf::GetLineP( const unsigned l_num ) const
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.lines.len(), "l_num < m.lines.len()" );

  return m.lines[ l_num ];
}

// Insert a new line on line l_num, which is a copy of line.
// l_num can be m.lines.len();
//
void FileBuf::InsertLine( const unsigned l_num, const Line& line )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <= m.lines.len(), "l_num < m.lines.len()" );

  Line* lp = m.vis.BorrowLine( __FILE__,__LINE__, line );
  Line* sp = m.vis.BorrowLine( __FILE__,__LINE__, line.len(), 0 );
  ASSERT( __LINE__, lp->len() == sp->len(), "(lp->len()=%u) != (sp->len()=%u)", lp->len(), sp->len() );

  bool ok = m.lines.insert( l_num, lp )
         && m.styles.insert( l_num, sp )
         && m.lineRegexsValid.insert( l_num, false );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( m, l_num );

  if( SavingHist( m ) ) m.history.Save_InsertLine( l_num );

  InsertLine_Adjust_Views_topLines( l_num );
}

// Insert pLine on line l_num.  FileBuf will delete pLine.
// l_num can be m.lines.len();
//
void FileBuf::InsertLine( const unsigned l_num, Line* const pLine )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <= m.lines.len(), "l_num < m.lines.len()" );

  Line* sp = m.vis.BorrowLine( __FILE__,__LINE__,  pLine->len(), 0 );
  ASSERT( __LINE__, pLine->len() == sp->len(), "(pLine->len()=%u) != (sp->len()=%u)", pLine->len(), sp->len() );

  bool ok = m.lines.insert( l_num, pLine )
         && m.styles.insert( l_num, sp )
         && m.lineRegexsValid.insert( l_num, false );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( m, l_num );

  if( SavingHist( m ) ) m.history.Save_InsertLine( l_num );

  InsertLine_Adjust_Views_topLines( l_num );
}

// Insert a new empty line on line l_num.
// l_num can be m.lines.len();
//
void FileBuf::InsertLine( const unsigned l_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <= m.lines.len(), "l_num < m.lines.len()" );

  Line* lp = m.vis.BorrowLine( __FILE__,__LINE__ );
  Line* sp = m.vis.BorrowLine( __FILE__,__LINE__ );
  ASSERT( __LINE__, lp->len() == sp->len(), "(lp->len()=%u) != (sp->len()=%u)", lp->len(), sp->len() );

  bool ok = m.lines.insert( l_num, lp )
         && m.styles.insert( l_num, sp )
         && m.lineRegexsValid.insert( l_num, false );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( m, l_num );

  if( SavingHist( m ) ) m.history.Save_InsertLine( l_num );

  InsertLine_Adjust_Views_topLines( l_num );
}

void FileBuf::InsertLine_Adjust_Views_topLines( const unsigned l_num )
{
  for( unsigned w=0; w<MAX_WINS && w<m.views.len(); w++ )
  {
    View* const pV = m.views[w];

    const unsigned top_line = pV->GetTopLine();

    if( l_num < top_line ) pV->SetTopLine( top_line+1 );
  }
  if( 0 != m.line_view )
  {
    LineView* const pV = m.line_view;

    const unsigned top_line = pV->GetTopLine();

    if( l_num < top_line ) pV->SetTopLine( top_line+1 );
  }
}

// Insert byte on line l_num at position c_num.
// c_num can be length of line.
//
void FileBuf::InsertChar( const unsigned l_num
                        , const unsigned c_num
                        , const uint8_t  C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.lines.len(), "l_num < m.lines.len()" );

  Line* lp =  m.lines[ l_num ];
  Line* sp = m.styles[ l_num ];

  ASSERT( __LINE__, c_num <= lp->len(), "c_num < lp->len()" );
  ASSERT( __LINE__, c_num <= sp->len(), "c_num < sp->len()" );

  bool ok = lp->insert( c_num, C )
         && sp->insert( c_num, 0 )
         && m.lineRegexsValid.set( l_num, false );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( m, l_num );

  if( SavingHist( m ) ) m.history.Save_InsertChar( l_num, c_num );
}

// Add a new line at the end of FileBuf, which is a copy of line
//
void FileBuf::PushLine( const Line& line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = m.vis.BorrowLine( __FILE__,__LINE__, line );
  Line* sp = m.vis.BorrowLine( __FILE__,__LINE__, line.len(), 0 );
  ASSERT( __LINE__, lp->len() == sp->len(), "(lp->len()=%u) != (sp->len()=%u)", lp->len(), sp->len() );

  bool ok = m.lines.push( lp )
         && m.styles.push( sp )
         && m.lineRegexsValid.push( false );

  ASSERT( __LINE__, ok, "ok" );

  if( SavingHist( m ) ) m.history.Save_InsertLine( m.lines.len()-1 );
}

// Add a new pLine to the end of FileBuf.
// FileBuf will delete pLine.
//
void FileBuf::PushLine( Line* const pLine )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* sp = m.vis.BorrowLine( __FILE__,__LINE__, pLine->len(), 0 );
  ASSERT( __LINE__, pLine->len() == sp->len(), "(pLine->len()=%u) != (sp->len()=%u)", pLine->len(), sp->len() );

  bool ok = m.lines.push( pLine )
        && m.styles.push( sp )
        && m.lineRegexsValid.push( false );

  ASSERT( __LINE__, ok, "ok" );

  if( SavingHist( m ) ) m.history.Save_InsertLine( m.lines.len()-1 );
}

// Add a new empty line at the end of FileBuf
//
void FileBuf::PushLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = m.vis.BorrowLine( __FILE__,__LINE__ );
  Line* sp = m.vis.BorrowLine( __FILE__,__LINE__ );
  ASSERT( __LINE__, lp->len() == sp->len(), "(lp->len()=%u) != (sp->len()=%u)", lp->len(), sp->len() );

  bool ok = m.lines.push( lp )
         && m.styles.push( sp )
         && m.lineRegexsValid.push( false );

  ASSERT( __LINE__, ok, "ok" );

  if( SavingHist( m ) ) m.history.Save_InsertLine( m.lines.len()-1 );
}

// Add byte C to the end of line l_num
//
void FileBuf::PushChar( const unsigned l_num, const uint8_t C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  m.lines.len(), "l_num < m.lines.len()" );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* lp =  m.lines[ l_num ];
  Line* sp = m.styles[ l_num ];

  bool ok = lp->push( C )
         && sp->push( 0 )
         && m.lineRegexsValid.set( l_num, false );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( m, l_num );

  if( SavingHist( m ) ) m.history.Save_InsertChar( l_num, lp->len()-1 );
}

// Add byte C to last line.  If no m.lines in file, add a line.
//
void FileBuf::PushChar( const uint8_t C )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.lines.len() ) PushLine();

  const unsigned l_num = m.lines.len()-1;

  PushChar( l_num, C );
}

// Remove and return a char from the end of line l_num
//
uint8_t FileBuf::PopChar( const unsigned l_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  m.lines.len(), "l_num < m.lines.len()" );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* lp =  m.lines[ l_num ];
  Line* sp = m.styles[ l_num ];

  uint8_t C = 0;
  bool ok = lp->pop( C )
         && sp->pop()
         && m.lineRegexsValid.set( l_num, false );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( m, l_num );

  if( SavingHist( m ) ) m.history.Save_RemoveChar( l_num, lp->len(), C );
  // For now, PopChar is not called in response to user action,
  // so dont need to save update

  return C;
}

// Remove a line from FileBuf, and return a copy.
// Line that was in FileBuf gets deleted.
//
void FileBuf::RemoveLine( const unsigned l_num, Line& line )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  m.lines.len(), "l_num < m.lines.len()" );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* lp = 0;
  Line* sp = 0;
  bool ok = m.lines.remove( l_num, lp )
         && m.styles.remove( l_num, sp )
         && m.lineRegexsValid.remove( l_num );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( m, l_num );

  line.copy( *lp );

  if( SavingHist( m ) ) m.history.Save_RemoveLine( l_num, line );

  RemoveLine_Adjust_Views_topLines( m, l_num );

  m.vis.ReturnLine( lp );
  m.vis.ReturnLine( sp );
}

// Remove a line from FileBuf without deleting it and return pointer to it.
// Caller of RemoveLine is responsible for deleting line returned.
//
Line* FileBuf::RemoveLineP( const unsigned l_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  m.lines.len(), "l_num < m.lines.len()" );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* pLine = 0;
  Line* sp = 0;
  bool ok = m.lines.remove( l_num, pLine )
         && m.styles.remove( l_num, sp )
         && m.lineRegexsValid.remove( l_num );

  m.vis.ReturnLine( sp );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( m, l_num );

  if( SavingHist( m ) ) m.history.Save_RemoveLine( l_num, *pLine );

  RemoveLine_Adjust_Views_topLines( m, l_num );

  return pLine;
}

// Remove a line from FileBuf, and without returning a copy.
// Line that was in FileBuf gets deleted.
//
void FileBuf::RemoveLine( const unsigned l_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  m.lines.len(), "l_num < m.lines.len()" );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* lp = 0;
  Line* sp = 0;
  bool ok = m.lines.remove( l_num, lp )
         && m.styles.remove( l_num, sp )
         && m.lineRegexsValid.remove( l_num );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( m, l_num );

  if( SavingHist( m ) ) m.history.Save_RemoveLine( l_num, *lp );

  RemoveLine_Adjust_Views_topLines( m, l_num );

  m.vis.ReturnLine( lp );
  m.vis.ReturnLine( sp );
}

// Remove from FileBuf and return the char at line l_num and position c_num
//
uint8_t FileBuf::RemoveChar( const unsigned l_num, const unsigned c_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  m.lines.len(), "l_num < m.lines.len()" );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* lp =  m.lines[ l_num ];
  Line* sp = m.styles[ l_num ];

  ASSERT( __LINE__, c_num < lp->len(), "c_num=%u < lp->len()=%u", c_num, lp->len() );
  ASSERT( __LINE__, c_num < sp->len(), "c_num < sp->len()" );

  uint8_t C = 0;
  bool ok = lp->remove( c_num, C )
         && sp->remove( c_num )
         && m.lineRegexsValid.set( l_num, false );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( m, l_num );

  if( SavingHist( m ) ) m.history.Save_RemoveChar( l_num, c_num, C );

  return C;
}

void FileBuf::PopLine( Line& line )
{
  Trace trace( __PRETTY_FUNCTION__ );
  Line* lp = 0;
  Line* sp = 0;
  bool ok = m.lines.pop( lp )
         && m.styles.pop( sp )
         && m.lineRegexsValid.pop();

  ASSERT( __LINE__, ok, "ok" );

  line.copy( *lp );

  m.vis.ReturnLine( lp );
  m.vis.ReturnLine( sp );

  ChangedLine( m, m.lines.len()-1 );

  if( SavingHist( m ) ) m.history.Save_RemoveLine( m.lines.len(), line );
  // For now, PopLine is not called in response to user action,
  // so dont need to save update
}

void FileBuf::PopLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.lines.len() )
  {
    Line* lp = 0;
    Line* sp = 0;
    bool ok = m.lines.pop( lp )
           && m.styles.pop( sp )
           && m.lineRegexsValid.pop();

    ASSERT( __LINE__, ok, "ok" );

    ChangedLine( m, m.lines.len()-1 );

    if( SavingHist( m ) ) m.history.Save_RemoveLine( m.lines.len(), *lp );
    // For now, PopLine is not called in response to user action,
    // so dont need to save update

    m.vis.ReturnLine( lp );
    m.vis.ReturnLine( sp );
  }
}

// Append line to end of line l_num
//
void FileBuf::AppendLineToLine( const unsigned l_num, const Line& line )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  m.lines.len(), "l_num < m.lines.len()" );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* lp = m.lines[ l_num ];
  Line* sp = m.styles[ l_num ];

  bool ok = lp->append( line )
         && m.lineRegexsValid.set( l_num, false );
  ASSERT( __LINE__, ok, "ok" );

  // Simply need to increase sp's length to match lp's new length:
  for( unsigned k=0; k<line.len(); k++ ) sp->push( 0 );

  ChangedLine( m, l_num );

  const unsigned first_insert = lp->len() - line.len();

  if( SavingHist( m ) )
  {
    for( unsigned k=0; k<line.len(); k++ )
    {
      m.history.Save_InsertChar( l_num, first_insert + k );
    }
  }
}

// Append pLine to end of line l_num, and delete pLine.
//
void FileBuf::AppendLineToLine( const unsigned l_num, const Line* pLine )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  m.lines.len(), "l_num < m.lines.len()" );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* lp =  m.lines[ l_num ];
  Line* sp = m.styles[ l_num ];

  bool ok = lp->append( *pLine )
         && m.lineRegexsValid.set( l_num, false );
  ASSERT( __LINE__, ok, "ok" );

  // Simply need to increase sp's length to match lp's new length:
  for( unsigned k=0; k<pLine->len(); k++ ) sp->push( 0 );

  ChangedLine( m, l_num );

  const unsigned first_insert = lp->len() - pLine->len();

  if( SavingHist( m ) )
  {
    for( unsigned k=0; k<pLine->len(); k++ )
    {
      m.history.Save_InsertChar( l_num, first_insert + k );
    }
  }
  m.vis.ReturnLine( const_cast<Line*>( pLine ) );
}

void FileBuf::ClearLines()
{
  Trace trace( __PRETTY_FUNCTION__ );

  while( m.lines.len() )
  {
    Line* lp = 0;
    Line* sp = 0;
    bool ok = m.lines.pop( lp ) && m.styles.pop( sp );

    ASSERT( __LINE__, ok, "ok" );

    m.vis.ReturnLine( lp );
    m.vis.ReturnLine( sp );
  }
  ChangedLine( m, 0 );

  m.lineRegexsValid.clear();
}

void FileBuf::Undo( View& rV )
{
  if( m.save_history )
  {
    m.save_history = false;

    m.history.Undo( rV );

    m.save_history = true;
  }
}

void FileBuf::UndoAll( View& rV )
{
  if( m.save_history )
  {
    m.save_history = false;

    m.history.UndoAll( rV );

    m.save_history = true;
  }
}

bool FileBuf::Changed() const
{
  return m.history.Has_Changes();
}

void FileBuf::ClearChanged()
{
  m.history.Clear();
}

unsigned FileBuf::GetSize()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.lines.len();

  unsigned size = 0;

  if( NUM_LINES )
  {
    // Absolute byte offset of beginning of first line in file is always zero:
    if( 0 == m.lineOffsets.len() ) m.lineOffsets.push( 0 );

    // Old line offsets length:
    const unsigned OLOL = m.lineOffsets.len();

    // New line offsets length:
    m.lineOffsets.set_len( NUM_LINES );

    // Absolute byte offset of beginning of first line in file is always zero:
    m.lineOffsets[ 0 ] = 0;

    for( unsigned k=OLOL; k<NUM_LINES; k++ )
    {
      m.lineOffsets[ k ] = m.lineOffsets[ k-1 ]
                         + m.lines[ k-1 ]->len()
                         + 1; //< Add 1 for '\n'
    }
    size = m.lineOffsets[ NUM_LINES-1 ] + m.lines[ NUM_LINES-1 ]->len();
    if( m.LF_at_EOF ) size++;
  }
  return size;
}

unsigned FileBuf::GetCursorByte( unsigned CL, unsigned CC )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.lines.len();

  unsigned crsByte = 0;

  if( NUM_LINES )
  {
    if( NUM_LINES <= CL ) CL = NUM_LINES-1;

    const unsigned CLL = m.lines[CL]->len();

    if( CLL <= CC ) CC = CLL ? CLL-1 : 0;

    // Absolute byte offset of beginning of first line in file is always zero:
    if( 0 == m.lineOffsets.len() ) m.lineOffsets.push( 0 );

    // HVLO = Highest valid line offset
    const unsigned HVLO = m.lineOffsets.len()-1;

    if( HVLO < CL )
    {
      m.lineOffsets.set_len( CL+1 );

      for( unsigned k=HVLO+1; k<=CL; k++ )
      {
        m.lineOffsets[ k ] = m.lineOffsets[ k-1 ]
                           + LineLen( k-1 )
                           + 1;
      }
    }
    crsByte = m.lineOffsets[ CL ] + CC;
  }
  return crsByte;
}

void UpdateWinViews( FileBuf::Data& m, const bool PRINT_CMD_LINE )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* const pV = m.views[w];

    for( unsigned w2=0; w2<m.vis.GetNumWins(); w2++ )
    {
      if( pV == m.vis.WinView( w2 ) )
      {
        m.self.Find_Styles( pV->GetTopLine() + pV->WorkingRows() );
        m.self.Find_Regexs( pV->GetTopLine(), pV->WorkingRows() );

        pV->RepositionView();
        pV->Print_Borders();
        pV->PrintWorkingView();
        pV->PrintStsLine();
        pV->PrintFileLine();

        if( PRINT_CMD_LINE ) pV->PrintCmdLine();

        pV->SetStsLineNeedsUpdate( true );
      }
    }
  }
}

void FileBuf::Update()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.vis.RunningDot() )
  {
    UpdateWinViews( m, true );

    Console::Update();

    // Put cursor back into current window
    m.vis.CV()->PrintCursor();
  }
}

void FileBuf::UpdateCmd()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.vis.RunningDot() )
  {
    UpdateWinViews( m, false );

    if( 0 != m.line_view )
    {
      LineView* const pV = m.line_view;

      pV->RepositionView();
      pV->PrintWorkingView();

      Console::Update();

      // Put cursor back into current window
      pV->PrintCursor();
    }
  }
}

void FileBuf::ClearStyles()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.hi_touched_line = 0;
}

// Find m.styles up to but not including up_to_line number
void FileBuf::Find_Styles( const unsigned up_to_line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();

  if( 0<NUM_LINES )
  {
    m.hi_touched_line = Min( m.hi_touched_line, NUM_LINES-1 );

    if( m.hi_touched_line < up_to_line )
    {
      // Find m.styles for some EXTRA_LINES beyond where we need to find
      // m.styles for the moment, so that when the user is scrolling down
      // through an area of a file that has not yet been syntax highlighed,
      // Find_Styles_In_Range() does not need to be called every time the
      // user scrolls down another line.  Find_Styles_In_Range() will only
      // be called once for every EXTRA_LINES scrolled down.
      const unsigned EXTRA_LINES = 10;

      CrsPos   st = Update_Styles_Find_St( m, m.hi_touched_line );
      unsigned fn = Min( up_to_line+EXTRA_LINES, NUM_LINES );

      Find_Styles_In_Range( m, st, fn );

      m.hi_touched_line = fn;
    }
  }
}

void FileBuf::Find_Regexs( const unsigned start_line
                         , const unsigned num_lines )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Check_4_New_Regex();

  const unsigned up_to_line = Min( start_line+num_lines, NumLines() );

  for( unsigned k=start_line; k<up_to_line; k++ )
  {
    Find_Regexs_4_Line( k );
  }
}

void FileBuf::Check_4_New_Regex()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.regex != m.vis.GetRegex() )
  {
    Invalidate_Regexs();

    m.regex = m.vis.GetRegex();
  }
}

void FileBuf::Invalidate_Regexs()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Invalidate all regexes
  for( unsigned k=0; k<m.lineRegexsValid.len(); k++ )
  {
    m.lineRegexsValid.set( k, false );
  }
}

#ifdef USE_REGEX

// Return true if found search_pattern in search_string, else false.
// If returning true, fills in match_pos and match_len in search_string.
//
bool Regex_Search( FileBuf::Data& m
                 , const char* search_string
                 , const char* search_pattern
                 , unsigned& match_pos
                 , unsigned& match_len )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found = false;
  try {
    found = std::regex_search( search_string
                             , m.cm
                             , std::regex( search_pattern ) );
  }
  catch( const std::regex_error& e )
  {
    found = false;
  }
  if( found )
  {
    match_pos = m.cm.position();
    match_len = m.cm.length();
  }
  return found;
}

void FileBuf::Find_Regexs_4_Line( const unsigned line_num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( line_num < m.lineRegexsValid.len() && !m.lineRegexsValid[line_num] )
  {
    Line* lp = m.lines[line_num];
    const unsigned LL = lp->len();

    // Clear the patterns for the line:
    for( unsigned pos=0; pos<LL; pos++ )
    {
      ClearStarStyle( m, line_num, pos );
    }
    // Find the patterns for the line:
    bool found = true;
    for( unsigned p=0; found && p<LL; )
    {
      unsigned ma_pos = 0;
      unsigned ma_len = 0;

      found = Regex_Search( m, lp->c_str(p), m.regex.c_str(), ma_pos, ma_len )
           && 0 < ma_len;

      if( found )
      {
        const unsigned ma_st = p + ma_pos;
        const unsigned ma_fn = p + ma_pos + ma_len;

        for( unsigned pos=ma_st; pos<LL && pos<ma_fn; pos++ )
        {
          Set__StarStyle( m, line_num, pos );
        }
        p = ma_fn;
      }
    }
    m.lineRegexsValid[line_num] = true;
  }
}

#else

//void Find_patterns_for_line( FileBuf::Data& m
//                           , const unsigned line_num
//                           , Line* lp
//                           , const unsigned LL )
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  // Find the patterns for the line:
//  bool        boundary_st = false; // word boundary at start
//  bool        boundary_fn = false; // word boundary at end
//  unsigned    star_len = m.regex.len();
//  const char* star_str = m.regex.c_str();
//
//  if( 2<star_len && m.regex.has_at("\\b", 0) )
//  {
//    star_str += 2;
//    star_len -= 2;
//    boundary_st = true;
//  }
//  if( 2<star_len && m.regex.ends_with("\\b") )
//  {
//    star_len -= 2;
//    boundary_fn = true;
//  }
//  if( star_len<=LL ) //< This prevents unsigned subtraction overflow.
//  {
//    // Search for m.regex in Line, lp, at each position p:
//    for( unsigned p=0; p<=(LL-star_len); p++ )
//    {
//      bool matches = !boundary_st || line_start_or_prev_C_non_ident( *lp, p );
//
//      for( unsigned k=0; matches && (p+k)<LL && k<star_len; k++ )
//      {
//        if( star_str[k] != lp->get(p+k) ) matches = false;
//        else {
//          if( k+1 == star_len ) // Found pattern
//          {
//            matches = !boundary_fn || line_end_or_non_ident( *lp, LL, p+k );
//            if( matches ) {
//              for( unsigned n=p; n<p+star_len; n++ ) Set__StarStyle( m, line_num, n );
//              // Increment p one less than star_len, because p
//              // will be incremented again by the for loop
//              p += star_len-1;
//            }
//          }
//        }
//      }
//    }
//  }
//}

void Find_patterns_for_line( FileBuf::Data& m
                           , const unsigned line_num
                           , Line* lp
                           , const int LL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Find the patterns for the line:
  bool        boundary_st = false; // word boundary at start
  bool        boundary_fn = false; // word boundary at end
  int         star_len = m.regex.len();
  const char* star_str = m.regex.c_str();

  if( 2<star_len && m.regex.has_at("\\b", 0) )
  {
    star_str += 2;
    star_len -= 2;
    boundary_st = true;
  }
  if( 2<star_len && m.regex.ends_with("\\b") )
  {
    star_len -= 2;
    boundary_fn = true;
  }
  // Search for m.regex in Line, lp, at each position p:
  for( int p=0; p<=(LL-star_len); p++ )
  {
    bool matches = !boundary_st || line_start_or_prev_C_non_ident( *lp, p );

    for( int k=0; matches && (p+k)<LL && k<star_len; k++ )
    {
      if( star_str[k] != lp->get(p+k) ) matches = false;
      else {
        if( k+1 == star_len ) // Found pattern
        {
          matches = !boundary_fn || line_end_or_non_ident( *lp, LL, p+k );
          if( matches ) {
            for( int n=p; n<p+star_len; n++ ) Set__StarStyle( m, line_num, n );
            // Increment p one less than star_len, because p
            // will be incremented again by the for loop
            p += star_len-1;
          }
        }
      }
    }
  }
}

bool Line_Has_Pattern( const Line* lp, const String& pattern )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Find the patterns for the line:
  const unsigned LL = lp->len();
        bool     boundary = false;
        unsigned star_len = pattern.len();
  const char*    star_str = pattern.c_str();
  if( 4<pattern.len()
   && pattern.has_at("\\b", 0)
   && pattern.ends_with("\\b") )
  {
    star_str += 2;
    star_len -= 4;
    boundary  = true;
  }
  if( star_len<=LL )
  {
    for( unsigned p=0; p<LL; p++ )
    {
      bool matches = !boundary || line_start_or_prev_C_non_ident( *lp, p );
      for( unsigned k=0; matches && (p+k)<LL && k<star_len; k++ )
      {
        if( star_str[k] != lp->get(p+k) ) matches = false;
        else {
          if( k+1 == star_len ) // Found pattern
          {
            matches = !boundary || line_end_or_non_ident( *lp, LL, p+k );
            if( matches )
            {
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

bool Have_Regex_In_File( FileBuf::Data& m, const String& fname )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found = false;
  if( IsReg( fname.c_str() ) )
  {
    FILE* fp = fopen( fname.c_str(), "rb" );
    if( fp )
    {
      m.line_buf.clear();
      int C = 0;
      while( !found && EOF != (C = fgetc( fp )) )
      {
        if( '\n' == C )
        {
          found = Line_Has_Regex( m.line_buf, m.regex );
          m.line_buf.clear();
        }
        else {
          bool ok = m.line_buf.push( C );
          if( !ok ) DIE("Line.push() failed");
        }
      }
      fclose( fp );
    }
  }
  return found;
}

bool Filename_Is_Relevant( String fname )
{
  return fname.ends_with(".txt")
      || fname.ends_with(".txt.new")
      || fname.ends_with(".txt.old")
      || fname.ends_with(".sh")
      || fname.ends_with(".sh.new"  )
      || fname.ends_with(".sh.old"  )
      || fname.ends_with(".bash"    )
      || fname.ends_with(".bash.new")
      || fname.ends_with(".bash.old")
      || fname.ends_with(".alias"   )
      || fname.ends_with(".bash_profile")
      || fname.ends_with(".bash_logout")
      || fname.ends_with(".bashrc" )
      || fname.ends_with(".profile")
      || fname.ends_with(".h"      )
      || fname.ends_with(".h.new"  )
      || fname.ends_with(".h.old"  )
      || fname.ends_with(".c"      )
      || fname.ends_with(".c.new"  )
      || fname.ends_with(".c.old"  )
      || fname.ends_with(".hh"     )
      || fname.ends_with(".hh.new" )
      || fname.ends_with(".hh.old" )
      || fname.ends_with(".cc"     )
      || fname.ends_with(".cc.new" )
      || fname.ends_with(".cc.old" )
      || fname.ends_with(".hpp"    )
      || fname.ends_with(".hpp.new")
      || fname.ends_with(".hpp.old")
      || fname.ends_with(".cpp"    )
      || fname.ends_with(".cpp.new")
      || fname.ends_with(".cpp.old")
      || fname.ends_with(".cxx"    )
      || fname.ends_with(".cxx.new")
      || fname.ends_with(".cxx.old")
      || fname.ends_with(".idl"    )
      || fname.ends_with(".idl.new")
      || fname.ends_with(".idl.old")
      || fname.ends_with(".idl.in"    )
      || fname.ends_with(".idl.in.new")
      || fname.ends_with(".idl.in.old")
      || fname.ends_with(".html"    )
      || fname.ends_with(".html.new")
      || fname.ends_with(".html.old")
      || fname.ends_with(".htm"     )
      || fname.ends_with(".htm.new" )
      || fname.ends_with(".htm.old" )
      || fname.ends_with(".java"    )
      || fname.ends_with(".java.new")
      || fname.ends_with(".java.old")
      || fname.ends_with(".js"    )
      || fname.ends_with(".js.new")
      || fname.ends_with(".js.old")
      || fname.ends_with(".Make"    )
      || fname.ends_with(".make"    )
      || fname.ends_with(".Make.new")
      || fname.ends_with(".make.new")
      || fname.ends_with(".Make.old")
      || fname.ends_with(".make.old")
      || fname.ends_with("Makefile" )
      || fname.ends_with("makefile" )
      || fname.ends_with("Makefile.new")
      || fname.ends_with("makefile.new")
      || fname.ends_with("Makefile.old")
      || fname.ends_with("makefile.old")
      || fname.ends_with(".stl"    )
      || fname.ends_with(".stl.new")
      || fname.ends_with(".stl.old")
      || fname.ends_with(".ste"    )
      || fname.ends_with(".ste.new")
      || fname.ends_with(".ste.old")
      || fname.ends_with(".py"    )
      || fname.ends_with(".py.new")
      || fname.ends_with(".py.old")
      || fname.ends_with(".sql"    )
      || fname.ends_with(".sql.new")
      || fname.ends_with(".sql.old")
      || fname.ends_with(".xml"     )
      || fname.ends_with(".xml.new" )
      || fname.ends_with(".xml.old" )
      || fname.ends_with(".xml.in"    )
      || fname.ends_with(".xml.in.new")
      || fname.ends_with(".xml.in.old")
      || fname.ends_with(".cmake"     )
      || fname.ends_with(".cmake.new" )
      || fname.ends_with(".cmake.old" )
      || fname.ends_with(".cmake"     )
      || fname.ends_with(".cmake.new" )
      || fname.ends_with(".cmake.old" )
      || fname.ends_with("CMakeLists.txt")
      || fname.ends_with("CMakeLists.txt.old")
      || fname.ends_with("CMakeLists.txt.new");
}

bool File_Has_Regex( FileBuf::Data& m, Line* lp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.file_type == FT_DIR )
  {
    String hname = lp->c_str(0);
    String fname = m.dir_name; fname.append( hname );

    if( Filename_Is_Relevant( hname ) )
    {
      return Have_Regex_In_File( m, fname );
    }
  }
  else if( m.file_type == FT_BUFFER_EDITOR )
  {
    String fname = lp->c_str(0);

    if( fname !=  EDIT_BUF_NAME
     && fname !=  HELP_BUF_NAME
     && fname !=  MSG__BUF_NAME
     && fname != SHELL_BUF_NAME
     && fname != COLON_BUF_NAME
     && fname != SLASH_BUF_NAME
     && !fname.ends_with( DirDelimStr() ) )
    {
      FileBuf* pfb = m.vis.GetFileBuf( fname );
      if( 0 != pfb )
      {
        return pfb->Has_Pattern( m.regex );
      }
    }
  }
  return false;
}

void FileBuf::Find_Regexs_4_Line( const unsigned line_num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( line_num < m.lineRegexsValid.len() && !m.lineRegexsValid.at(line_num) )
  {
    Line* lp = m.lines[line_num];
    const unsigned LL = lp->len();

    // Clear the patterns for the line:
    for( unsigned pos=0; pos<LL; pos++ )
    {
      ClearStarStyle( m, line_num, pos );
    }
    if( 0<m.regex.len() && 0<LL )
    {
      if( m.file_type == FT_BUFFER_EDITOR
       || m.file_type == FT_DIR )
      {
        if( File_Has_Regex( m, lp ) )
        {
          for( int k=0; k<LL; k++ ) Set__StarStyle( m, line_num, k );
        }
      }
      Find_patterns_for_line( m, line_num, lp, LL );
    }
    m.lineRegexsValid.set( line_num, true );
  }
}

#endif

// Leave star style unchanged, and clear syntax m.styles
void FileBuf::ClearSyntaxStyles( const unsigned l_num
                               , const unsigned c_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* sp = m.styles[ l_num ];

  ASSERT( __LINE__, c_num < sp->len(), "c_num < sp->len()" );

  // Clear everything except star
  sp->set( c_num, sp->get( c_num ) & HI_STAR );
}

// Leave star style unchanged, and set syntax style
void FileBuf::SetSyntaxStyle( const unsigned l_num
                            , const unsigned c_num
                            , const unsigned style )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* sp = m.styles[ l_num ];

  ASSERT( __LINE__, c_num < sp->len(), "c_num < sp->len()" );

  uint8_t s = sp->get( c_num );

  s &= HI_STAR; //< Clear everything except star
  s |= style;   //< Set style

  sp->set( c_num, s );
}

bool FileBuf::HasStyle( const unsigned l_num
                      , const unsigned c_num
                      , const unsigned style )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* sp = m.styles[ l_num ];

  ASSERT( __LINE__, c_num < sp->len(), "c_num=%u < sp->len()=%u", c_num, sp->len() );

  const uint8_t S = sp->get( c_num );

  return S & style;
}

// Returns number of tabs removed
unsigned RemoveTabs_from_line( FileBuf::Data& m
                             , const unsigned l
                             , const unsigned tab_sz )
{
  Trace trace( __PRETTY_FUNCTION__ );
  unsigned tabs_removed = 0;

  Line* l_c = m.lines[l];
  unsigned LL = l_c->len();
  unsigned cnum_t = 0; // char number with respect to tabs

  for( unsigned p=0; p<LL; p++ )
  {
    const uint8_t C = l_c->get(p);

    if( C != '\t' ) cnum_t += 1;
    else {
      tabs_removed++;
      const unsigned num_spaces = tab_sz-(cnum_t%tab_sz);
      m.self.Set( l, p, ' ', false );
      for( unsigned i=1; i<num_spaces; i++ )
      {
        p++;
        m.self.InsertChar( l, p, ' ');
        LL++;
      }
      cnum_t = 0;
    }
  }
  return tabs_removed;
}

// Returns number of spaces removed
unsigned RemoveSpcs_from_EOL( FileBuf::Data& m, const unsigned l )
{
  Trace trace( __PRETTY_FUNCTION__ );
  unsigned spaces_removed = 0;

  Line* l_c = m.lines[l];
  const unsigned LL = l_c->len();

  if( 0 < LL )
  {
    const unsigned end_C = l_c->get(LL-1);
    const unsigned logical_EOL = end_C == '\r'
                               ? LL-2  // Windows line ending
                               : LL-1; // Unix line ending
    bool done = false;
    for( int p=logical_EOL; !done && -1<p; p-- )
    {
      if( ' ' == l_c->get( p ) )
      {
        m.self.RemoveChar( l, p );
        spaces_removed++;
      }
      else done = true;
    }
  }
  return spaces_removed;
}

void FileBuf::RemoveTabs_SpacesAtEOLs( const unsigned tab_sz )
{
  unsigned num_tabs_removed = 0;
  unsigned num_spcs_removed = 0;

  const unsigned NUM_LINES = m.lines.len();

  for( unsigned l=0; l<NUM_LINES; l++ )
  {
    num_tabs_removed += RemoveTabs_from_line( m, l, tab_sz );
    num_spcs_removed += RemoveSpcs_from_EOL( m, l );
  }
  if( 0 < num_tabs_removed && 0 < num_spcs_removed )
  {
    Update();
    m.vis.CmdLineMessage("Removed %u tabs, %u spaces"
                        , num_tabs_removed, num_spcs_removed );
  }
  else if( 0 < num_tabs_removed )
  {
    Update();
    m.vis.CmdLineMessage("Removed %u tabs", num_tabs_removed );
  }
  else if( 0 < num_spcs_removed )
  {
    Update();
    m.vis.CmdLineMessage("Removed %u spaces", num_spcs_removed );
  }
  else {
    m.vis.CmdLineMessage("No tabs or spaces removed");
  }
}

void FileBuf::dos2unix()
{
  unsigned num_CRs_removed = 0;

  const unsigned NUM_LINES = m.lines.len();

  for( unsigned l=0; l<NUM_LINES; l++ )
  {
    Line* l_c = m.lines[l];
    const unsigned LL = l_c->len();

    if( 0 < LL )
    {
      const unsigned C = l_c->get( LL-1 );

      if( C == '\r' )
      {
        RemoveChar( l, LL-1 );
        num_CRs_removed++;
      }
    }
  }
  if( 0 < num_CRs_removed )
  {
    Update();
    m.vis.CmdLineMessage("Removed %u CRs", num_CRs_removed);
  }
  else {
    m.vis.CmdLineMessage("No CRs removed");
  }
}

void FileBuf::unix2dos()
{
  unsigned num_CRs_added = 0;

  const unsigned NUM_LINES = m.lines.len();

  for( unsigned l=0; l<NUM_LINES; l++ )
  {
    Line* l_c = m.lines[l];
    const unsigned LL = l_c->len();

    if( 0 < LL )
    {
      const unsigned C = l_c->get( LL-1 );

      if( C != '\r' )
      {
        PushChar( l, '\r' );
        num_CRs_added++;
      }
    }
    else {
      PushChar( l, '\r' );
      num_CRs_added++;
    }
  }
  if( 0 < num_CRs_added )
  {
    Update();
    m.vis.CmdLineMessage("Added %u CRs", num_CRs_added);
  }
  else {
    m.vis.CmdLineMessage("No CRs added");
  }
}

// Returns true if this FileBuf has pattern
bool FileBuf::Has_Pattern( const String& pattern ) const
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned k=0; k<NumLines(); k++ )
  {
    const Line* l_k = GetLineP( k );

    if( 0 < l_k->len() )
    {
      if( Line_Has_Pattern( l_k, pattern ) )
      {
        return true;
      }
    }
  }
  return false;
}

bool Comment_CPP( FileBuf::Data& m )
{
  bool commented = false;

  if( FT_CPP  == m.file_type
   || FT_IDL  == m.file_type
   || FT_JAVA == m.file_type
   || FT_JS   == m.file_type
   || FT_STL  == m.file_type )
  {
    const unsigned NUM_LINES = m.self.NumLines();

    // Comment all lines:
    for( unsigned k=0; k<NUM_LINES; k++ )
    {
      m.self.InsertChar( k, 0, '/' );
      m.self.InsertChar( k, 0, '/' );
    }
    commented = true;
  }
  return commented;
}
bool Comment_Script( FileBuf::Data& m )
{
  bool commented = false;

  if( FT_BASH  == m.file_type
   || FT_CMAKE == m.file_type
   || FT_MAKE  == m.file_type
   || FT_PY    == m.file_type )
  {
    const unsigned NUM_LINES = m.self.NumLines();

    // Comment all lines:
    for( unsigned k=0; k<NUM_LINES; k++ )
    {
      m.self.InsertChar( k, 0, '#' );
    }
    commented = true;
  }
  return commented;
}
bool Comment_MIB( FileBuf::Data& m )
{
  bool commented = false;

  if( FT_MIB == m.file_type
   || FT_SQL == m.file_type )
  {
    const unsigned NUM_LINES = m.self.NumLines();

    // Comment all lines:
    for( unsigned k=0; k<NUM_LINES; k++ )
    {
      m.self.InsertChar( k, 0, '-' );
      m.self.InsertChar( k, 0, '-' );
    }
    commented = true;
  }
  return commented;
}

void FileBuf::Comment()
{
  if( Comment_CPP(m)
   || Comment_Script(m)
   || Comment_MIB(m) )
  {
    Update();
  }
}

bool UnComment_CPP( FileBuf::Data& m )
{
  bool uncommented = false;

  if( FT_CPP  == m.file_type
   || FT_IDL  == m.file_type
   || FT_JAVA == m.file_type
   || FT_JS   == m.file_type
   || FT_STL  == m.file_type )
  {
    bool all_lines_commented = true;

    const unsigned NUM_LINES = m.self.NumLines();

    // Determine if all lines are commented:
    for( unsigned k=0; all_lines_commented && k<NUM_LINES; k++ )
    {
      const Line& l_k = m.self.GetLine( k );

      if( (l_k.len() < 2)
       || ('/' != l_k.get( 0 ))
       || ('/' != l_k.get( 1 )) )
      {
        all_lines_commented = false;
      }
    }
    // Un-Comment all lines:
    if( all_lines_commented )
    {
      for( unsigned k=0; k<NUM_LINES; k++ )
      {
        m.self.RemoveChar( k, 0 );
        m.self.RemoveChar( k, 0 );
      }
      uncommented = true;
    }
  }
  return uncommented;
}
bool UnComment_Script( FileBuf::Data& m )
{
  bool uncommented = false;

  if( FT_BASH  == m.file_type
   || FT_CMAKE == m.file_type
   || FT_MAKE  == m.file_type
   || FT_PY    == m.file_type )
  {
    bool all_lines_commented = true;

    const unsigned NUM_LINES = m.self.NumLines();

    // Determine if all lines are commented:
    for( unsigned k=0; all_lines_commented && k<NUM_LINES; k++ )
    {
      const Line& l_k = m.self.GetLine( k );

      if( (l_k.len() < 1)
       || ('#' != l_k.get( 0 )) )
      {
        all_lines_commented = false;
      }
    }
    // Un-Comment all lines:
    if( all_lines_commented )
    {
      for( unsigned k=0; k<NUM_LINES; k++ )
      {
        m.self.RemoveChar( k, 0 );
      }
      uncommented = true;
    }
  }
  return uncommented;
}
bool UnComment_MIB( FileBuf::Data& m )
{
  bool uncommented = false;

  if( FT_MIB == m.file_type
   || FT_SQL == m.file_type )
  {
    bool all_lines_commented = true;

    const unsigned NUM_LINES = m.self.NumLines();

    // Determine if all lines are commented:
    for( unsigned k=0; all_lines_commented && k<NUM_LINES; k++ )
    {
      const Line& l_k = m.self.GetLine( k );

      if( (l_k.len() < 2)
       || ('-' != l_k.get( 0 ))
       || ('-' != l_k.get( 1 )) )
      {
        all_lines_commented = false;
      }
    }
    // Un-Comment all lines:
    if( all_lines_commented )
    {
      for( unsigned k=0; k<NUM_LINES; k++ )
      {
        m.self.RemoveChar( k, 0 );
        m.self.RemoveChar( k, 0 );
      }
      uncommented = true;
    }
  }
  return uncommented;
}

void FileBuf::UnComment()
{
  if( UnComment_CPP(m)
   || UnComment_Script(m)
   || UnComment_MIB(m) )
  {
    Update();
  }
}

