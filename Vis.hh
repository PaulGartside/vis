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

#ifndef __VIS_HH__
#define __VIS_HH__

#include "Types.hh"
#include "String.hh"
#include "Diff.hh"
#include "Colon.hh"

const uint16_t MAX_COLS   = 1024; // Arbitrary maximum char width of window
const unsigned BE_FILE    = 0;    // Buffer editor file
const unsigned HELP_FILE  = 1;    // Help          file
const unsigned SE_FILE    = 2;    // Search editor file
const unsigned MSG_FILE   = 3;    // Message       file
const unsigned CMD_FILE   = 4;    // Command Shell file

typedef gArray_t<String*>     StringList;

struct Vis
{
  Vis( const int ARGC, const char* const ARGV[] );
  ~Vis();

  void InitBufferEditor();
  void InitHelpBuffer();
  void InitSearchEditor();
  void InitMsgBuffer();
  void InitCmdBuffer();
  bool InitUserBuffers( const int ARGC, const char* const ARGV[] );
  void InitCmdFuncs();
  void InitFileHistory();

  void CheckWindowSize();
  void CheckFileModTime();

  void Run();
  void Quit();
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
  void QuitAll();
  void Help();
  void DoDiff();
  View* DoDiff_FindRegFileView( const FileBuf* pfb_reg
                              , const FileBuf* pfb_dir
                              , const unsigned win_idx
                              ,       View*    pv );
  View* DoDiff_CheckPossibleFile( const int win_idx
                                , const char* pos_fname );
  void NoDiff();
  void AddToBufferEditor( const char* fname );
  void CmdLineMessage( const char* const msg_fmt, ... );
  void Window_Message( const char* const msg_fmt, ... );
  void Handle_Cmd();
  void HandleColon_b();
  void HandleColon_e();
  void HandleColon_w();
  void HandleColon_hi();
  void HandleColon_MapStart();
  void HandleColon_MapEnd();
  void HandleColon_MapShow();
  void HandleColon_Cover();
  void HandleColon_CoverKey();

  void UpdateAll();
  bool Update_Status_Lines();
  bool Update_Change_Statuses();

  void GoToFile();
  bool GoToFile_GoToBuffer( String& fname );
  bool HaveFile( const char* file_name, unsigned* index=0 );
  bool FName_2_FNum( const String& full_fname, unsigned& file_num );
  bool File_Is_Displayed( const String& full_fname );
  bool File_Is_Displayed( const unsigned file_num );
  void ReleaseFileName( const String& full_fname );
  void ReleaseFileNum( const unsigned file_num );

  void GoToBuffer( const unsigned buf_idx );
  void SetWinToBuffer( const unsigned win_idx
                     , const unsigned buf_idx
                     , const bool     update );
  void GoToCurrBuffer();
  void GoToNextBuffer();
  void GoToPrevBuffer();
  void BufferEditor();
  void BufferMessage();
  void BufferShell();
  void Ch_Dir();
  void GetCWD();
  void SearchEditor();
  void VSplitWindow();
  void HSplitWindow();
  void AdjustViews();
  void GoToNextWindow();
  void GoToNextWindow_l();
  bool GoToNextWindow_l_Find();
  void GoToNextWindow_h();
  bool GoToNextWindow_h_Find();
  void GoToNextWindow_jk();
  bool GoToNextWindow_jk_Find();
  void FlipWindows();
#ifndef WIN32
  void RunCommand();
  bool RunCommand_GetCommand( String& cmd );
  void RunCommand_RunCommand( String& cmd );
#endif

  void Do_Star_Update_Search_Editor();
  void Do_Star_PrintPatterns( const bool HIGHLIGHT );
  void Do_Star_FindPatterns();
  void Do_Star_ClearPatterns();

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
  void Handle_f();
  void Handle_SemiColon();
  void Handle_Percent();
  void Handle_LeftSquigglyBracket();
  void Handle_RightSquigglyBracket();
  void Handle_F();
  void Handle_B();
  void Handle_Colon();
  void Handle_Slash();
  void Handle_Slash_GotPattern( const String& pattern
                              , const bool MOVE_TO_FIRST_PATTERN=true );
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
  void Handle_n();
  void Handle_N();
  void Handle_u();
  void Handle_U();
  void Handle_z();

  Line* BorrowLine( const char* _FILE_, const unsigned _LINE_, const unsigned SIZE = 0 );
  Line* BorrowLine( const char* _FILE_, const unsigned _LINE_, const unsigned LEN, const uint8_t FILL );
  Line* BorrowLine( const char* _FILE_, const unsigned _LINE_, const Line& line );
  void  ReturnLine( Line* lp );

  LineChange* BorrowLineChange( const ChangeType type
                              , const unsigned   lnum
                              , const unsigned   cpos );
  void  ReturnLineChange( LineChange* lcp );

  FileList files;      // list of file buffers
  unsigned win;        // Sub-window index
  unsigned num_wins;   // Number of sub-windows currently on screen
  ViewList views[MAX_WINS];     // Array of lists of file views
  unsList  file_hist[MAX_WINS]; // Array of lists of view history. [win][view_num]
  View*    CV() { return views[win][ file_hist[win][0] ]; }
  View*    PV() { return views[win][ file_hist[win][1] ]; }
  char     m_cbuf[MAX_COLS];
  int      tabStop;
  bool     running;
  LinesList reg;     // Register
  LinesList line_cache;
  ChangeList change_cache;
  Paste_Mode paste_mode;
  String   star;     // current text highlighted by '*' command
  bool     slash;    // indicated whether star pattern is slash type or * type
  String   m_sbuf;   // General purpose string buffer
  int      fast_char; // Char on line to goto when ';' is entered
  Diff     diff;
  bool     diff_mode; // true if displaying diff
  Colon    m_colon;

  typedef void (Vis::*CmdFunc) ();
  CmdFunc CmdFuncs[128];
};

#endif

