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
#include "Vis.hh"

const char* PROG_NAME;

const int FD_IO = 0; // read/write file descriptor

extern MemLog<MEM_LOG_BUF_SIZE> Log;
extern const char* DIR_DELIM_STR;

const uint16_t MAX_COLS   = 1024; // Arbitrary maximum char width of window
const unsigned BE_FILE    = 0;    // Buffer editor file
const unsigned HELP_FILE  = 1;    // Help          file
const unsigned SE_FILE    = 2;    // Search editor file
const unsigned MSG_FILE   = 3;    // Message       file
const unsigned CMD_FILE   = 4;    // Command Shell file

const char* EDIT_BUF_NAME = "BUFFER_EDITOR";
const char* HELP_BUF_NAME = "VIS_HELP";
const char* SRCH_BUF_NAME = "SEARCH_EDITOR";
const char* MSG__BUF_NAME = "MESSAGE_BUFFER";
const char* CMD__BUF_NAME = "SHELL_BUFFER";

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

class Vis::Imp
{
public:
  Vis&       m_vis;
  bool       m_running;
  Key        m_key;
  Diff       m_diff;
  Colon      m_colon;
  char       m_cbuf[MAX_COLS];    // General purpose char buffer
  String     m_sbuf;              // General purpose string buffer
  unsigned   m_win;               // Sub-window index
  unsigned   m_num_wins;          // Number of sub-windows currently on screen
  FileList   m_files;             // list of file buffers
  ViewList   m_views[MAX_WINS];   // Array of lists of file views
  unsList    m_file_hist[MAX_WINS]; // Array of lists of view history. [m_win][m_view_num]
  LinesList  m_reg;               // Register
  LinesList  m_line_cache;
  ChangeList m_change_cache;
  Paste_Mode m_paste_mode;
  bool       m_diff_mode; // true if displaying diff
  bool       m_cmd_mode;  // true if running a shell command
  String     m_star;      // current text highlighted by '*' command
  bool       m_slash;     // indicated whether star pattern is slash type or * type
  int        m_fast_char; // Char on line to goto when ';' is entered

  typedef void (Imp::*CmdFunc) ();
  CmdFunc CmdFuncs[128];

public:
  Imp( Vis& vis );
  ~Imp();

  void Init( const int ARGC, const char* const ARGV[] );
  void Run();
  void Stop();
  View* CV() const;
  View* WinView( const unsigned w ) const;
  unsigned    GetNumWins() const;
  Paste_Mode  GetPasteMode() const;
  void        SetPasteMode( Paste_Mode pm );
  bool        InDiffMode() const;
  bool        RunningCmd() const;
  bool        RunningDot() const;
  FileBuf*    GetFileBuf( const unsigned index ) const;
  unsigned    GetStarLen() const;
  const char* GetStar() const;
  bool        GetSlash() const;

  void CheckWindowSize();
  void CheckFileModTime();
  void Add_FileBuf_2_Lists_Create_Views( FileBuf* pfb, const char* fname );
  void CmdLineMessage( const char* const msg );
  void Window_Message( const char* const msg );
  void UpdateAll();
  bool Update_Status_Lines();
  bool Update_Change_Statuses();
  void PrintCursor();
  bool HaveFile( const char* file_name, unsigned* index=0 );
  bool File_Is_Displayed( const String& full_fname );
  void ReleaseFileName( const String& full_fname );
  bool GoToBuffer_Fname( String& fname );
  void Handle_f();
  void Handle_z();
  void Handle_SemiColon();
  void Handle_Slash_GotPattern( const String& pattern
                              , const bool MOVE_TO_FIRST_PATTERN=true );

  Line* BorrowLine( const char* _FILE_, const unsigned _LINE_, const unsigned SIZE = 0 );
  Line* BorrowLine( const char* _FILE_, const unsigned _LINE_, const unsigned LEN, const uint8_t FILL );
  Line* BorrowLine( const char* _FILE_, const unsigned _LINE_, const Line& line );
  void  ReturnLine( Line* lp );

  LineChange* BorrowLineChange( const ChangeType type
                              , const unsigned   lnum
                              , const unsigned   cpos );
  void  ReturnLineChange( LineChange* lcp );

private:
  View* PV() const;
  void Ch_Dir();
  void GetCWD();
  void Set_Syntax();
  void AdjustViews();

  void InitBufferEditor();
  void InitHelpBuffer();
  void InitSearchEditor();
  void InitMsgBuffer();
  void InitCmdBuffer();
  bool InitUserFiles( const int ARGC, const char* const ARGV[] );
  void InitFileHistory();
  void InitCmdFuncs();
  void AddToBufferEditor( const char* fname );
  bool File_Is_Displayed( const unsigned file_num );

  void Quit();
  void QuitAll();
  void Quit_ShiftDown();
  void Quit_JoinTiles( const Tile_Pos TP );
  void Quit_JoinTiles_TP_LEFT_HALF();
  void Quit_JoinTiles_TP_RITE_HALF();
  void Quit_JoinTiles_TP_TOP__HALF();
  void Quit_JoinTiles_TP_BOT__HALF();
  void Quit_JoinTiles_TP_TOP__LEFT_QTR();
  void Quit_JoinTiles_TP_TOP__RITE_QTR();
  void Quit_JoinTiles_TP_BOT__LEFT_QTR();
  void Quit_JoinTiles_TP_BOT__RITE_QTR();
  void Quit_JoinTiles_TP_LEFT_QTR();
  void Quit_JoinTiles_TP_RITE_QTR();
  void Quit_JoinTiles_TP_LEFT_CTR__QTR();
  void Quit_JoinTiles_TP_RITE_CTR__QTR();
  void Quit_JoinTiles_TP_TOP__LEFT_8TH();
  void Quit_JoinTiles_TP_TOP__RITE_8TH();
  void Quit_JoinTiles_TP_TOP__LEFT_CTR_8TH();
  void Quit_JoinTiles_TP_TOP__RITE_CTR_8TH();
  void Quit_JoinTiles_TP_BOT__LEFT_8TH();
  void Quit_JoinTiles_TP_BOT__RITE_8TH();
  void Quit_JoinTiles_TP_BOT__LEFT_CTR_8TH();
  void Quit_JoinTiles_TP_BOT__RITE_CTR_8TH();
  bool Have_TP_BOT__HALF();
  bool Have_TP_TOP__HALF();
  bool Have_TP_BOT__LEFT_QTR();
  bool Have_TP_TOP__LEFT_QTR();
  bool Have_TP_BOT__RITE_QTR();
  bool Have_TP_TOP__RITE_QTR();
  void Help();
  void DoDiff();
  View* DoDiff_FindRegFileView( const FileBuf* pfb_reg
                              , const FileBuf* pfb_dir
                              , const unsigned win_idx
                              ,       View*    pv );
  View* DoDiff_CheckPossibleFile( const int win_idx
                                , const char* pos_fname );
  void NoDiff();
  void SetWinToBuffer( const unsigned win_idx
                     , const unsigned buf_idx
                     , const bool     update );

  void GoToFile();
  void GoToBuffer( const unsigned buf_idx );
  void GoToNextBuffer();
  void GoToCurrBuffer();
  void GoToPrevBuffer();
  void GoToPoundBuffer();
  void GoToBufferEditor();
  void GoToMsgBuffer();
  void GoToCmdBuffer();
  void GoToSearchBuffer();
  void GoToNextWindow();
  void GoToNextWindow_l();
  bool GoToNextWindow_l_Find();
  void GoToNextWindow_h();
  bool GoToNextWindow_h_Find();
  void GoToNextWindow_jk();
  bool GoToNextWindow_jk_Find();
  void VSplitWindow();
  void HSplitWindow();
  void FlipWindows();
  bool FName_2_FNum( const String& full_fname, unsigned& file_num );
  void ReleaseFileNum( const unsigned file_num );

  void Do_Star_FindPatterns();
  void Do_Star_ClearPatterns();
  void Do_Star_PrintPatterns( const bool HIGHLIGHT );
  void Do_Star_Update_Search_Editor();

  void Handle_Cmd();
  void Handle_i();
  void Handle_v();
  void Handle_V();
  void Handle_a();
  void Handle_A();
  void Handle_o();
  void Handle_O();
  void Handle_x();
  void Handle_s();
  void Handle_c();
  void Handle_Q();
  void Handle_k();
  void Handle_j();
  void Handle_h();
  void Handle_l();
  void Handle_H();
  void Handle_L();
  void Handle_M();
  void Handle_0();
  void Handle_Dollar();
  void Handle_Return();
  void Handle_G();
  void Handle_b();
  void Handle_w();
  void Handle_e();
  void Handle_Percent();
  void Handle_LeftSquigglyBracket();
  void Handle_RightSquigglyBracket();
  void Handle_F();
  void Handle_B();
  void Handle_Dot();
  void Handle_m();
  void Handle_g();
  void Handle_W();
  void Handle_d();
  void Handle_y();
  void Handle_D();
  void Handle_p();
  void Handle_P();
  void Handle_R();
  void Handle_J();
  void Handle_Tilda();
  void Handle_Star();
  void Handle_Slash();
  void Handle_n();
  void Handle_N();
  void Handle_u();
  void Handle_U();

  void Handle_Colon();
  void HandleColon_b();
  void HandleColon_e();
  void HandleColon_w();

#ifndef WIN32
  void RunCommand();
  bool RunCommand_GetCommand( String& cmd );
  bool RunCommand_RunCommand( const String& cmd, FileBuf* pfb, int& exit_val );
#endif
};

Vis::Imp::Imp( Vis& vis )
  : m_vis( vis )
  , m_running( true )
  , m_key()
  , m_diff( m_vis, m_key, m_reg )
  , m_colon( m_vis, m_key, m_diff, m_cbuf, m_sbuf )
  , m_win( 0 )
  , m_num_wins( 1 )
  , m_files(__FILE__, __LINE__)
  , m_views()
  , m_file_hist()
  , m_reg()
  , m_line_cache()
  , m_change_cache()
  , m_paste_mode( PM_LINE )
  , m_diff_mode( false )
  , m_cmd_mode( false )
  , m_slash( false )
  , m_fast_char( -1 )
{
}

void Vis::Imp::Init( const int ARGC, const char* const ARGV[] )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_cbuf[0] = 0;

  Console::SetVis( &m_vis );

  m_running = Console::GetWindowSize();
              Console::SetConsoleCursor();

  InitBufferEditor();
  InitHelpBuffer();
  InitSearchEditor();
  InitMsgBuffer();
  InitCmdBuffer();
  bool run_diff = InitUserFiles( ARGC, ARGV );
  InitFileHistory();
  InitCmdFuncs();

  if( run_diff && ( (CMD_FILE+1+2) == m_files.len()) )
  {
    // User supplied: "-d file1 file2", so run diff:
    m_diff_mode = true;
    m_num_wins = 2;
    m_file_hist[ 0 ][0] = 5;
    m_file_hist[ 1 ][0] = 6;
    m_views[0][ m_file_hist[ 0 ][0] ]->SetTilePos( TP_LEFT_HALF );
    m_views[1][ m_file_hist[ 1 ][0] ]->SetTilePos( TP_RITE_HALF );

    DoDiff();
  }
}

Vis::Imp::~Imp()
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned i=0; i<MAX_WINS; i++ )
  {
    for( unsigned k=0; k<m_views[i].len(); k++ )
    {
      MemMark(__FILE__,__LINE__);
      delete m_views[i][k];
    }
  }
  for( unsigned k=0; k<m_files.len(); k++ )
  {
    MemMark(__FILE__,__LINE__);
    delete m_files[k];
  }
}

void Vis::Imp::Run()
{
  Trace trace( __PRETTY_FUNCTION__ );
  UpdateAll();

  while( m_running )
  {
    Handle_Cmd();
  }
  Console::Flush();
}

void Vis::Imp::Stop()
{
  m_running = false;
}

View* Vis::Imp::CV() const
{
  return m_views[m_win][ m_file_hist[m_win][0] ];
}

View* Vis::Imp::PV() const
{
   return m_views[m_win][ m_file_hist[m_win][1] ];
}

View* Vis::Imp::WinView( const unsigned w ) const
{
  return m_views[w][ m_file_hist[w][0] ];
}

unsigned Vis::Imp::GetNumWins() const
{
  return m_num_wins;
}

Paste_Mode Vis::Imp::GetPasteMode() const
{
  return m_paste_mode;
}

void Vis::Imp::SetPasteMode( Paste_Mode pm )
{
  m_paste_mode = pm;
}

bool Vis::Imp::InDiffMode() const
{
  return m_diff_mode;
}

bool Vis::Imp::RunningCmd() const
{
  return m_cmd_mode;
}

bool Vis::Imp::RunningDot() const
{
  return m_key.get_from_dot_buf;
}

FileBuf* Vis::Imp::GetFileBuf( const unsigned index ) const
{
  return m_files[ index ];
}

unsigned Vis::Imp::GetStarLen() const
{
  return m_star.len();
}

const char* Vis::Imp::GetStar() const
{
  return m_star.c_str();
}

bool Vis::Imp::GetSlash() const
{
  return m_slash;
}

// If window has resized, update window
void Vis::Imp::CheckWindowSize()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned numRows = Console::Num_Rows();
  const unsigned numCols = Console::Num_Cols();

  if( Console::GetWindowSize() )
  {
    if( Console::Num_Rows() != numRows
     || Console::Num_Cols() != numCols )
    {
      AdjustViews();
      Console::Invalidate();
      UpdateAll();
    }
  }
}

void Vis::Imp::CheckFileModTime()
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

      for( unsigned w=0; w<m_num_wins; w++ )
      {
        if( pfb == m_views[w][ m_file_hist[w][0] ]->GetFB() )
        {
          // View is currently displayed, perform needed update:
          m_views[w][ m_file_hist[w][0] ]->Update();
        }
      }
    }
    else { // Regular file
      // Update file modification time so that the message window
      // will not keep popping up:
      pfb->SetModTime( curr_mod_time );

      m_vis.Window_Message("\n%s\n\nhas changed since it was read in\n\n", fname );
    }
  }
}

void Vis::Imp::Add_FileBuf_2_Lists_Create_Views( FileBuf* pfb, const char* fname )
{
  // Add this file buffer to global list of files
  m_files.push(__FILE__,__LINE__, pfb );
  // Create a new view for each window for FileBuf
  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* pV  = new(__FILE__,__LINE__) View( m_vis, m_key, *pfb, m_reg );
    bool ok = m_views[w].push(__FILE__,__LINE__, pV );
    ASSERT( __LINE__, ok, "ok" );
    pfb->AddView( pV );
  }
  // Push file name onto buffer editor buffer
  AddToBufferEditor( fname );
}

// Print a command line message.
// Put cursor back in edit window.
//
void Vis::Imp::CmdLineMessage( const char* const msg )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = CV();

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

void Vis::Imp::Window_Message( const char* const msg )
{
  Trace trace( __PRETTY_FUNCTION__ );

  FileBuf* pMB = m_views[0][MSG_FILE]->GetFB();

  // Clear Message Buffer:
  while( pMB->NumLines() ) pMB->PopLine();

  // Add msg to pMB
  Line* pL = 0;
  const unsigned MSB_BUF_LEN = strlen( msg );
  for( unsigned k=0; k<MSB_BUF_LEN; k++ )
  {
    if( 0==pL ) pL = BorrowLine( __FILE__,__LINE__ );

    const char C = msg[k];

    if( C == '\n' ) { pMB->PushLine( pL ); pL = 0; }
    else            { pL->push(__FILE__,__LINE__, C ); }
  }
  // Make sure last borrowed line gets put into Message Buffer:
  if( pL ) pMB->PushLine( pL );

  // Initially, put cursor at top of Message Buffer:
  View* pV = m_views[m_win][MSG_FILE];
  pV->SetCrsRow( 0 );
  pV->SetCrsCol( 0 );
  pV->SetTopLine( 0 );

  GoToMsgBuffer();
}

void Vis::Imp::UpdateAll()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_diff_mode )
  {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      m_views[k][ m_file_hist[k][0] ]->Update();
    }
  }
}

bool Vis::Imp::Update_Status_Lines()
{
  bool updated_a_sts_line = false;

  if( m_diff_mode )
  {
     updated_a_sts_line = m_diff.Update_Status_Lines();
  }
  else {
    for( unsigned w=0; w<m_num_wins; w++ )
    {
      // pV points to currently displayed view in window w:
      View* const pV = m_views[w][ m_file_hist[w][0] ];

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

bool Vis::Imp::Update_Change_Statuses()
{
  // Update buffer changed status around windows:
  bool updated_change_sts = false;

  for( unsigned k=0; k<m_num_wins; k++ )
  {
    // pV points to currently displayed view in window w:
    View* const pV = m_views[k][ m_file_hist[k][0] ];

    if( pV->GetUnSavedChangeSts() != pV->GetFB()->Changed() )
    {
      pV->Print_Borders();
      pV->SetUnSavedChangeSts( pV->GetFB()->Changed() );
      updated_change_sts = true;
    }
  }
  return updated_change_sts;
}

void Vis::Imp::PrintCursor()
{
  if( m_diff_mode )
  {
   m_diff.PrintCursor( CV() );
  }
  else {
    CV()->PrintCursor();
  }
}

bool Vis::Imp::HaveFile( const char* file_name, unsigned* file_index )
{
  bool already_have_file = false;

  const unsigned NUM_FILES = m_files.len();

  for( unsigned k=0; !already_have_file && k<NUM_FILES; k++ )
  {
    if( 0==strcmp( m_files[k]->GetFileName(), file_name ) )
    {
      already_have_file = true;

      if( file_index ) *file_index = k;
    }
  }
  return already_have_file;
}

bool Vis::Imp::File_Is_Displayed( const String& full_fname )
{
  unsigned file_num = 0;

  if( FName_2_FNum( full_fname, file_num ) )
  {
    return File_Is_Displayed( file_num );
  }
  return false;
}

void Vis::Imp::ReleaseFileName( const String& full_fname )
{
  unsigned file_num = 0;
  if( FName_2_FNum( full_fname, file_num ) )
  {
    ReleaseFileNum( file_num );
  }
}

// Return true if went to buffer indicated by fname, else false
bool Vis::Imp::GoToBuffer_Fname( String& fname )
{
  // 1. Search for fname in buffer list, and if found, go to that buffer:
  unsigned file_index = 0;
  if( HaveFile( fname.c_str(), &file_index ) )
  { GoToBuffer( file_index ); return true; }

  // 2. Go to directory of current buffer:
  if( ! CV()->GoToDir() )
  {
    m_vis.CmdLineMessage( "Could not find file: %s", fname.c_str() );
    return false;
  }

  // 3. Get full file name
  if( !FindFullFileName( fname ) )
  {
    m_vis.CmdLineMessage( "Could not find file: %s", fname.c_str() );
    return false;
  }

  // 4. Search for fname in buffer list, and if found, go to that buffer:
  if( HaveFile( fname.c_str(), &file_index ) )
  {
    GoToBuffer( file_index ); return true;
  }

  // 5. See if file exists, and if so, add a file buffer, and go to that buffer
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
    FileBuf* fb = new(__FILE__,__LINE__) FileBuf( m_vis, fname.c_str(), true, FT_UNKNOWN );
    fb->ReadFile();
    GoToBuffer( m_views[m_win].len()-1 );
  }
  else {
    m_vis.CmdLineMessage( "Could not find file: %s", fname.c_str() );
    return false;
  }
  return true;
}

void Vis::Imp::Handle_f()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_fast_char = m_key.In();

  if( m_diff_mode ) m_diff.Do_f( m_fast_char );
  else               CV()->Do_f( m_fast_char );
}

void Vis::Imp::Handle_z()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC2 = m_key.In();

  if( CC2 == 't' || IsEndOfLineDelim( CC2 ) )
  {
    if( m_diff_mode ) m_diff.MoveCurrLineToTop();
    else               CV()->MoveCurrLineToTop();
  }
  else if( CC2 == 'z' )
  {
    if( m_diff_mode ) m_diff.MoveCurrLineCenter();
    else               CV()->MoveCurrLineCenter();
  }
  else if( CC2 == 'b' )
  {
    if( m_diff_mode ) m_diff.MoveCurrLineToBottom();
    else               CV()->MoveCurrLineToBottom();
  }
}

void Vis::Imp::Handle_SemiColon()
{
  if( 0 <= m_fast_char )
  {
    if( m_diff_mode ) m_diff.Do_f( m_fast_char );
    else           CV()->Do_f( m_fast_char );
  }
}

void Vis::Imp::Handle_Slash_GotPattern( const String& pattern
                                      , const bool MOVE_TO_FIRST_PATTERN )
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( m_slash && pattern == m_star )
  {
    CV()->PrintCursor();
    return;
  }
  // Un-highlight old star patterns for windows displayed:
  if( m_star.len()  )
  { // Since m_diff_mode does Console::Update(),
    // no need to print patterns here if in m_diff_mode
    if( !m_diff_mode ) Do_Star_PrintPatterns( false );
  }
  Do_Star_ClearPatterns();

  m_star = pattern;

  if( !m_star.len() ) CV()->PrintCursor();
  else {
    m_slash = true;

    Do_Star_Update_Search_Editor();
    Do_Star_FindPatterns();

    // Highlight new star patterns for windows displayed:
    if( !m_diff_mode ) Do_Star_PrintPatterns( true );

    if( MOVE_TO_FIRST_PATTERN )
    {
      if( m_diff_mode ) m_diff.Do_n(); // Move to first pattern
      else               CV()->Do_n(); // Move to first pattern
    }
    if( m_diff_mode ) m_diff.Update();
    else {
      // Print out all the changes:
      Console::Update();
      Console::Flush();
    }
  }
}

// Line returned has at least SIZE, but zero length
//
Line* Vis::Imp::BorrowLine( const char*    _FILE_
                          , const unsigned _LINE_
                          , const unsigned SIZE )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;

  if( m_line_cache.len() )
  {
    bool ok = m_line_cache.pop( lp );
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
Line* Vis::Imp::BorrowLine( const char*    _FILE_
                          , const unsigned _LINE_
                          , const unsigned LEN
                          , const uint8_t  FILL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;

  if( m_line_cache.len() )
  {
    bool ok = m_line_cache.pop( lp );
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
Line* Vis::Imp::BorrowLine( const char*    _FILE_
                          , const unsigned _LINE_
                          , const Line&    line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;

  if( m_line_cache.len() )
  {
    bool ok = m_line_cache.pop( lp );
    ASSERT( __LINE__, ok, "ok" );

    ok = lp->copy( line );
    ASSERT( __LINE__, ok, "ok" );
  }
  else {
    lp = new(_FILE_,_LINE_) Line( line );
  }
  return lp;
}

void Vis::Imp::ReturnLine( Line* lp )
{
  if( lp ) m_line_cache.push( lp );
}

LineChange* Vis::Imp::BorrowLineChange( const ChangeType type
                                      , const unsigned   lnum
                                      , const unsigned   cpos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  LineChange* lcp = 0;

  if( m_change_cache.len() )
  {
    bool ok = m_change_cache.pop( lcp );

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

void Vis::Imp::ReturnLineChange( LineChange* lcp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( lcp ) m_change_cache.push( lcp );
}

void Vis::Imp::Ch_Dir()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char* path = 0;

  // 1. First get path to chdir to:
  if( 0 != m_cbuf[2] ) // :cd relative_path
  {
    path = m_cbuf + 2;
  }
  else // :cd - chdir to location of current file
  {
    const char* fname = CV()->GetFB()->GetFileName();
    const char* last_slash = strrchr( fname, DIR_DELIM );
    if( 0==last_slash )
    {
      path = fname;
    }
    else {
      // Put everything in fname except last slash into m_sbuf:
      m_sbuf.clear(); // m_sbuf is a general purpose string buffer
      for( const char* cp = fname; cp < last_slash; cp++ ) m_sbuf.push( *cp );
      path = m_sbuf.c_str();
    }
  }

  // 2. chdir
  int err = chdir( path );
  if( err )
  {
    m_vis.CmdLineMessage( "chdir(%s) failed", path );
    CV()->PrintCursor();
  }
  else {
    GetCWD();
  }
}

void Vis::Imp::GetCWD()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned FILE_NAME_LEN = 1024;
  char cwd[ FILE_NAME_LEN ];

  if( ! getcwd( cwd, FILE_NAME_LEN ) )
  {
    m_vis.CmdLineMessage( "getcwd() failed" );
  }
  else {
    m_vis.CmdLineMessage( "%s", cwd );
  }
  CV()->PrintCursor();
}

void Vis::Imp::Set_Syntax()
{
  const char* syn = strchr( m_cbuf, '=' );

  if( NULL != syn )
  {
    // Move past '='
    syn++;
    if( 0 != *syn )
    {
      // Something after the '=' 
      CV()->GetFB()->Set_File_Type( syn );
    }
  }
}

void Vis::Imp::AdjustViews()
{
  for( unsigned w=0; w<m_num_wins; w++ )
  {
    m_views[w][ m_file_hist[w][0] ]->SetViewPos();
  }
}

void Vis::Imp::InitBufferEditor()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m_files in Add_FileBuf_2_Lists_Create_Views()
  // Buffer editor, 0
  FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( m_vis, EDIT_BUF_NAME, false, FT_BUFFER_EDITOR );
}

void Vis::Imp::InitHelpBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m_files in Add_FileBuf_2_Lists_Create_Views()
  // Help buffer, 1
  FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( m_vis, HELP_BUF_NAME, false, FT_TEXT );
}

void Vis::Imp::InitSearchEditor()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m_files in Add_FileBuf_2_Lists_Create_Views()
  // Search editor buffer, 2
  FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( m_vis, SRCH_BUF_NAME, false, FT_TEXT );
}

void Vis::Imp::InitMsgBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m_files in Add_FileBuf_2_Lists_Create_Views()
  // Message buffer, 3
  FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( m_vis, MSG__BUF_NAME, false, FT_TEXT );
}

void Vis::Imp::InitCmdBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // pfb gets added to m_files in Add_FileBuf_2_Lists_Create_Views()
  // Command buffer, CMD_FILE(4)
  FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( m_vis, CMD__BUF_NAME, false, FT_TEXT );
  pfb->PushLine(); // Add an empty line
}

bool Vis::Imp::InitUserFiles( const int ARGC, const char* const ARGV[] )
{
  bool run_diff = false;

  // Save original working directory from which to open all user supplied files names
  const unsigned FILE_NAME_LEN = 1024;
  char orig_dir[ FILE_NAME_LEN ];
  bool got_orig_dir = !! getcwd( orig_dir, FILE_NAME_LEN );

  // User file buffers, 5, 6, ...
  for( int k=1; k<ARGC; k++ )
  {
    if( strcmp( "-d", ARGV[k] ) == 0 )
    {
      run_diff = true;
    }
    else {
      String file_name = ARGV[k];
      if( FindFullFileName( file_name ) )
      {
        if( !HaveFile( file_name.c_str() ) )
        {
          FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( m_vis, file_name.c_str(), true, FT_UNKNOWN );
          pfb->ReadFile();
        }
        // Restore original directory, for next call to FindFullFileName()
        if( got_orig_dir ) chdir( orig_dir );
      }
    }
  }
  return run_diff;
}

void Vis::Imp::InitFileHistory()
{
  for( int w=0; w<MAX_WINS; w++ )
  {
    m_file_hist[w].push( __FILE__,__LINE__, BE_FILE );
    m_file_hist[w].push( __FILE__,__LINE__, HELP_FILE );

    if( 5<m_views[w].len() )
    {
      m_file_hist[w].insert( __FILE__,__LINE__, 0, 5 );

      for( int f=m_views[w].len()-1; 6<=f; f-- )
      {
        m_file_hist[w].push( __FILE__,__LINE__, f );
      }
    }
  }
}

void Vis::Imp::InitCmdFuncs()
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned k=0; k<128; k++ ) CmdFuncs[k] = 0;

  CmdFuncs[ 'i' ] = &Imp::Handle_i;
  CmdFuncs[ 'v' ] = &Imp::Handle_v;
  CmdFuncs[ 'V' ] = &Imp::Handle_V;
  CmdFuncs[ 'a' ] = &Imp::Handle_a;
  CmdFuncs[ 'A' ] = &Imp::Handle_A;
  CmdFuncs[ 'o' ] = &Imp::Handle_o;
  CmdFuncs[ 'O' ] = &Imp::Handle_O;
  CmdFuncs[ 'x' ] = &Imp::Handle_x;
  CmdFuncs[ 's' ] = &Imp::Handle_s;
  CmdFuncs[ 'c' ] = &Imp::Handle_c;
  CmdFuncs[ 'Q' ] = &Imp::Handle_Q;
  CmdFuncs[ 'k' ] = &Imp::Handle_k;
  CmdFuncs[ 'j' ] = &Imp::Handle_j;
  CmdFuncs[ 'h' ] = &Imp::Handle_h;
  CmdFuncs[ 'l' ] = &Imp::Handle_l;
  CmdFuncs[ 'H' ] = &Imp::Handle_H;
  CmdFuncs[ 'L' ] = &Imp::Handle_L;
  CmdFuncs[ 'M' ] = &Imp::Handle_M;
  CmdFuncs[ '0' ] = &Imp::Handle_0;
  CmdFuncs[ '$' ] = &Imp::Handle_Dollar;
  CmdFuncs[ '\n'] = &Imp::Handle_Return;
  CmdFuncs[ 'G' ] = &Imp::Handle_G;
  CmdFuncs[ 'b' ] = &Imp::Handle_b;
  CmdFuncs[ 'w' ] = &Imp::Handle_w;
  CmdFuncs[ 'e' ] = &Imp::Handle_e;
  CmdFuncs[ 'f' ] = &Imp::Handle_f;
  CmdFuncs[ ';' ] = &Imp::Handle_SemiColon;
  CmdFuncs[ '%' ] = &Imp::Handle_Percent;
  CmdFuncs[ '{' ] = &Imp::Handle_LeftSquigglyBracket;
  CmdFuncs[ '}' ] = &Imp::Handle_RightSquigglyBracket;
  CmdFuncs[ 'F' ] = &Imp::Handle_F;
  CmdFuncs[ 'B' ] = &Imp::Handle_B;
  CmdFuncs[ ':' ] = &Imp::Handle_Colon;
  CmdFuncs[ '/' ] = &Imp::Handle_Slash; // Crashes
  CmdFuncs[ '*' ] = &Imp::Handle_Star;
  CmdFuncs[ '.' ] = &Imp::Handle_Dot;
  CmdFuncs[ 'm' ] = &Imp::Handle_m;
  CmdFuncs[ 'g' ] = &Imp::Handle_g;
  CmdFuncs[ 'W' ] = &Imp::Handle_W;
  CmdFuncs[ 'd' ] = &Imp::Handle_d;
  CmdFuncs[ 'y' ] = &Imp::Handle_y;
  CmdFuncs[ 'D' ] = &Imp::Handle_D;
  CmdFuncs[ 'p' ] = &Imp::Handle_p;
  CmdFuncs[ 'P' ] = &Imp::Handle_P;
  CmdFuncs[ 'R' ] = &Imp::Handle_R;
  CmdFuncs[ 'J' ] = &Imp::Handle_J;
  CmdFuncs[ '~' ] = &Imp::Handle_Tilda;
  CmdFuncs[ 'n' ] = &Imp::Handle_n;
  CmdFuncs[ 'N' ] = &Imp::Handle_N;
  CmdFuncs[ 'u' ] = &Imp::Handle_u;
  CmdFuncs[ 'U' ] = &Imp::Handle_U;
  CmdFuncs[ 'z' ] = &Imp::Handle_z;
}

void Vis::Imp::AddToBufferEditor( const char* fname )
{
  Trace trace( __PRETTY_FUNCTION__ );
  Line line(__FILE__, __LINE__, strlen( fname ) );
//unsigned NUM_BUFFERS = m_views[0].len();
//char str[32]; 
//sprintf( str, "%3u ", NUM_BUFFERS-1 );
//for( const char* p=str  ; *p; p++ ) line.push(__FILE__,__LINE__, *p );
  for( const char* p=fname; *p; p++ ) line.push(__FILE__,__LINE__, *p );
  FileBuf* pfb = m_views[0][ BE_FILE ]->GetFB();
  pfb->PushLine( line );
  pfb->BufferEditor_Sort();
  pfb->ClearChanged();

  // Since buffer editor file has been re-arranged, make sure none of its
  // views have the cursor position past the end of the line
  for( unsigned k=0; k<MAX_WINS; k++ )
  {
    View* pV = m_views[k][ BE_FILE ];

    unsigned CL = pV->CrsLine();
    unsigned CP = pV->CrsChar();
    unsigned LL = pfb->LineLen( CL );

    if( LL <= CP )
    {
      pV->GoToCrsPos_NoWrite( CL, LL-1 );
    }
  }
}

bool Vis::Imp::File_Is_Displayed( const unsigned file_num )
{
  for( unsigned w=0; w<m_num_wins; w++ )
  {
    if( file_num == m_file_hist[ w ][ 0 ] )
    {
      return true;
    }
  }
  return false;
}

void Vis::Imp::Quit()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const Tile_Pos TP = CV()->GetTilePos();

  if( m_num_wins <= 1 ) QuitAll();
  else {
    if( m_win < m_num_wins-1 )
    {
      Quit_ShiftDown();
    }
    if( 0 < m_win ) m_win--;
    m_num_wins--;

    Quit_JoinTiles( TP );

    UpdateAll();

    CV()->PrintCursor();
  }
}

void Vis::Imp::QuitAll()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Move cursor down to bottom of screen:
  Console::Move_2_Row_Col( Console::Num_Rows()-1, 0 );

  // Put curson on a new line:
  Console::NewLine();

  m_running = false;
}

void Vis::Imp::Quit_ShiftDown()
{
  // Make copy of win's list of views and view history:
  ViewList win_views    ( m_views    [m_win] );
   unsList win_view_hist( m_file_hist[m_win] );

  // Shift everything down
  for( unsigned w=m_win+1; w<m_num_wins; w++ )
  {
    m_views    [w-1] = m_views    [w];
    m_file_hist[w-1] = m_file_hist[w];
  }
  // Put win's list of views at end of views:
  // Put win's view history at end of view historys:
  m_views    [m_num_wins-1] = win_views;
  m_file_hist[m_num_wins-1] = win_view_hist;
}

void Vis::Imp::Quit_JoinTiles( const Tile_Pos TP )
{
  Trace trace( __PRETTY_FUNCTION__ );
  // win is disappearing, so move its screen space to another view:
  if     ( TP == TP_LEFT_HALF )         Quit_JoinTiles_TP_LEFT_HALF();
  else if( TP == TP_RITE_HALF )         Quit_JoinTiles_TP_RITE_HALF();
  else if( TP == TP_TOP__HALF )         Quit_JoinTiles_TP_TOP__HALF();
  else if( TP == TP_BOT__HALF )         Quit_JoinTiles_TP_BOT__HALF();
  else if( TP == TP_TOP__LEFT_QTR )     Quit_JoinTiles_TP_TOP__LEFT_QTR();
  else if( TP == TP_TOP__RITE_QTR )     Quit_JoinTiles_TP_TOP__RITE_QTR();
  else if( TP == TP_BOT__LEFT_QTR )     Quit_JoinTiles_TP_BOT__LEFT_QTR();
  else if( TP == TP_BOT__RITE_QTR )     Quit_JoinTiles_TP_BOT__RITE_QTR();
  else if( TP == TP_LEFT_QTR )          Quit_JoinTiles_TP_LEFT_QTR();
  else if( TP == TP_RITE_QTR )          Quit_JoinTiles_TP_RITE_QTR();
  else if( TP == TP_LEFT_CTR__QTR )     Quit_JoinTiles_TP_LEFT_CTR__QTR();
  else if( TP == TP_RITE_CTR__QTR )     Quit_JoinTiles_TP_RITE_CTR__QTR();
  else if( TP == TP_TOP__LEFT_8TH )     Quit_JoinTiles_TP_TOP__LEFT_8TH();
  else if( TP == TP_TOP__RITE_8TH )     Quit_JoinTiles_TP_TOP__RITE_8TH();
  else if( TP == TP_TOP__LEFT_CTR_8TH ) Quit_JoinTiles_TP_TOP__LEFT_CTR_8TH();
  else if( TP == TP_TOP__RITE_CTR_8TH ) Quit_JoinTiles_TP_TOP__RITE_CTR_8TH();
  else if( TP == TP_BOT__LEFT_8TH )     Quit_JoinTiles_TP_BOT__LEFT_8TH();
  else if( TP == TP_BOT__RITE_8TH )     Quit_JoinTiles_TP_BOT__RITE_8TH();
  else if( TP == TP_BOT__LEFT_CTR_8TH ) Quit_JoinTiles_TP_BOT__LEFT_CTR_8TH();
  else /*( TP == TP_BOT__RITE_CTR_8TH*/ Quit_JoinTiles_TP_BOT__RITE_CTR_8TH();
}

void Vis::Imp::Quit_JoinTiles_TP_LEFT_HALF()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
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

void Vis::Imp::Quit_JoinTiles_TP_RITE_HALF()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
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

void Vis::Imp::Quit_JoinTiles_TP_TOP__HALF()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
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

void Vis::Imp::Quit_JoinTiles_TP_BOT__HALF()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
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

void Vis::Imp::Quit_JoinTiles_TP_TOP__LEFT_QTR()
{
  if( Have_TP_BOT__HALF() )
  {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_TOP__RITE_QTR     ) { v->SetTilePos( TP_TOP__HALF ); break; }
      else if( TP == TP_TOP__RITE_8TH     ) v->SetTilePos( TP_TOP__RITE_QTR );
      else if( TP == TP_TOP__RITE_CTR_8TH ) v->SetTilePos( TP_TOP__LEFT_QTR );
    }
  }
  else {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_BOT__LEFT_QTR     ) { v->SetTilePos( TP_LEFT_HALF ); break; }
      else if( TP == TP_BOT__LEFT_8TH     ) v->SetTilePos( TP_LEFT_QTR );
      else if( TP == TP_BOT__LEFT_CTR_8TH ) v->SetTilePos( TP_LEFT_CTR__QTR );
    }
  }
}

void Vis::Imp::Quit_JoinTiles_TP_TOP__RITE_QTR()
{
  if( Have_TP_BOT__HALF() )
  {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_TOP__LEFT_QTR     ) { v->SetTilePos( TP_TOP__HALF ); break; }
      else if( TP == TP_TOP__LEFT_8TH     ) v->SetTilePos( TP_TOP__LEFT_QTR );
      else if( TP == TP_TOP__LEFT_CTR_8TH ) v->SetTilePos( TP_TOP__RITE_QTR );
    }
  }
  else {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_BOT__RITE_QTR     ) { v->SetTilePos( TP_RITE_HALF ); break; }
      else if( TP == TP_BOT__RITE_8TH     ) v->SetTilePos( TP_RITE_QTR );
      else if( TP == TP_BOT__RITE_CTR_8TH ) v->SetTilePos( TP_RITE_CTR__QTR );
    }
  }
}

void Vis::Imp::Quit_JoinTiles_TP_BOT__LEFT_QTR()
{
  if( Have_TP_TOP__HALF() )
  {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_BOT__RITE_QTR     ) { v->SetTilePos( TP_BOT__HALF ); break; }
      else if( TP == TP_BOT__RITE_8TH     ) v->SetTilePos( TP_BOT__RITE_QTR );
      else if( TP == TP_BOT__RITE_CTR_8TH ) v->SetTilePos( TP_BOT__LEFT_QTR );
    }
  }
  else {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_TOP__LEFT_QTR     ) { v->SetTilePos( TP_LEFT_HALF ); break; }
      else if( TP == TP_TOP__LEFT_8TH     ) v->SetTilePos( TP_LEFT_QTR );
      else if( TP == TP_TOP__LEFT_CTR_8TH ) v->SetTilePos( TP_LEFT_CTR__QTR );
    }
  }
}

void Vis::Imp::Quit_JoinTiles_TP_BOT__RITE_QTR()
{
  if( Have_TP_TOP__HALF() )
  {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_BOT__LEFT_QTR     ) { v->SetTilePos( TP_BOT__HALF ); break; }
      else if( TP == TP_BOT__LEFT_8TH     ) v->SetTilePos( TP_BOT__LEFT_QTR );
      else if( TP == TP_BOT__LEFT_CTR_8TH ) v->SetTilePos( TP_BOT__RITE_QTR );
    }
  }
  else {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if     ( TP == TP_TOP__RITE_QTR     ) { v->SetTilePos( TP_RITE_HALF ); break; }
      else if( TP == TP_TOP__RITE_8TH     ) v->SetTilePos( TP_RITE_QTR );
      else if( TP == TP_TOP__RITE_CTR_8TH ) v->SetTilePos( TP_RITE_CTR__QTR );
    }
  }
}

void Vis::Imp::Quit_JoinTiles_TP_LEFT_QTR()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_LEFT_CTR__QTR     ) { v->SetTilePos( TP_LEFT_HALF ); break; }
    else if( TP == TP_TOP__LEFT_CTR_8TH ) v->SetTilePos( TP_TOP__LEFT_QTR );
    else if( TP == TP_BOT__LEFT_CTR_8TH ) v->SetTilePos( TP_BOT__LEFT_QTR );
  }
}

void Vis::Imp::Quit_JoinTiles_TP_RITE_QTR()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_RITE_CTR__QTR     ) { v->SetTilePos( TP_RITE_HALF ); break; }
    else if( TP == TP_TOP__RITE_CTR_8TH ) v->SetTilePos( TP_TOP__RITE_QTR );
    else if( TP == TP_BOT__RITE_CTR_8TH ) v->SetTilePos( TP_BOT__RITE_QTR );
  }
}

void Vis::Imp::Quit_JoinTiles_TP_LEFT_CTR__QTR()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_LEFT_QTR      ) { v->SetTilePos( TP_LEFT_HALF ); break; }
    else if( TP == TP_TOP__LEFT_8TH ) v->SetTilePos( TP_TOP__LEFT_QTR );
    else if( TP == TP_BOT__LEFT_8TH ) v->SetTilePos( TP_BOT__LEFT_QTR );
  }
}

void Vis::Imp::Quit_JoinTiles_TP_RITE_CTR__QTR()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if     ( TP == TP_RITE_QTR      ) { v->SetTilePos( TP_RITE_HALF ); break; }
    else if( TP == TP_TOP__RITE_8TH ) v->SetTilePos( TP_TOP__RITE_QTR );
    else if( TP == TP_BOT__RITE_8TH ) v->SetTilePos( TP_BOT__RITE_QTR );
  }
}

void Vis::Imp::Quit_JoinTiles_TP_TOP__LEFT_8TH()
{
  if( Have_TP_BOT__LEFT_QTR() )
  {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__LEFT_CTR_8TH ) { v->SetTilePos( TP_TOP__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__LEFT_8TH ) { v->SetTilePos( TP_LEFT_QTR ); break; }
    }
  }
}

void Vis::Imp::Quit_JoinTiles_TP_TOP__RITE_8TH()
{
  if( Have_TP_BOT__RITE_QTR() )
  {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__RITE_CTR_8TH ) { v->SetTilePos( TP_TOP__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__RITE_8TH ) { v->SetTilePos( TP_RITE_QTR ); break; }
    }
  }
}

void Vis::Imp::Quit_JoinTiles_TP_TOP__LEFT_CTR_8TH()
{
  if( Have_TP_BOT__LEFT_QTR() )
  {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__LEFT_8TH ) { v->SetTilePos( TP_TOP__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__LEFT_CTR_8TH ) { v->SetTilePos( TP_LEFT_CTR__QTR ); break; }
    }
  }
}

void Vis::Imp::Quit_JoinTiles_TP_TOP__RITE_CTR_8TH()
{
  if( Have_TP_BOT__RITE_QTR() )
  {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__RITE_8TH ) { v->SetTilePos( TP_TOP__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__RITE_CTR_8TH ) { v->SetTilePos( TP_RITE_CTR__QTR ); break; }
    }
  }
}

void Vis::Imp::Quit_JoinTiles_TP_BOT__LEFT_8TH()
{
  if( Have_TP_TOP__LEFT_QTR() )
  {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__LEFT_CTR_8TH ) { v->SetTilePos( TP_BOT__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__LEFT_8TH ) { v->SetTilePos( TP_LEFT_QTR ); break; }
    }
  }
}

void Vis::Imp::Quit_JoinTiles_TP_BOT__RITE_8TH()
{
  if( Have_TP_TOP__RITE_QTR() )
  {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__RITE_CTR_8TH ) { v->SetTilePos( TP_BOT__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__RITE_8TH ) { v->SetTilePos( TP_RITE_QTR ); break; }
    }
  }
}

void Vis::Imp::Quit_JoinTiles_TP_BOT__LEFT_CTR_8TH()
{
  if( Have_TP_TOP__LEFT_QTR() )
  {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__LEFT_8TH ) { v->SetTilePos( TP_BOT__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__LEFT_CTR_8TH ) { v->SetTilePos( TP_LEFT_CTR__QTR ); break; }
    }
  }
}

void Vis::Imp::Quit_JoinTiles_TP_BOT__RITE_CTR_8TH()
{
  if( Have_TP_TOP__RITE_QTR() )
  {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_BOT__RITE_8TH ) { v->SetTilePos( TP_BOT__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<m_num_wins; k++ )
    {
      View* v = m_views[k][ m_file_hist[k][0] ];
      const Tile_Pos TP = v->GetTilePos();

      if( TP == TP_TOP__RITE_CTR_8TH ) { v->SetTilePos( TP_RITE_CTR__QTR ); break; }
    }
  }
}

bool Vis::Imp::Have_TP_BOT__HALF()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_BOT__HALF ) return true;
  }
  return false;
}

bool Vis::Imp::Have_TP_TOP__HALF()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_TOP__HALF ) return true;
  }
  return false;
}

bool Vis::Imp::Have_TP_BOT__LEFT_QTR()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_BOT__LEFT_QTR ) return true;
  }
  return false;
}

bool Vis::Imp::Have_TP_TOP__LEFT_QTR()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_TOP__LEFT_QTR ) return true;
  }
  return false;
}

bool Vis::Imp::Have_TP_BOT__RITE_QTR()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_BOT__RITE_QTR ) return true;
  }
  return false;
}

bool Vis::Imp::Have_TP_TOP__RITE_QTR()
{
  for( unsigned k=0; k<m_num_wins; k++ )
  {
    View* v = m_views[k][ m_file_hist[k][0] ];
    const Tile_Pos TP = v->GetTilePos();

    if( TP == TP_TOP__RITE_QTR ) return true;
  }
  return false;
}

void Vis::Imp::Help()
{
  GoToBuffer( HELP_FILE );
}

void Vis::Imp::DoDiff()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Must be exactly 2 buffers to do diff:
  if( 2 == m_num_wins )
  {
    View* pv0 = m_views[0][ m_file_hist[0][0] ];
    View* pv1 = m_views[1][ m_file_hist[1][0] ];
    FileBuf* pfb0 = pv0->GetFB();
    FileBuf* pfb1 = pv1->GetFB();

    // New code in progress:
    bool ok = true;
    if( !pfb0->IsDir() && pfb1->IsDir() )
    {
      pv1 = DoDiff_FindRegFileView( pfb0, pfb1, 1, pv1 );
    }
    else if( pfb0->IsDir() && !pfb1->IsDir() )
    {
      pv0 = DoDiff_FindRegFileView( pfb1, pfb0, 0, pv0 );
    }
    else {
      if( !FileExists( pfb0->GetFileName() ) )
      {
        ok = false;
        Log.Log("\n%s does not exist\n", pfb0->GetFileName() );
      }
      else if( !FileExists( pfb1->GetFileName() ) )
      {
        ok = false;
        Log.Log("\n%s does not exist\n", pfb1->GetFileName() );
      }
    }
    if( !ok ) m_running = false;
    else {
#ifndef WIN32
      timeval tv1; gettimeofday( &tv1, 0 );
#endif
      bool ok = m_diff.Run( pv0, pv1 );
      if( ok ) {
        m_diff_mode = true;

#ifndef WIN32
        timeval tv2; gettimeofday( &tv2, 0 );

        double secs = (tv2.tv_sec-tv1.tv_sec)
                    + double(tv2.tv_usec)/1e6
                    - double(tv1.tv_usec)/1e6;
        m_vis.CmdLineMessage( "Diff took: %g seconds", secs );
#endif
      }
    }
  }
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

View* Vis::Imp::DoDiff_FindRegFileView( const FileBuf* pfb_reg
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

    View* nv = DoDiff_CheckPossibleFile( win_idx, pos_fname );

    if( 0 != nv ) return nv;
  }
  return pv;
}

// If file is found, puts View of file in win_idx window,
// and returns the View, else returns null
View* Vis::Imp::DoDiff_CheckPossibleFile( const int win_idx
                                        , const char* pos_fname )
{
  struct stat sbuf;
  int err = my_stat( pos_fname, sbuf );

  if( 0 == err )
  {
    // File exists, find or create FileBuf, and set second view to display that file:
    if( !HaveFile( pos_fname ) )
    {
      FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( m_vis, pos_fname, true, FT_UNKNOWN );
      pfb->ReadFile();
    }
  }
  unsigned file_index = 0;
  if( HaveFile( pos_fname, &file_index ) )
  {
    SetWinToBuffer( win_idx, file_index, false );

    return m_views[win_idx][ m_file_hist[win_idx][0] ];
  }
  return 0;
}

void Vis::Imp::NoDiff()
{
  if( true == m_diff_mode )
  {
    m_diff_mode = false;

    UpdateAll();
  }
}

void Vis::Imp::SetWinToBuffer( const unsigned win_idx
                             , const unsigned buf_idx
                             , const bool     update )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_views[win_idx].len() <= buf_idx )
  {
    m_vis.CmdLineMessage( "Buffer %lu does not exist", buf_idx );
  }
  else {
    if( buf_idx == m_file_hist[win_idx][0] )
    {
      // User asked for view that is currently displayed in win_idx.
      // Dont do anything.
    }
    else {
      m_file_hist[win_idx].insert(__FILE__,__LINE__, 0, buf_idx );

      // Remove subsequent buf_idx's from m_file_hist[win_idx]:
      for( unsigned k=1; k<m_file_hist[win_idx].len(); k++ )
      {
        if( buf_idx == m_file_hist[win_idx][k] ) m_file_hist[win_idx].remove( k );
      }
      View* pV_curr = m_views[win_idx][ m_file_hist[win_idx][0] ];
      View* pV_prev = m_views[win_idx][ m_file_hist[win_idx][1] ];

                   pV_curr->SetTilePos( pV_prev->GetTilePos() );
      if( update ) pV_curr->Update();
    }
  }
}

// 1. Get filename underneath the cursor
// 2. Search for filename in buffer list, and if found, go to that buffer
// 3. If not found, print a command line message
//
void Vis::Imp::GoToFile()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // 1. Get fname underneath the cursor:
  String fname;
  bool ok = CV()->GoToFile_GetFileName( fname );

  if( ok ) GoToBuffer_Fname( fname );
}

void Vis::Imp::GoToBuffer( const unsigned buf_idx )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_views[m_win].len() <= buf_idx )
  {
    m_vis.CmdLineMessage( "Buffer %lu does not exist", buf_idx );
  }
  else {
    if( buf_idx == m_file_hist[m_win][0] )
    {
      // User asked for view that is currently displayed.
      // Dont do anything, just put cursor back in place.
      CV()->PrintCursor();
    }
    else {
      m_file_hist[m_win].insert(__FILE__,__LINE__, 0, buf_idx );

      // Remove subsequent buf_idx's from m_file_hist[m_win]:
      for( unsigned k=1; k<m_file_hist[m_win].len(); k++ )
      {
        if( buf_idx == m_file_hist[m_win][k] ) m_file_hist[m_win].remove( k );
      }
      View* nv = CV(); // New View to display
      if( ! nv->Has_Context() )
      {
        // Look for context for the new view:
        bool found_context = false;
        for( unsigned w=0; !found_context && w<m_num_wins; w++ )
        {
          View* v = m_views[w][ buf_idx ];
          if( v->Has_Context() )
          {
            found_context = true;

            nv->Set_Context( *v );
          }
        }
      }
      nv->SetTilePos( PV()->GetTilePos() );
      nv->Update();
    }
  }
}

void Vis::Imp::GoToNextBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned FILE_HIST_LEN = m_file_hist[m_win].len();

  if( FILE_HIST_LEN <= 1 )
  {
    // Nothing to do, so just put cursor back
    CV()->PrintCursor();
  }
  else {
    View*    const pV_old = CV();
    Tile_Pos const tp_old = pV_old->GetTilePos();

    // Move view index at back to front of m_file_hist
    unsigned view_index_new = 0;
    m_file_hist[m_win].pop( view_index_new );
    m_file_hist[m_win].insert(__FILE__,__LINE__, 0, view_index_new );

    // Redisplay current window with new view:
    CV()->SetTilePos( tp_old );
    CV()->Update();
  }
}

void Vis::Imp::GoToCurrBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // CVI = Current View Index
  const unsigned CVI = m_file_hist[m_win][0];

  if( CVI == BE_FILE
   || CVI == HELP_FILE
   || CVI == SE_FILE )  
  {
    GoToBuffer( m_file_hist[m_win][1] );
  }
  else {
    // User asked for view that is currently displayed.
    // Dont do anything, just put cursor back in place.
    CV()->PrintCursor();
  }
}

void Vis::Imp::GoToPrevBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned FILE_HIST_LEN = m_file_hist[m_win].len();

  if( FILE_HIST_LEN <= 1 )
  {
    // Nothing to do, so just put cursor back
    CV()->PrintCursor();
  }
  else {
    View*    const pV_old = CV();
    Tile_Pos const tp_old = pV_old->GetTilePos();

    // Move view index at front to back of m_file_hist
    unsigned view_index_old = 0;
    m_file_hist[m_win].remove( 0, view_index_old );
    m_file_hist[m_win].push(__FILE__,__LINE__, view_index_old );

    // Redisplay current window with new view:
    CV()->SetTilePos( tp_old );
    CV()->Update();
  }
}

void Vis::Imp::GoToPoundBuffer()
{
  if( BE_FILE == m_file_hist[m_win][1] )
  {
    GoToBuffer( m_file_hist[m_win][2] );
  }
  else GoToBuffer( m_file_hist[m_win][1] );
}

void Vis::Imp::GoToBufferEditor()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToBuffer( BE_FILE );
}

void Vis::Imp::GoToMsgBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToBuffer( MSG_FILE );
}

void Vis::Imp::GoToCmdBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToBuffer( CMD_FILE );
}

void Vis::Imp::GoToSearchBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToBuffer( SE_FILE );
}

void Vis::Imp::GoToNextWindow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < m_num_wins )
  {
    const unsigned win_old = m_win;

    m_win = (++m_win) % m_num_wins;

    View* pV     = m_views[m_win  ][ m_file_hist[m_win  ][0] ];
    View* pV_old = m_views[win_old][ m_file_hist[win_old][0] ];

    pV_old->Print_Borders();
    pV    ->Print_Borders();

    Console::Update();

    m_diff_mode ? m_diff.PrintCursor( pV ) : pV->PrintCursor();
  }
}

void Vis::Imp::GoToNextWindow_l()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < m_num_wins )
  {
    const unsigned win_old = m_win;

    // If next view to go to was not found, dont do anything, just return
    // If next view to go to is found, m_win will be updated to new value
    if( GoToNextWindow_l_Find() )
    {
      View* pV     = m_views[m_win  ][ m_file_hist[m_win  ][0] ];
      View* pV_old = m_views[win_old][ m_file_hist[win_old][0] ];

      pV_old->Print_Borders();
      pV    ->Print_Borders();

      Console::Update();

      m_diff_mode ? m_diff.PrintCursor( pV ) : pV->PrintCursor();
    }
  }
}

bool Vis::Imp::GoToNextWindow_l_Find()
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found = false; // Found next view to go to

  const View*    curr_V  = m_views[m_win][ m_file_hist[m_win][0] ];
  const Tile_Pos curr_TP = curr_V->GetTilePos();

  if( curr_TP == TP_LEFT_HALF )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF         == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_HALF )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF     == TP
       || TP_TOP__LEFT_QTR == TP
       || TP_BOT__LEFT_QTR == TP
       || TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP
       || TP_BOT__LEFT_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF         == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_QTR == TP
       || TP_TOP__LEFT_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF         == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_BOT__LEFT_QTR == TP
       || TP_BOT__LEFT_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP
       || TP_BOT__LEFT_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_LEFT_CTR__QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF         == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_CTR__QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP
       || TP_BOT__RITE_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_CTR__QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF         == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF         == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_QTR      == TP
       || TP_BOT__RITE_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_QTR == TP
       || TP_TOP__LEFT_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_BOT__LEFT_QTR == TP
       || TP_BOT__LEFT_8TH == TP ) { m_win = k; found = true; }
    }
  }
  return found;
}

void Vis::Imp::GoToNextWindow_h()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < m_num_wins )
  {
    const unsigned win_old = m_win;

    // If next view to go to was not found, dont do anything, just return
    // If next view to go to is found, m_win will be updated to new value
    if( GoToNextWindow_h_Find() )
    {
      View* pV     = m_views[m_win  ][ m_file_hist[m_win  ][0] ];
      View* pV_old = m_views[win_old][ m_file_hist[win_old][0] ];

      pV_old->Print_Borders();
      pV    ->Print_Borders();

      Console::Update();

      m_diff_mode ? m_diff.PrintCursor( pV ) : pV->PrintCursor();
    }
  }
}

bool Vis::Imp::GoToNextWindow_h_Find()
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found = false; // Found next view to go to

  const View*    curr_V  = m_views[m_win][ m_file_hist[m_win][0] ];
  const Tile_Pos curr_TP = curr_V->GetTilePos();

  if( curr_TP == TP_LEFT_HALF )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF     == TP
       || TP_TOP__RITE_QTR == TP
       || TP_BOT__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP
       || TP_BOT__RITE_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_HALF )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF     == TP
       || TP_TOP__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF         == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF     == TP
       || TP_BOT__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_BOT__RITE_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF         == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF     == TP
       || TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP
       || TP_BOT__RITE_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_LEFT_CTR__QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP
       || TP_BOT__LEFT_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_CTR__QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF         == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF     == TP
       || TP_TOP__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_HALF     == TP
       || TP_BOT__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_BOT__RITE_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_QTR      == TP
       || TP_BOT__LEFT_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_LEFT_HALF         == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_CTR__QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_RITE_CTR__QTR     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  return found;
}

void Vis::Imp::GoToNextWindow_jk()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < m_num_wins )
  {
    const unsigned win_old = m_win;

    // If next view to go to was not found, dont do anything, just return
    // If next view to go to is found, m_win will be updated to new value
    if( GoToNextWindow_jk_Find() )
    {
      View* pV     = m_views[m_win  ][ m_file_hist[m_win  ][0] ];
      View* pV_old = m_views[win_old][ m_file_hist[win_old][0] ];

      pV_old->Print_Borders();
      pV    ->Print_Borders();

      Console::Update();

      m_diff_mode ? m_diff.PrintCursor( pV ) : pV->PrintCursor();
    }
  }
}

bool Vis::Imp::GoToNextWindow_jk_Find()
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found = false; // Found next view to go to

  const View*    curr_V  = m_views[m_win][ m_file_hist[m_win][0] ];
  const Tile_Pos curr_TP = curr_V->GetTilePos();

  if( curr_TP == TP_TOP__HALF )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF         == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_BOT__LEFT_8TH     == TP
       || TP_BOT__RITE_8TH     == TP
       || TP_BOT__LEFT_CTR_8TH == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__HALF )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_TOP__LEFT_8TH     == TP
       || TP_TOP__RITE_8TH     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF         == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_BOT__LEFT_8TH     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF         == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_BOT__RITE_8TH     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_TOP__LEFT_8TH     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_QTR )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF         == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_TOP__RITE_8TH     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF     == TP
       || TP_BOT__LEFT_QTR == TP
       || TP_BOT__LEFT_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF     == TP
       || TP_BOT__RITE_QTR == TP
       || TP_BOT__RITE_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF         == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_BOT__HALF         == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF     == TP
       || TP_TOP__LEFT_QTR == TP
       || TP_TOP__LEFT_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF     == TP
       || TP_TOP__RITE_QTR == TP
       || TP_TOP__RITE_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<m_num_wins; k++ )
    {
      const Tile_Pos TP = (m_views[k][ m_file_hist[k][0] ])->GetTilePos();
      if( TP_TOP__HALF         == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { m_win = k; found = true; }
    }
  }
  return found;
}

void Vis::Imp::VSplitWindow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV();
  const Tile_Pos cv_tp = cv->GetTilePos();

  if( m_num_wins < MAX_WINS 
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
    ASSERT( __LINE__, m_win < m_num_wins, "m_win < m_num_wins" );

    m_file_hist[m_num_wins] = m_file_hist[m_win];

    View* nv = m_views[m_num_wins][ m_file_hist[m_num_wins][0] ];

    nv->SetTopLine ( cv->GetTopLine () );
    nv->SetLeftChar( cv->GetLeftChar() );
    nv->SetCrsRow  ( cv->GetCrsRow  () );
    nv->SetCrsCol  ( cv->GetCrsCol  () );

    m_win = m_num_wins;
    m_num_wins++;

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
  UpdateAll();
}

void Vis::Imp::HSplitWindow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV();
  const Tile_Pos cv_tp = cv->GetTilePos();

  if( m_num_wins < MAX_WINS 
   && ( cv_tp == TP_FULL
     || cv_tp == TP_LEFT_HALF
     || cv_tp == TP_RITE_HALF
     || cv_tp == TP_LEFT_QTR
     || cv_tp == TP_RITE_QTR
     || cv_tp == TP_LEFT_CTR__QTR
     || cv_tp == TP_RITE_CTR__QTR ) )
  {
    ASSERT( __LINE__, m_win < m_num_wins, "m_win < m_num_wins" );

    m_file_hist[m_num_wins] = m_file_hist[m_win];

    View* nv = m_views[m_num_wins][ m_file_hist[m_num_wins][0] ];

    nv->SetTopLine ( cv->GetTopLine () );
    nv->SetLeftChar( cv->GetLeftChar() );
    nv->SetCrsRow  ( cv->GetCrsRow  () );
    nv->SetCrsCol  ( cv->GetCrsCol  () );

    m_win = m_num_wins;
    m_num_wins++;

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
  UpdateAll();
}

void Vis::Imp::FlipWindows()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 1 < m_num_wins )
  {
    // This code only works for MAX_WINS == 2
    View* pV1 = m_views[0][ m_file_hist[0][0] ];
    View* pV2 = m_views[1][ m_file_hist[1][0] ];

    if( pV1 != pV2 )
    {
      // Swap pV1 and pV2 Tile Positions:
      Tile_Pos tp_v1 = pV1->GetTilePos();
      pV1->SetTilePos( pV2->GetTilePos() );
      pV2->SetTilePos( tp_v1 );
    }
    UpdateAll();
  }
}

bool Vis::Imp::FName_2_FNum( const String& full_fname, unsigned& file_num )
{
  bool found = false;

  for( unsigned k=0; !found && k<m_files.len(); k++ )
  {
    if( full_fname == m_files[ k ]->GetFileName() )
    {
      found = true;
      file_num = k;
    }
  }
  return found;
}

void Vis::Imp::ReleaseFileNum( const unsigned file_num )
{
  bool ok = m_files.remove( file_num );

  for( unsigned k=0; ok && k<MAX_WINS; k++ )
  {
    View* win_k_view_of_file_num;
    m_views[k].remove( file_num, win_k_view_of_file_num );

    if( 0==k ) {
      // Delete the file:
      MemMark(__FILE__,__LINE__);
      delete win_k_view_of_file_num->GetFB();
    }
    // Delete the view:
    MemMark(__FILE__,__LINE__);
    delete win_k_view_of_file_num;

    unsList& file_hist_k = m_file_hist[k];

    // Remove all file_num's from m_file_hist
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

void Vis::Imp::Do_Star_FindPatterns()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Tell every FileBuf that it needs to find the new pattern:
  for( unsigned w=0; w<m_views[0].len(); w++ )
  {
    m_views[0][w]->GetFB()->NeedToFindStars();
  }
  // Only find new pattern now for FileBuf's that are displayed:
  for( unsigned w=0; w<m_num_wins; w++ )
  {
    View* pV = m_views[w][ m_file_hist[w][0] ];

    if( pV ) pV->GetFB()->Find_Stars();
  }
}

void Vis::Imp::Do_Star_ClearPatterns()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Tell every FileBuf that it needs to clear the old pattern:
  for( unsigned w=0; w<m_views[0].len(); w++ )
  {
    m_views[0][w]->GetFB()->NeedToClearStars();
  }
  // Remove star patterns from displayed FileBuf's only:
  for( unsigned w=0; w<m_num_wins; w++ )
  {
    View* pV = m_views[w][ m_file_hist[w][0] ];

    if( pV ) pV->GetFB()->ClearStars();
  }
}

void Vis::Imp::Do_Star_PrintPatterns( const bool HIGHLIGHT )
{
  for( unsigned w=0; w<m_num_wins; w++ )
  {
    m_views[w][ m_file_hist[w][0] ]->PrintPatterns( HIGHLIGHT );
  }
}

// 1. Search for star pattern in search editor.
// 2. If star pattern is found in search editor,
//         move pattern to end of search editor
//    else add star pattern to end of search editor
// 3. Clear buffer editor un-saved change status
// 4. If search editor is displayed, update search editor window
//
void Vis::Imp::Do_Star_Update_Search_Editor()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* const pseV = m_views[m_win][ SE_FILE ];
  // Determine whether search editor has the star pattern
  const unsigned NUM_SE_LINES = pseV->GetFB()->NumLines(); // Number of search editor lines
  bool found_pattern_in_search_editor = false;
  unsigned line_in_search_editor = 0;

  for( unsigned ln=0; !found_pattern_in_search_editor && ln<NUM_SE_LINES; ln++ )
  {
    const unsigned LL = pseV->GetFB()->LineLen( ln );
    // Copy line into m_sbuf until end of line or NULL byte
    m_sbuf.clear();
    int c = 1;
    for( unsigned k=0; c && k<LL; k++ )
    {
      c = pseV->GetFB()->Get( ln, k );
      m_sbuf.push( c );
    }
    if( m_sbuf == m_star )
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
  else
  {
    // Push star onto search editor buffer
    Line line(__FILE__,__LINE__);
    for( const char* p=m_star.c_str(); *p; p++ ) line.push(__FILE__,__LINE__, *p );
    pseV->GetFB()->PushLine( line );
  }
  // 3. Clear buffer editor un-saved change status
  pseV->GetFB()->ClearChanged();

  // 4. If search editor is displayed, update search editor window
  for( unsigned w=0; w<m_num_wins; w++ )
  {
    if( SE_FILE == m_file_hist[w][0] )
    {
      m_views[w][ SE_FILE ]->Update();
    }
  }
}

void Vis::Imp::Handle_Cmd()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const char CC = m_key.In();
 
  CmdFunc cf = CmdFuncs[ CC ];
  if( cf ) (this->*cf)();
}

void Vis::Imp::Handle_i()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.dot_buf.clear();
    m_key.dot_buf.push(__FILE__,__LINE__,'i');
    m_key.save_2_dot_buf = true;
  }

  if( m_diff_mode ) m_diff.Do_i();
  else               CV()->Do_i();

  if( !m_key.get_from_dot_buf )
  {
    m_key.save_2_dot_buf = false;
  }
}

void Vis::Imp::Handle_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.vis_buf.clear();
    m_key.vis_buf.push(__FILE__,__LINE__,'v');
    m_key.save_2_vis_buf = true;
  }
  const bool copy_vis_buf_2_dot_buf = m_diff_mode
                                    ? m_diff.Do_v()
                                    :  CV()->Do_v();
  if( !m_key.get_from_dot_buf )
  {
    m_key.save_2_vis_buf = false;

    if( copy_vis_buf_2_dot_buf )
    {
      m_key.dot_buf.copy( m_key.vis_buf );
    }
  }
}

void Vis::Imp::Handle_V()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.vis_buf.clear();
    m_key.vis_buf.push(__FILE__,__LINE__,'V');
    m_key.save_2_vis_buf = true;
  }
  const bool copy_vis_buf_2_dot_buf = m_diff_mode
                                    ? m_diff.Do_V()
                                    :  CV()->Do_V();
  if( !m_key.get_from_dot_buf )
  {
    m_key.save_2_vis_buf = false;

    if( copy_vis_buf_2_dot_buf )
    {
      m_key.dot_buf.copy( m_key.vis_buf );
    }
  }
}

void Vis::Imp::Handle_a()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.dot_buf.clear();
    m_key.dot_buf.push(__FILE__,__LINE__,'a');
    m_key.save_2_dot_buf = true;
  }

  if( m_diff_mode ) m_diff.Do_a();
  else               CV()->Do_a();

  if( !m_key.get_from_dot_buf )
  {
    m_key.save_2_dot_buf = false;
  }
}

void Vis::Imp::Handle_A()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.dot_buf.clear();
    m_key.dot_buf.push(__FILE__,__LINE__,'A');
    m_key.save_2_dot_buf = true;
  }

  if( m_diff_mode ) m_diff.Do_A();
  else               CV()->Do_A();

  if( !m_key.get_from_dot_buf )
  {
    m_key.save_2_dot_buf = false;
  }
}

void Vis::Imp::Handle_o()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.dot_buf.clear();
    m_key.dot_buf.push(__FILE__,__LINE__,'o');
    m_key.save_2_dot_buf = true;
  }

  if( m_diff_mode ) m_diff.Do_o();
  else               CV()->Do_o();

  if( !m_key.get_from_dot_buf )
  {
    m_key.save_2_dot_buf = false;
  }
}

void Vis::Imp::Handle_O()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.dot_buf.clear();
    m_key.dot_buf.push(__FILE__,__LINE__,'O');
    m_key.save_2_dot_buf = true;
  }

  if( m_diff_mode ) m_diff.Do_O();
  else               CV()->Do_O();

  if( !m_key.get_from_dot_buf )
  {
    m_key.save_2_dot_buf = false;
  }
}

void Vis::Imp::Handle_x()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.dot_buf.clear();
    m_key.dot_buf.push(__FILE__,__LINE__,'x');
  }
  if( m_diff_mode ) m_diff.Do_x();
  else               CV()->Do_x();
}

void Vis::Imp::Handle_s()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.dot_buf.clear();
    m_key.dot_buf.push(__FILE__,__LINE__,'s');
    m_key.save_2_dot_buf = true;
  }

  if( m_diff_mode ) m_diff.Do_s();
  else               CV()->Do_s();

  if( !m_key.get_from_dot_buf )
  {
    m_key.save_2_dot_buf = false;
  }
}

void Vis::Imp::Handle_c()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_diff_mode ) return;

  const char C = m_key.In();
  if( C == 'w' )
  {
    if( !m_key.get_from_dot_buf )
    {
      m_key.dot_buf.clear();
      m_key.dot_buf.push(__FILE__,__LINE__,'c');
      m_key.dot_buf.push(__FILE__,__LINE__,'w');
      m_key.save_2_dot_buf = true;
    }
    CV()->Do_cw();

    if( !m_key.get_from_dot_buf )
    {
      m_key.save_2_dot_buf = false;
    }
  }
  else if( C == '$' )
  {
    if( !m_key.get_from_dot_buf )
    {
      m_key.dot_buf.clear();
      m_key.dot_buf.push(__FILE__,__LINE__,'c');
      m_key.dot_buf.push(__FILE__,__LINE__,'$');
      m_key.save_2_dot_buf = true;
    }
    CV()->Do_D();
    CV()->Do_a();

    if( !m_key.get_from_dot_buf )
    {
      m_key.save_2_dot_buf = false;
    }
  }
}

void Vis::Imp::Handle_Q()
{
  Handle_Dot();
  Handle_j();
  Handle_0();
}

void Vis::Imp::Handle_k()
{
  if( m_diff_mode ) m_diff.GoUp(); 
  else               CV()->GoUp();
}

void Vis::Imp::Handle_j()
{
  if( m_diff_mode ) m_diff.GoDown();
  else               CV()->GoDown();
}

void Vis::Imp::Handle_h()
{
  if( m_diff_mode ) m_diff.GoLeft();
  else               CV()->GoLeft();
}

void Vis::Imp::Handle_l()
{
  if( m_diff_mode ) m_diff.GoRight();
  else               CV()->GoRight();
}

void Vis::Imp::Handle_H()
{
  if( m_diff_mode ) m_diff.GoToTopLineInView();
  else               CV()->GoToTopLineInView();
}

void Vis::Imp::Handle_L()
{
  if( m_diff_mode ) m_diff.GoToBotLineInView();
  else               CV()->GoToBotLineInView();
}

void Vis::Imp::Handle_M()
{
  if( m_diff_mode ) m_diff.GoToMidLineInView();
  else               CV()->GoToMidLineInView();
}

void Vis::Imp::Handle_0()
{
  if( m_diff_mode ) m_diff.GoToBegOfLine();
  else               CV()->GoToBegOfLine();
}

void Vis::Imp::Handle_Dollar()
{
  if( m_diff_mode ) m_diff.GoToEndOfLine();
  else               CV()->GoToEndOfLine();
}

void Vis::Imp::Handle_Return()
{
  if( m_diff_mode ) m_diff.GoToBegOfNextLine();
  else               CV()->GoToBegOfNextLine();
}

void Vis::Imp::Handle_G()
{
  if( m_diff_mode ) m_diff.GoToEndOfFile();
  else               CV()->GoToEndOfFile();
}

void Vis::Imp::Handle_b()
{
  if( m_diff_mode ) m_diff.GoToPrevWord();
  else               CV()->GoToPrevWord();
}

void Vis::Imp::Handle_w()
{
  if( m_diff_mode ) m_diff.GoToNextWord();
  else               CV()->GoToNextWord();
}

void Vis::Imp::Handle_e()
{
  if( m_diff_mode ) m_diff.GoToEndOfWord();
  else               CV()->GoToEndOfWord();
}

void Vis::Imp::Handle_Percent()
{
  if( m_diff_mode ) m_diff.GoToOppositeBracket();
  else               CV()->GoToOppositeBracket();
}

// Left squiggly bracket
void Vis::Imp::Handle_LeftSquigglyBracket()
{
  if( m_diff_mode ) m_diff.GoToLeftSquigglyBracket();
  else               CV()->GoToLeftSquigglyBracket();
}

// Right squiggly bracket
void Vis::Imp::Handle_RightSquigglyBracket()
{
  if( m_diff_mode ) m_diff.GoToRightSquigglyBracket();
  else               CV()->GoToRightSquigglyBracket();
}

void Vis::Imp::Handle_F()
{
  if( m_diff_mode )  m_diff.PageDown();
  else                CV()->PageDown();
}

void Vis::Imp::Handle_B()
{
  if( m_diff_mode )  m_diff.PageUp();
  else                CV()->PageUp();
}

void Vis::Imp::Handle_Dot()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<m_key.dot_buf.len() )
  {
    if( m_key.save_2_map_buf )
    {
      // Pop '.' off map_buf, because the contents of m_key.map_buf
      // will be saved to m_key.map_buf.
      m_key.map_buf.pop();
    }
    m_key.get_from_dot_buf = true;

    while( m_key.get_from_dot_buf )
    {
      const char CC = m_key.In();

      CmdFunc cf = CmdFuncs[ CC ];
      if( cf ) (this->*cf)();
    }
    if( m_diff_mode ) {
      // Diff does its own update every time a command is run
    }
    else {
      // Dont update until after all the commands have been executed:
      CV()->GetFB()->Update();
    }
  }
}

void Vis::Imp::Handle_m()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_key.save_2_map_buf || 0==m_key.map_buf.len() )
  {
    // When mapping, 'm' is ignored.
    // If not mapping and map buf len is zero, 'm' is ignored.
    return;
  }
  m_key.get_from_map_buf = true;

  while( m_key.get_from_map_buf )
  {
    const char CC = m_key.In();
 
    CmdFunc cf = CmdFuncs[ CC ];
    if( cf ) (this->*cf)();
  }
  if( m_diff_mode ) {
    // Diff does its own update every time a command is run
  }
  else {
    // Dont update until after all the commands have been executed:
    CV()->GetFB()->Update();
  }
}

void Vis::Imp::Handle_g()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC2 = m_key.In();

  if( CC2 == 'g' )
  {
    if( m_diff_mode ) m_diff.GoToTopOfFile();
    else               CV()->GoToTopOfFile();
  }
  else if( CC2 == '0' )
  {
    if( m_diff_mode ) m_diff.GoToStartOfRow();
    else               CV()->GoToStartOfRow();
  }
  else if( CC2 == '$' )
  {
    if( m_diff_mode ) m_diff.GoToEndOfRow();
    else               CV()->GoToEndOfRow();
  }
  else if( CC2 == 'f' )
  {
    if( !m_diff_mode ) GoToFile();
  }
}

void Vis::Imp::Handle_W()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const char CC2 = m_key.In();

  if     ( CC2 == 'W' ) GoToNextWindow();
  else if( CC2 == 'l' ) GoToNextWindow_l();
  else if( CC2 == 'h' ) GoToNextWindow_h();
  else if( CC2 == 'j'
        || CC2 == 'k' ) GoToNextWindow_jk();
  else if( CC2 == 'R' ) FlipWindows();
}

void Vis::Imp::Handle_d()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char C = m_key.In();

  if( C == 'd' )
  {
    if( !m_key.get_from_dot_buf )
    {
      m_key.dot_buf.clear();
      m_key.dot_buf.push(__FILE__,__LINE__,'d');
      m_key.dot_buf.push(__FILE__,__LINE__,'d');
    }
    if( m_diff_mode ) m_diff.Do_dd();
    else               CV()->Do_dd();
  }
  else if( C == 'w' )
  {
    if( m_diff_mode ) return;

    if( !m_key.get_from_dot_buf )
    {
      m_key.dot_buf.clear();
      m_key.dot_buf.push(__FILE__,__LINE__,'d');
      m_key.dot_buf.push(__FILE__,__LINE__,'w');
    }
    if( m_diff_mode ) m_diff.Do_dw();
    else               CV()->Do_dw();
  }
}

void Vis::Imp::Handle_y()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char C = m_key.In();

  if( C == 'y' )
  {
    if( m_diff_mode ) m_diff.Do_yy();
    else               CV()->Do_yy();
  }
  else if( C == 'w' )
  {
    if( m_diff_mode ) return;

    CV()->Do_yw();
  }
}

void Vis::Imp::Handle_D()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.dot_buf.clear();
    m_key.dot_buf.push(__FILE__,__LINE__,'D');
  }
  if( m_diff_mode ) m_diff.Do_D();
  else               CV()->Do_D();
}

void Vis::Imp::Handle_p()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.dot_buf.clear();
    m_key.dot_buf.push(__FILE__,__LINE__,'p');
  }
  if( m_diff_mode ) m_diff.Do_p();
  else           CV()->Do_p();
}

void Vis::Imp::Handle_P()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.dot_buf.clear();
    m_key.dot_buf.push(__FILE__,__LINE__,'P');
  }
  if( m_diff_mode ) m_diff.Do_P();
  else           CV()->Do_P();
}

void Vis::Imp::Handle_R()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.dot_buf.clear();
    m_key.dot_buf.push(__FILE__,__LINE__,'R');
    m_key.save_2_dot_buf = true;
  }
  if( m_diff_mode ) m_diff.Do_R();
  else               CV()->Do_R();

  if( !m_key.get_from_dot_buf )
  {
    m_key.save_2_dot_buf = false;
  }
}

void Vis::Imp::Handle_J()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.dot_buf.clear();
    m_key.dot_buf.push(__FILE__,__LINE__,'J');
  }
  if( m_diff_mode ) m_diff.Do_J();
  else               CV()->Do_J();
}

void Vis::Imp::Handle_Tilda()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_key.get_from_dot_buf )
  {
    m_key.dot_buf.clear();
    m_key.dot_buf.push(__FILE__,__LINE__,'~');
  }
  if( m_diff_mode ) m_diff.Do_Tilda();
  else               CV()->Do_Tilda();
}

void Vis::Imp::Handle_Star()
{
  Trace trace( __PRETTY_FUNCTION__ );

  String new_star = m_diff_mode ?  m_diff.Do_Star_GetNewPattern()
                                :   CV()->Do_Star_GetNewPattern();

  if( !m_slash && new_star == m_star ) return;

  // Un-highlight old star patterns for windows displayed:
  if( m_star.len() )
  { // Since m_diff_mode does Console::Update(),
    // no need to print patterns here if in m_diff_mode
    if( !m_diff_mode ) Do_Star_PrintPatterns( false );
  }
  Do_Star_ClearPatterns();

  m_star = new_star;

  if( m_star.len() )
  {
    m_slash = false;

    Do_Star_Update_Search_Editor();
    Do_Star_FindPatterns();
 
    // Highlight new star patterns for windows displayed:
    if( !m_diff_mode ) Do_Star_PrintPatterns( true );
  }
  if( m_diff_mode ) m_diff.Update();
  else {
    // Print out all the changes:
    Console::Update();
    // Put cursor back where it was
    CV()->PrintCursor();
  }
}

void Vis::Imp::Handle_Slash()
{
  Trace trace( __PRETTY_FUNCTION__ );

  CV()->GoToCmdLineClear("/");

  m_colon.GetCommand(1);

  String new_slash( m_cbuf );

  Handle_Slash_GotPattern( new_slash );
}

void Vis::Imp::Handle_n()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_diff_mode ) m_diff.Do_n();
  else           CV()->Do_n();
}

void Vis::Imp::Handle_N()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_diff_mode ) m_diff.Do_N();
  else               CV()->Do_N();
}

void Vis::Imp::Handle_u()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_diff_mode ) return; // Need to implement
  else            CV()->Do_u();
}

void Vis::Imp::Handle_U()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_diff_mode ) return; // Need to implement
  else            CV()->Do_U();
}

void Vis::Imp::Handle_Colon()
{
  Trace trace( __PRETTY_FUNCTION__ );
  CV()->GoToCmdLineClear(":");
  m_colon.GetCommand(1);
  RemoveSpaces( m_cbuf );
  m_colon.MapEnd();

  if     ( strcmp( m_cbuf,"q"   )==0 ) Quit();
  else if( strcmp( m_cbuf,"qa"  )==0 ) QuitAll();
  else if( strcmp( m_cbuf,"help")==0 ) Help();
  else if( strcmp( m_cbuf,"diff")==0 ) DoDiff();
  else if( strcmp( m_cbuf,"nodiff")==0)NoDiff();
  else if( strcmp( m_cbuf,"n"   )==0 ) GoToNextBuffer();
  else if( strcmp( m_cbuf,"se"  )==0 ) GoToSearchBuffer();
  else if( strcmp( m_cbuf,"vsp" )==0 ) VSplitWindow();
  else if( strcmp( m_cbuf,"sp"  )==0 ) HSplitWindow();
  else if( strcmp( m_cbuf,"cs1" )==0 ) { Console::Set_Color_Scheme_1(); }
  else if( strcmp( m_cbuf,"cs2" )==0 ) { Console::Set_Color_Scheme_2(); }
  else if( strcmp( m_cbuf,"cs3" )==0 ) { Console::Set_Color_Scheme_3(); }
  else if( strcmp( m_cbuf,"cs4" )==0 ) { Console::Set_Color_Scheme_4(); }
  else if( strcmp( m_cbuf,"hi"  )==0 ) m_colon.hi();
  else if( strncmp(m_cbuf,"cd",2)==0 ) { Ch_Dir(); }
  else if( strncmp(m_cbuf,"syn",3)==0) { Set_Syntax(); }
  else if( strcmp( m_cbuf,"pwd" )==0 ) { GetCWD(); }
  else if( strcmp( m_cbuf,"sh"  )==0
        || strcmp( m_cbuf,"shell")==0) { GoToCmdBuffer(); }
#ifndef WIN32
  else if( strcmp( m_cbuf,"run" )==0 ) { RunCommand(); }
#endif
  else if( strncmp(m_cbuf,"re",2)==0 ) { Console::Refresh(); }
  else if( strcmp( m_cbuf,"map" )==0 )     m_colon.MapStart();
  else if( strcmp( m_cbuf,"showmap")==0)   m_colon.MapShow();
  else if( strcmp( m_cbuf,"cover")==0)     m_colon.Cover();
  else if( strcmp( m_cbuf,"coverkey")==0)  m_colon.CoverKey();
  else if( 'e' == m_cbuf[0] )              HandleColon_e();
  else if( 'w' == m_cbuf[0] )              HandleColon_w();
  else if( 'b' == m_cbuf[0] )              HandleColon_b();
  else if( '0' <= m_cbuf[0] && m_cbuf[0] <= '9' )
  {
    // Move cursor to line:
    const unsigned line_num = atol( m_cbuf );
    if( m_diff_mode ) m_diff.GoToLine( line_num );
    else               CV()->GoToLine( line_num );
  }
  else { // Put cursor back to line and column in edit window:
    if( m_diff_mode ) m_diff.PrintCursor( CV() );
    else               CV()->PrintCursor();
  }
}

void Vis::Imp::HandleColon_b()
{
  if( 0 == m_cbuf[1] ) // :b
  {
    GoToPrevBuffer();
  }
  else {
    // Switch to a different buffer:
    if     ( '#' == m_cbuf[1] ) GoToPoundBuffer();// :b#
    else if( 'c' == m_cbuf[1] ) GoToCurrBuffer(); // :bc
    else if( 'e' == m_cbuf[1] ) GoToBufferEditor();   // :be
    else if( 'm' == m_cbuf[1] ) GoToMsgBuffer();  // :bm
    else {
      unsigned buffer_num = atol( m_cbuf+1 ); // :b<number>
      GoToBuffer( buffer_num );
    }
  }
}

void Vis::Imp::HandleColon_e()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = CV();
  if( 0 == m_cbuf[1] ) // :e
  {
    FileBuf* pfb = pV->GetFB();
    pfb->ReReadFile();

    for( unsigned w=0; w<m_num_wins; w++ )
    {
      if( pfb == m_views[w][ m_file_hist[w][0] ]->GetFB() )
      {
        // View is currently displayed, perform needed update:
        m_views[w][ m_file_hist[w][0] ]->Update();
      }
    }
  }
  else // :e file_name
  {
    // Edit file of supplied file name:
    String fname( m_cbuf + 1 );
    if( pV->GoToDir() && FindFullFileName( fname ) )
    {
      unsigned file_index = 0;
      if( HaveFile( fname.c_str(), &file_index ) )
      {
        GoToBuffer( file_index );
      }
      else {
        FileBuf* p_fb = new(__FILE__,__LINE__) FileBuf( m_vis, fname.c_str(), true, FT_UNKNOWN );
        p_fb->ReadFile();
        GoToBuffer( m_views[m_win].len()-1 );
      }
    }
  }
}

void Vis::Imp::HandleColon_w()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = CV();

  if( 0 == m_cbuf[1] ) // :w
  {
    if( pV == m_views[m_win][ CMD_FILE ] )
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
      if( CV() == pV ) pV->PrintCursor();
    }
  }
  else // :w file_name
  {
    // Edit file of supplied file name:
    String fname( m_cbuf + 1 );
    if( pV->GoToDir() && FindFullFileName( fname ) )
    {
      unsigned file_index = 0;
      if( HaveFile( fname.c_str(), &file_index ) )
      {
        GetFileBuf( file_index )->Write();
      }
      else if( fname.get_end() != DIR_DELIM )
      {
        FileBuf* p_fb = new(__FILE__,__LINE__) FileBuf( m_vis, fname.c_str(), *pV->GetFB() );
        p_fb->Write();
      }
    }
  }
}

// m_vis.m_file_hist[m_vis.m_win]:
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

#ifndef WIN32
void Vis::Imp::RunCommand()
{
  Trace trace( __PRETTY_FUNCTION__ );
  String cmd;
  bool ok = RunCommand_GetCommand( cmd );
  if( !ok )
  {
    CV()->PrintCursor();
  }
  else {
    FileBuf* pfb = CV()->GetFB();
    // Add ######################################
    pfb->PushLine();
    for( unsigned k=0; k<40; k++ ) pfb->PushChar( '#' );

    int exit_val = 0;
    bool ran_cmd = RunCommand_RunCommand( cmd, pfb, exit_val );

    if( ran_cmd )
    {
      char exit_msg[128];
      sprintf( exit_msg, "Exit_Value=%i", exit_val );

      // Append exit_msg:
      if( 0<pfb->LineLen( pfb->NumLines()-1 ) ) pfb->PushLine();
      const unsigned EXIT_MSG_LEN = strlen( exit_msg );
      for( unsigned k=0; k<EXIT_MSG_LEN; k++ ) pfb->PushChar( exit_msg[k] );
    }
    // Add ###.. line followed by empty line
    pfb->PushLine();
    for( unsigned k=0; k<40; k++ ) pfb->PushChar( '#' );
    pfb->PushLine();

    // Move cursor to bottom of file
    const unsigned NUM_LINES = pfb->NumLines();
    CV()->GoToCrsPos_NoWrite( NUM_LINES-1, 0 );
    pfb->Update();
  }
}

bool Vis::Imp::RunCommand_GetCommand( String& cmd )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( CMD_FILE != m_file_hist[m_win][0] ) return false;

  FileBuf* pfb = CV()->GetFB();
  int LAST_LINE = pfb->NumLines() - 1;
  // Nothing in COMMAND_BUFFER so just return
  if( LAST_LINE < 0 ) return false;

  // Find first line:
  bool found_first_line = false;
  int first_line = 0;
  for( int l=LAST_LINE; !found_first_line && 0<=l; l-- )
  {
    const unsigned LL = pfb->LineLen( l );

    for( unsigned p=0; !found_first_line && p<LL; p++ )
    {
      const uint8_t C = pfb->Get( l, p );
      if( !IsSpace( C ) ) {
        if( '#' == C ) {
          first_line = l+1;
          found_first_line = true;
        }
      }
    }
  }
  // No command so just return
  if( LAST_LINE < first_line ) return false;

  // Concatenate all command lines into String cmd:
  for( unsigned k=first_line; k<=LAST_LINE; k++ )
  {
    const unsigned LL = pfb->LineLen( k );
    for( unsigned p=0; p<LL; p++ )
    {
      const uint8_t C = pfb->Get( k, p );
      cmd.push( C );
    }
    if( LL && LAST_LINE != k ) cmd.push(' ');
  }
  cmd.trim(); //< Remove leading and ending spaces

  return cmd.len() ? true : false;
}

// Returns true of ran command, else false
bool Vis::Imp::RunCommand_RunCommand( const String& cmd
                               , FileBuf* pfb
                               , int& exit_val )
{
  Trace trace( __PRETTY_FUNCTION__ );

  pid_t child_pid = 0;
  FILE* fp = POpenRead( cmd.c_str(), child_pid );
  if( NULL == fp )
  {
    m_vis.Window_Message("\nPOpenRead( %s ) failed\n\n", cmd.c_str() );
    return false;
  }
  pfb->PushLine();
  m_cmd_mode = true;
  // Move cursor to bottom of file
  const unsigned NUM_LINES = pfb->NumLines();
  CV()->GoToCrsPos_NoWrite( NUM_LINES-1, 0 );
  pfb->Update(); 

  double T1 = GetTimeSeconds();
  for( int C = fgetc( fp ); EOF != C; C = fgetc( fp ) )
  {
    if( '\n' != C ) pfb->PushChar( C );
    else {
      pfb->PushLine(); 
      double T2 = GetTimeSeconds();
      if( 0.5 < (T2-T1) ) {
        T1 = T2;
        // Move cursor to bottom of file
        const unsigned NUM_LINES = pfb->NumLines();
        CV()->GoToCrsPos_NoWrite( NUM_LINES-1, 0 );
        pfb->Update(); 
      }
    }
  }
  exit_val = PClose( fp, child_pid );
  m_cmd_mode = false;
  return true;
}
#endif

Vis::Vis()
  : m( *new(__FILE__, __LINE__) Imp( *this ) )
{
}

void Vis::Init( const int ARGC, const char* const ARGV[] )
{
  m.Init( ARGC, ARGV );
}

Vis::~Vis()
{
  delete &m;
}

void Vis::Run()
{
  m.Run();
}

void Vis::Stop()
{
  m.Stop();
}

View* Vis::CV() const
{
  return m.CV();
}

View* Vis::WinView( const unsigned w ) const
{
  return m.WinView( w );
}

unsigned Vis::GetNumWins() const
{
  return m.GetNumWins();
}

Paste_Mode Vis::GetPasteMode() const
{
  return m.GetPasteMode();
}

void Vis::SetPasteMode( Paste_Mode pm )
{
  m.SetPasteMode( pm );
}

bool Vis::InDiffMode() const
{
  return m.InDiffMode();
}

bool Vis::RunningCmd() const
{
  return m.RunningCmd();
}

bool Vis::RunningDot() const
{
  return m.RunningDot();
}

FileBuf* Vis::GetFileBuf( const unsigned index ) const
{
  return m.GetFileBuf( index );
}

unsigned Vis::GetStarLen() const
{
  return m.GetStarLen();
}

const char* Vis::GetStar() const
{
  return m.GetStar();
}

bool Vis::GetSlash() const
{
  return m.GetSlash();
}

void Vis::CheckWindowSize()
{
  return m.CheckWindowSize();
}

void Vis::CheckFileModTime()
{
  return m.CheckFileModTime();
}

void Vis::Add_FileBuf_2_Lists_Create_Views( FileBuf* pfb, const char* fname )
{
  return m.Add_FileBuf_2_Lists_Create_Views( pfb, fname );
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

  m.CmdLineMessage( msg_buf );
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

  m.Window_Message( msg_buf );
}

void Vis::UpdateAll()
{
  m.UpdateAll();
}

bool Vis::Update_Status_Lines()
{
  return m.Update_Status_Lines();
}

bool Vis::Update_Change_Statuses()
{
  return m.Update_Change_Statuses();
}

void Vis::PrintCursor()
{
  m.PrintCursor();
}

bool Vis::HaveFile( const char* file_name, unsigned* index )
{
  return m.HaveFile( file_name, index );
}

bool Vis::File_Is_Displayed( const String& full_fname )
{
  return m.File_Is_Displayed( full_fname );
}

void Vis::ReleaseFileName( const String& full_fname )
{
  return m.ReleaseFileName( full_fname );
}

bool Vis::GoToBuffer_Fname( String& fname )
{
  return m.GoToBuffer_Fname( fname );
}

void Vis::Handle_f()
{
  m.Handle_f();
}

void Vis::Handle_z()
{
  m.Handle_z();
}

void Vis::Handle_SemiColon()
{
  m.Handle_SemiColon();
}

void Vis::Handle_Slash_GotPattern( const String& pattern
                            , const bool MOVE_TO_FIRST_PATTERN )
{
  m.Handle_Slash_GotPattern( pattern, MOVE_TO_FIRST_PATTERN );
}

Line* Vis::BorrowLine( const char* _FILE_
                     , const unsigned _LINE_
                     , const unsigned SIZE )
{
  return m.BorrowLine( _FILE_, _LINE_, SIZE );
}

Line* Vis::BorrowLine( const char* _FILE_
                     , const unsigned _LINE_
                     , const unsigned LEN, const uint8_t FILL )
{
  return m.BorrowLine( _FILE_, _LINE_, LEN, FILL );
}

Line* Vis::BorrowLine( const char* _FILE_
                     , const unsigned _LINE_
                     , const Line& line )
{
  return m.BorrowLine( _FILE_, _LINE_, line );
}

void Vis::ReturnLine( Line* lp )
{
  m.ReturnLine( lp );
}

LineChange* Vis::BorrowLineChange( const ChangeType type
                                 , const unsigned   lnum
                                 , const unsigned   cpos )
{
  return m.BorrowLineChange( type, lnum, cpos );
}

void Vis::ReturnLineChange( LineChange* lcp )
{
  m.ReturnLineChange( lcp );
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

