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

class Vis;
class Key;
class FileBuf;
class String;

class View // Window View
{
public:
  View( Vis& vis, Key& key, FileBuf& fb, LinesList& reg );
  ~View();

  FileBuf* GetFB() const;

  unsigned WinCols() const;
  unsigned WinRows() const;
  unsigned X() const;
  unsigned Y() const;

  unsigned GetTopLine () const;
  unsigned GetLeftChar() const;
  unsigned GetCrsRow  () const;
  unsigned GetCrsCol  () const;

  void SetTopLine ( const unsigned val );
  void SetLeftChar( const unsigned val );
  void SetCrsRow  (       unsigned val );
  void SetCrsCol  ( const unsigned val );

  unsigned WorkingRows() const;
  unsigned WorkingCols() const;
  unsigned CrsLine  () const;
  unsigned BotLine  () const;
  unsigned CrsChar  () const;
  unsigned RightChar() const;

  unsigned Row_Win_2_GL( const unsigned win_row ) const;
  unsigned Col_Win_2_GL( const unsigned win_col ) const;
  unsigned Line_2_GL( const unsigned file_line ) const;
  unsigned Char_2_GL( const unsigned line_char ) const;
  unsigned Sts__Line_Row() const;
  unsigned File_Line_Row() const;
  unsigned Cmd__Line_Row() const;
  Tile_Pos GetTilePos() const;
  void     SetTilePos( const Tile_Pos tp );
  void     SetViewPos();

  bool GetInsertMode() const;
  void SetInsertMode( const bool val );
  bool GetReplaceMode() const;
  void SetReplaceMode( const bool val );

  void GoUp();
  void GoDown();
  void GoLeft();
  void GoRight();
  void PageDown();
  void PageUp();
  void GoToBegOfLine();
  void GoToEndOfLine();
  void GoToBegOfNextLine();
  void GoToTopLineInView();
  void GoToMidLineInView();
  void GoToBotLineInView();
  void GoToLine( const unsigned user_line_num );
  void GoToTopOfFile();
  void GoToEndOfFile();
  void GoToStartOfRow();
  void GoToEndOfRow();
  void GoToNextWord();
  void GoToPrevWord();
  void GoToEndOfWord();
  void GoToOppositeBracket();
  void GoToLeftSquigglyBracket();
  void GoToRightSquigglyBracket();
  void MoveCurrLineToTop();
  void MoveCurrLineCenter();
  void MoveCurrLineToBottom();

  void Do_a();
  void Do_A();
  void Do_cw();
  void Do_dd();
  int  Do_dw();
  void Do_D();
  void Do_f( const char FAST_CHAR );
  void Do_i();
  void Do_J();
  void Do_n();
  void Do_N();
  void Do_o();
  void Do_O();
  void Do_p();
  void Do_P();
  void Do_R();
  void Do_s();
  void Do_Tilda();
  void Do_u();
  void Do_U();
  bool Do_v();
  bool Do_V();
  void Do_x();
  void Do_yy();
  void Do_yw();

  String Do_Star_GetNewPattern();
  void   PrintPatterns( const bool HIGHLIGHT );

  void GoToCrsPos_NoWrite( const unsigned ncp_crsLine
                         , const unsigned ncp_crsChar );
  void GoToCrsPos_Write( const unsigned ncp_crsLine
                       , const unsigned ncp_crsChar );
  void GoToFile();
  bool GoToFile_GetFileName( String& fname );
  void GoToCmdLineClear( const char* S );

  bool MoveInBounds();

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

  void Update( const bool PRINT_CURSOR = true );
  void RepositionView();
  void Print_Borders();
  void PrintStsLine();
  void PrintFileLine();
  void PrintCmdLine();
  void PrintCursor();
  void PrintWorkingView();
  void PrintWorkingView_Set( const unsigned LL
                           , const unsigned G_ROW
                           , const unsigned col
                           , const unsigned i
                           , const unsigned byte
                           , const Style    s );
  void DisplayMapping();

  bool GetStsLineNeedsUpdate() const;
  bool GetUnSavedChangeSts() const;
  void SetStsLineNeedsUpdate( const bool val );
  void SetUnSavedChangeSts( const bool val );

  void Clear_Context();
  bool Has_Context();
  void Set_Context( View& vr );

  bool GoToDir();

  struct Data;

private:
  Data& m;
};

#endif

