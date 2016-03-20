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

#include "Types.hh"

class View;
class FileBuf;

enum Diff_Type
{
  DT_UNKN0WN,
  DT_SAME,
  DT_CHANGED,
  DT_INSERTED,
  DT_DELETED
};

struct CompArea // Comparison area
{
  unsigned stl_s; // Start  line in short file
  unsigned fnl_s; // Finish line in short file

  unsigned stl_l; // Start  line in long file
  unsigned fnl_l; // Finish line in long file
};

struct SameArea
{
  unsigned ln_s;   // Beginning line number in short file
  unsigned ln_l;   // Beginning line number in long  file
  unsigned nlines; // Number of consecutive lines the same
  unsigned nbytes; // Number of bytes in consecutive lines the same

  void Clear();
  void Init( unsigned _ln_s, unsigned _ln_l, unsigned _nbytes );
  void Inc( unsigned _nbytes );
  void Set( const SameArea& a );
};

struct SameLineSec
{
  unsigned ch_s;   // Beginning char number in short line
  unsigned ch_l;   // Beginning char number in long  line
  unsigned nbytes; // Number of consecutive bytes the same

  void Init( unsigned _ch_s, unsigned _ch_l );
  void Set( const SameLineSec& a );
};

typedef Array_t<Diff_Type> LineInfo;

struct SimLines // Similar lines
{
  unsigned ln_s;   // Line number in short comp area
  unsigned ln_l;   // Line number in long  comp area
  unsigned nbytes; // Number of bytes in common between lines
  LineInfo* li_s; // Line comparison info in short comp area
  LineInfo* li_l; // Line comparison info in long  comp area
};

struct DiffArea
{
  unsigned ln_s;     // Beginning line number in short file
  unsigned ln_l;     // Beginning line number in long  file
  unsigned nlines_s; // Number of consecutive lines different in short file
  unsigned nlines_l; // Number of consecutive lines different in long  file
};

struct Diff_Info
{
  Diff_Type diff_type;
  unsigned  line_num;  // Line number in file to which this Diff_Info applies (view line)
  LineInfo* pLineInfo;
};

class Diff
{
public:
  Diff();
  ~Diff();

  bool   Run( View* const pv0, View* const pv1 );
  void   RunDiff( const CompArea CA );
  void CleanDiff();
  bool DiffSameAsPrev( View* const pv0, View* const pv1 );
  void Update();

  bool On_Deleted_View_Line_Zero( const unsigned DL );

  void Patch_Diff_Info_Changed ( View* pV, const unsigned DPL );
  void Patch_Diff_Info_Inserted( View* pV, const unsigned DPL, const bool ON_DELETED_VIEW_LINE_ZERO=false );
  void Patch_Diff_Info_Inserted_Inc( const unsigned DPL
                                   , const bool ON_DELETED_VIEW_LINE_ZERO
                                   , Array_t<Diff_Info>& cDI_List );
  void Patch_Diff_Info_Deleted ( View* pV, const unsigned DPL );

  unsigned  NumLines() const;
  unsigned  LineLen() const;
  unsigned  DiffLine( const View* pV, const unsigned view_line );
  unsigned  ViewLine( const View* pV, const unsigned diff_line );
  Diff_Type DiffType( const View* pV, const unsigned diff_line );

  void PageDown();
  void PageUp();
  void GoDown();
  void GoUp();
  void GoRight();
  void GoLeft();
  void GoToBegOfLine();
  void GoToEndOfLine();
  void GoToBegOfNextLine();
  void GoToLine( const unsigned user_line_num );
  void GoToTopLineInView();
  void GoToBotLineInView();
  void GoToMidLineInView();
  void GoToTopOfFile();
  void GoToEndOfFile();
  void GoToStartOfRow();
  void GoToEndOfRow();
  void GoToOppositeBracket();
  void GoToLeftSquigglyBracket();
  void GoToRightSquigglyBracket();
  void GoToOppositeBracket_Forward( const char ST_C, const char FN_C );
  void GoToOppositeBracket_Backward( const char ST_C, const char FN_C );
  void GoToNextWord();
  bool GoToNextWord_GetPosition( CrsPos& ncp );
  void GoToPrevWord();
  bool GoToPrevWord_GetPosition( CrsPos& ncp );
  void GoToEndOfWord();
  bool GoToEndOfWord_GetPosition( CrsPos& ncp );
  void Do_n();
  void Do_n_Pattern();
  bool Do_n_FindNextPattern( CrsPos& ncp );
  void Do_n_Diff();
  bool Do_n_Search_for_Same( unsigned& dl, const Array_t<Diff_Info>& DI_List );
  bool Do_n_Search_for_Diff( unsigned& dl, const Array_t<Diff_Info>& DI_List );
  void Do_N();
  void Do_N_Pattern();
  bool Do_N_FindPrevPattern( CrsPos& ncp );
  void Do_N_Diff();
  bool Do_N_Search_for_Same( int& dl, const Array_t<Diff_Info>& DI_List );
  bool Do_N_Search_for_Diff( int& dl, const Array_t<Diff_Info>& DI_List );
  void Do_f( const char FAST_CHAR );
  bool MoveInBounds();
  void MoveCurrLineToTop();
  void MoveCurrLineCenter();
  void MoveCurrLineToBottom();

  void Do_i();
  void Do_a();
  void Do_A();
  void Do_o();
  void Do_O();
  void Do_x();
  void Do_s();
  void Do_D();
  void Do_J();
  void Do_dd();
  void Do_dw();
  void Do_cw();
  void Do_yy();
  void Do_y_v_st_fn();
  void Do_Y_v_st_fn();
  void Do_y_v_block();
  void Do_Y_v();
  void Do_y_v();
  void Do_D_v();
  void Do_D_v_find_new_crs_pos();
  void Do_x_v();
  void Do_x_range();
  void Do_x_range_pre();
  void Do_x_range_post( unsigned st_line, unsigned st_char );
  void Do_x_range_block();
  void Do_x_range_single( const unsigned L
                        , const unsigned st_char
                        , const unsigned fn_char );
  void Do_x_range_multiple( const unsigned st_line
                          , const unsigned st_char
                          , const unsigned fn_line
                          , const unsigned fn_char );
  void Do_s_v();
  void Do_Tilda_v();
  void Do_Tilda_v_block();
  void Do_Tilda_v_st_fn();
  void Do_p();
  void Do_p_line();
  void Do_p_or_P_st_fn( Paste_Pos paste_pos );
  void Do_p_or_P_st_fn_FirstLine( Paste_Pos      paste_pos
                                , const unsigned k
                                , const unsigned ODL
                                , const unsigned OVL
                                , const bool     ON_DELETED );
  void Do_p_or_P_st_fn_LastLine( const unsigned k
                               , const unsigned ODL
                               , const unsigned OVL
                               , const bool     ON_DELETED );
  void Do_p_or_P_st_fn_IntermediatLine( const unsigned k
                                      , const unsigned ODL
                                      , const unsigned OVL
                                      , const bool     ON_DELETED );
  void Do_p_block();
  void Do_p_block_Insert_Line( const unsigned k
                             , const unsigned DL
                             , const unsigned VL
                             , const unsigned ISP );
  void Do_p_block_Change_Line( const unsigned k
                             , const unsigned DL
                             , const unsigned VL
                             , const unsigned ISP );
  void Do_P();
  void Do_P_line();
//void Do_P_st_fn();
  void Do_P_block();
  bool Do_v();
  bool Do_V();
  bool Do_visualMode();
  void Undo_v();
  void Do_v_Handle_g();
  void Do_v_Handle_gp();
  void PageDown_v();
  void PageUp_v();
  void Do_R();
  void ReplaceAddReturn();
  void ReplaceAddChar( const char C );
  void ReplaceAddChar_ON_DELETED( const char C
                                , const unsigned DL
                                , const unsigned VL );
  void Do_Tilda();

  String Do_Star_GetNewPattern();

  unsigned WorkingRows( View* pV ) const;
  unsigned WorkingCols( View* pV ) const;
  unsigned CrsLine  () const;
  unsigned CrsChar  () const;
  unsigned BotLine  ( View* pV ) const;
  unsigned RightChar( View* pV ) const;
  unsigned Row_Win_2_GL( View* pV, const unsigned win_row ) const;
  unsigned Col_Win_2_GL( View* pV, const unsigned win_col ) const;
  unsigned Line_2_GL( View* pV, const unsigned file_line ) const;
  unsigned Char_2_GL( View* pV, const unsigned line_char ) const;

  void PrintCursor( View* pV );
  bool Update_Status_Lines();

private:
//void Calc_Chk_Sums();
  void Popu_SameList( const CompArea CA );
  void Sort_SameList();
  void PrintSameList();
  void Popu_DiffList( const CompArea CA );
  void Popu_DiffList_Begin( const CompArea CA );
  void Popu_DiffList_End( const CompArea CA );
  void PrintDiffList();
  void Popu_DI_List( const CompArea CA );
  void PrintDI_List( const CompArea CA );
  void Popu_DI_List_NoSameArea();
  void Popu_DI_List_NoDiffArea();
  void Popu_DI_List_DiffAndSame( const CompArea CA );
  void Popu_DI_List_AddSame( const SameArea sa );
  void Popu_DI_List_AddDiff( const DiffArea da );
  void Popu_DI_List_AddDiff_Common
       (
         const unsigned da_ln_s // Beginning line number of shorter side
       , const unsigned da_ln_l // Beginning line number of longer  side
       , const unsigned da_nlines_s   // Number of lines on shorter side
       , const unsigned da_nlines_l   // Number of lines on longer  side
       , Array_t<Diff_Info>& DI_List_s // Diff_Info list of shorter side
       , Array_t<Diff_Info>& DI_List_l // Diff_Info list of longer  side
       , FileBuf* pfs              // Pointer to FileBuf of shorter side
       , FileBuf* pfl              // Pointer to Filebuf of linger  side
       );
  void Popu_SimiList( const unsigned da_ln_s
                    , const unsigned da_ln_l
                    , const unsigned da_nlines_s
                    , const unsigned da_nlines_l
                    , FileBuf* pfs
                    , FileBuf* pfl );
  void Sort_SimiList();
  void PrintSimiList();
  void SimiList_2_DI_Lists( const unsigned da_ln_s // Beginning line number of shorter side
                          , const unsigned da_ln_l // Beginning line number of longer  side
                          , const unsigned da_nlines_s   // Number of lines on shorter side
                          , const unsigned da_nlines_l   // Number of lines on longer  side
                          , Array_t<Diff_Info>& DI_List_s // Diff_Info list of shorter side
                          , Array_t<Diff_Info>& DI_List_l ); //ff_Info list of longer  side
  void Print_L();
  void Print_S();

  SameArea Find_Max_Same( const CompArea ca, const unsigned count );
  SimLines Find_Lines_Most_Same( CompArea ca, FileBuf* pfs, FileBuf* pfl );
  unsigned Compare_Lines( Line* ls, LineInfo* li_s
                        , Line* ll, LineInfo* li_l );
  void Fill_In_LineInfo( const unsigned SLL
                       , const unsigned LLL
                       , LineInfo* const pli_s
                       , LineInfo* const pli_l
                       , SameLineSec& max_same
                       , Line* pls
                       , Line* pll );
  unsigned DiffLine_S( const unsigned view_line );
  unsigned DiffLine_L( const unsigned view_line );

  LineInfo* Borrow_LineInfo( const char* _FILE_, const unsigned _LINE_ );
  void      Return_LineInfo( LineInfo* lip );
  void      Clear_SimiList();
  void      Clear_DI_List( Array_t<Diff_Info>& DI_List );
  void      Clear_DI_List_CA( const unsigned st_line
                            , const unsigned fn_line
                            , Array_t<Diff_Info>& DI_List );
  void     Insert_DI_List( const Diff_Info di
                         , Array_t<Diff_Info>& DI_List );

  void GoToCrsPos_Write( const unsigned ncp_crsLine
                       , const unsigned ncp_crsChar );
  void GoToCrsPos_Write_Visual( const unsigned OCL, const unsigned OCP
                              , const unsigned NCL, const unsigned NCP );
  void GoToCrsPos_WV_Forward( const unsigned OCL, const unsigned OCP
                            , const unsigned NCL, const unsigned NCP );
  void GoToCrsPos_WV_Backward( const unsigned OCL, const unsigned OCP
                             , const unsigned NCL, const unsigned NCP );
  void GoToCrsPos_Write_VisualBlock( const int OCL, const int OCP
                                   , const int NCL, const int NCP );
  void GoToCrsPos_NoWrite( const unsigned ncp_crsLine
                         , const unsigned ncp_crsChar );
  void PrintStsLine( View* pV );
  void PrintCmdLine( View* pV );
  void PrintWorkingView( View* pV );
  void PrintWorkingView_DT_CHANGED( View* pV
                                  , const unsigned WC
                                  , const unsigned G_ROW
                                  , const unsigned dl
                                  ,       unsigned col );
  void DisplayBanner();
  void Remove_Banner();
  void Replace_Crs_Char( Style style );
  Style Get_Style( View* pV, const unsigned DL, const unsigned VL, const unsigned pos );
  Style DiffStyle( const Style s );
  bool  InVisualArea( View* pV, const unsigned DL, const unsigned pos );
  bool  InVisualBlock( const unsigned DL, const unsigned pos );
  bool  InVisualStFn( const unsigned DL, const unsigned pos );

  void InsertAddChar( const char c );
  void InsertAddReturn();
  void InsertBackspace();
  void InsertBackspace_RmC( const unsigned DL
                          , const unsigned OCP );
  void InsertBackspace_RmNL( const unsigned DL );

  void Swap_Visual_St_Fn_If_Needed();
  void Swap_Visual_Block_If_Needed();

  unsigned topLine;   // top  of buffer view line number.
  unsigned leftChar;  // left of buffer view character number.
  unsigned crsRow;    // cursor row    in buffer view. 0 <= crsRow < WorkingRows().
  unsigned crsCol;    // cursor column in buffer view. 0 <= crsCol < WorkingCols().

  View* pvS;
  View* pvL;
  FileBuf* pfS;
  FileBuf* pfL;
  double mod_time_s;
  double mod_time_l;

  Array_t<SameArea> sameList;
  Array_t<DiffArea> diffList;

  Array_t<Diff_Info> DI_List_S;
  Array_t<Diff_Info> DI_List_L;

  Array_t<SimLines> simiList;
  Array_t<LineInfo*> line_info_cache;

  unsList chk_sums_s;
  unsList chk_sums_l;

  bool sts_line_needs_update;
  bool  inVisualMode;  // true if in visual  mode, else false
  bool  inVisualBlock; // true if in visual block, else false
  bool m_undo_v;

  unsigned v_st_line;  // Visual start line number
  unsigned v_st_char;  // Visual start char number on line
  unsigned v_fn_line;  // Visual ending line number
  unsigned v_fn_char;  // Visual ending char number on line
};

#endif

