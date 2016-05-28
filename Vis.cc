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
#include "View.hh"
#include "Key.hh"
#include "Vis.hh"

const char* PROG_NAME;

Vis*           gl_pVis        = 0;
Key*           gl_pKey        = 0;
ConstCharList* gl_pCall_Stack = 0;

const int FD_IO = 0; // read/write file descriptor

extern const char* DIR_DELIM_STR;
extern MemLog<MEM_LOG_BUF_SIZE> Log;

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

Vis::Vis( const int ARGC, const char* const ARGV[] )
  : files(__FILE__, __LINE__)
  , win( 0 )
  , num_wins( 1 )
  , views()
  , file_hist()
  , tabStop( 8 )
  , running( true )
  , reg()
  , line_cache()
  , change_cache()
  , paste_mode( PM_LINE )
  , slash( false )
  , fast_char( -1 )
  , diff()
  , m_diff_mode( false )
  , m_colon( *this )
{
  Trace trace( __PRETTY_FUNCTION__ );

  gl_pVis = this;

  m_cbuf[0] = 0;

  running = Console::GetWindowSize();
            Console::SetConsoleCursor();

  InitBufferEditor();
  InitHelpBuffer();
  InitSearchEditor();
  InitMsgBuffer();
  InitCmdBuffer();
  bool run_diff = InitUserBuffers( ARGC, ARGV );
  InitFileHistory();
  InitCmdFuncs();

  if( run_diff && ( (CMD_FILE+1+2) == files.len()) )
  {
    // User supplied: "-d file1 file2", so run diff:
    m_diff_mode = true;
    num_wins = 2;
    file_hist[ 0 ][0] = 5;
    file_hist[ 1 ][0] = 6;
    views[0][ file_hist[ 0 ][0] ]->SetTilePos( TP_LEFT_HALF );
    views[1][ file_hist[ 1 ][0] ]->SetTilePos( TP_RITE_HALF );

    DoDiff();
  }
}

Vis::~Vis()
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned i=0; i<MAX_WINS; i++ )
  {
    for( unsigned k=0; k<views[i].len(); k++ )
    {
      MemMark(__FILE__,__LINE__);
      delete views[i][k];
    }
  }
  for( unsigned k=0; k<files.len(); k++ )
  {
    MemMark(__FILE__,__LINE__);
    delete files[k];
  }
}

void Vis::InitBufferEditor()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // Buffer editor, 0
  const char* buf_edit_name = EDIT_BUF_NAME;
  FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( buf_edit_name, false, FT_BUFFER_EDITOR );
  files.push(__FILE__,__LINE__, pfb );
  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* pV = new(__FILE__,__LINE__) View( pfb );
    views[w].push(__FILE__,__LINE__, pV );
    pfb->AddView( pV );
  }
  // Push "Buffer_Editor" onto buffer editor buffer
  AddToBufferEditor( buf_edit_name );
}

void Vis::InitHelpBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // Help buffer, 1
  const char* help_buf_name = HELP_BUF_NAME;
  FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( help_buf_name, false, FT_TEXT );
  pfb->ReadString( HELP_STR );
  files.push(__FILE__,__LINE__, pfb );
  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* pV = new(__FILE__,__LINE__) View( pfb );
    views[w].push(__FILE__,__LINE__, pV );
    pfb->AddView( pV );
  }
  // Push "VIT_HELP" onto buffer editor buffer
  AddToBufferEditor( help_buf_name );
}

void Vis::InitSearchEditor()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // Search editor buffer, 2
  const char* search_buf_name = SRCH_BUF_NAME;
  FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( search_buf_name, false, FT_TEXT );
  files.push(__FILE__,__LINE__, pfb );
  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* pV = new(__FILE__,__LINE__) View( pfb );
    views[w].push(__FILE__,__LINE__, pV );
    pfb->AddView( pV );
  }
  // Push "SEARCH_EDITOR" onto buffer editor buffer
  AddToBufferEditor( search_buf_name );
}

void Vis::InitMsgBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // Message buffer, 3
  const char* msg_buf_name = MSG__BUF_NAME;
  FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( msg_buf_name, false, FT_TEXT );
  files.push(__FILE__,__LINE__, pfb );
  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* pV = new(__FILE__,__LINE__) View( pfb );
    views[w].push(__FILE__,__LINE__, pV );
    pfb->AddView( pV );
  }
  // Push "MESSAGE_EDITOR" onto buffer editor buffer
  AddToBufferEditor( msg_buf_name );
}

void Vis::InitCmdBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // Command buffer, CMD_FILE(4)
  const char* cmd_buf_name = CMD__BUF_NAME;
  FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( cmd_buf_name, false, FT_TEXT );
  pfb->PushLine(); // Add an empty line
  files.push(__FILE__,__LINE__, pfb );
  for( unsigned w=0; w<MAX_WINS; w++ )
  {
    View* pV = new(__FILE__,__LINE__) View( pfb );
    views[w].push(__FILE__,__LINE__, pV );
    pfb->AddView( pV );
  }
  // Push "COMMAND_BUFFER" onto buffer editor buffer
  AddToBufferEditor( cmd_buf_name );
}

bool Vis::InitUserBuffers( const int ARGC, const char* const ARGV[] )
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
          FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( file_name.c_str(), true, FT_UNKNOWN );
          pfb->ReadFile();
        }
        // Restore original directory, for next call to FindFullFileName()
        if( got_orig_dir ) chdir( orig_dir );
      }
    }
  }
  return run_diff;
}

void Vis::InitFileHistory()
{
  for( int w=0; w<MAX_WINS; w++ )
  {
    file_hist[w].push( __FILE__,__LINE__, BE_FILE );
    file_hist[w].push( __FILE__,__LINE__, HELP_FILE );

    if( 5<views[w].len() )
    {
      file_hist[w].insert( __FILE__,__LINE__, 0, 5 );

      for( int f=views[w].len()-1; 6<=f; f-- )
      {
        file_hist[w].push( __FILE__,__LINE__, f );
      }
    }
  }
}

void Vis::InitCmdFuncs()
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned k=0; k<128; k++ ) CmdFuncs[k] = 0;

  CmdFuncs[ 'i' ] = &Vis::Handle_i;
  CmdFuncs[ 'v' ] = &Vis::Handle_v;
  CmdFuncs[ 'V' ] = &Vis::Handle_V;
  CmdFuncs[ 'a' ] = &Vis::Handle_a;
  CmdFuncs[ 'A' ] = &Vis::Handle_A;
  CmdFuncs[ 'o' ] = &Vis::Handle_o;
  CmdFuncs[ 'O' ] = &Vis::Handle_O;
  CmdFuncs[ 'x' ] = &Vis::Handle_x;
  CmdFuncs[ 's' ] = &Vis::Handle_s;
  CmdFuncs[ 'c' ] = &Vis::Handle_c;
  CmdFuncs[ 'Q' ] = &Vis::Handle_Q;
  CmdFuncs[ 'k' ] = &Vis::Handle_k;
  CmdFuncs[ 'j' ] = &Vis::Handle_j;
  CmdFuncs[ 'h' ] = &Vis::Handle_h;
  CmdFuncs[ 'l' ] = &Vis::Handle_l;
  CmdFuncs[ 'H' ] = &Vis::Handle_H;
  CmdFuncs[ 'L' ] = &Vis::Handle_L;
  CmdFuncs[ 'M' ] = &Vis::Handle_M;
  CmdFuncs[ '0' ] = &Vis::Handle_0;
  CmdFuncs[ '$' ] = &Vis::Handle_Dollar;
  CmdFuncs[ '\n'] = &Vis::Handle_Return;
  CmdFuncs[ 'G' ] = &Vis::Handle_G;
  CmdFuncs[ 'b' ] = &Vis::Handle_b;
  CmdFuncs[ 'w' ] = &Vis::Handle_w;
  CmdFuncs[ 'e' ] = &Vis::Handle_e;
  CmdFuncs[ 'f' ] = &Vis::Handle_f;
  CmdFuncs[ ';' ] = &Vis::Handle_SemiColon;
  CmdFuncs[ '%' ] = &Vis::Handle_Percent;
  CmdFuncs[ '{' ] = &Vis::Handle_LeftSquigglyBracket;
  CmdFuncs[ '}' ] = &Vis::Handle_RightSquigglyBracket;
  CmdFuncs[ 'F' ] = &Vis::Handle_F;
  CmdFuncs[ 'B' ] = &Vis::Handle_B;
  CmdFuncs[ ':' ] = &Vis::Handle_Colon;
  CmdFuncs[ '/' ] = &Vis::Handle_Slash; // Crashes
  CmdFuncs[ '*' ] = &Vis::Handle_Star;
  CmdFuncs[ '.' ] = &Vis::Handle_Dot;
  CmdFuncs[ 'm' ] = &Vis::Handle_m;
  CmdFuncs[ 'g' ] = &Vis::Handle_g;
  CmdFuncs[ 'W' ] = &Vis::Handle_W;
  CmdFuncs[ 'd' ] = &Vis::Handle_d;
  CmdFuncs[ 'y' ] = &Vis::Handle_y;
  CmdFuncs[ 'D' ] = &Vis::Handle_D;
  CmdFuncs[ 'p' ] = &Vis::Handle_p;
  CmdFuncs[ 'P' ] = &Vis::Handle_P;
  CmdFuncs[ 'R' ] = &Vis::Handle_R;
  CmdFuncs[ 'J' ] = &Vis::Handle_J;
  CmdFuncs[ '~' ] = &Vis::Handle_Tilda;
  CmdFuncs[ 'n' ] = &Vis::Handle_n;
  CmdFuncs[ 'N' ] = &Vis::Handle_N;
  CmdFuncs[ 'u' ] = &Vis::Handle_u;
  CmdFuncs[ 'U' ] = &Vis::Handle_U;
  CmdFuncs[ 'z' ] = &Vis::Handle_z;
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
      AdjustViews();
      Console::Invalidate();
      UpdateAll();
    }
  }
}

void Vis::CheckFileModTime()
{
  Trace trace( __PRETTY_FUNCTION__ );

  FileBuf* pfb = CV()->pfb;
  const char* fname = pfb->file_name.c_str();

  const double curr_mod_time = ModificationTime( fname );

  if( pfb->mod_time < curr_mod_time )
  {
    if( pfb->is_dir )
    {
      // Dont ask the user, just read in the directory.
      // pfb->mod_time will get updated in pfb->ReReadFile()
      pfb->ReReadFile();

      for( unsigned w=0; w<num_wins; w++ )
      {
        if( pfb == views[w][ file_hist[w][0] ]->pfb )
        {
          // View is currently displayed, perform needed update:
          views[w][ file_hist[w][0] ]->Update();
        }
      }
    }
    else { // Regular file
      // Update file modification time so that the message window
      // will not keep popping up:
      pfb->mod_time = curr_mod_time;

      Window_Message("\n%s\n\nhas changed since it was read in\n\n", fname );
    }
  }
}

void Vis::Run()
{
  Trace trace( __PRETTY_FUNCTION__ );
  UpdateAll();

  while( running )
  {
    Handle_Cmd();
  }
  Console::Flush();
}

void Vis::Quit()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const Tile_Pos TP = CV()->tile_pos;

  if( num_wins <= 1 ) QuitAll();
  else {
    if( win < num_wins-1 )
    {
      Quit_ShiftDown();
    }
    if( 0 < win ) win--;
    num_wins--;

    Quit_JoinTiles( TP );

    UpdateAll();

    CV()->PrintCursor();
  }
}

void Vis::Quit_ShiftDown()
{
  // Make copy of win's list of views and view history:
  ViewList win_views    ( views    [win] );
   unsList win_view_hist( file_hist[win] );

  // Shift everything down
  for( unsigned w=win+1; w<num_wins; w++ )
  {
    views    [w-1] = views    [w];
    file_hist[w-1] = file_hist[w];
  }
  // Put win's list of views at end of views:
  // Put win's view history at end of view historys:
  views    [num_wins-1] = win_views;
  file_hist[num_wins-1] = win_view_hist;
}

void Vis::Quit_JoinTiles( const Tile_Pos TP )
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

void Vis::Quit_JoinTiles_TP_LEFT_HALF()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

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

void Vis::Quit_JoinTiles_TP_RITE_HALF()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

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

void Vis::Quit_JoinTiles_TP_TOP__HALF()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

    if     ( TP == TP_BOT__HALF         ) { v->SetTilePos( TP_FULL ); break; }
    else if( TP == TP_BOT__LEFT_QTR     ) v->SetTilePos( TP_LEFT_HALF );
    else if( TP == TP_BOT__RITE_QTR     ) v->SetTilePos( TP_RITE_HALF );
    else if( TP == TP_BOT__LEFT_8TH     ) v->SetTilePos( TP_LEFT_QTR );
    else if( TP == TP_BOT__RITE_8TH     ) v->SetTilePos( TP_RITE_QTR );
    else if( TP == TP_BOT__LEFT_CTR_8TH ) v->SetTilePos( TP_LEFT_CTR__QTR );
    else if( TP == TP_BOT__RITE_CTR_8TH ) v->SetTilePos( TP_RITE_CTR__QTR );
  }
}

void Vis::Quit_JoinTiles_TP_BOT__HALF()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

    if     ( TP == TP_TOP__HALF         ) { v->SetTilePos( TP_FULL ); break; }
    else if( TP == TP_TOP__LEFT_QTR     ) v->SetTilePos( TP_LEFT_HALF );
    else if( TP == TP_TOP__RITE_QTR     ) v->SetTilePos( TP_RITE_HALF );
    else if( TP == TP_TOP__LEFT_8TH     ) v->SetTilePos( TP_LEFT_QTR );
    else if( TP == TP_TOP__RITE_8TH     ) v->SetTilePos( TP_RITE_QTR );
    else if( TP == TP_TOP__LEFT_CTR_8TH ) v->SetTilePos( TP_LEFT_CTR__QTR );
    else if( TP == TP_TOP__RITE_CTR_8TH ) v->SetTilePos( TP_RITE_CTR__QTR );
  }
}

void Vis::Quit_JoinTiles_TP_TOP__LEFT_QTR()
{
  if( Have_TP_BOT__HALF() )
  {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if     ( TP == TP_TOP__RITE_QTR     ) { v->SetTilePos( TP_TOP__HALF ); break; }
      else if( TP == TP_TOP__RITE_8TH     ) v->SetTilePos( TP_TOP__RITE_QTR );
      else if( TP == TP_TOP__RITE_CTR_8TH ) v->SetTilePos( TP_TOP__LEFT_QTR );
    }
  }
  else {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if     ( TP == TP_BOT__LEFT_QTR     ) { v->SetTilePos( TP_LEFT_HALF ); break; }
      else if( TP == TP_BOT__LEFT_8TH     ) v->SetTilePos( TP_LEFT_QTR );
      else if( TP == TP_BOT__LEFT_CTR_8TH ) v->SetTilePos( TP_LEFT_CTR__QTR );
    }
  }
}

void Vis::Quit_JoinTiles_TP_TOP__RITE_QTR()
{
  if( Have_TP_BOT__HALF() )
  {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if     ( TP == TP_TOP__LEFT_QTR     ) { v->SetTilePos( TP_TOP__HALF ); break; }
      else if( TP == TP_TOP__LEFT_8TH     ) v->SetTilePos( TP_TOP__LEFT_QTR );
      else if( TP == TP_TOP__LEFT_CTR_8TH ) v->SetTilePos( TP_TOP__RITE_QTR );
    }
  }
  else {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if     ( TP == TP_BOT__RITE_QTR     ) { v->SetTilePos( TP_RITE_HALF ); break; }
      else if( TP == TP_BOT__RITE_8TH     ) v->SetTilePos( TP_RITE_QTR );
      else if( TP == TP_BOT__RITE_CTR_8TH ) v->SetTilePos( TP_RITE_CTR__QTR );
    }
  }
}

void Vis::Quit_JoinTiles_TP_BOT__LEFT_QTR()
{
  if( Have_TP_TOP__HALF() )
  {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if     ( TP == TP_BOT__RITE_QTR     ) { v->SetTilePos( TP_BOT__HALF ); break; }
      else if( TP == TP_BOT__RITE_8TH     ) v->SetTilePos( TP_BOT__RITE_QTR );
      else if( TP == TP_BOT__RITE_CTR_8TH ) v->SetTilePos( TP_BOT__LEFT_QTR );
    }
  }
  else {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if     ( TP == TP_TOP__LEFT_QTR     ) { v->SetTilePos( TP_LEFT_HALF ); break; }
      else if( TP == TP_TOP__LEFT_8TH     ) v->SetTilePos( TP_LEFT_QTR );
      else if( TP == TP_TOP__LEFT_CTR_8TH ) v->SetTilePos( TP_LEFT_CTR__QTR );
    }
  }
}

void Vis::Quit_JoinTiles_TP_BOT__RITE_QTR()
{
  if( Have_TP_TOP__HALF() )
  {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if     ( TP == TP_BOT__LEFT_QTR     ) { v->SetTilePos( TP_BOT__HALF ); break; }
      else if( TP == TP_BOT__LEFT_8TH     ) v->SetTilePos( TP_BOT__LEFT_QTR );
      else if( TP == TP_BOT__LEFT_CTR_8TH ) v->SetTilePos( TP_BOT__RITE_QTR );
    }
  }
  else {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if     ( TP == TP_TOP__RITE_QTR     ) { v->SetTilePos( TP_RITE_HALF ); break; }
      else if( TP == TP_TOP__RITE_8TH     ) v->SetTilePos( TP_RITE_QTR );
      else if( TP == TP_TOP__RITE_CTR_8TH ) v->SetTilePos( TP_RITE_CTR__QTR );
    }
  }
}

void Vis::Quit_JoinTiles_TP_LEFT_QTR()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

    if     ( TP == TP_LEFT_CTR__QTR     ) { v->SetTilePos( TP_LEFT_HALF ); break; }
    else if( TP == TP_TOP__LEFT_CTR_8TH ) v->SetTilePos( TP_TOP__LEFT_QTR );
    else if( TP == TP_BOT__LEFT_CTR_8TH ) v->SetTilePos( TP_BOT__LEFT_QTR );
  }
}

void Vis::Quit_JoinTiles_TP_RITE_QTR()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

    if     ( TP == TP_RITE_CTR__QTR     ) { v->SetTilePos( TP_RITE_HALF ); break; }
    else if( TP == TP_TOP__RITE_CTR_8TH ) v->SetTilePos( TP_TOP__RITE_QTR );
    else if( TP == TP_BOT__RITE_CTR_8TH ) v->SetTilePos( TP_BOT__RITE_QTR );
  }
}

void Vis::Quit_JoinTiles_TP_LEFT_CTR__QTR()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

    if     ( TP == TP_LEFT_QTR      ) { v->SetTilePos( TP_LEFT_HALF ); break; }
    else if( TP == TP_TOP__LEFT_8TH ) v->SetTilePos( TP_TOP__LEFT_QTR );
    else if( TP == TP_BOT__LEFT_8TH ) v->SetTilePos( TP_BOT__LEFT_QTR );
  }
}

void Vis::Quit_JoinTiles_TP_RITE_CTR__QTR()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

    if     ( TP == TP_RITE_QTR      ) { v->SetTilePos( TP_RITE_HALF ); break; }
    else if( TP == TP_TOP__RITE_8TH ) v->SetTilePos( TP_TOP__RITE_QTR );
    else if( TP == TP_BOT__RITE_8TH ) v->SetTilePos( TP_BOT__RITE_QTR );
  }
}

void Vis::Quit_JoinTiles_TP_TOP__LEFT_8TH()
{
  if( Have_TP_BOT__LEFT_QTR() )
  {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_TOP__LEFT_CTR_8TH ) { v->SetTilePos( TP_TOP__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_BOT__LEFT_8TH ) { v->SetTilePos( TP_LEFT_QTR ); break; }
    }
  }
}

void Vis::Quit_JoinTiles_TP_TOP__RITE_8TH()
{
  if( Have_TP_BOT__RITE_QTR() )
  {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_TOP__RITE_CTR_8TH ) { v->SetTilePos( TP_TOP__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_BOT__RITE_8TH ) { v->SetTilePos( TP_RITE_QTR ); break; }
    }
  }
}

void Vis::Quit_JoinTiles_TP_TOP__LEFT_CTR_8TH()
{
  if( Have_TP_BOT__LEFT_QTR() )
  {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_TOP__LEFT_8TH ) { v->SetTilePos( TP_TOP__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_BOT__LEFT_CTR_8TH ) { v->SetTilePos( TP_LEFT_CTR__QTR ); break; }
    }
  }
}

void Vis::Quit_JoinTiles_TP_TOP__RITE_CTR_8TH()
{
  if( Have_TP_BOT__RITE_QTR() )
  {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_TOP__RITE_8TH ) { v->SetTilePos( TP_TOP__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_BOT__RITE_CTR_8TH ) { v->SetTilePos( TP_RITE_CTR__QTR ); break; }
    }
  }
}

void Vis::Quit_JoinTiles_TP_BOT__LEFT_8TH()
{
  if( Have_TP_TOP__LEFT_QTR() )
  {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_BOT__LEFT_CTR_8TH ) { v->SetTilePos( TP_BOT__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_TOP__LEFT_8TH ) { v->SetTilePos( TP_LEFT_QTR ); break; }
    }
  }
}

void Vis::Quit_JoinTiles_TP_BOT__RITE_8TH()
{
  if( Have_TP_TOP__RITE_QTR() )
  {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_BOT__RITE_CTR_8TH ) { v->SetTilePos( TP_BOT__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_TOP__RITE_8TH ) { v->SetTilePos( TP_RITE_QTR ); break; }
    }
  }
}

void Vis::Quit_JoinTiles_TP_BOT__LEFT_CTR_8TH()
{
  if( Have_TP_TOP__LEFT_QTR() )
  {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_BOT__LEFT_8TH ) { v->SetTilePos( TP_BOT__LEFT_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_TOP__LEFT_CTR_8TH ) { v->SetTilePos( TP_LEFT_CTR__QTR ); break; }
    }
  }
}

void Vis::Quit_JoinTiles_TP_BOT__RITE_CTR_8TH()
{
  if( Have_TP_TOP__RITE_QTR() )
  {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_BOT__RITE_8TH ) { v->SetTilePos( TP_BOT__RITE_QTR ); break; }
    }
  }
  else {
    for( unsigned k=0; k<num_wins; k++ )
    {
      View* v = views[k][ file_hist[k][0] ];
      const Tile_Pos TP = v->tile_pos;

      if( TP == TP_TOP__RITE_CTR_8TH ) { v->SetTilePos( TP_RITE_CTR__QTR ); break; }
    }
  }
}

bool Vis::Have_TP_BOT__HALF()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

    if( TP == TP_BOT__HALF ) return true;
  }
  return false;
}

bool Vis::Have_TP_TOP__HALF()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

    if( TP == TP_TOP__HALF ) return true;
  }
  return false;
}

bool Vis::Have_TP_BOT__LEFT_QTR()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

    if( TP == TP_BOT__LEFT_QTR ) return true;
  }
  return false;
}

bool Vis::Have_TP_TOP__LEFT_QTR()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

    if( TP == TP_TOP__LEFT_QTR ) return true;
  }
  return false;
}

bool Vis::Have_TP_BOT__RITE_QTR()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

    if( TP == TP_BOT__RITE_QTR ) return true;
  }
  return false;
}

bool Vis::Have_TP_TOP__RITE_QTR()
{
  for( unsigned k=0; k<num_wins; k++ )
  {
    View* v = views[k][ file_hist[k][0] ];
    const Tile_Pos TP = v->tile_pos;

    if( TP == TP_TOP__RITE_QTR ) return true;
  }
  return false;
}

void Vis::QuitAll()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Move cursor down to bottom of screen:
  Console::Move_2_Row_Col( Console::Num_Rows()-1, 0 );

  // Put curson on a new line:
  Console::NewLine();

  running = false;
}

void Vis::Help()
{
  GoToBuffer( HELP_FILE );
}

void Vis::DoDiff()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Must be exactly 2 buffers to do diff:
  if( 2 == num_wins )
  {
    View* pv0 = views[0][ file_hist[0][0] ];
    View* pv1 = views[1][ file_hist[1][0] ];
    FileBuf* pfb0 = pv0->pfb;
    FileBuf* pfb1 = pv1->pfb;

    // New code in progress:
    bool ok = true;
    if( !pfb0->is_dir && pfb1->is_dir )
    {
      pv1 = DoDiff_FindRegFileView( pfb0, pfb1, 1, pv1 );
    }
    else if( pfb0->is_dir && !pfb1->is_dir )
    {
      pv0 = DoDiff_FindRegFileView( pfb1, pfb0, 0, pv0 );
    }
    else {
      if( !FileExists( pfb0->file_name.c_str() ) )
      {
        ok = false;
        Log.Log("\n%s does not exist\n", pfb0->file_name.c_str() );
      }
      else if( !FileExists( pfb1->file_name.c_str() ) )
      {
        ok = false;
        Log.Log("\n%s does not exist\n", pfb1->file_name.c_str() );
      }
    }
    if( !ok ) running = false;
    else {
#ifndef WIN32
      timeval tv1; gettimeofday( &tv1, 0 );
#endif
      bool ok = diff.Run( pv0, pv1 );
      if( ok ) {
        m_diff_mode = true;

#ifndef WIN32
        timeval tv2; gettimeofday( &tv2, 0 );

        double secs = (tv2.tv_sec-tv1.tv_sec)
                    + double(tv2.tv_usec)/1e6
                    - double(tv1.tv_usec)/1e6;
        CmdLineMessage( "Diff took: %g seconds", secs );
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

View* Vis::DoDiff_FindRegFileView( const FileBuf* pfb_reg
                                 , const FileBuf* pfb_dir
                                 , const unsigned win_idx
                                 ,       View*    pv )
{
  String possible_fname = pfb_dir->file_name;
  String fname_extension;

  gArray_t<String*> path_parts;
  Create_Path_Parts_List( pfb_reg->file_name, path_parts );

  for( int k=path_parts.len()-1; 0<=k; k-- )
  {
    // Revert back to pfb_dir.m_fname:
    possible_fname = pfb_dir->file_name;

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
View* Vis::DoDiff_CheckPossibleFile( const int win_idx
                                   , const char* pos_fname )
{
  struct stat sbuf;
  int err = my_stat( pos_fname, sbuf );

  if( 0 == err )
  {
    // File exists, find or create FileBuf, and set second view to display that file:
    if( !HaveFile( pos_fname ) )
    {
      FileBuf* pfb = new(__FILE__,__LINE__) FileBuf( pos_fname, true, FT_UNKNOWN );
      pfb->ReadFile();
    }
  }
  unsigned file_index = 0;
  if( HaveFile( pos_fname, &file_index ) )
  {
    SetWinToBuffer( win_idx, file_index, false );

    return views[win_idx][ file_hist[win_idx][0] ];
  }
  return 0;
}

void Vis::NoDiff()
{
  if( true == m_diff_mode )
  {
    m_diff_mode = false;

    UpdateAll();
  }
}

void Vis::AddToBufferEditor( const char* fname )
{
  Trace trace( __PRETTY_FUNCTION__ );
  Line line(__FILE__, __LINE__, strlen( fname ) );
//unsigned NUM_BUFFERS = views[0].len();
//char str[32]; 
//sprintf( str, "%3u ", NUM_BUFFERS-1 );
//for( const char* p=str  ; *p; p++ ) line.push(__FILE__,__LINE__, *p );
  for( const char* p=fname; *p; p++ ) line.push(__FILE__,__LINE__, *p );
  FileBuf* pfb = views[0][ BE_FILE ]->pfb;
  pfb->PushLine( line );
  pfb->BufferEditor_Sort();
  pfb->ClearChanged();

  // Since buffer editor file has been re-arranged, make sure none of its
  // views have the cursor position past the end of the line
  for( unsigned k=0; k<MAX_WINS; k++ )
  {
    View* pV = views[k][ BE_FILE ];

    unsigned CL = pV->CrsLine();
    unsigned CP = pV->CrsChar();
    unsigned LL = pfb->LineLen( CL );

    if( LL <= CP )
    {
      pV->GoToCrsPos_NoWrite( CL, LL-1 );
    }
  }
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

  View* pV = CV();

  const unsigned WC  = pV->WorkingCols();
  const unsigned ROW = pV->Cmd__Line_Row();
  const unsigned COL = pV->Col_Win_2_GL( 0 );
  const unsigned MSG_LEN = strlen( msg_buf );
  if( WC < MSG_LEN )
  {
    // messaged does not fit, so truncate beginning
    Console::SetS( ROW, COL, msg_buf + (MSG_LEN - WC), S_NORMAL );
  }
  else {
    // messaged fits, add spaces at end
    Console::SetS( ROW, COL, msg_buf, S_NORMAL );
    for( unsigned k=0; k<(WC-MSG_LEN); k++ )
    {
      Console::Set( ROW, pV->Col_Win_2_GL( k+MSG_LEN ), ' ', S_NORMAL );
    }
  }
  Console::Update();
  pV->PrintCursor();
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

  FileBuf* pMB = views[0][MSG_FILE]->pfb;

  // Clear Message Buffer:
  while( pMB->NumLines() ) pMB->PopLine();

  // Add msg_buf to pMB
  Line* pL = 0;
  const unsigned MSB_BUF_LEN = strlen( msg_buf );
  for( unsigned k=0; k<MSB_BUF_LEN; k++ )
  {
    if( 0==pL ) pL = BorrowLine( __FILE__,__LINE__ );

    const char C = msg_buf[k];

    if( C == '\n' ) { pMB->PushLine( pL ); pL = 0; }
    else            { pL->push(__FILE__,__LINE__, C ); }
  }
  // Make sure last borrowed line gets put into Message Buffer:
  if( pL ) pMB->PushLine( pL );

  // Initially, put cursor at top of Message Buffer:
  View* pV = views[win][MSG_FILE];
  pV->crsRow = 0;
  pV->crsCol = 0;
  pV->topLine = 0;

  BufferMessage();
}

void Vis::Handle_Cmd()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const char CC = gl_pKey->In();
 
  CmdFunc cf = CmdFuncs[ CC ];
  if( cf ) (this->*cf)();
}

void Vis::Handle_i()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->dot_buf.clear();
    gl_pKey->dot_buf.push(__FILE__,__LINE__,'i');
    gl_pKey->save_2_dot_buf = true;
  }

  if( m_diff_mode ) diff.Do_i();
  else           CV()->Do_i();

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->save_2_dot_buf = false;
  }
}

void Vis::Handle_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->vis_buf.clear();
    gl_pKey->vis_buf.push(__FILE__,__LINE__,'v');
    gl_pKey->save_2_vis_buf = true;
  }
  const bool copy_vis_buf_2_dot_buf = m_diff_mode
                                    ? diff.Do_v()
                                    : CV()->Do_v();
  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->save_2_vis_buf = false;

    if( copy_vis_buf_2_dot_buf )
    {
      gl_pKey->dot_buf.copy( gl_pKey->vis_buf );
    }
  }
}

void Vis::Handle_V()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->vis_buf.clear();
    gl_pKey->vis_buf.push(__FILE__,__LINE__,'V');
    gl_pKey->save_2_vis_buf = true;
  }
  const bool copy_vis_buf_2_dot_buf = m_diff_mode
                                    ? diff.Do_V()
                                    : CV()->Do_V();
  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->save_2_vis_buf = false;

    if( copy_vis_buf_2_dot_buf )
    {
      gl_pKey->dot_buf.copy( gl_pKey->vis_buf );
    }
  }
}

void Vis::Handle_a()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->dot_buf.clear();
    gl_pKey->dot_buf.push(__FILE__,__LINE__,'a');
    gl_pKey->save_2_dot_buf = true;
  }

  if( m_diff_mode ) diff.Do_a();
  else           CV()->Do_a();

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->save_2_dot_buf = false;
  }
}

void Vis::Handle_A()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->dot_buf.clear();
    gl_pKey->dot_buf.push(__FILE__,__LINE__,'A');
    gl_pKey->save_2_dot_buf = true;
  }

  if( m_diff_mode ) diff.Do_A();
  else           CV()->Do_A();

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->save_2_dot_buf = false;
  }
}

void Vis::Handle_o()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->dot_buf.clear();
    gl_pKey->dot_buf.push(__FILE__,__LINE__,'o');
    gl_pKey->save_2_dot_buf = true;
  }

  if( m_diff_mode ) diff.Do_o();
  else           CV()->Do_o();

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->save_2_dot_buf = false;
  }
}

void Vis::Handle_O()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->dot_buf.clear();
    gl_pKey->dot_buf.push(__FILE__,__LINE__,'O');
    gl_pKey->save_2_dot_buf = true;
  }

  if( m_diff_mode ) diff.Do_O();
  else           CV()->Do_O();

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->save_2_dot_buf = false;
  }
}

void Vis::Handle_x()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->dot_buf.clear();
    gl_pKey->dot_buf.push(__FILE__,__LINE__,'x');
  }
  if( m_diff_mode ) diff.Do_x();
  else           CV()->Do_x();
}

void Vis::Handle_s()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->dot_buf.clear();
    gl_pKey->dot_buf.push(__FILE__,__LINE__,'s');
    gl_pKey->save_2_dot_buf = true;
  }

  if( m_diff_mode ) diff.Do_s();
  else           CV()->Do_s();

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->save_2_dot_buf = false;
  }
}

void Vis::Handle_c()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_diff_mode ) return;

  const char C = gl_pKey->In();
  if( C == 'w' )
  {
    if( !gl_pKey->get_from_dot_buf )
    {
      gl_pKey->dot_buf.clear();
      gl_pKey->dot_buf.push(__FILE__,__LINE__,'c');
      gl_pKey->dot_buf.push(__FILE__,__LINE__,'w');
      gl_pKey->save_2_dot_buf = true;
    }
    CV()->Do_cw();

    if( !gl_pKey->get_from_dot_buf )
    {
      gl_pKey->save_2_dot_buf = false;
    }
  }
  else if( C == '$' )
  {
    if( !gl_pKey->get_from_dot_buf )
    {
      gl_pKey->dot_buf.clear();
      gl_pKey->dot_buf.push(__FILE__,__LINE__,'c');
      gl_pKey->dot_buf.push(__FILE__,__LINE__,'$');
      gl_pKey->save_2_dot_buf = true;
    }
    CV()->Do_D();
    CV()->Do_a();

    if( !gl_pKey->get_from_dot_buf )
    {
      gl_pKey->save_2_dot_buf = false;
    }
  }
}

void Vis::Handle_Q()
{
  Handle_Dot();
  Handle_j();
  Handle_0();
}

void Vis::Handle_k()
{
  if( m_diff_mode ) diff.GoUp(); 
  else           CV()->GoUp();
}

void Vis::Handle_j()
{
  if( m_diff_mode ) diff.GoDown();
  else           CV()->GoDown();
}

void Vis::Handle_h()
{
  if( m_diff_mode ) diff.GoLeft();
  else           CV()->GoLeft();
}

void Vis::Handle_l()
{
  if( m_diff_mode ) diff.GoRight();
  else           CV()->GoRight();
}

void Vis::Handle_H()
{
  if( m_diff_mode ) diff.GoToTopLineInView();
  else           CV()->GoToTopLineInView();
}

void Vis::Handle_L()
{
  if( m_diff_mode ) diff.GoToBotLineInView();
  else           CV()->GoToBotLineInView();
}

void Vis::Handle_M()
{
  if( m_diff_mode ) diff.GoToMidLineInView();
  else           CV()->GoToMidLineInView();
}

void Vis::Handle_0()
{
  if( m_diff_mode ) diff.GoToBegOfLine();
  else           CV()->GoToBegOfLine();
}

void Vis::Handle_Dollar()
{
  if( m_diff_mode ) diff.GoToEndOfLine();
  else           CV()->GoToEndOfLine();
}

void Vis::Handle_Return()
{
  if( m_diff_mode ) diff.GoToBegOfNextLine();
  else           CV()->GoToBegOfNextLine();
}

void Vis::Handle_G()
{
  if( m_diff_mode ) diff.GoToEndOfFile();
  else           CV()->GoToEndOfFile();
}

void Vis::Handle_b()
{
  if( m_diff_mode ) diff.GoToPrevWord();
  else           CV()->GoToPrevWord();
}

void Vis::Handle_w()
{
  if( m_diff_mode ) diff.GoToNextWord();
  else           CV()->GoToNextWord();
}

void Vis::Handle_e()
{
  if( m_diff_mode ) diff.GoToEndOfWord();
  else           CV()->GoToEndOfWord();
}

void Vis::Handle_f()
{
  Trace trace( __PRETTY_FUNCTION__ );

  fast_char = gl_pKey->In();

  if( m_diff_mode ) diff.Do_f( fast_char );
  else           CV()->Do_f( fast_char );
}

void Vis::Handle_SemiColon()
{
  if( 0 <= fast_char )
  {
    if( m_diff_mode ) diff.Do_f( fast_char );
    else           CV()->Do_f( fast_char );
  }
}

void Vis::Handle_Percent()
{
  if( m_diff_mode ) diff.GoToOppositeBracket();
  else           CV()->GoToOppositeBracket();
}

// Left squiggly bracket
void Vis::Handle_LeftSquigglyBracket()
{
  if( m_diff_mode ) diff.GoToLeftSquigglyBracket();
  else           CV()->GoToLeftSquigglyBracket();
}

// Right squiggly bracket
void Vis::Handle_RightSquigglyBracket()
{
  if( m_diff_mode ) diff.GoToRightSquigglyBracket();
  else           CV()->GoToRightSquigglyBracket();
}

void Vis::Handle_F()
{
  if( m_diff_mode )  diff.PageDown();
  else            CV()->PageDown();
}

void Vis::Handle_B()
{
  if( m_diff_mode )  diff.PageUp();
  else            CV()->PageUp();
}

void Vis::Handle_Colon()
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
  else if( strcmp( m_cbuf,"se"  )==0 ) SearchEditor();
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
        || strcmp( m_cbuf,"shell")==0) { BufferShell(); }
#ifndef WIN32
  else if( strcmp( m_cbuf,"run" )==0 ) { RunCommand(); }
#endif
  else if( strncmp(m_cbuf,"re",2)==0 ) { Console::Refresh(); }
  else if( strcmp( m_cbuf,"map" )==0 )     m_colon.MapStart();
  else if( strcmp( m_cbuf,"showmap")==0)   m_colon.MapShow();
  else if( strcmp( m_cbuf,"cover")==0)     m_colon.Cover();
  else if( strcmp( m_cbuf,"coverkey")==0)  m_colon.CoverKey();
  else if( 'e' == m_cbuf[0] )              m_colon.e();
  else if( 'w' == m_cbuf[0] )              m_colon.w();
  else if( 'b' == m_cbuf[0] )              m_colon.b();
  else if( '0' <= m_cbuf[0] && m_cbuf[0] <= '9' )
  {
    // Move cursor to line:
    const unsigned line_num = atol( m_cbuf );
    if( m_diff_mode ) diff.GoToLine( line_num );
    else           CV()->GoToLine( line_num );
  }
  else { // Put cursor back to line and column in edit window:
    if( m_diff_mode ) diff.PrintCursor( CV() );
    else           CV()->PrintCursor();
  }
}

void Vis::Handle_Slash()
{
  Trace trace( __PRETTY_FUNCTION__ );

  CV()->GoToCmdLineClear("/");

  m_colon.GetCommand(1);

  String new_slash( m_cbuf );

  Handle_Slash_GotPattern( new_slash );
}

void Vis::Handle_Slash_GotPattern( const String& pattern
                                 , const bool MOVE_TO_FIRST_PATTERN )
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( slash && pattern == star )
  {
    CV()->PrintCursor();
    return;
  }
  // Un-highlight old star patterns for windows displayed:
  if( star.len()  )
  { // Since m_diff_mode does Console::Update(),
    // no need to print patterns here if in m_diff_mode
    if( !m_diff_mode ) Do_Star_PrintPatterns( false );
  }
  Do_Star_ClearPatterns();

  star = pattern;

  if( !star.len() ) CV()->PrintCursor();
  else {
    slash = true;

    Do_Star_Update_Search_Editor();
    Do_Star_FindPatterns();

    // Highlight new star patterns for windows displayed:
    if( !m_diff_mode ) Do_Star_PrintPatterns( true );

    if( MOVE_TO_FIRST_PATTERN )
    {
      if( m_diff_mode ) diff.Do_n(); // Move to first pattern
      else           CV()->Do_n(); // Move to first pattern
    }
    if( m_diff_mode ) diff.Update();
    else {
      // Print out all the changes:
      Console::Update();
      Console::Flush();
    }
  }
}

void Vis::Handle_Dot()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<gl_pKey->dot_buf.len() )
  {
    if( gl_pKey->save_2_map_buf )
    {
      // Pop '.' off map_buf, because the contents of gl_pKey->map_buf
      // will be saved to gl_pKey->map_buf.
      gl_pKey->map_buf.pop();
    }
    gl_pKey->get_from_dot_buf = true;

    while( gl_pKey->get_from_dot_buf )
    {
      const char CC = gl_pKey->In();

      CmdFunc cf = CmdFuncs[ CC ];
      if( cf ) (this->*cf)();
    }
    if( m_diff_mode ) {
      // Diff does its own update every time a command is run
    }
    else {
      // Dont update until after all the commands have been executed:
      CV()->pfb->Update();
    }
  }
}

void Vis::Handle_m()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( gl_pKey->save_2_map_buf || 0==gl_pKey->map_buf.len() )
  {
    // When mapping, 'm' is ignored.
    // If not mapping and map buf len is zero, 'm' is ignored.
    return;
  }
  gl_pKey->get_from_map_buf = true;

  while( gl_pKey->get_from_map_buf )
  {
    const char CC = gl_pKey->In();
 
    CmdFunc cf = CmdFuncs[ CC ];
    if( cf ) (this->*cf)();
  }
  if( m_diff_mode ) {
    // Diff does its own update every time a command is run
  }
  else {
    // Dont update until after all the commands have been executed:
    CV()->pfb->Update();
  }
}

void Vis::Handle_g()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC2 = gl_pKey->In();

  if( CC2 == 'g' )
  {
    if( m_diff_mode ) diff.GoToTopOfFile();
    else           CV()->GoToTopOfFile();
  }
  else if( CC2 == '0' )
  {
    if( m_diff_mode ) diff.GoToStartOfRow();
    else           CV()->GoToStartOfRow();
  }
  else if( CC2 == '$' )
  {
    if( m_diff_mode ) diff.GoToEndOfRow();
    else           CV()->GoToEndOfRow();
  }
  else if( CC2 == 'f' )
  {
    if( !m_diff_mode ) GoToFile();
  }
}

void Vis::Handle_W()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const char CC2 = gl_pKey->In();

  if     ( CC2 == 'W' ) GoToNextWindow();
  else if( CC2 == 'l' ) GoToNextWindow_l();
  else if( CC2 == 'h' ) GoToNextWindow_h();
  else if( CC2 == 'j'
        || CC2 == 'k' ) GoToNextWindow_jk();
  else if( CC2 == 'R' ) FlipWindows();
}

void Vis::Handle_d()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char C = gl_pKey->In();

  if( C == 'd' )
  {
    if( !gl_pKey->get_from_dot_buf )
    {
      gl_pKey->dot_buf.clear();
      gl_pKey->dot_buf.push(__FILE__,__LINE__,'d');
      gl_pKey->dot_buf.push(__FILE__,__LINE__,'d');
    }
    if( m_diff_mode ) diff.Do_dd();
    else           CV()->Do_dd();
  }
  else if( C == 'w' )
  {
    if( m_diff_mode ) return;

    if( !gl_pKey->get_from_dot_buf )
    {
      gl_pKey->dot_buf.clear();
      gl_pKey->dot_buf.push(__FILE__,__LINE__,'d');
      gl_pKey->dot_buf.push(__FILE__,__LINE__,'w');
    }
    if( m_diff_mode ) diff.Do_dw();
    else           CV()->Do_dw();
  }
}

void Vis::Handle_y()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char C = gl_pKey->In();

  if( C == 'y' )
  {
    if( m_diff_mode ) diff.Do_yy();
    else           CV()->Do_yy();
  }
  else if( C == 'w' )
  {
    if( m_diff_mode ) return;

    CV()->Do_yw();
  }
}

void Vis::Handle_D()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->dot_buf.clear();
    gl_pKey->dot_buf.push(__FILE__,__LINE__,'D');
  }
  if( m_diff_mode ) diff.Do_D();
  else           CV()->Do_D();
}

void Vis::Handle_p()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->dot_buf.clear();
    gl_pKey->dot_buf.push(__FILE__,__LINE__,'p');
  }
  if( m_diff_mode ) diff.Do_p();
  else           CV()->Do_p();
}

void Vis::Handle_P()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->dot_buf.clear();
    gl_pKey->dot_buf.push(__FILE__,__LINE__,'P');
  }
  if( m_diff_mode ) diff.Do_P();
  else           CV()->Do_P();
}

void Vis::Handle_R()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->dot_buf.clear();
    gl_pKey->dot_buf.push(__FILE__,__LINE__,'R');
    gl_pKey->save_2_dot_buf = true;
  }
  if( m_diff_mode ) diff.Do_R();
  else           CV()->Do_R();

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->save_2_dot_buf = false;
  }
}

void Vis::Handle_J()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->dot_buf.clear();
    gl_pKey->dot_buf.push(__FILE__,__LINE__,'J');
  }
  if( m_diff_mode ) diff.Do_J();
  else           CV()->Do_J();
}

void Vis::Handle_Tilda()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !gl_pKey->get_from_dot_buf )
  {
    gl_pKey->dot_buf.clear();
    gl_pKey->dot_buf.push(__FILE__,__LINE__,'~');
  }
  if( m_diff_mode ) diff.Do_Tilda();
  else           CV()->Do_Tilda();
}

void Vis::Handle_Star()
{
  Trace trace( __PRETTY_FUNCTION__ );

  String new_star = m_diff_mode ?  diff.Do_Star_GetNewPattern()
                              : CV()->Do_Star_GetNewPattern();

  if( !slash && new_star == star ) return;

  // Un-highlight old star patterns for windows displayed:
  if( star.len() )
  { // Since m_diff_mode does Console::Update(),
    // no need to print patterns here if in m_diff_mode
    if( !m_diff_mode ) Do_Star_PrintPatterns( false );
  }
  Do_Star_ClearPatterns();

  star = new_star;

  if( star.len() )
  {
    slash = false;

    Do_Star_Update_Search_Editor();
    Do_Star_FindPatterns();
 
    // Highlight new star patterns for windows displayed:
    if( !m_diff_mode ) Do_Star_PrintPatterns( true );
  }
  if( m_diff_mode ) diff.Update();
  else {
    // Print out all the changes:
    Console::Update();
    // Put cursor back where it was
    CV()->PrintCursor();
  }
}

void Vis::Handle_n()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_diff_mode ) diff.Do_n();
  else           CV()->Do_n();
}

void Vis::Handle_N()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_diff_mode ) diff.Do_N();
  else           CV()->Do_N();
}

void Vis::Handle_u()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_diff_mode ) return; // Need to implement
  else            CV()->Do_u();
}

void Vis::Handle_U()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_diff_mode ) return; // Need to implement
  else            CV()->Do_U();
}

void Vis::Handle_z()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC2 = gl_pKey->In();

  if( CC2 == 't' || IsEndOfLineDelim( CC2 ) )
  {
    if( m_diff_mode ) diff.MoveCurrLineToTop();
    else           CV()->MoveCurrLineToTop();
  }
  else if( CC2 == 'z' )
  {
    if( m_diff_mode ) diff.MoveCurrLineCenter();
    else           CV()->MoveCurrLineCenter();
  }
  else if( CC2 == 'b' )
  {
    if( m_diff_mode ) diff.MoveCurrLineToBottom();
    else           CV()->MoveCurrLineToBottom();
  }
}

void Vis::UpdateAll()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !m_diff_mode )
  {
    for( unsigned k=0; k<num_wins; k++ )
    {
      views[k][ file_hist[k][0] ]->Update();
    }
  }
}

bool Vis::Update_Status_Lines()
{
  bool updated_a_sts_line = false;

  for( unsigned w=0; w<num_wins; w++ )
  {
    // pV points to currently displayed view in window w:
    View* const pV = views[w][ file_hist[w][0] ];

    if( pV->sts_line_needs_update )
    {
      // Update status line:
      pV->PrintStsLine();     // Print status line.
      Console::Update();

      pV->sts_line_needs_update = false;
      updated_a_sts_line = true;
    }
  }
  return updated_a_sts_line;
}

bool Vis::Update_Change_Statuses()
{
  // Update buffer changed status around windows:
  bool updated_change_sts = false;

  for( unsigned k=0; k<num_wins; k++ )
  {
    // pV points to currently displayed view in window w:
    View* const pV = views[k][ file_hist[k][0] ];

    if( pV->us_change_sts != pV->pfb->Changed() )
    {
      pV->Print_Borders();
      pV->us_change_sts = pV->pfb->Changed();
      updated_change_sts = true;
    }
  }
  return updated_change_sts;
}

// 1. Get filename underneath the cursor
// 2. Search for filename in buffer list, and if found, go to that buffer
// 3. If not found, print a command line message
//
void Vis::GoToFile()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // 1. Get fname underneath the cursor:
  String fname;
  bool ok = CV()->GoToFile_GetFileName( fname );

  if( ok ) GoToFile_GoToBuffer( fname );
}

// Return true if went to buffer indicated by fname, else false
bool Vis::GoToFile_GoToBuffer( String& fname )
{
  // 2. Search for fname in buffer list, and if found, go to that buffer:
  unsigned file_index = 0;
  if( HaveFile( fname.c_str(), &file_index ) )
  { GoToBuffer( file_index ); return true; }

  // 3. Go to directory of current buffer:
  if( ! CV()->GoToDir() )
  {
    CmdLineMessage( "Could not find file: %s", fname.c_str() );
    return false;
  }

  // 4. Get full file name
  if( !FindFullFileName( fname ) )
  {
    CmdLineMessage( "Could not find file: %s", fname.c_str() );
    return false;
  }

  // 5. Search for fname in buffer list, and if found, go to that buffer:
  if( HaveFile( fname.c_str(), &file_index ) )
  {
    GoToBuffer( file_index ); return true;
  }

  // 6. See if file exists, and if so, add a file buffer, and go to that buffer
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
    FileBuf* fb = new(__FILE__,__LINE__) FileBuf( fname.c_str(), true, FT_UNKNOWN );
    fb->ReadFile();
    GoToBuffer( views[win].len()-1 );
  }
  else {
    CmdLineMessage( "Could not find file: %s", fname.c_str() );
    return false;
  }
  return true;
}

bool Vis::HaveFile( const char* file_name, unsigned* file_index )
{
  bool already_have_file = false;

  const unsigned NUM_FILES = files.len();

  for( unsigned k=0; !already_have_file && k<NUM_FILES; k++ )
  {
    if( 0==strcmp( files[k]->file_name.c_str(), file_name ) )
    {
      already_have_file = true;

      if( file_index ) *file_index = k;
    }
  }
  return already_have_file;
}

bool Vis::FName_2_FNum( const String& full_fname, unsigned& file_num )
{
  bool found = false;

  for( unsigned k=0; !found && k<files.len(); k++ )
  {
    if( full_fname == files[ k ]->file_name )
    {
      found = true;
      file_num = k;
    }
  }
  return found;
}
bool Vis::File_Is_Displayed( const String& full_fname )
{
  unsigned file_num = 0;

  if( FName_2_FNum( full_fname, file_num ) )
  {
    return File_Is_Displayed( file_num );
  }
  return false;
}
bool Vis::File_Is_Displayed( const unsigned file_num )
{
  for( unsigned w=0; w<num_wins; w++ )
  {
    if( file_num == file_hist[ w ][ 0 ] )
    {
      return true;
    }
  }
  return false;
}
void Vis::ReleaseFileName( const String& full_fname )
{
  unsigned file_num = 0;
  if( FName_2_FNum( full_fname, file_num ) )
  {
    ReleaseFileNum( file_num );
  }
}
void Vis::ReleaseFileNum( const unsigned file_num )
{
  bool ok = files.remove( file_num );

  for( unsigned k=0; ok && k<MAX_WINS; k++ )
  {
    View* win_k_view_of_file_num;
    views[k].remove( file_num, win_k_view_of_file_num );

    if( 0==k ) {
      // Delete the file:
      MemMark(__FILE__,__LINE__);
      delete win_k_view_of_file_num->pfb;
    }
    // Delete the view:
    MemMark(__FILE__,__LINE__);
    delete win_k_view_of_file_num;

    unsList& file_hist_k = file_hist[k];

    // Remove all file_num's from file_hist
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

void Vis::Do_Star_PrintPatterns( const bool HIGHLIGHT )
{
  for( unsigned w=0; w<num_wins; w++ )
  {
    views[w][ file_hist[w][0] ]->PrintPatterns( HIGHLIGHT );
  }
}

void Vis::Do_Star_FindPatterns()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Tell every FileBuf that it needs to find the new pattern:
  for( unsigned w=0; w<views[0].len(); w++ )
  {
    views[0][w]->pfb->need_2_find_stars = true;
  }
  // Only find new pattern now for FileBuf's that are displayed:
  for( unsigned w=0; w<num_wins; w++ )
  {
    View* pV = views[w][ file_hist[w][0] ];

    if( pV ) pV->pfb->Find_Stars();
  }
}

void Vis::Do_Star_ClearPatterns()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Tell every FileBuf that it needs to clear the old pattern:
  for( unsigned w=0; w<views[0].len(); w++ )
  {
    views[0][w]->pfb->need_2_clear_stars = true;
  }
  // Remove star patterns from displayed FileBuf's only:
  for( unsigned w=0; w<num_wins; w++ )
  {
    View* pV = views[w][ file_hist[w][0] ];

    if( pV ) pV->pfb->ClearStars();
  }
}

// 1. Search for star pattern in search editor.
// 2. If star pattern is found in search editor,
//         move pattern to end of search editor
//    else add star pattern to end of search editor
// 3. Clear buffer editor un-saved change status
// 4. If search editor is displayed, update search editor window
//
void Vis::Do_Star_Update_Search_Editor()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* const pseV = views[win][ SE_FILE ];
  // Determine whether search editor has the star pattern
  const unsigned NUM_SE_LINES = pseV->pfb->NumLines(); // Number of search editor lines
  bool found_pattern_in_search_editor = false;
  unsigned line_in_search_editor = 0;

  for( unsigned ln=0; !found_pattern_in_search_editor && ln<NUM_SE_LINES; ln++ )
  {
    const unsigned LL = pseV->pfb->LineLen( ln );
    // Copy line into m_sbuf until end of line or NULL byte
    m_sbuf.clear();
    int c = 1;
    for( unsigned k=0; c && k<LL; k++ )
    {
      c = pseV->pfb->Get( ln, k );
      m_sbuf.push( c );
    }
    if( m_sbuf == star )
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
      Line* p = pseV->pfb->RemoveLineP( line_in_search_editor );
      pseV->pfb->InsertLine( NUM_SE_LINES-1, p );
    }
  }
  else
  {
    // Push star onto search editor buffer
    Line line(__FILE__,__LINE__);
    for( const char* p=star.c_str(); *p; p++ ) line.push(__FILE__,__LINE__, *p );
    pseV->pfb->PushLine( line );
  }
  // 3. Clear buffer editor un-saved change status
  pseV->pfb->ClearChanged();

  // 4. If search editor is displayed, update search editor window
  for( unsigned w=0; w<num_wins; w++ )
  {
    if( SE_FILE == file_hist[w][0] )
    {
      views[w][ SE_FILE ]->Update();
    }
  }
}

void Vis::GoToBuffer( const unsigned buf_idx )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( views[win].len() <= buf_idx )
  {
    CmdLineMessage( "Buffer %lu does not exist", buf_idx );
  }
  else {
    if( buf_idx == file_hist[win][0] )
    {
      // User asked for view that is currently displayed.
      // Dont do anything, just put cursor back in place.
      CV()->PrintCursor();
    }
    else {
      file_hist[win].insert(__FILE__,__LINE__, 0, buf_idx );

      // Remove subsequent buf_idx's from file_hist[win]:
      for( unsigned k=1; k<file_hist[win].len(); k++ )
      {
        if( buf_idx == file_hist[win][k] ) file_hist[win].remove( k );
      }
      View* nv = CV(); // New View to display
      if( ! nv->Has_Context() )
      {
        // Look for context for the new view:
        bool found_context = false;
        for( unsigned w=0; !found_context && w<num_wins; w++ )
        {
          View* v = views[w][ buf_idx ];
          if( v->Has_Context() )
          {
            found_context = true;

            nv->Set_Context( *v );
          }
        }
      }
      nv->SetTilePos( PV()->tile_pos );
      nv->Update();
    }
  }
}

void Vis::SetWinToBuffer( const unsigned win_idx
                        , const unsigned buf_idx
                        , const bool     update )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( views[win_idx].len() <= buf_idx )
  {
    CmdLineMessage( "Buffer %lu does not exist", buf_idx );
  }
  else {
    if( buf_idx == file_hist[win_idx][0] )
    {
      // User asked for view that is currently displayed in win_idx.
      // Dont do anything.
    }
    else {
      file_hist[win_idx].insert(__FILE__,__LINE__, 0, buf_idx );

      // Remove subsequent buf_idx's from file_hist[win_idx]:
      for( unsigned k=1; k<file_hist[win_idx].len(); k++ )
      {
        if( buf_idx == file_hist[win_idx][k] ) file_hist[win_idx].remove( k );
      }
      View* pV_curr = views[win_idx][ file_hist[win_idx][0] ];
      View* pV_prev = views[win_idx][ file_hist[win_idx][1] ];

                   pV_curr->SetTilePos( pV_prev->tile_pos );
      if( update ) pV_curr->Update();
    }
  }
}

void Vis::GoToCurrBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // CVI = Current View Index
  const unsigned CVI = file_hist[win][0];

  if( CVI == BE_FILE
   || CVI == HELP_FILE
   || CVI == SE_FILE )  
  {
    GoToBuffer( file_hist[win][1] );
  }
  else {
    // User asked for view that is currently displayed.
    // Dont do anything, just put cursor back in place.
    CV()->PrintCursor();
  }
}

void Vis::GoToNextBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned FILE_HIST_LEN = file_hist[win].len();

  if( FILE_HIST_LEN <= 1 )
  {
    // Nothing to do, so just put cursor back
    CV()->PrintCursor();
  }
  else {
    View*    const pV_old = CV();
    Tile_Pos const tp_old = pV_old->tile_pos;

    // Move view index at back to front of file_hist
    unsigned view_index_new = 0;
    file_hist[win].pop( view_index_new );
    file_hist[win].insert(__FILE__,__LINE__, 0, view_index_new );

    // Redisplay current window with new view:
    CV()->SetTilePos( tp_old );
    CV()->Update();
  }
}

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

//-------------------------------
//| f1 | be | bh | f4 | f3 | f2 |
//-------------------------------

void Vis::GoToPrevBuffer()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned FILE_HIST_LEN = file_hist[win].len();

  if( FILE_HIST_LEN <= 1 )
  {
    // Nothing to do, so just put cursor back
    CV()->PrintCursor();
  }
  else {
    View*    const pV_old = CV();
    Tile_Pos const tp_old = pV_old->tile_pos;

    // Move view index at front to back of file_hist
    unsigned view_index_old = 0;
    file_hist[win].remove( 0, view_index_old );
    file_hist[win].push(__FILE__,__LINE__, view_index_old );

    // Redisplay current window with new view:
    CV()->SetTilePos( tp_old );
    CV()->Update();
  }
}

void Vis::BufferEditor()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToBuffer( BE_FILE );
}

void Vis::BufferMessage()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToBuffer( MSG_FILE );
}

void Vis::BufferShell()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToBuffer( CMD_FILE );
}

void Vis::Ch_Dir()
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
    const char* fname = CV()->pfb->file_name.c_str();
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
    CmdLineMessage( "chdir(%s) failed", path );
    CV()->PrintCursor();
  }
  else {
    GetCWD();
  }
}

void Vis::GetCWD()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned FILE_NAME_LEN = 1024;
  char cwd[ FILE_NAME_LEN ];

  if( ! getcwd( cwd, FILE_NAME_LEN ) )
  {
    CmdLineMessage( "getcwd() failed" );
  }
  else {
    CmdLineMessage( "%s", cwd );
  }
  CV()->PrintCursor();
}

void Vis::Set_Syntax()
{
  const char* syn = strchr( m_cbuf, '=' );

  if( NULL != syn )
  {
    // Move past '='
    syn++;
    if( 0 != *syn )
    {
      // Something after the '=' 
      CV()->pfb->Set_File_Type( syn );
    }
  }
}

void Vis::SearchEditor()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToBuffer( SE_FILE );
}

void Vis::VSplitWindow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV();
  const Tile_Pos cv_tp = cv->tile_pos;

  if( num_wins < MAX_WINS 
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
    ASSERT( __LINE__, win < num_wins, "win < num_wins" );

    file_hist[num_wins] = file_hist[win];

    View* nv = views[num_wins][ file_hist[num_wins][0] ];

    nv-> topLine     = cv-> topLine    ;
    nv-> leftChar    = cv-> leftChar   ;
    nv-> crsRow      = cv-> crsRow     ;
    nv-> crsCol      = cv-> crsCol     ;

    win = num_wins;
    num_wins++;

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

void Vis::HSplitWindow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* cv = CV();
  const Tile_Pos cv_tp = cv->tile_pos;

  if( num_wins < MAX_WINS 
   && ( cv_tp == TP_FULL
     || cv_tp == TP_LEFT_HALF
     || cv_tp == TP_RITE_HALF
     || cv_tp == TP_LEFT_QTR
     || cv_tp == TP_RITE_QTR
     || cv_tp == TP_LEFT_CTR__QTR
     || cv_tp == TP_RITE_CTR__QTR ) )
  {
    ASSERT( __LINE__, win < num_wins, "win < num_wins" );

    file_hist[num_wins] = file_hist[win];

    View* nv = views[num_wins][ file_hist[num_wins][0] ];

    nv-> topLine     = cv-> topLine    ;
    nv-> leftChar    = cv-> leftChar   ;
    nv-> crsRow      = cv-> crsRow     ;
    nv-> crsCol      = cv-> crsCol     ;

    win = num_wins;
    num_wins++;

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

void Vis::AdjustViews()
{
  for( unsigned w=0; w<num_wins; w++ )
  {
    views[w][ file_hist[w][0] ]->SetViewPos();
  }
}

void Vis::GoToNextWindow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < num_wins )
  {
    const unsigned win_old = win;

    win = (++win) % num_wins;

    View* pV     = views[win    ][ file_hist[win    ][0] ];
    View* pV_old = views[win_old][ file_hist[win_old][0] ];

    pV_old->Print_Borders();
    pV    ->Print_Borders();

    Console::Update();

    m_diff_mode ? diff.PrintCursor( pV ) : pV->PrintCursor();
  }
}

void Vis::GoToNextWindow_l()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < num_wins )
  {
    const unsigned win_old = win;

    // If next view to go to was not found, dont do anything, just return
    // If next view to go to is found, win will be updated to new value
    if( GoToNextWindow_l_Find() )
    {
      View* pV     = views[win    ][ file_hist[win    ][0] ];
      View* pV_old = views[win_old][ file_hist[win_old][0] ];

      pV_old->Print_Borders();
      pV    ->Print_Borders();

      Console::Update();

      m_diff_mode ? diff.PrintCursor( pV ) : pV->PrintCursor();
    }
  }
}

bool Vis::GoToNextWindow_l_Find()
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found = false; // Found next view to go to

  const View*    curr_V  = views[win][ file_hist[win][0] ];
  const Tile_Pos curr_TP = curr_V->tile_pos;

  if( curr_TP == TP_LEFT_HALF )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_HALF         == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_HALF )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_HALF     == TP
       || TP_TOP__LEFT_QTR == TP
       || TP_BOT__LEFT_QTR == TP
       || TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP
       || TP_BOT__LEFT_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_HALF         == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_QTR == TP
       || TP_TOP__LEFT_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_HALF         == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_BOT__LEFT_QTR == TP
       || TP_BOT__LEFT_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_LEFT_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP
       || TP_BOT__LEFT_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_LEFT_CTR__QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_HALF         == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_CTR__QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP
       || TP_BOT__RITE_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_CTR__QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_HALF         == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_HALF         == TP
       || TP_RITE_CTR__QTR     == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_QTR      == TP
       || TP_BOT__RITE_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_QTR == TP
       || TP_TOP__LEFT_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_HALF     == TP
       || TP_LEFT_QTR      == TP
       || TP_BOT__LEFT_QTR == TP
       || TP_BOT__LEFT_8TH == TP ) { win = k; found = true; }
    }
  }
  return found;
}

void Vis::GoToNextWindow_h()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < num_wins )
  {
    const unsigned win_old = win;

    // If next view to go to was not found, dont do anything, just return
    // If next view to go to is found, win will be updated to new value
    if( GoToNextWindow_h_Find() )
    {
      View* pV     = views[win    ][ file_hist[win    ][0] ];
      View* pV_old = views[win_old][ file_hist[win_old][0] ];

      pV_old->Print_Borders();
      pV    ->Print_Borders();

      Console::Update();

      m_diff_mode ? diff.PrintCursor( pV ) : pV->PrintCursor();
    }
  }
}

bool Vis::GoToNextWindow_h_Find()
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found = false; // Found next view to go to

  const View*    curr_V  = views[win][ file_hist[win][0] ];
  const Tile_Pos curr_TP = curr_V->tile_pos;

  if( curr_TP == TP_LEFT_HALF )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_HALF     == TP
       || TP_TOP__RITE_QTR == TP
       || TP_BOT__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP
       || TP_BOT__RITE_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_HALF )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_HALF     == TP
       || TP_TOP__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_HALF         == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_HALF     == TP
       || TP_BOT__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_BOT__RITE_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_HALF         == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_LEFT_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_HALF     == TP
       || TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP
       || TP_BOT__RITE_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_LEFT_CTR__QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP
       || TP_BOT__LEFT_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_RITE_CTR__QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_HALF         == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_HALF     == TP
       || TP_TOP__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_TOP__RITE_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_HALF     == TP
       || TP_BOT__RITE_QTR == TP
       || TP_RITE_QTR      == TP
       || TP_BOT__RITE_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_QTR      == TP
       || TP_TOP__LEFT_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_QTR      == TP
       || TP_BOT__LEFT_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_LEFT_HALF         == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_LEFT_CTR__QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_CTR__QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_RITE_CTR__QTR     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  return found;
}

void Vis::GoToNextWindow_jk()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 1 < num_wins )
  {
    const unsigned win_old = win;

    // If next view to go to was not found, dont do anything, just return
    // If next view to go to is found, win will be updated to new value
    if( GoToNextWindow_jk_Find() )
    {
      View* pV     = views[win    ][ file_hist[win    ][0] ];
      View* pV_old = views[win_old][ file_hist[win_old][0] ];

      pV_old->Print_Borders();
      pV    ->Print_Borders();

      Console::Update();

      m_diff_mode ? diff.PrintCursor( pV ) : pV->PrintCursor();
    }
  }
}

bool Vis::GoToNextWindow_jk_Find()
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool found = false; // Found next view to go to

  const View*    curr_V  = views[win][ file_hist[win][0] ];
  const Tile_Pos curr_TP = curr_V->tile_pos;

  if( curr_TP == TP_TOP__HALF )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_BOT__HALF         == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_BOT__LEFT_8TH     == TP
       || TP_BOT__RITE_8TH     == TP
       || TP_BOT__LEFT_CTR_8TH == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__HALF )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_TOP__HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_TOP__LEFT_8TH     == TP
       || TP_TOP__RITE_8TH     == TP
       || TP_TOP__LEFT_CTR_8TH == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_BOT__HALF         == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_BOT__LEFT_8TH     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_BOT__HALF         == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_BOT__RITE_8TH     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_TOP__HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_TOP__LEFT_8TH     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_QTR )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_TOP__HALF         == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_TOP__RITE_8TH     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_BOT__HALF     == TP
       || TP_BOT__LEFT_QTR == TP
       || TP_BOT__LEFT_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_BOT__HALF     == TP
       || TP_BOT__RITE_QTR == TP
       || TP_BOT__RITE_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_BOT__HALF         == TP
       || TP_BOT__LEFT_QTR     == TP
       || TP_BOT__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_TOP__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_BOT__HALF         == TP
       || TP_BOT__RITE_QTR     == TP
       || TP_BOT__RITE_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_TOP__HALF     == TP
       || TP_TOP__LEFT_QTR == TP
       || TP_TOP__LEFT_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_TOP__HALF     == TP
       || TP_TOP__RITE_QTR == TP
       || TP_TOP__RITE_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__LEFT_CTR_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_TOP__HALF         == TP
       || TP_TOP__LEFT_QTR     == TP
       || TP_TOP__LEFT_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  else if( curr_TP == TP_BOT__RITE_CTR_8TH )
  {
    for( unsigned k=0; !found && k<num_wins; k++ )
    {
      const Tile_Pos TP = (views[k][ file_hist[k][0] ])->tile_pos;
      if( TP_TOP__HALF         == TP
       || TP_TOP__RITE_QTR     == TP
       || TP_TOP__RITE_CTR_8TH == TP ) { win = k; found = true; }
    }
  }
  return found;
}

void Vis::FlipWindows()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 1 < num_wins )
  {
    // This code only works for MAX_WINS == 2
    View* pV1 = views[0][ file_hist[0][0] ];
    View* pV2 = views[1][ file_hist[1][0] ];

    if( pV1 != pV2 )
    {
      // Swap pV1 and pV2 Tile Positions:
      Tile_Pos tp_v1 = pV1->tile_pos;
      pV1->SetTilePos( pV2->tile_pos );
      pV2->SetTilePos( tp_v1 );
    }
    UpdateAll();
  }
}

#ifndef WIN32
void Vis::RunCommand()
{
  Trace trace( __PRETTY_FUNCTION__ );
  String cmd;
  bool ok = RunCommand_GetCommand( cmd );
  if( !ok )
  {
    CV()->PrintCursor();
  }
  else {
    FileBuf* pfb = CV()->pfb;
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

bool Vis::RunCommand_GetCommand( String& cmd )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( CMD_FILE != file_hist[win][0] ) return false;

  FileBuf* pfb = CV()->pfb;
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
bool Vis::RunCommand_RunCommand( const String& cmd
                               , FileBuf* pfb
                               , int& exit_val )
{
  Trace trace( __PRETTY_FUNCTION__ );

  pid_t child_pid = 0;
  FILE* fp = POpenRead( cmd.c_str(), child_pid );
  if( NULL == fp )
  {
    Window_Message("\nPOpenRead( %s ) failed\n\n", cmd.c_str() );
    return false;
  }
  pfb->PushLine();
  m_run_mode = true;
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
  m_run_mode = false;
  return true;
}
#endif

// Line returned has at least SIZE, but zero length
//
Line* Vis::BorrowLine( const char*    _FILE_
                     , const unsigned _LINE_
                     , const unsigned SIZE )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;

  if( line_cache.len() )
  {
    bool ok = line_cache.pop( lp );
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
Line* Vis::BorrowLine( const char*    _FILE_
                     , const unsigned _LINE_
                     , const unsigned LEN
                     , const uint8_t  FILL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;

  if( line_cache.len() )
  {
    bool ok = line_cache.pop( lp );
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
Line* Vis::BorrowLine( const char*    _FILE_
                     , const unsigned _LINE_
                     , const Line&    line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Line* lp = 0;

  if( line_cache.len() )
  {
    bool ok = line_cache.pop( lp );
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
  if( lp ) line_cache.push( lp );
}

LineChange* Vis::BorrowLineChange( const ChangeType type
                                 , const unsigned   lnum
                                 , const unsigned   cpos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  LineChange* lcp = 0;

  if( change_cache.len() )
  {
    bool ok = change_cache.pop( lcp );

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

  if( lcp ) change_cache.push( lcp );
}

int main( int argc, char* argv[] )
{
  PROG_NAME = argv[0];
//printf("%s(%i)\n", __FILE__,__LINE__); fflush( stdout );

  Console::SetSignals();

  if( Console::Set_tty() )
  {
    atexit( Console::AtExit );

    Console::Allocate();

    gl_pCall_Stack = new(__FILE__,__LINE__) ConstCharList(__FILE__,__LINE__);
    gl_pKey        = new(__FILE__,__LINE__) Key;
    gl_pVis        = new(__FILE__,__LINE__) Vis( argc, argv );

    gl_pVis->Run();

    Trace::Print();

    MemMark(__FILE__,__LINE__); delete gl_pVis       ; gl_pVis        = 0;
    MemMark(__FILE__,__LINE__); delete gl_pKey       ; gl_pKey        = 0;
    MemMark(__FILE__,__LINE__); delete gl_pCall_Stack; gl_pCall_Stack = 0;

    Console::Cleanup();
  }
  MemClean();
  Log.Dump();
  return 0;
}

