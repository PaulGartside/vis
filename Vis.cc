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
#include <stdio.h>     // printf, stderr, FILE, fopen, fclose
#include <stdarg.h>    // va_list, va_start, va_end
#include <unistd.h>    // write, ioctl[unix], read, getcwd
#include <string.h>    // memcpy, memset
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>  // lstat
#include <sys/time.h>  // gettimeofday

#include "Array_t.hh"
#include "gArray_t.hh"
#include "String.hh"
#include "MemCheck.hh"
#include "MemLog.hh"
#include "Help.hh"
#include "FileBuf.hh"
#include "Utilities.hh"
#include "Console.hh"
#include "Colon.hh"
#include "ChangeHist.hh"
#include "Diff.hh"
#include "View.hh"
#include "Key.hh"
#include "Shell.hh"
#include "Vis.hh"

const char* PROG_NAME;

const int FD_IO = 0; // read/write file descriptor

extern MemLog<MEM_LOG_BUF_SIZE> Log;
extern const char* DIR_DELIM_STR;

       const uint16_t MAX_COLS   = 1024; // Arbitrary maximum char width of window
extern const unsigned BE_FILE    = 0;    // Buffer editor file
extern const unsigned HELP_FILE  = 1;    // Help          file
extern const unsigned SE_FILE    = 2;    // Search editor file
extern const unsigned MSG_FILE   = 3;    // Message       file
extern const unsigned SHELL_FILE = 4;    // Command Shell file

const char* EDIT_BUF_NAME = "BUFFER_EDITOR";
const char* HELP_BUF_NAME = "VIS_HELP";
const char* SRCH_BUF_NAME = "SEARCH_EDITOR";
const char* MSG__BUF_NAME = "MESSAGE_BUFFER";
const char* SHEL_BUF_NAME = "SHELL_BUFFER";

void WARN( const char* msg )
{
  printf("%s: %s\n", PROG_NAME, msg );
}

void Usage()
{
  char msg[128];
  snprintf( msg, 128, "usage: %s [file1 [file2 ...]]\n", PROG_NAME );
  printf("%s", msg );
  exit( 0 );
}

struct Vis::Data
{
  Vis&       vis;
  bool       running;
  Key        key;
  Diff       diff;
  Colon      colon;
  Shell      shell;
  char       cbuf[MAX_COLS];    // General purpose char buffer
  String     sbuf;              // General purpose string buffer
  unsigned   win;               // Sub-window index
  unsigned   num_wins;          // Number of sub-windows currently on screen
  FileList   files;             // list of file buffers
  ViewList   views[MAX_WINS];   // Array of lists of file views
  unsList    file_hist[MAX_WINS]; // Array of lists of view history. [win][m_view_num]
  LinesList  reg;               // Register
  LinesList  line_cache;
  ChangeList change_cache;
  Paste_Mode paste_mode;
  bool       diff_mode; // true if displaying diff
  String     star;      // current text highlighted by '*' command
  bool       slash;     // indicated whether star pattern is slash type or * type
  int        fast_char; // Char on line to goto when ';' is entered

  typedef void (*CmdFunc) ( Data& m );
  CmdFunc CmdFuncs[128];

  Data( Vis& vis );
  ~Data();
};

Vis::Data::Data( Vis& vis )
  : vis( vis )
  , running( true )
  , key()
  , diff( vis, key, reg )
  , colon( vis, key, diff, cbuf, sbuf )
  , shell( vis )
  , win( 0 )
  , num_wins( 1 )
  , files(__FILE__, __LINE__)
  , views()
  , file_hist()
  , reg()
  , line_cache()
  , change_cache()
  , paste_mode( PM_LINE )
  , diff_mode( false )
  , star()
  , slash( false )
  , fast_char( -1 )
{
}

Vis::Data::~Data()
{
}

void Handle_Cmd( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const char CC = m.key.In();

  Vis::Data::CmdFunc cf = m.CmdFuncs[ CC ];
  if( cf ) (*cf)(m);
}

View* CV( Vis::Data& m )
{
  return m.views[m.win][ m.file_hist[m.win][0] ];
}

View* PV( Vis::Data& m )
{
  return m.views[m.win][ m.file_hist[m.win][1] ];
}

void GetCWD( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned FILE_NAME_LEN = 1024;
  char cwd[ FILE_NAME_LEN ];

  if( ! getcwd( cwd, FILE_NAME_LEN ) )
  {
    m.vis.CmdLineMessage( "getcwd() failed" );
  }
  else {
    m.vis.CmdLineMessage( "%s", cwd );
  }
  CV(m)->PrintCursor();
}

void Ch_Dir( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char* path = 0;

  // 1. First get path to chdir to:
  if( 0 != m.cbuf[2] ) // :cd relative_path
  {
    path = m.cbuf + 2;
  }
  else // :cd - chdir to location of current file
  {
    const char* fname = CV(m)->GetFB()->GetFileName();
    const char* last_slash = strrchr( fname, DIR_DELIM );
    if( 0==last_slash )
    {
      path = fname;
    }
    else {
      // Put everything in fname up to last slash into m.sbuf:
      m.sbuf.clear(); // m.sbuf is a general purpose string buffer
      for( const char* cp = fname; cp < last_slash; cp++ ) m.sbuf.push( *cp );
      path = m.sbuf.c_str();
    }
  }

  // 2. chdir
  int err = chdir( path );
  if( err )
  {
    m.vis.CmdLineMessage( "chdir(%s) failed", path );
    CV(m)->PrintCursor();
  }
  else {
    GetCWD(m);
  }
}

void Set_Syntax( Vis::Data& m )
{
  const char* syn = strchr( m.cbuf, '=' );

  if( NULL != syn )
  {
    // Move past '='
    syn++;
    if( 0 != *syn )
    {
      // Something after the '='
      CV(m)->GetFB()->Set_File_Type( syn );
    }
  }
}

void AdjustViews( Vis::Data& m )
{
  for( unsigned w=0; w<m.num_wins; w++ )
  {
    m.views[w][ m.file_hist[w][0] ]->SetViewPos();
  }
}

void InitBufferEditor( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m.files in Add_FileBuf_2_Lists_Create_Views()
  // Buffer editor, 0
  FileBuf* pfb = new(__FILE__,__LINE__)
                 FileBuf( m.vis, EDIT_BUF_NAME, false, FT_BUFFER_EDITOR );
}

void InitHelpBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m.files in Add_FileBuf_2_Lists_Create_Views()
  // Help buffer, 1
  FileBuf* pfb = new(__FILE__,__LINE__)
                 FileBuf( m.vis, HELP_BUF_NAME, false, FT_TEXT );
  pfb->ReadString( HELP_STR );
}

void InitSearchBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m.files in Add_FileBuf_2_Lists_Create_Views()
  // Search editor buffer, 2
  FileBuf* pfb = new(__FILE__,__LINE__)
                 FileBuf( m.vis, SRCH_BUF_NAME, false, FT_TEXT );
}

void InitMsgBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m.files in Add_FileBuf_2_Lists_Create_Views()
  // Message buffer, 3
  FileBuf* pfb = new(__FILE__,__LINE__)
                 FileBuf( m.vis, MSG__BUF_NAME, false, FT_TEXT );
}

void InitShellBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m.files in Add_FileBuf_2_Lists_Create_Views()
  // Command buffer, SHELL_FILE(4)
  FileBuf* pfb = new(__FILE__,__LINE__)
                 FileBuf( m.vis, SHEL_BUF_NAME, false, FT_TEXT );

  // Add ######################################
  pfb->PushLine();
  for( unsigned k=0; k<40; k++ ) pfb->PushChar( '#' );

  // Add an empty line
  pfb->PushLine();

  // Put cursor on empty line below # line
  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* pV_shell = m.views[w][ SHELL_FILE ];
    pV_shell->SetCrsRow( 1 );
  }
}

bool InitUserFiles( Vis::Data& m, const int ARGC, const char* const ARGV[] )
{
  bool run_diff = false;

  // User file buffers, 5, 6, ...
  for( int k=1; k<ARGC; k++ )
  {
    if( strcmp( "-d", ARGV[k] ) == 0 )
    {
      run_diff = true;
    }
    else {
      String file_name = ARGV[k];
      if( FindFullFileNameRel2CWD( file_name ) )
      {
        if( !m.vis.HaveFile( file_name.c_str() ) )
        {
          FileBuf* pfb = new(__FILE__,__LINE__)
                         FileBuf( m.vis, file_name.c_str(), true, FT_UNKNOWN );
          pfb->ReadFile();
        }
      }
    }
  }
  return run_diff;
}

void InitFileHistory( Vis::Data& m )
{
  for( int w=0; w<MAX_WINS; w++ )
  {
    m.file_hist[w].push( __FILE__,__LINE__, BE_FILE );
    m.file_hist[w].push( __FILE__,__LINE__, HELP_FILE );

    if( 5<m.views[w].len() )
    {
      m.file_hist[w].insert( __FILE__,__LINE__, 0, 5 );

      for( int f=m.views[w].len()-1; 6<=f; f-- )
      {
        m.file_hist[w].push( __FILE__,__LINE__, f );
      }
    }
  }
}

void GoToFile( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Get fname underneath the cursor:
  String fname;
  bool ok = CV(m)->GoToFile_GetFileName( fname );

  if( ok ) m.vis.GoToBuffer_Fname( fname );
}

void GoToBuffer( Vis::Data& m, const unsigned buf_idx )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.views[m.win].len() <= buf_idx )
  {
    m.vis.CmdLineMessage( "Buffer %lu does not exist", buf_idx );
  }
  else {
    if( buf_idx == m.file_hist[m.win][0] )
    {
      // User asked for view that is currently displayed.
      // Dont do anything, just put cursor back in place.
      CV(m)->PrintCursor();
    }
    else {
      m.vis.NoDiff();

      m.file_hist[m.win].insert(__FILE__,__LINE__, 0, buf_idx );

      // Remove subsequent buf_idx's from m.file_hist[m.win]:
      for( unsigned k=1; k<m.file_hist[m.win].len(); k++ )
      {
        if( buf_idx == m.file_hist[m.win][k] ) m.file_hist[m.win].remove( k );
      }
      View* nv = CV(m); // New View to display
      if( ! nv->Has_Context() )
      {
        // Look for context for the new view:
        bool found_context = false;
        for( unsigned w=0; !found_context && w<m.num_wins; w++ )
        {
          View* v = m.views[w][ buf_idx ];
          if( v->Has_Context() )
          {
            found_context = true;

            nv->Set_Context( *v );
          }
        }
      }
      nv->SetTilePos( PV(m)->GetTilePos() );
      nv->Update();
    }
  }
}

// m.vis.m.file_hist[m.vis.m.win]:
//-------------------------
//| 5 | 4 | 3 | 2 | 1 | 0 |
//-------------------------
//:b -> GoToPrevBuffer()
//-------------------------
//| 4 | 3 | 2 | 1 | 0 | 5 |
//-------------------------
//:b -> GoToPrevBuffer()
//-------------------------
//| 3 | 2 | 1 | 0 | 5 | 4 |
//-------------------------
//:n -> GoToNextBuffer()
//-------------------------
//| 4 | 3 | 2 | 1 | 0 | 5 |
//-------------------------
//:n -> GoToNextBuffer()
//-------------------------
//| 5 | 4 | 3 | 2 | 1 | 0 |
//-------------------------

// After starting up:
//-------------------------------
//| f1 | be | bh | f4 | f3 | f2 |
//-------------------------------

void GoToNextBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned FILE_HIST_LEN = m.file_hist[m.win].len();

  if( FILE_HIST_LEN <= 1 )
  {
    // Nothing to do, so just put cursor back
    CV(m)->PrintCursor();
  }
  else {
    m.vis.NoDiff();

    View*    const pV_old = CV(m);
    Tile_Pos const tp_old = pV_old->GetTilePos();

    // Move view index at back to front of m.file_hist
    unsigned view_index_new = 0;
    m.file_hist[m.win].pop( view_index_new );
    m.file_hist[m.win].insert(__FILE__,__LINE__, 0, view_index_new );

    // Redisplay current window with new view:
    CV(m)->SetTilePos( tp_old );
    CV(m)->Update();
  }
}

void GoToCurrBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // CVI = Current View Index
  const unsigned CVI = m.file_hist[m.win][0];

  if( CVI == BE_FILE
   || CVI == HELP_FILE
   || CVI == SE_FILE )
  {
    m.vis.NoDiff();

    GoToBuffer( m, m.file_hist[m.win][1] );
  }
  else {
    // User asked for view that is currently displayed.
    // Dont do anything, just put cursor back in place.
    CV(m)->PrintCursor();
  }
}

void GoToPrevBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned FILE_HIST_LEN = m.file_hist[m.win].len();

  if( FILE_HIST_LEN <= 1 )
  {
    // Nothing to do, so just put cursor back
    CV(m)->PrintCursor();
  }
  else {
    m.vis.NoDiff();

    View*    const pV_old = CV(m);
    Tile_Pos const tp_old = pV_old->GetTilePos();

    // Move view index at front to back of m.file_hist
    unsigned view_index_old = 0;
    m.file_hist[m.win].remove( 0, view_index_old );
    m.file_hist[m.win].push(__FILE__,__LINE__, view_index_old );

    // Redisplay current window with new view:
    CV(m)->SetTilePos( tp_old );
    CV(m)->Update();
  }
}

void GoToPoundBuffer( Vis::Data& m )
{
  m.vis.NoDiff();

  if( BE_FILE == m.file_hist[m.win][1] )
  {
    GoToBuffer( m, m.file_hist[m.win][2] );
  }
  else GoToBuffer( m, m.file_hist[m.win][1] );
}

void GoToBufferEditor( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToBuffer( m, BE_FILE );
}

void GoToMsgBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToBuffer( m, MSG_FILE );
}

void GoToShellBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToBuffer( m, SHELL_FILE );
}

void GoToSearchBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToBuffer( m, SE_FILE );
}

void GoToNextWindow( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < m.num_wins )
  {
    const unsigned win_old = m.win;

    m.win = (++m.win) % m.num_wins;

    View* pV     = m.views[m.win  ][ m.file_hist[m.win  ][0] ];
    View* pV_old = m.views[win_old][ m.file_hist[win_old][0] ];

    pV_old->Print_Borders();
    pV    ->Print_Borders();

    Console::Update();

    m.diff_mode ? m.diff.PrintCursor( pV ) : pV->PrintCursor();
  }
}

bool GoToNextWindow_l_Find( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found = false; // Found next view to go to

  const View*    curr_V  = m.views[m.win][ m.file_hist[m.win][0] ];
  const Tile_Pos curr_TP = curr_V->GetTilePos();

  if( curr_TP == TP_LEFT_HALF )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF         == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_HALF )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF     == TP
       || TP_TOP__LEFT_QTR == TP
       || TP_BOT__LEFT_QTR == TP
       || TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP
       || TP_BOT__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF         == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_QTR == TP
       || TP_TOP__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF         == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_BOT__LEFT_QTR == TP
       || TP_BOT__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP
       || TP_BOT__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_LEFT_CTR__QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF         == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_CTR__QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP
       || TP_BOT__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_CTR__QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF         == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF         == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_QTR      == TP
       || TP_BOT__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_QTR == TP
       || TP_TOP__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_BOT__LEFT_QTR == TP
       || TP_BOT__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  return found;
}

void GoToNextWindow_l( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < m.num_wins )
  {
    const unsigned win_old = m.win;

    // If next view to go to was not found, dont do anything, just return
    // If next view to go to is found, m.win will be updated to new value
    if( GoToNextWindow_l_Find(m) )
    {
      View* pV     = m.views[m.win  ][ m.file_hist[m.win  ][0] ];
      View* pV_old = m.views[win_old][ m.file_hist[win_old][0] ];

      pV_old->Print_Borders();
      pV    ->Print_Borders();

      Console::Update();

      m.diff_mode ? m.diff.PrintCursor( pV ) : pV->PrintCursor();
    }
  }
}

bool GoToNextWindow_h_Find( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found = false; // Found next view to go to

  const View*    curr_V  = m.views[m.win][ m.file_hist[m.win][0] ];
  const Tile_Pos curr_TP = curr_V->GetTilePos();

  if( curr_TP == TP_LEFT_HALF )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF     == TP
       || TP_TOP__RITE_QTR == TP
       || TP_BOT__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP
       || TP_BOT__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_HALF )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF     == TP
       || TP_TOP__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF         == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF     == TP
       || TP_BOT__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_BOT__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF         == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF     == TP
       || TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP
       || TP_BOT__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_LEFT_CTR__QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP
       || TP_BOT__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_CTR__QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF         == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF     == TP
       || TP_TOP__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF     == TP
       || TP_BOT__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_BOT__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_QTR      == TP
       || TP_BOT__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF         == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_CTR__QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_CTR__QTR     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  return found;
}

void GoToNextWindow_h( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < m.num_wins )
  {
    const unsigned win_old = m.win;

    // If next view to go to was not found, dont do anything, just return
    // If next view to go to is found, m.win will be updated to new value
    if( GoToNextWindow_h_Find(m) )
    {
      View* pV     = m.views[m.win  ][ m.file_hist[m.win  ][0] ];
      View* pV_old = m.views[win_old][ m.file_hist[win_old][0] ];

      pV_old->Print_Borders();
      pV    ->Print_Borders();

      Console::Update();

      m.diff_mode ? m.diff.PrintCursor( pV ) : pV->PrintCursor();
    }
  }
}

bool GoToNextWindow_jk_Find( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found = false; // Found next view to go to

  const View*    curr_V  = m.views[m.win][ m.file_hist[m.win][0] ];
  const Tile_Pos curr_TP = curr_V->GetTilePos();

  if( curr_TP == TP_TOP__HALF )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF         == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_BOT__LEFT_8TH     == TP
       || TP_BOT__RITE_8TH     == TP
       || TP_BOT__LEFT_CTR_8TH == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__HALF )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_TOP__LEFT_8TH     == TP
       || TP_TOP__RITE_8TH     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF         == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_BOT__LEFT_8TH     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF         == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_BOT__RITE_8TH     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_TOP__LEFT_8TH     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF         == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_TOP__RITE_8TH     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF     == TP
       || TP_BOT__LEFT_QTR == TP
       || TP_BOT__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF     == TP
       || TP_BOT__RITE_QTR == TP
       || TP_BOT__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF         == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF         == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF     == TP
       || TP_TOP__LEFT_QTR == TP
       || TP_TOP__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF     == TP
       || TP_TOP__RITE_QTR == TP
       || TP_TOP__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = (m.views[k][ m.file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF         == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  return found;
}

void GoToNextWindow_jk( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < m.num_wins )
  {
    const unsigned win_old = m.win;

    // If next view to go to was not found, dont do anything, just return
    // If next view to go to is found, m.win will be updated to new value
    if( GoToNextWindow_jk_Find(m) )
    {
      View* pV     = m.views[m.win  ][ m.file_hist[m.win  ][0] ];
      View* pV_old = m.views[win_old][ m.file_hist[win_old][0] ];

      pV_old->Print_Borders();
      pV    ->Print_Borders();

      Console::Update();

      m.diff_mode ? m.diff.PrintCursor( pV ) : pV->PrintCursor();
    }
  }
}

void FlipWindows( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 1 < m.num_wins )
  {
    // This code only works for MAX_WINS == 2
    View* pV1 = m.views[0][ m.file_hist[0][0] ];
    View* pV2 = m.views[1][ m.file_hist[1][0] ];

    if( pV1 != pV2 )
    {
      // Swap pV1 and pV2 Tile Positions:
      Tile_Pos tp_v1 = pV1->GetTilePos();
      pV1->SetTilePos( pV2->GetTilePos() );
      pV2->SetTilePos( tp_v1 );
    }
    m.vis.UpdateAll();
  }
}

bool Have_TP_BOT__HALF( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_BOT__HALF ) return true;
  }
  return false;
}

bool Have_TP_TOP__HALF( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_TOP__HALF ) return true;
  }
  return false;
}

bool Have_TP_BOT__LEFT_QTR( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_BOT__LEFT_QTR ) return true;
  }
  return false;
}

bool Have_TP_TOP__LEFT_QTR( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_TOP__LEFT_QTR ) return true;
  }
  return false;
}

bool Have_TP_BOT__RITE_QTR( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_BOT__RITE_QTR ) return true;
  }
  return false;
}

bool Have_TP_TOP__RITE_QTR( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_TOP__RITE_QTR ) return true;
  }
  return false;
}

void Quit_JoinTiles_TP_LEFT_HALF( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_RITE_HALF         ) { v->SetTilePos( TP_FULL ); break; }
    else if( TP == TP_TOP__RITE_QTR     ) v->SetTilePos( TP_TOP__HALF );
    else if( TP == TP_BOT__RITE_QTR     ) v->SetTilePos( TP_BOT__HALF );
    else if( TP == TP_RITE_QTR          ) v->SetTilePos( TP_RITE_HALF );
    else if( TP == TP_RITE_CTR__QTR     ) v->SetTilePos( TP_LEFT_HALF );
    else if( TP == TP_TOP__RITE_8TH     ) v->SetTilePos( TP_TOP__RITE_QTR );
    else if( TP == TP_TOP__RITE_CTR_8TH ) v->SetTilePos( TP_TOP__LEFT_QTR );
    else if( TP == TP_BOT__RITE_8TH     ) v->SetTilePos( TP_BOT__RITE_QTR );
    else if( TP == TP_BOT__RITE_CTR_8TH ) v->SetTilePos( TP_BOT__LEFT_QTR );
  }
}

void Quit_JoinTiles_TP_RITE_HALF( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_LEFT_HALF         ) { v->SetTilePos( TP_FULL ); break; }
    else if( TP == TP_TOP__LEFT_QTR     ) v->SetTilePos( TP_TOP__HALF );
    else if( TP == TP_BOT__LEFT_QTR     ) v->SetTilePos( TP_BOT__HALF );
    else if( TP == TP_LEFT_QTR          ) v->SetTilePos( TP_LEFT_HALF );
    else if( TP == TP_LEFT_CTR__QTR     ) v->SetTilePos( TP_RITE_HALF );
    else if( TP == TP_TOP__LEFT_8TH     ) v->SetTilePos( TP_TOP__LEFT_QTR );
    else if( TP == TP_TOP__LEFT_CTR_8TH ) v->SetTilePos( TP_TOP__RITE_QTR );
    else if( TP == TP_BOT__LEFT_8TH     ) v->SetTilePos( TP_BOT__LEFT_QTR );
    else if( TP == TP_BOT__LEFT_CTR_8TH ) v->SetTilePos( TP_BOT__RITE_QTR );
  }
}

void Quit_JoinTiles_TP_TOP__HALF( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_BOT__HALF         ) { v->SetTilePos( TP_FULL ); break; }
    else if( TP == TP_BOT__LEFT_QTR     ) v->SetTilePos( TP_LEFT_HALF );
    else if( TP == TP_BOT__RITE_QTR     ) v->SetTilePos( TP_RITE_HALF );
    else if( TP == TP_BOT__LEFT_8TH     ) v->SetTilePos( TP_LEFT_QTR );
    else if( TP == TP_BOT__RITE_8TH     ) v->SetTilePos( TP_RITE_QTR );
    else if( TP == TP_BOT__LEFT_CTR_8TH ) v->SetTilePos( TP_LEFT_CTR__QTR );
    else if( TP == TP_BOT__RITE_CTR_8TH ) v->SetTilePos( TP_RITE_CTR__QTR );
  }
}

void Quit_JoinTiles_TP_BOT__HALF( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_TOP__HALF         ) { v->SetTilePos( TP_FULL ); break; }
    else if( TP == TP_TOP__LEFT_QTR     ) v->SetTilePos( TP_LEFT_HALF );
    else if( TP == TP_TOP__RITE_QTR     ) v->SetTilePos( TP_RITE_HALF );
    else if( TP == TP_TOP__LEFT_8TH     ) v->SetTilePos( TP_LEFT_QTR );
    else if( TP == TP_TOP__RITE_8TH     ) v->SetTilePos( TP_RITE_QTR );
    else if( TP == TP_TOP__LEFT_CTR_8TH ) v->SetTilePos( TP_LEFT_CTR__QTR );
    else if( TP == TP_TOP__RITE_CTR_8TH ) v->SetTilePos( TP_RITE_CTR__QTR );
  }
}

void Quit_JoinTiles_TP_TOP__LEFT_QTR( Vis::Data& m )
{
  if( Have_TP_BOT__HALF(m) )
  {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_TOP__RITE_QTR     ) { v->SetTilePos( TP_TOP__HALF ); break; }
      else if( TP == TP_TOP__RITE_8TH     ) v->SetTilePos( TP_TOP__RITE_QTR );
      else if( TP == TP_TOP__RITE_CTR_8TH ) v->SetTilePos( TP_TOP__LEFT_QTR );
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_BOT__LEFT_QTR     ) { v->SetTilePos( TP_LEFT_HALF ); break; }
      else if( TP == TP_BOT__LEFT_8TH     ) v->SetTilePos( TP_LEFT_QTR );
      else if( TP == TP_BOT__LEFT_CTR_8TH ) v->SetTilePos( TP_LEFT_CTR__QTR );
    }
  }
}

void Quit_JoinTiles_TP_TOP__RITE_QTR( Vis::Data& m )
{
  if( Have_TP_BOT__HALF(m) )
  {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_TOP__LEFT_QTR     ) { v->SetTilePos( TP_TOP__HALF ); break; }
      else if( TP == TP_TOP__LEFT_8TH     ) v->SetTilePos( TP_TOP__LEFT_QTR );
      else if( TP == TP_TOP__LEFT_CTR_8TH ) v->SetTilePos( TP_TOP__RITE_QTR );
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_BOT__RITE_QTR     ) { v->SetTilePos( TP_RITE_HALF ); break; }
      else if( TP == TP_BOT__RITE_8TH     ) v->SetTilePos( TP_RITE_QTR );
      else if( TP == TP_BOT__RITE_CTR_8TH ) v->SetTilePos( TP_RITE_CTR__QTR );
    }
  }
}

void Quit_JoinTiles_TP_BOT__LEFT_QTR( Vis::Data& m )
{
  if( Have_TP_TOP__HALF(m) )
  {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_BOT__RITE_QTR     ) { v->SetTilePos( TP_BOT__HALF ); break; }
      else if( TP == TP_BOT__RITE_8TH     ) v->SetTilePos( TP_BOT__RITE_QTR );
      else if( TP == TP_BOT__RITE_CTR_8TH ) v->SetTilePos( TP_BOT__LEFT_QTR );
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_TOP__LEFT_QTR     ) { v->SetTilePos( TP_LEFT_HALF ); break; }
      else if( TP == TP_TOP__LEFT_8TH     ) v->SetTilePos( TP_LEFT_QTR );
      else if( TP == TP_TOP__LEFT_CTR_8TH ) v->SetTilePos( TP_LEFT_CTR__QTR );
    }
  }
}

void Quit_JoinTiles_TP_BOT__RITE_QTR( Vis::Data& m )
{
  if( Have_TP_TOP__HALF(m) )
  {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_BOT__LEFT_QTR     ) { v->SetTilePos( TP_BOT__HALF ); break; }
      else if( TP == TP_BOT__LEFT_8TH     ) v->SetTilePos( TP_BOT__LEFT_QTR );
      else if( TP == TP_BOT__LEFT_CTR_8TH ) v->SetTilePos( TP_BOT__RITE_QTR );
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_TOP__RITE_QTR     ) { v->SetTilePos( TP_RITE_HALF ); break; }
      else if( TP == TP_TOP__RITE_8TH     ) v->SetTilePos( TP_RITE_QTR );
      else if( TP == TP_TOP__RITE_CTR_8TH ) v->SetTilePos( TP_RITE_CTR__QTR );
    }
  }
}

void Quit_JoinTiles_TP_LEFT_QTR( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_LEFT_CTR__QTR     ) { v->SetTilePos( TP_LEFT_HALF ); break; }
    else if( TP == TP_TOP__LEFT_CTR_8TH ) v->SetTilePos( TP_TOP__LEFT_QTR );
    else if( TP == TP_BOT__LEFT_CTR_8TH ) v->SetTilePos( TP_BOT__LEFT_QTR );
  }
}

void Quit_JoinTiles_TP_RITE_QTR( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_RITE_CTR__QTR     ) { v->SetTilePos( TP_RITE_HALF ); break; }
    else if( TP == TP_TOP__RITE_CTR_8TH ) v->SetTilePos( TP_TOP__RITE_QTR );
    else if( TP == TP_BOT__RITE_CTR_8TH ) v->SetTilePos( TP_BOT__RITE_QTR );
  }
}

void Quit_JoinTiles_TP_LEFT_CTR__QTR( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_LEFT_QTR      ) { v->SetTilePos( TP_LEFT_HALF ); break; }
    else if( TP == TP_TOP__LEFT_8TH ) v->SetTilePos( TP_TOP__LEFT_QTR );
    else if( TP == TP_BOT__LEFT_8TH ) v->SetTilePos( TP_BOT__LEFT_QTR );
  }
}

void Quit_JoinTiles_TP_RITE_CTR__QTR( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = m.views[k][ m.file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_RITE_QTR      ) { v->SetTilePos( TP_RITE_HALF ); break; }
    else if( TP == TP_TOP__RITE_8TH ) v->SetTilePos( TP_TOP__RITE_QTR );
    else if( TP == TP_BOT__RITE_8TH ) v->SetTilePos( TP_BOT__RITE_QTR );
  }
}

void Quit_JoinTiles_TP_TOP__LEFT_8TH( Vis::Data& m )
{
  if( Have_TP_BOT__LEFT_QTR(m) )
  {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__LEFT_CTR_8TH ) { v->SetTilePos( TP_TOP__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__LEFT_8TH ) { v->SetTilePos( TP_LEFT_QTR ); break; }
    }
  }
}

void Quit_JoinTiles_TP_TOP__RITE_8TH( Vis::Data& m )
{
  if( Have_TP_BOT__RITE_QTR(m) )
  {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__RITE_CTR_8TH ) { v->SetTilePos( TP_TOP__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__RITE_8TH ) { v->SetTilePos( TP_RITE_QTR ); break; }
    }
  }
}

void Quit_JoinTiles_TP_TOP__LEFT_CTR_8TH( Vis::Data& m )
{
  if( Have_TP_BOT__LEFT_QTR(m) )
  {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__LEFT_8TH ) { v->SetTilePos( TP_TOP__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__LEFT_CTR_8TH ) { v->SetTilePos( TP_LEFT_CTR__QTR ); break; }
    }
  }
}

void Quit_JoinTiles_TP_TOP__RITE_CTR_8TH( Vis::Data& m )
{
  if( Have_TP_BOT__RITE_QTR(m) )
  {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__RITE_8TH ) { v->SetTilePos( TP_TOP__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__RITE_CTR_8TH ) { v->SetTilePos( TP_RITE_CTR__QTR ); break; }
    }
  }
}

void Quit_JoinTiles_TP_BOT__LEFT_8TH( Vis::Data& m )
{
  if( Have_TP_TOP__LEFT_QTR(m) )
  {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__LEFT_CTR_8TH ) { v->SetTilePos( TP_BOT__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__LEFT_8TH ) { v->SetTilePos( TP_LEFT_QTR ); break; }
    }
  }
}

void Quit_JoinTiles_TP_BOT__RITE_8TH( Vis::Data& m )
{
  if( Have_TP_TOP__RITE_QTR(m) )
  {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__RITE_CTR_8TH ) { v->SetTilePos( TP_BOT__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__RITE_8TH ) { v->SetTilePos( TP_RITE_QTR ); break; }
    }
  }
}

void Quit_JoinTiles_TP_BOT__LEFT_CTR_8TH( Vis::Data& m )
{
  if( Have_TP_TOP__LEFT_QTR(m) )
  {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__LEFT_8TH ) { v->SetTilePos( TP_BOT__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__LEFT_CTR_8TH ) { v->SetTilePos( TP_LEFT_CTR__QTR ); break; }
    }
  }
}

void Quit_JoinTiles_TP_BOT__RITE_CTR_8TH( Vis::Data& m )
{
  if( Have_TP_TOP__RITE_QTR(m) )
  {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__RITE_8TH ) { v->SetTilePos( TP_BOT__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = m.views[k][ m.file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__RITE_CTR_8TH ) { v->SetTilePos( TP_RITE_CTR__QTR ); break; }
    }
  }
}

void Quit_JoinTiles( Vis::Data& m, const Tile_Pos TP )
{
  Trace trace( __PRETTY_FUNCTION__ );
  // win is disappearing, so move its screen space to another view:
  if     ( TP == TP_LEFT_HALF )         Quit_JoinTiles_TP_LEFT_HALF(m);
  else if( TP == TP_RITE_HALF )         Quit_JoinTiles_TP_RITE_HALF(m);
  else if( TP == TP_TOP__HALF )         Quit_JoinTiles_TP_TOP__HALF(m);
  else if( TP == TP_BOT__HALF )         Quit_JoinTiles_TP_BOT__HALF(m);
  else if( TP == TP_TOP__LEFT_QTR )     Quit_JoinTiles_TP_TOP__LEFT_QTR(m);
  else if( TP == TP_TOP__RITE_QTR )     Quit_JoinTiles_TP_TOP__RITE_QTR(m);
  else if( TP == TP_BOT__LEFT_QTR )     Quit_JoinTiles_TP_BOT__LEFT_QTR(m);
  else if( TP == TP_BOT__RITE_QTR )     Quit_JoinTiles_TP_BOT__RITE_QTR(m);
  else if( TP == TP_LEFT_QTR )          Quit_JoinTiles_TP_LEFT_QTR(m);
  else if( TP == TP_RITE_QTR )          Quit_JoinTiles_TP_RITE_QTR(m);
  else if( TP == TP_LEFT_CTR__QTR )     Quit_JoinTiles_TP_LEFT_CTR__QTR(m);
  else if( TP == TP_RITE_CTR__QTR )     Quit_JoinTiles_TP_RITE_CTR__QTR(m);
  else if( TP == TP_TOP__LEFT_8TH )     Quit_JoinTiles_TP_TOP__LEFT_8TH(m);
  else if( TP == TP_TOP__RITE_8TH )     Quit_JoinTiles_TP_TOP__RITE_8TH(m);
  else if( TP == TP_TOP__LEFT_CTR_8TH ) Quit_JoinTiles_TP_TOP__LEFT_CTR_8TH(m);
  else if( TP == TP_TOP__RITE_CTR_8TH ) Quit_JoinTiles_TP_TOP__RITE_CTR_8TH(m);
  else if( TP == TP_BOT__LEFT_8TH )     Quit_JoinTiles_TP_BOT__LEFT_8TH(m);
  else if( TP == TP_BOT__RITE_8TH )     Quit_JoinTiles_TP_BOT__RITE_8TH(m);
  else if( TP == TP_BOT__LEFT_CTR_8TH ) Quit_JoinTiles_TP_BOT__LEFT_CTR_8TH(m);
  else /*( TP == TP_BOT__RITE_CTR_8TH*/ Quit_JoinTiles_TP_BOT__RITE_CTR_8TH(m);
}

void Quit_ShiftDown( Vis::Data& m )
{
  // Make copy of win's list of views and view history:
  ViewList win_views    ( m.views    [m.win] );
   unsList win_view_hist( m.file_hist[m.win] );

  // Shift everything down
  for( unsigned w=m.win+1; w<m.num_wins; w++ )
  {
    m.views    [w-1] = m.views    [w];
    m.file_hist[w-1] = m.file_hist[w];
  }
  // Put win's list of views at end of views:
  // Put win's view history at end of view historys:
  m.views    [m.num_wins-1] = win_views;
  m.file_hist[m.num_wins-1] = win_view_hist;
}

void QuitAll( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Move cursor down to bottom of screen:
  Console::Move_2_Row_Col( Console::Num_Rows()-1, 0 );

  // Put curson on a new line:
  Console::NewLine();

  m.running = false;
}

void Quit( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const Tile_Pos TP = CV(m)->GetTilePos();

  if( m.num_wins <= 1 ) QuitAll(m);
  else {
    m.diff_mode = false;

    if( m.win < m.num_wins-1 )
    {
      Quit_ShiftDown(m);
    }
    if( 0 < m.win ) m.win--;
    m.num_wins--;

    Quit_JoinTiles( m, TP );

    m.vis.UpdateAll();

    CV(m)->PrintCursor();
  }
}

void Handle_i( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.dot_buf.clear();
    m.key.dot_buf.push(__FILE__,__LINE__,'i');
    m.key.save_2_dot_buf = true;
  }

  if( m.diff_mode ) m.diff.Do_i();
  else              CV(m)->Do_i();

  if( !m.key.get_from_dot_buf )
  {
    m.key.save_2_dot_buf = false;
  }
}

void Handle_v( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.vis_buf.clear();
    m.key.vis_buf.push(__FILE__,__LINE__,'v');
    m.key.save_2_vis_buf = true;
  }
  const bool copy_vis_buf_2_dot_buf = m.diff_mode
                                    ? m.diff.Do_v()
                                    : CV(m)->Do_v();
  if( !m.key.get_from_dot_buf )
  {
    m.key.save_2_vis_buf = false;

    if( copy_vis_buf_2_dot_buf )
    {
      m.key.dot_buf.copy( m.key.vis_buf );
    }
  }
}

void Handle_V( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.vis_buf.clear();
    m.key.vis_buf.push(__FILE__,__LINE__,'V');
    m.key.save_2_vis_buf = true;
  }
  const bool copy_vis_buf_2_dot_buf = m.diff_mode
                                    ? m.diff.Do_V()
                                    : CV(m)->Do_V();
  if( !m.key.get_from_dot_buf )
  {
    m.key.save_2_vis_buf = false;

    if( copy_vis_buf_2_dot_buf )
    {
      m.key.dot_buf.copy( m.key.vis_buf );
    }
  }
}

void Handle_a( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.dot_buf.clear();
    m.key.dot_buf.push(__FILE__,__LINE__,'a');
    m.key.save_2_dot_buf = true;
  }

  if( m.diff_mode ) m.diff.Do_a();
  else              CV(m)->Do_a();

  if( !m.key.get_from_dot_buf )
  {
    m.key.save_2_dot_buf = false;
  }
}

void Handle_A( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.dot_buf.clear();
    m.key.dot_buf.push(__FILE__,__LINE__,'A');
    m.key.save_2_dot_buf = true;
  }

  if( m.diff_mode ) m.diff.Do_A();
  else              CV(m)->Do_A();

  if( !m.key.get_from_dot_buf )
  {
    m.key.save_2_dot_buf = false;
  }
}

void Handle_o( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.dot_buf.clear();
    m.key.dot_buf.push(__FILE__,__LINE__,'o');
    m.key.save_2_dot_buf = true;
  }

  if( m.diff_mode ) m.diff.Do_o();
  else              CV(m)->Do_o();

  if( !m.key.get_from_dot_buf )
  {
    m.key.save_2_dot_buf = false;
  }
}

void Handle_O( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.dot_buf.clear();
    m.key.dot_buf.push(__FILE__,__LINE__,'O');
    m.key.save_2_dot_buf = true;
  }

  if( m.diff_mode ) m.diff.Do_O();
  else              CV(m)->Do_O();

  if( !m.key.get_from_dot_buf )
  {
    m.key.save_2_dot_buf = false;
  }
}

void Handle_x( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.dot_buf.clear();
    m.key.dot_buf.push(__FILE__,__LINE__,'x');
  }
  if( m.diff_mode ) m.diff.Do_x();
  else              CV(m)->Do_x();
}

void Handle_s( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.dot_buf.clear();
    m.key.dot_buf.push(__FILE__,__LINE__,'s');
    m.key.save_2_dot_buf = true;
  }

  if( m.diff_mode ) m.diff.Do_s();
  else              CV(m)->Do_s();

  if( !m.key.get_from_dot_buf )
  {
    m.key.save_2_dot_buf = false;
  }
}

void Handle_c( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char C = m.key.In();

  if( C == 'w' )
  {
    if( !m.key.get_from_dot_buf )
    {
      m.key.dot_buf.clear();
      m.key.dot_buf.push(__FILE__,__LINE__,'c');
      m.key.dot_buf.push(__FILE__,__LINE__,'w');
      m.key.save_2_dot_buf = true;
    }
    if( m.diff_mode ) m.diff.Do_cw();
    else              CV(m)->Do_cw();

    if( !m.key.get_from_dot_buf )
    {
      m.key.save_2_dot_buf = false;
    }
  }
  else if( C == '$' )
  {
    if( !m.key.get_from_dot_buf )
    {
      m.key.dot_buf.clear();
      m.key.dot_buf.push(__FILE__,__LINE__,'c');
      m.key.dot_buf.push(__FILE__,__LINE__,'$');
      m.key.save_2_dot_buf = true;
    }
    if( m.diff_mode ) { m.diff.Do_D(); m.diff.Do_a(); }
    else              { CV(m)->Do_D(); CV(m)->Do_a(); }

    if( !m.key.get_from_dot_buf )
    {
      m.key.save_2_dot_buf = false;
    }
  }
}

void Handle_Dot( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<m.key.dot_buf.len() )
  {
    if( m.key.save_2_map_buf )
    {
      // Pop '.' off map_buf, because the contents of m.key.map_buf
      // will be saved to m.key.map_buf.
      m.key.map_buf.pop();
    }
    m.key.get_from_dot_buf = true;

    while( m.key.get_from_dot_buf )
    {
      const char CC = m.key.In();

      Vis::Data::CmdFunc cf = m.CmdFuncs[ CC ];
      if( cf ) (*cf)(m);
    }
    if( m.diff_mode ) {
      // Diff does its own update every time a command is run
    }
    else {
      // Dont update until after all the commands have been executed:
      CV(m)->GetFB()->Update();
    }
  }
}

void Handle_j( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoDown();
  else              CV(m)->GoDown();
}

void Handle_k( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoUp();
  else              CV(m)->GoUp();
}

void Handle_h( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoLeft();
  else              CV(m)->GoLeft();
}

void Handle_l( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoRight();
  else              CV(m)->GoRight();
}

void Handle_H( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoToTopLineInView();
  else              CV(m)->GoToTopLineInView();
}

void Handle_L( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoToBotLineInView();
  else              CV(m)->GoToBotLineInView();
}

void Handle_M( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoToMidLineInView();
  else              CV(m)->GoToMidLineInView();
}

void Handle_0( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoToBegOfLine();
  else              CV(m)->GoToBegOfLine();
}

void Handle_Q( Vis::Data& m )
{
  Handle_Dot(m);
  Handle_j(m);
  Handle_0(m);
}

void Handle_Dollar( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoToEndOfLine();
  else              CV(m)->GoToEndOfLine();
}

void Handle_Return( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoToBegOfNextLine();
  else              CV(m)->GoToBegOfNextLine();
}

void Handle_G( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoToEndOfFile();
  else              CV(m)->GoToEndOfFile();
}

void Handle_b( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoToPrevWord();
  else              CV(m)->GoToPrevWord();
}

void Handle_w( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoToNextWord();
  else              CV(m)->GoToNextWord();
}

void Handle_e( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoToEndOfWord();
  else              CV(m)->GoToEndOfWord();
}

void Handle_f( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.fast_char = m.key.In();

  if( m.diff_mode ) m.diff.Do_f( m.fast_char );
  else              CV(m)->Do_f( m.fast_char );
}

void Handle_SemiColon( Vis::Data& m )
{
  if( 0 <= m.fast_char )
  {
    if( m.diff_mode ) m.diff.Do_f( m.fast_char );
    else              CV(m)->Do_f( m.fast_char );
  }
}

void Handle_z( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC2 = m.key.In();

  if( CC2 == 't' || IsEndOfLineDelim( CC2 ) )
  {
    if( m.diff_mode ) m.diff.MoveCurrLineToTop();
    else              CV(m)->MoveCurrLineToTop();
  }
  else if( CC2 == 'z' )
  {
    if( m.diff_mode ) m.diff.MoveCurrLineCenter();
    else              CV(m)->MoveCurrLineCenter();
  }
  else if( CC2 == 'b' )
  {
    if( m.diff_mode ) m.diff.MoveCurrLineToBottom();
    else              CV(m)->MoveCurrLineToBottom();
  }
}

void Handle_Percent( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoToOppositeBracket();
  else              CV(m)->GoToOppositeBracket();
}

// Left squiggly bracket
void Handle_LeftSquigglyBracket( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoToLeftSquigglyBracket();
  else              CV(m)->GoToLeftSquigglyBracket();
}

// Right squiggly bracket
void Handle_RightSquigglyBracket( Vis::Data& m )
{
  if( m.diff_mode ) m.diff.GoToRightSquigglyBracket();
  else              CV(m)->GoToRightSquigglyBracket();
}

void Handle_F( Vis::Data& m )
{
  if( m.diff_mode )  m.diff.PageDown();
  else               CV(m)->PageDown();
}

void Handle_B( Vis::Data& m )
{
  if( m.diff_mode )  m.diff.PageUp();
  else               CV(m)->PageUp();
}

void Handle_m( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.key.save_2_map_buf || 0==m.key.map_buf.len() )
  {
    // When mapping, 'm' is ignored.
    // If not mapping and map buf len is zero, 'm' is ignored.
    return;
  }
  m.key.get_from_map_buf = true;

  while( m.key.get_from_map_buf )
  {
    const char CC = m.key.In();

    Vis::Data::CmdFunc cf = m.CmdFuncs[ CC ];
    if( cf ) (*cf)(m);
  }
  if( m.diff_mode ) {
    // Diff does its own update every time a command is run
  }
  else {
    // Dont update until after all the commands have been executed:
    CV(m)->GetFB()->Update();
  }
}

void Handle_g( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC2 = m.key.In();

  if( CC2 == 'g' )
  {
    if( m.diff_mode ) m.diff.GoToTopOfFile();
    else              CV(m)->GoToTopOfFile();
  }
  else if( CC2 == '0' )
  {
    if( m.diff_mode ) m.diff.GoToStartOfRow();
    else              CV(m)->GoToStartOfRow();
  }
  else if( CC2 == '$' )
  {
    if( m.diff_mode ) m.diff.GoToEndOfRow();
    else              CV(m)->GoToEndOfRow();
  }
  else if( CC2 == 'f' )
  {
    if( m.diff_mode ) m.diff.GoToFile();
    else              CV(m)->GoToFile();
  }
}

void Handle_W( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const char CC2 = m.key.In();

  if     ( CC2 == 'W' ) GoToNextWindow(m);
  else if( CC2 == 'l' ) GoToNextWindow_l(m);
  else if( CC2 == 'h' ) GoToNextWindow_h(m);
  else if( CC2 == 'j'
        || CC2 == 'k' ) GoToNextWindow_jk(m);
  else if( CC2 == 'R' ) FlipWindows(m);
}

void Handle_d( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char C = m.key.In();

  if( C == 'd' )
  {
    if( !m.key.get_from_dot_buf )
    {
      m.key.dot_buf.clear();
      m.key.dot_buf.push(__FILE__,__LINE__,'d');
      m.key.dot_buf.push(__FILE__,__LINE__,'d');
    }
    if( m.diff_mode ) m.diff.Do_dd();
    else              CV(m)->Do_dd();
  }
  else if( C == 'w' )
  {
    if( !m.key.get_from_dot_buf )
    {
      m.key.dot_buf.clear();
      m.key.dot_buf.push(__FILE__,__LINE__,'d');
      m.key.dot_buf.push(__FILE__,__LINE__,'w');
    }
    if( m.diff_mode ) m.diff.Do_dw();
    else              CV(m)->Do_dw();
  }
}

void Handle_y( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char C = m.key.In();

  if( C == 'y' )
  {
    if( m.diff_mode ) m.diff.Do_yy();
    else              CV(m)->Do_yy();
  }
  else if( C == 'w' )
  {
    if( m.diff_mode ) m.diff.Do_yw();
    else              CV(m)->Do_yw();
  }
}

void Help( Vis::Data& m )
{
  GoToBuffer( m, HELP_FILE );
}

void Create_Path_Parts_List( const String& path
                           , gArray_t<String*>& path_parts )
{
  String str_2_split = path;

  String part;
  while( str_2_split.split( DIR_DELIM_STR, part ) )
  {
    if( 0<part.len() )
    {
      path_parts.push( new(__FILE__,__LINE__) String( part ) );
    }
  }
}

void SetWinToBuffer( Vis::Data& m
                   , const unsigned win_idx
                   , const unsigned buf_idx
                   , const bool     update )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.views[win_idx].len() <= buf_idx )
  {
    m.vis.CmdLineMessage( "Buffer %lu does not exist", buf_idx );
  }
  else {
    if( buf_idx == m.file_hist[win_idx][0] )
    {
      // User asked for view that is currently displayed in win_idx.
      // Dont do anything.
    }
    else {
      m.file_hist[win_idx].insert(__FILE__,__LINE__, 0, buf_idx );

      // Remove subsequent buf_idx's from m.file_hist[win_idx]:
      for( unsigned k=1; k<m.file_hist[win_idx].len(); k++ )
      {
        if( buf_idx == m.file_hist[win_idx][k] ) m.file_hist[win_idx].remove( k );
      }
      View* pV_curr = m.views[win_idx][ m.file_hist[win_idx][0] ];
      View* pV_prev = m.views[win_idx][ m.file_hist[win_idx][1] ];

                   pV_curr->SetTilePos( pV_prev->GetTilePos() );
      if( update ) pV_curr->Update();
    }
  }
}

// If file is found, puts View of file in win_idx window,
// and returns the View, else returns null
//View* DoDiff_CheckPossibleFile( Vis::Data& m
//                              , const int win_idx
//                              , const char* pos_fname )
//{
//  struct stat sbuf;
//  int err = my_stat( pos_fname, sbuf );
//
//  if( 0 == err )
//  {
//    // File exists, find or create FileBuf, and set second view to display that file:
//    if( !m.vis.HaveFile( pos_fname ) )
//    {
//      FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( m.vis, pos_fname, true, FT_UNKNOWN );
//      pfb->ReadFile();
//    }
//  }
//  unsigned file_index = 0;
//  if( m.vis.HaveFile( pos_fname, &file_index ) )
//  {
//    SetWinToBuffer( m, win_idx, file_index, false );
//
//    return m.views[win_idx][ m.file_hist[win_idx][0] ];
//  }
//  return 0;
//}

// If file is found, puts View of file in win_idx window,
// and returns the View, else returns null
View* DoDiff_CheckPossibleFile( Vis::Data& m
                              , const int win_idx
                              , const char* pos_fname )
{
  if( FileExists( pos_fname ) )
  {
    // File exists, find or create FileBuf, and set second view to display that file:
    if( !m.vis.HaveFile( pos_fname ) )
    {
      FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( m.vis, pos_fname, true, FT_UNKNOWN );
      pfb->ReadFile();
    }
  }
  unsigned file_index = 0;
  if( m.vis.HaveFile( pos_fname, &file_index ) )
  {
    SetWinToBuffer( m, win_idx, file_index, false );

    return m.views[win_idx][ m.file_hist[win_idx][0] ];
  }
  return 0;
}

View* DoDiff_FindRegFileView( Vis::Data& m
                            , const FileBuf* pfb_reg
                            , const FileBuf* pfb_dir
                            , const unsigned win_idx
                            ,       View*    pv )
{
  String possible_fname = pfb_dir->GetFileName();
  String fname_extension;

  gArray_t<String*> path_parts;
  Create_Path_Parts_List( pfb_reg->GetFileName(), path_parts );

  for( int k=path_parts.len()-1; 0<=k; k-- )
  {
    // Revert back to pfb_dir.m_fname:
    possible_fname = pfb_dir->GetFileName();

    if( 0<fname_extension.len()
     && fname_extension.get_end(0)!=DIR_DELIM )
    {
      fname_extension.insert( 0, DIR_DELIM );
    }
    fname_extension.insert( 0, path_parts[k]->c_str() );

    possible_fname.append( fname_extension );

    const char* pos_fname = possible_fname.c_str();

    View* nv = DoDiff_CheckPossibleFile( m, win_idx, pos_fname );

    if( 0 != nv ) return nv;
  }
  return pv;
}

//void NoDiff( Vis::Data& m )
//{
//  if( true == m.diff_mode )
//  {
//    m.diff_mode = false;
//
//    m.vis.UpdateAll();
//  }
//}

void DoDiff( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Must be exactly 2 buffers to do diff:
  if( 2 == m.num_wins )
  {
    View* pv0 = m.views[0][ m.file_hist[0][0] ];
    View* pv1 = m.views[1][ m.file_hist[1][0] ];
    FileBuf* pfb0 = pv0->GetFB();
    FileBuf* pfb1 = pv1->GetFB();

    // New code in progress:
    bool ok = true;
    if( !pfb0->IsDir() && pfb1->IsDir() )
    {
      pv1 = DoDiff_FindRegFileView( m, pfb0, pfb1, 1, pv1 );
    }
    else if( pfb0->IsDir() && !pfb1->IsDir() )
    {
      pv0 = DoDiff_FindRegFileView( m, pfb1, pfb0, 0, pv0 );
    }
    else {
      if( !FileExists( pfb0->GetFileName() )
       || !FileExists( pfb1->GetFileName() ) )
      {
        ok = false;
      }
    }
    if( !ok ) m.running = false;
    else {
#ifndef WIN32
      timeval tv1; gettimeofday( &tv1, 0 );
#endif
      bool ok = m.diff.Run( pv0, pv1 );
      if( ok ) {
        m.diff_mode = true;

#ifndef WIN32
        timeval tv2; gettimeofday( &tv2, 0 );

        double secs = (tv2.tv_sec-tv1.tv_sec)
                    + double(tv2.tv_usec)/1e6
                    - double(tv1.tv_usec)/1e6;
        m.vis.CmdLineMessage( "Diff took: %g seconds", secs );
#endif
      }
    }
  }
}

void VSplitWindow( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.vis.NoDiff();

  View* cv = CV(m);
  const Tile_Pos cv_tp = cv->GetTilePos();

  if( m.num_wins < MAX_WINS
   && ( cv_tp == TP_FULL
     || cv_tp == TP_TOP__HALF
     || cv_tp == TP_BOT__HALF
     || cv_tp == TP_LEFT_HALF
     || cv_tp == TP_RITE_HALF
     || cv_tp == TP_TOP__LEFT_QTR
     || cv_tp == TP_BOT__LEFT_QTR
     || cv_tp == TP_TOP__RITE_QTR
     || cv_tp == TP_BOT__RITE_QTR ) )
  {
    ASSERT( __LINE__, m.win < m.num_wins, "m.win < m.num_wins" );

    m.file_hist[m.num_wins] = m.file_hist[m.win];

    View* nv = m.views[m.num_wins][ m.file_hist[m.num_wins][0] ];

    nv->SetTopLine ( cv->GetTopLine () );
    nv->SetLeftChar( cv->GetLeftChar() );
    nv->SetCrsRow  ( cv->GetCrsRow  () );
    nv->SetCrsCol  ( cv->GetCrsCol  () );

    m.win = m.num_wins;
    m.num_wins++;

    if( cv_tp == TP_FULL )
    {
      cv->SetTilePos( TP_LEFT_HALF );
      nv->SetTilePos( TP_RITE_HALF );
    }
    else if( cv_tp == TP_TOP__HALF )
    {
      cv->SetTilePos( TP_TOP__LEFT_QTR );
      nv->SetTilePos( TP_TOP__RITE_QTR );
    }
    else if( cv_tp == TP_BOT__HALF )
    {
      cv->SetTilePos( TP_BOT__LEFT_QTR );
      nv->SetTilePos( TP_BOT__RITE_QTR );
    }
    else if( cv_tp == TP_LEFT_HALF )
    {
      cv->SetTilePos( TP_LEFT_QTR );
      nv->SetTilePos( TP_LEFT_CTR__QTR );
    }
    else if( cv_tp == TP_RITE_HALF )
    {
      cv->SetTilePos( TP_RITE_CTR__QTR );
      nv->SetTilePos( TP_RITE_QTR );
    }
    else if( cv_tp == TP_TOP__LEFT_QTR )
    {
      cv->SetTilePos( TP_TOP__LEFT_8TH );
      nv->SetTilePos( TP_TOP__LEFT_CTR_8TH );
    }
    else if( cv_tp == TP_BOT__LEFT_QTR )
    {
      cv->SetTilePos( TP_BOT__LEFT_8TH );
      nv->SetTilePos( TP_BOT__LEFT_CTR_8TH );
    }
    else if( cv_tp == TP_TOP__RITE_QTR )
    {
      cv->SetTilePos( TP_TOP__RITE_CTR_8TH );
      nv->SetTilePos( TP_TOP__RITE_8TH );
    }
    else //( cv_tp == TP_BOT__RITE_QTR )
    {
      cv->SetTilePos( TP_BOT__RITE_CTR_8TH );
      nv->SetTilePos( TP_BOT__RITE_8TH );
    }
  }
  m.vis.UpdateAll();
}

void HSplitWindow( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.vis.NoDiff();

  View* cv = CV(m);
  const Tile_Pos cv_tp = cv->GetTilePos();

  if( m.num_wins < MAX_WINS
   && ( cv_tp == TP_FULL
     || cv_tp == TP_LEFT_HALF
     || cv_tp == TP_RITE_HALF
     || cv_tp == TP_LEFT_QTR
     || cv_tp == TP_RITE_QTR
     || cv_tp == TP_LEFT_CTR__QTR
     || cv_tp == TP_RITE_CTR__QTR ) )
  {
    ASSERT( __LINE__, m.win < m.num_wins, "m.win < m.num_wins" );

    m.file_hist[m.num_wins] = m.file_hist[m.win];

    View* nv = m.views[m.num_wins][ m.file_hist[m.num_wins][0] ];

    nv->SetTopLine ( cv->GetTopLine () );
    nv->SetLeftChar( cv->GetLeftChar() );
    nv->SetCrsRow  ( cv->GetCrsRow  () );
    nv->SetCrsCol  ( cv->GetCrsCol  () );

    m.win = m.num_wins;
    m.num_wins++;

    if( cv_tp == TP_FULL )
    {
      cv->SetTilePos( TP_TOP__HALF );
      nv->SetTilePos( TP_BOT__HALF );
    }
    else if( cv_tp == TP_LEFT_HALF )
    {
      cv->SetTilePos( TP_TOP__LEFT_QTR );
      nv->SetTilePos( TP_BOT__LEFT_QTR );
    }
    else if( cv_tp == TP_RITE_HALF )
    {
      cv->SetTilePos( TP_TOP__RITE_QTR );
      nv->SetTilePos( TP_BOT__RITE_QTR );
    }
    else if( cv_tp == TP_LEFT_QTR )
    {
      cv->SetTilePos( TP_TOP__LEFT_8TH );
      nv->SetTilePos( TP_BOT__LEFT_8TH );
    }
    else if( cv_tp == TP_RITE_QTR )
    {
      cv->SetTilePos( TP_TOP__RITE_8TH );
      nv->SetTilePos( TP_BOT__RITE_8TH );
    }
    else if( cv_tp == TP_LEFT_CTR__QTR )
    {
      cv->SetTilePos( TP_TOP__LEFT_CTR_8TH );
      nv->SetTilePos( TP_BOT__LEFT_CTR_8TH );
    }
    else //( cv_tp == TP_RITE_CTR__QTR )
    {
      cv->SetTilePos( TP_TOP__RITE_CTR_8TH );
      nv->SetTilePos( TP_BOT__RITE_CTR_8TH );
    }
  }
  m.vis.UpdateAll();
}

void RunCommand( Vis::Data& m )
{
  if( !m.vis.Shell_Running() )
  {
    if( SHELL_FILE == m.file_hist[ m.win ][ 0 ] )
    {
      m.shell.Run();
    }
  }
}

void HandleColon_e( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = CV(m);
  if( 0 == m.cbuf[1] ) // :e
  {
    FileBuf* pfb = pV->GetFB();
    pfb->ReReadFile();

    for( unsigned w=0; w<m.num_wins; w++ )
    {
      if( pfb == m.views[w][ m.file_hist[w][0] ]->GetFB() )
      {
        // View is currently displayed, perform needed update:
        m.views[w][ m.file_hist[w][0] ]->Update();
      }
    }
  }
  else // :e file_name
  {
    // Edit file of supplied file name:
    String fname( m.cbuf + 1 );

    if( FindFullFileNameRel2( pV->GetPathName(), fname ) )
    {
      unsigned file_index = 0;
      if( m.vis.HaveFile( fname.c_str(), &file_index ) )
      {
        GoToBuffer( m, file_index );
      }
      else {
        FileBuf* p_fb = new(__FILE__,__LINE__) FileBuf( m.vis, fname.c_str(), true, FT_UNKNOWN );
        p_fb->ReadFile();
        GoToBuffer( m, m.views[m.win].len()-1 );
      }
    }
  }
}

void HandleColon_w( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = CV(m);

  if( 0 == m.cbuf[1] ) // :w
  {
    if( pV == m.views[m.win][ SHELL_FILE ] )
    {
      // Dont allow SHELL_BUFFER to be save with :w.
      // Require :w filename.
      pV->PrintCursor();
    }
    else {
      pV->GetFB()->Write();

      // If Write fails, current view will be message buffer, in which case
      // we dont want to PrintCursor with the original view, because that
      // would put the cursor in the wrong position:
      if( CV(m) == pV ) pV->PrintCursor();
    }
  }
  else // :w file_name
  {
    // Write file of supplied file name:
    String fname( m.cbuf + 1 );

    if( FindFullFileNameRel2( pV->GetPathName(), fname ) )
    {
      unsigned file_index = 0;
      if( m.vis.HaveFile( fname.c_str(), &file_index ) )
      {
        m.vis.GetFileBuf( file_index )->Write();
      }
      else if( fname.get_end() != DIR_DELIM )
      {
        FileBuf* p_fb = new(__FILE__,__LINE__) FileBuf( m.vis, fname.c_str(), *pV->GetFB() );
        p_fb->Write();
      }
    }
  }
}

void HandleColon_b( Vis::Data& m )
{
  if( 0 == m.cbuf[1] ) // :b
  {
    GoToPrevBuffer(m);
  }
  else {
    // Switch to a different buffer:
    if     ( '#' == m.cbuf[1] ) GoToPoundBuffer(m); // :b#
    else if( 'c' == m.cbuf[1] ) GoToCurrBuffer(m);  // :bc
    else if( 'e' == m.cbuf[1] ) GoToBufferEditor(m);// :be
    else if( 'm' == m.cbuf[1] ) GoToMsgBuffer(m);   // :bm
    else {
      unsigned buffer_num = atol( m.cbuf+1 ); // :b<number>
      GoToBuffer( m, buffer_num );
    }
  }
}

void Handle_Colon( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  CV(m)->GoToCmdLineClear(":");
  m.colon.GetCommand(1);
  RemoveSpaces( m.cbuf );
  m.colon.MapEnd();

  if     ( strcmp( m.cbuf,"q"   )==0 ) Quit(m);
  else if( strcmp( m.cbuf,"qa"  )==0 ) QuitAll(m);
  else if( strcmp( m.cbuf,"help")==0 ) Help(m);
  else if( strcmp( m.cbuf,"diff")==0 ) DoDiff(m);
  else if( strcmp( m.cbuf,"nodiff")==0)m.vis.NoDiff();
  else if( strcmp( m.cbuf,"n"   )==0 ) GoToNextBuffer(m);
  else if( strcmp( m.cbuf,"se"  )==0 ) GoToSearchBuffer(m);
  else if( strcmp( m.cbuf,"vsp" )==0 ) VSplitWindow(m);
  else if( strcmp( m.cbuf,"sp"  )==0 ) HSplitWindow(m);
  else if( strcmp( m.cbuf,"cs1" )==0 ) { Console::Set_Color_Scheme_1(); }
  else if( strcmp( m.cbuf,"cs2" )==0 ) { Console::Set_Color_Scheme_2(); }
  else if( strcmp( m.cbuf,"cs3" )==0 ) { Console::Set_Color_Scheme_3(); }
  else if( strcmp( m.cbuf,"cs4" )==0 ) { Console::Set_Color_Scheme_4(); }
  else if( strcmp( m.cbuf,"hi"  )==0 ) m.colon.hi();
  else if( strncmp(m.cbuf,"cd",2)==0 ) { Ch_Dir(m); }
  else if( strncmp(m.cbuf,"syn",3)==0) { Set_Syntax(m); }
  else if( strcmp( m.cbuf,"pwd" )==0 ) { GetCWD(m); }
  else if( strcmp( m.cbuf,"sh"  )==0
        || strcmp( m.cbuf,"shell")==0) { GoToShellBuffer(m); }
  else if( strcmp( m.cbuf,"run" )==0 ) { RunCommand(m); }
  else if( strncmp(m.cbuf,"re",2)==0 ) { Console::Refresh(); }
  else if( strcmp( m.cbuf,"map" )==0 )    m.colon.MapStart();
  else if( strcmp( m.cbuf,"showmap")==0)  m.colon.MapShow();
  else if( strcmp( m.cbuf,"cover")==0)    m.colon.Cover();
  else if( strcmp( m.cbuf,"coverkey")==0) m.colon.CoverKey();
  else if( 'e' == m.cbuf[0] )             HandleColon_e(m);
  else if( 'w' == m.cbuf[0] )             HandleColon_w(m);
  else if( 'b' == m.cbuf[0] )             HandleColon_b(m);
  else if( '0' <= m.cbuf[0] && m.cbuf[0] <= '9' )
  {
    // Move cursor to line:
    const unsigned line_num = atol( m.cbuf );
    if( m.diff_mode ) m.diff.GoToLine( line_num );
    else               CV(m)->GoToLine( line_num );
  }
  else { // Put cursor back to line and column in edit window:
    if( m.diff_mode ) m.diff.PrintCursor( CV(m) );
    else               CV(m)->PrintCursor();
  }
}

void Handle_Slash( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  CV(m)->GoToCmdLineClear("/");

  m.colon.GetCommand(1);

  String new_slash( m.cbuf );

  m.vis.Handle_Slash_GotPattern( new_slash );
}

void Handle_n( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.diff_mode ) m.diff.Do_n();
  else              CV(m)->Do_n();
}

void Handle_N( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.diff_mode ) m.diff.Do_N();
  else              CV(m)->Do_N();
}

void Handle_u( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.diff_mode ) m.diff.Do_u();
  else              CV(m)->Do_u();
}

void Handle_U( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.diff_mode ) m.diff.Do_U();
  else              CV(m)->Do_U();
}

void Do_Star_PrintPatterns( Vis::Data& m, const bool HIGHLIGHT )
{
  for( unsigned w=0; w<m.num_wins; w++ )
  {
    m.views[w][ m.file_hist[w][0] ]->PrintPatterns( HIGHLIGHT );
  }
}

void Do_Star_ClearPatterns( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Tell every FileBuf that it needs to clear the old pattern:
  for( unsigned w=0; w<m.views[0].len(); w++ )
  {
    m.views[0][w]->GetFB()->NeedToClearStars();
  }
  // Remove star patterns from displayed FileBuf's only:
  for( unsigned w=0; w<m.num_wins; w++ )
  {
    View* pV = m.views[w][ m.file_hist[w][0] ];

    if( pV ) pV->GetFB()->ClearStars();
  }
}

// 1. Search for star pattern in search editor.
// 2. If star pattern is found in search editor,
//         move pattern to end of search editor
//    else add star pattern to end of search editor
// 3. Clear buffer editor un-saved change status
// 4. If search editor is displayed, update search editor window
//
void Do_Star_Update_Search_Editor( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* const pseV = m.views[m.win][ SE_FILE ];
  // Determine whether search editor has the star pattern
  const unsigned NUM_SE_LINES = pseV->GetFB()->NumLines(); // Number of search editor lines
  bool found_pattern_in_search_editor = false;
  unsigned line_in_search_editor = 0;

  for( unsigned ln=0; !found_pattern_in_search_editor && ln<NUM_SE_LINES; ln++ )
  {
    const unsigned LL = pseV->GetFB()->LineLen( ln );
    // Copy line into m.sbuf until end of line or NULL byte
    m.sbuf.clear();
    int c = 1;
    for( unsigned k=0; c && k<LL; k++ )
    {
      c = pseV->GetFB()->Get( ln, k );
      m.sbuf.push( c );
    }
    if( m.sbuf == m.star )
    {
      found_pattern_in_search_editor = true;
      line_in_search_editor = ln;
    }
  }
  // 2. If star pattern is found in search editor,
  //         move pattern to end of search editor
  //    else add star pattern to end of search editor
  if( found_pattern_in_search_editor )
  {
    // Move pattern to end of search editor, so newest searches are at bottom of file
    if( line_in_search_editor < NUM_SE_LINES-1 )
    {
      Line* p = pseV->GetFB()->RemoveLineP( line_in_search_editor );
      pseV->GetFB()->InsertLine( NUM_SE_LINES-1, p );
    }
  }
  else {
    // Push star onto search editor buffer
    Line line(__FILE__,__LINE__);
    for( const char* p=m.star.c_str(); *p; p++ ) line.push(__FILE__,__LINE__, *p );
    pseV->GetFB()->PushLine( line );
  }
  // 3. Clear buffer editor un-saved change status
  pseV->GetFB()->ClearChanged();

  // 4. If search editor is displayed, update search editor window
  for( unsigned w=0; w<m.num_wins; w++ )
  {
    if( SE_FILE == m.file_hist[w][0] )
    {
      m.views[w][ SE_FILE ]->Update();
    }
  }
}

void Do_Star_FindPatterns( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Tell every FileBuf that it needs to find the new pattern:
  for( unsigned w=0; w<m.views[0].len(); w++ )
  {
    m.views[0][w]->GetFB()->NeedToFindStars();
  }
  // Only find new pattern now for FileBuf's that are displayed:
  for( unsigned w=0; w<m.num_wins; w++ )
  {
    View* pV = m.views[w][ m.file_hist[w][0] ];

    if( pV ) pV->GetFB()->Find_Stars();
  }
}

void Handle_Star( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  String new_star = m.diff_mode ?  m.diff.Do_Star_GetNewPattern()
                                :  CV(m)->Do_Star_GetNewPattern();

  if( !m.slash && new_star == m.star ) return;

  // Un-highlight old star patterns for windows displayed:
  if( m.star.len() )
  { // Since m.diff_mode does Console::Update(),
    // no need to print patterns here if in m.diff_mode
    if( !m.diff_mode ) Do_Star_PrintPatterns( m, false );
  }
  Do_Star_ClearPatterns(m);

  m.star = new_star;

  if( m.star.len() )
  {
    m.slash = false;

    Do_Star_Update_Search_Editor(m);
    Do_Star_FindPatterns(m);

    // Highlight new star patterns for windows displayed:
    if( !m.diff_mode ) Do_Star_PrintPatterns( m, true );
  }
  if( m.diff_mode ) m.diff.Update();
  else {
    // Print out all the changes:
    Console::Update();
    // Put cursor back where it was
    CV(m)->PrintCursor();
  }
}

void Handle_D( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.dot_buf.clear();
    m.key.dot_buf.push(__FILE__,__LINE__,'D');
  }
  if( m.diff_mode ) m.diff.Do_D();
  else              CV(m)->Do_D();
}

void Handle_p( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.dot_buf.clear();
    m.key.dot_buf.push(__FILE__,__LINE__,'p');
  }
  if( m.diff_mode ) m.diff.Do_p();
  else              CV(m)->Do_p();
}

void Handle_P( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.dot_buf.clear();
    m.key.dot_buf.push(__FILE__,__LINE__,'P');
  }
  if( m.diff_mode ) m.diff.Do_P();
  else              CV(m)->Do_P();
}

void Handle_R( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.dot_buf.clear();
    m.key.dot_buf.push(__FILE__,__LINE__,'R');
    m.key.save_2_dot_buf = true;
  }
  if( m.diff_mode ) m.diff.Do_R();
  else              CV(m)->Do_R();

  if( !m.key.get_from_dot_buf )
  {
    m.key.save_2_dot_buf = false;
  }
}

void Handle_J( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.dot_buf.clear();
    m.key.dot_buf.push(__FILE__,__LINE__,'J');
  }
  if( m.diff_mode ) m.diff.Do_J();
  else              CV(m)->Do_J();
}

void Handle_Tilda( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf )
  {
    m.key.dot_buf.clear();
    m.key.dot_buf.push(__FILE__,__LINE__,'~');
  }
  if( m.diff_mode ) m.diff.Do_Tilda();
  else              CV(m)->Do_Tilda();
}

void InitCmdFuncs( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned k=0; k<128; k++ ) m.CmdFuncs[k] = 0;

  m.CmdFuncs[ 'i' ] = &Handle_i;
  m.CmdFuncs[ 'v' ] = &Handle_v;
  m.CmdFuncs[ 'V' ] = &Handle_V;
  m.CmdFuncs[ 'a' ] = &Handle_a;
  m.CmdFuncs[ 'A' ] = &Handle_A;
  m.CmdFuncs[ 'o' ] = &Handle_o;
  m.CmdFuncs[ 'O' ] = &Handle_O;
  m.CmdFuncs[ 'x' ] = &Handle_x;
  m.CmdFuncs[ 's' ] = &Handle_s;
  m.CmdFuncs[ 'c' ] = &Handle_c;
  m.CmdFuncs[ 'Q' ] = &Handle_Q;
  m.CmdFuncs[ 'k' ] = &Handle_k;
  m.CmdFuncs[ 'j' ] = &Handle_j;
  m.CmdFuncs[ 'h' ] = &Handle_h;
  m.CmdFuncs[ 'l' ] = &Handle_l;
  m.CmdFuncs[ 'H' ] = &Handle_H;
  m.CmdFuncs[ 'L' ] = &Handle_L;
  m.CmdFuncs[ 'M' ] = &Handle_M;
  m.CmdFuncs[ '0' ] = &Handle_0;
  m.CmdFuncs[ '$' ] = &Handle_Dollar;
  m.CmdFuncs[ '\n'] = &Handle_Return;
  m.CmdFuncs[ 'G' ] = &Handle_G;
  m.CmdFuncs[ 'b' ] = &Handle_b;
  m.CmdFuncs[ 'w' ] = &Handle_w;
  m.CmdFuncs[ 'e' ] = &Handle_e;
  m.CmdFuncs[ 'f' ] = &Handle_f;
  m.CmdFuncs[ ';' ] = &Handle_SemiColon;
  m.CmdFuncs[ '%' ] = &Handle_Percent;
  m.CmdFuncs[ '{' ] = &Handle_LeftSquigglyBracket;
  m.CmdFuncs[ '}' ] = &Handle_RightSquigglyBracket;
  m.CmdFuncs[ 'F' ] = &Handle_F;
  m.CmdFuncs[ 'B' ] = &Handle_B;
  m.CmdFuncs[ ':' ] = &Handle_Colon;
  m.CmdFuncs[ '/' ] = &Handle_Slash; // Crashes
  m.CmdFuncs[ '*' ] = &Handle_Star;
  m.CmdFuncs[ '.' ] = &Handle_Dot;
  m.CmdFuncs[ 'm' ] = &Handle_m;
  m.CmdFuncs[ 'g' ] = &Handle_g;
  m.CmdFuncs[ 'W' ] = &Handle_W;
  m.CmdFuncs[ 'd' ] = &Handle_d;
  m.CmdFuncs[ 'y' ] = &Handle_y;
  m.CmdFuncs[ 'D' ] = &Handle_D;
  m.CmdFuncs[ 'p' ] = &Handle_p;
  m.CmdFuncs[ 'P' ] = &Handle_P;
  m.CmdFuncs[ 'R' ] = &Handle_R;
  m.CmdFuncs[ 'J' ] = &Handle_J;
  m.CmdFuncs[ '~' ] = &Handle_Tilda;
  m.CmdFuncs[ 'n' ] = &Handle_n;
  m.CmdFuncs[ 'N' ] = &Handle_N;
  m.CmdFuncs[ 'u' ] = &Handle_u;
  m.CmdFuncs[ 'U' ] = &Handle_U;
  m.CmdFuncs[ 'z' ] = &Handle_z;
}

void AddToBufferEditor( Vis::Data& m, const char* fname )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line line(__FILE__, __LINE__, strlen( fname ) );

  for( const char* p=fname; *p; p++ ) line.push(__FILE__,__LINE__, *p );

  FileBuf* pfb = m.views[0][ BE_FILE ]->GetFB();
  pfb->PushLine( line );
  pfb->BufferEditor_Sort();
  pfb->ClearChanged();

  // Since buffer editor file has been re-arranged, make sure none of its
  // views have the cursor position past the end of the line
  for( unsigned k=0; k<MAX_WINS; k++ )
  {
    View* pV = m.views[k][ BE_FILE ];

    unsigned CL = pV->CrsLine();
    unsigned CP = pV->CrsChar();
    unsigned LL = pfb->LineLen( CL );

    if( LL <= CP )
    {
      pV->GoToCrsPos_NoWrite( CL, LL-1 );
    }
  }
}

bool File_Is_Displayed( Vis::Data& m, const unsigned file_num )
{
  for( unsigned w=0; w<m.num_wins; w++ )
  {
    if( file_num == m.file_hist[ w ][ 0 ] )
    {
      return true;
    }
  }
  return false;
}

bool FName_2_FNum( Vis::Data& m, const String& full_fname, unsigned& file_num )
{
  bool found = false;

  for( unsigned k=0; !found && k<m.files.len(); k++ )
  {
    if( full_fname == m.files[ k ]->GetFileName() )
    {
      found = true;
      file_num = k;
    }
  }
  return found;
}

void ReleaseFileNum( Vis::Data& m, const unsigned file_num )
{
  bool ok = m.files.remove( file_num );

  for( unsigned k=0; ok && k<MAX_WINS; k++ )
  {
    View* win_k_view_of_file_num;
    m.views[k].remove( file_num, win_k_view_of_file_num );

    if( 0==k ) {
      // Delete the file:
      MemMark(__FILE__,__LINE__);
      delete win_k_view_of_file_num->GetFB();
    }
    // Delete the view:
    MemMark(__FILE__,__LINE__);
    delete win_k_view_of_file_num;

    unsList& file_hist_k = m.file_hist[k];

    // Remove all file_num's from m.file_hist
    for( unsigned i=0; i<file_hist_k.len(); i++ )
    {
      if( file_num == file_hist_k[ i ] )
      {
        file_hist_k.remove( i );
      }
    }
    // Decrement all file_hist numbers greater than file_num
    for( unsigned i=0; i<file_hist_k.len(); i++ )
    {
      const unsigned val = file_hist_k[ i ];

      if( file_num < val )
      {
        file_hist_k[ i ] = val-1;
      }
    }
  }
}

// Print a command line message.
// Put cursor back in edit window.
//
void CmdLineMessage( Vis::Data& m, const char* const msg )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = CV(m);

  const unsigned WC  = pV->WorkingCols();
  const unsigned ROW = pV->Cmd__Line_Row();
  const unsigned COL = pV->Col_Win_2_GL( 0 );
  const unsigned MSG_LEN = strlen( msg );
  if( WC < MSG_LEN )
  {
    // messaged does not fit, so truncate beginning
    Console::SetS( ROW, COL, msg + (MSG_LEN - WC), S_NORMAL );
  }
  else {
    // messaged fits, add spaces at end
    Console::SetS( ROW, COL, msg, S_NORMAL );
    for( unsigned k=0; k<(WC-MSG_LEN); k++ )
    {
      Console::Set( ROW, pV->Col_Win_2_GL( k+MSG_LEN ), ' ', S_NORMAL );
    }
  }
  Console::Update();
  pV->PrintCursor();
}

void Window_Message( Vis::Data& m, const char* const msg )
{
  Trace trace( __PRETTY_FUNCTION__ );

  FileBuf* pMB = m.views[0][MSG_FILE]->GetFB();

  // Clear Message Buffer:
  while( pMB->NumLines() ) pMB->PopLine();

  // Add msg to pMB
  Line* pL = 0;
  const unsigned MSB_BUF_LEN = strlen( msg );
  for( unsigned k=0; k<MSB_BUF_LEN; k++ )
  {
    if( 0==pL ) pL = m.vis.BorrowLine( __FILE__,__LINE__ );

    const char C = msg[k];

    if( C == '\n' ) { pMB->PushLine( pL ); pL = 0; }
    else            { pL->push(__FILE__,__LINE__, C ); }
  }
  // Make sure last borrowed line gets put into Message Buffer:
  if( pL ) pMB->PushLine( pL );

  // Initially, put cursor at top of Message Buffer:
  View* pV = m.views[m.win][MSG_FILE];
  pV->Clear_Context();

  GoToMsgBuffer(m);
}

Vis::Vis()
  : m( *new(__FILE__, __LINE__) Data( *this ) )
{
}

void Vis::Init( const int ARGC, const char* const ARGV[] )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.cbuf[0] = 0;

  Console::SetVis( &m.vis );

  m.running = Console::GetWindowSize();
              Console::SetConsoleCursor();

  InitBufferEditor(m);
  InitHelpBuffer(m);
  InitSearchBuffer(m);
  InitMsgBuffer(m);
  InitShellBuffer(m);
  const bool run_diff = InitUserFiles( m, ARGC, ARGV )
                     && (SHELL_FILE+1+2) == m.files.len();
  InitFileHistory(m);
  InitCmdFuncs(m);

  if( ! run_diff )
  {
    UpdateAll();
  }
  else {
    // User supplied: "-d file1 file2", so run diff:
    m.diff_mode = true;
    m.num_wins = 2;
    m.file_hist[ 0 ][0] = 5;
    m.file_hist[ 1 ][0] = 6;
    m.views[0][ m.file_hist[ 0 ][0] ]->SetTilePos( TP_LEFT_HALF );
    m.views[1][ m.file_hist[ 1 ][0] ]->SetTilePos( TP_RITE_HALF );

    DoDiff(m);
  }
}

Vis::~Vis()
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned i=0; i<MAX_WINS; i++ )
  {
    for( unsigned k=0; k<m.views[i].len(); k++ )
    {
      MemMark(__FILE__,__LINE__);
      delete m.views[i][k];
    }
  }
  for( unsigned k=0; k<m.files.len(); k++ )
  {
    MemMark(__FILE__,__LINE__);
    delete m.files[k];
  }
  delete &m;
}

void Vis::Run()
{
  Trace trace( __PRETTY_FUNCTION__ );

  while( m.running )
  {
    Handle_Cmd( m );
  }
  Console::Flush();
}

void Vis::Stop()
{
  m.running = false;
}

View* Vis::CV() const
{
  return m.views[m.win][ m.file_hist[m.win][0] ];
}

View* Vis::WinView( const unsigned w ) const
{
  return m.views[w][ m.file_hist[w][0] ];
}

FileBuf* Vis::FileNum2Buf( const unsigned file_num ) const
{
  return m.views[0][ file_num ]->GetFB();
}

unsigned Vis::GetNumWins() const
{
  return m.num_wins;
}

Paste_Mode Vis::GetPasteMode() const
{
  return m.paste_mode;
}

void Vis::SetPasteMode( Paste_Mode pm )
{
  m.paste_mode = pm;
}

void Vis::NoDiff()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( true == m.diff_mode )
  {
    m.diff_mode = false;

    View* pvS = m.diff.GetViewShort();
    View* pvL = m.diff.GetViewLong();

    if( 0 != pvS )
    {
      pvS->SetTopLine ( m.diff.GetTopLine ( pvS ) );
      pvS->SetLeftChar( m.diff.GetLeftChar() );
      pvS->SetCrsRow  ( m.diff.GetCrsRow  () );
      pvS->SetCrsCol  ( m.diff.GetCrsCol  () );
    }
    if( 0 != pvL )
    {
      pvL->SetTopLine ( m.diff.GetTopLine ( pvL ) );
      pvL->SetLeftChar( m.diff.GetLeftChar() );
      pvL->SetCrsRow  ( m.diff.GetCrsRow  () );
      pvL->SetCrsCol  ( m.diff.GetCrsCol  () );
    }
    UpdateAll();
  }
}

bool Vis::InDiffMode() const
{
  return m.diff_mode;
}

bool Vis::Shell_Running() const
{
  return m.shell.Running();;
}

void Vis::Update_Shell()
{
  m.shell.Update();
}

bool Vis::RunningDot() const
{
  return m.key.get_from_dot_buf;
}

FileBuf* Vis::GetFileBuf( const unsigned index ) const
{
  return m.files[ index ];
}

unsigned Vis::GetStarLen() const
{
  return m.star.len();
}

const char* Vis::GetStar() const
{
  return m.star.c_str();
}

bool Vis::GetSlash() const
{
  return m.slash;
}

// If window has resized, update window
void Vis::CheckWindowSize()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned numRows = Console::Num_Rows();
  const unsigned numCols = Console::Num_Cols();

  if( Console::GetWindowSize() )
  {
    if( Console::Num_Rows() != numRows
     || Console::Num_Cols() != numCols )
    {
      AdjustViews(m);
      Console::Invalidate();
      UpdateAll();
    }
  }
}

void Vis::CheckFileModTime()
{
  Trace trace( __PRETTY_FUNCTION__ );

  FileBuf* pfb = CV()->GetFB();
  const char* fname = pfb->GetFileName();

  const double curr_mod_time = ModificationTime( fname );

  if( pfb->GetModTime() < curr_mod_time )
  {
    if( pfb->IsDir() )
    {
      // Dont ask the user, just read in the directory.
      // pfb->GetModTime() will get updated in pfb->ReReadFile()
      pfb->ReReadFile();

      for( unsigned w=0; w<m.num_wins; w++ )
      {
        if( pfb == m.views[w][ m.file_hist[w][0] ]->GetFB() )
        {
          // View is currently displayed, perform needed update:
          m.views[w][ m.file_hist[w][0] ]->Update();
        }
      }
    }
    else { // Regular file
      // Update file modification time so that the message window
      // will not keep popping up:
      pfb->SetModTime( curr_mod_time );

      m.vis.Window_Message("\n%s\n\nhas changed since it was read in\n\n", fname );
    }
  }
}

void Vis::Add_FileBuf_2_Lists_Create_Views( FileBuf* pfb, const char* fname )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Add this file buffer to global list of files
  m.files.push(__FILE__,__LINE__, pfb );
  // Create a new view for each window for FileBuf
  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* pV  = new(__FILE__,__LINE__) View( m.vis, m.key, *pfb, m.reg );
    bool ok = m.views[w].push(__FILE__,__LINE__, pV );
    ASSERT( __LINE__, ok, "ok" );
    pfb->AddView( pV );
  }
  // Push file name onto buffer editor buffer
  AddToBufferEditor( m, fname );
}

// Print a command line message.
// Put cursor back in edit window.
//
void Vis::CmdLineMessage( const char* const msg_fmt, ... )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const int BUF_SZ = 2048;
  char msg_buf[ BUF_SZ ];

  va_list argp;
  va_start( argp, msg_fmt );
  vsprintf( msg_buf, msg_fmt, argp );
  va_end( argp );

  ::CmdLineMessage( m, msg_buf );
}

void Vis::Window_Message( const char* const msg_fmt, ... )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const int BUF_SZ = 2048;
  char msg_buf[ BUF_SZ ];

  va_list argp;
  va_start( argp, msg_fmt );
  vsprintf( msg_buf, msg_fmt, argp );
  va_end( argp );

  ::Window_Message( m, msg_buf );
}

void Vis::UpdateAll()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.diff_mode )
  {
    m.diff.Update();
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      m.views[k][ m.file_hist[k][0] ]->Update( false );
    }
    PrintCursor();
  }
}

bool Vis::Update_Status_Lines()
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool updated_a_sts_line = false;

  if( m.diff_mode )
  {
     updated_a_sts_line = m.diff.Update_Status_Lines();
  }
  else {
    for( unsigned w=0; w<m.num_wins; w++ )
    {
      // pV points to currently displayed view in window w:
      View* const pV = m.views[w][ m.file_hist[w][0] ];

      if( pV->GetStsLineNeedsUpdate() )
      {
        // Update status line:
        pV->PrintStsLine();     // Print status line.
        Console::Update();

        pV->SetStsLineNeedsUpdate( false );
        updated_a_sts_line = true;
      }
    }
  }
  return updated_a_sts_line;
}

// This ensures that proper change status is displayed around each window:
// '+++' for unsaved changes, and
// '   ' for no unsaved changes
bool Vis::Update_Change_Statuses()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Update buffer changed status around windows:
  bool updated_change_sts = false;

  for( unsigned w=0; w<m.num_wins; w++ )
  {
    // pV points to currently displayed view in window w:
    View* const pV = m.views[w][ m.file_hist[w][0] ];

    if( pV->GetUnSavedChangeSts() != pV->GetFB()->Changed() )
    {
      pV->Print_Borders();
      pV->SetUnSavedChangeSts( pV->GetFB()->Changed() );
      updated_change_sts = true;
    }
  }
  return updated_change_sts;
}

void Vis::PrintCursor()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.diff_mode ) m.diff.PrintCursor( CV() );
  else               CV()->PrintCursor();
}

bool Vis::HaveFile( const char* file_name, unsigned* file_index )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool already_have_file = false;

  const unsigned NUM_FILES = m.files.len();

  for( unsigned k=0; !already_have_file && k<NUM_FILES; k++ )
  {
    if( 0==strcmp( m.files[k]->GetFileName(), file_name ) )
    {
      already_have_file = true;

      if( file_index ) *file_index = k;
    }
  }
  return already_have_file;
}

bool Vis::File_Is_Displayed( const String& full_fname )
{
  Trace trace( __PRETTY_FUNCTION__ );

  unsigned file_num = 0;

  if( FName_2_FNum( m, full_fname, file_num ) )
  {
    return ::File_Is_Displayed( m, file_num );
  }
  return false;
}

void Vis::ReleaseFileName( const String& full_fname )
{
  Trace trace( __PRETTY_FUNCTION__ );

  unsigned file_num = 0;
  if( FName_2_FNum( m, full_fname, file_num ) )
  {
    ::ReleaseFileNum( m, file_num );
  }
}

// Return true if went to buffer indicated by fname, else false
bool Vis::GoToBuffer_Fname( String& fname )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // 1. Search for fname in buffer list, and if found, go to that buffer:
  unsigned file_index = 0;
  if( HaveFile( fname.c_str(), &file_index ) )
  {
    GoToBuffer( m, file_index );
    return true;
  }
  // 2. Get full file name
  if( fname.has_at( DirDelimStr(), 0 ) && FileExists( fname.c_str() ) )
  {
    ; // fname is already a full file name
  }
  else if( !FindFullFileNameRel2( CV()->GetPathName(), fname ) )
  {
    m.vis.CmdLineMessage( "Could not find file: %s", fname.c_str() );
    return false;
  }
  // 3. Search for fname in buffer list, and if found, go to that buffer:
  if( HaveFile( fname.c_str(), &file_index ) )
  {
    GoToBuffer( m, file_index ); return true;
  }
  // 4. See if file exists, and if so, add a file buffer, and go to that buffer
  bool exists = false;
  const bool IS_DIR = fname.get_end() == DIR_DELIM;
  if( IS_DIR )
  {
    DIR* dp = opendir( fname.c_str() );
    if( dp ) {
      exists = true;
      closedir( dp );
    }
  }
  else {
    FILE* fp = fopen( fname.c_str(), "rb" );
    if( fp ) {
      exists = true;
      fclose( fp );
    }
  }
  if( exists )
  {
    FileBuf* fb = new(__FILE__,__LINE__) FileBuf( m.vis, fname.c_str(), true, FT_UNKNOWN );
    fb->ReadFile();
    GoToBuffer( m, m.views[m.win].len()-1 );
  }
  else {
    m.vis.CmdLineMessage( "Could not find file: %s", fname.c_str() );
    return false;
  }
  return true;
}

void Vis::Handle_f()
{
  ::Handle_f(m);
}

void Vis::Handle_z()
{
  ::Handle_z(m);
}

void Vis::Handle_SemiColon()
{
  ::Handle_SemiColon(m);
}

//void Vis::Handle_Slash_GotPattern( const String& pattern
//                                 , const bool MOVE_TO_FIRST_PATTERN )
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//  if( m.slash && pattern == m.star )
//  {
//    CV()->PrintCursor();
//    return;
//  }
//  // Un-highlight old star patterns for windows displayed:
//  if( 0 < m.star.len()  )
//  { // Since m.diff_mode does Console::Update(),
//    // no need to print patterns here if in m.diff_mode
//    if( !m.diff_mode ) Do_Star_PrintPatterns( m, false );
//  }
//  Do_Star_ClearPatterns(m);
//
//  m.star = pattern;
//
//  if( !m.star.len() ) CV()->PrintCursor();
//  else {
//    m.slash = true;
//
//    Do_Star_Update_Search_Editor(m);
//    Do_Star_FindPatterns(m);
//
//    // Highlight new star patterns for windows displayed:
//    if( !m.diff_mode )
//    {
//      Do_Star_PrintPatterns( m, true );
//      Console::Update();
//    }
//    if( MOVE_TO_FIRST_PATTERN )
//    {
//      if( m.diff_mode ) m.diff.Do_n(); // Move to first pattern
//      else               CV()->Do_n(); // Move to first pattern
//    }
//    if( m.diff_mode ) m.diff.Update();
//  }
//}

void Vis::Handle_Slash_GotPattern( const String& pattern
                                 , const bool MOVE_TO_FIRST_PATTERN )
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( m.slash && pattern == m.star )
  {
    CV()->PrintCursor();
    return;
  }
  // Un-highlight old star patterns for windows displayed:
  if( 0 < m.star.len()  )
  { // Since m.diff_mode does Console::Update(),
    // no need to print patterns here if in m.diff_mode
    if( !m.diff_mode ) Do_Star_PrintPatterns( m, false );
  }
  Do_Star_ClearPatterns(m);

  m.star = pattern;

  if( !m.star.len() ) CV()->PrintCursor();
  else {
    m.slash = true;

    Do_Star_Update_Search_Editor(m);
    Do_Star_FindPatterns(m);

    // Highlight new star patterns for windows displayed:
    if( m.diff_mode )
    {
      if( MOVE_TO_FIRST_PATTERN ) m.diff.Do_n(); // Move to first pattern
      m.diff.Update();
    }
    else {
      Do_Star_PrintPatterns( m, true );
      if( MOVE_TO_FIRST_PATTERN ) CV()->Do_n(); // Move to first pattern
      else                        Console::Update();
    }
  }
}

// Line returned has at least SIZE, but zero length
//
Line* Vis::BorrowLine( const char* _FILE_
                     , const unsigned _LINE_
                     , const unsigned SIZE )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;

  if( m.line_cache.len() )
  {
    bool ok = m.line_cache.pop( lp );
    ASSERT( __LINE__, ok, "ok" );

    ok = lp->inc_size( __FILE__, __LINE__, SIZE );
    ASSERT( __LINE__, ok, "ok" );

    lp->clear();
  }
  else {
    lp = new( _FILE_, _LINE_ ) Line( __FILE__, __LINE__, SIZE );
  }
  return lp;
}

// Line returned has LEN and is filled up to LEN with FILL
//
Line* Vis::BorrowLine( const char* _FILE_
                     , const unsigned _LINE_
                     , const unsigned LEN, const uint8_t FILL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;

  if( m.line_cache.len() )
  {
    bool ok = m.line_cache.pop( lp );
    ASSERT( __LINE__, ok, "ok" );

    ok = lp->set_len(__FILE__,__LINE__, LEN );
    ASSERT( __LINE__, ok, "ok" );

    for( unsigned k=0; k<LEN; k++ ) lp->set( k, FILL );
  }
  else {
    lp = new(_FILE_,_LINE_) Line( __FILE__, __LINE__, LEN, FILL );
  }
  return lp;
}

// Line returned has same len and contents as line
//
Line* Vis::BorrowLine( const char* _FILE_
                     , const unsigned _LINE_
                     , const Line& line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;

  if( m.line_cache.len() )
  {
    bool ok = m.line_cache.pop( lp );
    ASSERT( __LINE__, ok, "ok" );

    ok = lp->copy( line );
    ASSERT( __LINE__, ok, "ok" );
  }
  else {
    lp = new(_FILE_,_LINE_) Line( line );
  }
  return lp;
}

void Vis::ReturnLine( Line* lp )
{
  if( lp ) m.line_cache.push( lp );
}

LineChange* Vis::BorrowLineChange( const ChangeType type
                                 , const unsigned   lnum
                                 , const unsigned   cpos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  LineChange* lcp = 0;

  if( m.change_cache.len() )
  {
    bool ok = m.change_cache.pop( lcp );

    lcp->type = type;
    lcp->lnum = lnum;
    lcp->cpos = cpos;
    lcp->line.clear();
  }
  else {
    lcp = new(__FILE__,__LINE__) LineChange( type, lnum, cpos );
  }
  return lcp;
}

void Vis::ReturnLineChange( LineChange* lcp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( lcp ) m.change_cache.push( lcp );
}

int main( int argc, char* argv[] )
{
  PROG_NAME = argv[0];

  Console::SetSignals();

  if( Console::Set_tty() )
  {
    atexit( Console::AtExit );

    Console::Allocate();
    Trace  ::Allocate();

    Vis* pVis = new(__FILE__,__LINE__) Vis();

    pVis->Init( argc, argv );
    pVis->Run();

    Trace::Print();

    MemMark(__FILE__,__LINE__); delete pVis; pVis = 0;

    Trace  ::Cleanup();
    Console::Cleanup();
  }
  MemClean();
  Log.Dump();
  return 0;
}

