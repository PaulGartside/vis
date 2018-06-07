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

#ifndef __DIFF_HH__
#define __DIFF_HH__

class Vis;
class Key;
class View;

class Diff
{
public:
  Diff( Vis& vis, Key& key, LinesList& reg );
  ~Diff();

  bool   Run( View* const pv0, View* const pv1 );
  void Update();

  View* GetViewShort() const;
  View* GetViewLong() const;

  void Set_Remaining_ViewContext_2_DiffContext();

  unsigned GetTopLine ( View* const pV ) const;
  unsigned GetLeftChar() const;
  unsigned GetCrsRow  () const;
  unsigned GetCrsCol  () const;

  void GoToCrsPos_NoWrite( const unsigned ncp_crsLine
                         , const unsigned ncp_crsChar );

  unsigned DiffLine( const View* pV, const unsigned view_line );

  void Patch_Diff_Info_Inserted( View* pV
                               , const unsigned DPL
                               , const bool ON_DELETED_VIEW_LINE_ZERO=false );
  void Patch_Diff_Info_Deleted( View* pV, const unsigned DPL );
  void Patch_Diff_Info_Changed( View* pV, const unsigned DPL );
  bool On_Deleted_View_Line_Zero( const unsigned DL );

  void GoUp( const int num=1 );
  void GoDown( const unsigned num=1 );
  void GoLeft( const int num=1 );
  void GoRight( const unsigned num=1 );
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

  void GoToFile();

  void PrintCursor( View* pV );
  bool Update_Status_Lines();
  bool ReDiff();

  void Set_Cmd_Line_Msg( const String& msg );

  struct Data;

private:
  Data& m;
};

#endif

