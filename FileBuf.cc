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

#include "Console.hh"
#include "Key.hh"
#include "Utilities.hh"
#include "MemLog.hh"
#include "View.hh"
#include "Vis.hh"
#include "FileBuf.hh"
#include "Highlight_Bash.hh"
#include "Highlight_CPP.hh"
#include "Highlight_HTML.hh"
#include "Highlight_IDL.hh"
#include "Highlight_Java.hh"
#include "Highlight_JS.hh"
#include "Highlight_ODB.hh"
#include "Highlight_SQL.hh"
#include "Highlight_STL.hh"
#include "Highlight_Swift.hh"
#include "Highlight_TCL.hh"
#include "Highlight_Text.hh"
#include "Highlight_XML.hh"

extern Vis* gl_pVis;
extern Key* gl_pKey;
extern MemLog<MEM_LOG_BUF_SIZE> Log;

FileBuf::FileBuf( const char* const FILE_NAME
                , const bool MUTABLE
                , const File_Type FT )
  : file_name( FILE_NAME )
  , is_dir( false )
  , mod_time( 0 )
  , views(__FILE__, __LINE__)
  , need_2_find_stars( true )
  , need_2_clear_stars( false )
  , history( *this )
  , save_history( false )
  , lineOffsets(__FILE__, __LINE__)
  , lines()
  , styles()
  , hi_touched_line( 0 )
  , LF_at_EOF( true )
  , file_type( FT )
  , pHi( 0 )
  , m_mutable( MUTABLE )
{
  // Absolute byte offset of beginning of first line in file is always zero:
  lineOffsets.push(__FILE__,__LINE__, 0 );

  if( file_name.get_end() == DIR_DELIM ) is_dir = true;

  if( FT_UNKNOWN == file_type ) Find_File_Type_Suffix();
}

FileBuf::FileBuf( const char* const FILE_NAME, const FileBuf& rfb )
  : file_name( FILE_NAME )
  , is_dir( rfb.is_dir )
  , mod_time( rfb.mod_time )
  , views(__FILE__, __LINE__)
  , need_2_find_stars( true )
  , need_2_clear_stars( false )
  , history( *this )
  , save_history( is_dir ? false : true )
  , lineOffsets ( rfb.lineOffsets )
  , lines()
  , styles()
  , hi_touched_line( 0 )
  , LF_at_EOF( rfb.LF_at_EOF )
  , file_type( rfb.file_type )
  , pHi( 0 )
  , m_mutable( true )
{
  Find_File_Type_Suffix();

  for( unsigned k=0; k<rfb.lines.len(); k++ )
  {
    lines.push( new(__FILE__,__LINE__) Line( *(rfb.lines[k]) ) );
  }
  for( unsigned k=0; k<rfb.styles.len(); k++ )
  {
    styles.push( new(__FILE__,__LINE__) Line( *(rfb.styles[k]) ) );
  }
  // Add this file buffer to global list of files
  gl_pVis->files.push(__FILE__,__LINE__, this );
  // Create a new view for each window for this FileBuf
  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* pV  = new(__FILE__,__LINE__) View( this );
    bool ok = gl_pVis->views[w].push(__FILE__,__LINE__, pV );
    ASSERT( __LINE__, ok, "ok" );
    AddView( pV );
  }
  // Push file name onto buffer editor buffer
  gl_pVis->AddToBufferEditor( file_name.c_str() );

  mod_time = ModificationTime( file_name.c_str() );
}

FileBuf::~FileBuf()
{
  MemMark(__FILE__,__LINE__);
  delete pHi;
}

void FileBuf::Find_File_Type_Suffix()
{
  if( Find_File_Type_Bash ()
   || Find_File_Type_CPP  ()
   || Find_File_Type_IDL  ()
   || Find_File_Type_Java ()
   || Find_File_Type_HTML ()
   || Find_File_Type_XML  ()
   || Find_File_Type_JS   ()
   || Find_File_Type_ODB  ()
   || Find_File_Type_SQL  ()
   || Find_File_Type_STL  ()
   || Find_File_Type_Swift()
   || Find_File_Type_TCL  () )
  {
    // File type found
  }
  else if( is_dir )
  {
    // File type NOT found, so for directories default to TEXT.
    // For files, the file type will be found in Find_File_Type_FirstLine()
    file_type = FT_TEXT;
    pHi = new(__FILE__,__LINE__) Highlight_Text( *this );
  }
}

bool FileBuf::Find_File_Type_Bash()
{
  const unsigned LEN = file_name.len();

  if( ( 3 < LEN && file_name.has_at(".sh"      , LEN-3 ) )
   || ( 7 < LEN && file_name.has_at(".sh.new"  , LEN-7 ) )
   || ( 7 < LEN && file_name.has_at(".sh.old"  , LEN-7 ) )
   || ( 5 < LEN && file_name.has_at(".bash"    , LEN-5 ) )
   || ( 9 < LEN && file_name.has_at(".bash.new", LEN-9 ) )
   || ( 9 < LEN && file_name.has_at(".bash.old", LEN-9 ) ) )
  {
    file_type = FT_BASH;
    pHi = new(__FILE__,__LINE__) Highlight_Bash( *this );
    return true;
  }
  return false;
}

bool FileBuf::Find_File_Type_CPP()
{
  const unsigned LEN = file_name.len();

  if( ( 2 < LEN && file_name.has_at(".h"      , LEN-2 ) )
   || ( 6 < LEN && file_name.has_at(".h.new"  , LEN-6 ) )
   || ( 6 < LEN && file_name.has_at(".h.old"  , LEN-6 ) )
   || ( 2 < LEN && file_name.has_at(".c"      , LEN-2 ) )
   || ( 6 < LEN && file_name.has_at(".c.new"  , LEN-6 ) )
   || ( 6 < LEN && file_name.has_at(".c.old"  , LEN-6 ) )
   || ( 3 < LEN && file_name.has_at(".hh"     , LEN-3 ) )
   || ( 7 < LEN && file_name.has_at(".hh.new" , LEN-7 ) )
   || ( 7 < LEN && file_name.has_at(".hh.old" , LEN-7 ) )
   || ( 3 < LEN && file_name.has_at(".cc"     , LEN-3 ) )
   || ( 7 < LEN && file_name.has_at(".cc.new" , LEN-7 ) )
   || ( 7 < LEN && file_name.has_at(".cc.old" , LEN-7 ) )
   || ( 4 < LEN && file_name.has_at(".hpp"    , LEN-4 ) )
   || ( 8 < LEN && file_name.has_at(".hpp.new", LEN-8 ) )
   || ( 8 < LEN && file_name.has_at(".hpp.old", LEN-8 ) )
   || ( 4 < LEN && file_name.has_at(".cpp"    , LEN-4 ) )
   || ( 8 < LEN && file_name.has_at(".cpp.new", LEN-8 ) )
   || ( 8 < LEN && file_name.has_at(".cpp.old", LEN-8 ) )
   || ( 4 < LEN && file_name.has_at(".cxx"    , LEN-4 ) )
   || ( 8 < LEN && file_name.has_at(".cxx.new", LEN-8 ) )
   || ( 8 < LEN && file_name.has_at(".cxx.old", LEN-8 ) ) )
  {
    file_type = FT_CPP;
    pHi = new(__FILE__,__LINE__) Highlight_CPP( *this );
    return true;
  }
  return false;
}

bool FileBuf::Find_File_Type_IDL()
{
  const unsigned LEN = file_name.len();

  if( ( 4 < LEN && file_name.has_at(".idl"      , LEN-4 ) )
   || ( 8 < LEN && file_name.has_at(".idl.new"  , LEN-8 ) )
   || ( 8 < LEN && file_name.has_at(".idl.old"  , LEN-8 ) ) )
  {
    file_type = FT_IDL;
    pHi = new(__FILE__,__LINE__) Highlight_IDL( *this );
    return true;
  }
  return false;
}

bool FileBuf::Find_File_Type_Java()
{
  const unsigned LEN = file_name.len();

  if( ( 5 < LEN && file_name.has_at(".java"      , LEN-5 ) )
   || ( 9 < LEN && file_name.has_at(".java.new"  , LEN-9 ) )
   || ( 9 < LEN && file_name.has_at(".java.old"  , LEN-9 ) ) )
  {
    file_type = FT_JAVA;
    pHi = new(__FILE__,__LINE__) Highlight_Java( *this );
    return true;
  }
  return false;
}

bool FileBuf::Find_File_Type_HTML()
{
  const unsigned LEN = file_name.len();

  if( ( 4 < LEN && file_name.has_at(".htm"     , LEN-4 ) )
   || ( 8 < LEN && file_name.has_at(".htm.new" , LEN-8 ) )
   || ( 8 < LEN && file_name.has_at(".htm.old" , LEN-8 ) )
   || ( 5 < LEN && file_name.has_at(".html"    , LEN-5 ) )
   || ( 9 < LEN && file_name.has_at(".html.new", LEN-9 ) )
   || ( 9 < LEN && file_name.has_at(".html.old", LEN-9 ) ) )
  {
    file_type = FT_HTML;
    pHi = new(__FILE__,__LINE__) Highlight_HTML( *this );
    return true;
  }
  return false;
}

bool FileBuf::Find_File_Type_XML()
{
  const unsigned LEN = file_name.len();

  if( ( 4 < LEN && file_name.has_at(".xml"     , LEN-4 ) )
   || ( 8 < LEN && file_name.has_at(".xml.new" , LEN-8 ) )
   || ( 8 < LEN && file_name.has_at(".xml.old" , LEN-8 ) ) )
  {
    file_type = FT_XML;
    pHi = new(__FILE__,__LINE__) Highlight_XML( *this );
    return true;
  }
  return false;
}

bool FileBuf::Find_File_Type_JS()
{
  const unsigned LEN = file_name.len();

  if( ( 3 < LEN && file_name.has_at(".js"    , LEN-3 ) )
   || ( 7 < LEN && file_name.has_at(".js.new", LEN-7 ) )
   || ( 7 < LEN && file_name.has_at(".js.old", LEN-7 ) ) )
  {
    file_type = FT_JS;
    pHi = new(__FILE__,__LINE__) Highlight_JS( *this );
    return true;
  }
  return false;
}

bool FileBuf::Find_File_Type_SQL()
{
  const unsigned LEN = file_name.len();

  if( ( 4 < LEN && file_name.has_at(".sql"    , LEN-4 ) )
   || ( 8 < LEN && file_name.has_at(".sql.new", LEN-8 ) )
   || ( 8 < LEN && file_name.has_at(".sql.old", LEN-8 ) ) )
  {
    file_type = FT_SQL;
    pHi = new(__FILE__,__LINE__) Highlight_SQL( *this );
    return true;
  }
  return false;
}

bool FileBuf::Find_File_Type_STL()
{
  const unsigned LEN = file_name.len();

  if( ( 4 < LEN && file_name.has_at(".stl"    , LEN-4 ) )
   || ( 8 < LEN && file_name.has_at(".stl.new", LEN-8 ) )
   || ( 8 < LEN && file_name.has_at(".stl.old", LEN-8 ) )
   || ( 4 < LEN && file_name.has_at(".ste"    , LEN-4 ) )
   || ( 8 < LEN && file_name.has_at(".ste.new", LEN-8 ) )
   || ( 8 < LEN && file_name.has_at(".ste.old", LEN-8 ) ) )
  {
    file_type = FT_STL;
    pHi = new(__FILE__,__LINE__) Highlight_STL( *this );
    return true;
  }
  return false;
}

bool FileBuf::Find_File_Type_ODB()
{
  const unsigned LEN = file_name.len();

  if( ( 4 < LEN && file_name.has_at(".odb"    , LEN-4 ) )
   || ( 8 < LEN && file_name.has_at(".odb.new", LEN-8 ) )
   || ( 8 < LEN && file_name.has_at(".odb.old", LEN-8 ) ) )
  {
    file_type = FT_ODB;
    pHi = new(__FILE__,__LINE__) Highlight_ODB( *this );
    return true;
  }
  return false;
}

bool FileBuf::Find_File_Type_Swift()
{
  const unsigned LEN = file_name.len();

  if( ( 6  < LEN && file_name.has_at(".swift"    , LEN-6 ) )
   || ( 10 < LEN && file_name.has_at(".swift.new", LEN-10 ) )
   || ( 10 < LEN && file_name.has_at(".swift.old", LEN-10 ) ) )
  {
    file_type = FT_SWIFT;
    pHi = new(__FILE__,__LINE__) Highlight_Swift( *this );
    return true;
  }
  return false;
}

bool FileBuf::Find_File_Type_TCL()
{
  const unsigned LEN = file_name.len();

  if( ( 4 < LEN && file_name.has_at(".tcl"    , LEN-4 ) )
   || ( 8 < LEN && file_name.has_at(".tcl.new", LEN-8 ) )
   || ( 8 < LEN && file_name.has_at(".tcl.old", LEN-8 ) ) )
  {
    file_type = FT_TCL;
    pHi = new(__FILE__,__LINE__) Highlight_TCL( *this );
    return true;
  }
  return false;
}

void FileBuf::Find_File_Type_FirstLine()
{
  const unsigned NUM_LINES = NumLines();

  if( 0 < NUM_LINES )
  {
    Line* lp0 = GetLineP( 0 );

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
        file_type = FT_BASH;
        pHi = new(__FILE__,__LINE__) Highlight_Bash( *this );
      }
    }
  }
  if( FT_UNKNOWN == file_type )
  {
    // File type NOT found, so default to TEXT
    file_type = FT_TEXT;
    pHi = new(__FILE__,__LINE__) Highlight_Text( *this );
  }
}

void FileBuf::Set_File_Type( const char* syn )
{
  if( !is_dir )
  {
    bool found_syntax_type = true;
    Highlight_Base* p_new_Hi = 0;

    if( 0==strcmp( syn, "sh")
     || 0==strcmp( syn, "bash") )
    {
      file_type = FT_BASH;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_Bash( *this );
    }
  //else if( 0==strcmp( syn, "cmake") )
  //{
  //  file_type = FT_CMAKE;
  //  p_new_Hi = new(__FILE__,__LINE__) Highlight_CMAKE( this );
  //}
    else if( 0==strcmp( syn, "c")
          || 0==strcmp( syn, "cpp") )
    {
      file_type = FT_CPP;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_CPP( *this );
    }
    else if( 0==strcmp( syn, "htm")
          || 0==strcmp( syn, "html") )
    {
      file_type = FT_HTML;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_HTML( *this );
    }
    else if( 0==strcmp( syn, "idl") )
    {
      file_type = FT_IDL;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_IDL( *this );
    }
    else if( 0==strcmp( syn, "java") )
    {
      file_type = FT_JAVA;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_Java( *this );
    }
    else if( 0==strcmp( syn, "js") )
    {
      file_type = FT_JS;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_JS( *this );
    }
    else if( 0==strcmp( syn, "odb") )
    {
      file_type = FT_ODB;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_ODB( *this );
    }
    else if( 0==strcmp( syn, "sql") )
    {
      file_type = FT_SQL;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_SQL( *this );
    }
    else if( 0==strcmp( syn, "ste")
          || 0==strcmp( syn, "stl") )
    {
      file_type = FT_STL;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_STL( *this );
    }
    else if( 0==strcmp( syn, "swift") )
    {
      file_type = FT_SWIFT;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_Swift( *this );
    }
    else if( 0==strcmp( syn, "tcl") )
    {
      file_type = FT_TCL;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_TCL( *this );
    }
    else if( 0==strcmp( syn, "text") )
    {
      file_type = FT_TEXT;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_Text( *this );
    }
    else if( 0==strcmp( syn, "xml") )
    {
      file_type = FT_XML;
      p_new_Hi = new(__FILE__,__LINE__) Highlight_XML( *this );
    }
    else {
      found_syntax_type = false;
    }
    if( found_syntax_type )
    {
      if( 0 != pHi )
      {
        MemMark(__FILE__,__LINE__);
        delete pHi;
      }
      pHi = p_new_Hi;

      hi_touched_line = 0;

      Update();
    }
  }
}

void FileBuf::AddView( View* v )
{
  views.push(__FILE__,__LINE__, v );
}

void FileBuf::ReadFile()
{
  if( is_dir )
  {
    // Directory
    DIR* dp = opendir( file_name.c_str() );
    if( dp ) {
      ReadExistingDir( dp, file_name.c_str() );
      closedir( dp );
    }
  }
  else {
    // Regular file
    FILE* fp = fopen( file_name.c_str(), "rb" );
    if( fp )
    {
      ReadExistingFile( fp );
      fclose( fp );
    }
    else {
      // File does not exist, so add an empty line:
      PushLine();
    }
    save_history = true;
  }
  // Add this file buffer to global list of files
  gl_pVis->files.push(__FILE__,__LINE__, this );
  // Create a new view for each window for this FileBuf
  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* pV  = new(__FILE__,__LINE__) View( this );
    bool ok = gl_pVis->views[w].push(__FILE__,__LINE__, pV );
    ASSERT( __LINE__, ok, "ok" );
    AddView( pV );
  }
  // Push file name onto buffer editor buffer
  gl_pVis->AddToBufferEditor( file_name.c_str() );

  mod_time = ModificationTime( file_name.c_str() );
}

void FileBuf::ReReadFile()
{
  ClearChanged();
  ClearLines();

  save_history = false;

  if( is_dir )
  {
    // Directory
    DIR* dp = opendir( file_name.c_str() );
    if( dp ) {
      ReadExistingDir( dp, file_name.c_str() );
      closedir( dp );
    }
  }
  else {
    // Regular file
    FILE* fp = fopen( file_name.c_str(), "rb" );
    if( fp )
    {
      ReadExistingFile( fp );
      fclose( fp );
    }
    else {
      // File does not exist, so add an empty line:
      PushLine();
    }
  }
  // To be safe, put cursor at top,left of each view of this file:
  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* const pV = views[w];
    pV->topLine  = 0;
    pV->leftChar = 0;
    pV->crsRow = 0;
    pV->crsCol = 0;
  }
  save_history = true;
  need_2_find_stars = true;
  hi_touched_line   = 0;

  mod_time = ModificationTime( file_name.c_str() );
}

void FileBuf::ReadExistingFile( FILE* fp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, fp, "fp" );

  Line* lp = 0;
  int C = 0;
  while( EOF != (C = fgetc( fp )) )
  {
    if( 0==lp ) lp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

    if( '\n' == C )
    {
      PushLine( lp ); //< FileBuf::lines takes ownership of lp
      lp = 0;
      LF_at_EOF = true;
    }
    else {
      bool ok = lp->push(__FILE__,__LINE__, C );
      if( !ok ) DIE("Line.push() failed");
      LF_at_EOF = false;
    }
  }
  if( lp ) PushLine( lp ); //< FileBuf::lines takes ownership of lp

  save_history = true;

  if( FT_UNKNOWN == file_type )
  {
    Find_File_Type_FirstLine();
  }
}

void FileBuf::ReadExistingDir( DIR* dp, String dir_path )
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
      const unsigned LINE_NUM = NumLines();
      PushLine();

      for( unsigned k=0; k<fname_len; k++ )
      {
        PushChar( LINE_NUM, fname[k] );
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
        if( IS_DIR ) PushChar( LINE_NUM, DIR_DELIM );
        else if( IS_LNK ) ReadExistingDir_AddLink( dir_path_fname, LINE_NUM );
      }
    }
  }
  ReadExistingDir_Sort();
}

// Add symbolic link info, i.e., -> symbolic_link_path, to file name
//
void FileBuf::ReadExistingDir_AddLink( const String& dir_path_fname
                                     , const unsigned LINE_NUM )
{
#ifndef WIN32
  const unsigned mbuf_sz = 1024;
  char mbuf[ 1024 ];
  int rval = readlink( dir_path_fname.c_str(), mbuf, mbuf_sz );
  if( 0 < rval )
  {
    PushChar( LINE_NUM, ' ');
    PushChar( LINE_NUM, '-');
    PushChar( LINE_NUM, '>');
    PushChar( LINE_NUM, ' ');

    for( unsigned k=0; k<rval; k++ )
    {
      PushChar( LINE_NUM, mbuf[k] );
    }
  }
#endif
}

void FileBuf::ReadExistingDir_Sort()
{
  const unsigned NUM_LINES = NumLines();

  // Add terminating NULL to all lines (file names):
  for( unsigned k=0; k<NUM_LINES; k++ ) PushChar( k, 0 );

  // Sort lines (file names), least to greatest:
  for( unsigned i=NUM_LINES-1; 0<i; i-- )
  {
    for( unsigned k=0; k<i; k++ )
    {
      const char* cp1 = lines[k  ]->c_str( 0 );
      const char* cp2 = lines[k+1]->c_str( 0 );

      if( 0<strcmp( cp1, cp2 ) ) SwapLines( k, k+1 );
    }
  }

  // Move non-directory files to end:
  for( unsigned i=NUM_LINES-1; 0<i; i-- )
  {
    for( unsigned k=0; k<i; k++ )
    {
      const char* cp = lines[k]->c_str( 0 );

      if( 0==strcmp( cp, ".." ) || 0==strcmp( cp, "." ) ) continue;

      Line* lp1 = lines[k  ]; const unsigned lp1_len = lp1->len();
      Line* lp2 = lines[k+1]; const unsigned lp2_len = lp2->len();

      // Since terminating NULLs were previously added to lines (file names),
      // '/'s will be at second to last char:
      if( DIR_DELIM != lp1->get( lp1_len-2 )
       && DIR_DELIM == lp2->get( lp2_len-2 ) ) SwapLines( k, k+1 );
    }
  }

  // Remove terminating NULLs just added from all lines (file names):
  for( unsigned k=0; k<NUM_LINES; k++ ) PopChar( k );
}

void FileBuf::BufferEditor_Sort()
{
  const unsigned NUM_LINES = NumLines();

  // Add terminating NULL to all lines (file names):
  for( unsigned k=0; k<NUM_LINES; k++ ) PushChar( k, 0 );

  const unsigned NUM_BUILT_IN_FILES = 5;
  const unsigned FNAME_START_CHAR   = 0;

  // Sort lines (file names), least to greatest:
  for( unsigned i=NUM_LINES-1; NUM_BUILT_IN_FILES<i; i-- )
  {
    for( unsigned k=NUM_BUILT_IN_FILES; k<i; k++ )
    {
      const char* cp1 = lines[k  ]->c_str( FNAME_START_CHAR );
      const char* cp2 = lines[k+1]->c_str( FNAME_START_CHAR );

      if( 0<strcmp( cp1, cp2 ) ) SwapLines( k, k+1 );
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
    if( 0==lp ) lp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

    if( '\n' == C )
    {
      PushLine( lp ); //< FileBuf::lines takes ownership of lp
      lp = 0;
      LF_at_EOF = true;
    }
    else {
      bool ok = lp->push(__FILE__,__LINE__, C );
      if( !ok ) DIE("Line.push() failed");
      LF_at_EOF = false;
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
    if( 0==lp ) lp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

    const int C = line.get( k );

    if( '\n' == C )
    {
      PushLine( lp ); //< FileBuf::lines takes ownership of lp
      lp = 0;
      LF_at_EOF = true;
    }
    else {
      bool ok = lp->push(__FILE__,__LINE__, C );
      if( !ok ) DIE("Line.push() failed");
      LF_at_EOF = false;
    }
  }
  if( lp ) PushLine( lp ); //< FileBuf::lines takes ownership of lp
}

void FileBuf::Write()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==file_name.len() )
  {
    // No file name message:
    gl_pVis->CmdLineMessage("No file name to write to");
  }
  else {
    FILE* fp = fopen( file_name.c_str(), "wb" );

    if( !fp ) {
      // Could not open file for writing message:
      gl_pVis->Window_Message("\nCould not open:\n\n%s\n\nfor writing\n\n"
                             , file_name.c_str() );
    }
    else {
      const unsigned NUM_LINES = lines.len();

      for( unsigned k=0; k<NUM_LINES; k++ )
      {
        const unsigned LL = lines[k]->len();
        for( unsigned i=0; i<LL; i++ )
        {
          int c = lines[k]->get(i);
          fputc( c, fp );
        }
        if( k<NUM_LINES-1 || LF_at_EOF )
        {
          fputc( '\n', fp );
        }
      }
      fclose( fp );

      mod_time = ModificationTime( file_name.c_str() );

      history.Clear();
      // Wrote to file message:
      gl_pVis->CmdLineMessage("\"%s\" written", file_name.c_str() );
    }
  }
}

// Return number of lines in file
//
unsigned FileBuf::NumLines() const
{
  Trace trace( __PRETTY_FUNCTION__ );

  return lines.len();
}

// Returns length of line line_num
//
unsigned FileBuf::LineLen( const unsigned line_num ) const
{
  Trace trace( __PRETTY_FUNCTION__ );

  // line_num decremented from 0 to -1:
  const unsigned MAX = ~0;
  if( line_num == MAX ) return 0;

  ASSERT( __LINE__, line_num < lines.len(), "line_num=%u < lines.len()=%u", line_num, lines.len() );

  return lines[ line_num ]->len();
}

// Get byte on line l_num at position c_num
//
uint8_t FileBuf::Get( const unsigned l_num, const unsigned c_num ) const
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < lines.len(), "l_num < lines.len()" );

  Line* lp = lines[ l_num ];

  ASSERT( __LINE__, c_num < lp->len(), "c_num=%u < lp->len()=%u", c_num, lp->len() );

  return lp->get(c_num);
}

// Get byte on line l_num at end of line
//
uint8_t FileBuf::GetEnd( const unsigned l_num ) const
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < lines.len(), "l_num < lines.len()" );

  Line* lp = lines[ l_num ];

  ASSERT( __LINE__, lp->len(), "lp->len()" );

  return lp->get(lp->len()-1);
}

// Set byte on line l_num at position c_num
//
void FileBuf::Set( const unsigned l_num
                 , const unsigned c_num
                 , const uint8_t  C
                 , const bool     continue_last_update )
{
  Trace trace( __PRETTY_FUNCTION__ );

  ASSERT( __LINE__, l_num < lines.len(), "l_num < lines.len()" );

  Line* lp = lines[ l_num ];

  ASSERT( __LINE__, c_num < lp->len(), "c_num < lp->len()" );

  const uint8_t old_C = lp->get(c_num);

  if( old_C != C )
  {
    lp->set( c_num, C );

    if( SavingHist() )
    {
      history.Save_Set( l_num, c_num, old_C, continue_last_update );
    }
    hi_touched_line = Min( hi_touched_line, l_num );
  }
}

// Return copy of line l_num
//
Line FileBuf::GetLine( const unsigned l_num ) const
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < lines.len(), "l_num < lines.len()" );

  Line* lp  = lines[ l_num ];
  ASSERT( __LINE__, lp, "lines[ %u ]", l_num );

  return *lp;
}

// Return copy of line l_num
//
Line FileBuf::GetStyle( const unsigned l_num ) const
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* lp  = styles[ l_num ];
  ASSERT( __LINE__, lp, "styles[ %u ]", l_num );

  return *lp;
}

// Put a copy of line l_num into l
//
void FileBuf::GetLine( const unsigned l_num, Line& l ) const
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < lines.len(), "l_num < lines.len()" );

  l = *(lines[ l_num ]);
}

Line* FileBuf::GetLineP( const unsigned l_num ) const
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < lines.len(), "l_num < lines.len()" );

  return lines[ l_num ];
}

// Insert a new line on line l_num, which is a copy of line.
// l_num can be lines.len();
//
void FileBuf::InsertLine( const unsigned l_num, const Line& line )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <= lines.len(), "l_num < lines.len()" );

  Line* lp = gl_pVis->BorrowLine( __FILE__,__LINE__, line );
  Line* sp = gl_pVis->BorrowLine( __FILE__,__LINE__, line.len(), 0 );
  ASSERT( __LINE__, lp->len() == sp->len(), "(lp->len()=%u) != (sp->len()=%u)", lp->len(), sp->len() );

  bool ok = lines.insert( l_num, lp ) && styles.insert( l_num, sp );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( l_num );

  if( SavingHist() ) history.Save_InsertLine( l_num );

  hi_touched_line = Min( hi_touched_line, l_num );

  InsertLine_Adjust_Views_topLines( l_num );
}

// Insert pLine on line l_num.  FileBuf will delete pLine.
// l_num can be lines.len();
//
void FileBuf::InsertLine( const unsigned l_num, Line* const pLine )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <= lines.len(), "l_num < lines.len()" );

  Line* sp = gl_pVis->BorrowLine( __FILE__,__LINE__,  pLine->len(), 0 );
  ASSERT( __LINE__, pLine->len() == sp->len(), "(pLine->len()=%u) != (sp->len()=%u)", pLine->len(), sp->len() );

  bool ok = lines.insert( l_num, pLine ) && styles.insert( l_num, sp );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( l_num );

  if( SavingHist() ) history.Save_InsertLine( l_num );

  hi_touched_line = Min( hi_touched_line, l_num );

  InsertLine_Adjust_Views_topLines( l_num );
}

// Insert a new empty line on line l_num.
// l_num can be lines.len();
//
void FileBuf::InsertLine( const unsigned l_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <= lines.len(), "l_num < lines.len()" );

  Line* lp = gl_pVis->BorrowLine( __FILE__,__LINE__ );
  Line* sp = gl_pVis->BorrowLine( __FILE__,__LINE__ );
  ASSERT( __LINE__, lp->len() == sp->len(), "(lp->len()=%u) != (sp->len()=%u)", lp->len(), sp->len() );

  bool ok = lines.insert( l_num, lp ) && styles.insert( l_num, sp );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( l_num );

  if( SavingHist() ) history.Save_InsertLine( l_num );

  hi_touched_line = Min( hi_touched_line, l_num );

  InsertLine_Adjust_Views_topLines( l_num );
}

void FileBuf::InsertLine_Adjust_Views_topLines( const unsigned l_num )
{
  for( unsigned w=0; w<MAX_WINS && w<views.len(); w++ )
  {
    View* const pV = views[w];

    if( l_num < pV->topLine ) pV->topLine++;
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
  ASSERT( __LINE__, l_num < lines.len(), "l_num < lines.len()" );

  Line* lp =  lines[ l_num ];
  Line* sp = styles[ l_num ];

  ASSERT( __LINE__, c_num <= lp->len(), "c_num < lp->len()" );
  ASSERT( __LINE__, c_num <= sp->len(), "c_num < sp->len()" );

  bool ok = lp->insert(__FILE__,__LINE__, c_num, C )
         && sp->insert(__FILE__,__LINE__, c_num, 0 );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( l_num );

  if( SavingHist() ) history.Save_InsertChar( l_num, c_num );

  hi_touched_line = Min( hi_touched_line, l_num );
}

// Add a new line at the end of FileBuf, which is a copy of line
//
void FileBuf::PushLine( const Line& line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = gl_pVis->BorrowLine( __FILE__,__LINE__,  line );
  Line* sp = gl_pVis->BorrowLine( __FILE__,__LINE__,  line.len(), 0 );
  ASSERT( __LINE__, lp->len() == sp->len(), "(lp->len()=%u) != (sp->len()=%u)", lp->len(), sp->len() );

  bool ok = lines.push( lp ) && styles.push( sp );

  ASSERT( __LINE__, ok, "ok" );

  if( SavingHist() ) history.Save_InsertLine( lines.len()-1 );
}

// Add a new pLine to the end of FileBuf.
// FileBuf will delete pLine.
//
void FileBuf::PushLine( Line* const pLine )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* sp = gl_pVis->BorrowLine( __FILE__,__LINE__,  pLine->len(), 0 );
  ASSERT( __LINE__, pLine->len() == sp->len(), "(pLine->len()=%u) != (sp->len()=%u)", pLine->len(), sp->len() );

  bool ok = lines.push( pLine )
        && styles.push( sp );

  ASSERT( __LINE__, ok, "ok" );

  if( SavingHist() ) history.Save_InsertLine( lines.len()-1 );
}

// Add a new empty line at the end of FileBuf
//
void FileBuf::PushLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = gl_pVis->BorrowLine( __FILE__,__LINE__ );
  Line* sp = gl_pVis->BorrowLine( __FILE__,__LINE__ );
  ASSERT( __LINE__, lp->len() == sp->len(), "(lp->len()=%u) != (sp->len()=%u)", lp->len(), sp->len() );

  bool ok = lines.push( lp ) && styles.push( sp );

  ASSERT( __LINE__, ok, "ok" );

  if( SavingHist() ) history.Save_InsertLine( lines.len()-1 );
}

// Add byte C to the end of line l_num
//
void FileBuf::PushChar( const unsigned l_num, const uint8_t C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  lines.len(), "l_num < lines.len()" );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* lp =  lines[ l_num ];
  Line* sp = styles[ l_num ];

  bool ok = lp->push(__FILE__,__LINE__, C )
         && sp->push(__FILE__,__LINE__, 0 );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( l_num );

  if( SavingHist() ) history.Save_InsertChar( l_num, lp->len()-1 );

  hi_touched_line = Min( hi_touched_line, l_num );
}

// Add byte C to last line.  If no lines in file, add a line.
//
void FileBuf::PushChar( const uint8_t C )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !lines.len() ) PushLine();

  const unsigned l_num = lines.len()-1;

  PushChar( l_num, C );
}

// Remove a line from FileBuf, and return a copy.
// Line that was in FileBuf gets deleted.
//
void FileBuf::RemoveLine( const unsigned l_num, Line& line )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  lines.len(), "l_num < lines.len()" );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* lp = 0;
  Line* sp = 0;
  bool ok = lines.remove( l_num, lp ) && styles.remove( l_num, sp );

  ASSERT( __LINE__, ok, "ok" );

  line = *lp;

  if( SavingHist() ) history.Save_RemoveLine( l_num, line );

  hi_touched_line = Min( hi_touched_line, l_num );

  RemoveLine_Adjust_Views_topLines( l_num );

  gl_pVis->ReturnLine( lp );
  gl_pVis->ReturnLine( sp );

  ChangedLine( l_num );
}

// Remove a line from FileBuf without deleting it and return pointer to it.
// Caller of RemoveLine is responsible for deleting line returned.
//
Line* FileBuf::RemoveLineP( const unsigned l_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  lines.len(), "l_num < lines.len()" );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* pLine = 0;
  Line* sp = 0;
  bool ok = lines.remove( l_num, pLine ) && styles.remove( l_num, sp );
  gl_pVis->ReturnLine( sp );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( l_num );

  if( SavingHist() ) history.Save_RemoveLine( l_num, *pLine );

  hi_touched_line = Min( hi_touched_line, l_num );

  RemoveLine_Adjust_Views_topLines( l_num );

  return pLine;
}

// Remove a line from FileBuf, and without returning a copy.
// Line that was in FileBuf gets deleted.
//
void FileBuf::RemoveLine( const unsigned l_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  lines.len(), "l_num < lines.len()" );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* lp = 0;
  Line* sp = 0;
  bool ok = lines.remove( l_num, lp ) && styles.remove( l_num, sp );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( l_num );

  if( SavingHist() ) history.Save_RemoveLine( l_num, *lp );

  hi_touched_line = Min( hi_touched_line, l_num );

  RemoveLine_Adjust_Views_topLines( l_num );

  gl_pVis->ReturnLine( lp );
  gl_pVis->ReturnLine( sp );
}

void FileBuf::RemoveLine_Adjust_Views_topLines( const unsigned l_num )
{
  for( unsigned w=0; w<MAX_WINS && w<views.len(); w++ )
  {
    View* const pV = views[w];

    if( l_num < pV->topLine ) pV->topLine--;

    if( lines.len() <= pV->CrsLine() )
    {
      // Only one line is removed at a time, so just decrementing should work:
      if     ( pV->crsRow  ) pV->crsRow--;
      else if( pV->topLine ) pV->topLine--;
    }
  }
}

// Remove from FileBuf and return the char at line l_num and position c_num
//
uint8_t FileBuf::RemoveChar( const unsigned l_num, const unsigned c_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  lines.len(), "l_num < lines.len()" );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* lp =  lines[ l_num ];
  Line* sp = styles[ l_num ];

  ASSERT( __LINE__, c_num < lp->len(), "c_num=%u < lp->len()=%u", c_num, lp->len() );
  ASSERT( __LINE__, c_num < sp->len(), "c_num < sp->len()" );

  uint8_t C = 0;
  bool ok = lp->remove( c_num, C ) && sp->remove( c_num );

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( l_num );

  if( SavingHist() ) history.Save_RemoveChar( l_num, c_num, C );

  hi_touched_line = Min( hi_touched_line, l_num );

  return C;
}

void FileBuf::PopLine( Line& line )
{
  Trace trace( __PRETTY_FUNCTION__ );
  Line* lp = 0;
  Line* sp = 0;
  bool ok = lines.pop( lp ) && styles.pop( sp );

  ASSERT( __LINE__, ok, "ok" );

  line = *lp;

  gl_pVis->ReturnLine( lp );
  gl_pVis->ReturnLine( sp );

  ChangedLine( lines.len()-1 );

  if( SavingHist() ) history.Save_RemoveLine( lines.len(), line );
  // For now, PopLine is not called in response to user action,
  // so dont need to save update
}

void FileBuf::PopLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( lines.len() )
  {
    Line* lp = 0;
    Line* sp = 0;
    bool ok = lines.pop( lp ) && styles.pop( sp );

    ASSERT( __LINE__, ok, "ok" );

    ChangedLine( lines.len()-1 );

    if( SavingHist() ) history.Save_RemoveLine( lines.len(), *lp );
    // For now, PopLine is not called in response to user action,
    // so dont need to save update

    gl_pVis->ReturnLine( lp );
    gl_pVis->ReturnLine( sp );
  }
}

// Remove and return a char from the end of line l_num
//
uint8_t FileBuf::PopChar( const unsigned l_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  lines.len(), "l_num < lines.len()" );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* lp =  lines[ l_num ];
  Line* sp = styles[ l_num ];

  uint8_t C = 0;
  bool ok = lp->pop( C ) && sp->pop();

  ASSERT( __LINE__, ok, "ok" );

  ChangedLine( l_num );

  if( SavingHist() ) history.Save_RemoveChar( l_num, lp->len(), C );
  // For now, PopChar is not called in response to user action,
  // so dont need to save update

  return C;
}

// Append line to end of line l_num
//
void FileBuf::AppendLineToLine( const unsigned l_num, const Line& line )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  lines.len(), "l_num < lines.len()" );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* lp =  lines[ l_num ];
  Line* sp = styles[ l_num ];

  bool ok = lp->append(__FILE__,__LINE__, line );
  ASSERT( __LINE__, ok, "ok" );

  // Simply need to increase sp's length to match lp's new length:
  for( unsigned k=0; k<line.len(); k++ ) sp->push(__FILE__,__LINE__, 0 );

  ChangedLine( l_num );

  const unsigned first_insert = lp->len() - line.len();

  if( SavingHist() )
  {
    for( unsigned k=0; k<line.len(); k++ )
    {
      history.Save_InsertChar( l_num, first_insert + k );
    }
  }
  hi_touched_line = Min( hi_touched_line, l_num );
}

// Append pLine to end of line l_num, and delete pLine.
//
void FileBuf::AppendLineToLine( const unsigned l_num, const Line* pLine )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num <  lines.len(), "l_num < lines.len()" );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* lp =  lines[ l_num ];
  Line* sp = styles[ l_num ];

  bool ok = lp->append(__FILE__,__LINE__, *pLine );
  ASSERT( __LINE__, ok, "ok" );

  // Simply need to increase sp's length to match lp's new length:
  for( unsigned k=0; k<pLine->len(); k++ ) sp->push(__FILE__,__LINE__, 0 );

  ChangedLine( l_num );

  const unsigned first_insert = lp->len() - pLine->len();

  if( SavingHist() )
  {
    for( unsigned k=0; k<pLine->len(); k++ )
    {
      history.Save_InsertChar( l_num, first_insert + k );
    }
  }
  hi_touched_line = Min( hi_touched_line, l_num );

  gl_pVis->ReturnLine( const_cast<Line*>( pLine ) );
}

void FileBuf::SwapLines( const unsigned l_num_1, const unsigned l_num_2 )
{
  Trace trace( __PRETTY_FUNCTION__ );
  bool ok =  lines.swap( l_num_1, l_num_2 )
         && styles.swap( l_num_1, l_num_2 );

  ASSERT( __LINE__, ok, "ok" );

  if( SavingHist() ) history.Save_SwapLines( l_num_1, l_num_2 );

  ChangedLine( Min( l_num_1, l_num_2 ) );
}

void FileBuf::ClearLines()
{
  Trace trace( __PRETTY_FUNCTION__ );

  while( lines.len() )
  {
    Line* lp = 0;
    Line* sp = 0;
    bool ok = lines.pop( lp ) && styles.pop( sp );

    ASSERT( __LINE__, ok, "ok" );

    gl_pVis->ReturnLine( lp );
    gl_pVis->ReturnLine( sp );
  }
  ChangedLine( 0 );
}

void FileBuf::Undo( View* const pV )
{
  save_history = false;

  history.Undo( pV );

  save_history = true;
}

void FileBuf::UndoAll( View* const pV )
{
  save_history = false;

  history.UndoAll( pV );

  save_history = true;
}

bool FileBuf::Changed() const
{
  return history.Has_Changes();
}

void FileBuf::ClearChanged()
{
  history.Clear();
}

void FileBuf::ChangedLine( const unsigned line_num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // HVLO = Highest valid line offset
  unsigned HVLO = 0;

  if( line_num && lineOffsets.len() )
  {
    HVLO = Min( line_num-1, lineOffsets.len()-1 );
  }
  lineOffsets.set_len(__FILE__,__LINE__, HVLO );
}

unsigned FileBuf::GetSize()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = lines.len();

  unsigned size = 0;

  if( NUM_LINES )
  {
    // Absolute byte offset of beginning of first line in file is always zero:
    if( 0 == lineOffsets.len() ) lineOffsets.push(__FILE__,__LINE__, 0 );

    // Old line offsets length:
    const unsigned OLOL = lineOffsets.len();

    // New line offsets length:
    lineOffsets.set_len(__FILE__,__LINE__, NUM_LINES );

    // Absolute byte offset of beginning of first line in file is always zero:
    lineOffsets[ 0 ] = 0;

    for( unsigned k=OLOL; k<NUM_LINES; k++ )
    {
      lineOffsets[ k ] = lineOffsets[ k-1 ]
                       + lines[ k-1 ]->len()
                       + 1; //< Add 1 for '\n'
    }
    size = lineOffsets[ NUM_LINES-1 ] + lines[ NUM_LINES-1 ]->len();
    if( LF_at_EOF ) size++;
  }
  return size;
}

unsigned FileBuf::GetCursorByte( unsigned CL, unsigned CC )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = lines.len();

  unsigned crsByte = 0;

  if( NUM_LINES )
  {
    if( NUM_LINES <= CL ) CL = NUM_LINES-1;

    const unsigned CLL = lines[CL]->len();

    if( CLL <= CC ) CC = CLL ? CLL-1 : 0;

    // Absolute byte offset of beginning of first line in file is always zero:
    if( 0 == lineOffsets.len() ) lineOffsets.push(__FILE__,__LINE__, 0 );

    // HVLO = Highest valid line offset
    const unsigned HVLO = lineOffsets.len()-1;

    if( HVLO < CL )
    {
      lineOffsets.set_len(__FILE__,__LINE__, CL+1 );

      for( unsigned k=HVLO+1; k<=CL; k++ )
      {
        lineOffsets[ k ] = lineOffsets[ k-1 ]
                         + LineLen( k-1 )
                         + 1;
      }
    }
    crsByte = lineOffsets[ CL ] + CC;
  }
  return crsByte;
}

void FileBuf::Update()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( gl_pKey->get_from_dot_buf ) return;

  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* const pV = views[w];
 
    for( unsigned w2=0; w2<gl_pVis->num_wins; w2++ )
    {
      if( pV == gl_pVis->views[w2][ gl_pVis->file_hist[w2][0] ] )
      {
      //pV->Update();
        Find_Styles( pV->topLine + pV->WorkingRows() );
        ClearStars();
        Find_Stars();

        pV->RepositionView();
        pV->Print_Borders();
        pV->PrintWorkingView();
        pV->PrintStsLine();
        pV->PrintFileLine();
        pV->PrintCmdLine();

        pV->sts_line_needs_update = true;
      }
    }
  }
  Console::Update();
  // Put cursor back into current window
  gl_pVis->CV()->PrintCursor();
}

void FileBuf::ClearStyles()
{
  Trace trace( __PRETTY_FUNCTION__ );

  hi_touched_line = 0;
}

CrsPos FileBuf::Update_Styles_Find_St( const unsigned first_line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Default start position is beginning of file
  CrsPos st = { 0, 0 };

  // 1. Find first position without a style before first line
  bool done = false;
  for( int l=first_line-1; !done && 0<=l; l-- )
  {
    const int LL = LineLen( l );
    for( int p=LL-1; !done && 0<=p; p-- )
    {
      const uint8_t S = styles[l]->get(p);
      if( !S ) {
        st.crsLine = l;
        st.crsChar = p;
        done = true;
      } 
    }
  }
  return st;
}

// Find styles starting at st up to but not including fn line number
void FileBuf::Find_Styles_In_Range( const CrsPos st, const int fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pHi should have already have been allocated, but just in case
  // check and allocate here if needed.
  if( 0 == pHi )
  {
    file_type = FT_TEXT;
    pHi = new(__FILE__,__LINE__) Highlight_Text( *this );
  }
  pHi->Run_Range( st, fn );

  ClearStars_In_Range( st, fn );
  Find_Stars_In_Range( st, fn );
}

// Find stars starting at st up to but not including fn line number
void FileBuf::Find_Stars_In_Range( const CrsPos st, const int fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char*    star_str  = gl_pVis->star.c_str();
  const unsigned STAR_LEN  = gl_pVis->star.len();
  const bool     SLASH     = gl_pVis->slash;
  const unsigned NUM_LINES = NumLines();

  for( unsigned l=st.crsLine; STAR_LEN && l<NUM_LINES && l<fn; l++ )
  {
    Line* lp = lines[l];
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
              for( unsigned m=p; m<p+STAR_LEN; m++ ) SetStarStyle( l, m );
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

// Find styles up to but not including up_to_line number
void FileBuf::Find_Styles( const unsigned up_to_line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();

  if( 0<NUM_LINES )
  {
    hi_touched_line = Min( hi_touched_line, NUM_LINES-1 );

    if( hi_touched_line < up_to_line )
    {
      // Find styles for some EXTRA_LINES beyond where we need to find
      // styles for the moment, so that when the user is scrolling down
      // through an area of a file that has not yet been syntax highlighed,
      // Find_Styles() does not need to be called every time the user
      // scrolls down another line.  Find_Styles() will only be called
      // once for every EXTRA_LINES scrolled down.
      const unsigned EXTRA_LINES = 10;

      CrsPos   st = Update_Styles_Find_St( hi_touched_line );
      unsigned fn = Min( up_to_line+EXTRA_LINES, NUM_LINES );

      Find_Styles_In_Range( st, fn );

      hi_touched_line = fn;
    }
  }
}

void FileBuf::Find_Stars()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( !need_2_find_stars ) return;

  const char*    star_str  = gl_pVis->star.c_str();
  const unsigned STAR_LEN  = gl_pVis->star.len();
  const bool     SLASH     = gl_pVis->slash;
  const unsigned NUM_LINES = NumLines();

  for( unsigned l=0; STAR_LEN && l<NUM_LINES; l++ )
  {
    Line* lp = lines[l];
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
              for( unsigned m=p; m<p+STAR_LEN; m++ ) SetStarStyle( l, m );
              // Increment p one less than STAR_LEN, because p
              // will be incremented again by the for loop
              p += STAR_LEN-1;
            }
          }
        }
      }
    }
  }
  need_2_find_stars = false;
}

void FileBuf::ClearStars()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( need_2_clear_stars )
  {
    const unsigned NUM_LINES = styles.len();

    for( unsigned l=0; l<NUM_LINES; l++ )
    {
      Line* sp = styles[l];
      const unsigned LL = sp->len();

      for( unsigned p=0; p<LL; p++ )
      {
        const uint8_t old_S = sp->get( p );
        sp->set( p, old_S & ~HI_STAR );
      }
    }
    need_2_clear_stars = false;
  }
}

// Clear stars starting at st up to but not including fn line number
void FileBuf::ClearStars_In_Range( const CrsPos st, const int fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = styles.len();

  for( unsigned l=st.crsLine; l<NUM_LINES && l<fn; l++ )
  {
    Line* sp = styles[l];
    const unsigned LL = sp->len();
    const unsigned st_pos = st.crsLine==l ? st.crsChar : 0;

    for( unsigned p=st_pos; p<LL; p++ )
    {
      const uint8_t old_S = sp->get( p );
      sp->set( p, old_S & ~HI_STAR );
    }
  }
}

// Clear all styles includeing star and syntax
void FileBuf::ClearAllStyles( const unsigned l_num
                            , const unsigned c_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* sp = styles[ l_num ];

  ASSERT( __LINE__, c_num < sp->len(), "c_num < sp->len()" );

  sp->set( c_num, 0 );
}

// Leave star style unchanged, and clear syntax styles
void FileBuf::ClearSyntaxStyles( const unsigned l_num
                               , const unsigned c_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* sp = styles[ l_num ];

  ASSERT( __LINE__, c_num < sp->len(), "c_num < sp->len()" );

  // Clear everything except star
  sp->set( c_num, sp->get( c_num ) & HI_STAR );
}

// Leave syntax styles unchanged, and clear star style
void FileBuf::ClearStarStyle( const unsigned l_num
                            , const unsigned c_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* sp = styles[ l_num ];

  ASSERT( __LINE__, c_num < sp->len(), "c_num < sp->len()" );

  // Clear only star
  sp->set( c_num, sp->get( c_num ) & ~HI_STAR );
}

// Leave star style unchanged, and set syntax style
void FileBuf::SetSyntaxStyle( const unsigned l_num
                            , const unsigned c_num
                            , const unsigned style )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* sp = styles[ l_num ];

  ASSERT( __LINE__, c_num < sp->len(), "c_num < sp->len()" );

  uint8_t s = sp->get( c_num );

  s &= HI_STAR; //< Clear everything except star
  s |= style;   //< Set style

  sp->set( c_num, s );
}

// Leave syntax styles unchanged, and set star style
void FileBuf::SetStarStyle( const unsigned l_num
                          , const unsigned c_num )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* sp = styles[ l_num ];

  ASSERT( __LINE__, c_num < sp->len(), "c_num < sp->len()" );

  sp->set( c_num, sp->get( c_num ) | HI_STAR );
}

bool FileBuf::HasStyle( const unsigned l_num, const unsigned c_num, const unsigned style )
{
  Trace trace( __PRETTY_FUNCTION__ );
  ASSERT( __LINE__, l_num < styles.len(), "l_num < styles.len()" );

  Line* sp = styles[ l_num ];

  ASSERT( __LINE__, c_num < sp->len(), "c_num=%u < sp->len()=%u", c_num, sp->len() );

  const uint8_t S = sp->get( c_num );

  return S & style;
}

bool FileBuf::SavingHist() const
{
  return m_mutable && save_history;
}

