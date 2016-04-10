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

#include "MemLog.hh"
#include "Key.hh"
#include "Vis.hh"
#include "View.hh"
#include "FileBuf.hh"
#include "Utilities.hh"
#include "Console.hh"
#include "Line.hh"
#include "Diff.hh"

extern Vis* gl_pVis;
extern Key* gl_pKey;
extern MemLog<MEM_LOG_BUF_SIZE> Log;

const char* Diff_Type_2_Str( const Diff_Type dt )
{
  const char* s = "UNKN0WN";

  if     ( dt == DT_UNKN0WN  ) s = "DT_UNKN0WN";
  else if( dt == DT_SAME     ) s = "DT_SAME";
  else if( dt == DT_CHANGED  ) s = "DT_CHANGED";
  else if( dt == DT_INSERTED ) s = "DT_INSERTED";
  else if( dt == DT_DELETED  ) s = "DT_DELETED";

  return s;
}

Diff::Diff()
  : topLine( 0 )
  , leftChar( 0 )
  , crsRow( 0 )
  , crsCol( 0 )
  , pvS( 0 )
  , pvL( 0 )
  , pfS( 0 )
  , pfL( 0 )
  , mod_time_s( 0 )
  , mod_time_l( 0 )
  , sameList(__FILE__, __LINE__)
  , diffList(__FILE__, __LINE__)
  , DI_List_S(__FILE__, __LINE__)
  , DI_List_L(__FILE__, __LINE__)
  , simiList(__FILE__, __LINE__)
  , line_info_cache(__FILE__, __LINE__)
  , sts_line_needs_update( false )
  , inVisualMode ( false )
  , inVisualBlock( false )
  , v_st_line( 0 )
  , v_st_char( 0 )
  , v_fn_line( 0 )
  , v_fn_char( 0 )
{
}

Diff::~Diff()
{
  for( unsigned k=0; k<DI_List_S.len(); k++ )
  {
    MemMark(__FILE__,__LINE__); delete DI_List_S[k].pLineInfo; DI_List_S[k].pLineInfo = 0;
  }
  for( unsigned k=0; k<DI_List_L.len(); k++ )
  {
    MemMark(__FILE__,__LINE__); delete DI_List_L[k].pLineInfo; DI_List_L[k].pLineInfo = 0;
  }
  for( unsigned k=0; k<simiList.len(); k++ )
  {
    MemMark(__FILE__,__LINE__); delete simiList[k].li_s; simiList[k].li_s = 0;
    MemMark(__FILE__,__LINE__); delete simiList[k].li_l; simiList[k].li_l = 0;
  }
  for( unsigned k=0; k<line_info_cache.len(); k++ )
  {
    MemMark(__FILE__,__LINE__); delete line_info_cache[k]; line_info_cache[k] = 0;
  }
}

// Returns true if diff took place, else false
//
bool Diff::Run( View* const pv0, View* const pv1 )
{
  Trace trace( __PRETTY_FUNCTION__ );
  // Each buffer must be displaying a different file to do diff:
  if( pv0->pfb != pv1->pfb )
  {
    const Tile_Pos tp0 = pv0->tile_pos;
    const Tile_Pos tp1 = pv1->tile_pos;

    // Buffers must be vertically split to do diff:
    if( (TP_LEFT_HALF == tp0 && TP_RITE_HALF == tp1)
     || (TP_LEFT_HALF == tp1 && TP_RITE_HALF == tp0) )
    {
      if( DiffSameAsPrev( pv0, pv1 ) )
      {
        // Dont need to re-run the diff, just display the results:
        Update();
      }
      else {
        CleanDiff(); //< Start over with clean slate

        const unsigned nLines_0 = pv0->pfb->NumLines();
        const unsigned nLines_1 = pv1->pfb->NumLines();

        pvS = nLines_0 < nLines_1 ? pv0 : pv1; // Short view
        pvL = nLines_0 < nLines_1 ? pv1 : pv0; // Long  view
        pfS = pvS->pfb;
        pfL = pvL->pfb;
        mod_time_s = pfS->mod_time;
        mod_time_l = pfL->mod_time;

        // All lines in both files:
        CompArea CA = { 0, pfS->NumLines(), 0, pfL->NumLines() };

        RunDiff( CA );
      }
      return true;
    }
  }
  return false;
}

void Diff::RunDiff( const CompArea CA )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Popu_SameList( CA );
  Sort_SameList();
//PrintSameList();
  Popu_DiffList( CA );
//PrintDiffList();
  Popu_DI_List( CA );
//PrintDI_List( CA );

  Update();
}

void Diff::CleanDiff()
{
  sameList.clear();
  diffList.clear();

  Clear_DI_List( DI_List_S );
  Clear_DI_List( DI_List_L );

  Clear_SimiList();

  // Reset some other variables:
  topLine  = 0;
  leftChar = 0;
  crsRow   = 0;
  crsCol   = 0;
  sts_line_needs_update = false;
   inVisualMode = false;
  v_st_line = 0;
  v_st_char = 0;
  v_fn_line = 0;
  v_fn_char = 0;
}

bool Diff::DiffSameAsPrev( View* const pv0, View* const pv1 )
{
        bool DATES_SAME_AS_BEFORE = false;

  const bool FILES_SAME_AS_BEFORE = pfS && pfL &&
                                  (
                                    ( pv0->pfb == pfS && pv1->pfb == pfL )
                                 || ( pv0->pfb == pfL && pv1->pfb == pfS )
                                  );
  if( FILES_SAME_AS_BEFORE )
  {
    DATES_SAME_AS_BEFORE =
    (
      ( mod_time_s == pfS->mod_time && mod_time_l == pfL->mod_time )
   || ( mod_time_l == pfS->mod_time && mod_time_s == pfL->mod_time )
    );
  }
  return FILES_SAME_AS_BEFORE
      && DATES_SAME_AS_BEFORE;
}

void Diff::Update()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Update long view:
  pfL->Find_Styles( ViewLine( pvL, topLine ) + WorkingRows( pvL ) );
  pfL->ClearStars();
  pfL->Find_Stars();

  pvL->RepositionView();
  pvL->Print_Borders();
  PrintWorkingView( pvL );
  PrintStsLine( pvL );
  pvL->PrintFileLine();
  PrintCmdLine( pvL );

  // Update short view:
  pfS->Find_Styles( ViewLine( pvS, topLine ) + WorkingRows( pvS ) );
  pfS->ClearStars();
  pfS->Find_Stars();

  pvS->RepositionView();
  pvS->Print_Borders();
  PrintWorkingView( pvS );
  PrintStsLine( pvS );
  pvS->PrintFileLine();
  PrintCmdLine( pvS );

  Console::Update();

  PrintCursor( gl_pVis->CV() );
}

unsigned Diff::WorkingRows( View* pV ) const { return pV->nRows -5 ; }
unsigned Diff::WorkingCols( View* pV ) const { return pV->nCols -2 ; }
unsigned Diff::CrsLine    () const { return topLine  + crsRow; }
unsigned Diff::CrsChar    () const { return leftChar + crsCol; }
unsigned Diff::BotLine    ( View* pV ) const { return topLine  + WorkingRows( pV )-1; }
unsigned Diff::RightChar  ( View* pV ) const { return leftChar + WorkingCols( pV )-1; }

unsigned Diff::Row_Win_2_GL( View* pV, const unsigned win_row ) const
{
  return pV->y + 1 + win_row;
}

unsigned Diff::Col_Win_2_GL( View* pV, const unsigned win_col ) const
{
  return pV->x + 1 + win_col;
}

// Translates zero based file line number to zero based global row
unsigned Diff::Line_2_GL( View* pV, const unsigned file_line ) const
{
  return pV->y + 1 + file_line - topLine;
}

// Translates zero based file line char position to zero based global column
unsigned Diff::Char_2_GL( View* pV, const unsigned line_char ) const
{
  return pV->x + 1 + line_char - leftChar;
}

unsigned Diff::NumLines() const
{
  // DI_List_L and DI_List_L should be the same length
  return DI_List_L.len();
}

unsigned Diff::LineLen() const
{
  View* pV = gl_pVis->CV();

  const unsigned diff_line = CrsLine();

  Diff_Info& rDI = ( pV == pvS ) ? DI_List_S[ diff_line ]
                                 : DI_List_L[ diff_line ];
  if( DT_UNKN0WN == rDI.diff_type
   || DT_DELETED == rDI.diff_type )
  {
    return 0;
  }
  const unsigned view_line = rDI.line_num;

  return pV->pfb->LineLen( view_line );
}

unsigned Diff::DiffLine( const View* pV, const unsigned view_line )
{
  return ( pV == pvS ) ? DiffLine_S( view_line )
                       : DiffLine_L( view_line );
}

unsigned Diff::ViewLine( const View* pV, const unsigned diff_line )
{
  return ( pV == pvS ) ? DI_List_S[ diff_line ].line_num
                       : DI_List_L[ diff_line ].line_num;
}

Diff_Type Diff::DiffType( const View* pV, const unsigned diff_line )
{
  return ( pV == pvS ) ? DI_List_S[ diff_line ].diff_type
                       : DI_List_L[ diff_line ].diff_type;
}

unsigned Diff::DiffLine_S( const unsigned view_line )
{
  const unsigned LEN = DI_List_S.len();

  // Diff line is greater or equal to view line,
  // so start at view line number and search forward
  bool ok = true;
  for( unsigned k=view_line; k<LEN && ok; k++ )
  {
    Diff_Info di = DI_List_S[ k ];

    if( DT_SAME     == di.diff_type
     || DT_CHANGED  == di.diff_type
     || DT_INSERTED == di.diff_type )
    {
      if( view_line == di.line_num ) return k;
    }
  }
  ASSERT( __LINE__, 0, "view_line : %u : not found", view_line );
  return view_line;
}

unsigned Diff::DiffLine_L( const unsigned view_line )
{
  const unsigned LEN = DI_List_L.len();

  // Diff line is greater or equal to view line,
  // so start at view line number and search forward
  bool ok = true;
  for( unsigned k=view_line; k<LEN && ok; k++ )
  {
    Diff_Info di = DI_List_L[ k ];

    if( DT_SAME     == di.diff_type
     || DT_CHANGED  == di.diff_type
     || DT_INSERTED == di.diff_type )
    {
      if( view_line == di.line_num ) return k;
    }
  }
  ASSERT( __LINE__, 0, "view_line : %u : not found", view_line );
  return view_line;
}

void Diff::PrintCursor( View* pV )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Console::Move_2_Row_Col( Row_Win_2_GL( pV, crsRow )
                         , Col_Win_2_GL( pV, crsCol ) );
  Console::Flush();
}

void Diff::PrintWorkingView( View* pV )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = NumLines();
  const unsigned WR        = WorkingRows( pV );
  const unsigned WC        = WorkingCols( pV );

  unsigned row = 0; // (dl=diff line)
  for( unsigned dl=topLine; dl<NUM_LINES && row<WR; dl++, row++ )
  {
    unsigned col=0;
    const unsigned G_ROW = Row_Win_2_GL( pV, row );
    const Diff_Type DT = DiffType( pV, dl );
    if( DT == DT_UNKN0WN )
    {
      for( ; col<WC; col++ )
      {
        Console::Set( G_ROW, Col_Win_2_GL( pV, col ), '~', S_DIFF_DEL );
      }
    }
    else if( DT == DT_DELETED )
    {
      for( ; col<WC; col++ )
      {
        Console::Set( G_ROW, Col_Win_2_GL( pV, col ), '-', S_DIFF_DEL );
      }
    }
    else if( DT == DT_CHANGED )
    {
      PrintWorkingView_DT_CHANGED( pV, WC, G_ROW, dl, col );
    }
    else // DT == DT_INSERTED || DT == DT_SAME
    {
      const unsigned vl = ViewLine( pV, dl ); //(vl=view line)
      const unsigned LL = pV->pfb->LineLen( vl );
      for( unsigned i=leftChar; i<LL && col<WC; i++, col++ )
      {
        uint8_t c = pV->pfb->Get( vl, i );
        Style   s = Get_Style( pV, dl, vl, i );

        if( DT == DT_INSERTED ) s = DiffStyle( s );
        pV->PrintWorkingView_Set( LL, G_ROW, col, i, c, s );
      }
      for( ; col<WC; col++ )
      {
        Console::Set( G_ROW, Col_Win_2_GL( pV, col ), ' ', DT==DT_SAME ? S_NORMAL : S_DIFF_NORMAL );
      }
    }
  }
  // Not enough lines to display, fill in with ~
  for( ; row < WR; row++ )
  {
    const unsigned G_ROW = Row_Win_2_GL( pV, row );

    Console::Set( G_ROW, Col_Win_2_GL( pV, 0 ), '~', S_EMPTY );

    for( unsigned col=1; col<WC; col++ )
    {
      Console::Set( G_ROW, Col_Win_2_GL( pV, col ), ' ', S_EMPTY );
    }
  }
}

void Diff::PrintWorkingView_DT_CHANGED( View* pV
                                      , const unsigned WC
                                      , const unsigned G_ROW
                                      , const unsigned dl
                                      ,       unsigned col )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned vl = ViewLine( pV, dl ); //(vl=view line)
  const unsigned LL = pV->pfb->LineLen( vl );
  Diff_Info di = (pV == pvS) ? DI_List_S[ dl ] : DI_List_L[ dl ];

  if( di.pLineInfo )
  {
    const unsigned LIL = di.pLineInfo->len();
    unsigned cp = leftChar; // char position
    for( unsigned i=leftChar; cp<LL && i<LIL && col<WC; i++, col++ )
    {
      Diff_Type dt = (*di.pLineInfo)[i];

      if( DT_SAME == dt )
      { 
        Style s    = Get_Style( pV, dl, vl, cp );
        int   byte = pV->pfb->Get( vl, cp );
        pV->PrintWorkingView_Set( LL, G_ROW, col, cp, byte, s );
        cp++;
      }
      else if( DT_CHANGED == dt || DT_INSERTED == dt )
      {
        Style s    = Get_Style( pV, dl, vl, cp ); s = DiffStyle( s );
        int   byte = pV->pfb->Get( vl, cp );
        pV->PrintWorkingView_Set( LL, G_ROW, col, cp, byte, s );
        cp++;
      }
      else if( DT_DELETED == dt )
      {
        Console::Set( G_ROW, Col_Win_2_GL( pV, col ), '-', S_DIFF_DEL );
      }
      else //( DT_UNKN0WN  == dt )
      {
        Console::Set( G_ROW, Col_Win_2_GL( pV, col ), '~', S_DIFF_DEL );
      }
    }
    for( ; col<WC; col++ )
    {
      Console::Set( G_ROW, Col_Win_2_GL( pV, col ), ' ', S_NORMAL );
    }
  }
  else {
    for( unsigned i=leftChar; i<LL && col<WC; i++, col++ )
    {
      Style s = Get_Style( pV, dl, vl, i );
            s = DiffStyle( s );
      int byte = pV->pfb->Get( vl, i );
      pV->PrintWorkingView_Set( LL, G_ROW, col, i, byte, s );
    }
    for( ; col<WC; col++ )
    {
      Console::Set( G_ROW, Col_Win_2_GL( pV, col ), ' ', S_DIFF_NORMAL );
    }
  }
}

void Diff::PrintStsLine( View* pV )
{
  Trace trace( __PRETTY_FUNCTION__ );
  char buf1[  16]; buf1[0] = 0;
  char buf2[1024]; buf2[0] = 0;

  Array_t<Diff_Info>& DI_List = (pV == pvS) ? DI_List_S : DI_List_L;
  FileBuf* pfb = pV->pfb;
  const unsigned CLd = CrsLine();               // Line position diff
  const unsigned CLv = DI_List[ CLd ].line_num; // Line position view
  const unsigned CC = CrsChar();                // Char position
  const unsigned LL = NumLines() ? pfb->LineLen( CLv )  : 0;
  const unsigned WC = WorkingCols( pV );

  // When inserting text at the end of a line, CrsChar() == LL
  if( LL && CC < LL ) // Print current char info:
  {
    const int c = pfb->Get( CLv, CC );

    if( (  0 <= c && c <=  8 )
     || ( 11 <= c && c <= 12 )
     || ( 14 <= c && c <= 31 )
     || (127 <= c && c <= 255 ) ) // Non-printable char:
    {
      sprintf( buf1, "%u", c );
    }
    else if(  9 == c ) sprintf( buf1, "%u,\\t", c );
    else if( 13 == c ) sprintf( buf1, "%u,\\r", c );
    else               sprintf( buf1, "%u,%c", c, c );
  }
  const unsigned fileSize = pfb->GetSize();
  const unsigned  crsByte = pfb->GetCursorByte( CLv, CC );
  char percent = SCast<char>(100*double(crsByte)/double(fileSize) + 0.5);
  // Screen width so far
  char* p = buf2;

  p += sprintf( buf2, "Pos=(%u,%u)  (%i%%, %u/%u)  Char=(%s)  "
                    , CLv+1, CC+1, percent, crsByte, pfb->GetSize(), buf1 );
  unsigned SW = p - buf2; // Screen width so far

  if( SW < WC )
  {
    for( unsigned k=SW; k<WC; k++ ) *p++ = ' ';
  }
  else if( WC < SW )
  {
    p = buf2 + WC; // Truncate extra part
  }
  *p = 0;

  Console::SetS( pV->Sts__Line_Row(), pV->Col_Win_2_GL( 0 ), buf2, S_STATUS );
}

void Diff::PrintCmdLine( View* pV )
{
  // Prints "--INSERT--" banner, and/or clears command line
  Trace trace( __PRETTY_FUNCTION__ );

  unsigned i=0;
  // Draw insert banner if needed
  if( pV->inInsertMode )
  {
    i=10; // Strlen of "--INSERT--"
    Console::SetS( pV->Cmd__Line_Row(), pV->Col_Win_2_GL( 0 ), "--INSERT--", S_BANNER );
  }
  const unsigned WC = WorkingCols( pV );

  for( ; i<WC-7; i++ )
  {
    Console::Set( pV->Cmd__Line_Row(), pV->Col_Win_2_GL( i ), ' ', S_NORMAL );
  }
  Console::SetS( pV->Cmd__Line_Row(), pV->Col_Win_2_GL( WC-8 ), "--DIFF--", S_BANNER );
}

Style Diff::Get_Style( View* pV
                     , const unsigned DL
                     , const unsigned VL
                     , const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Style s = S_NORMAL;

  if     (  InVisualArea( pV, DL, pos ) ) s = S_RV_VISUAL;
  else if( pV->InStar       ( VL, pos ) ) s = S_STAR;
  else if( pV->InDefine     ( VL, pos ) ) s = S_DEFINE;
  else if( pV->InComment    ( VL, pos ) ) s = S_COMMENT;
  else if( pV->InConst      ( VL, pos ) ) s = S_CONST;
  else if( pV->InControl    ( VL, pos ) ) s = S_CONTROL;
  else if( pV->InVarType    ( VL, pos ) ) s = S_VARTYPE;

  return s;
}

Style Diff::DiffStyle( const Style s )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // If s is already a DIFF style, just return it
  Style diff_s = s;

  if     ( s == S_NORMAL   ) diff_s = S_DIFF_NORMAL   ;
  else if( s == S_STAR     ) diff_s = S_DIFF_STAR     ;
  else if( s == S_COMMENT  ) diff_s = S_DIFF_COMMENT  ;
  else if( s == S_DEFINE   ) diff_s = S_DIFF_DEFINE   ;
  else if( s == S_CONST    ) diff_s = S_DIFF_CONST    ;
  else if( s == S_CONTROL  ) diff_s = S_DIFF_CONTROL  ;
  else if( s == S_VARTYPE  ) diff_s = S_DIFF_VARTYPE  ;
  else if( s == S_VISUAL   ) diff_s = S_DIFF_VISUAL   ;

  return diff_s;
}

bool Diff::InVisualArea( View* pV, const unsigned DL, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Only one diff view, current view, can be in visual mode.
  if( gl_pVis->CV() == pV && inVisualMode )
  {
    if( inVisualBlock ) return InVisualBlock( DL, pos );
    else                return InVisualStFn ( DL, pos );
  }
  return false;
}

bool Diff::InVisualBlock( const unsigned DL, const unsigned pos )
{
  return ( v_st_line <= DL && DL <= v_fn_line && v_st_char <= pos && pos <= v_fn_char ) // bot rite
      || ( v_st_line <= DL && DL <= v_fn_line && v_fn_char <= pos && pos <= v_st_char ) // bot left
      || ( v_fn_line <= DL && DL <= v_st_line && v_st_char <= pos && pos <= v_fn_char ) // top rite
      || ( v_fn_line <= DL && DL <= v_st_line && v_fn_char <= pos && pos <= v_st_char );// top left
}

bool Diff::InVisualStFn( const unsigned DL, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( !inVisualMode ) return false;

  if( v_st_line == DL && DL == v_fn_line )
  {
    return (v_st_char <= pos && pos <= v_fn_char)
        || (v_fn_char <= pos && pos <= v_st_char);
  }
  else if( (v_st_line < DL && DL < v_fn_line)
        || (v_fn_line < DL && DL < v_st_line) )
  {
    return true;
  }
  else if( v_st_line == DL && DL < v_fn_line )
  {
    return v_st_char <= pos;
  }
  else if( v_fn_line == DL && DL < v_st_line )
  {
    return v_fn_char <= pos;
  }
  else if( v_st_line < DL && DL == v_fn_line )
  {
    return pos <= v_fn_char;
  }
  else if( v_fn_line < DL && DL == v_st_line )
  {
    return pos <= v_st_char;
  }
  return false;
}

void Diff::DisplayBanner()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  // Command line row in window:
  const unsigned WIN_ROW = WorkingRows( pV ) + 2;
  const unsigned WIN_COL = 0;

  const unsigned G_ROW = Row_Win_2_GL( pV, WIN_ROW );
  const unsigned G_COL = Col_Win_2_GL( pV, WIN_COL );

  if( pV->inInsertMode )
  {
    Console::SetS( G_ROW, G_COL, "--INSERT --", S_BANNER );
  }
  else if( pV->inReplaceMode )
  {
    Console::SetS( G_ROW, G_COL, "--REPLACE--", S_BANNER );
  }
  else if( inVisualMode )
  {
    Console::SetS( G_ROW, G_COL, "--VISUAL --", S_BANNER );
  }
  Console::Update();
  PrintCursor( pV ); // Put cursor back in position.
}

void Diff::Remove_Banner()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  const unsigned WC = WorkingCols( pV );
  const unsigned N = Min( WC, 11 );

  // Command line row in window:
  const unsigned WIN_ROW = WorkingRows( pV ) + 2;

  // Clear command line:
  for( unsigned k=0; k<N; k++ )
  {
    Console::Set( Row_Win_2_GL( pV, WIN_ROW )
                , Col_Win_2_GL( pV, k )
                , ' '
                , S_NORMAL );
  }
  Console::Update();
  PrintCursor( pV ); // Put cursor back in position.
}

void Diff::Popu_SameList( const CompArea CA )
{
  Trace trace( __PRETTY_FUNCTION__ );
  sameList.clear();
  Array_t<CompArea> compList(__FILE__, __LINE__);  compList.push(__FILE__,__LINE__, CA );
  unsigned count = 0;
  CompArea ca;

  while( compList.pop( ca ) )
  {
    SameArea same = Find_Max_Same( ca, count++ );

    if( same.nlines && same.nbytes ) //< Dont count a single empty line as a same area
    {
      sameList.push(__FILE__,__LINE__, same );

      const unsigned SAME_FNL_S = same.ln_s+same.nlines; // Same finish line short
      const unsigned SAME_FNL_L = same.ln_l+same.nlines; // Same finish line long

      if( ( same.ln_s == ca.stl_s || same.ln_l == ca.stl_l )
       && SAME_FNL_S < ca.fnl_s
       && SAME_FNL_L < ca.fnl_l )
      {
        // Only one new CompArea after same:
        CompArea ca1 = { SAME_FNL_S, ca.fnl_s
                       , SAME_FNL_L, ca.fnl_l };
        compList.push(__FILE__,__LINE__, ca1 );
      }
      else if( ( SAME_FNL_S == ca.fnl_s || SAME_FNL_L == ca.fnl_l )
            && ca.stl_s < same.ln_s
            && ca.stl_l < same.ln_l )
      {
        // Only one new CompArea before same:
        CompArea ca1 = { ca.stl_s, same.ln_s
                       , ca.stl_l, same.ln_l };
        compList.push(__FILE__,__LINE__, ca1 );
      }
      else if( ca.stl_s < same.ln_s && SAME_FNL_S < ca.fnl_s
            && ca.stl_l < same.ln_l && SAME_FNL_L < ca.fnl_l )
      {
        // Two new CompArea's, one before same, and one after same:
        CompArea ca1 = { ca.stl_s, same.ln_s
                       , ca.stl_l, same.ln_l };
        CompArea ca2 = { SAME_FNL_S, ca.fnl_s
                       , SAME_FNL_L, ca.fnl_l };
        compList.push(__FILE__,__LINE__, ca1 );
        compList.push(__FILE__,__LINE__, ca2 );
      }
    }
  }
}

SameArea Diff::Find_Max_Same( const CompArea ca, const unsigned count )
{
  Trace trace( __PRETTY_FUNCTION__ );
  SameArea max_same = { 0, 0, 0, 0 };

  for( unsigned _ln_s = ca.stl_s; _ln_s<ca.fnl_s-max_same.nlines; _ln_s++ )
  {
    unsigned ln_s = _ln_s;
    SameArea cur_same = { 0, 0, 0, 0 };
    for( unsigned ln_l = ca.stl_l; ln_s<ca.fnl_s && ln_l<ca.fnl_l; ln_l++ )
    {
      Line* ls = pfS->GetLineP( ln_s );
      Line* ll = pfL->GetLineP( ln_l );

      if( ls->chksum() != ll->chksum() ) { cur_same.Clear(); ln_s = _ln_s; }
      else {
        if( 0 == max_same.nlines   // First line match
         || 0 == cur_same.nlines ) // First line match this outer loop
        {
          cur_same.Init( ln_s, ln_l, ls->len()+1 ); // Add one to account for line delimiter
        }
        else { // Continuation of cur_same
          cur_same.Inc( Min( ls->len()+1, ll->len()+1 ) ); // Add one to account for line delimiter
        }
        if( max_same.nbytes < cur_same.nbytes ) max_same.Set( cur_same );
        ln_s++;
      }
    }
    // This line makes the diff run faster:
    if( max_same.nlines ) _ln_s = Max( _ln_s, max_same.ln_s+max_same.nlines-1 );
  }
  return max_same;
}

void Diff::Sort_SameList()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned SLL = sameList.len();

  for( unsigned k=0; k<SLL; k++ )
  {
    for( unsigned j=SLL-1; k<j; j-- )
    {
      SameArea sa0 = sameList[ j-1 ];
      SameArea sa1 = sameList[ j   ];

      if( sa1.ln_l < sa0.ln_l )
      {
        sameList[ j-1 ] = sa1;
        sameList[ j   ] = sa0;
      }
    }
  }
}

void Diff::PrintSameList()
{
  for( unsigned k=0; k<sameList.len(); k++ )
  {
    SameArea same = sameList[k];
    Log.Log( "Same: (%s):(%u-%u), (%s):(%u-%u), nlines=%u, nbytes=%u\n"
           , pfS->file_name.c_str(), same.ln_s+1, same.ln_s+same.nlines
           , pfL->file_name.c_str(), same.ln_l+1, same.ln_l+same.nlines
           , same.nlines
           , same.nbytes );
  }
}

void Diff::Popu_DiffList( const CompArea CA )
{
  Trace trace( __PRETTY_FUNCTION__ );
  diffList.clear();

  Popu_DiffList_Begin( CA );

  const unsigned SLL = sameList.len();

  for( unsigned k=1; k<SLL; k++ )
  {
    SameArea sa0 = sameList[ k-1 ];
    SameArea sa1 = sameList[ k   ];

    unsigned da_ln_s = sa0.ln_s+sa0.nlines;
    unsigned da_ln_l = sa0.ln_l+sa0.nlines;

    DiffArea da = { da_ln_s               // da.ln_s
                  , da_ln_l               // da.ln_l
                  , sa1.ln_s - da_ln_s    // da.nline_s
                  , sa1.ln_l - da_ln_l }; // da.nline_l
    diffList.push(__FILE__,__LINE__, da );
  }
  Popu_DiffList_End( CA );
}

void Diff::Popu_DiffList_Begin( const CompArea CA )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( sameList.len() ) // Add DiffArea before first SameArea if needed:
  {
    SameArea sa = sameList[ 0 ];

    const unsigned nlines_s_da = sa.ln_s - CA.stl_s; // Num lines in short diff area
    const unsigned nlines_l_da = sa.ln_l - CA.stl_l; // Num lines in long  diff area

    if( nlines_s_da || nlines_l_da )
    {
      // DiffArea at beginning of CompArea:
      DiffArea da = { CA.stl_s, CA.stl_l, nlines_s_da, nlines_l_da };
      diffList.push(__FILE__,__LINE__, da );
    }
  }
}

void Diff::Popu_DiffList_End( const CompArea CA )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned SLL = sameList.len();
  const unsigned nLines_S_CA = CA.fnl_s - CA.stl_s;
  const unsigned nLines_L_CA = CA.fnl_l - CA.stl_l;

  if( SLL ) // Add DiffArea after last SameArea if needed:
  {
    SameArea sa = sameList[ SLL-1 ];
    const unsigned sa_s_end = sa.ln_s + sa.nlines;
    const unsigned sa_l_end = sa.ln_l + sa.nlines;

    if( sa_s_end < CA.fnl_s
     || sa_l_end < CA.fnl_l ) // DiffArea at end of file:
    {
      // Number of lines of short and long equal to
      // start of SameArea short and long
      DiffArea da = { sa_s_end
                    , sa_l_end
                    , CA.fnl_s - sa_s_end
                    , CA.fnl_l - sa_l_end };
      diffList.push(__FILE__,__LINE__, da );
    }
  }
  else // No SameArea, so whole CompArea is a DiffArea:
  {
    DiffArea da = { CA.stl_s, CA.stl_l, nLines_S_CA, nLines_L_CA };
    diffList.push(__FILE__,__LINE__, da );
  }
}

void Diff::PrintDiffList()
{
  for( unsigned k=0; k<diffList.len(); k++ )
  {
    DiffArea da = diffList[k];
    Log.Log( "Diff: (%s):(%u-%u), (%s):(%u-%u)\n"
           , pfS->file_name.c_str(), da.ln_s+1, da.ln_s+da.nlines_s
           , pfL->file_name.c_str(), da.ln_l+1, da.ln_l+da.nlines_l );
  }
}

void Diff::Popu_DI_List( const CompArea CA )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Clear_DI_List_CA( CA.stl_s, CA.fnl_s, DI_List_S );
  Clear_DI_List_CA( CA.stl_l, CA.fnl_l, DI_List_L );

  const unsigned SLL = sameList.len();
  const unsigned DLL = diffList.len();

  if     ( SLL == 0 ) Popu_DI_List_NoSameArea();
  else if( DLL == 0 ) Popu_DI_List_NoDiffArea();
  else                Popu_DI_List_DiffAndSame( CA );
}

void Diff::PrintDI_List( const CompArea CA )
{
  const unsigned DILL = DI_List_S.len();

  for( unsigned k=CA.stl_s; k<DILL; k++ )
  {
    Diff_Info dis = DI_List_S[k];
    Diff_Info dil = DI_List_L[k];

    Log.Log("DIS (%u:%s), DIL (%u,%s)\n"
           , dis.line_num+1, Diff_Type_2_Str( dis.diff_type )
           , dil.line_num+1, Diff_Type_2_Str( dil.diff_type ) );

    if( CA.fnl_s <= dis.line_num ) break;
  }
}

void Diff::Clear_DI_List_CA( const unsigned st_line
                           , const unsigned fn_line
                           , Array_t<Diff_Info>& DI_List )
{
  bool     found_first = false;
  unsigned first_to_remove = 0;
  unsigned num___to_remove = 0;

  // Since, Clear_DI_List_CA will only be call when  DI_List is
  // fully populated, the Diff_Info.line_num's will be at indexes
  // greater than or equal to st_line
  for( unsigned k=st_line; k<DI_List.len(); k++ )
  {
    Diff_Info& di = DI_List[k];

    if( st_line <= di.line_num && di.line_num < fn_line )
    {
      if( di.pLineInfo )
      {
        // Return all the previously allocated LineInfo's:
        Return_LineInfo( di.pLineInfo ); di.pLineInfo = 0;
      }
      if( !found_first )
      {
        found_first = true;
        first_to_remove = k;
      }
      num___to_remove++;
    }
    else if( fn_line <= di.line_num )
    {
      // Past the range of line_num's we want to remove
      break;
    }
  }
  DI_List.remove_n( first_to_remove, num___to_remove );
}

void Diff::Popu_DI_List_NoSameArea()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Should only be one DiffArea, which is the whole CompArea:
  const unsigned DLL = diffList.len();
  ASSERT( __LINE__, DLL==1, "DLL==1" );

  Popu_DI_List_AddDiff( diffList[0] );
}

void Diff::Popu_DI_List_NoDiffArea()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Should only be one SameArea, which is the whole CompArea:
  const unsigned SLL = sameList.len();
  ASSERT( __LINE__, SLL==1, "SLL==1" );

  Popu_DI_List_AddSame( sameList[0] );
}

void Diff::Popu_DI_List_DiffAndSame( const CompArea CA )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned SLL = sameList.len();
  const unsigned DLL = diffList.len();

  DiffArea da = diffList[ 0 ];

  if( CA.stl_s==da.ln_s && CA.stl_l==da.ln_l )
  {
    // Start with DiffArea, and then alternate between SameArea and DiffArea.
    // There should be at least as many DiffArea's as SameArea's.
    ASSERT( __LINE__, SLL<=DLL, "SLL<=DLL" );
 
    for( unsigned k=0; k<SLL; k++ )
    {
      DiffArea da = diffList[ k ]; Popu_DI_List_AddDiff( da );
      SameArea sa = sameList[ k ]; Popu_DI_List_AddSame( sa );
    }
    if( SLL < DLL )
    {
      ASSERT( __LINE__, SLL+1==DLL, "SLL+1==DLL" );
      DiffArea da = diffList[ DLL-1 ]; Popu_DI_List_AddDiff( da );
    }
  }
  else {
    // Start with SameArea, and then alternate between DiffArea and SameArea.
    // There should be at least as many SameArea's as DiffArea's.
    ASSERT( __LINE__, DLL<=SLL, "DLL<=SLL" );
 
    for( unsigned k=0; k<DLL; k++ )
    {
      SameArea sa = sameList[ k ]; Popu_DI_List_AddSame( sa );
      DiffArea da = diffList[ k ]; Popu_DI_List_AddDiff( da );
    }
    if( DLL < SLL )
    {
      ASSERT( __LINE__, DLL+1==SLL, "DLL+1==SLL" );
      SameArea sa = sameList[ SLL-1 ]; Popu_DI_List_AddSame( sa );
    }
  }
}

void Diff::Popu_DI_List_AddSame( const SameArea sa )
{
  Trace trace( __PRETTY_FUNCTION__ );

  for( unsigned k=0; k<sa.nlines; k++ )
  {
    Diff_Info dis = { DT_SAME, sa.ln_s+k };
    Diff_Info dil = { DT_SAME, sa.ln_l+k };

    Insert_DI_List( dis, DI_List_S );
    Insert_DI_List( dil, DI_List_L );
  }
}

void Diff::Popu_DI_List_AddDiff( const DiffArea da )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( da.nlines_s < da.nlines_l )
  {
    Popu_DI_List_AddDiff_Common( da.ln_s
                               , da.ln_l
                               , da.nlines_s
                               , da.nlines_l
                               , DI_List_S
                               , DI_List_L
                               , pfS, pfL );
  }
  else if( da.nlines_l < da.nlines_s )
  {
    Popu_DI_List_AddDiff_Common( da.ln_l
                               , da.ln_s
                               , da.nlines_l
                               , da.nlines_s
                               , DI_List_L
                               , DI_List_S
                               , pfL, pfS );
  }
  else // da.nlines_s == da.nlines_l
  {
    for( unsigned k=0; k<da.nlines_l; k++ )
    {
      Line* ls = pfS->GetLineP( da.ln_s+k );
      Line* ll = pfL->GetLineP( da.ln_l+k );

      LineInfo* li_s = Borrow_LineInfo(__FILE__,__LINE__);
      LineInfo* li_l = Borrow_LineInfo(__FILE__,__LINE__);

      unsigned bytes_same = Compare_Lines( ls, li_s, ll, li_l );

      Diff_Info dis = { DT_CHANGED, da.ln_s+k, li_s }; Insert_DI_List( dis, DI_List_S );
      Diff_Info dil = { DT_CHANGED, da.ln_l+k, li_l }; Insert_DI_List( dil, DI_List_L );
    }
  }
}

void Diff::Popu_DI_List_AddDiff_Common( const unsigned da_ln_s
                                      , const unsigned da_ln_l
                                      , const unsigned da_nlines_s
                                      , const unsigned da_nlines_l
                                      , Array_t<Diff_Info>& DI_List_s
                                      , Array_t<Diff_Info>& DI_List_l
                                      , FileBuf* pfs
                                      , FileBuf* pfl )
{
  Popu_SimiList( da_ln_s
               , da_ln_l
               , da_nlines_s
               , da_nlines_l
               , pfs
               , pfl );
  Sort_SimiList();
//PrintSimiList();

  SimiList_2_DI_Lists( da_ln_s
                     , da_ln_l
                     , da_nlines_s
                     , da_nlines_l
                     , DI_List_s
                     , DI_List_l );
}

void Diff::Popu_SimiList( const unsigned da_ln_s
                        , const unsigned da_ln_l
                        , const unsigned da_nlines_s
                        , const unsigned da_nlines_l
                        , FileBuf* pfs
                        , FileBuf* pfl )
{
  Trace trace( __PRETTY_FUNCTION__ );
  Clear_SimiList();

  if( da_nlines_s && da_nlines_l )
  {
    CompArea ca = { da_ln_s, da_ln_s+da_nlines_s
                  , da_ln_l, da_ln_l+da_nlines_l };

    Array_t<CompArea> compList(__FILE__,__LINE__);
                      compList.push(__FILE__,__LINE__, ca );

    while( compList.pop( ca ) )
    {
      SimLines siml = Find_Lines_Most_Same( ca, pfs, pfl );

      if( simiList.len() == da_nlines_s )
      {
        // Not putting siml into simiList, so delete any new'ed memory:
        MemMark(__FILE__,__LINE__); delete siml.li_s; siml.li_s = 0;
        MemMark(__FILE__,__LINE__); delete siml.li_l; siml.li_l = 0;
        return;
      }
      simiList.push(__FILE__,__LINE__, siml );
      if( ( siml.ln_s == ca.stl_s || siml.ln_l == ca.stl_l )
       && siml.ln_s+1 < ca.fnl_s
       && siml.ln_l+1 < ca.fnl_l )
      {
        // Only one new CompArea after siml:
        CompArea ca1 = { siml.ln_s+1, ca.fnl_s
                       , siml.ln_l+1, ca.fnl_l };
        compList.push(__FILE__,__LINE__, ca1 );
      }
      else if( ( siml.ln_s+1 == ca.fnl_s || siml.ln_l+1 == ca.fnl_l )
            && ca.stl_s < siml.ln_s
            && ca.stl_l < siml.ln_l )
      {
        // Only one new CompArea before siml:
        CompArea ca1 = { ca.stl_s, siml.ln_s
                       , ca.stl_l, siml.ln_l };
        compList.push(__FILE__,__LINE__, ca1 );
      }
      else if( ca.stl_s < siml.ln_s && siml.ln_s+1 < ca.fnl_s
            && ca.stl_l < siml.ln_l && siml.ln_l+1 < ca.fnl_l )
      {
        // Two new CompArea's, one before siml, and one after siml:
        CompArea ca1 = { ca.stl_s, siml.ln_s
                       , ca.stl_l, siml.ln_l };
        CompArea ca2 = { siml.ln_s+1, ca.fnl_s
                       , siml.ln_l+1, ca.fnl_l };
        compList.push(__FILE__,__LINE__, ca1 );
        compList.push(__FILE__,__LINE__, ca2 );
      }
    }
  }
}

void Diff::Sort_SimiList()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned SLL = simiList.len();

  for( unsigned k=0; k<SLL; k++ )
  {
    for( unsigned j=SLL-1; k<j; j-- )
    {
      SimLines sl0 = simiList[ j-1 ];
      SimLines sl1 = simiList[ j   ];

      if( sl1.ln_l < sl0.ln_l )
      {
        simiList[ j-1 ] = sl1;
        simiList[ j   ] = sl0;
      }
    }
  }
}

void Diff::PrintSimiList()
{
  const unsigned SLL = simiList.len();

  for( unsigned k=0; k<SLL; k++ )
  {
    SimLines sl = simiList[k];

    Log.Log("SimLines: ln_s=%u, ln_l=%u, nbytes=%u\n"
           , sl.ln_s+1, sl.ln_l+1, sl.nbytes );
  }
}

void Diff::SimiList_2_DI_Lists( const unsigned da_ln_s
                              , const unsigned da_ln_l
                              , const unsigned da_nlines_s
                              , const unsigned da_nlines_l
                              , Array_t<Diff_Info>& DI_List_s
                              , Array_t<Diff_Info>& DI_List_l )
{
  // Diff info short line number:
  unsigned dis_ln = da_ln_s ? da_ln_s-1 : 0;

  for( unsigned k=0; k<da_nlines_l; k++ )
  {
    Diff_Info dis = { DT_DELETED , dis_ln    };
    Diff_Info dil = { DT_INSERTED, da_ln_l+k };

    for( unsigned j=0; j<simiList.len(); j++ )
    {
      SimLines& siml = simiList[ j ];

      if( siml.ln_l == da_ln_l+k )
      {
        dis.diff_type = DT_CHANGED;
        dis.line_num  = siml.ln_s;
        dis.pLineInfo = siml.li_s; siml.li_s = 0; // Transfer ownership of LineInfo from siml to dis

        dil.diff_type = DT_CHANGED;
        dil.pLineInfo = siml.li_l; siml.li_l = 0; // Transfer ownership of LineInfo from siml to dil

        dis_ln = dis.line_num;
        break;
      }
    }
    // DI_List_s and DI_List_l now own LineInfo objects:
    Insert_DI_List( dis, DI_List_s );
    Insert_DI_List( dil, DI_List_l );
  }
}

SimLines Diff::Find_Lines_Most_Same( CompArea ca, FileBuf* pfs, FileBuf* pfl )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // LD = Length Difference between long area and short area
  const unsigned LD = (ca.fnl_l-ca.stl_l)-(ca.fnl_s-ca.stl_s);

  SimLines most_same = { 0, 0, 0, 0, 0 };
  for( unsigned ln_s = ca.stl_s; ln_s<ca.fnl_s; ln_s++ )
  {
    const unsigned ST_L = ca.stl_l+(ln_s-ca.stl_s);

    for( unsigned ln_l = ST_L; ln_l<ca.fnl_l && ln_l<ST_L+LD+1; ln_l++ )
    {
      Line* ls = pfs->GetLineP( ln_s ); // Line from short area
      Line* ll = pfl->GetLineP( ln_l ); // Line from long  area

      LineInfo* li_s = Borrow_LineInfo(__FILE__,__LINE__);
      LineInfo* li_l = Borrow_LineInfo(__FILE__,__LINE__);
      unsigned bytes_same = Compare_Lines( ls, li_s, ll, li_l );

      if( most_same.nbytes < bytes_same )
      {
        if( most_same.li_s ) { Return_LineInfo( most_same.li_s ); }
        if( most_same.li_l ) { Return_LineInfo( most_same.li_l ); }
        most_same.ln_s   = ln_s;
        most_same.ln_l   = ln_l;
        most_same.nbytes = bytes_same;
        most_same.li_s   = li_s; // Hand off li_s
        most_same.li_l   = li_l; // and      li_l
      }
      else {
        Return_LineInfo( li_s );
        Return_LineInfo( li_l );
      }
    }
  }
  if( 0==most_same.nbytes )
  {
    // This if() block ensures that each line in the short CompArea is
    // matched to a line in the long CompArea.  Each line in the short
    // CompArea must be matched to a line in the long CompArea or else
    // SimiList_2_DI_Lists wont work right.
    most_same.ln_s   = ca.stl_s;
    most_same.ln_l   = ca.stl_l;
    most_same.nbytes = 1;
  }
  return most_same;
}

unsigned Diff::Compare_Lines( Line* ls, LineInfo* li_s
                            , Line* ll, LineInfo* li_l )
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==ls->len() && 0==ll->len() ) { return 1; }
  li_s->clear(); li_l->clear();
  SameLineSec max_same = { 0, 0, 0 };
  Line* pls = ls; LineInfo* pli_s = li_s;
  Line* pll = ll; LineInfo* pli_l = li_l;
  if( ll->len() < ls->len() ) { pls = ll; pli_s = li_l;
                                pll = ls; pli_l = li_s; }
  const unsigned SLL = pls->len();
  const unsigned LLL = pll->len();

  for( unsigned _ch_s = 0; _ch_s<SLL; _ch_s++ )
  {
    unsigned ch_s = _ch_s;
    SameLineSec cur_same = { 0, 0, 0 };

    for( unsigned ch_l = 0; ch_s<SLL && ch_l<LLL; ch_l++ )
    {
      const uint8_t cs = pls->get( ch_s );
      const uint8_t cl = pll->get( ch_l );

      if( cs != cl ) { cur_same.nbytes = 0; ch_s = _ch_s; }
      else {
        if( 0 == max_same.nbytes ) // First char match
        {
          max_same.Init( ch_s, ch_l );
          cur_same.Init( ch_s, ch_l );
        }
        else if( 0 == cur_same.nbytes ) // First char match this outer loop
        {
          cur_same.Init( ch_s, ch_l );
        }
        else { // Continuation of cur_same
          cur_same.nbytes++;
          if( max_same.nbytes < cur_same.nbytes ) max_same.Set( cur_same );
        }
        ch_s++;
      }
    }
  }
  Fill_In_LineInfo( SLL, LLL, pli_s, pli_l, max_same, pls, pll );

  return max_same.nbytes;
}

void Diff::Fill_In_LineInfo( const unsigned SLL
                           , const unsigned LLL
                           , LineInfo* const pli_s
                           , LineInfo* const pli_l
                           , SameLineSec& max_same
                           , Line* pls
                           , Line* pll )
{
  pli_l->set_len(__FILE__,__LINE__, LLL );
  pli_s->set_len(__FILE__,__LINE__, LLL );

  for( unsigned k=0; k<SLL; k++ )
  {
    (*pli_s)[k] = DT_CHANGED;
    (*pli_l)[k] = DT_CHANGED;
  }
  for( unsigned k=SLL; k<LLL; k++ )
  {
    (*pli_s)[k] = DT_DELETED;
    (*pli_l)[k] = DT_INSERTED;
  }
  for( unsigned k=0; k<max_same.nbytes; k++ )
  {
    (*pli_s)[k+max_same.ch_s] = DT_SAME;
    (*pli_l)[k+max_same.ch_l] = DT_SAME;
  }
  const unsigned SAME_ST = Min( max_same.ch_s, max_same.ch_l );
  const unsigned SAME_FN = Max( max_same.ch_s+max_same.nbytes
                              , max_same.ch_l+max_same.nbytes );

  for( unsigned k=0; k<SAME_ST; k++ )
  {
    if( pls->get(k) == pll->get(k) )
    {
      pli_s->set( k, DT_SAME );
      pli_l->set( k, DT_SAME );
    }
  }
  for( unsigned k=SAME_FN; k<SLL; k++ )
  {
    if( pls->get(k) == pll->get(k) )
    {
      pli_s->set( k, DT_SAME );
      pli_l->set( k, DT_SAME );
    }
  }
}

void Diff::GoToCrsPos_Write( const unsigned ncp_crsLine
                           , const unsigned ncp_crsChar )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  const unsigned OCL = CrsLine();
  const unsigned OCP = CrsChar();
  const unsigned NCL = ncp_crsLine;
  const unsigned NCP = ncp_crsChar;

  if( OCL == NCL && OCP == NCP )
  {
    // Not moving to new cursor line so just put cursor back where is was
    PrintCursor( pV );
  }
  else {
    if( inVisualMode )
    {
      v_fn_line = NCL;
      v_fn_char = NCP;
    }
    // These moves refer to View of buffer:
    const bool MOVE_DOWN  = BotLine( pV )   < NCL;
    const bool MOVE_RIGHT = RightChar( pV ) < NCP;
    const bool MOVE_UP    = NCL < topLine;
    const bool MOVE_LEFT  = NCP < leftChar;
 
    bool redraw = MOVE_DOWN || MOVE_RIGHT || MOVE_UP || MOVE_LEFT;
 
    if( redraw )
    {
      if     ( MOVE_DOWN ) topLine = NCL - WorkingRows( pV ) + 1;
      else if( MOVE_UP   ) topLine = NCL;
 
      if     ( MOVE_RIGHT ) leftChar = NCP - WorkingCols( pV ) + 1;
      else if( MOVE_LEFT  ) leftChar = NCP;
 
      // crsRow and crsCol must be set to new values before calling CalcNewCrsByte
      crsRow = NCL - topLine;
      crsCol = NCP - leftChar;
 
      Update();
    }
    else if( inVisualMode )
    {
      if( inVisualBlock ) GoToCrsPos_Write_VisualBlock( OCL, OCP, NCL, NCP );
      else                GoToCrsPos_Write_Visual     ( OCL, OCP, NCL, NCP );
    }
    else {
      // crsRow and crsCol must be set to new values before calling CalcNewCrsByte and PrintCursor
      crsRow = NCL - topLine;
      crsCol = NCP - leftChar;
 
      PrintCursor( pV );  // Put cursor into position.
 
      sts_line_needs_update = true;
    }
  }
}

void Diff::GoToCrsPos_Write_Visual( const unsigned OCL, const unsigned OCP
                                  , const unsigned NCL, const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // (old cursor pos) < (new cursor pos)
  const bool OCP_LT_NCP = OCL < NCL || (OCL == NCL && OCP < NCP);

  if( OCP_LT_NCP ) // Cursor moved forward
  {
    GoToCrsPos_WV_Forward( OCL, OCP, NCL, NCP );
  }
  else // NCP_LT_OCP // Cursor moved backward
  {
    GoToCrsPos_WV_Backward( OCL, OCP, NCL, NCP );
  }
  crsRow = NCL - topLine;
  crsCol = NCP - leftChar;
  Console::Update();
  PrintCursor( gl_pVis->CV() );
  sts_line_needs_update = true;
}

// Cursor is moving forward
// Write out from (OCL,OCP) up to but not including (NCL,NCP)
void Diff::GoToCrsPos_WV_Forward( const unsigned OCL, const unsigned OCP
                                , const unsigned NCL, const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );
  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;
  // Convert OCL and NCL, which are diff lines, to view lines:
  const unsigned OCLv = ViewLine( pV, OCL );
  const unsigned NCLv = ViewLine( pV, NCL );

  if( OCL == NCL ) // Only one line:
  {
    for( unsigned k=OCP; k<NCP; k++ )
    {
      const char  C = pfb->Get( OCLv, k );
      const Style S = Get_Style(pV,OCL,OCLv,k);
      Console::Set( Line_2_GL( pV, OCL ), Char_2_GL( pV, k ), C, S );
    }
  }
  else { // Multiple lines
    // Write out first line:
    const unsigned FIRST_LINE_DIFF_TYPE = DiffType( pV, OCL );
    if( FIRST_LINE_DIFF_TYPE != DT_DELETED )
    {
      const unsigned OCLL = pfb->LineLen( OCLv ); // Old cursor line length
      const unsigned END_FIRST_LINE = Min( RightChar( pV )+1, OCLL );
      for( unsigned k=OCP; k<END_FIRST_LINE; k++ )
      {
        const char  C = pfb->Get( OCLv, k );
        const Style S = Get_Style(pV,OCL,OCLv,k);
        Console::Set( Line_2_GL( pV, OCL ), Char_2_GL( pV, k ), C, S );
      }
    }
    // Write out intermediate lines:
    for( unsigned l=OCL+1; l<NCL; l++ )
    {
      const unsigned LINE_DIFF_TYPE = DiffType( pV, l );
      if( LINE_DIFF_TYPE != DT_DELETED )
      {
        // Convert OCL, which is diff line, to view line
        const unsigned Vl = ViewLine( pV, l );
        const unsigned LL = pfb->LineLen( Vl ); // Line length
        const unsigned END_OF_LINE = Min( RightChar( pV )+1, LL );
        for( unsigned k=leftChar; k<END_OF_LINE; k++ )
        {
          const char  C = pfb->Get( Vl, k );
          const Style S = Get_Style(pV,l,Vl,k);
          Console::Set( Line_2_GL( pV, l ), Char_2_GL( pV, k ), C, S );
        }
      }
    }
    // Write out last line:
    const unsigned LAST_LINE_DIFF_TYPE = DiffType( pV, NCL );
    if( LAST_LINE_DIFF_TYPE != DT_DELETED )
    {
      // Print from beginning of next line to new cursor position:
      const unsigned NCLL = pfb->LineLen( NCLv ); // Line length
      const unsigned END_LAST_LINE = Min( NCLL, NCP );
      for( unsigned k=leftChar; k<END_LAST_LINE; k++ )
      {
        const char  C = pfb->Get( NCLv, k );
        const Style S = Get_Style(pV,NCL,NCLv,k);
        Console::Set( Line_2_GL( pV, NCL ), Char_2_GL( pV, k ), C, S );
      }
    }
  }
}

// Cursor is moving backwards
// Write out from (OCL,OCP) back to but not including (NCL,NCP)
void Diff::GoToCrsPos_WV_Backward( const unsigned OCL, const unsigned OCP
                                 , const unsigned NCL, const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );
  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;
  // Convert OCL and NCL, which are diff lines, to view lines:
  const unsigned OCLv = ViewLine( pV, OCL );
  const unsigned NCLv = ViewLine( pV, NCL );

  if( OCL == NCL ) // Only one line:
  {
    for( unsigned k=OCP; NCP<k; k-- )
    {
      const char  C = pfb->Get( OCLv, k );
      const Style S = Get_Style(pV,OCL,OCLv,k);
      Console::Set( Line_2_GL( pV, OCL ), Char_2_GL( pV, k ), C, S );
    }
  }
  else { // Multiple lines
    // Write out first line:
    const int FIRST_LINE_DIFF_TYPE = DiffType( pV, OCL );
    if( FIRST_LINE_DIFF_TYPE != DT_DELETED )
    {
      const unsigned OCLL = pfb->LineLen( OCLv ); // Old cursor line length
      const unsigned RIGHT_MOST_POS = Min( OCP, 0<OCLL ? OCLL-1 : 0 );
      for( unsigned k=RIGHT_MOST_POS; leftChar<k; k-- )
      {
        const char  C = pfb->Get( OCLv, k );
        const Style S = Get_Style(pV,OCL,OCLv,k);
        Console::Set( Line_2_GL( pV, OCL ), Char_2_GL( pV, k ), C, S );
      }
      if( leftChar < OCLL ) {
        const char  C = pfb->Get( OCLv, leftChar );
        const Style S = Get_Style(pV,OCL,OCLv,leftChar);
        Console::Set( Line_2_GL( pV, OCL ), Char_2_GL( pV, leftChar ), C, S );
      }
    }
    // Write out intermediate lines:
    for( unsigned l=OCL-1; NCL<l; l-- )
    {
      const int LINE_DIFF_TYPE = DiffType( pV, l );
      if( LINE_DIFF_TYPE != DT_DELETED )
      {
        // Convert l, which is diff line, to view line:
        const unsigned Vl = ViewLine( pV, l );
        const unsigned LL = pfb->LineLen( Vl ); // Line length
        const unsigned END_OF_LINE = Min( RightChar( pV ), 0<LL ? LL-1 : 0 );
        for( unsigned k=END_OF_LINE; leftChar<k; k-- )
        {
          const char  C = pfb->Get( Vl, k );
          const Style S = Get_Style(pV,l,Vl,k);
          Console::Set( Line_2_GL( pV, l ), Char_2_GL( pV, k ), C, S );
        }
      }
    }
    // Write out last line:
    const int LAST_LINE_DIFF_TYPE = DiffType( pV, NCL );
    if( LAST_LINE_DIFF_TYPE != DT_DELETED )
    {
      // Print from end of last line to new cursor position:
      const unsigned NCLL = pfb->LineLen( NCLv ); // New cursor line length
      const unsigned END_LAST_LINE = Min( RightChar( pV ), 0<NCLL ? NCLL-1 : 0 );
      for( unsigned k=END_LAST_LINE; NCP<k; k-- )
      {
        const char  C = pfb->Get( NCLv, k );
        const Style S = Get_Style(pV,NCL,NCLv,k);
        Console::Set( Line_2_GL( pV, NCL ), Char_2_GL( pV, k ), C, S );
      }
      if( NCP < NCLL ) {
        const char  C = pfb->Get( NCLv, NCP );
        const Style S = Get_Style(pV,NCL,NCLv,NCP);
        Console::Set( Line_2_GL( pV, NCL ), Char_2_GL( pV, NCP ), C, S );
      }
    }
  }
}

void Diff::GoToCrsPos_Write_VisualBlock( const int OCL
                                       , const int OCP
                                       , const int NCL
                                       , const int NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );
  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;
  // v_fn_line == NCL && v_fn_char == NCP, so dont need to include
  // v_fn_line       and v_fn_char in Min and Max calls below:
  const int vis_box_left = Min( v_st_char, Min( OCP, NCP ) );
  const int vis_box_rite = Max( v_st_char, Max( OCP, NCP ) );
  const int vis_box_top  = Min( v_st_line, Min( OCL, NCL ) );
  const int vis_box_bot  = Max( v_st_line, Max( OCL, NCL ) );

  const int draw_box_left = Max( leftChar     , vis_box_left );
  const int draw_box_rite = Min( RightChar(pV), vis_box_rite );
  const int draw_box_top  = Max( topLine      , vis_box_top  );
  const int draw_box_bot  = Min( BotLine(pV)  , vis_box_bot  );

  for( int DL=draw_box_top; DL<=draw_box_bot; DL++ )
  {
    const int VL = ViewLine( pV, DL ); // View line number

    const int LL = pfb->LineLen( VL );

    for( int k=draw_box_left; k<LL && k<=draw_box_rite; k++ )
    {
      const char  C = pfb->Get( VL, k );
      const Style S = Get_Style( pV, DL, VL, k );

      Console::Set( Line_2_GL( pV, DL ), Char_2_GL( pV, k ), C, S );
    }
  }
  crsRow = NCL - topLine;
  crsCol = NCP - leftChar;
  Console::Update();
  PrintCursor( pV ); // Does Console::Update()
  sts_line_needs_update = true;
}

void Diff::GoToCrsPos_NoWrite( const unsigned ncp_crsLine
                             , const unsigned ncp_crsChar )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  // These moves refer to View of buffer:
  const bool MOVE_DOWN  = BotLine( pV )   < ncp_crsLine;
  const bool MOVE_RIGHT = RightChar( pV ) < ncp_crsChar;
  const bool MOVE_UP    = ncp_crsLine     < topLine;
  const bool MOVE_LEFT  = ncp_crsChar     < leftChar;

  if     ( MOVE_DOWN ) topLine = ncp_crsLine - WorkingRows( pV ) + 1;
  else if( MOVE_UP   ) topLine = ncp_crsLine;
  crsRow  = ncp_crsLine - topLine;

  if     ( MOVE_RIGHT ) leftChar = ncp_crsChar - WorkingCols( pV ) + 1;
  else if( MOVE_LEFT  ) leftChar = ncp_crsChar;
  crsCol   = ncp_crsChar - leftChar;
}

void Diff::PageDown()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();
  if( 0==NUM_LINES ) return;

  View* pV = gl_pVis->CV();

  // new diff top line:
  const unsigned newTopLine = topLine + WorkingRows( pV ) - 1;
  // Subtracting 1 above leaves one line in common between the 2 pages.

  if( newTopLine < NUM_LINES )
  {
    crsCol = 0;
    topLine = newTopLine;

    // Dont let cursor go past the end of the file:
    if( NUM_LINES <= CrsLine() )
    {
      // This line places the cursor at the top of the screen, which I prefer:
      crsRow = 0;
    }
    Update();
  }
}

void Diff::PageUp()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Dont scroll if we are at the top of the file:
  if( topLine )
  {
    //Leave crsRow unchanged.
    crsCol = 0;

    View* pV = gl_pVis->CV();

    // Dont scroll past the top of the file:
    if( topLine < WorkingRows( pV ) - 1 )
    {
      topLine = 0;
    }
    else {
      topLine -= WorkingRows( pV ) - 1;
    }
    Update();
  }
}

void Diff::GoDown()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();
  const unsigned OCL       = CrsLine(); // Old cursor line
  const unsigned NCL       = OCL+1;     // New cursor line

  if( 0 < NUM_LINES && NCL < NUM_LINES )
  {
    const unsigned OCP = CrsChar(); // Old cursor position
          unsigned NCP = OCP;

    GoToCrsPos_Write( NCL, NCP );
  }
}

void Diff::GoUp()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();
  const unsigned OCL       = CrsLine(); // Old cursor line
  const unsigned NCL       = OCL-1; // New cursor line

  if( 0 < NUM_LINES && 0 < OCL )
  {
    const unsigned OCP = CrsChar(); // Old cursor position
          unsigned NCP = OCP;

    GoToCrsPos_Write( NCL, NCP );
  }
}

void Diff::GoLeft()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CP = CrsChar(); // Cursor position

  if( 0<NumLines() && 0<CP )
  {
    const unsigned CL = CrsLine(); // Cursor line

    GoToCrsPos_Write( CL, CP-1 );
  }
}

void Diff::GoRight()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned LL = LineLen();
  const unsigned CP = CrsChar(); // Cursor position

  if( 0<NumLines() && 0<LL && CP<LL-1 )
  {
    const unsigned CL = CrsLine(); // Cursor line

    GoToCrsPos_Write( CL, CP+1 );
  }
}

void Diff::GoToBegOfLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<NumLines() )
  {
    const unsigned CL = CrsLine(); // Cursor line

    GoToCrsPos_Write( CL, 0 );
  }
}

void Diff::GoToEndOfLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<NumLines() )
  {
    const unsigned LL = LineLen();

    const unsigned OCL = CrsLine(); // Old cursor line

    GoToCrsPos_Write( OCL, 0<LL ? LL-1 : 0 );
  }
}

void Diff::GoToBegOfNextLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();

  if( 0<NUM_LINES )
  {
    const unsigned OCL = CrsLine(); // Old cursor line

    if( OCL < (NUM_LINES-1) )
    {
      // Before last line, so can go down
      GoToCrsPos_Write( OCL+1, 0 );
    }
  }
}

void Diff::GoToLine( const unsigned user_line_num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  // Internal line number is 1 less than user line number:
  const unsigned NCLv = user_line_num - 1; // New cursor view line number

  if( pfb->NumLines() <= NCLv )
  {
    // Cant move to NCLv so just put cursor back where is was
    PrintCursor( pV );
  }
  else {
    const unsigned NCLd = DiffLine( pV, NCLv ); 

    GoToCrsPos_Write( NCLd, 0 );
  }
}

void Diff::GoToTopLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToCrsPos_Write( topLine, 0 );
}

void Diff::GoToBotLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  const unsigned NUM_LINES = NumLines();

  unsigned bottom_line_in_view = topLine + WorkingRows( pV )-1;

  bottom_line_in_view = Min( NUM_LINES-1, bottom_line_in_view );

  GoToCrsPos_Write( bottom_line_in_view, 0  );
}

void Diff::GoToMidLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  const unsigned NUM_LINES = NumLines();

  // Default: Last line in file is not in view
  unsigned crsLine = topLine + WorkingRows( pV )/2;

  if( NUM_LINES-1 < BotLine( pV ) )
  {
    // Last line in file above bottom of view
    crsLine = topLine + (NUM_LINES-1 - topLine)/2;
  }
  GoToCrsPos_Write( crsLine, 0 );
}

void Diff::GoToTopOfFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToCrsPos_Write( 0, 0 );
}

void Diff::GoToEndOfFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();

  if( 0<NUM_LINES )
  {
    GoToCrsPos_Write( NUM_LINES-1, 0 );
  }
}

void Diff::GoToStartOfRow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<NumLines() )
  {
    const int OCL = CrsLine(); // Old cursor line

    GoToCrsPos_Write( OCL, leftChar );
  }
}
void Diff::GoToEndOfRow()
{
  if( 0<NumLines() )
  {
    View*    pV  = gl_pVis->CV();
    FileBuf* pfb = pV->pfb;

    const int DL = CrsLine();          // Diff line
    const int VL = ViewLine( pV, DL ); // View line

    const int LL = pfb->LineLen( VL );
    if( 0 < LL )
    {
      const int NCP = Min( LL-1, leftChar + WorkingCols( pV ) - 1 );

      GoToCrsPos_Write( DL, NCP );
    }
  }
}

void Diff::GoToOppositeBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  pV->MoveInBounds();

  const unsigned NUM_LINES = pV->pfb->NumLines();
  const unsigned CL        = ViewLine( pV, CrsLine() ); //< View line
  const unsigned CC        = CrsChar();
  const unsigned LL        = LineLen();

  if( 0==NUM_LINES || 0==LL ) return;

  const char C = pV->pfb->Get( CL, CC );

  if( C=='{' || C=='[' || C=='(' )
  {
    char finish_char = 0;
    if     ( C=='{' ) finish_char = '}';
    else if( C=='[' ) finish_char = ']';
    else if( C=='(' ) finish_char = ')';
    else              ASSERT( __LINE__, 0, "Un-handled case" );

    GoToOppositeBracket_Forward( C, finish_char );
  }
  else if( C=='}' || C==']' || C==')' )
  {
    char finish_char = 0;
    if     ( C=='}' ) finish_char = '{';
    else if( C==']' ) finish_char = '[';
    else if( C==')' ) finish_char = '(';
    else              ASSERT( __LINE__, 0, "Un-handled case" );

    GoToOppositeBracket_Backward( C, finish_char );
  }
}

void Diff::GoToLeftSquigglyBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  pV->MoveInBounds();

  const char  start_char = '}';
  const char finish_char = '{';
  GoToOppositeBracket_Backward( start_char, finish_char );
}

void Diff::GoToRightSquigglyBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  pV->MoveInBounds();

  const char  start_char = '{';
  const char finish_char = '}';
  GoToOppositeBracket_Forward( start_char, finish_char );
}

void Diff::GoToOppositeBracket_Forward( const char ST_C, const char FN_C )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  const unsigned NUM_LINES = pV->pfb->NumLines();

  // Convert from diff line (CrsLine()), to view line:
  const unsigned CL = ViewLine( pV, CrsLine() );
  const unsigned CC = CrsChar();

  // Search forward
  unsigned level = 0;
  bool     found = false;

  for( unsigned vl=CL; !found && vl<NUM_LINES; vl++ )
  {
    const unsigned LL = pV->pfb->LineLen( vl );

    for( unsigned p=(CL==vl)?(CC+1):0; !found && p<LL; p++ )
    {
      const char C = pV->pfb->Get( vl, p );

      if     ( C==ST_C ) level++;
      else if( C==FN_C )
      {
        if( 0 < level ) level--;
        else {
          found = true;

          // Convert from view line back to diff line:
          const unsigned dl = DiffLine(pV, vl);

          GoToCrsPos_Write( dl, p );
        }
      }
    }
  }
}

void Diff::GoToOppositeBracket_Backward( const char ST_C, const char FN_C )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  // Convert from diff line (CrsLine()), to view line:
  const int CL = ViewLine( pV, CrsLine() );
  const int CC = CrsChar();

  // Search forward
  unsigned level = 0;
  bool     found = false;

  for( int vl=CL; !found && 0<=vl; vl-- )
  {
    const unsigned LL = pV->pfb->LineLen( vl );

    for( int p=(CL==vl)?(CC-1):(LL-1); !found && 0<=p; p-- )
    {
      const char C = pV->pfb->Get( vl, p );

      if     ( C==ST_C ) level++;
      else if( C==FN_C )
      {
        if( 0 < level ) level--;
        else {
          found = true;

          // Convert from view line back to dif line:
          const unsigned dl = DiffLine(pV, vl);

          GoToCrsPos_Write( dl, p );
        }
      }
    }
  }
}

void Diff::GoToNextWord()
{
  Trace trace( __PRETTY_FUNCTION__ );
  CrsPos ncp = { 0, 0 };

  if( GoToNextWord_GetPosition( ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

// Returns true if found next word, else false
//
bool Diff::GoToNextWord_GetPosition( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  const unsigned NUM_LINES = pV->pfb->NumLines();
  if( 0==NUM_LINES ) return false;

  bool found_space = false;
  bool found_word  = false;

  // Convert from diff line (CrsLine()), to view line:
  const unsigned OCL = ViewLine( pV, CrsLine() ); //< Old cursor view line
  const unsigned OCP = CrsChar();                 // Old cursor position

  IsWord_Func isWord = IsWord_Ident;

  // Find white space, and then find non-white space
  for( unsigned vl=OCL; (!found_space || !found_word) && vl<NUM_LINES; vl++ )
  {
    const unsigned LL = pV->pfb->LineLen( vl );
    if( LL==0 || OCL<vl )
    {
      found_space = true;
      // Once we have encountered a space, word is anything non-space.
      // An empty line is considered to be a space.
      isWord = NotSpace;
    }
    const unsigned START_C = OCL==vl ? OCP : 0;

    for( unsigned p=START_C; (!found_space || !found_word) && p<LL; p++ )
    {
      const int C = pV->pfb->Get( vl, p );

      if( found_space  )
      {
        if( isWord( C ) ) found_word = true;
      }
      else {
        if( !isWord( C ) ) found_space = true;
      }
      // Once we have encountered a space, word is anything non-space
      if( IsSpace( C ) ) isWord = NotSpace;

      if( found_space && found_word )
      {
        // Convert from view line back to diff line:
        const unsigned dl = DiffLine( pV, vl );

        ncp.crsLine = dl;
        ncp.crsChar = p;
      }
    }
  }
  return found_space && found_word;
}

void Diff::GoToPrevWord()
{
  Trace trace( __PRETTY_FUNCTION__ );
  CrsPos ncp = { 0, 0 };

  if( GoToPrevWord_GetPosition( ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

// Return true if new cursor position found, else false
bool Diff::GoToPrevWord_GetPosition( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  const unsigned NUM_LINES = pV->pfb->NumLines();
  if( 0==NUM_LINES ) return false;

  // Convert from diff line (CrsLine()), to view line:
  const int      OCL = ViewLine( pV, CrsLine() );
  const unsigned LL  = pV->pfb->LineLen( OCL );

  if( LL < CrsChar() ) // Since cursor is now allowed past EOL,
  {                    // it may need to be moved back:
    if( LL && !IsSpace( pV->pfb->Get( OCL, LL-1 ) ) )
    {
      // Backed up to non-white space, which is previous word, so return true
      // Convert from view line back to diff line:
      ncp.crsLine = CrsLine(); //< diff line
      ncp.crsChar = LL-1;
      return true;
    }
    else {
      GoToCrsPos_NoWrite( CrsLine(), LL ? LL-1 : 0 );
    }
  }
  bool found_space = false;
  bool found_word  = false;
  const unsigned OCP = CrsChar(); // Old cursor position

  IsWord_Func isWord = NotSpace;

  // Find word to non-word transition
  for( int vl=OCL; (!found_space || !found_word) && -1<vl; vl-- )
  {
    const int LL = pV->pfb->LineLen( vl );
    if( LL==0 || vl<OCL )
    {
      // Once we have encountered a space, word is anything non-space.
      // An empty line is considered to be a space.
      isWord = NotSpace;
    }
    const unsigned START_C = OCL==vl ? OCP-1 : LL-1;

    for( int p=START_C; (!found_space || !found_word) && -1<p; p-- )
    {
      const int C = pV->pfb->Get( vl, p);

      if( found_word  )
      {
        if( !isWord( C ) || p==0 ) found_space = true;
      }
      else {
        if( isWord( C ) ) {
          found_word = true;
          if( 0==p ) found_space = true;
        }
      }
      // Once we have encountered a space, word is anything non-space
      if( IsIdent( C ) ) isWord = IsWord_Ident;

      if( found_space && found_word )
      {
        // Convert from view line back to diff line:
        const unsigned dl = DiffLine( pV, vl );

        ncp.crsLine = dl;
        ncp.crsChar = p;
      }
    }
    if( found_space && found_word )
    {
      if( ncp.crsChar && ncp.crsChar < LL-1 ) ncp.crsChar++;
    }
  }
  return found_space && found_word;
}

void Diff::GoToEndOfWord()
{
  Trace trace( __PRETTY_FUNCTION__ );
  CrsPos ncp = { 0, 0 };

  if( GoToEndOfWord_GetPosition( ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

// Returns true if found end of word, else false
// 1. If at end of word, or end of non-word, move to next char
// 2. If on white space, skip past white space
// 3. If on word, go to end of word
// 4. If on non-white-non-word, go to end of non-white-non-word
bool Diff::GoToEndOfWord_GetPosition( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  const unsigned NUM_LINES = pV->pfb->NumLines();
  if( 0==NUM_LINES ) return false;

  // Convert from diff line (CrsLine()), to view line:
  const unsigned CL = ViewLine( pV, CrsLine() );
  const unsigned LL = pV->pfb->LineLen( CL );
        unsigned CP = CrsChar(); // Cursor position

  // At end of line, or line too short:
  if( (LL-1) <= CP || LL < 2 ) return false;

  int CC = pV->pfb->Get( CL, CP );   // Current char
  int NC = pV->pfb->Get( CL, CP+1 ); // Next char

  // 1. If at end of word, or end of non-word, move to next char
  if( (IsWord_Ident   ( CC ) && !IsWord_Ident   ( NC ))
   || (IsWord_NonIdent( CC ) && !IsWord_NonIdent( NC )) ) CP++;

  // 2. If on white space, skip past white space
  if( IsSpace( pV->pfb->Get(CL, CP) ) )
  {
    for( ; CP<LL && IsSpace( pV->pfb->Get(CL, CP) ); CP++ ) ;
    if( LL <= CP ) return false; // Did not find non-white space
  }
  // At this point (CL,CP) should be non-white space
  CC = pV->pfb->Get( CL, CP );  // Current char

  ncp.crsLine = CrsLine(); // Diff line

  if( IsWord_Ident( CC ) ) // On identity
  {
    // 3. If on word space, go to end of word space
    for( ; CP<LL && IsWord_Ident( pV->pfb->Get(CL, CP) ); CP++ )
    {
      ncp.crsChar = CP;
    }
  }
  else if( IsWord_NonIdent( CC ) )// On Non-identity, non-white space
  {
    // 4. If on non-white-non-word, go to end of non-white-non-word
    for( ; CP<LL && IsWord_NonIdent( pV->pfb->Get(CL, CP) ); CP++ )
    {
      ncp.crsChar = CP;
    }
  }
  else { // Should never get here:
    return false;
  }
  return true;
}

void Diff::Do_n()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( gl_pVis->star.len() ) Do_n_Pattern();
  else                      Do_n_Diff();
}

void Diff::Do_n_Pattern()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  const unsigned NUM_LINES = pV->pfb->NumLines();

  if( NUM_LINES == 0 ) return;

  CrsPos ncp = { 0, 0 }; // Next cursor position

  if( Do_n_FindNextPattern( ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

bool Diff::Do_n_FindNextPattern( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned NUM_LINES = pfb->NumLines();
  const unsigned STAR_LEN  = gl_pVis->star.len();

  const unsigned OCL = CrsLine(); // Diff line
  const unsigned OCC = CrsChar();

  const unsigned OCLv = ViewLine( pV, OCL ); // View line

  unsigned st_l = OCLv;
  unsigned st_c = OCC;

  bool found_next_star = false;

  // Move past current star:
  const unsigned LL = pfb->LineLen( OCLv );

  for( ; st_c<LL && pV->InStar(OCLv,st_c); st_c++ ) ;

  // Go down to next line
  if( LL <= st_c ) { st_c=0; st_l++; }

  // Search for first star position past current position
  for( unsigned l=st_l; !found_next_star && l<NUM_LINES; l++ )
  {
    const unsigned LL = pfb->LineLen( l );

    for( unsigned p=st_c
       ; !found_next_star && p<LL
       ; p++ )
    {
      if( pV->InStar(l,p) )
      {
        found_next_star = true;
        // Convert from view line back to diff line:
        const unsigned dl = DiffLine( pV, l );
        ncp.crsLine = dl;
        ncp.crsChar = p;
      }
    }
    // After first line, always start at beginning of line
    st_c = 0;
  }
  // Near end of file and did not find any patterns, so go to first pattern in file
  if( !found_next_star )
  {
    for( unsigned l=0; !found_next_star && l<=OCLv; l++ )
    {
      const unsigned LL = pfb->LineLen( l );
      const unsigned END_C = (OCLv==l) ? Min( OCC, LL ) : LL;

      for( unsigned p=0; !found_next_star && p<END_C; p++ )
      {
        if( pV->InStar(l,p) )
        {
          found_next_star = true;
          // Convert from view line back to diff line:
          const unsigned dl = DiffLine( pV, l );
          ncp.crsLine = dl;
          ncp.crsChar = p;
        }
      }
    }
  }
  return found_next_star;
}

void Diff::Do_n_Diff()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();
  if( 0==NUM_LINES ) return;

  unsigned dl = CrsLine(); // Diff line

  View* pV = gl_pVis->CV();

  Array_t<Diff_Info>& DI_List = (pV == pvS) ? DI_List_S : DI_List_L;

  const Diff_Type DT = DI_List[dl].diff_type; // Current diff type

  bool found = true;

  if( DT == DT_CHANGED || DT == DT_INSERTED || DT == DT_DELETED )
  {
    found = Do_n_Search_for_Same( dl, DI_List );
  }
  if( found )
  {
    found = Do_n_Search_for_Diff( dl, DI_List );

    if( found )
    {
      GoToCrsPos_Write( dl, CrsChar() );
    }
  }
}

bool Diff::Do_n_Search_for_Same( unsigned& dl
                               , const Array_t<Diff_Info>& DI_List )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();

  // Search forward for DT_SAME
  bool found = false;

  while( !found && dl<NUM_LINES )
  {
    const Diff_Type DT = DI_List[dl].diff_type;

    if( DT == DT_SAME )
    {
      found = true;
    }
    else dl++;
  }
  return found;
}

bool Diff::Do_n_Search_for_Diff( unsigned& dl
                               , const Array_t<Diff_Info>& DI_List )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();

  // Search forward for non-DT_SAME
  bool found = false;

  while( !found && dl<NUM_LINES )
  {
    const Diff_Type DT = DI_List[dl].diff_type;

    if( DT == DT_CHANGED || DT == DT_INSERTED || DT == DT_DELETED )
    {
      found = true;
    }
    else dl++;
  }
  return found;
}

void Diff::Do_N()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( gl_pVis->star.len() ) Do_N_Pattern();
  else                      Do_N_Diff();
}

void Diff::Do_N_Pattern()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  const unsigned NUM_LINES = pV->pfb->NumLines();

  if( NUM_LINES == 0 ) return;

  CrsPos ncp = { 0, 0 }; // Next cursor position

  if( Do_N_FindPrevPattern( ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

bool Diff::Do_N_FindPrevPattern( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();

  View* pV = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned NUM_LINES = pfb->NumLines();
  const unsigned STAR_LEN  = gl_pVis->star.len();

  const unsigned OCL = CrsLine();
  const unsigned OCC = CrsChar();

  const unsigned OCLv = ViewLine( pV, OCL ); // View line

  bool found_prev_star = false;

  // Search for first star position before current position
  for( int l=OCLv; !found_prev_star && 0<=l; l-- )
  {
    const int LL = pfb->LineLen( l );

    int p=LL-1;
    if( OCLv==l ) p = OCC ? OCC-1 : 0;

    for( ; 0<p && !found_prev_star; p-- )
    {
      for( ; 0<=p && pV->InStar(l,p); p-- )
      {
        found_prev_star = true;
        // Convert from view line back to diff line:
        const unsigned dl = DiffLine( pV, l );
        ncp.crsLine = dl;
        ncp.crsChar = p;
      }
    }
  }
  // Near beginning of file and did not find any patterns, so go to last pattern in file
  if( !found_prev_star )
  {
    for( int l=NUM_LINES-1; !found_prev_star && OCLv<l; l-- )
    {
      const unsigned LL = pfb->LineLen( l );

      int p=LL-1;
      if( OCLv==l ) p = OCC ? OCC-1 : 0;

      for( ; 0<p && !found_prev_star; p-- )
      {
        for( ; 0<=p && pV->InStar(l,p); p-- )
        {
          found_prev_star = true;
          // Convert from view line back to diff line:
          const unsigned dl = DiffLine( pV, l );
          ncp.crsLine = dl;
          ncp.crsChar = p;
        }
      }
    }
  }
  return found_prev_star;
}

void Diff::Do_N_Diff()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();
  if( 0==NUM_LINES ) return;

  int dl = CrsLine();
  if( 0 == dl ) return;

  View* pV = gl_pVis->CV();

  Array_t<Diff_Info>& DI_List = (pV == pvS) ? DI_List_S : DI_List_L;

  const Diff_Type DT = DI_List[dl].diff_type; // Current diff type

  bool found = true;
  if( DT == DT_CHANGED || DT == DT_INSERTED || DT == DT_DELETED )
  {
    found = Do_N_Search_for_Same( dl, DI_List );
  }
  if( found )
  {
    found = Do_N_Search_for_Diff( dl, DI_List );

    if( found )
    {
      GoToCrsPos_Write( dl, CrsChar() );
    }
  }
}

bool Diff::Do_N_Search_for_Same( int& dl
                               , const Array_t<Diff_Info>& DI_List )
{
  // Search backwards for DT_SAME
  bool found = false;

  while( !found && 0<=dl )
  {
    if( DT_SAME == DI_List[dl].diff_type )
    {
      found = true;
    }
    else dl--;
  }
  return found;
}

bool Diff::Do_N_Search_for_Diff( int& dl
                               , const Array_t<Diff_Info>& DI_List )
{
  // Search backwards for non-DT_SAME
  bool found = false;

  while( !found && 0<=dl )
  {
    const Diff_Type DT = DI_List[dl].diff_type;

    if( DT == DT_CHANGED || DT == DT_INSERTED || DT == DT_DELETED )
    {
      found = true;
    }
    else dl--;
  }
  return found;
}

void Diff::Do_f( const char FAST_CHAR )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  const unsigned LL  = LineLen(); // Line length
  const unsigned OCP = CrsChar(); // Old cursor position

  if( LL-1 <= OCP ) return;

  View* pV = gl_pVis->CV();

  unsigned NCP = 0;
  bool found_char = false;
  for( unsigned p=OCP+1; !found_char && p<LL; p++ )
  {
    const char C = pV->pfb->Get( OCL, p );

    if( C == FAST_CHAR )
    {
      NCP = p;
      found_char = true;
    }
  }
  if( found_char )
  {
    GoToCrsPos_Write( OCL, NCP );
  }
}

// If past end of line, move back to end of line.
// Returns true if moved, false otherwise.
//
bool Diff::MoveInBounds()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV  = gl_pVis->CV();

  const unsigned DL  = CrsLine();  // Diff line
  const unsigned VL  = ViewLine( pV, DL );      // View line
  const unsigned LL  = pV->pfb->LineLen( VL );
  const unsigned EOL = LL ? LL-1 : 0;

  if( EOL < CrsChar() ) // Since cursor is now allowed past EOL,
  {                      // it may need to be moved back:
    GoToCrsPos_NoWrite( DL, EOL );
    return true;
  }
  return false;
}

void Diff::MoveCurrLineToTop()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0<crsRow )
  {
    topLine += crsRow;
    crsRow = 0;
    Update();
  }
}

void Diff::MoveCurrLineCenter()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  const unsigned center = SCast<unsigned>( 0.5*WorkingRows(pV) + 0.5 );

  const unsigned OCL = CrsLine(); // Old cursor line

  if( 0 < OCL && OCL < center && 0 < topLine )
  {
    // Cursor line cannot be moved to center, but can be moved closer to center
    // CrsLine() does not change:
    crsRow += topLine;
    topLine = 0;
    Update();
  }
  else if( center < OCL
        && center != crsRow )
  {
    topLine += crsRow - center;
    crsRow = center;
    Update();
  }
}

void Diff::MoveCurrLineToBottom()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 < topLine )
  {
    View* pV = gl_pVis->CV();

    const unsigned WR  = WorkingRows( pV );
    const unsigned OCL = CrsLine(); // Old cursor line

    if( WR-1 <= OCL )
    {
      topLine -= WR - crsRow - 1;
      crsRow = WR-1;
      Update();
    }
    else {
      // Cursor line cannot be moved to bottom, but can be moved closer to bottom
      // CrsLine() does not change:
      crsRow += topLine;
      topLine = 0;
      Update();
    }
  }
}

String Diff::Do_Star_GetNewPattern()
{
  Trace trace( __PRETTY_FUNCTION__ );
  String new_star;
  View* pV  = gl_pVis->CV();

  if( pV->pfb->NumLines() == 0 ) return new_star;

  const unsigned CL = CrsLine();
  // Convert CL, which is diff line, to view line:
  const unsigned CLv = ViewLine( pV, CrsLine() );
  const unsigned LL = pV->pfb->LineLen( CLv );

  if( LL )
  {
    MoveInBounds();
    const unsigned CC = CrsChar();

    const int c = pV->pfb->Get( CLv,  CC );

    if( isalnum( c ) || c=='_' )
    {
      new_star.push( c );

      // Search forward:
      for( unsigned k=CC+1; k<LL; k++ )
      {
        const int c = pV->pfb->Get( CLv, k );
        if( isalnum( c ) || c=='_' ) new_star.push( c );
        else                         break;
      }
      // Search backward:
      for( int k=CC-1; 0<=k; k-- )
      {
        const int c = pV->pfb->Get( CLv, k );
        if( isalnum( c ) || c=='_' ) new_star.insert( 0, c );
        else                         break;
      }
    }
    else {
      if( isgraph( c ) ) new_star.push( c );
    }
  }
  return new_star;
}

void Diff::Do_i()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  pV->inInsertMode = true;
  DisplayBanner();

  if( 0 == pfb->NumLines() ) pfb->PushLine();

  const unsigned DL = CrsLine(); // Diff line number
  const unsigned VL = ViewLine( pV, DL ); // View line number
  const unsigned LL = pfb->LineLen( VL ); // Line length

  if( LL < CrsChar() ) // Since cursor is now allowed past EOL,
  {                    // it may need to be moved back:
    // For user friendlyness, move cursor to new position immediately:
    GoToCrsPos_Write( DL, LL );
  }
  unsigned count = 0;
  for( char c=gl_pKey->In(); c != ESC; c=gl_pKey->In() )
  {
    if( BS == c || DEL == c )
    {
      if( 0<count )
      {
        InsertBackspace();
        count--;
      }
    }
    if( IsEndOfLineDelim( c ) )
    {
      InsertAddReturn();
      count++;
    }
    else {
      InsertAddChar( c );
      count++;
    }
  }
  pV->inInsertMode = false;

  // Move cursor back one space:
  if( crsCol ) crsCol--;

  // Always update when leaving insert mode because banner will need to be removed:
  Update();
}

void Diff::InsertAddChar( const char c )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  if( pfb->NumLines()==0 ) pfb->PushLine();

  const unsigned DL = CrsLine(); // Diff line number

  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current
  Diff_Info& cDI = cDI_List[ DL ];

  const unsigned VL = ViewLine( pV, DL ); // View line number

  if( DT_DELETED == cDI.diff_type )
  {
    crsCol = 0;
    pfb->InsertLine( VL+1 );
    pfb->InsertChar( VL+1, 0, c );
    Patch_Diff_Info_Inserted( pV, DL );
  }
  else {
    pfb->InsertChar( VL, CrsChar(), c );
    Patch_Diff_Info_Changed( pV, DL );
  }
  if( WorkingCols( pV ) <= crsCol+1 )
  {
    // On last working column, need to scroll right:
    leftChar++;
  }
  else {
    crsCol += 1;
  }
  Update();
}

void Diff::InsertAddReturn()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  // The lines in fb do not end with '\n's.
  // When the file is written, '\n's are added to the ends of the lines.
  Line new_line(__FILE__,__LINE__);
  const unsigned DL = CrsLine();          // Diff line number
  const unsigned VL = ViewLine( pV, DL ); // View line number
  const unsigned OLL = pfb->LineLen( VL ); // Old line length
  const unsigned OCP = CrsChar();          // Old cursor position

  for( unsigned k=OCP; k<OLL; k++ )
  {
    const uint8_t C = pfb->RemoveChar( VL, OCP );
    bool ok = new_line.push(__FILE__,__LINE__, C );
    ASSERT( __LINE__, ok, "ok" );
  }
  // Truncate the rest of the old line:
  // Add the new line:
  const unsigned new_line_num = VL+1;
  pfb->InsertLine( new_line_num, new_line );
  crsCol = 0;
  leftChar = 0;
  if( DL < BotLine( pV ) ) crsRow++;
  else {
    // If we were on the bottom working line, scroll screen down
    // one line so that the cursor line is not below the screen.
    topLine++;
  }
  Patch_Diff_Info_Changed( pV, DL );
  Patch_Diff_Info_Inserted( pV, DL+1 );
  Update();
}

void Diff::InsertBackspace()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  // If no lines in buffer, no backspacing to be done
  if( 0==pfb->NumLines() ) return;

  const unsigned DL = CrsLine();  // Diff line

  const unsigned OCP = CrsChar(); // Old cursor position

  if( OCP ) InsertBackspace_RmC ( DL, OCP );
  else      InsertBackspace_RmNL( DL );
}

void Diff::InsertBackspace_RmC( const unsigned DL
                              , const unsigned OCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned VL = ViewLine( pV, DL ); // View line number

  pfb->RemoveChar( VL, OCP-1 );

  crsCol -= 1;

  Patch_Diff_Info_Changed( pV, DL );
  Update();
}

void Diff::InsertBackspace_RmNL( const unsigned DL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned VL = ViewLine( pV, DL ); // View line number

  // Cursor Line Position is zero, so:
  // 1. Save previous line, end of line + 1 position
  CrsPos ncp = { DL-1, pfb->LineLen( VL-1 ) };

  // 2. Remove the line
  Line lr(__FILE__, __LINE__);
  pfb->RemoveLine( VL, lr );

  // 3. Append rest of line to previous line
  pfb->AppendLineToLine( VL-1, lr );

  // 4. Put cursor at the old previous line end of line + 1 position
  const bool MOVE_UP    = ncp.crsLine < topLine;
  const bool MOVE_RIGHT = RightChar( pV ) < ncp.crsChar;

  if( MOVE_UP ) topLine = ncp.crsLine;
                crsRow = ncp.crsLine - topLine;

  if( MOVE_RIGHT ) leftChar = ncp.crsChar - WorkingCols( pV ) + 1;
                   crsCol = ncp.crsChar - leftChar;

  // 5. Removed a line, so update to re-draw window view
  Patch_Diff_Info_Deleted( pV, DL );
  Patch_Diff_Info_Changed( pV, DL-1 );
  Update();
}

void Diff::Do_a()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  if( 0<pfb->NumLines() )
  {
    const unsigned DL = CrsLine();
    const unsigned VL = ViewLine( pV, DL ); // View line number
    const unsigned LL = pfb->LineLen( VL );

    if( 0<LL ) {
      const bool CURSOR_AT_EOL = ( CrsChar() == LL-1 );
      if( CURSOR_AT_EOL )
      {
        GoToCrsPos_NoWrite( DL, LL );
      }
      const bool CURSOR_AT_RIGHT_COL = ( crsCol == WorkingCols( pV )-1 );

      if( CURSOR_AT_RIGHT_COL )
      {
        // Only need to scroll window right, and then enter insert i:
        leftChar++; //< This increments CrsChar()
      }
      else if( !CURSOR_AT_EOL ) // If cursor was at EOL, already moved cursor forward
      {
        // Only need to move cursor right, and then enter insert i:
        crsCol += 1; //< This increments CrsChar()
      }
      Update();
    }
  }
  Do_i();
}

void Diff::Do_A()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToEndOfLine();

  Do_a();
}

void Diff::Do_o()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned NL = pfb->NumLines();
  const unsigned DL = CrsLine();

  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current

  const bool ON_DELETED = DT_DELETED == cDI_List[ DL ].diff_type;

  // If no lines or on a deleted line, just Do_i()
  if( 0<NL && !ON_DELETED )
  {
    const unsigned VL = ViewLine( pV, DL ); // View line

    pfb->InsertLine( VL+1 );
    crsCol   = 0;
    leftChar = 0;
    if( DL < BotLine( pV ) ) crsRow++;
    else {
      // If we were on the bottom working line, scroll screen down
      // one line so that the cursor line is not below the screen.
      topLine++;
    }
    Patch_Diff_Info_Inserted( pV, DL+1, false );

    Update();
  }
  Do_i();
}

// Wrapper around Do_o approach:
void Diff::Do_O()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned DL = CrsLine();

  if( 0<DL )
  {
    // Not on top line, so just back up and then Do_o:
    GoToCrsPos_NoWrite( DL-1, CrsChar() );
    Do_o();
  }
  else {
    // On top line, so cannot move up a line and then Do_o,
    // so use some custom code:
    View*    pV  = gl_pVis->CV();
    FileBuf* pfb = pV->pfb;

    pfb->InsertLine( 0 );
    Patch_Diff_Info_Inserted( pV, 0, true );

    Update();
    Do_i();
  }
}

void Diff::Do_x()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  // If there is nothing to 'x', just return:
  if( 0<pfb->NumLines() )
  {
    const unsigned DL = CrsLine(); // Diff line number
    const unsigned VL = ViewLine( pV, DL ); // View line number
    const unsigned LL = pfb->LineLen( VL );

    // If nothing on line, just return:
    if( 0<LL )
    {
      // If past end of line, move to end of line:
      if( LL-1 < CrsChar() )
      {
        GoToCrsPos_Write( DL, LL-1 );
      }
      const uint8_t C = pfb->RemoveChar( VL, CrsChar() );

      // Put char x'ed into register:
      Line* nlp = gl_pVis->BorrowLine(__FILE__,__LINE__);
      nlp->push(__FILE__,__LINE__, C );
      gl_pVis->reg.clear();
      gl_pVis->reg.push( nlp );
      gl_pVis->paste_mode = PM_ST_FN;
    
      const unsigned NLL = pfb->LineLen( VL ); // New line length

      // Reposition the cursor:
      if( NLL <= leftChar+crsCol )
      {
        // The char x'ed is the last char on the line, so move the cursor
        //   back one space.  Above, a char was removed from the line,
        //   but crsCol has not changed, so the last char is now NLL.
        // If cursor is not at beginning of line, move it back one more space.
        if( crsCol ) crsCol--;
      }
      Patch_Diff_Info_Changed( pV, DL );
      Update();
    }
  }
}

void Diff::Do_s()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned DL  = CrsLine();          // Diff line
  const unsigned VL  = ViewLine( pV, DL ); // View line
  const unsigned LL  = pfb->LineLen( VL );
  const unsigned EOL = LL ? LL-1 : 0;
  const unsigned CP  = CrsChar();

  if( CP < EOL )
  {
    Do_x();
    Do_i();
  }
  else if( EOL < CP )
  {
    // Extend line out to where cursor is:
    for( unsigned k=LL; k<CP; k++ )
    {
      pfb->PushChar( VL, ' ' );
    }
    Patch_Diff_Info_Changed( pV, DL );
    Do_a();
  }
  else // CP == EOL
  {
    Do_x();
    Do_a();
  }
}

void Diff::Do_D()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned NUM_LINES = pfb->NumLines();
  const unsigned DL = CrsLine();  // Old cursor line
  const unsigned VL = ViewLine( pV, DL ); // View line
  const unsigned CP = CrsChar();  // Old cursor position
  const unsigned LL = pfb->LineLen( VL );  // Old line length

  // If there is nothing to 'D', just return:
  if( 0<NUM_LINES && 0<LL && CP<LL )
  {
    Line* lpd = gl_pVis->BorrowLine( __FILE__,__LINE__ );

    for( unsigned k=CP; k<LL; k++ )
    {
      uint8_t c = pfb->RemoveChar( VL, CP );
      lpd->push(__FILE__,__LINE__, c );
    }
    gl_pVis->reg.clear();
    gl_pVis->reg.push( lpd );
    gl_pVis->paste_mode = PM_ST_FN;

    // If cursor is not at beginning of line, move it back one space.
    if( 0<crsCol ) crsCol--;

    Patch_Diff_Info_Changed( pV, DL );
    Update();
  }
}

void Diff::Do_dd()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  const unsigned NVL = pV->pfb->NumLines();   // Number of view lines

  // If there is nothing to 'dd', just return:
  if( 1 < NVL )
  {
    const unsigned DL = CrsLine(); // Old Diff line

    // Cant delete a deleted or unknown line
    const Diff_Type DT = DiffType( pV, DL );
    if( DT != DT_UNKN0WN && DT != DT_DELETED )
    {
      const unsigned VL = ViewLine( pV, DL );    // View line

      // Remove line from FileBuf and save in paste register:
      Line* lp = pV->pfb->RemoveLineP( VL );
      if( lp ) {
        // gl_pVis->reg will own lp
        gl_pVis->reg.clear();
        gl_pVis->reg.push( lp );
    
        gl_pVis->paste_mode = PM_LINE;
      }
      Patch_Diff_Info_Deleted( pV, DL );

      // Figure out where to put cursor after deletion:
      const bool DELETED_LAST_LINE = VL == NVL-1;

      unsigned ncld = DL;
      // Deleting last line of file, so move to line above:
      if( DELETED_LAST_LINE ) ncld--;
      else {
        // If cursor is now sitting on a deleted line, move to line below:
        const Diff_Type DTN = DiffType( pV, DL );
        if( DTN == DT_DELETED ) ncld++;
      }
      GoToCrsPos_NoWrite( ncld, CrsChar() );

      Update();
    }
  }
}

void Diff::Do_J()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const int DL  = CrsLine(); // Diff line
  const int VL  = ViewLine( pV, DL ); // View line

  if( VL < pfb->NumLines()-1 )
  {
    Array_t<Diff_Info> cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current diff info list
    const Diff_Type cDT = cDI_List[DL].diff_type; // Current diff type

    if( 0 < VL
     && ( cDT == DT_SAME
       || cDT == DT_CHANGED
       || cDT == DT_INSERTED ) )
    {
      const int DLp = DiffLine( pV, VL+1 ); // Diff line for VL+1

      Line* lp = pfb->RemoveLineP( VL+1 );
      Patch_Diff_Info_Deleted( pV, DLp );

      pfb->AppendLineToLine( VL, lp );
      Patch_Diff_Info_Changed( pV, DL );

      Update();
    }
  }
}

void Diff::Do_dw()
{
}
void Diff::Do_cw()
{
}

void Diff::Do_yy()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();

  // If there is nothing to 'yy', just return:
  if( 0<pV->pfb->NumLines() )
  {
    const unsigned DL = CrsLine();  // Diff line

    // Cant yank a deleted or unknown line
    const Diff_Type DT = DiffType( pV, DL );
    if( DT != DT_UNKN0WN && DT != DT_DELETED )
    {
      const unsigned VL = ViewLine( pV, DL ); // View Cursor line

      Line l = pV->pfb->GetLine( VL );

      gl_pVis->reg.clear();
      gl_pVis->reg.push( gl_pVis->BorrowLine( __FILE__,__LINE__, l ) );

      gl_pVis->paste_mode = PM_LINE;
    }
  }
}

void Diff::Do_y_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  gl_pVis->reg.clear();

  if( inVisualBlock ) Do_y_v_block();
  else                Do_y_v_st_fn();

  inVisualMode = false;
}

void Diff::Do_y_v_block()
{
  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  Swap_Visual_Block_If_Needed();

  for( unsigned DL=v_st_line; DL<=v_fn_line; DL++ )
  {
    Line* nlp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

    const unsigned VL = ViewLine( pV, DL );
    const unsigned LL = pfb->LineLen( VL );

    for( unsigned P = v_st_char; P<LL && P <= v_fn_char; P++ )
    {
      nlp->push( __FILE__,__LINE__, pfb->Get( VL, P ) );
    }
    // gl_pVis->reg will delete nlp
    gl_pVis->reg.push( nlp );
  }
  gl_pVis->paste_mode = PM_BLOCK;

  // Try to put cursor at (v_st_line, v_st_char), but
  // make sure the cursor is in bounds after the deletion:
  const unsigned NUM_LINES = NumLines();
  unsigned ncl = v_st_line;
  if( NUM_LINES <= ncl ) ncl = NUM_LINES-1;
  const unsigned NLL = pfb->LineLen( ViewLine( pV, ncl ) );
  unsigned ncc = 0;
  if( 0<NLL ) ncc = NLL <= v_st_char ? NLL-1 : v_st_char;

  GoToCrsPos_NoWrite( ncl, ncc );
}

void Diff::Do_y_v_st_fn()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  Swap_Visual_Block_If_Needed();

  for( unsigned L=v_st_line; L<=v_fn_line; L++ )
  {
    const unsigned LINE_DIFF_TYPE = DiffType( pV, L );
    if( LINE_DIFF_TYPE != DT_DELETED )
    {
      Line* nlp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

      // Convert L, which is diff line, to view line
      const unsigned VL = ViewLine( pV, L );
      const unsigned LL = pV->pfb->LineLen( VL );

      if( 0<LL )
      {
        const unsigned P_st = (L==v_st_line) ? v_st_char : 0;
        const unsigned P_fn = (L==v_fn_line) ? Min(LL-1,v_fn_char) : LL-1;

        for( unsigned P = P_st; P <= P_fn; P++ )
        {
          nlp->push(__FILE__,__LINE__, pV->pfb->Get( VL, P ) );
        }
      }
      // gl_pVis->reg will delete nlp
      gl_pVis->reg.push( nlp );
    }
  }
  gl_pVis->paste_mode = PM_ST_FN;
}

void Diff::Do_Y_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  gl_pVis->reg.clear();

  if( inVisualBlock ) Do_y_v_block();
  else                Do_Y_v_st_fn();

  inVisualMode = false;
}

void Diff::Do_Y_v_st_fn()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  Swap_Visual_Block_If_Needed();

  for( unsigned L=v_st_line; L<=v_fn_line; L++ )
  {
    const unsigned LINE_DIFF_TYPE = DiffType( pV, L );
    if( LINE_DIFF_TYPE != DT_DELETED )
    {
      Line* nlp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

      // Convert L, which is diff line, to view line
      const unsigned VL = ViewLine( pV, L );
      const unsigned LL = pfb->LineLen( VL );

      if( 0<LL )
      {
        for( unsigned P = 0; P <= LL-1; P++ )
        {
          nlp->push(__FILE__,__LINE__,  pfb->Get( VL, P ) );
        }
      }
      // gl_pVis->reg will delete nlp
      gl_pVis->reg.push( nlp );
    }
  }
  gl_pVis->paste_mode = PM_LINE;
}

void Diff::Do_D_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  gl_pVis->reg.clear();
  Swap_Visual_Block_If_Needed();

  bool removed_line = false;
  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current diff info list
  Array_t<Diff_Info>& oDI_List = (pV == pvS) ? DI_List_L : DI_List_S; // Other   diff info list
  const unsigned VL = ViewLine( pV, v_st_line ); // View line

  // To avoid crashing, dont remove all lines in file
  for( unsigned DL = v_st_line; 1 < pfb->NumLines() && DL<=v_fn_line; DL++ )
  {
    const Diff_Type cDT = cDI_List[DL].diff_type; // Current diff type
    const Diff_Type oDT = oDI_List[DL].diff_type; // Other   diff type

    if( cDT == DT_SAME
     || cDT == DT_CHANGED
     || cDT == DT_INSERTED )
    {
      Line* lp = pfb->RemoveLineP( VL );
      gl_pVis->reg.push( lp ); // gl_pVis->reg will delete lp

      Patch_Diff_Info_Deleted( pV, DL );

      removed_line = true;
      // If line on other side is DT_DELETED, a diff line will be removed
      // from both sides, so decrement DL to stay on same DL, decrement
      // v_fn_line because it just moved up a line
      if( oDT == DT_DELETED ) { DL--; v_fn_line--; }
    }
  }
  gl_pVis->paste_mode = PM_LINE;

  // Deleted lines will be removed, so no need to Undo_v()
  inVisualMode = false;

  if( removed_line )
  {
    Do_D_v_find_new_crs_pos();
    Update();
  }
}
void Diff::Do_D_v_find_new_crs_pos()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV  = gl_pVis->CV();

  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current diff info list

  // Figure out new cursor position:
  unsigned ncld = v_fn_line+1;
  if( cDI_List.len()-1 < ncld ) ncld = cDI_List.len()-1;

  const unsigned nclv = ViewLine( pV, ncld );
  const unsigned NCLL = pV->pfb->LineLen( nclv );

  unsigned ncc = 0;
  if( NCLL ) ncc = v_st_char < NCLL ? v_st_char : NCLL-1;

  GoToCrsPos_NoWrite( ncld, ncc );
}

void Diff::Do_s_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Need to know if cursor is at end of line before Do_x_v() is called:
  const int LL = LineLen();
  const bool CURSOR_AT_END_OF_LINE = 0<LL ? CrsChar() == LL-1 : false;

  Do_x_v();

  if( inVisualBlock )
  {
  //if( CURSOR_AT_END_OF_LINE ) Do_a_vb();
  //else                        Do_i_vb(); 
  }
  else {
    if( CURSOR_AT_END_OF_LINE ) Do_a();
    else                        Do_i();
  }
  inVisualMode = false;
}

void Diff::Do_Tilda_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  Swap_Visual_Block_If_Needed();

  if( inVisualBlock ) Do_Tilda_v_block();
  else                Do_Tilda_v_st_fn();

  inVisualMode = false;
}
void Diff::Do_Tilda_v_st_fn()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  for( unsigned DL = v_st_line; DL<=v_fn_line; DL++ )
  {
    const unsigned VL = ViewLine( pV, DL );
    const unsigned LL = pfb->LineLen( VL );
    const unsigned P_st = (DL==v_st_line) ? v_st_char : 0;
    const unsigned P_fn = (DL==v_fn_line) ? v_fn_char : (0<LL?LL-1:0);

    bool changed_line = false;

    for( unsigned P = P_st; P <= P_fn; P++ )
    {
      char C = pfb->Get( VL, P );
      bool changed = false;
      if     ( isupper( C ) ) { C = tolower( C ); changed = true; }
      else if( islower( C ) ) { C = toupper( C ); changed = true; }

      if( changed )
      {
        pfb->Set( VL, P, C );
        changed_line = true;
      }
    }
    if( changed_line )
    {
      Patch_Diff_Info_Changed( pV, DL );
    }
  }
}
void Diff::Do_Tilda_v_block()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  for( unsigned L = v_st_line; L<=v_fn_line; L++ )
  {
    const unsigned LL = pfb->LineLen( L );

    for( unsigned P = v_st_char; P<LL && P <= v_fn_char; P++ )
    {
      char C = pfb->Get( L, P );
      bool changed = false;
      if     ( isupper( C ) ) { C = tolower( C ); changed = true; }
      else if( islower( C ) ) { C = toupper( C ); changed = true; }
      if( changed ) pfb->Set( L, P, C );
    }
  }
}

void Diff::Do_x_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( inVisualBlock )
  {
    Do_x_range_block();
  }
  else {
    Do_x_range();
  }
  inVisualMode = false;

  Update(); //<- No need to Undo_v() or Remove_Banner() because of this
}

void Diff::Do_x_range()
{
  Do_x_range_pre();

  if( v_st_line == v_fn_line )
  {
    Do_x_range_single( v_st_line, v_st_char, v_fn_char );
  }
  else {
    Do_x_range_multiple( v_st_line, v_st_char, v_fn_line, v_fn_char );
  }
  Do_x_range_post( v_st_line, v_st_char );
}

void Diff::Do_x_range_pre()
{
  Trace trace( __PRETTY_FUNCTION__ );

  Swap_Visual_Block_If_Needed();

  gl_pVis->reg.clear();
}

void Diff::Do_x_range_post( unsigned st_line, unsigned st_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( inVisualBlock ) gl_pVis->paste_mode = PM_BLOCK;
  else                gl_pVis->paste_mode = PM_ST_FN;

  View* pV = gl_pVis->CV();

  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current diff info list

  // Make sure the cursor is in bounds after the deletion:
  unsigned ncld = st_line;
  if( cDI_List.len()-1 < ncld ) ncld = cDI_List.len()-1;

  const unsigned nclv = ViewLine( pV, ncld ); // New cursor line view
  const unsigned NCLL = pV->pfb->LineLen( nclv );

  unsigned ncc = 0;
  if( 0<NCLL ) ncc = st_char < NCLL ? st_char : NCLL-1;

  GoToCrsPos_NoWrite( ncld, ncc );
}

void Diff::Do_x_range_single( const unsigned L
                            , const unsigned st_char
                            , const unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned DL = L;
  const unsigned VL = ViewLine( pV, DL ); // View line

  Line* nlp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

  unsigned LL = pfb->LineLen( VL );

  // Dont remove a single line, or else Q wont work right
  bool removed_char = false;

  for( unsigned P = st_char; st_char < LL && P <= fn_char; P++ )
  {
    nlp->push(__FILE__,__LINE__, pfb->RemoveChar( VL, st_char ) );
    LL = pfb->LineLen( VL ); // Removed a char, so re-set LL
    removed_char = true;
  }
  if( removed_char ) Patch_Diff_Info_Changed( pV, DL );

  gl_pVis->reg.push( nlp );
}

void Diff::Do_x_range_multiple( const unsigned st_line
                              , const unsigned st_char
                              , const unsigned fn_line
                              , const unsigned fn_char )
{
  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current diff info list
  Array_t<Diff_Info>& oDI_List = (pV == pvS) ? DI_List_L : DI_List_S; // Other   diff info list

  bool started_in_middle = false;
  bool ended___in_middle = false;

  unsigned n_fn_line = fn_line; // New finish line

  for( unsigned DL = st_line; DL<=n_fn_line; DL++ )
  {
    const Diff_Type cDT = cDI_List[DL].diff_type; // Current diff type
    const Diff_Type oDT = oDI_List[DL].diff_type; // Other diff type

    if( cDT != DT_SAME       // If cDT is UNKN0WN or DELETED,
     && cDT != DT_CHANGED    // nothing to do so continue
     && cDT != DT_INSERTED ) continue;

    const unsigned VL  = ViewLine( pV, DL ); // View line
    const unsigned OLL = pfb->LineLen( VL ); // Original line length

    Line* nlp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

    const unsigned P_st = (DL==  st_line) ? Min(st_char,OLL-1) : 0;
    const unsigned P_fn = (DL==n_fn_line) ? Min(fn_char,OLL-1) : OLL-1;

    if(   st_line == DL && 0    < P_st  ) started_in_middle = true;
    if( n_fn_line == DL && P_fn < OLL-1 ) ended___in_middle = true;

    bool removed_char = false;
    unsigned LL = OLL;
    for( unsigned P = P_st; P_st < LL && P <= P_fn; P++ )
    {
      nlp->push( __FILE__,__LINE__, pfb->RemoveChar( VL, P_st ) );
      LL = pfb->LineLen( VL ); // Removed a char, so re-calculate LL
      removed_char = true;
    }
    if( 0 == P_st && OLL-1 == P_fn )
    {
      pfb->RemoveLine( VL );
      Patch_Diff_Info_Deleted( pV, DL );
      // If line on other side is DT_DELETED, a diff line will be removed
      // from both sides, so decrement DL to stay on same DL, decrement
      // n_fn_line because it just moved up a line
      if( oDT == DT_DELETED ) { DL--; n_fn_line--; }
    }
    else {
      if( removed_char ) Patch_Diff_Info_Changed( pV, DL );
    }
    gl_pVis->reg.push( nlp );
  }
  if( started_in_middle && ended___in_middle )
  {
    const unsigned v_st_line  = ViewLine( pV, st_line ); // View line start
    const unsigned v_fn_line  = ViewLine( pV, fn_line ); // View line finish

    Line lr(__FILE__, __LINE__);
    pfb->RemoveLine( v_fn_line, lr );
    pfb->AppendLineToLine( v_st_line, lr );

    Patch_Diff_Info_Deleted( pV, fn_line );
    Patch_Diff_Info_Changed( pV, st_line );
  }
}

void Diff::Do_x_range_block()

{
  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  Do_x_range_pre();

  for( int DL = v_st_line; DL<=v_fn_line; DL++ )
  {
    const int VL = ViewLine( pV, DL ); // View line

    Line* nlp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

    const int LL = pfb->LineLen( VL );

    for( int P = v_st_char; P<LL && P <= v_fn_char; P++ )
    {
      nlp->push( __FILE__,__LINE__, pfb->RemoveChar( VL, v_st_char ) );
    }
    gl_pVis->reg.push( nlp );
  }
  Do_x_range_post( v_st_line, v_st_char );
}

void Diff::Do_p()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( PM_ST_FN == gl_pVis->paste_mode ) return Do_p_or_P_st_fn( PP_After );
  else if( PM_BLOCK == gl_pVis->paste_mode ) return Do_p_block();
  else /*( PM_LINE  == gl_pVis->paste_mode*/ return Do_p_line();
}

void Diff::Do_p_line()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned DL = CrsLine();          // Diff line
  const unsigned VL = ViewLine( pV, DL ); // View line

  const unsigned NUM_LINES_TO_INSERT = gl_pVis->reg.len();

  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current diff info list
  Diff_Info& cDI = cDI_List[ DL ];

  // If cursor is on a deleted diff line, start inserting lines into that deleted diff line
  // If cursor is NOT on a deleted diff line, start inserting lines below diff cursor line
  const bool ON_DELETED = DT_DELETED == cDI.diff_type;
        bool ODVL0 = On_Deleted_View_Line_Zero( DL );
  const unsigned DL_START = ON_DELETED ? DL : DL+1;
  const unsigned VL_START = ODVL0      ? VL : VL+1;

  for( unsigned k=0; k<NUM_LINES_TO_INSERT; k++ )
  {
    // In FileBuf: Put reg on line below:
    pfb->InsertLine( VL_START+k, *(gl_pVis->reg[k]) );

    Patch_Diff_Info_Inserted( pV, DL_START+k, ODVL0 );
    ODVL0 = false;
  }
//pfb->Update_Styles( VL, VL+NUM_LINES_TO_INSERT );
  Update();
}

void Diff::Do_p_or_P_st_fn( Paste_Pos paste_pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned NUM_LINES = gl_pVis->reg.len();
  const unsigned ODL       = CrsLine();           // Original Diff line
  const unsigned OVL       = ViewLine( pV, ODL ); // Original View line

  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current

  for( unsigned k=0; k<NUM_LINES; k++ )
  {
    Diff_Info& cDI = cDI_List[ ODL+k ];

    const bool ON_DELETED = DT_DELETED == cDI.diff_type;

    if( 0 == k ) // Add to current line
    {
      Do_p_or_P_st_fn_FirstLine( paste_pos, k, ODL, OVL, ON_DELETED );
    }
    else if( NUM_LINES-1 == k ) // Last line
    {
      Do_p_or_P_st_fn_LastLine( k, ODL, OVL, ON_DELETED );
    }
    else { // Intermediate line
      Do_p_or_P_st_fn_IntermediatLine( k, ODL, OVL, ON_DELETED );
    }
  }
//pfb->Update_Styles( OVL, OVL+NUM_LINES );
  Update();
}
void Diff::Do_p_or_P_st_fn_FirstLine( Paste_Pos      paste_pos
                                    , const unsigned k
                                    , const unsigned ODL
                                    , const unsigned OVL
                                    , const bool     ON_DELETED )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned NUM_LINES = gl_pVis->reg.len();

  const unsigned NLL = gl_pVis->reg[ k ]->len();  // New line length
  const unsigned VL  = ViewLine( pV, ODL+k ); // View line

  if( ON_DELETED )
  {
    const bool ODVL0 = On_Deleted_View_Line_Zero( ODL );

    // In FileBuf: Put reg on line below:
    pfb->InsertLine( ODVL0 ? VL : VL+1, *(gl_pVis->reg[0]) );

    Patch_Diff_Info_Inserted( pV, ODL+k, ODVL0 );
  }
  else {
    MoveInBounds();
    const unsigned LL = pfb->LineLen( VL );
    const unsigned CP = CrsChar();         // Cursor position

    // If line we are pasting to is zero length, dont paste a space forward
    const unsigned forward = 0<LL ? ( paste_pos==PP_After ? 1 : 0 ) : 0;

    for( unsigned i=0; i<NLL; i++ )
    {
      char C = gl_pVis->reg[k]->get(i);

      pfb->InsertChar( VL, CP+i+forward, C );
    }
    Patch_Diff_Info_Changed( pV, ODL+k );

    // Move rest of first line onto new line below
    if( 1 < NUM_LINES && CP+forward < LL )
    {
      pfb->InsertLine( VL+1 );
      for( unsigned i=0; i<(LL-CP-forward); i++ )
      {
        char C = pfb->RemoveChar( VL, CP + NLL+forward );
        pfb->PushChar( VL+1, C );
      }
      Patch_Diff_Info_Inserted( pV, ODL+k+1, false ); //< Always false since we are starting on line below
    }
  }
}
void Diff::Do_p_or_P_st_fn_LastLine( const unsigned k
                                   , const unsigned ODL
                                   , const unsigned OVL
                                   , const bool     ON_DELETED )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned VL  = ViewLine( pV, ODL+k ); // View line
  const unsigned NLL = gl_pVis->reg[ k ]->len();  // New line length

  if( ON_DELETED )
  {
    pfb->InsertLine( VL+1, *(gl_pVis->reg[k]) );
    Patch_Diff_Info_Inserted( pV, ODL+k, false );
  }
  else {
    for( unsigned i=0; i<NLL; i++ )
    {
      char C = gl_pVis->reg[k]->get(i);
      pfb->InsertChar( VL, i, C );
    }
    Patch_Diff_Info_Changed( pV, ODL+k );
  }
}
void Diff::Do_p_or_P_st_fn_IntermediatLine( const unsigned k
                                          , const unsigned ODL
                                          , const unsigned OVL
                                          , const bool     ON_DELETED )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned NUM_LINES = gl_pVis->reg.len();

  const unsigned NLL = gl_pVis->reg[ k ]->len(); // New line length
  const unsigned VL  = ViewLine( pV, ODL+k );    // View line

  if( ON_DELETED )
  {
    // In FileBuf: Put reg on line below:
    pfb->InsertLine( VL+1, *( gl_pVis->reg[k] ) );

    Patch_Diff_Info_Inserted( pV, ODL+k, false );
  }
  else {
    MoveInBounds();
    const unsigned LL = pfb->LineLen( VL );

    for( unsigned i=0; i<NLL; i++ )
    {
      char C = gl_pVis->reg[k]->get(i);

      pfb->InsertChar( VL, i, C );
    }
    Patch_Diff_Info_Changed( pV, ODL+k );

    // Move rest of first line onto new line below
    if( 1 < NUM_LINES && 0 < LL )
    {
      pfb->InsertLine( VL+1 );
      for( unsigned i=0; i<LL; i++ )
      {
        char C = pfb->RemoveChar( VL, NLL );
        pfb->PushChar( VL+1, C );
      }
      Patch_Diff_Info_Inserted( pV, ODL+k+1, false ); //< Always false since we are starting on line below
    }
  }
}

void Diff::Do_p_block()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current

  const unsigned DL = CrsLine();          // Diff line
  const unsigned CP = CrsChar();          // Cursor position
  const unsigned VL = ViewLine( pV, DL ); // View line
  const bool ON_DELETED = DT_DELETED == cDI_List[ DL ].diff_type;
  const unsigned LL = ON_DELETED ? 0 : pfb->LineLen( VL ); // Line length
  const unsigned ISP = 0<CP ? CP+1    // Insert position
                     : ( 0<LL ? 1:0 );// If at beginning of line,
                                      // and LL is zero insert at 0,
                                      // else insert at 1
  const unsigned N_REG_LINES = gl_pVis->reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    if( VL+k < pfb->NumLines()
     && DT_DELETED != cDI_List[ DL+k ].diff_type )
    {
      Do_p_block_Change_Line( k, DL, VL, ISP );
    }
    else {
      Do_p_block_Insert_Line( k, DL, 0<VL?VL+1:0, ISP );
    }
  }
  Update();
}
void Diff::Do_p_block_Insert_Line( const unsigned k
                                 , const unsigned DL
                                 , const unsigned VL
                                 , const unsigned ISP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  pfb->InsertLine( VL+k );

  const unsigned LL_k = pfb->LineLen( VL+k );

  if( LL_k < ISP )
  {
    // Fill in line with white space up to ISP:
    for( unsigned i=0; i<(ISP-LL_k); i++ )
    {
      // Insert at end of line so undo will be atomic:
      const unsigned NLL = pfb->LineLen( VL+k ); // New line length
      pfb->InsertChar( VL+k, NLL, ' ' );
    }
  }
  Line* reg_line = gl_pVis->reg[k];
  const unsigned RLL = reg_line->len();

  for( unsigned i=0; i<RLL; i++ )
  {
    char C = reg_line->get(i);

    pfb->InsertChar( VL+k, ISP+i, C );
  }
  const bool ODVL0 = On_Deleted_View_Line_Zero( DL+k );

  Patch_Diff_Info_Inserted( pV, DL+k, ODVL0 );
}
void Diff::Do_p_block_Change_Line( const unsigned k
                                 , const unsigned DL
                                 , const unsigned VL
                                 , const unsigned ISP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  const unsigned LL_k = pfb->LineLen( VL+k );

  if( LL_k < ISP )
  {
    // Fill in line with white space up to ISP:
    for( unsigned i=0; i<(ISP-LL_k); i++ )
    {
      // Insert at end of line so undo will be atomic:
      const unsigned NLL = pfb->LineLen( VL+k ); // New line length
      pfb->InsertChar( VL+k, NLL, ' ' );
    }
  }
  Line* reg_line = gl_pVis->reg[k];
  const unsigned RLL = reg_line->len();

  for( unsigned i=0; i<RLL; i++ )
  {
    char C = reg_line->get(i);

    pfb->InsertChar( VL+k, ISP+i, C );
  }
  Patch_Diff_Info_Changed( pV, DL+k );
}

void Diff::Do_P()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( PM_ST_FN == gl_pVis->paste_mode ) return Do_p_or_P_st_fn( PP_Before );
  else if( PM_BLOCK == gl_pVis->paste_mode ) return Do_P_block();
  else /*( PM_LINE  == gl_pVis->paste_mode*/ return Do_P_line();
}

void Diff::Do_P_line()
{
  const int DL = CrsLine(); // Diff line

  // Move to line above, and then do 'p':
  if( 0<DL ) GoToCrsPos_NoWrite( DL-1, CrsChar() );

  Do_p_line();
}

void Diff::Do_P_block()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current

  const unsigned DL = CrsLine();          // Diff line
  const unsigned CP = CrsChar();          // Cursor position
  const unsigned VL = ViewLine( pV, DL ); // View line
  const bool     ON_DELETED = DT_DELETED == cDI_List[ DL ].diff_type;
  const unsigned LL = ON_DELETED ? 0 : pfb->LineLen( VL ); // Line length
  const unsigned ISP = 0<CP ? CP : 0;     // Insert position

  const unsigned N_REG_LINES = gl_pVis->reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    if( VL+k < pfb->NumLines()
     && DT_DELETED != cDI_List[ DL+k ].diff_type )
    {
      Do_p_block_Change_Line( k, DL, VL, ISP );
    }
    else {
      Do_p_block_Insert_Line( k, DL, 0<VL?VL+1:0, ISP );
    }
  }
  Update();
}

bool Diff::Do_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  inVisualBlock = false;

  return Do_visualMode();
}

bool Diff::Do_V()
{
  Trace trace( __PRETTY_FUNCTION__ );

  inVisualBlock = true;

  return Do_visualMode();
}

bool Diff::Do_visualMode()
{
  Trace trace( __PRETTY_FUNCTION__ );

  MoveInBounds();
  inVisualMode = true;
  m_undo_v     = true;
  DisplayBanner();

  v_st_line = CrsLine();  v_fn_line = v_st_line;
  v_st_char = CrsChar();  v_fn_char = v_st_char;

  // Write current byte in visual:
  Replace_Crs_Char( S_RV_VISUAL );

  while( inVisualMode )
  {
    const char C=gl_pKey->In();

    if     ( C == 'l' ) GoRight();
    else if( C == 'h' ) GoLeft();
    else if( C == 'j' ) GoDown();
    else if( C == 'k' ) GoUp();
    else if( C == 'H' ) GoToTopLineInView();
    else if( C == 'L' ) GoToBotLineInView();
    else if( C == 'M' ) GoToMidLineInView();
    else if( C == '0' ) GoToBegOfLine();
    else if( C == '$' ) GoToEndOfLine();
    else if( C == 'g' ) Do_v_Handle_g();
    else if( C == 'G' ) GoToEndOfFile();
    else if( C == 'F' ) PageDown_v();
    else if( C == 'B' ) PageUp_v();
    else if( C == 'b' ) GoToPrevWord();
    else if( C == 'w' ) GoToNextWord();
    else if( C == 'e' ) GoToEndOfWord();
    else if( C == '%' ) GoToOppositeBracket();
    else if( C == 'z' ) gl_pVis->Handle_z();
    else if( C == 'f' ) gl_pVis->Handle_f();
    else if( C == ';' ) gl_pVis->Handle_SemiColon();
    else if( C == 'y' ) { Do_y_v(); goto EXIT_VISUAL; }
    else if( C == 'Y' ) { Do_Y_v(); goto EXIT_VISUAL; }
    else if( C == 'x'
          || C == 'd' ) { Do_x_v();     return true; }
    else if( C == 'D' ) { Do_D_v();     return true; }
    else if( C == 's' ) { Do_s_v();     return true; }
    else if( C == '~' ) { Do_Tilda_v(); return true; }
    else if( C == ESC ) { goto EXIT_VISUAL; }
  }
  return false;

EXIT_VISUAL:
  inVisualMode = false;
  Undo_v();
  Remove_Banner();
  return false;
}

void Diff::Undo_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_undo_v )
  {
    Update();
  }
}

void Diff::Replace_Crs_Char( Style style )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View* pV  = gl_pVis->CV();

  // Convert CL, which is diff line, to view line:
  const unsigned CLv = ViewLine( pV, CrsLine() );

  const unsigned LL = pV->pfb->LineLen( CLv ); // Line length
  if( LL )
  {
    int byte = pV->pfb->Get( CLv, CrsChar() );

    Console::Set( Row_Win_2_GL( pV, CrsLine()-topLine )
                , Col_Win_2_GL( pV, CrsChar()-leftChar )
                , byte, style );
  }
}

void Diff::Do_v_Handle_g()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC2 = gl_pKey->In();

  if     ( CC2 == 'g' ) GoToTopOfFile();
  else if( CC2 == '0' ) GoToStartOfRow();
  else if( CC2 == '$' ) GoToEndOfRow();
  else if( CC2 == 'p' ) Do_v_Handle_gp();
}

void Diff::Do_v_Handle_gp()
{
  if( v_st_line == v_fn_line )
  {
    View*    pV  = gl_pVis->CV();
    FileBuf* pfb = pV->pfb;

    Swap_Visual_Block_If_Needed();

    const int VL = ViewLine( pV, v_st_line );

    String pattern;

    for( int P = v_st_char; P<=v_fn_char; P++ )
    {
      pattern.push( pfb->Get( v_st_line, P  ) );
    }
    gl_pVis->Handle_Slash_GotPattern( pattern, false );

    inVisualMode = false;
  }
}

// This one works better when IN visual mode:
void Diff::PageDown_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();

  if( 0<NUM_LINES )
  {
    const unsigned OCLd = CrsLine(); // Old cursor line diff

    unsigned NCLd = OCLd + WorkingRows( gl_pVis->CV() ) - 1; // New cursor line diff

    // Dont let cursor go past the end of the file:
    if( NUM_LINES-1 < NCLd ) NCLd = NUM_LINES-1;

    GoToCrsPos_Write( NCLd, 0 );
  }
}
// This one works better when IN visual mode:
void Diff::PageUp_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = NumLines();

  if( 0<NUM_LINES )
  {
    const unsigned OCLd = CrsLine(); // Old cursor line diff

    int NCLd = OCLd - WorkingRows( gl_pVis->CV() ) + 1; // New cursor line diff

    // Check for underflow:
    if( NCLd < 0 ) NCLd = 0;

    GoToCrsPos_Write( NCLd, 0 );
  }
}

void Diff::Do_R()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  pV->inReplaceMode = true;
  DisplayBanner();

  if( 0 == pfb->NumLines() ) pfb->PushLine();

  unsigned count = 0;
  for( char C=gl_pKey->In(); C != ESC; C=gl_pKey->In() )
  {
    if( BS == C || DEL == C )
    {
      if( 0<count )
      {
        InsertBackspace();
        count--;
      }
    }
    else if( IsEndOfLineDelim( C ) )
    {
      ReplaceAddReturn();
      count++;
    }
    else {
      ReplaceAddChar( C );
      count++;
    }
  }
  Remove_Banner();
  pV->inReplaceMode = false;

  // Move cursor back one space:
  if( 0<crsCol ) crsCol--;  // Move cursor back one space.

  Update();
}

void Diff::ReplaceAddReturn()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;
  // The lines in fb do not end with '\n's.
  // When the file is written, '\n's are added to the ends of the lines.
  Line new_line(__FILE__, __LINE__);
  const unsigned ODL = CrsLine();
  const unsigned OVL = ViewLine( pV, ODL ); // View line number
  const unsigned OLL = pfb->LineLen( OVL );
  const unsigned OCP = CrsChar();

  for( unsigned k=OCP; k<OLL; k++ )
  {
    const uint8_t C = pfb->RemoveChar( OVL, OCP );
    bool ok = new_line.push(__FILE__,__LINE__, C );
    ASSERT( __LINE__, ok, "ok" );
  }
  // Truncate the rest of the old line:
  // Add the new line:
  const unsigned new_line_num = OVL+1;
  pfb->InsertLine( new_line_num, new_line );

  GoToCrsPos_NoWrite( ODL+1, 0 );

  Patch_Diff_Info_Changed( pV, ODL );
  Patch_Diff_Info_Inserted( pV, ODL+1 );
  Update();
}

void Diff::ReplaceAddChar( const char C )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  if( pfb->NumLines()==0 ) pfb->PushLine();

  const unsigned DL = CrsLine();
  const unsigned VL = ViewLine( pV, DL ); // View line number

  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current
  Diff_Info& cDI = cDI_List[ DL ];

  const bool ON_DELETED = DT_DELETED == cDI.diff_type;
  if( ON_DELETED )
  {
    ReplaceAddChar_ON_DELETED( C, DL, VL );
  }
  else {
    const unsigned CP = CrsChar();
    const unsigned LL = pfb->LineLen( VL );
    const unsigned EOL = 0<LL ? LL-1 : 0;

    if( EOL < CP )
    {
      // Extend line out to where cursor is:
      for( unsigned k=LL; k<CP; k++ ) pfb->PushChar( VL, ' ' );
    }
    // Put char back in file buffer
    const bool continue_last_update = false;
    if( CP < LL ) pfb->Set( VL, CP, C, continue_last_update );
    else {
      pfb->PushChar( VL, C );
    }
    Patch_Diff_Info_Changed( pV, DL );
  }
  if( crsCol < WorkingCols( pV )-1 )
  {
    crsCol++;
  }
  else {
    leftChar++;
  }
  Update();
}

void Diff::ReplaceAddChar_ON_DELETED( const char C
                                    , const unsigned DL
                                    , const unsigned VL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current

  const bool ODVL0 = On_Deleted_View_Line_Zero( DL );

  Line* nlp = gl_pVis->BorrowLine(__FILE__,__LINE__);
  nlp->push( __FILE__,__LINE__, C );
  pfb->InsertLine( ODVL0 ? VL : VL+1, nlp );
  Patch_Diff_Info_Inserted( pV, DL, ODVL0 );
}

void Diff::Do_Tilda()
{
  Trace trace( __PRETTY_FUNCTION__ );

  View*    pV  = gl_pVis->CV();
  FileBuf* pfb = pV->pfb;

  if( 0==pfb->NumLines() ) return;

  const unsigned DL = CrsLine();          // Diff line
  const unsigned VL = ViewLine( pV, DL ); // View line
  const unsigned CP = CrsChar();          // Cursor position
  const unsigned LL = pfb->LineLen( VL );

  if( 0<LL && CP<LL )
  {
    char C = pfb->Get( VL, CP );
    bool changed = false;
    if     ( isupper( C ) ) { C = tolower( C ); changed = true; }
    else if( islower( C ) ) { C = toupper( C ); changed = true; }

    const bool CONT_LAST_UPDATE = true;
    if( crsCol < Min( LL-1, WorkingCols( pV )-1 ) )
    {
      if( changed ) pfb->Set( VL, CP, C, CONT_LAST_UPDATE );
      // Need to move cursor right:
      crsCol++;
    }
    else if( RightChar( pV ) < LL-1 )
    {
      // Need to scroll window right:
      if( changed ) pfb->Set( VL, CP, C, CONT_LAST_UPDATE );
      leftChar++;
    }
    else // RightChar() == LL-1
    {
      // At end of line so cant move or scroll right:
      if( changed ) pfb->Set( VL, CP, C, CONT_LAST_UPDATE );
    }
    if( changed ) Patch_Diff_Info_Changed( pV, DL );
    Update();
  }
}

bool Diff::On_Deleted_View_Line_Zero( const unsigned DL )
{
  bool ODVL0 = false; // On Deleted View Line Zero

  View* pV = gl_pVis->CV();
  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current
  Diff_Info& cDI = cDI_List[ DL ];

  if( DT_DELETED == cDI.diff_type )
  {
    ODVL0 = true;

    for( unsigned k=0; ODVL0 && k<DL; k++ )
    {
      if( DT_DELETED != cDI_List[ k ].diff_type ) ODVL0 = false;
    }
  }
  return ODVL0;
}

//| Action | ThisSide | OtherSide | Action
//--------------------------------------------------------------------------------
//| Change | SAME     | SAME      | Change this side and other side to CHANGED
//|        | CHANGED  | CHANGED   | Compare sides, if same change both to SAME, else leave both CHANGED
//|        | INSERTED | DELETED   | Dont change anything
void Diff::Patch_Diff_Info_Changed( View* pV, const unsigned DPL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current
  Array_t<Diff_Info>& oDI_List = (pV == pvS) ? DI_List_L : DI_List_S; // Other

  Diff_Info& cDI = cDI_List [ DPL ]; // Current Diff_Info
  Diff_Info& oDI = oDI_List [ DPL ]; // Other   Diff_Info

  Diff_Info& sDI = DI_List_S[ DPL ]; // Short   Diff_Info
  Diff_Info& lDI = DI_List_L[ DPL ]; // Long    Diff_Info

  Line* ls = pfS->GetLineP( sDI.line_num ); // Line from short view
  Line* ll = pfL->GetLineP( lDI.line_num ); // Line from long  view

  if( DT_SAME == cDI.diff_type )
  {
    if( !sDI.pLineInfo ) sDI.pLineInfo = Borrow_LineInfo(__FILE__,__LINE__);
    if( !lDI.pLineInfo ) lDI.pLineInfo = Borrow_LineInfo(__FILE__,__LINE__);

    Compare_Lines( ls, sDI.pLineInfo, ll, lDI.pLineInfo );

    cDI.diff_type = DT_CHANGED;
    oDI.diff_type = DT_CHANGED;
  }
  else if( DT_CHANGED == cDI.diff_type )
  {
    if( ls->chksum() == ll->chksum() ) // Lines are now equal
    {
      cDI.diff_type = DT_SAME;
      oDI.diff_type = DT_SAME;

      Return_LineInfo( cDI.pLineInfo ); cDI.pLineInfo = 0;
      Return_LineInfo( oDI.pLineInfo ); oDI.pLineInfo = 0;
    }
    else { // Lines are still different
      if( !sDI.pLineInfo ) sDI.pLineInfo = Borrow_LineInfo(__FILE__,__LINE__);
      if( !lDI.pLineInfo ) lDI.pLineInfo = Borrow_LineInfo(__FILE__,__LINE__);

      Compare_Lines( ls, sDI.pLineInfo, ll, lDI.pLineInfo );
    }
  }
}

//| Action | ThisSide | OtherSide | Action
//--------------------------------------------------------------------------------
//| Insert | DELETED  | INSERTED  | Compare sides, if same set both to SAME, else set both to CHANGED
//|        | -------- | ANY OTHER | Add line to both sides, and set this side to INSERTED and other side to DELETED
void Diff::Patch_Diff_Info_Inserted( View* pV, const unsigned DPL, const bool ON_DELETED_VIEW_LINE_ZERO )
{
  Trace trace( __PRETTY_FUNCTION__ );
  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current
  Array_t<Diff_Info>& oDI_List = (pV == pvS) ? DI_List_L : DI_List_S; // Other

  const unsigned DI_Len = cDI_List.len();

  if( DI_Len <= DPL )
  {
    // Inserting onto end of Diff_Info lists:
    Diff_Info dic = { DT_INSERTED, cDI_List[ DI_Len-1 ].line_num+1 };
    Diff_Info dio = { DT_DELETED , oDI_List[ DI_Len-1 ].line_num   };

    bool ok1 = cDI_List.insert(__FILE__,__LINE__, DI_Len, dic ); ASSERT( __LINE__, ok1, "ok1" );
    bool ok2 = oDI_List.insert(__FILE__,__LINE__, DI_Len, dio ); ASSERT( __LINE__, ok2, "ok2" );
  }
  else { // Inserting into beginning or middle of Diff_Info lists:
    Diff_Info& cDI = cDI_List[ DPL ];
    Diff_Info& oDI = oDI_List[ DPL ];

    if( DT_DELETED == cDI.diff_type )
    {
      Patch_Diff_Info_Inserted_Inc( DPL, ON_DELETED_VIEW_LINE_ZERO, cDI_List );

      Diff_Info& sDI = DI_List_S[ DPL ]; // Short   Diff_Info
      Diff_Info& lDI = DI_List_L[ DPL ]; // Long    Diff_Info

      Line* ls = pfS->GetLineP( sDI.line_num ); // Line from short view
      Line* ll = pfL->GetLineP( lDI.line_num ); // Line from long  view

      if( ls->chksum() == ll->chksum() ) // Lines are now equal
      {
        cDI.diff_type = DT_SAME;
        oDI.diff_type = DT_SAME;
      }
      else { // Lines are different
        if( !sDI.pLineInfo ) sDI.pLineInfo = Borrow_LineInfo(__FILE__,__LINE__);
        if( !lDI.pLineInfo ) lDI.pLineInfo = Borrow_LineInfo(__FILE__,__LINE__);

        Compare_Lines( ls, sDI.pLineInfo, ll, lDI.pLineInfo );

        cDI.diff_type = DT_CHANGED;
        oDI.diff_type = DT_CHANGED;
      }
    }
    else {
      unsigned dio_line = DT_DELETED==oDI.diff_type
                        ? oDI.line_num                            // Use current  line number
                        : (0<oDI.line_num ? oDI.line_num-1 : 0 ); // Use previous line number
      // Current side gets current  line number
      Diff_Info dic = { DT_INSERTED, cDI.line_num };
      Diff_Info dio = { DT_DELETED , dio_line };

      bool ok1 = cDI_List.insert(__FILE__,__LINE__, DPL, dic ); ASSERT( __LINE__, ok1, "ok1" );
      bool ok2 = oDI_List.insert(__FILE__,__LINE__, DPL, dio ); ASSERT( __LINE__, ok2, "ok2" );

      // Added a view line, so increment all following view line numbers:
      for( unsigned k=DPL+1; k<cDI_List.len(); k++ )
      {
        cDI_List[ k ].line_num++;
      }
    }
  }
}
void Diff::Patch_Diff_Info_Inserted_Inc( const unsigned DPL
                                       , const bool ON_DELETED_VIEW_LINE_ZERO
                                       , Array_t<Diff_Info>& cDI_List )
{
  // If started inserting into empty first line in file, dont increment
  // Diff_Info line_num, because DELETED first line starts at zero:
  unsigned inc_st = DPL;
  if( ON_DELETED_VIEW_LINE_ZERO ) {
    // If there are DT_DELETED lines directly below where
    // we inserted a line, decrement their Diff_Info.line_num's
    // because they were incremented in Patch_Diff_Info_Inserted()
    // and they should not be incremented here:
    for( unsigned k=DPL+1; k<cDI_List.len(); k++ )
    {
      Diff_Info& di = cDI_List[ k ];
      if( DT_DELETED == di.diff_type )
      {
        inc_st = k+1;
      }
      else break;
    }
  }
  // Added a view line, so increment all following view line numbers:
  for( unsigned k=inc_st; k<cDI_List.len(); k++ )
  {
    cDI_List[ k ].line_num++;
  }
}

//| Action | ThisSide | OtherSide | Action
//--------------------------------------------------------------------------------
//| Delete | SAME     | SAME      | Change this side to DELETED and other side to INSERTED
//|        | CHANGED  | CHANGED   | Change this side to DELETED and other side to INSERTED
//|        | INSERTED | DELETED   | Remove line on both sides
//|        | DELETED  | --------- | Do nothing
void Diff::Patch_Diff_Info_Deleted( View* pV, const unsigned DPL )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Array_t<Diff_Info>& cDI_List = (pV == pvS) ? DI_List_S : DI_List_L; // Current
  Array_t<Diff_Info>& oDI_List = (pV == pvS) ? DI_List_L : DI_List_S; // Other

  Diff_Info& cDI = cDI_List[ DPL ];
  Diff_Info& oDI = oDI_List[ DPL ];

  if( DT_SAME == cDI.diff_type )
  {
    cDI.diff_type = DT_DELETED;
    oDI.diff_type = DT_INSERTED;
  }
  else if( DT_CHANGED == cDI.diff_type )
  {
    cDI.diff_type = DT_DELETED;
    oDI.diff_type = DT_INSERTED;

    Return_LineInfo( cDI.pLineInfo ); cDI.pLineInfo = 0;
    Return_LineInfo( oDI.pLineInfo ); oDI.pLineInfo = 0;
  }
  else if( DT_INSERTED == cDI.diff_type )
  {
    bool ok1 = cDI_List.remove( DPL );
    bool ok2 = oDI_List.remove( DPL );

    ASSERT( __LINE__, ok1, "ok1" );
    ASSERT( __LINE__, ok2, "ok2" );
  }
  // Removed a view line, so decrement current and all following view line numbers:
  for( unsigned k=DPL; k<cDI_List.len(); k++ )
  {
    cDI_List[ k ].line_num--;
  }
}

void Diff::Swap_Visual_Block_If_Needed()
{
  if( v_fn_line < v_st_line ) Swap( v_st_line, v_fn_line );
  if( v_fn_char < v_st_char ) Swap( v_st_char, v_fn_char );
}

void Diff::Print_L()
{
  Trace trace( __PRETTY_FUNCTION__ );
  char fname_L[512];
  sprintf( fname_L, "%s.diff", pfL->file_name.c_str() );
  FILE* fpL = fopen( fname_L, "wb" );

  for( unsigned k=0; fpL && k<DI_List_L.len(); k++ )
  {
    Diff_Info di = DI_List_L[k];
    const unsigned LN = di.line_num;
    if( DT_UNKN0WN  == di.diff_type )
    {
      fprintf( fpL, "U:-----------------------------");
    }
    else if( DT_SAME     == di.diff_type )
    {
      fprintf( fpL, "S:%u:", LN ); 
      const unsigned LL = pfL->LineLen( LN );
      for( unsigned i=0; i<LL; i++ ) fprintf( fpL, "%c", pfL->Get( LN, i ) );
    }
    else if( DT_CHANGED  == di.diff_type )
    {
      fprintf( fpL, "C:%u:", LN ); 
      const unsigned LL = pfL->LineLen( LN );
      for( unsigned i=0; i<LL; i++ ) fprintf( fpL, "%c", pfL->Get( LN, i ) );
    }
    else if( DT_INSERTED == di.diff_type )
    {
      fprintf( fpL, "I:%u:", LN ); 
      const unsigned LL = pfL->LineLen( LN );
      for( unsigned i=0; i<LL; i++ ) fprintf( fpL, "%c", pfL->Get( LN, i ) );
    }
    else if( DT_DELETED  == di.diff_type )
    {
      fprintf( fpL, "D:-----------------------------");
    }
    fprintf( fpL, "\n" );
  }
  fclose( fpL );
}

void Diff::Print_S()
{
  Trace trace( __PRETTY_FUNCTION__ );
  char fname_S[512];
  sprintf( fname_S, "%s.diff", pfS->file_name.c_str() );
  FILE* fpS = fopen( fname_S, "wb" );

  for( unsigned k=0; fpS && k<DI_List_L.len(); k++ )
  {
    Diff_Info di = DI_List_S[k];
    const unsigned LN = di.line_num;
    if( DT_UNKN0WN  == di.diff_type )
    {
      fprintf( fpS, "U:-----------------------------");
    }
    else if( DT_SAME     == di.diff_type )
    {
      fprintf( fpS, "S:%u:", LN ); 
      const unsigned LL = pfS->LineLen( LN );
      for( unsigned i=0; i<LL; i++ ) fprintf( fpS, "%c", pfS->Get( LN, i ) );
    }
    else if( DT_CHANGED  == di.diff_type )
    {
      fprintf( fpS, "C:%u:", LN ); 
      const unsigned LL = pfS->LineLen( LN );
      for( unsigned i=0; i<LL; i++ ) fprintf( fpS, "%c", pfS->Get( LN, i ) );
    }
    else if( DT_INSERTED == di.diff_type )
    {
      fprintf( fpS, "I:%u:", LN ); 
      const unsigned LL = pfS->LineLen( LN );
      for( unsigned i=0; i<LL; i++ ) fprintf( fpS, "%c", pfS->Get( LN, i ) );
    }
    else if( DT_DELETED  == di.diff_type )
    {
      fprintf( fpS, "D:-----------------------------");
    }
    fprintf( fpS, "\n" );
  }
  fclose( fpS );
}

void SameArea::Clear()
{
  ln_s   = 0;
  ln_l   = 0;
  nlines = 0;
  nbytes = 0;
}

void SameArea::Init( unsigned _ln_s, unsigned _ln_l, unsigned _nbytes )
{
  ln_s   = _ln_s;
  ln_l   = _ln_l;
  nlines = 1;
  nbytes = _nbytes;
}

void SameArea::Inc( unsigned _nbytes )
{
  nlines += 1;
  nbytes += _nbytes;
}

void SameArea::Set( const SameArea& a )
{
  ln_s   = a.ln_s;
  ln_l   = a.ln_l;
  nlines = a.nlines;
  nbytes = a.nbytes;
}

void SameLineSec::Init( unsigned _ch_s, unsigned _ch_l )
{
  ch_s   = _ch_s;
  ch_l   = _ch_l;
  nbytes = 1;
}

void SameLineSec::Set( const SameLineSec& a )
{
  ch_s   = a.ch_s;
  ch_l   = a.ch_l;
  nbytes = a.nbytes;
}

LineInfo* Diff::Borrow_LineInfo( const char* _FILE_, const unsigned _LINE_ )
{
  LineInfo* lip = 0;

  if( line_info_cache.len() )
  {
    bool ok = line_info_cache.pop( lip );
    ASSERT( __LINE__, ok, "ok" );

    lip->clear();
  }
  else {
    lip = new(_FILE_,_LINE_) LineInfo(__FILE__, __LINE__);
  }
  return lip;
}

void Diff::Return_LineInfo( LineInfo* lip )
{
  if( lip ) line_info_cache.push(__FILE__,__LINE__, lip );
}

void Diff::Clear_SimiList()
{
  // Return all the previously allocated LineInfo's:
  for( unsigned k=0; k<simiList.len(); k++ )
  {
    SimLines& siml = simiList[k];

    if( siml.li_s ) { Return_LineInfo( siml.li_s ); siml.li_s = 0; }
    if( siml.li_l ) { Return_LineInfo( siml.li_l ); siml.li_l = 0; }
  }
  simiList.clear();
}

void Diff::Clear_DI_List( Array_t<Diff_Info>& DI_List )
{
  for( unsigned k=0; k<DI_List.len(); k++ )
  {
    Diff_Info& di = DI_List[k];

    if( di.pLineInfo )
    {
      // Return all the previously allocated LineInfo's:
      Return_LineInfo( di.pLineInfo ); di.pLineInfo = 0;
    }
  }
  DI_List.clear();
}

void Diff::Insert_DI_List( const Diff_Info di
                         , Array_t<Diff_Info>& DI_List )
{
  DI_List.push(__FILE__,__LINE__, di );
}

bool Diff::Update_Status_Lines()
{
  bool updated_a_sts_line = false;

  if( sts_line_needs_update )
  {
    // Update status line:
    PrintStsLine( pvS );
    PrintStsLine( pvL );
    Console::Update();

    sts_line_needs_update = false;
    updated_a_sts_line = true;
  }
  return updated_a_sts_line;
}

