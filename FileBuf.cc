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
#include "Highlight_CPP.hh"
#include "Highlight_Dir.hh"
#include "Highlight_HTML.hh"
#include "Highlight_IDL.hh"
#include "Highlight_Java.hh"
#include "Highlight_JS.hh"
#include "Highlight_Make.hh"
#include "Highlight_CMake.hh"
#include "Highlight_ODB.hh"
#include "Highlight_SQL.hh"
#include "Highlight_STL.hh"
#include "Highlight_Swift.hh"
#include "Highlight_TCL.hh"
#include "Highlight_Text.hh"
#include "Highlight_XML.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

extern const unsigned USER_FILE;  // First user file

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
  String          file_name; // Full path and filename head = path_name + head_name
  String          path_name; // Full path     = file_name - head_name, (for directories this is the same a file_name)
  String          head_name; // Filename head = file_name - path_name, (for directories this is empty)
  bool            is_dir;
  double          mod_time;
  ViewList        views;     // List of views that display this file
  LineView*       line_view;
  bool            need_2_find_stars;
  bool            need_2_clear_stars;

  bool       save_history;
  unsList    lineOffsets; // absolute byte offset of beginning of line in file
  LinesList  lines;    // list of file lines.
  LinesList  styles;   // list of file styles.
  unsigned   hi_touched_line; // Line before which highlighting is valid
  bool       LF_at_EOF; // Line feed at end of file
  File_Type  file_type;
  const bool m_mutable;
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
  , file_name( FILE_NAME )
  , path_name( "" )
  , head_name( "" )
  , is_dir( ::IsDir( file_name.c_str() ) )
  , mod_time( 0 )
  , views(__FILE__, __LINE__)
  , line_view( 0 )
  , need_2_find_stars( true )
  , need_2_clear_stars( false )
  , save_history( false )
  , lineOffsets(__FILE__, __LINE__)
  , lines()
  , styles()
  , hi_touched_line( 0 )
  , LF_at_EOF( true )
  , file_type( FT )
  , m_mutable( MUTABLE )
{
  if( is_dir )
  {
    path_name = file_name;
    if( DIR_DELIM != file_name.get_end() ) file_name.push( DIR_DELIM );
  }
  else {
    path_name = GetFnameTail( file_name.c_str() );
    head_name = GetFnameHead( file_name.c_str() );
  }
}

FileBuf::Data::Data( FileBuf& parent
                   , Vis& vis
                   , const char* const FILE_NAME
                   , const FileBuf& rfb )
  : self( parent )
  , vis( vis )
  , file_name( FILE_NAME )
  , path_name( "" )
  , head_name( "" )
  , is_dir( rfb.m.is_dir )
  , mod_time( rfb.m.mod_time )
  , views(__FILE__, __LINE__)
  , line_view( 0 )
  , need_2_find_stars( true )
  , need_2_clear_stars( false )
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
{
  if( is_dir )
  {
    path_name = file_name;
    if( DIR_DELIM != file_name.get_end() ) file_name.push( DIR_DELIM );
  }
  else {
    path_name = GetFnameTail( file_name.c_str() );
    head_name = GetFnameHead( file_name.c_str() );
  }
}

FileBuf::Data::~Data()
{
}

bool Find_File_Type_Bash( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( m.file_name.ends_with(".sh"      )
   || m.file_name.ends_with(".sh.new"  )
   || m.file_name.ends_with(".sh.old"  )
   || m.file_name.ends_with(".bash"    )
   || m.file_name.ends_with(".bash.new")
   || m.file_name.ends_with(".bash.old")
   || m.file_name.ends_with(".alias"   )
   || m.file_name.ends_with(".bashrc"  )
   || m.file_name.ends_with(".profile" )
   || m.file_name.ends_with(".bash_profile")
   || m.file_name.ends_with(".bash_logout" ) )
  {
    m.file_type = FT_BASH;
    m.pHi = new(__FILE__,__LINE__) Highlight_Bash( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_CPP( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( m.file_name.ends_with(".h"      )
   || m.file_name.ends_with(".h.new"  )
   || m.file_name.ends_with(".h.old"  )
   || m.file_name.ends_with(".c"      )
   || m.file_name.ends_with(".c.new"  )
   || m.file_name.ends_with(".c.old"  )
   || m.file_name.ends_with(".hh"     )
   || m.file_name.ends_with(".hh.new" )
   || m.file_name.ends_with(".hh.old" )
   || m.file_name.ends_with(".cc"     )
   || m.file_name.ends_with(".cc.new" )
   || m.file_name.ends_with(".cc.old" )
   || m.file_name.ends_with(".hpp"    )
   || m.file_name.ends_with(".hpp.new")
   || m.file_name.ends_with(".hpp.old")
   || m.file_name.ends_with(".cpp"    )
   || m.file_name.ends_with(".cpp.new")
   || m.file_name.ends_with(".cpp.old")
   || m.file_name.ends_with(".cxx"    )
   || m.file_name.ends_with(".cxx.new")
   || m.file_name.ends_with(".cxx.old") )
  {
    m.file_type = FT_CPP;
    m.pHi = new(__FILE__,__LINE__) Highlight_CPP( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_IDL( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( m.file_name.ends_with(".idl"    )
   || m.file_name.ends_with(".idl.new")
   || m.file_name.ends_with(".idl.old") )
  {
    m.file_type = FT_IDL;
    m.pHi = new(__FILE__,__LINE__) Highlight_IDL( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_Java( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( m.file_name.ends_with(".java"    )
   || m.file_name.ends_with(".java.new")
   || m.file_name.ends_with(".java.old") )
  {
    m.file_type = FT_JAVA;
    m.pHi = new(__FILE__,__LINE__) Highlight_Java( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_HTML( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( m.file_name.ends_with(".htm"     )
   || m.file_name.ends_with(".htm.new" )
   || m.file_name.ends_with(".htm.old" )
   || m.file_name.ends_with(".html"    )
   || m.file_name.ends_with(".html.new")
   || m.file_name.ends_with(".html.old") )
  {
    m.file_type = FT_HTML;
    m.pHi = new(__FILE__,__LINE__) Highlight_HTML( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_XML( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( m.file_name.ends_with(".xml"       )
   || m.file_name.ends_with(".xml.new"   )
   || m.file_name.ends_with(".xml.old"   )
   || m.file_name.ends_with(".xml.in"    )
   || m.file_name.ends_with(".xml.in.new")
   || m.file_name.ends_with(".xml.in.old") )
  {
    m.file_type = FT_XML;
    m.pHi = new(__FILE__,__LINE__) Highlight_XML( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_JS( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( m.file_name.ends_with(".js"    )
   || m.file_name.ends_with(".js.new")
   || m.file_name.ends_with(".js.old") )
  {
    m.file_type = FT_JS;
    m.pHi = new(__FILE__,__LINE__) Highlight_JS( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_Make( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( m.file_name.ends_with(".Make"       )
   || m.file_name.ends_with(".make"       )
   || m.file_name.ends_with(".Make.new"   )
   || m.file_name.ends_with(".make.new"   )
   || m.file_name.ends_with(".Make.old"   )
   || m.file_name.ends_with(".make.old"   )
   || m.file_name.ends_with("Makefile"    )
   || m.file_name.ends_with("makefile"    )
   || m.file_name.ends_with("Makefile.new")
   || m.file_name.ends_with("makefile.new")
   || m.file_name.ends_with("Makefile.old")
   || m.file_name.ends_with("makefile.old") )
  {
    m.file_type = FT_MAKE;
    m.pHi = new(__FILE__,__LINE__) Highlight_Make( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_CMake( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( m.file_name.ends_with(".cmake"    )
   || m.file_name.ends_with(".cmake.new")
   || m.file_name.ends_with(".cmake.old")
   || m.file_name.ends_with("CMakeLists.txt"    )
   || m.file_name.ends_with("CMakeLists.new.txt")
   || m.file_name.ends_with("CMakeLists.old.txt") )
  {
    m.file_type = FT_CMAKE;
    m.pHi = new(__FILE__,__LINE__) Highlight_CMake( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_SQL( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( m.file_name.ends_with(".sql"    )
   || m.file_name.ends_with(".sql.new")
   || m.file_name.ends_with(".sql.old") )
  {
    m.file_type = FT_SQL;
    m.pHi = new(__FILE__,__LINE__) Highlight_SQL( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_STL( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( ( 4 < LEN && m.file_name.has_at(".stl"    , LEN-4 ) )
   || ( 8 < LEN && m.file_name.has_at(".stl.new", LEN-8 ) )
   || ( 8 < LEN && m.file_name.has_at(".stl.old", LEN-8 ) )
   || ( 4 < LEN && m.file_name.has_at(".ste"    , LEN-4 ) )
   || ( 8 < LEN && m.file_name.has_at(".ste.new", LEN-8 ) )
   || ( 8 < LEN && m.file_name.has_at(".ste.old", LEN-8 ) ) )
  {
    m.file_type = FT_STL;
    m.pHi = new(__FILE__,__LINE__) Highlight_STL( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_ODB( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( m.file_name.ends_with(".odb"    )
   || m.file_name.ends_with(".odb.new")
   || m.file_name.ends_with(".odb.old") )
  {
    m.file_type = FT_ODB;
    m.pHi = new(__FILE__,__LINE__) Highlight_ODB( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_Swift( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( m.file_name.ends_with(".swift"    )
   || m.file_name.ends_with(".swift.new")
   || m.file_name.ends_with(".swift.old") )
  {
    m.file_type = FT_SWIFT;
    m.pHi = new(__FILE__,__LINE__) Highlight_Swift( m.self );
    return true;
  }
  return false;
}

bool Find_File_Type_TCL( FileBuf::Data& m )
{
  const unsigned LEN = m.file_name.len();

  if( m.file_name.ends_with(".tcl"    )
   || m.file_name.ends_with(".tcl.new")
   || m.file_name.ends_with(".tcl.old") )
  {
    m.file_type = FT_TCL;
    m.pHi = new(__FILE__,__LINE__) Highlight_TCL( m.self );
    return true;
  }
  return false;
}

void Find_File_Type_Suffix( FileBuf::Data& m )
{
  if( m.is_dir )
  {
    m.file_type = FT_DIR;
    m.pHi = new(__FILE__,__LINE__) Highlight_Dir( m.self );
  }
  else if( Find_File_Type_Bash ( m )
        || Find_File_Type_CPP  ( m )
        || Find_File_Type_IDL  ( m )
        || Find_File_Type_Java ( m )
        || Find_File_Type_HTML ( m )
        || Find_File_Type_XML  ( m )
        || Find_File_Type_JS   ( m )
        || Find_File_Type_Make ( m )
        || Find_File_Type_CMake( m )
        || Find_File_Type_ODB  ( m )
        || Find_File_Type_SQL  ( m )
        || Find_File_Type_STL  ( m )
        || Find_File_Type_Swift( m )
        || Find_File_Type_TCL  ( m ) )
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
  const unsigned NUM_LINES = m.self.NumLines();

  if( 0 < NUM_LINES )
  {
    Line* lp0 = m.self.GetLineP( 0 );

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
  m.lineOffsets.set_len(__FILE__,__LINE__, HVLO );

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
      bool ok = lp->push(__FILE__,__LINE__, C );
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
    bool ok = lp->push(__FILE__,__LINE__, DIR_DELIM )
           && sp->push(__FILE__,__LINE__, 0 );

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
  const unsigned NUM_LINES = m.self.NumLines();

  // Add terminating NULL to all lines (file names):
  for( unsigned k=0; k<NUM_LINES; k++ ) m.self.PushChar( k, 0 );

  // Sort lines (file names), least to greatest:
  for( unsigned i=NUM_LINES-1; 0<i; i-- )
  {
    for( unsigned k=0; k<i; k++ )
    {
      const char* cp1 = m.lines[k  ]->c_str( 0 );
      const char* cp2 = m.lines[k+1]->c_str( 0 );

      if( 0<strcmp( cp1, cp2 ) ) SwapLines( m, k, k+1 );
    }
  }

  // Move non-directory files to end:
  for( unsigned i=NUM_LINES-1; 0<i; i-- )
  {
    for( unsigned k=0; k<i; k++ )
    {
      const char* cp = m.lines[k]->c_str( 0 );

      if( 0==strcmp( cp, ".." ) || 0==strcmp( cp, "." ) ) continue;

      Line* lp1 = m.lines[k  ]; const unsigned lp1_len = lp1->len();
      Line* lp2 = m.lines[k+1]; const unsigned lp2_len = lp2->len();

      // Since terminating NULLs were previously added to lines (file names),
      // '/'s will be at second to last char:
      if( DIR_DELIM != lp1->get( lp1_len-2 )
       && DIR_DELIM == lp2->get( lp2_len-2 ) ) SwapLines( m, k, k+1 );
    }
  }

  // Remove terminating NULLs just added from all lines (file names):
  for( unsigned k=0; k<NUM_LINES; k++ ) m.self.PopChar( k );
}

void ReadExistingDir( FileBuf::Data& m, DIR* dp, String dir_path )
{
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

// Leave syntax m.styles unchanged, and set star style
void SetStarStyle( FileBuf::Data& m
                 , const unsigned l_num
                 , const unsigned c_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.styles.len(), "l_num < m.styles.len()" );

  Line* sp = m.styles[ l_num ];

  ASSERT( __LINE__, c_num < sp->len(), "c_num < sp->len()" );

  sp->set( c_num, sp->get( c_num ) | HI_STAR );
}

// Find stars starting at st up to but not including fn line number
void Find_Stars_In_Range( FileBuf::Data& m, const CrsPos st, const int fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char*    star_str  = m.vis.GetStar();
  const unsigned STAR_LEN  = m.vis.GetStarLen();
  const bool     SLASH     = m.vis.GetSlash();
  const unsigned NUM_LINES = m.self.NumLines();

  for( unsigned l=st.crsLine; STAR_LEN && l<NUM_LINES && l<fn; l++ )
  {
    Line* lp = m.lines[l];
    const unsigned LL = lp->len();
    if( LL<STAR_LEN ) continue;

    const unsigned st_pos = st.crsLine==l ? st.crsChar : 0;

    for( unsigned p=st_pos; p<LL; p++ )
    {
      bool matches = SLASH || line_start_or_prev_C_non_ident( *lp, p );
      for( unsigned k=0; matches && (p+k)<LL && k<STAR_LEN; k++ )
      {
        if( star_str[k] != lp->get(p+k) ) matches = false;
        else {
          if( k+1 == STAR_LEN ) // Found pattern
          {
            matches = SLASH || line_end_or_non_ident( *lp, LL, p+k );
            if( matches ) {
              for( unsigned n=p; n<p+STAR_LEN; n++ ) SetStarStyle( m, l, n );
              // Increment p one less than STAR_LEN, because p
              // will be incremented again by the for loop
              p += STAR_LEN-1;
            }
          }
        }
      }
    }
  }
}

// Clear stars starting at st up to but not including fn line number
void ClearStars_In_Range( FileBuf::Data& m, const CrsPos st, const int fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.styles.len();

  for( unsigned l=st.crsLine; l<NUM_LINES && l<fn; l++ )
  {
    Line* sp = m.styles[l];
    const unsigned LL = sp->len();
    const unsigned st_pos = st.crsLine==l ? st.crsChar : 0;

    for( unsigned p=st_pos; p<LL; p++ )
    {
      const uint8_t old_S = sp->get( p );
      sp->set( p, old_S & ~HI_STAR );
    }
  }
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

  ClearStars_In_Range( m, st, fn );
  Find_Stars_In_Range( m, st, fn );
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

FileBuf::FileBuf( Vis& vis
                , const char* const FILE_NAME
                , const bool MUTABLE
                , const File_Type FT )
  : m( *new(__FILE__, __LINE__)
        Data( *this, vis, FILE_NAME, MUTABLE, FT )
     )
{
  // Absolute byte offset of beginning of first line in file is always zero:
  m.lineOffsets.push(__FILE__,__LINE__, 0 );

  if( FT == FT_BUFFER_EDITOR )
  {
    m.file_type = FT_BUFFER_EDITOR;
    m.pHi = new(__FILE__,__LINE__) Highlight_BufferEditor( *this );
  }
  else if( FT_UNKNOWN == m.file_type ) Find_File_Type_Suffix( m );

  m.vis.Add_FileBuf_2_Lists_Create_Views( this, m.file_name.c_str() );
}

FileBuf::FileBuf( Vis& vis
                , const char* const FILE_NAME
                , const FileBuf& rfb )
  : m( *new(__FILE__, __LINE__)
        Data( *this, vis, FILE_NAME, rfb )
     )
{
  Find_File_Type_Suffix( m );

  for( unsigned k=0; k<rfb.m.lines.len(); k++ )
  {
    m.lines.push( new(__FILE__,__LINE__) Line( *(rfb.m.lines[k]) ) );
  }
  for( unsigned k=0; k<rfb.m.styles.len(); k++ )
  {
    m.styles.push( new(__FILE__,__LINE__) Line( *(rfb.m.styles[k]) ) );
  }
  m.mod_time = ModificationTime( m.file_name.c_str() );

  m.vis.Add_FileBuf_2_Lists_Create_Views( this, m.file_name.c_str() );
}

FileBuf::~FileBuf()
{
  MemMark(__FILE__,__LINE__); delete m.pHi;
  MemMark(__FILE__,__LINE__); delete &m;
}

bool FileBuf::IsDir() const { return m.is_dir; }
double FileBuf::GetModTime() const { return m.mod_time; }
void FileBuf::SetModTime( const double mt ) { m.mod_time = mt; }
const char* FileBuf::GetFileName() const { return m.file_name.c_str(); }
const char* FileBuf::GetPathName() const { return m.path_name.c_str(); }
const char* FileBuf::GetHeadName() const { return m.head_name.c_str(); }
void FileBuf::NeedToFindStars() { m.need_2_find_stars = true; }
void FileBuf::NeedToClearStars() { m.need_2_clear_stars = true; }

void FileBuf::Set_File_Type( const char* syn )
{
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
    else if( 0==strcmp( syn, "odb") )
    {
      m.file_type = FT_ODB;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_ODB( *this );
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

//void FileBuf::Set_Save_History( const bool val )
//{
//  m.save_history = val;
//}

void FileBuf::AddView( View* v )
{
  m.views.push(__FILE__,__LINE__, v );
}

void FileBuf::AddView( LineView* v )
{
  m.line_view = v;
}

void FileBuf::ReadFile()
{
  if( m.is_dir )
  {
    // Directory
    DIR* dp = opendir( m.file_name.c_str() );
    if( dp ) {
      ReadExistingDir( m, dp, m.file_name.c_str() );
      closedir( dp );
    }
  }
  else {
    // Regular file
    FILE* fp = fopen( m.file_name.c_str(), "rb" );
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
  m.mod_time = ModificationTime( m.file_name.c_str() );
}

void FileBuf::ReReadFile()
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
  m.save_history      = true;
  m.need_2_find_stars = true;
  m.hi_touched_line   = 0;

  m.mod_time = ModificationTime( m.file_name.c_str() );
}

void FileBuf::BufferEditor_Sort()
{
  const unsigned NUM_LINES = NumLines();

  // Add terminating NULL to all lines (file names):
  for( unsigned k=0; k<NUM_LINES; k++ ) PushChar( k, 0 );

  const unsigned NUM_BUILT_IN_FILES = USER_FILE;
  const unsigned FNAME_START_CHAR   = 0;

  // Sort lines (file names), least to greatest:
  for( unsigned i=NUM_LINES-1; NUM_BUILT_IN_FILES<i; i-- )
  {
    for( unsigned k=NUM_BUILT_IN_FILES; k<i; k++ )
    {
      const char* cp1 = m.lines[k  ]->c_str( FNAME_START_CHAR );
      const char* cp2 = m.lines[k+1]->c_str( FNAME_START_CHAR );

      if( 0<strcmp( cp1, cp2 ) ) SwapLines( m, k, k+1 );
    }
  }
  // Remove terminating NULLs just added from all lines (file names):
  for( unsigned k=0; k<NUM_LINES; k++ ) PopChar( k );
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
      bool ok = lp->push(__FILE__,__LINE__, C );
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
      bool ok = lp->push(__FILE__,__LINE__, C );
      if( !ok ) DIE("Line.push() failed");
      m.LF_at_EOF = false;
    }
  }
  if( lp ) PushLine( lp ); //< FileBuf::lines takes ownership of lp
}

void FileBuf::Write()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==m.file_name.len() )
  {
    // No file name message:
    m.vis.CmdLineMessage("No file name to write to");
  }
  else {
    FILE* fp = fopen( m.file_name.c_str(), "wb" );

    if( !fp ) {
      // Could not open file for writing message:
      m.vis.Window_Message("\nCould not open:\n\n%s\n\nfor writing\n\n"
                           , m.file_name.c_str() );
    }
    else {
      const unsigned NUM_LINES = m.lines.len();

      for( unsigned k=0; k<NUM_LINES; k++ )
      {
        const unsigned LL = m.lines[k]->len();
        for( unsigned i=0; i<LL; i++ )
        {
          int c = m.lines[k]->get(i);
          fputc( c, fp );
        }
        if( k<NUM_LINES-1 || m.LF_at_EOF )
        {
          fputc( '\n', fp );
        }
      }
      fclose( fp );

      m.mod_time = ModificationTime( m.file_name.c_str() );

      m.history.Clear();
      // Wrote to file message:
      m.vis.CmdLineMessage("\"%s\" written", m.file_name.c_str() );
    }
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

  // line_num decremented from 0 to -1:
  const unsigned MAX = ~0;
  if( line_num == MAX ) return 0;

  ASSERT( __LINE__, line_num < m.lines.len(), "line_num=%u < m.lines.len()=%u", line_num, m.lines.len() );

  return m.lines[ line_num ]->len();
}

// Get byte on line l_num at position c_num
//
uint8_t FileBuf::Get( const unsigned l_num, const unsigned c_num ) const
{
  Trace trace( __PRETTY_FUNCTION__ );

  ASSERT( __LINE__, l_num < m.lines.len(), "l_num < m.lines.len()" );

  Line* lp = m.lines[ l_num ];

  ASSERT( __LINE__, c_num < lp->len(), "c_num=%u < lp->len()=%u", c_num, lp->len() );

  return lp->get(c_num);
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
  }
}

bool FileBuf::Has_LF_at_EOF() { return m.LF_at_EOF; }

// Return copy of line l_num
//
Line FileBuf::GetLine( const unsigned l_num ) const
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < m.lines.len(), "l_num < m.lines.len()" );

  Line* lp  = m.lines[ l_num ];
  ASSERT( __LINE__, lp, "m.lines[ %u ]", l_num );

  return *lp;
}

// Return copy of line l_num
//
Line FileBuf::GetStyle( const unsigned l_num ) const
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

  l = *(m.lines[ l_num ]);
}

Line* FileBuf::GetLineP( const unsigned l_num ) const
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

  bool ok = m.lines.insert( l_num, lp ) && m.styles.insert( l_num, sp );

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

  bool ok = m.lines.insert( l_num, pLine ) && m.styles.insert( l_num, sp );

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

  bool ok = m.lines.insert( l_num, lp ) && m.styles.insert( l_num, sp );

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

  bool ok = lp->insert(__FILE__,__LINE__, c_num, C )
         && sp->insert(__FILE__,__LINE__, c_num, 0 );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( m, l_num );

  if( SavingHist( m ) ) m.history.Save_InsertChar( l_num, c_num );
}

// Add a new line at the end of FileBuf, which is a copy of line
//
void FileBuf::PushLine( const Line& line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = m.vis.BorrowLine( __FILE__,__LINE__,  line );
  Line* sp = m.vis.BorrowLine( __FILE__,__LINE__,  line.len(), 0 );
  ASSERT( __LINE__, lp->len() == sp->len(), "(lp->len()=%u) != (sp->len()=%u)", lp->len(), sp->len() );

  bool ok = m.lines.push( lp ) && m.styles.push( sp );

  ASSERT( __LINE__, ok, "ok" );

  if( SavingHist( m ) ) m.history.Save_InsertLine( m.lines.len()-1 );
}

// Add a new pLine to the end of FileBuf.
// FileBuf will delete pLine.
//
void FileBuf::PushLine( Line* const pLine )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* sp = m.vis.BorrowLine( __FILE__,__LINE__,  pLine->len(), 0 );
  ASSERT( __LINE__, pLine->len() == sp->len(), "(pLine->len()=%u) != (sp->len()=%u)", pLine->len(), sp->len() );

  bool ok = m.lines.push( pLine )
        && m.styles.push( sp );

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

  bool ok = m.lines.push( lp ) && m.styles.push( sp );

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

  bool ok = lp->push(__FILE__,__LINE__, C )
         && sp->push(__FILE__,__LINE__, 0 );

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
  bool ok = lp->pop( C ) && sp->pop();

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
  bool ok = m.lines.remove( l_num, lp ) && m.styles.remove( l_num, sp );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( m, l_num );

  line = *lp;

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
  bool ok = m.lines.remove( l_num, pLine ) && m.styles.remove( l_num, sp );
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
  bool ok = m.lines.remove( l_num, lp ) && m.styles.remove( l_num, sp );

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
  bool ok = lp->remove( c_num, C ) && sp->remove( c_num );

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
  bool ok = m.lines.pop( lp ) && m.styles.pop( sp );

  ASSERT( __LINE__, ok, "ok" );

  line = *lp;

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
    bool ok = m.lines.pop( lp ) && m.styles.pop( sp );

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

  Line* lp =  m.lines[ l_num ];
  Line* sp = m.styles[ l_num ];

  bool ok = lp->append(__FILE__,__LINE__, line );
  ASSERT( __LINE__, ok, "ok" );

  // Simply need to increase sp's length to match lp's new length:
  for( unsigned k=0; k<line.len(); k++ ) sp->push(__FILE__,__LINE__, 0 );

  ChangedLine( m, l_num );

  const unsigned first_insert = lp->len() - line.len();

  if( SavingHist( m ) )
  {
    for( unsigned k=0; k<line.len(); k++ )
    {
      m.history.Save_InsertChar( l_num, first_insert + k );
    }
  }
//m.hi_touched_line = Min( m.hi_touched_line, l_num );
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

  bool ok = lp->append(__FILE__,__LINE__, *pLine );
  ASSERT( __LINE__, ok, "ok" );

  // Simply need to increase sp's length to match lp's new length:
  for( unsigned k=0; k<pLine->len(); k++ ) sp->push(__FILE__,__LINE__, 0 );

  ChangedLine( m, l_num );

  const unsigned first_insert = lp->len() - pLine->len();

  if( SavingHist( m ) )
  {
    for( unsigned k=0; k<pLine->len(); k++ )
    {
      m.history.Save_InsertChar( l_num, first_insert + k );
    }
  }
//m.hi_touched_line = Min( m.hi_touched_line, l_num );

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
}

void FileBuf::Undo( View_IF& rV )
{
  if( m.save_history )
  {
    m.save_history = false;

    m.history.Undo( rV );

    m.save_history = true;
  }
}

void FileBuf::UndoAll( View_IF& rV )
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
    if( 0 == m.lineOffsets.len() ) m.lineOffsets.push(__FILE__,__LINE__, 0 );

    // Old line offsets length:
    const unsigned OLOL = m.lineOffsets.len();

    // New line offsets length:
    m.lineOffsets.set_len(__FILE__,__LINE__, NUM_LINES );

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
    if( 0 == m.lineOffsets.len() ) m.lineOffsets.push(__FILE__,__LINE__, 0 );

    // HVLO = Highest valid line offset
    const unsigned HVLO = m.lineOffsets.len()-1;

    if( HVLO < CL )
    {
      m.lineOffsets.set_len(__FILE__,__LINE__, CL+1 );

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
        m.self.ClearStars();
        m.self.Find_Stars();

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

  if( m.vis.RunningDot() ) return;

  UpdateWinViews( m, true );

  Console::Update();

  // Put cursor back into current window
  m.vis.CV()->PrintCursor();
}

void FileBuf::UpdateCmd()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.vis.RunningDot() ) return;

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
      // Find_Styles() does not need to be called every time the user
      // scrolls down another line.  Find_Styles() will only be called
      // once for every EXTRA_LINES scrolled down.
      const unsigned EXTRA_LINES = 10;

      CrsPos   st = Update_Styles_Find_St( m, m.hi_touched_line );
      unsigned fn = Min( up_to_line+EXTRA_LINES, NUM_LINES );

      Find_Styles_In_Range( m, st, fn );

      m.hi_touched_line = fn;
    }
  }
}

void FileBuf::Find_Stars()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( !m.need_2_find_stars ) return;

  const char*    star_str  = m.vis.GetStar();
  const unsigned STAR_LEN  = m.vis.GetStarLen();
  const bool     SLASH     = m.vis.GetSlash();
  const unsigned NUM_LINES = NumLines();

  for( unsigned l=0; STAR_LEN && l<NUM_LINES; l++ )
  {
    Line* lp = m.lines[l];
    const unsigned LL = lp->len();
    if( LL<STAR_LEN ) continue;

    for( unsigned p=0; p<LL; p++ )
    {
      bool matches = SLASH || line_start_or_prev_C_non_ident( *lp, p );
      for( unsigned k=0; matches && (p+k)<LL && k<STAR_LEN; k++ )
      {
        if( star_str[k] != lp->get(p+k) ) matches = false;
        else {
          if( k+1 == STAR_LEN ) // Found pattern
          {
            matches = SLASH || line_end_or_non_ident( *lp, LL, p+k );
            if( matches ) {
              for( unsigned n=p; n<p+STAR_LEN; n++ ) SetStarStyle( m, l, n );
              // Increment p one less than STAR_LEN, because p
              // will be incremented again by the for loop
              p += STAR_LEN-1;
            }
          }
        }
      }
    }
  }
  m.need_2_find_stars = false;
}

void FileBuf::ClearStars()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.need_2_clear_stars )
  {
    const unsigned NUM_LINES = m.styles.len();

    for( unsigned l=0; l<NUM_LINES; l++ )
    {
      Line* sp = m.styles[l];
      const unsigned LL = sp->len();

      for( unsigned p=0; p<LL; p++ )
      {
        const uint8_t old_S = sp->get( p );
        sp->set( p, old_S & ~HI_STAR );
      }
    }
    m.need_2_clear_stars = false;
  }
}

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


