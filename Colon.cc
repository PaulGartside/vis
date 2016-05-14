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

#include "Cover_Array.hh"
#include "Key.hh"
#include "MemLog.hh"
#include "Utilities.hh"
#include "View.hh"
#include "FileBuf.hh"
#include "Vis.hh"
#include "Console.hh"
#include "Colon.hh"

extern Key* gl_pKey;
extern MemLog<MEM_LOG_BUF_SIZE> Log;

Colon::Colon( Vis& vis )
  : m_vis( vis )
  , m_cv( 0 )
  , m_fb( 0 )
  , m_cbuf( m_vis.m_cbuf )
  , m_sbuf( m_vis.m_sbuf )
  , m_cover_key()
  , m_cover_buf()
  , m_file_index( 0 )
  , m_partial_path()
  , m_search__head()
{
}

void Colon::b()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 == m_cbuf[1] ) // :b
  {
    m_vis.GoToPrevBuffer();
  }
  else {
    // Switch to a different buffer:
    if( '#' == m_cbuf[1] ) // :b#
    {
      if( BE_FILE == m_vis.file_hist[m_vis.win][1] )
      {
        m_vis.GoToBuffer( m_vis.file_hist[m_vis.win][2] );
      }
      else m_vis.GoToBuffer( m_vis.file_hist[m_vis.win][1] );
    }
    else if( 'c' == m_cbuf[1] ) m_vis.GoToCurrBuffer(); // :bc
    else if( 'e' == m_cbuf[1] ) m_vis.BufferEditor();   // :be
    else if( 'm' == m_cbuf[1] ) m_vis.BufferMessage();  // :bm
    else {
      unsigned buffer_num = atol( m_cbuf+1 ); // :b<number>
      m_vis.GoToBuffer( buffer_num );
    }
  }
}

void Colon::e()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_cv = m_vis.CV();
  if( 0 == m_cbuf[1] ) // :e
  {
    m_fb = m_cv->pfb;
    m_fb->ReReadFile();

    for( unsigned w=0; w<m_vis.num_wins; w++ )
    {
      if( m_fb == m_vis.views[w][ m_vis.file_hist[w][0] ]->pfb )
      {
        // View is currently displayed, perform needed update:
        m_vis.views[w][ m_vis.file_hist[w][0] ]->Update();
      }
    }
  }
  else // :e file_name
  {
    // Edit file of supplied file name:
    String fname( m_cbuf + 1 );
    if( m_cv->GoToDir() && FindFullFileName( fname ) )
    {
      unsigned file_index = 0;
      if( m_vis.HaveFile( fname.c_str(), &file_index ) )
      {
        m_vis.GoToBuffer( file_index );
      }
      else {
        FileBuf* p_fb = new(__FILE__,__LINE__) FileBuf( fname.c_str(), true, FT_UNKNOWN );
        p_fb->ReadFile();
        m_vis.GoToBuffer( m_vis.views[m_vis.win].len()-1 );
      }
    }
  }
}

void Colon::w()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_cv = m_vis.CV();

  if( 0 == m_cbuf[1] ) // :w
  {
    if( m_cv == m_vis.views[m_vis.win][ CMD_FILE ] )
    {
      // Dont allow SHELL_BUFFER to be save with :w.
      // Require :w filename.
      m_cv->PrintCursor();
    }
    else {
      m_cv->pfb->Write();

      // If Write fails, current view will be message buffer, in which case
      // we dont want to PrintCursor with the original view, because that
      // would put the cursor in the wrong position:
      if( m_vis.CV() == m_cv ) m_cv->PrintCursor();
    }
  }
  else // :w file_name
  {
    // Edit file of supplied file name:
    String fname( m_cbuf + 1 );
    if( m_cv->GoToDir() && FindFullFileName( fname ) )
    {
      unsigned file_index = 0;
      if( m_vis.HaveFile( fname.c_str(), &file_index ) )
      {
        m_vis.files[ file_index ]->Write();
      //m_vis.GoToBuffer( file_index );
      }
      else if( fname.get_end() != DIR_DELIM )
      {
        FileBuf* p_fb = new(__FILE__,__LINE__) FileBuf( fname.c_str(), *m_cv->pfb );
        p_fb->Write();
      }
    }
  }
}

void Colon::hi()
{
  m_cv = m_vis.CV();
  m_cv->pfb->ClearStyles();

  if( m_vis.m_diff_mode ) m_vis.diff.Update();
  else                         m_cv->Update();
}

void Colon::MapStart()
{
  m_cv = m_vis.CV();

  gl_pKey->map_buf.clear();
  gl_pKey->save_2_map_buf = true;
  m_cv->DisplayMapping();
}

void Colon::MapEnd()
{
  if( gl_pKey->save_2_map_buf )
  {
    gl_pKey->save_2_map_buf = false;
    // Remove trailing ':' from gl_pKey->map_buf:
    Line& map_buf = gl_pKey->map_buf;
    map_buf.pop(); // '\n'
    map_buf.pop(); // ':'
  }
}

void Colon::MapShow()
{
  Trace trace( __PRETTY_FUNCTION__ );
  m_cv = m_vis.CV();
  const unsigned ROW = m_cv->Cmd__Line_Row();
  const unsigned ST  = m_cv->Col_Win_2_GL( 0 );
  const unsigned WC  = m_cv->WorkingCols();
  const unsigned MAP_LEN = gl_pKey->map_buf.len();

  // Print :
  Console::Set( ROW, ST, ':', S_NORMAL );

  // Print map
  unsigned offset = 1;
  for( unsigned k=0; k<MAP_LEN && offset+k<WC; k++ )
  {
    const char C = gl_pKey->map_buf.get( k );
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
  if( m_vis.m_diff_mode ) m_vis.diff.PrintCursor( m_cv );
  else                         m_cv->PrintCursor();
}

void Colon::Cover()
{
  m_cv = m_vis.CV();
  m_fb = m_cv->pfb;

  if( m_fb->is_dir )
  {
    m_cv->PrintCursor();
  }
  else {
    const uint8_t seed = m_fb->GetSize() % 256;

    Cover_Array( *m_fb, m_cover_buf, seed, m_cover_key );

    // Fill in m_cover_buf from old file data:
    // Clear old file:
    m_fb->ClearChanged();
    m_fb->ClearLines();
    // Read in covered file:
    m_fb->ReadArray( m_cover_buf );

    // Reset view position:
    m_cv->topLine  = 0;
    m_cv->leftChar = 0;
    m_cv->crsRow = 0;
    m_cv->crsCol = 0;

    m_cv->Update();
  }
}

void Colon::CoverKey()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_cv = m_vis.CV();

  const char* msg = "Enter cover key:";
  const unsigned msg_len = strlen( msg );
  m_cv->GoToCmdLineClear( msg );

  GetCommand( msg_len, true );

  m_cover_key = m_cbuf;

  m_cv->PrintCursor();
}

void Colon::Reset_File_Name_Completion_Variables()
{
  m_cv          = m_vis.CV();
  m_fb          = 0;
  m_file_index  = 0;
  m_colon_op    = OP_unknown;
  m_partial_path.clear();
  m_search__head.clear();
}

void Colon::GetCommand( const unsigned MSG_LEN, const bool HIDE )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Reset_File_Name_Completion_Variables();

  char* p = m_cbuf;

  for( uint8_t c=gl_pKey->In(); !IsEndOfLineDelim( c ); c=gl_pKey->In() )
  {
    if( !HIDE && '\t' == c && m_cbuf < p )
    {
      HandleTab( MSG_LEN, p );
    }
    else {                  // Clear
      Reset_File_Name_Completion_Variables();

      if( BS != c && DEL != c )
      {
        HandleNormal( MSG_LEN, HIDE, c, p );
      }
      else {  // Backspace or Delete key
        if( m_cbuf < p )
        {
          // Replace last typed char with space:
          const unsigned G_ROW = m_cv->Cmd__Line_Row(); // Global row
          const unsigned G_COL = m_cv->Col_Win_2_GL( p-m_cbuf+MSG_LEN-1 );

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

void Colon::HandleNormal( const unsigned MSG_LEN
                        , const bool     HIDE
                        , const uint8_t  c
                        ,       char*&   p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned WC    = m_cv->WorkingCols();
  const unsigned G_ROW = m_cv->Cmd__Line_Row(); // Global row

  *p++ = c;
  const unsigned local_COL = Min( p-m_cbuf+MSG_LEN-1, WC-2 );
  const unsigned G_COL = m_cv->Col_Win_2_GL( local_COL );

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

void Colon::HandleTab( const unsigned  MSG_LEN
                     ,       char*&    p )
{
  Trace trace( __PRETTY_FUNCTION__ );
  *p = 0;
  bool found_tab_fname = false;

  if( 0 == m_fb )
  {
    found_tab_fname = Find_File_Name_Completion_Variables();
  }
  else { // Put list of file names in tab_fnames:
    found_tab_fname = Have_File_Name_Completion_Variables();
  }
  if( found_tab_fname )
  {
    DisplaySbuf( p );
  }
  else {
    // If we fall through, just treat tab like a space:
    HandleNormal( MSG_LEN, false, ' ', p );
  }
}

// Returns true if found tab filename, else false
bool Colon::Find_File_Name_Completion_Variables()
{
  bool found_tab_fname = false;

  m_sbuf = m_cbuf;
  m_sbuf.trim(); // Remove leading and trailing white space
  if( m_sbuf.has_at("e ", 0)
   || m_sbuf=="e" )
  {
    m_colon_op = OP_e;
  }
  else if( m_sbuf.has_at("w ", 0)
        || m_sbuf=="w" )
  {
    m_colon_op = OP_w;
  }

  if( OP_e == m_colon_op
   || OP_w == m_colon_op )
  {
    m_sbuf.shift(1); m_sbuf.trim_beg(); // Remove initial 'e' and space after 'e'

    if( FindFileBuf() )
    {
      // Have FileBuf, so add matching files names to tab_fnames
      for( unsigned k=0; !found_tab_fname && k<m_fb->NumLines(); k++ )
      {
        Line l = m_fb->GetLine( k );
        const char* fname = l.c_str( 0 );

        if( fname && 0==strncmp( fname, m_search__head.c_str(), m_search__head.len() ) )
        {
          found_tab_fname = true;
          m_file_index = k;
          m_sbuf = m_partial_path;
          if( 0<m_sbuf.len() )
          {
            m_sbuf.push('/'); // Dont append '/' if no m_partial_path
          }
          // Cant use m_sbuf.append here because Line is not NULL terminated:
          for( unsigned i=0; i<l.len(); i++ ) m_sbuf.push( l.get(i) );
        }
      }
    }
    // Removed "e" above, so add it back here:
    if( OP_e == m_colon_op ) m_sbuf.insert( 0, "e ");
    else                     m_sbuf.insert( 0, "w ");
  }
  return found_tab_fname;
}

// Returns true if found tab filename, else false
bool Colon::Have_File_Name_Completion_Variables()
{
  bool found_tab_fname = false;

  // Already have a FileBuf, just search for next matching filename:
  for( unsigned k=m_file_index+1
     ; !found_tab_fname && k<m_file_index+m_fb->NumLines(); k++ )
  {
    Line l = m_fb->GetLine( k % m_fb->NumLines() );
    const char* fname = l.c_str( 0 );

    if( 0==strncmp( fname, m_search__head.c_str(), m_search__head.len() ) )
    {
      found_tab_fname = true;
      m_file_index = k;
      m_sbuf = m_partial_path;
      if( 0<m_sbuf.len() )
      {
        m_sbuf.push('/'); // Done append '/' if no m_partial_path
      }
      // Cant use m_sbuf.append here because Line is not NULL terminated:
      for( unsigned i=0; i<l.len(); i++ ) m_sbuf.push( l.get(i) );
      if( OP_e == m_colon_op ) m_sbuf.insert( 0, "e ");
      else                     m_sbuf.insert( 0, "w ");
    }
  }
  return found_tab_fname;
}

// in_out_fname goes in as         some/path/partial_file_name
// and if successful, comes out as some/path
// the relative path to the files
bool Colon::FindFileBuf()
{
  const char*    in_fname     = m_sbuf.c_str();
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

  if( m_cv->GoToDir() && FindFullFileName( f_full_path ) )
  {
    m_partial_path = f_name_tail;
    m_search__head = f_name_head;
    // f_full_path is now the full path to the directory
    // to search for matches to f_name_head
    unsigned file_index = 0;
    if( m_vis.HaveFile( f_full_path.c_str(), &file_index ) )
    {
      m_fb = m_vis.files[ file_index ];
    }
    else {
      m_fb = new(__FILE__,__LINE__) FileBuf( f_full_path.c_str(), true, FT_UNKNOWN );
      m_fb->ReadFile();
    }
    // Restore original directory, for next call to FindFullFileName()
    if( got_orig_dir ) chdir( orig_dir );
    return true;
  }
  return false;
}

void Colon::DisplaySbuf( char*& p )
{
  // Display m_cbuf on command line:
  const unsigned ROW = m_cv->Cmd__Line_Row();
  const unsigned ST  = m_cv->Col_Win_2_GL( 0 );
  const unsigned WC  = m_cv->WorkingCols();
  Console::Set( ROW, ST, ':', S_NORMAL );

  // Put m_sbuf into m_cbuf:
  p = m_cbuf;
  for( unsigned k=0; k<m_sbuf.len(); k++ ) *p++ = m_sbuf.get(k);
  *p = 0;

  const char*    S     = m_cbuf;
        unsigned S_LEN = strlen( S );
  if( WC-3 < S_LEN )
  {
    // Put end of m_sbuf into m_cbuf:
    p = m_cbuf + S_LEN-(WC-3);
    for( unsigned k=S_LEN-(WC-3); k<m_sbuf.len(); k++ ) *p++ = m_sbuf.get(k);
    *p = 0;
    S     = m_cbuf + S_LEN-(WC-3);
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
  Console::Move_2_Row_Col( ROW, m_cv->Col_Win_2_GL( S_LEN+1 ) );
}

