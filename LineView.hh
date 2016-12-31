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

#ifndef __LINE_VIEW_HH__
#define __LINE_VIEW_HH__

#include "View_IF.hh"

class Vis;
class Key;
class FileBuf;
class String;

class LineView : public View_IF // Line View
{
public:
  LineView( Vis&       vis
          , Key&       key
          , FileBuf&   fb
          , LinesList& reg
          , char*      cbuf
          , String&    sbuf
          , const char banner_delim );
  ~LineView();

  FileBuf* GetFB() const;

  unsigned WinCols() const;
  unsigned X() const;
  unsigned Y() const;

  unsigned GetTopLine () const;
  unsigned GetLeftChar() const;
  unsigned GetCrsRow  () const;
  unsigned GetCrsCol  () const;
  const Line* GetCrsLine();

  void SetTopLine ( const unsigned val );
  void SetLeftChar( const unsigned val );
  void SetCrsRow  (       unsigned val );
  void SetCrsCol  ( const unsigned val );

  unsigned WorkingCols() const;
  unsigned CrsLine  () const;
  unsigned BotLine  () const;
  unsigned CrsChar  () const;
  unsigned RightChar() const;

  unsigned Col_Win_2_GL( const unsigned win_col ) const;
  unsigned Char_2_GL( const unsigned line_char ) const;
  Tile_Pos GetTilePos() const;
  void     SetTilePos( const Tile_Pos tp );
  void     SetContext( const unsigned num_cols
                     , const unsigned x
                     , const unsigned y );

  bool GetInsertMode() const;
  void SetInsertMode( const bool val );
  bool GetReplaceMode() const;
  void SetReplaceMode( const bool val );

  void GoUp();
  void GoDown();
  void GoLeft();
  void GoRight();
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

  bool Do_a();
  bool Do_A();
  void Do_cw();
  void Do_dd();
  int  Do_dw();
  void Do_D();
  void Do_f( const char FAST_CHAR );
  bool Do_i();
  void Do_J();
  void Do_n();
  void Do_N();
  bool Do_o();
  void Do_p();
  void Do_P();
  bool Do_R();
  void Do_s();
  void Do_Tilda();
//void Do_u();
//void Do_U();
  bool Do_v();
  void Do_x();
  void Do_yy();
  void Do_yw();

  void GoToCrsPos_NoWrite( const unsigned ncp_crsLine
                         , const unsigned ncp_crsChar );
  void GoToCrsPos_Write( const unsigned ncp_crsLine
                       , const unsigned ncp_crsChar );

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

  void Update();
  void RepositionView();
  void PrintStsLine();
  void PrintFileLine();
  void PrintCursor();
  void PrintWorkingView();
  void PrintWorkingView_Set( const unsigned LL
                           , const unsigned G_ROW
                           , const unsigned col
                           , const unsigned i
                           , const unsigned byte
                           , const Style    s );
  void DisplayMapping();

  bool GetUnSavedChangeSts() const;
  void SetUnSavedChangeSts( const bool val );

  bool Has_Context();
  void Clear_Context();
  void Check_Context();

  const char* GetPathName();

  bool HandleReturn();

  void Cover();
  void CoverKey();

  struct Data;

private:
  Data& m;
};

#endif

