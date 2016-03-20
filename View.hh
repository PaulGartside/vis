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

#ifndef __VIEW_HH__
#define __VIEW_HH__

#include "Types.hh"
#include "String.hh"

class FileBuf;

static unsigned top___border = 1;
static unsigned bottomborder = 1;
static unsigned left__border = 1;
static unsigned right_border = 1;

class View // Window View
{
public:
  View( FileBuf* pfb );

  FileBuf* pfb;
  bool sts_line_needs_update;
  bool us_change_sts; // un-saved change status, true if '+", false if ' '

  Tile_Pos  tile_pos;
  unsigned  nCols;       // number of rows in buffer view
  unsigned  nRows;       // number of columns in buffer view
  unsigned  x;           // Top left x-position of buffer view in parent window
  unsigned  y;           // Top left y-position of buffer view in parent window
  unsigned  topLine;     // top  of buffer view line number.
  unsigned  topLine_displayed;
  unsigned  leftChar;    // left of buffer view character number.
  unsigned  leftChar_displayed;
  unsigned  crsRow;      // cursor row    in buffer view. 0 <= crsRow < WorkingRows().
  unsigned  crsRow_displayed;
  unsigned  crsCol;      // cursor column in buffer view. 0 <= crsCol < WorkingCols().
  unsigned  crsCol_displayed;

  unsigned v_st_line;  // Visual start line number
  unsigned v_st_char;  // Visual start char number on line
  unsigned v_fn_line;  // Visual ending line number
  unsigned v_fn_char;  // Visual ending char number on line

  unsigned WinCols() const { return nCols; }
  unsigned WinRows() const { return nRows; }
  unsigned WorkingRows() const { return WinRows()-3-top___border-bottomborder; }
  unsigned WorkingCols() { return WinCols()-  left__border-right_border; }
  unsigned CrsLine  ()   { return topLine  + crsRow; }
  unsigned BotLine  () const { return topLine  + WorkingRows()-1; }
  unsigned CrsChar  ()   { return leftChar + crsCol; }
  unsigned RightChar()   { return leftChar + WorkingCols()-1; }

  unsigned Row_Win_2_GL( const unsigned win_row ) const;
  unsigned Col_Win_2_GL( const unsigned win_col ) const;
  unsigned Line_2_GL( const unsigned file_line ) const;
  unsigned Char_2_GL( const unsigned line_char ) const;
  unsigned Sts__Line_Row() const;
  unsigned File_Line_Row() const;
  unsigned Cmd__Line_Row() const;

  void SetTilePos( const Tile_Pos tp );
  void SetViewPos();
  void TilePos_2_x();
  void TilePos_2_y();
  void TilePos_2_nRows();
  void TilePos_2_nCols();

  void Update();
  void UpdateLines( const unsigned st_line, const unsigned fn_line );
  void GoToCmdLineClear( const char* S );

  void Do_i();
  void Do_i_vb();
  void Do_a_vb();
  void InsertBackspace_vb();
  void InsertAddChar_vb( const char c );
  void Do_a();
  void Do_A();
  void Do_o();
  void Do_O();
  bool Do_v();
  bool Do_V();
  bool Do_visualMode();
  void Undo_v();
  void Do_v_Handle_g();
  void Do_v_Handle_gf();
  void Do_v_Handle_gp();

  void InsertAddChar( const char c );
  void InsertAddReturn();
  void InsertBackspace();
  void InsertBackspace_RmC( const unsigned OCL
                          , const unsigned OCP );
  void InsertBackspace_RmNL( const unsigned OCL );

  void ReplaceAddReturn();
  void ReplaceBackspace( Line* lp );
  void ReplaceAddChars( const char C );

  void DisplayBanner();
  void Remove_Banner();
  void DisplayMapping();

  void RepositionView();

  void Replace_Crs_Char( Style style );

  void PrintWorkingView();
  void PrintWorkingView_Set( const unsigned LL
                           , const unsigned G_ROW
                           , const unsigned col
                           , const unsigned i
                           , const unsigned byte
                           , const Style    s );
  void PrintLines( const unsigned st_line, const unsigned fn_line );
  void Print_Borders();
  void Print_Borders_Top   ( const Style S );
  void Print_Borders_Bottom( const Style S );
  void Print_Borders_Left  ( const Style S );
  void Print_Borders_Right ( const Style S );
  void PrintStsLine();
  void PrintFileLine();
  void PrintCmdLine();
  void PrintCursor();
  void PrintDo_n();

public:
  bool InVisualArea ( const unsigned line, const unsigned pos );
  bool InVisualStFn ( const unsigned line, const unsigned pos );
  bool InVisualBlock( const unsigned line, const unsigned pos );
  bool InComment    ( const unsigned line, const unsigned pos );
  bool InDefine     ( const unsigned line, const unsigned pos );
  bool InConst      ( const unsigned line, const unsigned pos );
  bool InControl    ( const unsigned line, const unsigned pos );
  bool InVarType    ( const unsigned line, const unsigned pos );
  bool InStar       ( const unsigned line, const unsigned pos );
  bool InNonAscii   ( const unsigned line, const unsigned pos );

  void GoUp();
  void GoDown();
  void GoLeft();
  void GoRight();
  bool GoToFile_GetFileName( String& fname );
  void GoToLine( const unsigned user_line_num );
  void GoToTopLineInView();
  void GoToBotLineInView();
  void GoToMidLineInView();
  void GoToBegOfLine();
  void GoToEndOfLine();
  void GoToBegOfNextLine();
  void GoToStartOfRow();
  void GoToEndOfRow();
  void GoToTopOfFile();
  void GoToEndOfFile();
  void GoToNextWord();
  bool GoToNextWord_GetPosition( CrsPos& ncp );
  void GoToPrevWord();
  bool GoToPrevWord_GetPosition( CrsPos& ncp );
  void GoToEndOfWord();
  bool GoToEndOfWord_GetPosition( CrsPos& ncp );
  void GoToOppositeBracket();
  void GoToLeftSquigglyBracket();
  void GoToRightSquigglyBracket();
  void GoToOppositeBracket_Forward( const char ST_C, const char FN_C );
  void GoToOppositeBracket_Backward( const char ST_C, const char FN_C );
  void GoToCrsPos_Write( const unsigned ncp_crsLine
                       , const unsigned ncp_crsChar );
  void GoToCrsPos_Write_Visual( const unsigned OCL, const unsigned OCP
                              , const unsigned NCL, const unsigned NCP );
  void GoToCrsPos_WV_Forward( const unsigned OCL, const unsigned OCP
                            , const unsigned NCL, const unsigned NCP );
  void GoToCrsPos_WV_Backward( const unsigned OCL, const unsigned OCP
                             , const unsigned NCL, const unsigned NCP );
  void GoToCrsPos_Write_VisualBlock( const unsigned OCL
                                   , const unsigned OCP
                                   , const unsigned NCL
                                   , const unsigned NCP );
  void GoToCrsPos_NoWrite( const unsigned ncp_crsLine
                         , const unsigned ncp_crsChar );
  bool GoToDir();

  bool MoveInBounds();
  void MoveCurrLineToTop();
  void MoveCurrLineCenter();
  void MoveCurrLineToBottom();

  void PageDown();
  void PageDown_v();
  void PageUp();
  void PageUp_v();

  String Do_Star_GetNewPattern();
  void   PrintPatterns( const bool HIGHLIGHT );

  void Do_n();
  void Do_N();
  bool Do_n_FindNextPattern( CrsPos& ncp );
  bool Do_N_FindPrevPattern( CrsPos& ncp );
  void Do_f( const char FAST_CHAR );

  void Do_u();
  void Do_U();

  void Do_Tilda();
  void Do_x();
  void Do_x_v();
  void Do_D_v();  void Do_D_v_line();
  void Do_s();
  void Do_s_v();
  void Do_cw();
  int  Do_dw();
  void Do_dw_get_fn( unsigned& fn_line, unsigned& fn_char );
  void Do_Tilda_v(); void Do_Tilda_v_st_fn(); void Do_Tilda_v_block();
  void Do_dd();
  void Do_yy();
  void Do_yw();
  void Do_y_v(); void Do_y_v_st_fn(); void Do_y_v_block();
  void Do_Y_v(); void Do_Y_v_st_fn();
  void Do_D();
  void Do_p();
  void Do_p_line();
  void Do_p_or_P_st_fn( Paste_Pos paste_pos );
  void Do_p_block();
  void Do_P();
  void Do_P_line();
  void Do_P_block();
  void Do_R();
  void Do_J();
  void Do_x_range( unsigned st_line, unsigned st_char
                 , unsigned fn_line, unsigned fn_char );
  void Do_x_range_block( unsigned st_line, unsigned st_char
                       , unsigned fn_line, unsigned fn_char );
  void Do_x_range_pre( unsigned& st_line, unsigned& st_char
                     , unsigned& fn_line, unsigned& fn_char );
  void Do_x_range_post( const unsigned st_line, const unsigned st_char );
  void Do_x_range_single( const unsigned L
                        , const unsigned st_char
                        , const unsigned fn_char );
  void Do_x_range_multiple( const unsigned st_line, const unsigned st_char
                          , const unsigned fn_line, const unsigned fn_char );
  bool Has_Context();
  void Set_Context( View& vr );

  bool inInsertMode; // true if in insert  mode, else false
  bool inReplaceMode;

private:
  bool  RV_Style( const Style s ) const;
  Style RV_Style_2_NonRV( const Style RVS ) const;
  Style Get_Style( const unsigned line, const unsigned pos );

  bool inVisualMode;
  bool inVisualBlock;
};

#endif

