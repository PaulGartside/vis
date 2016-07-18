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

#include <string.h>  // strncmp
#include <unistd.h>  // getcwd

#include "String.hh"
#include "Cover_Array.hh"
#include "Key.hh"
#include "MemLog.hh"
#include "Utilities.hh"
#include "View.hh"
#include "Diff.hh"
#include "FileBuf.hh"
#include "Vis.hh"
#include "Console.hh"
#include "Colon.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

struct ColonOp
{
  enum E
  {
    unknown,
    e,
    w
  };
};

struct Colon::Data
{
  Colon&     colon;
  Vis&       vis;
  Key&       key;
  Diff&      diff;
  View*      cv;
  FileBuf*   fb;
  char*      cbuf;
  String&    sbuf;
  String     cover_key;
  Line       cover_buf;
  unsigned   file_index;
  String     partial_path;
  String     search__head;
  ColonOp::E colon_op;

  Data( Colon&  colon
      , Vis&    vis
      , Key&    key
      , Diff&   diff
      , char*   cbuf
      , String& sbuf );
};

Colon::Data::Data( Colon&  colon
                 , Vis&    vis
                 , Key&    key
                 , Diff&   diff
                 , char*   cbuf
                 , String& sbuf )
  : colon( colon )
  , vis( vis )
  , key( key )
  , diff( diff )
  , cv( 0 )
  , fb( 0 )
  , cbuf( cbuf )
  , sbuf( sbuf )
  , cover_key()
  , cover_buf()
  , file_index( 0 )
  , partial_path()
  , search__head()
{
}

//void Colon::Imp::b()
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  if( 0 == m_cbuf[1] ) // :b
//  {
//    m_vis.GoToPrevBuffer();
//  }
//  else {
//    // Switch to a different buffer:
//    if( '#' == m_cbuf[1] ) // :b#
//    {
//      m_vis.GoToPoundBuffer();
//    }
//    else if( 'c' == m_cbuf[1] ) m_vis.GoToCurrBuffer(); // :bc
//    else if( 'e' == m_cbuf[1] ) m_vis.BufferEditor();   // :be
//    else if( 'm' == m_cbuf[1] ) m_vis.BufferMessage();  // :bm
//    else {
//      unsigned buffer_num = atol( m_cbuf+1 ); // :b<number>
//      m_vis.GoToBuffer( buffer_num );
//    }
//  }
//}

//void Colon::Imp::e()
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  m_cv = m_vis.CV();
//  if( 0 == m_cbuf[1] ) // :e
//  {
//    m_fb = m_cv->GetFB();
//    m_fb->ReReadFile();
//
//    for( unsigned w=0; w<m_vis.num_wins; w++ )
//    {
//      if( m_fb == m_vis.views[w][ m_vis.file_hist[w][0] ]->GetFB() )
//      {
//        // View is currently displayed, perform needed update:
//        m_vis.views[w][ m_vis.file_hist[w][0] ]->Update();
//      }
//    }
//  }
//  else // :e file_name
//  {
//    // Edit file of supplied file name:
//    String fname( m_cbuf + 1 );
//    if( m_cv->GoToDir() && FindFullFileName( fname ) )
//    {
//      unsigned file_index = 0;
//      if( m_vis.HaveFile( fname.c_str(), &file_index ) )
//      {
//        m_vis.GoToBuffer( file_index );
//      }
//      else {
//        FileBuf* p_fb = new(__FILE__,__LINE__) FileBuf( m_vis, fname.c_str(), true, FT_UNKNOWN );
//        p_fb->ReadFile();
//        m_vis.GoToBuffer( m_vis.views[m_vis.win].len()-1 );
//      }
//    }
//  }
//}

//void Colon::Imp::w()
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  m_cv = m_vis.CV();
//
//  if( 0 == m_cbuf[1] ) // :w
//  {
//    if( m_cv == m_vis.views[m_vis.win][ CMD_FILE ] )
//    {
//      // Dont allow SHELL_BUFFER to be save with :w.
//      // Require :w filename.
//      m_cv->PrintCursor();
//    }
//    else {
//      m_cv->GetFB()->Write();
//
//      // If Write fails, current view will be message buffer, in which case
//      // we dont want to PrintCursor with the original view, because that
//      // would put the cursor in the wrong position:
//      if( m_vis.CV() == m_cv ) m_cv->PrintCursor();
//    }
//  }
//  else // :w file_name
//  {
//    // Edit file of supplied file name:
//    String fname( m_cbuf + 1 );
//    if( m_cv->GoToDir() && FindFullFileName( fname ) )
//    {
//      unsigned file_index = 0;
//      if( m_vis.HaveFile( fname.c_str(), &file_index ) )
//      {
//        m_vis.GetFileBuf( file_index )->Write();
//      }
//      else if( fname.get_end() != DIR_DELIM )
//      {
//        FileBuf* p_fb = new(__FILE__,__LINE__) FileBuf( m_vis, fname.c_str(), *m_cv->GetFB() );
//        p_fb->Write();
//      }
//    }
//  }
//}

void Reset_File_Name_Completion_Variables( Colon::Data& m )
{
  m.cv          = m.vis.CV();
  m.fb          = 0;
  m.file_index  = 0;
  m.colon_op    = ColonOp::unknown;
  m.partial_path.clear();
  m.search__head.clear();
}

void HandleNormal( Colon::Data& m
                 , const unsigned MSG_LEN
                 , const bool     HIDE
                 , const uint8_t  c
                 ,       char*&   p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned WC    = m.cv->WorkingCols();
  const unsigned G_ROW = m.cv->Cmd__Line_Row(); // Global row

  *p++ = c;
  const unsigned local_COL = Min( p-m.cbuf+MSG_LEN-1, WC-2 );
  const unsigned G_COL = m.cv->Col_Win_2_GL( local_COL );

  Console::Set( G_ROW, G_COL, (HIDE ? '*' : c), S_NORMAL );

  const bool c_written = Console::Update();
  if( !c_written )
  {
    // If c was written, the cursor moves over automatically.
    // If c was not written, the cursor must be moved over manually.
    // Cursor is not written if c entered is same as what is already displayed.
    Console::Move_2_Row_Col( G_ROW, G_COL+1 );
  }
}

// in_out_fname goes in as         some/path/partial_file_name
// and if successful, comes out as some/path
// the relative path to the files
bool FindFileBuf( Colon::Data& m )
{
  const char*    in_fname     = m.sbuf.c_str();
  const unsigned in_fname_len = strlen( in_fname );

  // 1. seperate in_fname into f_name_tail and f_name_head
  String f_name_head;
  String f_name_tail;
  char* nc_in_fname = const_cast<char*>(in_fname);
  char* const last_slash = strrchr( nc_in_fname, DIR_DELIM );
  if( last_slash )
  {
    for( char* cp = last_slash + 1; *cp; cp++ ) f_name_head.push( *cp );
    for( char* cp = nc_in_fname; cp < last_slash; cp++ ) f_name_tail.push( *cp );
  }
  else {
    // No tail, all head:
    for( char* cp = nc_in_fname; *cp; cp++ ) f_name_head.push( *cp );
  }
#ifndef WIN32
  if( f_name_tail == "~" ) f_name_tail = "$HOME";
#endif
  String f_full_path = f_name_tail;
  if( ! f_full_path.len() ) f_full_path.push('.');

  const unsigned FILE_NAME_LEN = 1024;
  char orig_dir[ FILE_NAME_LEN ];
  bool got_orig_dir = !! getcwd( orig_dir, FILE_NAME_LEN );

  if( m.cv->GoToDir() && FindFullFileName( f_full_path ) )
  {
    m.partial_path = f_name_tail;
    m.search__head = f_name_head;
    // f_full_path is now the full path to the directory
    // to search for matches to f_name_head
    unsigned file_index = 0;
    if( m.vis.HaveFile( f_full_path.c_str(), &file_index ) )
    {
      m.fb = m.vis.GetFileBuf( file_index );
    }
    else {
      // This is not a memory leak.
      // m.fb gets added to m.vis.m.files in Vis::Add_FileBuf_2_Lists_Create_Views()
      m.fb = new(__FILE__,__LINE__)
             FileBuf( m.vis, f_full_path.c_str(), true, FT_UNKNOWN );
      m.fb->ReadFile();
    }
    // Restore original directory, for next call to FindFullFileName()
    if( got_orig_dir ) chdir( orig_dir );
    return true;
  }
  return false;
}

// Returns true if found tab filename, else false
bool Find_File_Name_Completion_Variables( Colon::Data& m )
{
  bool found_tab_fname = false;

  m.sbuf = m.cbuf;
  m.sbuf.trim(); // Remove leading and trailing white space
  if( m.sbuf.has_at("e ", 0)
   || m.sbuf=="e" )
  {
    m.colon_op = ColonOp::e;
  }
  else if( m.sbuf.has_at("w ", 0)
        || m.sbuf=="w" )
  {
    m.colon_op = ColonOp::w;
  }

  if( ColonOp::e == m.colon_op
   || ColonOp::w == m.colon_op )
  {
    m.sbuf.shift(1); m.sbuf.trim_beg(); // Remove initial 'e' and space after 'e'

    if( FindFileBuf(m) )
    {
      // Have FileBuf, so add matching files names to tab_fnames
      for( unsigned k=0; !found_tab_fname && k<m.fb->NumLines(); k++ )
      {
        Line l = m.fb->GetLine( k );
        const char* fname = l.c_str( 0 );

        if( fname && 0==strncmp( fname, m.search__head.c_str(), m.search__head.len() ) )
        {
          found_tab_fname = true;
          m.file_index = k;
          m.sbuf = m.partial_path;
          if( 0<m.sbuf.len() )
          {
            m.sbuf.push('/'); // Dont append '/' if no m.partial_path
          }
          // Cant use m.sbuf.append here because Line is not NULL terminated:
          for( unsigned i=0; i<l.len(); i++ ) m.sbuf.push( l.get(i) );
        }
      }
    }
    // Removed "e" above, so add it back here:
    if( ColonOp::e == m.colon_op ) m.sbuf.insert( 0, "e ");
    else                           m.sbuf.insert( 0, "w ");
  }
  return found_tab_fname;
}

// Returns true if found tab filename, else false
bool Have_File_Name_Completion_Variables( Colon::Data& m )
{
  bool found_tab_fname = false;

  // Already have a FileBuf, just search for next matching filename:
  for( unsigned k=m.file_index+1
     ; !found_tab_fname && k<m.file_index+m.fb->NumLines(); k++ )
  {
    Line l = m.fb->GetLine( k % m.fb->NumLines() );
    const char* fname = l.c_str( 0 );

    if( 0==strncmp( fname, m.search__head.c_str(), m.search__head.len() ) )
    {
      found_tab_fname = true;
      m.file_index = k;
      m.sbuf = m.partial_path;
      if( 0<m.sbuf.len() )
      {
        m.sbuf.push('/'); // Done append '/' if no m.partial_path
      }
      // Cant use m.sbuf.append here because Line is not NULL terminated:
      for( unsigned i=0; i<l.len(); i++ ) m.sbuf.push( l.get(i) );
      if( ColonOp::e == m.colon_op ) m.sbuf.insert( 0, "e ");
      else                           m.sbuf.insert( 0, "w ");
    }
  }
  return found_tab_fname;
}

void DisplaySbuf( Colon::Data& m, char*& p )
{
  // Display m.cbuf on command line:
  const unsigned ROW = m.cv->Cmd__Line_Row();
  const unsigned ST  = m.cv->Col_Win_2_GL( 0 );
  const unsigned WC  = m.cv->WorkingCols();
  Console::Set( ROW, ST, ':', S_NORMAL );

  // Put m.sbuf into m.cbuf:
  p = m.cbuf;
  for( unsigned k=0; k<m.sbuf.len(); k++ ) *p++ = m.sbuf.get(k);
  *p = 0;

  const char*    S     = m.cbuf;
        unsigned S_LEN = strlen( S );
  if( WC-3 < S_LEN )
  {
    // Put end of m.sbuf into m.cbuf:
    p = m.cbuf + S_LEN-(WC-3);
    for( unsigned k=S_LEN-(WC-3); k<m.sbuf.len(); k++ ) *p++ = m.sbuf.get(k);
    *p = 0;
    S     = m.cbuf + S_LEN-(WC-3);
    S_LEN = strlen( S );
  }
  for( unsigned k=0; k<S_LEN; k++ )
  {
    Console::Set( ROW, ST+k+1, S[k], S_NORMAL );
  }
  for( unsigned k=S_LEN; k<WC-1; k++ )
  {
    Console::Set( ROW, ST+k+1, ' ', S_NORMAL );
  }
  Console::Update();
  Console::Move_2_Row_Col( ROW, m.cv->Col_Win_2_GL( S_LEN+1 ) );
}

void HandleTab( Colon::Data& m
              , const unsigned MSG_LEN
              ,       char*&   p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  *p = 0;
  bool found_tab_fname = false;

  if( 0 == m.fb )
  {
    found_tab_fname = Find_File_Name_Completion_Variables(m);
  }
  else { // Put list of file names in tab_fnames:
    found_tab_fname = Have_File_Name_Completion_Variables(m);
  }
  if( found_tab_fname )
  {
    DisplaySbuf( m, p );
  }
  else {
    // If we fall through, just treat tab like a space:
    HandleNormal( m, MSG_LEN, false, ' ', p );
  }
}

Colon::Colon( Vis&    vis
            , Key&    key
            , Diff&   diff
            , char*   cbuf
            , String& sbuf )
  : m( *new(__FILE__, __LINE__)
        Colon::Data( *this, vis, key, diff, cbuf, sbuf ) )
{
}

Colon::~Colon()
{
  delete &m;
}

void Colon::GetCommand( const unsigned MSG_LEN, const bool HIDE )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Reset_File_Name_Completion_Variables(m);

  char* p = m.cbuf;

  for( uint8_t c=m.key.In(); !IsEndOfLineDelim( c ); c=m.key.In() )
  {
    if( !HIDE && '\t' == c && m.cbuf < p )
    {
      HandleTab( m, MSG_LEN, p );
    }
    else {                  // Clear
      Reset_File_Name_Completion_Variables(m);

      if( BS != c && DEL != c )
      {
        HandleNormal( m, MSG_LEN, HIDE, c, p );
      }
      else {  // Backspace or Delete key
        if( m.cbuf < p )
        {
          // Replace last typed char with space:
          const unsigned G_ROW = m.cv->Cmd__Line_Row(); // Global row
          const unsigned G_COL = m.cv->Col_Win_2_GL( p-m.cbuf+MSG_LEN-1 );

          Console::Set( G_ROW, G_COL, ' ', S_NORMAL );
          // Display space:
          Console::Update();
          // Move back onto new space:
          Console::Move_2_Row_Col( G_ROW, G_COL );
          p--;
        }
      }
    }
    Console::Flush();
  }
  *p++ = 0;
}

void Colon::hi()
{
  m.cv = m.vis.CV();
  m.cv->GetFB()->ClearStyles();

  if( m.vis.InDiffMode() ) m.diff.Update();
  else                      m.cv->Update();
}

void Colon::MapStart()
{
  m.cv = m.vis.CV();

  m.key.map_buf.clear();
  m.key.save_2_map_buf = true;
  m.cv->DisplayMapping();
}

void Colon::MapEnd()
{
  if( m.key.save_2_map_buf )
  {
    m.key.save_2_map_buf = false;
    // Remove trailing ':' from m.key.map_buf:
    Line& map_buf = m.key.map_buf;
    map_buf.pop(); // '\n'
    map_buf.pop(); // ':'
  }
}

void Colon::MapShow()
{
  Trace trace( __PRETTY_FUNCTION__ );
  m.cv = m.vis.CV();
  const unsigned ROW = m.cv->Cmd__Line_Row();
  const unsigned ST  = m.cv->Col_Win_2_GL( 0 );
  const unsigned WC  = m.cv->WorkingCols();
  const unsigned MAP_LEN = m.key.map_buf.len();

  // Print :
  Console::Set( ROW, ST, ':', S_NORMAL );

  // Print map
  unsigned offset = 1;
  for( unsigned k=0; k<MAP_LEN && offset+k<WC; k++ )
  {
    const char C = m.key.map_buf.get( k );
    if( C == '\n' )
    {
      Console::Set( ROW, ST+offset+k, '<', S_NORMAL ); offset++;
      Console::Set( ROW, ST+offset+k, 'C', S_NORMAL ); offset++;
      Console::Set( ROW, ST+offset+k, 'R', S_NORMAL ); offset++;
      Console::Set( ROW, ST+offset+k, '>', S_NORMAL );
    }
    else if( C == '\E' )
    {
      Console::Set( ROW, ST+offset+k, '<', S_NORMAL ); offset++;
      Console::Set( ROW, ST+offset+k, 'E', S_NORMAL ); offset++;
      Console::Set( ROW, ST+offset+k, 'S', S_NORMAL ); offset++;
      Console::Set( ROW, ST+offset+k, 'C', S_NORMAL ); offset++;
      Console::Set( ROW, ST+offset+k, '>', S_NORMAL );
    }
    else {
      Console::Set( ROW, ST+offset+k, C, S_NORMAL );
    }
  }
  // Print empty space after map
  for( unsigned k=MAP_LEN; offset+k<WC; k++ )
  {
    Console::Set( ROW, ST+offset+k, ' ', S_NORMAL );
  }
  Console::Update();
  if( m.vis.InDiffMode() ) m.diff.PrintCursor( m.cv );
  else                      m.cv->PrintCursor();
}

void Colon::Cover()
{
  m.cv = m.vis.CV();
  m.fb = m.cv->GetFB();

  if( m.fb->IsDir() )
  {
    m.cv->PrintCursor();
  }
  else {
    const uint8_t seed = m.fb->GetSize() % 256;

    Cover_Array( *m.fb, m.cover_buf, seed, m.cover_key );

    // Fill in m.cover_buf from old file data:
    // Clear old file:
    m.fb->ClearChanged();
    m.fb->ClearLines();
    // Read in covered file:
    m.fb->ReadArray( m.cover_buf );

    // Reset view position:
    m.cv->SetTopLine( 0 );
    m.cv->SetLeftChar( 0 );
    m.cv->SetCrsRow( 0 );
    m.cv->SetCrsCol( 0 );

    m.cv->Update();
  }
}

void Colon::CoverKey()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.cv = m.vis.CV();

  const char* msg = "Enter cover key:";
  const unsigned msg_len = strlen( msg );
  m.cv->GoToCmdLineClear( msg );

  GetCommand( msg_len, true );

  m.cover_key = m.cbuf;

  m.cv->PrintCursor();
}

