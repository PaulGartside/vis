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
#include "ChangeHist.hh"
#include "Diff.hh"
#include "View.hh"
#include "LineView.hh"
#include "Key.hh"
#include "Shell.hh"
#include "Vis.hh"

const char* PROG_NAME;

const int FD_IO = 0; // read/write file descriptor

extern MemLog<MEM_LOG_BUF_SIZE> Log;
extern const char* DIR_DELIM_STR;

extern const uint16_t MAX_COLS   = 1024; // Arbitrary maximum char width of window
extern const unsigned BE_FILE    = 0;    // Buffer editor file
extern const unsigned HELP_FILE  = 1;    // Help          file
extern const unsigned MSG_FILE   = 2;    // Message       file
extern const unsigned SHELL_FILE = 3;    // Command Shell file
extern const unsigned COLON_FILE = 4;    // Colon command file
extern const unsigned SLASH_FILE = 5;    // Slash command file
extern const unsigned USER_FILE  = 6;    // First user file

const char*  EDIT_BUF_NAME = "BUFFER_EDITOR";
const char*  HELP_BUF_NAME = "VIS_HELP";
const char*  MSG__BUF_NAME = "MESSAGE_BUFFER";
const char* SHELL_BUF_NAME = "SHELL_BUFFER";
const char* COLON_BUF_NAME = "COLON_BUFFER";
const char* SLASH_BUF_NAME = "SLASH_BUFFER";

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
  Shell      shell;
  char       cbuf[MAX_COLS];    // General purpose char buffer
  String     sbuf;              // General purpose string buffer
  unsigned   win;               // Sub-window index
  unsigned   num_wins;          // Number of sub-windows currently on screen
  FileList   files;             // list of file buffers
  ViewList   views[MAX_WINS];   // Array of lists of file views
  FileBuf*   colon_file;        // Buffer for colon commands
  LineView*  colon_view;        // View   of  colon commands
  FileBuf*   slash_file;        // Buffer for slash commands
  LineView*  slash_view;        // View   of  slash commands
  unsList    file_hist[MAX_WINS]; // Array of lists of view history. [win][m_view_num]
  LinesList  reg;               // Register
  LinesList  line_cache;
  ChangeList change_cache;
  Paste_Mode paste_mode;
  bool       diff_mode; // true if displaying diff
  bool       colon_mode;// true if cursor is on vis colon line
  bool       slash_mode;// true if cursor is on vis slash line
  bool       sort_by_time;
  String     regex;     // current regular expression pattern to highlight
  int        fast_char; // Char on line to goto when ';' is entered
  unsigned   repeat;
  String     repeat_buf;
  View*      cv_old;

  typedef void (*CmdFunc) ( Data& m );
  CmdFunc ViewFuncs[128];
  CmdFunc LineFuncs[128];

  Data( Vis& vis );
  ~Data();
};

Vis::Data::Data( Vis& vis )
  : vis( vis )
  , running( true )
  , key()
  , diff( vis, key, reg )
  , shell( vis )
  , win( 0 )
  , num_wins( 1 )
  , files()
  , views()
  , file_hist()
  , reg()
  , line_cache()
  , change_cache()
  , paste_mode( PM_LINE )
  , diff_mode( false )
  , colon_mode( false )
  , slash_mode( false )
  , sort_by_time( false )
  , regex()
  , fast_char( -1 )
  , repeat( 1 )
  , repeat_buf()
  , cv_old( nullptr )
{
}

Vis::Data::~Data()
{
}

void Handle_Cmd( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC = m.key.In();

  if( ('1' <= CC && CC <= '9')
   || ('0' == CC && 0 < m.repeat_buf.len()) ) //< Dont override [1-9]0 movement
  {
    m.repeat_buf.push( CC );
  }
  else {
    if( 0 < m.repeat_buf.len() )
    {
      m.repeat = atol( m.repeat_buf.c_str() );
    }
    Vis::Data::CmdFunc cf = ( m.colon_mode || m.slash_mode )
                          ? m.LineFuncs[ CC ]
                          : m.ViewFuncs[ CC ];
    if( cf ) (*cf)(m);

    m.repeat = 1;
    m.repeat_buf.clear();
  }
}

View* CV( Vis::Data& m )
{
  return m.views[m.win][ m.file_hist[m.win][0] ];
}

View* PV( Vis::Data& m )
{
  return m.views[m.win][ m.file_hist[m.win][1] ];
}

// Get view of window w, currently displayed file
View* GetView_Win( Vis::Data& m, const unsigned w )
{
  return m.views[w][ m.file_hist[w][0] ];
}

// Get view of window w, prev'th displayed file
View* GetView_WinPrev( Vis::Data& m, const unsigned w, const unsigned prev )
{
  View* pV = 0;

  if( prev < m.file_hist[w].len() )
  {
    pV = m.views[w][ m.file_hist[w][prev] ];
  }
  return pV;
}

// Get window number of currently displayed View
int GetWinNum_Of_View( Vis::Data& m, const View* rV )
{
  for( int w=0; w<m.num_wins; w++ )
  {
    if( rV == GetView_Win( m, w ) )
    {
      return w;
    }
  }
  return -1;
}

unsigned Curr_FileNum( Vis::Data& m )
{
  return m.file_hist[ m.win ][ 0 ];
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
    const char* fname = CV(m)->GetFB()->GetPathName();
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
    GetView_Win( m, w )->SetViewPos();
  }
}

void InitBufferEditor( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m.files in Add_FileBuf_2_Lists_Create_Views()
  // Buffer editor, BE_FILE(0)
  FileBuf* pfb = new(__FILE__,__LINE__)
                 FileBuf( m.vis, EDIT_BUF_NAME, false, FT_BUFFER_EDITOR );
}

void InitHelpBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m.files in Add_FileBuf_2_Lists_Create_Views()
  // Help buffer, HELP_FILE(1)
  FileBuf* pfb = new(__FILE__,__LINE__)
                 FileBuf( m.vis, HELP_BUF_NAME, false, FT_TEXT );
  pfb->ReadString( HELP_STR );
}

void InitMsgBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m.files in Add_FileBuf_2_Lists_Create_Views()
  // Message buffer, MSG_FILE(3)
  FileBuf* pfb = new(__FILE__,__LINE__)
                 FileBuf( m.vis, MSG__BUF_NAME, false, FT_TEXT );
}

void InitShellBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m.files in Add_FileBuf_2_Lists_Create_Views()
  // Shell command buffer, SHELL_FILE(4)
  FileBuf* pfb = new(__FILE__,__LINE__)
                 FileBuf( m.vis, SHELL_BUF_NAME, false, FT_TEXT );

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

void InitColonBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m.files in Add_FileBuf_2_Lists_Create_Views()
  // Editor command buffer, COLON_FILE(5)
  m.colon_file = new(__FILE__,__LINE__) FileBuf( m.vis
                                               , COLON_BUF_NAME
                                               , true
                                               , FT_TEXT );

  m.colon_view = new(__FILE__,__LINE__) LineView( m.vis
                                                , m.key
                                                , *m.colon_file
                                                , m.reg
                                                , m.cbuf
                                                , m.sbuf
                                                , ':' );
  m.colon_file->AddView( m.colon_view );
}

void InitSlashBuffer( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m.files in Add_FileBuf_2_Lists_Create_Views()
  // Editor command buffer, SLASH_FILE(6)
  m.slash_file = new(__FILE__,__LINE__) FileBuf( m.vis
                                               , SLASH_BUF_NAME
                                               , true
                                               , FT_TEXT );
  // Add an empty line
  m.slash_file->PushLine();

  m.slash_view = new(__FILE__,__LINE__) LineView( m.vis
                                                , m.key
                                                , *m.slash_file
                                                , m.reg
                                                , m.cbuf
                                                , m.sbuf
                                                , '/' );
  m.slash_file->AddView( m.slash_view );
}

void InitUserFiles_AddFile( Vis::Data& m, const char* relative_name )
{
  String file_name( relative_name );

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
bool InitUserFiles( Vis::Data& m, const int ARGC, const char* const ARGV[] )
{
  bool run_diff = false;

  if( ARGC<2 )
  {
    // If user does not supply arguments, open current directory:
    InitUserFiles_AddFile( m, "." );
  }
  else {
    // User file buffers, 6, 7, ...
    for( int k=1; k<ARGC; k++ )
    {
      if( strcmp( "-d", ARGV[k] ) == 0 )
      {
        run_diff = true;
      }
      else {
        InitUserFiles_AddFile( m, ARGV[k] );
      }
    }
  }
  return run_diff;
}

void InitFileHistory( Vis::Data& m )
{
  for( int w=0; w<MAX_WINS; w++ )
  {
    m.file_hist[w].push( BE_FILE );
    m.file_hist[w].push( HELP_FILE );

    if( USER_FILE<m.views[w].len() )
    {
      m.file_hist[w].insert( 0, USER_FILE );

      for( int f=m.views[w].len()-1; (USER_FILE+1)<=f; f-- )
      {
        m.file_hist[w].push( f );
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

      m.file_hist[m.win].insert( 0, buf_idx );

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
    m.file_hist[m.win].insert( 0, view_index_new );

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
   || CVI == COLON_FILE
   || CVI == SLASH_FILE )
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

bool FName_2_FNum( Vis::Data& m, const String& full_fname, unsigned& file_num )
{
  bool found = false;

  for( unsigned k=0; !found && k<m.files.len(); k++ )
  {
    if( full_fname == m.files[ k ]->GetPathName() )
    {
      found = true;
      file_num = k;
    }
  }
  return found;
}

bool WentBackToPrevDirDiff( Vis::Data& m )
{
  bool went_back = false;
  View* pV = CV(m);
  View* const pDiff_vS = m.diff.GetViewShort();
  View* const pDiff_vL = m.diff.GetViewLong ();
  View* cV = (pV == pDiff_vS) ? pDiff_vS : pDiff_vL; // Current view
  View* oV = (pV == pDiff_vS) ? pDiff_vL : pDiff_vS; // Other   view
  // Get m_win for cV and oV
  const int c_win = GetWinNum_Of_View( m, cV );
  const int o_win = GetWinNum_Of_View( m, oV );
  View* cV_prev = GetView_WinPrev( m, c_win, 1 );
  View* oV_prev = GetView_WinPrev( m, o_win, 1 );

  if( 0 != cV_prev && 0 != oV_prev )
  {
    if( cV_prev->GetFB()->IsDir() && oV_prev->GetFB()->IsDir() )
    {
      Line l_cV_prev = cV_prev->GetFB()->GetLine( cV_prev->CrsLine() );
      Line l_oV_prev = oV_prev->GetFB()->GetLine( oV_prev->CrsLine() );

      if( l_cV_prev == l_oV_prev )
      { // Previous file one both sides were directories, and cursor was
        // on same file name on both sides, so go back to previous diff:
        unsigned c_file_idx = 0;
        unsigned o_file_idx = 0;

        if( FName_2_FNum( m, cV_prev->GetFB()->GetPathName(), c_file_idx )
         && FName_2_FNum( m, oV_prev->GetFB()->GetPathName(), o_file_idx ) )
        {
          // Move view indexes at front to back of m.file_hist
          unsigned c_view_index_old = m.file_hist[ c_win ].remove( 0 );
          unsigned o_view_index_old = m.file_hist[ o_win ].remove( 0 );
          m.file_hist[ c_win ].push( c_view_index_old );
          m.file_hist[ o_win ].push( o_view_index_old );

          went_back = m.diff.Run( cV_prev, oV_prev );

          if( went_back ) {
            m.diff_mode = true;
            m.diff.GetViewShort()->SetInDiff( true );
            m.diff.GetViewLong() ->SetInDiff( true );
          }
        }
      }
    }
  }
  return went_back;
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
    bool went_back_to_prev_dir_diff = false;

    if( CV(m)->GetInDiff() )
    {
      went_back_to_prev_dir_diff = WentBackToPrevDirDiff(m);

      if( !went_back_to_prev_dir_diff ) m.vis.NoDiff();
    }
    if( !went_back_to_prev_dir_diff )
    {
      View*    const pV_old = CV(m);
      Tile_Pos const tp_old = pV_old->GetTilePos();

      // Move view index at front to back of m.file_hist
      unsigned view_index_old = 0;
      m.file_hist[m.win].remove( 0, view_index_old );
      m.file_hist[m.win].push( view_index_old );

      // Redisplay current window with new view:
      CV(m)->SetTilePos( tp_old );
      CV(m)->Update();
    }
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

  GoToBuffer( m, SLASH_FILE );
}

void GoToNextWindow( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < m.num_wins )
  {
    const unsigned win_old = m.win;

    m.win = (++m.win) % m.num_wins;

    View* pV     = GetView_Win( m, m.win );
    View* pV_old = GetView_Win( m, win_old );

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

  const View*    curr_V  = GetView_Win( m, m.win );
  const Tile_Pos curr_TP = curr_V->GetTilePos();

  if( curr_TP == TP_LEFT_HALF )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP
       || TP_BOT__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_LEFT_CTR__QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_RITE_QTR      == TP
       || TP_BOT__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      View* pV     = GetView_Win( m, m.win );
      View* pV_old = GetView_Win( m, win_old );

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

  const View*    curr_V  = GetView_Win( m, m.win );
  const Tile_Pos curr_TP = curr_V->GetTilePos();

  if( curr_TP == TP_LEFT_HALF )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_RITE_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_LEFT_CTR__QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP
       || TP_BOT__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_CTR__QTR )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_LEFT_QTR      == TP
       || TP_BOT__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_RITE_CTR__QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      View* pV     = GetView_Win( m, m.win   );
      View* pV_old = GetView_Win( m, win_old );

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

  const View*    curr_V  = GetView_Win( m, m.win );
  const Tile_Pos curr_TP = curr_V->GetTilePos();

  if( curr_TP == TP_TOP__HALF )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_BOT__HALF     == TP
       || TP_BOT__LEFT_QTR == TP
       || TP_BOT__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_BOT__HALF     == TP
       || TP_BOT__RITE_QTR == TP
       || TP_BOT__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_BOT__HALF         == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_BOT__HALF         == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_TOP__HALF     == TP
       || TP_TOP__LEFT_QTR == TP
       || TP_TOP__LEFT_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_TOP__HALF     == TP
       || TP_TOP__RITE_QTR == TP
       || TP_TOP__RITE_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
      if( TP_TOP__HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { m.win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m.num_wins; k++ )
    {
      const Tile_Pos TP = GetView_Win( m, k )->GetTilePos();
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
      View* pV     = GetView_Win( m, m.win   );
      View* pV_old = GetView_Win( m, win_old );

      pV_old->Print_Borders();
      pV    ->Print_Borders();

      Console::Update();

      m.diff_mode ? m.diff.PrintCursor( pV ) : pV->PrintCursor();
    }
  }
}

//void FlipWindows( Vis::Data& m )
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//  if( 1 < m.num_wins )
//  {
//    // This code only works for MAX_WINS == 2
//    View* pV1 = GetView_Win( m, 0 );
//    View* pV2 = GetView_Win( m, 1 );
//
//    if( pV1 != pV2 )
//    {
//      // Swap pV1 and pV2 Tile Positions:
//      Tile_Pos tp_v1 = pV1->GetTilePos();
//      pV1->SetTilePos( pV2->GetTilePos() );
//      pV2->SetTilePos( tp_v1 );
//    }
//    m.vis.UpdateAll();
//  }
//}

Tile_Pos FlipWindows_Horizontally( const Tile_Pos OTP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Tile_Pos NTP = TP_NONE;

  if     ( OTP == TP_LEFT_HALF         ) NTP = TP_RITE_HALF        ;
  else if( OTP == TP_RITE_HALF         ) NTP = TP_LEFT_HALF        ;
  else if( OTP == TP_TOP__LEFT_QTR     ) NTP = TP_TOP__RITE_QTR    ;
  else if( OTP == TP_TOP__RITE_QTR     ) NTP = TP_TOP__LEFT_QTR    ;
  else if( OTP == TP_BOT__LEFT_QTR     ) NTP = TP_BOT__RITE_QTR    ;
  else if( OTP == TP_BOT__RITE_QTR     ) NTP = TP_BOT__LEFT_QTR    ;
  else if( OTP == TP_LEFT_QTR          ) NTP = TP_RITE_QTR         ;
  else if( OTP == TP_RITE_QTR          ) NTP = TP_LEFT_QTR         ;
  else if( OTP == TP_LEFT_CTR__QTR     ) NTP = TP_RITE_CTR__QTR    ;
  else if( OTP == TP_RITE_CTR__QTR     ) NTP = TP_LEFT_CTR__QTR    ;
  else if( OTP == TP_TOP__LEFT_8TH     ) NTP = TP_TOP__RITE_8TH    ;
  else if( OTP == TP_TOP__RITE_8TH     ) NTP = TP_TOP__LEFT_8TH    ;
  else if( OTP == TP_TOP__LEFT_CTR_8TH ) NTP = TP_TOP__RITE_CTR_8TH;
  else if( OTP == TP_TOP__RITE_CTR_8TH ) NTP = TP_TOP__LEFT_CTR_8TH;
  else if( OTP == TP_BOT__LEFT_8TH     ) NTP = TP_BOT__RITE_8TH    ;
  else if( OTP == TP_BOT__RITE_8TH     ) NTP = TP_BOT__LEFT_8TH    ;
  else if( OTP == TP_BOT__LEFT_CTR_8TH ) NTP = TP_BOT__RITE_CTR_8TH;
  else if( OTP == TP_BOT__RITE_CTR_8TH ) NTP = TP_BOT__LEFT_CTR_8TH;

  return NTP;
}

Tile_Pos FlipWindows_Vertically( const Tile_Pos OTP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Tile_Pos NTP = TP_NONE;

  if     ( OTP == TP_TOP__HALF         ) NTP = TP_BOT__HALF        ;
  else if( OTP == TP_BOT__HALF         ) NTP = TP_TOP__HALF        ;
  else if( OTP == TP_TOP__LEFT_QTR     ) NTP = TP_BOT__LEFT_QTR    ;
  else if( OTP == TP_TOP__RITE_QTR     ) NTP = TP_BOT__RITE_QTR    ;
  else if( OTP == TP_BOT__LEFT_QTR     ) NTP = TP_TOP__LEFT_QTR    ;
  else if( OTP == TP_BOT__RITE_QTR     ) NTP = TP_TOP__RITE_QTR    ;
  else if( OTP == TP_TOP__LEFT_8TH     ) NTP = TP_BOT__LEFT_8TH    ;
  else if( OTP == TP_TOP__RITE_8TH     ) NTP = TP_BOT__RITE_8TH    ;
  else if( OTP == TP_TOP__LEFT_CTR_8TH ) NTP = TP_BOT__LEFT_CTR_8TH;
  else if( OTP == TP_TOP__RITE_CTR_8TH ) NTP = TP_BOT__RITE_CTR_8TH;
  else if( OTP == TP_BOT__LEFT_8TH     ) NTP = TP_TOP__LEFT_8TH    ;
  else if( OTP == TP_BOT__RITE_8TH     ) NTP = TP_TOP__RITE_8TH    ;
  else if( OTP == TP_BOT__LEFT_CTR_8TH ) NTP = TP_TOP__LEFT_CTR_8TH;
  else if( OTP == TP_BOT__RITE_CTR_8TH ) NTP = TP_TOP__RITE_CTR_8TH;

  return NTP;
}

void FlipWindows( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < m.num_wins )
  {
    bool split_horizontally = false;

    for( unsigned k=0; !split_horizontally && k<m.num_wins; k++ )
    {
      // pV is View of displayed window k
      View* pV = GetView_Win( m, k );

      split_horizontally = pV->GetTilePos() == TP_TOP__HALF
                        || pV->GetTilePos() == TP_BOT__HALF;
    }
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      // pV is View of displayed window k
            View*    pV  = GetView_Win( m, k );
      const Tile_Pos OTP = pV->GetTilePos(); // Old tile position

      // New tile position:
      const Tile_Pos NTP = split_horizontally
                         ? FlipWindows_Vertically( OTP )
                         : FlipWindows_Horizontally( OTP );
      if( NTP != TP_NONE )
      {
        pV->SetTilePos( NTP );
      }
    }
    m.vis.UpdateAll( false );
  }
}

bool Have_TP_BOT__HALF( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = GetView_Win( m, k );
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_BOT__HALF ) return true;
  }
  return false;
}

bool Have_TP_TOP__HALF( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = GetView_Win( m, k );
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_TOP__HALF ) return true;
  }
  return false;
}

bool Have_TP_BOT__LEFT_QTR( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = GetView_Win( m, k );
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_BOT__LEFT_QTR ) return true;
  }
  return false;
}

bool Have_TP_TOP__LEFT_QTR( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = GetView_Win( m, k );
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_TOP__LEFT_QTR ) return true;
  }
  return false;
}

bool Have_TP_BOT__RITE_QTR( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = GetView_Win( m, k );
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_BOT__RITE_QTR ) return true;
  }
  return false;
}

bool Have_TP_TOP__RITE_QTR( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = GetView_Win( m, k );
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_TOP__RITE_QTR ) return true;
  }
  return false;
}

void Quit_JoinTiles_TP_LEFT_HALF( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = GetView_Win( m, k );
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
    View* v = GetView_Win( m, k );
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
    View* v = GetView_Win( m, k );
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
    View* v = GetView_Win( m, k );
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
      View* v = GetView_Win( m, k );
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_TOP__RITE_QTR     ) { v->SetTilePos( TP_TOP__HALF ); break; }
      else if( TP == TP_TOP__RITE_8TH     ) v->SetTilePos( TP_TOP__RITE_QTR );
      else if( TP == TP_TOP__RITE_CTR_8TH ) v->SetTilePos( TP_TOP__LEFT_QTR );
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = GetView_Win( m, k );
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
      View* v = GetView_Win( m, k );
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_TOP__LEFT_QTR     ) { v->SetTilePos( TP_TOP__HALF ); break; }
      else if( TP == TP_TOP__LEFT_8TH     ) v->SetTilePos( TP_TOP__LEFT_QTR );
      else if( TP == TP_TOP__LEFT_CTR_8TH ) v->SetTilePos( TP_TOP__RITE_QTR );
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = GetView_Win( m, k );
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
      View* v = GetView_Win( m, k );
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_BOT__RITE_QTR     ) { v->SetTilePos( TP_BOT__HALF ); break; }
      else if( TP == TP_BOT__RITE_8TH     ) v->SetTilePos( TP_BOT__RITE_QTR );
      else if( TP == TP_BOT__RITE_CTR_8TH ) v->SetTilePos( TP_BOT__LEFT_QTR );
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = GetView_Win( m, k );
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
      View* v = GetView_Win( m, k );
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_BOT__LEFT_QTR     ) { v->SetTilePos( TP_BOT__HALF ); break; }
      else if( TP == TP_BOT__LEFT_8TH     ) v->SetTilePos( TP_BOT__LEFT_QTR );
      else if( TP == TP_BOT__LEFT_CTR_8TH ) v->SetTilePos( TP_BOT__RITE_QTR );
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = GetView_Win( m, k );
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
    View* v = GetView_Win( m, k );
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
    View* v = GetView_Win( m, k );
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
    View* v = GetView_Win( m, k );
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
    View* v = GetView_Win( m, k );
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
      View* v = GetView_Win( m, k );
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__LEFT_CTR_8TH ) { v->SetTilePos( TP_TOP__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = GetView_Win( m, k );
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
      View* v = GetView_Win( m, k );
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__RITE_CTR_8TH ) { v->SetTilePos( TP_TOP__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = GetView_Win( m, k );
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
      View* v = GetView_Win( m, k );
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__LEFT_8TH ) { v->SetTilePos( TP_TOP__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = GetView_Win( m, k );
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
      View* v = GetView_Win( m, k );
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__RITE_8TH ) { v->SetTilePos( TP_TOP__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = GetView_Win( m, k );
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
      View* v = GetView_Win( m, k );
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__LEFT_CTR_8TH ) { v->SetTilePos( TP_BOT__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = GetView_Win( m, k );
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
      View* v = GetView_Win( m, k );
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__RITE_CTR_8TH ) { v->SetTilePos( TP_BOT__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = GetView_Win( m, k );
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
      View* v = GetView_Win( m, k );
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__LEFT_8TH ) { v->SetTilePos( TP_BOT__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = GetView_Win( m, k );
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
      View* v = GetView_Win( m, k );
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__RITE_8TH ) { v->SetTilePos( TP_BOT__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m.num_wins; k++ )
    {
      View* v = GetView_Win( m, k );
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__RITE_CTR_8TH ) { v->SetTilePos( TP_RITE_CTR__QTR ); break; }
    }
  }
}

void Quit_JoinTiles_TP_LEFT_THIRD( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = GetView_Win( m, k );
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_CTR__THIRD      ) v->SetTilePos( TP_LEFT_TWO_THIRDS );
    else if( TP == TP_RITE_TWO_THIRDS ) v->SetTilePos( TP_FULL );
  }
}

void Quit_JoinTiles_TP_CTR__THIRD( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = GetView_Win( m, k );
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_RITE_THIRD ) v->SetTilePos( TP_RITE_TWO_THIRDS );
  }
}

void Quit_JoinTiles_TP_RITE_THIRD( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = GetView_Win( m, k );
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_CTR__THIRD      ) v->SetTilePos( TP_RITE_TWO_THIRDS );
    else if( TP == TP_LEFT_TWO_THIRDS ) v->SetTilePos( TP_FULL );
  }
}

void Quit_JoinTiles_TP_LEFT_TWO_THIRDS( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = GetView_Win( m, k );
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_RITE_THIRD ) v->SetTilePos( TP_FULL );
  }
}

void Quit_JoinTiles_TP_RITE_TWO_THIRDS( Vis::Data& m )
{
  for( unsigned k=0; k<m.num_wins; k++ )
  {
    View* v = GetView_Win( m, k );
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_LEFT_THIRD ) v->SetTilePos( TP_FULL );
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
  else if( TP == TP_BOT__RITE_CTR_8TH ) Quit_JoinTiles_TP_BOT__RITE_CTR_8TH(m);
  else if( TP == TP_LEFT_THIRD )        Quit_JoinTiles_TP_LEFT_THIRD(m);
  else if( TP == TP_CTR__THIRD )        Quit_JoinTiles_TP_CTR__THIRD(m);
  else if( TP == TP_RITE_THIRD )        Quit_JoinTiles_TP_RITE_THIRD(m);
  else if( TP == TP_LEFT_TWO_THIRDS )   Quit_JoinTiles_TP_LEFT_TWO_THIRDS(m);
  else if( TP == TP_RITE_TWO_THIRDS )   Quit_JoinTiles_TP_RITE_TWO_THIRDS(m);
}

void Quit_ShiftDown( Vis::Data& m )
{
  // Make copy of win's list of views and view history:
  ViewList win_views    ( m.views    [m.win] );
   unsList win_file_hist( m.file_hist[m.win] );

  // Shift everything down
  for( unsigned w=m.win+1; w<m.num_wins; w++ )
  {
    m.views    [w-1] = m.views    [w];
    m.file_hist[w-1] = m.file_hist[w];
  }
  // Put win's list of views at end of views:
  // Put win's view history at end of view historys:
  m.views    [m.num_wins-1] = win_views;
  m.file_hist[m.num_wins-1] = win_file_hist;
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

  if( m.num_wins <= 1 ) QuitAll(m);
  else {
    View* cv = CV(m);

    const Tile_Pos TP = CV(m)->GetTilePos();

    if( cv->GetInDiff() )
    {
      m.diff.GetViewShort()->SetInDiff( false );
      m.diff.GetViewLong() ->SetInDiff( false );
      m.diff.Set_Remaining_ViewContext_2_DiffContext();
      m.diff_mode = false;
    }
    if( m.win < m.num_wins-1 )
    {
      Quit_ShiftDown(m);
    }
    if( 0 < m.win ) m.win--;
    m.num_wins--;

    Quit_JoinTiles( m, TP );

    m.vis.UpdateAll( false );

    CV(m)->PrintCursor();
  }
}

void MapStart( Vis::Data& m )
{
  m.key.map_buf.clear();
  m.key.save_2_map_buf = true;

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.DisplayMapping();
  else                     cv->DisplayMapping();
}

void MapEnd( Vis::Data& m )
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

void MapShow( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);
  const unsigned ROW = cv->Cmd__Line_Row();
  const unsigned ST  = cv->Col_Win_2_GL( 0 );
  const unsigned WC  = cv->WorkingCols();
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

  if( cv->GetInDiff() ) m.diff.PrintCursor( cv );
  else                     cv->PrintCursor();
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
      m.file_hist[win_idx].insert( 0, buf_idx );

      // Remove subsequent buf_idx's from m.file_hist[win_idx]:
      for( unsigned k=1; k<m.file_hist[win_idx].len(); k++ )
      {
        if( buf_idx == m.file_hist[win_idx][k] ) m.file_hist[win_idx].remove( k );
      }
      View* pV_curr = GetView_WinPrev( m, win_idx, 0 );
      View* pV_prev = GetView_WinPrev( m, win_idx, 1 );

                   pV_curr->SetTilePos( pV_prev->GetTilePos() );
      if( update ) pV_curr->Update();
    }
  }
}

// If file is found, puts View of file in win_idx window,
// and returns the View, else returns null
View* Diff_CheckPossibleFile( Vis::Data& m
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

    return GetView_Win( m, win_idx );
  }
  return 0;
}

View* Diff_FindRegFileView( Vis::Data& m
                          , const FileBuf* pfb_reg
                          , const FileBuf* pfb_dir
                          , const unsigned win_idx
                          ,       View*    pv )
{
  String possible_fname = pfb_dir->GetPathName();
  String fname_extension;

  gArray_t<String*> path_parts;
  Create_Path_Parts_List( pfb_reg->GetPathName(), path_parts );

  for( int k=path_parts.len()-1; 0<=k; k-- )
  {
    // Revert back to pfb_dir.m_fname:
    possible_fname = pfb_dir->GetPathName();

    if( 0<fname_extension.len()
     && fname_extension.get_end(0)!=DIR_DELIM )
    {
      fname_extension.insert( 0, DIR_DELIM );
    }
    fname_extension.insert( 0, path_parts[k]->c_str() );

    possible_fname.append( fname_extension );

    const char* pos_fname = possible_fname.c_str();

    View* nv = Diff_CheckPossibleFile( m, win_idx, pos_fname );

    if( 0 != nv ) return nv;
  }
  return pv;
}

Tile_Pos DoDiff_Find_Matching_Tile_Pos( const Tile_Pos tp_c )
{
  Tile_Pos tp_m = TP_NONE; // Matching tile pos

  if     ( tp_c == TP_LEFT_HALF         ) tp_m = TP_RITE_HALF;
  else if( tp_c == TP_RITE_HALF         ) tp_m = TP_LEFT_HALF;
  else if( tp_c == TP_TOP__HALF         ) tp_m = TP_BOT__HALF;
  else if( tp_c == TP_BOT__HALF         ) tp_m = TP_TOP__HALF;
  else if( tp_c == TP_TOP__LEFT_QTR     ) tp_m = TP_TOP__RITE_QTR;
  else if( tp_c == TP_TOP__RITE_QTR     ) tp_m = TP_TOP__LEFT_QTR;
  else if( tp_c == TP_BOT__LEFT_QTR     ) tp_m = TP_BOT__RITE_QTR;
  else if( tp_c == TP_BOT__RITE_QTR     ) tp_m = TP_BOT__LEFT_QTR;
  else if( tp_c == TP_LEFT_QTR          ) tp_m = TP_LEFT_CTR__QTR;
  else if( tp_c == TP_LEFT_CTR__QTR     ) tp_m = TP_LEFT_QTR;
  else if( tp_c == TP_RITE_CTR__QTR     ) tp_m = TP_RITE_QTR;
  else if( tp_c == TP_RITE_QTR          ) tp_m = TP_RITE_CTR__QTR;
  else if( tp_c == TP_TOP__LEFT_8TH     ) tp_m = TP_TOP__LEFT_CTR_8TH;
  else if( tp_c == TP_TOP__LEFT_CTR_8TH ) tp_m = TP_TOP__LEFT_8TH;
  else if( tp_c == TP_TOP__RITE_CTR_8TH ) tp_m = TP_TOP__RITE_8TH;
  else if( tp_c == TP_TOP__RITE_8TH     ) tp_m = TP_TOP__RITE_CTR_8TH;
  else if( tp_c == TP_BOT__LEFT_8TH     ) tp_m = TP_BOT__LEFT_CTR_8TH;
  else if( tp_c == TP_BOT__LEFT_CTR_8TH ) tp_m = TP_BOT__LEFT_8TH;
  else if( tp_c == TP_BOT__RITE_CTR_8TH ) tp_m = TP_BOT__RITE_8TH;
  else if( tp_c == TP_BOT__RITE_8TH     ) tp_m = TP_BOT__RITE_CTR_8TH;

  return tp_m;
}

int DoDiff_Find_Win_2_Diff( Vis::Data& m )
{
  int diff_win_num = -1; // Failure value

  // Must be not already doing a diff and at least 2 buffers to do diff:
  if( !m.diff_mode && 2 <= m.num_wins )
  {
    View*     v_c = GetView_Win( m, m.win ); // Current View
    Tile_Pos tp_c = v_c->GetTilePos();       // Current Tile_Pos

    // tp_m = matching Tile_Pos to tp_c
    const Tile_Pos tp_m = DoDiff_Find_Matching_Tile_Pos( tp_c );

    if( TP_NONE != tp_m )
    {
      // See if one of the other views is in tp_m
      for( unsigned k=0; -1 == diff_win_num && k<m.num_wins; k++ )
      {
        if( k != m.win )
        {
          View* v_k = GetView_Win( m, k );
          if( tp_m == v_k->GetTilePos() )
          {
            diff_win_num = k;
          }
        }
      }
    }
  }
  return diff_win_num;
}

// Execute user diff command
void Diff_Files_Displayed( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const int d_win = DoDiff_Find_Win_2_Diff(m); // Diff win number
  if( 0 <= d_win )
  {
    View* pv0 = GetView_Win( m, m.win );
    View* pv1 = GetView_Win( m, d_win );
    FileBuf* pfb0 = pv0->GetFB();
    FileBuf* pfb1 = pv1->GetFB();

    // New code in progress:
    bool ok = true;
    if( !pfb0->IsDir() && pfb1->IsDir() )
    {
      pv1 = Diff_FindRegFileView( m, pfb0, pfb1, 1, pv1 );
    }
    else if( pfb0->IsDir() && !pfb1->IsDir() )
    {
      pv0 = Diff_FindRegFileView( m, pfb1, pfb0, 0, pv0 );
    }
    else {
      if( ( strcmp( SHELL_BUF_NAME, pfb0->GetFileName() )
         && !FileExists( pfb0->GetPathName() ) ) )
      {
        ok = false;
        m.vis.Window_Message("\n%s does not exist\n\n", pfb0->GetFileName());
      }
      if( ( strcmp( SHELL_BUF_NAME, pfb1->GetFileName() )
         && !FileExists( pfb1->GetPathName() ) ) )
      {
        ok = false;
        m.vis.Window_Message("\n%s does not exist\n\n", pfb1->GetFileName());
      }
    }
    if( ok ) {
      bool ok = m.diff.Run( pv0, pv1 );
      if( ok ) {
        m.diff_mode = true;
        m.diff.GetViewShort()->SetInDiff( true );
        m.diff.GetViewLong()->SetInDiff( true );
      }
    }
  }
}

void ReDiff( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( true == m.diff_mode )
  {
    m.diff.ReDiff();
  }
}

void VSplitWindow( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.vis.NoDiff();

  View* cv = CV(m);
  const Tile_Pos cv_tp = cv->GetTilePos();

  // Make sure current view can be vertically split:
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

    // New window will be m.num_wins.
    // Duplicate file hist of current window into new window.
    m.file_hist[m.num_wins] = m.file_hist[m.win];

    // Copy current view context into new view
    View* nv = GetView_Win( m, m.num_wins );

    nv->Set_Context( *cv );

    // Make new window the current window:
    m.win = m.num_wins;
    m.num_wins++;

    // Set the new tile positions of the old view cv, and the new view nv:
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
  else if( m.num_wins+1 < MAX_WINS
        && ( cv_tp == TP_LEFT_TWO_THIRDS
          || cv_tp == TP_RITE_TWO_THIRDS ) )
  {
    m.file_hist[m.num_wins] = m.file_hist[m.win];

    // Copy current view context into new view
    View* nv = GetView_Win( m, m.num_wins );

    nv->Set_Context( *cv );

    // Make new window the current window:
    m.num_wins += 1;

    // Set the new tile positions.
    if( cv_tp == TP_LEFT_TWO_THIRDS )
    {
      cv->SetTilePos( TP_LEFT_THIRD );
      nv->SetTilePos( TP_CTR__THIRD );
    }
    else //( cv_tp == TP_RITE_TWO_THIRDS )
    {
      cv->SetTilePos( TP_CTR__THIRD );
      nv->SetTilePos( TP_RITE_THIRD );
    }
  }
  m.vis.UpdateAll( false );
}

void HSplitWindow( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.vis.NoDiff();

  View* cv = CV(m);
  const Tile_Pos cv_tp = cv->GetTilePos();

  // Make sure current view can be horizontally split:
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

    // New window will be m.num_wins.
    // Duplicate file hist of current window into new window.
    m.file_hist[m.num_wins] = m.file_hist[m.win];

    // Copy current view context into new view
    View* nv = GetView_Win( m, m.num_wins );

    nv->Set_Context( *cv );

    // Make new window the current window:
    m.win = m.num_wins;
    m.num_wins++;

    // Set the new tile positions of the old view cv, and the new view nv:
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
  m.vis.UpdateAll( false );
}

void _3SplitWindow( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.vis.NoDiff();

  View* cv = CV(m);
  const Tile_Pos cv_tp = cv->GetTilePos();

  // Make sure current view can be 3 split:
  if( m.num_wins+1 < MAX_WINS
   && ( cv_tp == TP_FULL ) )
  {
    ASSERT( __LINE__, m.win < m.num_wins, "m.win < m.num_wins" );

    // New windows will be m.num_wins, and m.num_wins+1.
    // Duplicate file hist of current window into new windows.
    m.file_hist[m.num_wins]   = m.file_hist[m.win];
    m.file_hist[m.num_wins+1] = m.file_hist[m.win];

    // Copy current view context into new views
    View* nv1 = GetView_Win( m, m.num_wins );
    View* nv2 = GetView_Win( m, m.num_wins+1 );

    nv1->Set_Context( *cv );
    nv2->Set_Context( *cv );

    // Current window, does not change, but there are 2 new windows:
    m.num_wins += 2;

    // Set the new tile positions.
    cv ->SetTilePos( TP_LEFT_THIRD );
    nv1->SetTilePos( TP_CTR__THIRD );
    nv2->SetTilePos( TP_RITE_THIRD );
  }
  m.vis.UpdateAll( false );
}

void ReHighlight_CV( Vis::Data& m )
{
  View* cv = CV(m);

  FileBuf* pfb = cv->GetFB();

  pfb->ClearStyles();

  if( cv->GetInDiff() ) m.diff.Update();
  else                    pfb->Update();
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

void HandleColon_detab( Vis::Data& m )
{
  if( 6 < strlen( m.cbuf ) )
  {
    const unsigned tab_sz = atol( m.cbuf + 6 );
    if( 0 < tab_sz && tab_sz <= 32 )
    {
      CV(m)->GetFB()->RemoveTabs_SpacesAtEOLs( tab_sz );
    }
  }
}

void HandleColon_dos2unix( Vis::Data& m )
{
  CV(m)->GetFB()->dos2unix();
}

void HandleColon_unix2dos( Vis::Data& m )
{
  CV(m)->GetFB()->unix2dos();
}

void HandleColon_sort( Vis::Data& m )
{
  m.sort_by_time = !m.sort_by_time;

  FileBuf* pfb = m.views[0][ BE_FILE ]->GetFB();
  pfb->Sort();
  pfb->Update();

  if( m.sort_by_time )
  {
    m.vis.CmdLineMessage("Sorting files by focus time");
  }
  else {
    m.vis.CmdLineMessage("Sorting files by name");
  }
}

//void HandleColon_e( Vis::Data& m )
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  View* pV = CV(m);
//  if( 0 == m.cbuf[1] ) // :e
//  {
//    FileBuf* pfb = pV->GetFB();
//    pfb->ReReadFile();
//
//    for( unsigned w=0; w<m.num_wins; w++ )
//    {
//      if( pfb == GetView_Win( m, w )->GetFB() )
//      {
//        // View is currently displayed, perform needed update:
//        GetView_Win( m, w )->Update();
//      }
//    }
//  }
//  else // :e file_name
//  {
//    // Edit file of supplied file name:
//    String fname( m.cbuf + 1 );
//
//    if( FindFullFileNameRel2( pV->GetDirName(), fname ) )
//    {
//      unsigned file_index = 0;
//      if( m.vis.HaveFile( fname.c_str(), &file_index ) )
//      {
//        GoToBuffer( m, file_index );
//      }
//      else {
//        FileBuf* p_fb = new(__FILE__,__LINE__)
//                        FileBuf( m.vis, fname.c_str(), true, FT_UNKNOWN );
//        p_fb->ReadFile();
//        GoToBuffer( m, m.views[m.win].len()-1 );
//      }
//    }
//  }
//}

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
      if( pfb == GetView_Win( m, w )->GetFB() )
      {
        // View is currently displayed, perform needed update:
        GetView_Win( m, w )->Update();
      }
    }
  }
  else // :e file_name
  {
    // Edit file of supplied file name:
    String fname( m.cbuf + 1 );

    if( FindFullFileNameRel2( pV->GetDirName(), fname ) )
    {
      m.vis.NotHaveFileAddFile( fname );

      unsigned file_index = 0;

      if( m.vis.HaveFile( fname.c_str(), &file_index ) )
      {
        GoToBuffer( m, file_index );
      }
    }
  }
}

void HandleColon_w( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = CV(m);

  if(   0 == m.cbuf[1] // :w
   || ('q'== m.cbuf[1] && 0 == m.cbuf[2]) ) // :wq
  {
    if( pV == m.views[m.win][ SHELL_FILE ] )
    {
      // Dont allow SHELL_BUFFER to be saved with :w.
      // Require :w filename.
      pV->PrintCursor();
    }
    else {
      pV->GetFB()->Write();

      // If Write fails, current view will be message buffer, in which case
      // we dont want to PrintCursor with the original view, because that
      // would put the cursor in the wrong position:
      if( CV(m) == pV )
      {
        if( pV->GetInDiff() ) m.diff.PrintCursor( pV );
        else                     pV->PrintCursor();
      }
    }
    if( 'q'== m.cbuf[1] ) Quit(m);
  }
  else // :w file_name
  {
    // Write file of supplied file name:
    String fname( m.cbuf + 1 );

    if( FindFullFileNameRel2( pV->GetDirName(), fname ) )
    {
      unsigned file_index = 0;
      if( m.vis.HaveFile( fname.c_str(), &file_index ) )
      {
        m.vis.GetFileBuf( file_index )->Write();
      }
      else if( fname.get_end() != DIR_DELIM )
      {
        FileBuf* p_fb = new(__FILE__,__LINE__)
                        FileBuf( m.vis, fname.c_str(), *pV->GetFB() );
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

void MoveToLine( Vis::Data& m )
{
  // Move cursor to line:
  const unsigned line_num = atol( m.cbuf );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToLine( line_num );
  else                     cv->GoToLine( line_num );
}

void Handle_Colon_Cmd( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  RemoveSpaces( m.cbuf );
  MapEnd(m);

  if     ( strcmp( m.cbuf,"q"   )==0 ) Quit(m);
  else if( strcmp( m.cbuf,"qa"  )==0 ) QuitAll(m);
  else if( strcmp( m.cbuf,"help")==0 ) Help(m);
  else if( strcmp( m.cbuf,"diff")==0 ) Diff_Files_Displayed(m);
  else if( strcmp( m.cbuf,"rediff")==0) ReDiff(m);
  else if( strcmp( m.cbuf,"nodiff")==0)m.vis.NoDiff();
  else if( strcmp( m.cbuf,"n"   )==0 ) GoToNextBuffer(m);
  else if( strcmp( m.cbuf,"se"  )==0 ) GoToSearchBuffer(m);
  else if( strcmp( m.cbuf,"vsp" )==0 ) VSplitWindow(m);
  else if( strcmp( m.cbuf,"sp"  )==0 ) HSplitWindow(m);
  else if( strcmp( m.cbuf,"3sp" )==0 ) _3SplitWindow(m);
  else if( strcmp( m.cbuf,"cs1" )==0 ) { Console::Set_Color_Scheme_1(); }
  else if( strcmp( m.cbuf,"cs2" )==0 ) { Console::Set_Color_Scheme_2(); }
  else if( strcmp( m.cbuf,"cs3" )==0 ) { Console::Set_Color_Scheme_3(); }
  else if( strcmp( m.cbuf,"cs4" )==0 ) { Console::Set_Color_Scheme_4(); }
  else if( strcmp( m.cbuf,"hi"  )==0 ) ReHighlight_CV(m);
  else if( strncmp(m.cbuf,"cd",2)==0 ) { Ch_Dir(m); }
  else if( strncmp(m.cbuf,"syn",3)==0) { Set_Syntax(m); }
  else if( strcmp( m.cbuf,"pwd" )==0 ) { GetCWD(m); }
  else if( strcmp( m.cbuf,"sh"  )==0
        || strcmp( m.cbuf,"shell")==0) { GoToShellBuffer(m); }
  else if( strcmp( m.cbuf,"run" )==0 ) { RunCommand(m); }
  else if( strncmp(m.cbuf,"re",2)==0 ) { Console::Refresh(); }
  else if( strcmp( m.cbuf,"map" )==0 )    MapStart(m);
  else if( strcmp( m.cbuf,"showmap")==0)  MapShow(m);
  else if( strcmp( m.cbuf,"cover")==0)    m.colon_view->Cover();
  else if( strcmp( m.cbuf,"coverkey")==0) m.colon_view->CoverKey();
  else if( strncmp(m.cbuf,"detab=",6)==0) HandleColon_detab(m);
  else if( strcmp( m.cbuf,"dos2unix")==0) HandleColon_dos2unix(m);
  else if( strcmp( m.cbuf,"unix2dos")==0) HandleColon_unix2dos(m);
  else if( strcmp( m.cbuf,"sort")==0)     HandleColon_sort(m);
  else if( 'e' == m.cbuf[0] )             HandleColon_e(m);
  else if( 'w' == m.cbuf[0] )             HandleColon_w(m);
  else if( 'b' == m.cbuf[0] )             HandleColon_b(m);
  else if( '0' <= m.cbuf[0] && m.cbuf[0] <= '9' ) MoveToLine(m);
  else { // Put cursor back to line and column in edit window:
    if( m.diff_mode ) m.diff.PrintCursor( CV(m) );
    else              CV(m)->PrintCursor();
  }
}

void Handle_i( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.dot_buf_n.clear();
    m.key.dot_buf_n.push('i');
    m.key.save_2_dot_buf_n = true;
  }

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_i();
  else                     cv->Do_i();

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.save_2_dot_buf_n = false;
  }
}

void L_Handle_i( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_l )
  {
    m.key.dot_buf_l.clear();
    m.key.dot_buf_l.push('i');
    m.key.save_2_dot_buf_l = true;
  }
  if( m.colon_mode )
  {
    bool end_of_line_delim = m.colon_view->Do_i();

    if( end_of_line_delim )
    {
      m.colon_mode = false;

      Handle_Colon_Cmd( m );
    }
  }
  else if( m.slash_mode )
  {
    bool end_of_line_delim = m.slash_view->Do_i();

    if( end_of_line_delim )
    {
      m.slash_mode = false;

      String slash_pattern( m.cbuf );

      m.vis.Handle_Slash_GotPattern( slash_pattern );
    }
  }
  if( !m.key.get_from_dot_buf_l )
  {
    m.key.save_2_dot_buf_l = false;
  }
}

void Handle_v( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.vis_buf.clear();
    m.key.vis_buf.push('v');
    m.key.save_2_vis_buf = true;
  }
  View* cv = CV(m);

  const bool copy_vis_buf_2_dot_buf_n = cv->GetInDiff()
                                      ? m.diff.Do_v()
                                      :    cv->Do_v();
  if( !m.key.get_from_dot_buf_n )
  {
    m.key.save_2_vis_buf = false;

    if( copy_vis_buf_2_dot_buf_n )
    {
      m.key.dot_buf_n.copy( m.key.vis_buf );
    }
  }
}

void L_Handle_v( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_l )
  {
    m.key.vis_buf.clear();
    m.key.vis_buf.push('v');
    m.key.save_2_vis_buf = true;
  }
  bool copy_vis_buf_2_dot_buf_l = false;

  if     ( m.colon_mode ) copy_vis_buf_2_dot_buf_l = m.colon_view->Do_v();
  else if( m.slash_mode ) copy_vis_buf_2_dot_buf_l = m.slash_view->Do_v();

  if( !m.key.get_from_dot_buf_l )
  {
    m.key.save_2_vis_buf = false;

    if( copy_vis_buf_2_dot_buf_l )
    {
      m.key.dot_buf_l.copy( m.key.vis_buf );
    }
  }
}

void Handle_V( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.vis_buf.clear();
    m.key.vis_buf.push('V');
    m.key.save_2_vis_buf = true;
  }
  View* cv = CV(m);

  const bool copy_vis_buf_2_dot_buf_n = cv->GetInDiff()
                                      ? m.diff.Do_V()
                                      :    cv->Do_V();
  if( !m.key.get_from_dot_buf_n )
  {
    m.key.save_2_vis_buf = false;

    if( copy_vis_buf_2_dot_buf_n )
    {
      m.key.dot_buf_n.copy( m.key.vis_buf );
    }
  }
}

void Handle_a( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.dot_buf_n.clear();
    m.key.dot_buf_n.push('a');
    m.key.save_2_dot_buf_n = true;
  }

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_a();
  else                     cv->Do_a();

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.save_2_dot_buf_n = false;
  }
}

void L_Handle_a( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_l )
  {
    m.key.dot_buf_l.clear();
    m.key.dot_buf_l.push('a');
    m.key.save_2_dot_buf_l = true;
  }
  if( m.colon_mode )
  {
    bool end_of_line_delim = m.colon_view->Do_a();

    if( end_of_line_delim )
    {
      m.colon_mode = false;

      Handle_Colon_Cmd( m );
    }
  }
  else if( m.slash_mode )
  {
    bool end_of_line_delim = m.slash_view->Do_a();

    if( end_of_line_delim )
    {
      m.slash_mode = false;

      String slash_pattern( m.cbuf );

      m.vis.Handle_Slash_GotPattern( slash_pattern );
    }
  }
  if( !m.key.get_from_dot_buf_l )
  {
    m.key.save_2_dot_buf_l = false;
  }
}

void Handle_A( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.dot_buf_n.clear();
    m.key.dot_buf_n.push('A');
    m.key.save_2_dot_buf_n = true;
  }
  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_A();
  else                     cv->Do_A();

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.save_2_dot_buf_n = false;
  }
}

void L_Handle_A( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.colon_mode )
  {
    bool end_of_line_delim = m.colon_view->Do_A();

    if( end_of_line_delim )
    {
      m.colon_mode = false;

      Handle_Colon_Cmd( m );
    }
  }
  else if( m.slash_mode )
  {
    bool end_of_line_delim = m.slash_view->Do_A();

    if( end_of_line_delim )
    {
      m.slash_mode = false;

      String slash_pattern( m.cbuf );

      m.vis.Handle_Slash_GotPattern( slash_pattern );
    }
  }
}

void Handle_o( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.dot_buf_n.clear();
    m.key.dot_buf_n.push('o');
    m.key.save_2_dot_buf_n = true;
  }

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_o();
  else                     cv->Do_o();

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.save_2_dot_buf_n = false;
  }
}

void L_Handle_o( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.colon_mode )
  {
    bool end_of_line_delim = m.colon_view->Do_o();

    if( end_of_line_delim )
    {
      m.colon_mode = false;

      Handle_Colon_Cmd( m );
    }
  }
  else if( m.slash_mode )
  {
    bool end_of_line_delim = m.slash_view->Do_o();

    if( end_of_line_delim )
    {
      m.slash_mode = false;

      String slash_pattern( m.cbuf );

      m.vis.Handle_Slash_GotPattern( slash_pattern );
    }
  }
}

void Handle_O( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.dot_buf_n.clear();
    m.key.dot_buf_n.push('O');
    m.key.save_2_dot_buf_n = true;
  }

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_O();
  else                     cv->Do_O();

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.save_2_dot_buf_n = false;
  }
}

void Handle_x( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.dot_buf_n.clear();
    m.key.dot_buf_n.push('x');
  }
  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_x();
  else                     cv->Do_x();
}

void L_Handle_x( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_l )
  {
    m.key.dot_buf_l.clear();
    m.key.dot_buf_l.push('x');
  }
  if     ( m.colon_mode ) m.colon_view->Do_x();
  else if( m.slash_mode ) m.slash_view->Do_x();
}

void Handle_s( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.dot_buf_n.clear();
    m.key.dot_buf_n.push('s');
    m.key.save_2_dot_buf_n = true;
  }
  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_s();
  else                     cv->Do_s();

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.save_2_dot_buf_n = false;
  }
}

void L_Handle_s( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_l )
  {
    m.key.dot_buf_l.clear();
    m.key.dot_buf_l.push('s');
    m.key.save_2_dot_buf_l = true;
  }

  if     ( m.colon_mode ) m.colon_view->Do_s();
  else if( m.slash_mode ) m.slash_view->Do_s();

  if( !m.key.get_from_dot_buf_l )
  {
    m.key.save_2_dot_buf_l = false;
  }
}

void Handle_c( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char C = m.key.In();

  if( C == 'w' )
  {
    if( !m.key.get_from_dot_buf_n )
    {
      m.key.dot_buf_n.clear();
      m.key.dot_buf_n.push('c');
      m.key.dot_buf_n.push('w');
      m.key.save_2_dot_buf_n = true;
    }
    View* cv = CV(m);

    if( cv->GetInDiff() ) m.diff.Do_cw();
    else                     cv->Do_cw();

    if( !m.key.get_from_dot_buf_n )
    {
      m.key.save_2_dot_buf_n = false;
    }
  }
  else if( C == '$' )
  {
    if( !m.key.get_from_dot_buf_n )
    {
      m.key.dot_buf_n.clear();
      m.key.dot_buf_n.push('c');
      m.key.dot_buf_n.push('$');
      m.key.save_2_dot_buf_n = true;
    }
    View* cv = CV(m);

    if( cv->GetInDiff() ) { m.diff.Do_D(); m.diff.Do_a(); }
    else                  {    cv->Do_D();    cv->Do_a(); }

    if( !m.key.get_from_dot_buf_n )
    {
      m.key.save_2_dot_buf_n = false;
    }
  }
}

void L_Handle_c( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char C = m.key.In();

  if( C == 'w' )
  {
    if( !m.key.get_from_dot_buf_l )
    {
      m.key.dot_buf_l.clear();
      m.key.dot_buf_l.push('c');
      m.key.dot_buf_l.push('w');
      m.key.save_2_dot_buf_l = true;
    }
    if     ( m.colon_mode ) m.colon_view->Do_cw();
    else if( m.slash_mode ) m.slash_view->Do_cw();

    if( !m.key.get_from_dot_buf_l )
    {
      m.key.save_2_dot_buf_l = false;
    }
  }
  else if( C == '$' )
  {
    if( !m.key.get_from_dot_buf_l )
    {
      m.key.dot_buf_l.clear();
    }
    if( m.colon_mode )
    {
      m.colon_view->Do_D();
      m.colon_view->Do_a();
    }
    else if( m.slash_mode )
    {
      m.slash_view->Do_D();
      m.slash_view->Do_a();
    }
  }
}

void Handle_Dot( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<m.key.dot_buf_n.len() )
  {
    if( m.key.save_2_map_buf )
    {
      // Pop '.' off map_buf, because the contents of m.key.map_buf
      // will be saved to m.key.map_buf.
      m.key.map_buf.pop();
    }
    m.key.get_from_dot_buf_n = true;

    while( m.key.get_from_dot_buf_n )
    {
      const char CC = m.key.In();

      Vis::Data::CmdFunc cf = m.ViewFuncs[ CC ];
      if( cf ) (*cf)(m);
    }
    View* cv = CV(m);

    if( cv->GetInDiff() ) {
      // Diff does its own update every time a command is run
    }
    else {
      // Dont update until after all the commands have been executed:
      cv->GetFB()->Update();
    }
  }
}

void L_Handle_Dot( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<m.key.dot_buf_l.len() )
  {
    m.key.get_from_dot_buf_l = true;

    while( m.key.get_from_dot_buf_l )
    {
      const char CC = m.key.In();

      Vis::Data::CmdFunc cf = m.LineFuncs[ CC ];
      if( cf ) (*cf)(m);
    }
    if( CV(m)->GetInDiff() ) {
      // Diff does its own update every time a command is run
    }
    else {
      // Dont update until after all the commands have been executed:
      if     ( m.colon_mode ) m.colon_view->Update();
      else if( m.slash_mode ) m.slash_view->Update();
    }
  }
}

void Handle_j( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoDown( m.repeat );
  else                     cv->GoDown( m.repeat );
}

void L_Handle_j( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( m.colon_mode ) m.colon_view->GoDown();
  else if( m.slash_mode ) m.slash_view->GoDown();
}

void Handle_k( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoUp( m.repeat );
  else                     cv->GoUp( m.repeat );
}

void L_Handle_k( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( m.colon_mode ) m.colon_view->GoUp();
  else if( m.slash_mode ) m.slash_view->GoUp();
}

void Handle_h( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoLeft( m.repeat );
  else                     cv->GoLeft( m.repeat );
}

void L_Handle_h( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( m.colon_mode ) m.colon_view->GoLeft();
  else if( m.slash_mode ) m.slash_view->GoLeft();
}

void Handle_l( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoRight( m.repeat );
  else                     cv->GoRight( m.repeat );
}

void L_Handle_l( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( m.colon_mode ) m.colon_view->GoRight();
  else if( m.slash_mode ) m.slash_view->GoRight();
}

void Handle_H( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToTopLineInView();
  else                     cv->GoToTopLineInView();
}

void Handle_L( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToBotLineInView();
  else                     cv->GoToBotLineInView();
}

void Handle_M( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToMidLineInView();
  else                     cv->GoToMidLineInView();
}

void Handle_0( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToBegOfLine();
  else                     cv->GoToBegOfLine();
}

void L_Handle_0( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( m.colon_mode ) m.colon_view->GoToBegOfLine();
  else if( m.slash_mode ) m.slash_view->GoToBegOfLine();
}

void Handle_Q( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Handle_Dot(m);
  Handle_j(m);
  Handle_0(m);
}

void Handle_Dollar( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToEndOfLine();
  else                     cv->GoToEndOfLine();
}

void L_Handle_Dollar( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( m.colon_mode ) m.colon_view->GoToEndOfLine();
  else if( m.slash_mode ) m.slash_view->GoToEndOfLine();
}

//void Handle_Return( Vis::Data& m )
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  View* cv = CV(m);
//
//  if( cv->GetInDiff() ) m.diff.GoToBegOfNextLine();
//  else                     cv->GoToBegOfNextLine();
//}

void Handle_Return( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToBegOfNextLine();
  else {
    if( SLASH_FILE != Curr_FileNum(m) )
    {
      // Normal case:
      cv->GoToBegOfNextLine();
    }
    else {
      // In search buffer, search for pattern on current line:
      const Line* lp = cv->GetFB()->GetLineP( cv->CrsLine() );

      m.vis.Handle_Slash_GotPattern( lp->toString() );
    }
  }
}

void Handle_G( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToEndOfFile();
  else                     cv->GoToEndOfFile();
}

void L_Handle_G( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( m.colon_mode ) m.colon_view->GoToEndOfFile();
  else if( m.slash_mode ) m.slash_view->GoToEndOfFile();
}

void Handle_b( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToPrevWord();
  else                     cv->GoToPrevWord();
}

void L_Handle_b( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( m.colon_mode ) m.colon_view->GoToPrevWord();
  else if( m.slash_mode ) m.slash_view->GoToPrevWord();
}

void Handle_w( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToNextWord();
  else                     cv->GoToNextWord();
}

void L_Handle_w( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( m.colon_mode ) m.colon_view->GoToNextWord();
  else if( m.slash_mode ) m.slash_view->GoToNextWord();
}

void Handle_e( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToEndOfWord();
  else                     cv->GoToEndOfWord();
}

void L_Handle_e( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( m.colon_mode ) m.colon_view->GoToEndOfWord();
  else if( m.slash_mode ) m.slash_view->GoToEndOfWord();
}

void Handle_f( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.fast_char = m.key.In();

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_f( m.fast_char );
  else                     cv->Do_f( m.fast_char );
}

void L_Handle_f( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.fast_char = m.key.In();

  if     ( m.colon_mode ) m.colon_view->Do_f( m.fast_char );
  else if( m.slash_mode ) m.slash_view->Do_f( m.fast_char );
}

void Handle_SemiColon( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 <= m.fast_char )
  {
    View* cv = CV(m);

    if( cv->GetInDiff() ) m.diff.Do_f( m.fast_char );
    else                     cv->Do_f( m.fast_char );
  }
}

void L_Handle_SemiColon( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 <= m.fast_char )
  {
    if     ( m.colon_mode ) m.colon_view->Do_f( m.fast_char );
    else if( m.slash_mode ) m.slash_view->Do_f( m.fast_char );
  }
}

void Handle_z( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);
  const char CC2 = m.key.In();

  if( CC2 == 't' || IsEndOfLineDelim( CC2 ) )
  {
    if( cv->GetInDiff() ) m.diff.MoveCurrLineToTop();
    else                     cv->MoveCurrLineToTop();
  }
  else if( CC2 == 'z' )
  {
    if( cv->GetInDiff() ) m.diff.MoveCurrLineCenter();
    else                     cv->MoveCurrLineCenter();
  }
  else if( CC2 == 'b' )
  {
    if( cv->GetInDiff() ) m.diff.MoveCurrLineToBottom();
    else                     cv->MoveCurrLineToBottom();
  }
}

void Handle_Percent( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToOppositeBracket();
  else                     cv->GoToOppositeBracket();
}

void L_Handle_Percent( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( m.colon_mode ) m.colon_view->GoToOppositeBracket();
  else if( m.slash_mode ) m.slash_view->GoToOppositeBracket();
}

// Left squiggly bracket
void Handle_LeftSquigglyBracket( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToLeftSquigglyBracket();
  else                     cv->GoToLeftSquigglyBracket();
}

// Right squiggly bracket
void Handle_RightSquigglyBracket( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.GoToRightSquigglyBracket();
  else                     cv->GoToRightSquigglyBracket();
}

void Handle_F( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.PageDown();
  else                     cv->PageDown();
}

void Handle_B( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.PageUp();
  else                     cv->PageUp();
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

    Vis::Data::CmdFunc cf = m.ViewFuncs[ CC ];
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

  View* cv = CV(m);
  const char CC2 = m.key.In();

  if( CC2 == 'g' )
  {
    if( cv->GetInDiff() ) m.diff.GoToTopOfFile();
    else                     cv->GoToTopOfFile();
  }
  else if( CC2 == '0' )
  {
    if( cv->GetInDiff() ) m.diff.GoToStartOfRow();
    else                     cv->GoToStartOfRow();
  }
  else if( CC2 == '$' )
  {
    if( cv->GetInDiff() ) m.diff.GoToEndOfRow();
    else                     cv->GoToEndOfRow();
  }
  else if( CC2 == 'f' )
  {
    if( cv->GetInDiff() ) m.diff.GoToFile();
    else                     cv->GoToFile();
  }
}

void L_Handle_g( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC2 = m.key.In();

  if( CC2 == 'g' )
  {
    if     ( m.colon_mode ) m.colon_view->GoToTopOfFile();
    else if( m.slash_mode ) m.slash_view->GoToTopOfFile();
  }
  else if( CC2 == '0' )
  {
    if     ( m.colon_mode ) m.colon_view->GoToStartOfRow();
    else if( m.slash_mode ) m.slash_view->GoToStartOfRow();
  }
  else if( CC2 == '$' )
  {
    if     ( m.colon_mode ) m.colon_view->GoToEndOfRow();
    else if( m.slash_mode ) m.slash_view->GoToEndOfRow();
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
    if( !m.key.get_from_dot_buf_n )
    {
      m.key.dot_buf_n.clear();
      m.key.dot_buf_n.push('d');
      m.key.dot_buf_n.push('d');
    }
    View* cv = CV(m);

    if( cv->GetInDiff() ) m.diff.Do_dd();
    else                     cv->Do_dd();
  }
  else if( C == 'w' )
  {
    if( !m.key.get_from_dot_buf_n )
    {
      m.key.dot_buf_n.clear();
      m.key.dot_buf_n.push('d');
      m.key.dot_buf_n.push('w');
    }
    View* cv = CV(m);

    if( cv->GetInDiff() ) m.diff.Do_dw();
    else                     cv->Do_dw();
  }
}

void L_Handle_d( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char C = m.key.In();

  if( C == 'd' )
  {
    if( !m.key.get_from_dot_buf_l )
    {
      m.key.dot_buf_l.clear();
    }
    if     ( m.colon_mode ) m.colon_view->Do_dd();
    else if( m.slash_mode ) m.slash_view->Do_dd();
  }
  else if( C == 'w' )
  {
    if( !m.key.get_from_dot_buf_l )
    {
      m.key.dot_buf_l.clear();
      m.key.dot_buf_l.push('d');
      m.key.dot_buf_l.push('w');
    }
    if     ( m.colon_mode ) m.colon_view->Do_dw();
    else if( m.slash_mode ) m.slash_view->Do_dw();
  }
}

void Handle_y( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);
  const char C = m.key.In();

  if( C == 'y' )
  {
    if( cv->GetInDiff() ) m.diff.Do_yy();
    else                     cv->Do_yy();
  }
  else if( C == 'w' )
  {
    if( cv->GetInDiff() ) m.diff.Do_yw();
    else                     cv->Do_yw();
  }
}

void L_Handle_y( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char C = m.key.In();

  if( C == 'y' )
  {
    if     ( m.colon_mode ) m.colon_view->Do_yy();
    else if( m.slash_mode ) m.slash_view->Do_yy();
  }
  else if( C == 'w' )
  {
    if     ( m.colon_mode ) m.colon_view->Do_yw();
    else if( m.slash_mode ) m.slash_view->Do_yw();
  }
}

void Handle_Colon( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 == m.colon_file->NumLines() )
  {
    m.colon_file->PushLine();
  }
  View* cv = CV(m);
  const unsigned NUM_COLS = cv->WinCols();
  const unsigned X        = cv->X();
  const unsigned Y        = cv->Cmd__Line_Row();

  m.colon_view->SetContext( NUM_COLS, X, Y );
  m.colon_mode = true;

  const unsigned CL = m.colon_view->CrsLine();
  const unsigned LL = m.colon_file->LineLen( CL );

  if( 0<LL )
  {
    // Something on current line, so goto command line in escape mode
    m.colon_view->Update();
  }
  else {
    // Nothing on current line, so goto command line in insert mode
    L_Handle_i( m );
  }
}

void L_Handle_Colon( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.colon_mode = false;

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.PrintCursor( cv );
  else                     cv->PrintCursor();
}

void L_Handle_Return( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.colon_mode )
  {
    m.colon_mode = false;

    m.colon_view->HandleReturn();

    Handle_Colon_Cmd( m );
  }
  else if( m.slash_mode )
  {
    m.slash_mode = false;

    m.slash_view->HandleReturn();

    String slash_pattern( m.cbuf );

    m.vis.Handle_Slash_GotPattern( slash_pattern );
  }
}

void Handle_Slash( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 == m.slash_file->NumLines() )
  {
    m.slash_file->PushLine();
  }
  View* cv = CV(m);
  const unsigned NUM_COLS = cv->WinCols();
  const unsigned X        = cv->X();
  const unsigned Y        = cv->Cmd__Line_Row();

  m.slash_view->SetContext( NUM_COLS, X, Y );
  m.slash_mode = true;

  const unsigned CL = m.slash_view->CrsLine();
  const unsigned LL = m.slash_file->LineLen( CL );

  if( 0<LL )
  {
    // Something on current line, so goto command line in escape mode
    m.slash_view->Update();
  }
  else {
    // Nothing on current line, so goto command line in insert mode
    L_Handle_i( m );
  }
}

void L_Handle_Slash( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.slash_mode = false;

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.PrintCursor( cv );
  else                     cv->PrintCursor();
}

void L_Handle_Escape( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.colon_mode )
  {
    L_Handle_Colon( m );
  }
  else if( m.slash_mode )
  {
    L_Handle_Slash( m );
  }
}

void Handle_n( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_n();
  else                     cv->Do_n();
}

void L_Handle_n( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( m.colon_mode ) m.colon_view->Do_n();
  else if( m.slash_mode ) m.slash_view->Do_n();
}

void Handle_N( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_N();
  else                     cv->Do_N();
}

void L_Handle_N( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( m.colon_mode ) m.colon_view->Do_N();
  else if( m.slash_mode ) m.slash_view->Do_N();
}

void Handle_u( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_u();
  else                     cv->Do_u();
}

//void L_Handle_u( Vis::Data& m )
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  if     ( m.colon_mode ) m.colon_view->Do_u();
//  else if( m.slash_mode ) m.slash_view->Do_u();
//}

void Handle_U( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_U();
  else                     cv->Do_U();
}

//void L_Handle_U( Vis::Data& m )
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  if     ( m.colon_mode ) m.colon_view->Do_U();
//  else if( m.slash_mode ) m.slash_view->Do_U();
//}

// 1. Search for regex pattern in search editor.
// 2. If regex pattern is found in search editor,
//         move pattern to end of search editor
//    else add regex pattern to end of search editor
// 3. If search editor is displayed, update search editor window
//
void Do_Star_Update_Search_Editor( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  FileBuf* const pfb = m.slash_file;

  // Remove last line if it is blank:
  unsigned NUM_SE_LINES = pfb->NumLines(); // Number of search editor lines
  if( 0<NUM_SE_LINES && 0 == pfb->LineLen( NUM_SE_LINES-1 ) )
  {
    pfb->RemoveLine( NUM_SE_LINES-1 );
    NUM_SE_LINES = pfb->NumLines();
  }
  // 1. Search for regex pattern in search editor.
  bool found_pattern_in_search_editor = false;
  unsigned line_in_search_editor = 0;

  for( unsigned ln=0; !found_pattern_in_search_editor && ln<NUM_SE_LINES; ln++ )
  {
    const unsigned LL = pfb->LineLen( ln );
    // Copy line into m.sbuf until end of line or NULL byte
    m.sbuf.clear();
    int c = 1;
    for( unsigned k=0; c && k<LL; k++ )
    {
      c = pfb->Get( ln, k );
      m.sbuf.push( c );
    }
    if( m.sbuf == m.regex )
    {
      found_pattern_in_search_editor = true;
      line_in_search_editor = ln;
    }
  }
  // 2. If regex pattern is found in search editor,
  //         move pattern to end of search editor
  //    else add regex pattern to end of search editor
  if( found_pattern_in_search_editor )
  {
    // Move pattern to end of search editor, so newest searches are at bottom of file
    if( line_in_search_editor < NUM_SE_LINES-1 )
    {
      Line* lp = pfb->RemoveLineP( line_in_search_editor );
      pfb->PushLine( lp );
    }
  }
  else {
    // Push regex onto search editor buffer
    Line line;
    for( const char* p=m.regex.c_str(); *p; p++ ) line.push( *p );
    pfb->PushLine( line );
  }
  // Push an emtpy line onto slash buffer to leave empty / prompt:
  pfb->PushLine();

  // 3. If search editor is displayed, update search editor window
  View* cv = CV(m);
  m.slash_view->SetContext( cv->WinCols(), cv->X(), cv->Cmd__Line_Row() );
  m.slash_view->GoToCrsPos_NoWrite( pfb->NumLines()-1, 0 );
  pfb->Update();
}

void Handle_Star( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV(m);

  String pattern = cv->GetInDiff() ?  m.diff.Do_Star_GetNewPattern()
                                   :     cv->Do_Star_GetNewPattern();
  if( pattern != m.regex )
  {
    m.regex = pattern;

    if( 0<m.regex.len() )
    {
      Do_Star_Update_Search_Editor(m);
    }
    // Show new star pattern for all windows currently displayed:
    m.vis.UpdateAll( true );
  }
}

void Handle_D( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.dot_buf_n.clear();
    m.key.dot_buf_n.push('D');
  }
  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_D();
  else                     cv->Do_D();
}

void L_Handle_D( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_l )
  {
    m.key.dot_buf_l.clear();
  }
  if     ( m.colon_mode ) m.colon_view->Do_D();
  else if( m.slash_mode ) m.slash_view->Do_D();
}

void Handle_p( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.dot_buf_n.clear();
    m.key.dot_buf_n.push('p');
  }
  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_p();
  else                     cv->Do_p();
}

void L_Handle_p( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_l )
  {
    m.key.dot_buf_l.clear();
  }
  if     ( m.colon_mode ) m.colon_view->Do_p();
  else if( m.slash_mode ) m.slash_view->Do_p();
}

void Handle_P( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.dot_buf_n.clear();
    m.key.dot_buf_n.push('P');
  }
  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_P();
  else                     cv->Do_P();
}

void L_Handle_P( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_l )
  {
    m.key.dot_buf_l.clear();
  }
  if     ( m.colon_mode ) m.colon_view->Do_P();
  else if( m.slash_mode ) m.slash_view->Do_P();
}

void Handle_R( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.dot_buf_n.clear();
    m.key.dot_buf_n.push('R');
    m.key.save_2_dot_buf_n = true;
  }
  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_R();
  else                     cv->Do_R();

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.save_2_dot_buf_n = false;
  }
}

void L_Handle_R( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_l )
  {
    m.key.dot_buf_l.clear();
    m.key.dot_buf_l.push('R');
    m.key.save_2_dot_buf_l = true;
  }
  if( m.colon_mode )
  {
    bool end_of_line_delim = m.colon_view->Do_R();

    if( end_of_line_delim )
    {
      m.colon_mode = false;

      Handle_Colon_Cmd( m );
    }
  }
  else if( m.slash_mode )
  {
    bool end_of_line_delim = m.slash_view->Do_R();

    if( end_of_line_delim )
    {
      m.slash_mode = false;

      String slash_pattern( m.cbuf );

      m.vis.Handle_Slash_GotPattern( slash_pattern );
    }
  }
  if( !m.key.get_from_dot_buf_l )
  {
    m.key.save_2_dot_buf_l = false;
  }
}

void Handle_J( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.dot_buf_n.clear();
    m.key.dot_buf_n.push('J');
  }
  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_J();
  else                     cv->Do_J();
}

void L_Handle_J( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_l )
  {
    m.key.dot_buf_l.clear();
  }
  if     ( m.colon_mode ) m.colon_view->Do_J();
  else if( m.slash_mode ) m.slash_view->Do_J();
}

void Handle_Tilda( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_n )
  {
    m.key.dot_buf_n.clear();
    m.key.dot_buf_n.push('~');
  }
  View* cv = CV(m);

  if( cv->GetInDiff() ) m.diff.Do_Tilda();
  else                     cv->Do_Tilda();
}

void L_Handle_Tilda( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m.key.get_from_dot_buf_l )
  {
    m.key.dot_buf_l.clear();
    m.key.dot_buf_l.push('~');
  }
  if     ( m.colon_mode ) m.colon_view->Do_Tilda();
  else if( m.slash_mode ) m.slash_view->Do_Tilda();
}

void InitViewFuncs( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned k=0; k<128; k++ ) m.ViewFuncs[k] = 0;

  m.ViewFuncs[ 'i' ] = &Handle_i;
  m.ViewFuncs[ 'v' ] = &Handle_v;
  m.ViewFuncs[ 'V' ] = &Handle_V;
  m.ViewFuncs[ 'a' ] = &Handle_a;
  m.ViewFuncs[ 'A' ] = &Handle_A;
  m.ViewFuncs[ 'o' ] = &Handle_o;
  m.ViewFuncs[ 'O' ] = &Handle_O;
  m.ViewFuncs[ 'x' ] = &Handle_x;
  m.ViewFuncs[ 's' ] = &Handle_s;
  m.ViewFuncs[ 'c' ] = &Handle_c;
  m.ViewFuncs[ 'Q' ] = &Handle_Q;
  m.ViewFuncs[ 'k' ] = &Handle_k;
  m.ViewFuncs[ 'j' ] = &Handle_j;
  m.ViewFuncs[ 'h' ] = &Handle_h;
  m.ViewFuncs[ 'l' ] = &Handle_l;
  m.ViewFuncs[ 'H' ] = &Handle_H;
  m.ViewFuncs[ 'L' ] = &Handle_L;
  m.ViewFuncs[ 'M' ] = &Handle_M;
  m.ViewFuncs[ '0' ] = &Handle_0;
  m.ViewFuncs[ '$' ] = &Handle_Dollar;
  m.ViewFuncs[ '\n'] = &Handle_Return;
  m.ViewFuncs[ 'G' ] = &Handle_G;
  m.ViewFuncs[ 'b' ] = &Handle_b;
  m.ViewFuncs[ 'w' ] = &Handle_w;
  m.ViewFuncs[ 'e' ] = &Handle_e;
  m.ViewFuncs[ 'f' ] = &Handle_f;
  m.ViewFuncs[ ';' ] = &Handle_SemiColon;
  m.ViewFuncs[ '%' ] = &Handle_Percent;
  m.ViewFuncs[ '{' ] = &Handle_LeftSquigglyBracket;
  m.ViewFuncs[ '}' ] = &Handle_RightSquigglyBracket;
  m.ViewFuncs[ 'F' ] = &Handle_F;
  m.ViewFuncs[ 'B' ] = &Handle_B;
  m.ViewFuncs[ ':' ] = &Handle_Colon;
  m.ViewFuncs[ '/' ] = &Handle_Slash;
  m.ViewFuncs[ '*' ] = &Handle_Star;
  m.ViewFuncs[ '.' ] = &Handle_Dot;
  m.ViewFuncs[ 'm' ] = &Handle_m;
  m.ViewFuncs[ 'g' ] = &Handle_g;
  m.ViewFuncs[ 'W' ] = &Handle_W;
  m.ViewFuncs[ 'd' ] = &Handle_d;
  m.ViewFuncs[ 'y' ] = &Handle_y;
  m.ViewFuncs[ 'D' ] = &Handle_D;
  m.ViewFuncs[ 'p' ] = &Handle_p;
  m.ViewFuncs[ 'P' ] = &Handle_P;
  m.ViewFuncs[ 'R' ] = &Handle_R;
  m.ViewFuncs[ 'J' ] = &Handle_J;
  m.ViewFuncs[ '~' ] = &Handle_Tilda;
  m.ViewFuncs[ 'n' ] = &Handle_n;
  m.ViewFuncs[ 'N' ] = &Handle_N;
  m.ViewFuncs[ 'u' ] = &Handle_u;
  m.ViewFuncs[ 'U' ] = &Handle_U;
  m.ViewFuncs[ 'z' ] = &Handle_z;
}

void InitCmd_Funcs( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned k=0; k<128; k++ ) m.LineFuncs[k] = 0;

  m.LineFuncs[ 'i' ] = &L_Handle_i;
  m.LineFuncs[ 'v' ] = &L_Handle_v;
  m.LineFuncs[ 'a' ] = &L_Handle_a;
  m.LineFuncs[ 'A' ] = &L_Handle_A;
  m.LineFuncs[ 'o' ] = &L_Handle_o;
  m.LineFuncs[ 'x' ] = &L_Handle_x;
  m.LineFuncs[ 's' ] = &L_Handle_s;
  m.LineFuncs[ 'c' ] = &L_Handle_c;
  m.LineFuncs[ 'k' ] = &L_Handle_k;
  m.LineFuncs[ 'j' ] = &L_Handle_j;
  m.LineFuncs[ 'h' ] = &L_Handle_h;
  m.LineFuncs[ 'l' ] = &L_Handle_l;
  m.LineFuncs[ '0' ] = &L_Handle_0;
  m.LineFuncs[ '$' ] = &L_Handle_Dollar;
  m.LineFuncs[ '\n'] = &L_Handle_Return;
  m.LineFuncs[ 'G' ] = &L_Handle_G;
  m.LineFuncs[ 'b' ] = &L_Handle_b;
  m.LineFuncs[ 'w' ] = &L_Handle_w;
  m.LineFuncs[ 'e' ] = &L_Handle_e;
  m.LineFuncs[ 'f' ] = &L_Handle_f;
  m.LineFuncs[ ';' ] = &L_Handle_SemiColon;
  m.LineFuncs[ '%' ] = &L_Handle_Percent;
  m.LineFuncs[ ':' ] = &L_Handle_Colon;
  m.LineFuncs[ '\E'] = &L_Handle_Escape;
  m.LineFuncs[ '/' ] = &L_Handle_Slash;
  m.LineFuncs[ '.' ] = &L_Handle_Dot;
  m.LineFuncs[ 'g' ] = &L_Handle_g;
  m.LineFuncs[ 'd' ] = &L_Handle_d;
  m.LineFuncs[ 'y' ] = &L_Handle_y;
  m.LineFuncs[ 'D' ] = &L_Handle_D;
  m.LineFuncs[ 'p' ] = &L_Handle_p;
  m.LineFuncs[ 'P' ] = &L_Handle_P;
  m.LineFuncs[ 'R' ] = &L_Handle_R;
  m.LineFuncs[ 'J' ] = &L_Handle_J;
  m.LineFuncs[ '~' ] = &L_Handle_Tilda;
  m.LineFuncs[ 'n' ] = &L_Handle_n;
  m.LineFuncs[ 'N' ] = &L_Handle_N;
//m.LineFuncs[ 'u' ] = &L_Handle_u;
//m.LineFuncs[ 'U' ] = &L_Handle_U;
}

void AddToBufferEditor( Vis::Data& m, const char* fname )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line line( strlen( fname ) );

  for( const char* p=fname; *p; p++ ) line.push( *p );

  FileBuf* pfb = m.views[0][ BE_FILE ]->GetFB();
  pfb->PushLine( line );
  pfb->BufferEditor_SortName();
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

  if( pV->GetInDiff() ) m.diff.PrintCursor( pV );
  else                     pV->PrintCursor();
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
    else            { pL->push( C ); }
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
  InitMsgBuffer(m);
  InitShellBuffer(m);
  InitColonBuffer(m);
  InitSlashBuffer(m);

  const bool run_diff = InitUserFiles( m, ARGC, ARGV )
                     && (USER_FILE+2) == m.files.len();
  InitFileHistory(m);
  InitViewFuncs(m);
  InitCmd_Funcs(m);

  if( ! run_diff )
  {
    UpdateAll( false );
  }
  else {
    // User supplied: "-d file1 file2", so run diff:
    m.num_wins = 2;
    m.file_hist[ 0 ][0] = USER_FILE;
    m.file_hist[ 1 ][0] = USER_FILE+1;
    GetView_Win( m, 0 )->SetTilePos( TP_LEFT_HALF );
    GetView_Win( m, 1 )->SetTilePos( TP_RITE_HALF );

    Diff_Files_Displayed(m);
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
  MemMark(__FILE__,__LINE__); delete m.colon_view;
  MemMark(__FILE__,__LINE__); delete m.slash_view;

  delete &m;
}

void Vis::Run()
{
  Trace trace( __PRETTY_FUNCTION__ );

  while( m.running )
  {
    Handle_Cmd( m );

    // Handle focus time and sorting buffer editor:
    View* cv = CV();
    if( cv != m.cv_old )
    {
      m.cv_old = cv;
      cv->GetFB()->SetFocTime( GetTimeSeconds() );

      // cv is of buffer editor:
      if( m.sort_by_time && cv->GetFB() == m.views[0][ BE_FILE ]->GetFB() )
      {
        // Update the buffer editor if it changed in the sort:
        if( cv->GetFB()->Sort() ) cv->Update();
      }
    }
  }
  Console::Flush();
}

void Vis::Stop()
{
  m.running = false;
}

View* Vis::CV() const
{
  return GetView_Win( m, m.win );
}

View* Vis::WinView( const unsigned w ) const
{
  return GetView_Win( m, w );
}

FileBuf* Vis::FileNum2Buf( const unsigned file_num ) const
{
  return m.views[0][ file_num ]->GetFB();
}

unsigned Vis::Buf2FileNum( const FileBuf* pfb ) const
{
  for( unsigned k=0; k<m.views[0].len(); k++ )
  {
    if( m.views[0][ k ]->GetFB() == pfb )
    {
      return k;
    }
  }
  return 0;
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

void Clear_Diff( Vis::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.diff_mode = false;

  // Make sure diff is turned off for everything:
  for( int w=0; w<MAX_WINS; w++ )
  {
    ViewList& vl = m.views[w];

    for( int f=0; f<vl.len(); f++ )
    {
      vl[ f ]->SetInDiff( false );
    }
  }
}

void Vis::NoDiff()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( CV()->GetInDiff() )
  {
    m.diff_mode = false;

    View* pvS = m.diff.GetViewShort();
    View* pvL = m.diff.GetViewLong();

    // Set the view contexts to similar values as the diff contexts:
    if( 0 != pvS )
    {
      pvS->SetInDiff( false );
      pvS->SetTopLine ( m.diff.GetTopLine ( pvS ) );
      pvS->SetLeftChar( m.diff.GetLeftChar() );
      pvS->SetCrsRow  ( m.diff.GetCrsRow  () );
      pvS->SetCrsCol  ( m.diff.GetCrsCol  () );
    }
    if( 0 != pvL )
    {
      pvL->SetInDiff( false );
      pvL->SetTopLine ( m.diff.GetTopLine ( pvL ) );
      pvL->SetLeftChar( m.diff.GetLeftChar() );
      pvL->SetCrsRow  ( m.diff.GetCrsRow  () );
      pvL->SetCrsCol  ( m.diff.GetCrsCol  () );
    }
  }
  Clear_Diff(m);

  UpdateAll( false );
}

bool Vis::InDiffMode() const
{
  return m.diff_mode;
}

bool Vis::Shell_Running() const
{
  return m.shell.Running();
}

bool Vis::GetSortByTime() const
{
  return m.sort_by_time;
}

void Vis::Update_Shell()
{
  m.shell.Update();
}

bool Vis::RunningDot() const
{
  return m.key.get_from_dot_buf_n
      || m.key.get_from_dot_buf_l;
}

FileBuf* Vis::GetFileBuf( const unsigned index ) const
{
  return m.files[ index ];
}

FileBuf* Vis::GetFileBuf( const String& fname ) const
{
  for( unsigned k=0; k<m.files.len(); k++ )
  {
    FileBuf* pfb_k = m.files[ k ];

    if( fname == pfb_k->GetPathName() )
    {
      return pfb_k;
    }
  }
  return 0;
}

Diff& Vis::GetDiff() const
{
  return m.diff;
}

unsigned Vis::GetRegexLen() const
{
  return m.regex.len();
}

String Vis::GetRegex() const
{
  return m.regex;
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
      UpdateAll( false );
    }
  }
}

void Vis::CheckFileModTime()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // m.file_hist[m.win][0] is the current file number of the current window
  if( USER_FILE <= m.file_hist[m.win][0] )
  {
    FileBuf* pfb = CV()->GetFB();
    const char* fname = pfb->GetPathName();

    const double curr_mod_time = ModificationTime( fname );

    if( pfb->GetModTime() < curr_mod_time )
    {
      if( pfb->IsRegular() )
      {
        // Update file modification time so that the message window
        // will not keep popping up:
      //pfb->SetModTime( curr_mod_time );
        pfb->SetChangedExternally();
      //m.vis.Window_Message("\n%s\n\nhas changed since it was read in\n\n", fname );
      }
      else if( pfb->IsDir() )
      {
        // Dont ask the user, just read in the directory.
        // pfb->GetModTime() will get updated in pfb->ReReadFile()
        pfb->ReReadFile();

        for( unsigned w=0; w<m.num_wins; w++ )
        {
          if( pfb == GetView_Win( m, w )->GetFB() )
          {
            // View is currently displayed, perform needed update:
            GetView_Win( m, w )->Update();
          }
        }
      }
    }
  }
}

void Vis::Add_FileBuf_2_Lists_Create_Views( FileBuf* pfb, const char* fname )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Add this file buffer to global list of files
  m.files.push( pfb );
  // Create a new view for each window for FileBuf
  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* pV  = new(__FILE__,__LINE__) View( m.vis, m.key, *pfb, m.reg );
    bool ok = m.views[w].push( pV );
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

//void Vis::UpdateAll()
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//
//  if( m.diff_mode )
//  {
//    m.diff.Update();
//  }
//  else {
//    for( unsigned k=0; k<m.num_wins; k++ )
//    {
//      GetView_Win( m, k )->Update( false );
//    }
//    PrintCursor();
//  }
//}

void Vis::UpdateAll( const bool show_search )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned w=0; w<m.num_wins; w++ )
  {
    View* pv = GetView_Win( m, w );

    if( ! pv->GetInDiff() )
    {
      if( show_search )
      {
        String msg("/");
        pv->Set_Cmd_Line_Msg( msg += m.regex );
      }
      pv->Update( false ); //< Do not print cursor
    }
  }
  if( m.diff_mode )
  {
    if( show_search )
    {
      String msg("/");
      m.diff.Set_Cmd_Line_Msg( msg += m.regex );
    }
    m.diff.Update();
  }
  View* cv = CV();

  if( !cv->GetInDiff() ) cv->PrintCursor();
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
      View* const pV = GetView_Win( m, w );

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
// '    ' for file in vis same as file on file system,
// '++++' for changes in vis not written to file system,
// '////' for file on file system changed externally to vis,
// '+/+/' for changes in vis and on file system
bool Vis::Update_Change_Statuses()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Update buffer changed status around windows:
  bool updated_change_sts = false;

  for( unsigned w=0; w<m.num_wins; w++ )
  {
    // pV points to currently displayed view in window w:
    View* const pV = GetView_Win( m, w );

    if( (pV->GetUnSaved_ChangeSts() != pV->GetFB()->Changed())
     || (pV->GetExternalChangeSts() != pV->GetFB()->GetChangedExternally()) )
    {
      pV->Print_Borders();
      pV->SetUnSaved_ChangeSts( pV->GetFB()->Changed() );
      pV->SetExternalChangeSts( pV->GetFB()->GetChangedExternally() );
      updated_change_sts = true;
    }
  }
  return updated_change_sts;
}

void Vis::PrintCursor()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.colon_mode )
  {
    m.colon_view->PrintCursor();
  }
  else if( m.slash_mode )
  {
    m.slash_view->PrintCursor();
  }
  else {
    View* cv = CV();

    if( cv->GetInDiff() ) m.diff.PrintCursor( cv );
    else                     cv->PrintCursor();
  }
}

bool Vis::HaveFile( const char* path_name, unsigned* file_index )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool already_have_file = false;

  const unsigned NUM_FILES = m.files.len();

  for( unsigned k=0; !already_have_file && k<NUM_FILES; k++ )
  {
    if( 0==strcmp( m.files[k]->GetPathName(), path_name ) )
    {
      already_have_file = true;

      if( file_index ) *file_index = k;
    }
  }
  return already_have_file;
}

bool Vis::NotHaveFileAddFile( const String& pname )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool added_file = false;

  if( !HaveFile( pname.c_str() ) )
  {
    FileBuf* p_fb = new(__FILE__,__LINE__)
                    FileBuf( m.vis, pname.c_str(), true, FT_UNKNOWN );
    p_fb->ReadFile();

    added_file = true;
  }
  return added_file;
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
  else if( !FindFullFileNameRel2( CV()->GetDirName(), fname ) )
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
  if( FileExists( fname.c_str() ) )
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

void Vis::L_Handle_f()
{
  ::L_Handle_f(m);
}

void Vis::Handle_z()
{
  ::Handle_z(m);
}

void Vis::Handle_SemiColon()
{
  ::Handle_SemiColon(m);
}

void Vis::L_Handle_SemiColon()
{
  ::L_Handle_SemiColon(m);
}

void Vis::Handle_Slash_GotPattern( const String& pattern
                                 , const bool MOVE_TO_FIRST_PATTERN )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.regex = pattern;

  if( 0<m.regex.len() )
  {
    Do_Star_Update_Search_Editor(m);
  }
  View* cv = CV();

  if( MOVE_TO_FIRST_PATTERN && 0<pattern.len() )
  {
    if( cv->GetInDiff() ) m.diff.Do_n();
    else                     cv->Do_n();
  }
  // Show new slash pattern for all windows currently displayed:
  m.vis.UpdateAll( true );
}

// Given view of currently displayed on this side and other side,
// and file indexes of files to diff on this side and other side,
// perform diff of files identified by the file indexes.
// cV - View of currently displayed file on this side
// oV - View of currently displayed file on other side
// c_file_idx - Index of file to diff on this side
// o_file_idx - Index of file to diff on other side
bool Vis::Diff_By_File_Indexes( View* const cV, unsigned const c_file_idx
                              , View* const oV, unsigned const o_file_idx )
{
  bool ok = false;
  // Get m_win for cV and oV
  const int c_win = GetWinNum_Of_View( m, cV );
  const int o_win = GetWinNum_Of_View( m, oV );

  if( 0 <= c_win && 0 <= o_win )
  {
    m.file_hist[ c_win ].insert( 0, c_file_idx );
    m.file_hist[ o_win ].insert( 0, o_file_idx );
    // Remove subsequent file_idx's from m.file_hist[ c_win ]:
    for( unsigned k=1; k<m.file_hist[ c_win ].len(); k++ )
    {
      if( c_file_idx == m.file_hist[ c_win ][ k ] )
      {
        m.file_hist[ c_win ].remove( k );
      }
    }
    // Remove subsequent file_idx's from m.file_hist[ o_win ]:
    for( unsigned k=1; k<m.file_hist[ o_win ].len(); k++ )
    {
      if( c_file_idx == m.file_hist[ o_win ][ k ] )
      {
        m.file_hist[ o_win ].remove( k );
      }
    }
    View* nv_c = GetView_WinPrev( m, c_win, 0 );
    View* nv_o = GetView_WinPrev( m, o_win, 0 );

    nv_c->SetTilePos( cV->GetTilePos() );
    nv_o->SetTilePos( oV->GetTilePos() );

    ok = m.diff.Run( nv_c, nv_o );
    if( ok ) {
      m.diff_mode = true;
      m.diff.GetViewShort()->SetInDiff( true );
      m.diff.GetViewLong() ->SetInDiff( true );
    }
  }
  return ok;
}

// Line returned has at least SIZE, but zero length
//
Line* Vis::BorrowLine( const char*    _FILE_
                     , const unsigned _LINE_
                     , const unsigned SIZE )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;

  if( 0<m.line_cache.len() )
  {
    bool ok = m.line_cache.pop( lp );
    ASSERT( __LINE__, ok, "ok" );

    ok = lp->inc_cap( SIZE );
    ASSERT( __LINE__, ok, "ok" );

    lp->clear();
  }
  else {
    lp = new( _FILE_, _LINE_ ) Line( SIZE );
  }
  return lp;
}

// Line returned has LEN and is filled up to LEN with FILL
//
Line* Vis::BorrowLine( const char*    _FILE_
                     , const unsigned _LINE_
                     , const unsigned LEN, const uint8_t FILL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;

  if( 0<m.line_cache.len() )
  {
    bool ok = m.line_cache.pop( lp );
    ASSERT( __LINE__, ok, "ok" );

    ok = lp->set_len( LEN );
    ASSERT( __LINE__, ok, "ok" );

    for( unsigned k=0; k<LEN; k++ ) lp->set( k, FILL );
  }
  else {
    lp = new(_FILE_,_LINE_) Line( LEN, FILL );
  }
  return lp;
}

// Line returned has same len and contents as line
//
Line* Vis::BorrowLine( const char*    _FILE_
                     , const unsigned _LINE_
                     , const Line&    line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;

  if( 0<m.line_cache.len() )
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

