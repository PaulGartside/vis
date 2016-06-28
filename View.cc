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
#include <string.h>
#include <unistd.h>    // write, ioctl[unix], read, chdir

#include "String.hh"
#include "Console.hh"
#include "MemLog.hh"
#include "FileBuf.hh"
#include "Utilities.hh"
#include "Key.hh"
#include "Vis.hh"
#include "View.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

const unsigned BE_FILE  = 0;   // Buffer editor file
const unsigned CMD_FILE = 4;   // Command Shell file

static const unsigned top___border = 1;
static const unsigned bottomborder = 1;
static const unsigned left__border = 1;
static const unsigned right_border = 1;

class View::Imp
{
public:
  Vis&       m_vis;
  Key&       m_key;
  FileBuf&   m_fb;
  View&      m_view;
  LinesList& m_reg;

  unsigned m_nCols;     // number of rows in buffer view
  unsigned m_nRows;     // number of columns in buffer view
  unsigned m_x;         // Top left x-position of buffer view in parent window
  unsigned m_y;         // Top left y-position of buffer view in parent window
  unsigned m_topLine;   // top  of buffer view line number.
  unsigned m_leftChar;  // left of buffer view character number.
  unsigned m_crsRow;    // cursor row    in buffer view. 0 <= m_crsRow < WorkingRows().
  unsigned m_crsCol;    // cursor column in buffer view. 0 <= m_crsCol < WorkingCols().
  Tile_Pos m_tile_pos;

  unsigned v_st_line;  // Visual start line number
  unsigned v_st_char;  // Visual start char number on line
  unsigned v_fn_line;  // Visual ending line number
  unsigned v_fn_char;  // Visual ending char number on line

  bool m_inInsertMode; // true if in insert  mode, else false
  bool m_inReplaceMode;
  bool m_inVisualMode;
  bool m_inVisualBlock;

  bool m_sts_line_needs_update;
  bool m_us_change_sts; // un-saved change status, true if '+", false if ' '

public:
  Imp( Vis& vis, Key& key, FileBuf& fb, View& view, LinesList& reg );
  ~Imp();

  unsigned WinCols() const { return m_nCols; }
  unsigned WinRows() const { return m_nRows; }
  unsigned X() const { return m_x; }
  unsigned Y() const { return m_y; }

  unsigned GetTopLine () const { return m_topLine; }
  unsigned GetLeftChar() const { return m_leftChar; }
  unsigned GetCrsRow  () const { return m_crsRow; }
  unsigned GetCrsCol  () const { return m_crsCol; }

  void SetTopLine ( const unsigned val ) { m_topLine  = val; }
  void SetLeftChar( const unsigned val ) { m_leftChar = val; }
  void SetCrsRow  ( const unsigned val ) { m_crsRow   = val; }
  void SetCrsCol  ( const unsigned val ) { m_crsCol   = val; }

  unsigned WorkingRows() const { return WinRows()-3-top___border-bottomborder; }
  unsigned WorkingCols() const { return WinCols()-  left__border-right_border; }
  unsigned CrsLine  () const { return m_topLine  + m_crsRow; }
  unsigned BotLine  () const { return m_topLine  + WorkingRows()-1; }
  unsigned CrsChar  () const { return m_leftChar + m_crsCol; }
  unsigned RightChar() const { return m_leftChar + WorkingCols()-1; }

  // Translates zero based working view row to zero based global row
  unsigned Row_Win_2_GL( const unsigned win_row ) const
  {
    return m_y + top___border + win_row;
  }
  // Translates zero based working view column to zero based global column
  unsigned Col_Win_2_GL( const unsigned win_col ) const
  {
    return m_x + left__border + win_col;
  }
  // Translates zero based file line number to zero based global row
  unsigned Line_2_GL( const unsigned file_line ) const
  {
    return m_y + top___border + file_line - m_topLine;
  }
  // Translates zero based file line char position to zero based global column
  unsigned Char_2_GL( const unsigned line_char ) const
  {
    return m_x + left__border + line_char - m_leftChar;
  }

  unsigned Sts__Line_Row() const
  {
    return Row_Win_2_GL( WorkingRows() );
  }
  unsigned File_Line_Row() const
  {
    return Row_Win_2_GL( WorkingRows() + 1 );
  }

  unsigned Cmd__Line_Row() const
  {
    return Row_Win_2_GL( WorkingRows() + 2 );
  }

  Tile_Pos GetTilePos() const { return m_tile_pos; }
  void     SetTilePos( const Tile_Pos tp )
  {
    m_tile_pos = tp;

    SetViewPos();
  }
  void SetViewPos();

  bool GetInsertMode() const { return m_inInsertMode; }
  void SetInsertMode( const bool val ) { m_inInsertMode = val; }
  bool GetReplaceMode() const { return m_inReplaceMode; }
  void SetReplaceMode( const bool val ) { m_inReplaceMode = val; }

  void GoToBegOfLine();
  void GoToEndOfLine();
  void GoToBegOfNextLine();
  void GoToTopLineInView();
  void GoToMidLineInView();
  void GoToBotLineInView();
  void GoToEndOfFile();
  void GoToPrevWord();
  void GoToNextWord();
  void GoToEndOfWord();
  void GoToLine( const unsigned user_line_num );
  void GoToStartOfRow();
  void GoToEndOfRow();
  void GoToTopOfFile();
  void GoToOppositeBracket();
  void GoToLeftSquigglyBracket();
  void GoToRightSquigglyBracket();
  void GoToCrsPos_NoWrite( const unsigned ncp_crsLine
                         , const unsigned ncp_crsChar );
  void GoToCrsPos_Write( const unsigned ncp_crsLine
                       , const unsigned ncp_crsChar );
  bool GoToFile_GetFileName( String& fname );
  void GoToCmdLineClear( const char* S );

  void GoUp();
  void GoDown();
  void GoLeft();
  void GoRight();
  void PageDown();
  void PageUp();

  bool MoveInBounds();
  void MoveCurrLineToTop();
  void MoveCurrLineCenter();
  void MoveCurrLineToBottom();

  void Do_i();
  bool Do_v();
  bool Do_V();
  void Do_a();
  void Do_A();
  void Do_o();
  void Do_O();
  void Do_x();
  void Do_s();
  void Do_cw();
  void Do_D();
  void Do_f( const char FAST_CHAR );
  void Do_n();
  void Do_N();
  void Do_dd();
  int  Do_dw();
  void Do_yy();
  void Do_yw();
  void Do_p();
  void Do_P();
  void Do_R();
  void Do_J();
  void Do_Tilda();
  void Do_u();
  void Do_U();

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
  void Print_Borders();
  void PrintStsLine();
  void PrintFileLine();
  void PrintCmdLine();
  void PrintWorkingView();
  void PrintWorkingView_Set( const unsigned LL
                           , const unsigned G_ROW
                           , const unsigned col
                           , const unsigned i
                           , const unsigned byte
                           , const Style    s );
  void PrintCursor();
  void DisplayMapping();

  bool GetStsLineNeedsUpdate() const;
  bool GetUnSavedChangeSts() const;
  void SetStsLineNeedsUpdate( const bool val );
  void SetUnSavedChangeSts( const bool val );

  String Do_Star_GetNewPattern();
  void   PrintPatterns( const bool HIGHLIGHT );

  bool Has_Context();
  void Set_Context( View& vr );

  bool GoToDir();

private:
  void TilePos_2_x();
  void TilePos_2_y();
  void TilePos_2_nRows();
  void TilePos_2_nCols();

  bool GoToNextWord_GetPosition( CrsPos& ncp );
  bool GoToPrevWord_GetPosition( CrsPos& ncp );
  bool GoToEndOfWord_GetPosition( CrsPos& ncp );
  void GoToOppositeBracket_Forward( const char ST_C, const char FN_C );
  void GoToOppositeBracket_Backward( const char ST_C, const char FN_C );

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
  void PageDown_v();
  void PageUp_v();

  void Do_i_vb();
  void Do_a_vb();
  bool Do_visualMode();
  void Undo_v();
  void Do_v_Handle_g();
  void Do_v_Handle_gf();
  void Do_v_Handle_gp();

  bool Do_n_FindNextPattern( CrsPos& ncp );
  bool Do_N_FindPrevPattern( CrsPos& ncp );

  void Do_x_v();
  void Do_D_v();
  void Do_D_v_line();
  void Do_s_v();
  bool Do_dw_get_fn( const int st_line, const int st_char
                   , unsigned& fn_line, unsigned& fn_char );
  void Do_Tilda_v();
  void Do_Tilda_v_st_fn();
  void Do_Tilda_v_block();
  void Do_y_v();
  void Do_y_v_st_fn();
  void Do_y_v_block();
  void Do_Y_v();
  void Do_Y_v_st_fn();
  void Do_p_line();
  void Do_p_or_P_st_fn( Paste_Pos paste_pos );
  void Do_p_block();
  void Do_P_line();
  void Do_P_block();
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
  void Do_dd_BufferEditor( const unsigned ONL );
  void Do_dd_Normal( const unsigned ONL );

  void InsertBackspace_vb();
  void InsertAddChar_vb( const char c );
  void InsertAddChar( const char c );
  void InsertAddReturn();
  void InsertBackspace();
  void InsertBackspace_RmC( const unsigned OCL
                          , const unsigned OCP );
  void InsertBackspace_RmNL( const unsigned OCL );

  void ReplaceAddReturn();
  void ReplaceBackspace( Line* lp );
  void ReplaceAddChars( const char C );

  void UpdateLines( const unsigned st_line, const unsigned fn_line );
  void PrintLines( const unsigned st_line, const unsigned fn_line );
  void Print_Borders_Top   ( const Style S );
  void Print_Borders_Bottom( const Style S );
  void Print_Borders_Left  ( const Style S );
  void Print_Borders_Right ( const Style S );
  void PrintDo_n();
  void DisplayBanner();
  void Remove_Banner();
  void Replace_Crs_Char( Style style );

  Style Get_Style( const unsigned line, const unsigned pos );
  bool  RV_Style( const Style s ) const;
  Style RV_Style_2_NonRV( const Style RVS ) const;
};

View::Imp::Imp( Vis& vis, Key& key, FileBuf& fb, View& view, LinesList& reg )
  : m_vis( vis )
  , m_key( key )
  , m_fb( fb )
  , m_view( view )
  , m_reg( reg )
  , m_nCols( Console::Num_Cols() )
  , m_nRows( Console::Num_Rows() )
  , m_x( 0 )
  , m_y( 0 )
  , m_topLine( 0 )
  , m_leftChar( 0 )
  , m_crsRow( 0 )
  , m_crsCol( 0 )
  , m_tile_pos( TP_FULL )
  , v_st_line( 0 )
  , v_st_char( 0 )
  , v_fn_line( 0 )
  , v_fn_char( 0 )
  , m_inInsertMode( false )
  , m_inReplaceMode( false )
  , m_inVisualMode( false )
  , m_inVisualBlock( false )
  , m_sts_line_needs_update( false )
  , m_us_change_sts( false )
{
}

View::Imp::~Imp()
{
}

void View::Imp::SetViewPos()
{
  TilePos_2_x();
  TilePos_2_y();
  TilePos_2_nRows();
  TilePos_2_nCols();
}

void View::Imp::GoToBegOfLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m_fb.NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line

  GoToCrsPos_Write( OCL, 0 );
}

void View::Imp::GoToEndOfLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m_fb.NumLines() ) return;

  const unsigned LL = m_fb.LineLen( CrsLine() );

  const unsigned OCL = CrsLine(); // Old cursor line

  if( m_inVisualMode && m_inVisualBlock )
  {
    // In Visual Block, $ puts cursor at the position
    // of the end of the longest line in the block
    unsigned max_LL = LL;

    for( unsigned L=v_st_line; L<=v_fn_line; L++ )
    {
      max_LL = Max( max_LL, m_fb.LineLen( L ) );
    }
    GoToCrsPos_Write( OCL, max_LL ? max_LL-1 : 0 );
  }
  else {
    GoToCrsPos_Write( OCL, LL ? LL-1 : 0 );
  }
}

void View::Imp::GoToBegOfNextLine()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m_fb.NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  if( (NUM_LINES-1) <= OCL ) return; // On last line, so cant go down

  GoToCrsPos_Write( OCL+1, 0 );
}

void View::Imp::GoToTopLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToCrsPos_Write( m_topLine, 0 );
}

void View::Imp::GoToMidLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m_fb.NumLines();

  // Default: Last line in file is not in view
  unsigned NCL = m_topLine + WorkingRows()/2; // New cursor line

  if( NUM_LINES-1 < BotLine() )
  {
    // Last line in file above bottom of view
    NCL = m_topLine + (NUM_LINES-1 - m_topLine)/2;
  }
  GoToCrsPos_Write( NCL, 0 );
}

void View::Imp::GoToBotLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m_fb.NumLines();

  unsigned bottom_line_in_view = m_topLine + WorkingRows()-1;

  bottom_line_in_view = Min( NUM_LINES-1, bottom_line_in_view );

  GoToCrsPos_Write( bottom_line_in_view, 0 );
}

void View::Imp::GoToEndOfFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m_fb.NumLines();

  GoToCrsPos_Write( NUM_LINES-1, 0 );
}

void View::Imp::GoToPrevWord()
{
  Trace trace( __PRETTY_FUNCTION__ );

  CrsPos ncp = { 0, 0 };

  if( GoToPrevWord_GetPosition( ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

void View::Imp::GoToNextWord()
{
  Trace trace( __PRETTY_FUNCTION__ );
  CrsPos ncp = { 0, 0 };

  if( GoToNextWord_GetPosition( ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

void View::Imp::GoToEndOfWord()
{
  Trace trace( __PRETTY_FUNCTION__ );

  CrsPos ncp = { 0, 0 };

  if( GoToEndOfWord_GetPosition( ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

void View::Imp::GoToLine( const unsigned user_line_num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Internal line number is 1 less than user line number:
  const unsigned NCL = user_line_num - 1; // New cursor line number

  if( m_fb.NumLines() <= NCL )
  {
    PrintCursor();
  }
  else {
    GoToCrsPos_Write( NCL, 0 );
  }
}

void View::Imp::GoToStartOfRow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m_fb.NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line

  GoToCrsPos_Write( OCL, m_leftChar );
}

void View::Imp::GoToEndOfRow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m_fb.NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line

  const unsigned LL = m_fb.LineLen( OCL );
  if( 0==LL ) return;

  const unsigned NCP = Min( LL-1, m_leftChar + WorkingCols() - 1 );

  GoToCrsPos_Write( OCL, NCP );
}

void View::Imp::GoToTopOfFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToCrsPos_Write( 0, 0 );
}

void View::Imp::GoToOppositeBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();
  const unsigned NUM_LINES = m_fb.NumLines();
  const unsigned CL = CrsLine();
  const unsigned CC = CrsChar();
  const unsigned LL = m_fb.LineLen( CL );

  if( 0==NUM_LINES || 0==LL ) return;

  const char C = m_fb.Get( CL, CC );

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

void View::Imp::GoToLeftSquigglyBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();

  const char  start_char = '}';
  const char finish_char = '{';
  GoToOppositeBracket_Backward( start_char, finish_char );
}

void View::Imp::GoToRightSquigglyBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();

  const char  start_char = '{';
  const char finish_char = '}';
  GoToOppositeBracket_Forward( start_char, finish_char );
}

void View::Imp::GoToCrsPos_NoWrite( const unsigned ncp_crsLine
                                  , const unsigned ncp_crsChar )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // These moves refer to View of buffer:
  const bool MOVE_DOWN  = BotLine()   < ncp_crsLine;
  const bool MOVE_RIGHT = RightChar() < ncp_crsChar;
  const bool MOVE_UP    = ncp_crsLine < m_topLine;
  const bool MOVE_LEFT  = ncp_crsChar < m_leftChar;

  if     ( MOVE_DOWN ) m_topLine = ncp_crsLine - WorkingRows() + 1;
  else if( MOVE_UP   ) m_topLine = ncp_crsLine;
  m_crsRow  = ncp_crsLine - m_topLine;

  if     ( MOVE_RIGHT ) m_leftChar = ncp_crsChar - WorkingCols() + 1;
  else if( MOVE_LEFT  ) m_leftChar = ncp_crsChar;
  m_crsCol   = ncp_crsChar - m_leftChar;
}

void View::Imp::GoToCrsPos_Write( const unsigned ncp_crsLine
                                , const unsigned ncp_crsChar )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = CrsLine();
  const unsigned OCP = CrsChar();
  const unsigned NCL = ncp_crsLine;
  const unsigned NCP = ncp_crsChar;

  if( OCL == NCL && OCP == NCP )
  {
    PrintCursor();  // Put cursor back into position.
    return;
  }
  if( m_inVisualMode )
  {
    v_fn_line = NCL;
    v_fn_char = NCP;
  }
  // These moves refer to View of buffer:
  const bool MOVE_DOWN  = BotLine()   < NCL;
  const bool MOVE_RIGHT = RightChar() < NCP;
  const bool MOVE_UP    = NCL < m_topLine;
  const bool MOVE_LEFT  = NCP < m_leftChar;

  const bool redraw = MOVE_DOWN || MOVE_RIGHT || MOVE_UP || MOVE_LEFT;

  if( redraw )
  {
    if     ( MOVE_DOWN ) m_topLine = NCL - WorkingRows() + 1;
    else if( MOVE_UP   ) m_topLine = NCL;

    if     ( MOVE_RIGHT ) m_leftChar = NCP - WorkingCols() + 1;
    else if( MOVE_LEFT  ) m_leftChar = NCP;

    // m_crsRow and m_crsCol must be set to new values before calling CalcNewCrsByte
    m_crsRow = NCL - m_topLine;
    m_crsCol = NCP - m_leftChar;

    Update();
  }
  else if( m_inVisualMode )
  {
    if( m_inVisualBlock ) GoToCrsPos_Write_VisualBlock( OCL, OCP, NCL, NCP );
    else                  GoToCrsPos_Write_Visual     ( OCL, OCP, NCL, NCP );
  }
  else {
    // m_crsRow and m_crsCol must be set to new values before calling CalcNewCrsByte and PrintCursor
    m_crsRow = NCL - m_topLine;
    m_crsCol = NCP - m_leftChar;

    PrintCursor();  // Put cursor into position.

    m_sts_line_needs_update = true;
  }
}

bool View::Imp::GoToFile_GetFileName( String& fname )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool got_filename = false;

  const unsigned CL = CrsLine();
  const unsigned LL = m_fb.LineLen( CL );

  if( LL )
  {
    MoveInBounds();
    const int CC = CrsChar();
    char c = m_fb.Get( CL, CC );

    if( IsFileNameChar( c ) )
    {
      // Get the file name:
      got_filename = true;

      fname.push( c );

      // Search backwards, until white space is found:
      for( int k=CC-1; -1<k; k-- )
      {
        c = m_fb.Get( CL, k );

        if( !IsFileNameChar( c ) ) break;
        else fname.insert( 0, c );
      }
      // Search forwards, until white space is found:
      for( unsigned k=CC+1; k<LL; k++ )
      {
        c = m_fb.Get( CL, k );

        if( !IsFileNameChar( c ) ) break;
        else fname.push( c );
      }
      EnvKeys2Vals( fname );
    }
  }
  return got_filename;
}

// 1. Move to command line.
// 2. Clear command line.
// 3. Go back to beginning of command line.
//
void View::Imp::GoToCmdLineClear( const char* S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned ROW = Cmd__Line_Row();

  // Clear command line:
  for( unsigned k=0; k<WorkingCols(); k++ )
  {
    Console::Set( ROW, Col_Win_2_GL( k ), ' ', S_NORMAL );
  }
  const unsigned S_LEN = strlen( S );
  const unsigned ST    = Col_Win_2_GL( 0 );

  for( unsigned k=0; k<S_LEN; k++ )
  {
    Console::Set( ROW, ST+k, S[k], S_NORMAL );
  }
  Console::Update();
  Console::Move_2_Row_Col( ROW, Col_Win_2_GL( S_LEN ) );
  Console::Flush();
}

void View::Imp::GoUp()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m_fb.NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  if( OCL == 0 ) return;

  const unsigned NCL = OCL-1; // New cursor line

  const unsigned OCP = CrsChar(); // Old cursor position
        unsigned NCP = OCP;

  GoToCrsPos_Write( NCL, NCP );
}

void View::Imp::GoDown()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m_fb.NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  if( OCL == NUM_LINES-1 ) return;

  const unsigned NCL = OCL+1; // New cursor line

  const unsigned OCP = CrsChar(); // Old cursor position
        unsigned NCP = OCP;

  GoToCrsPos_Write( NCL, NCP );
}

void View::Imp::GoLeft()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==m_fb.NumLines() ) return;

  const unsigned CL = CrsLine(); // Cursor line
  const unsigned CP = CrsChar(); // Cursor position

  if( CP == 0 ) return;

  GoToCrsPos_Write( CL, CP-1 );
}

void View::Imp::GoRight()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==m_fb.NumLines() ) return;

  const unsigned CL = CrsLine(); // Cursor line
  const unsigned LL = m_fb.LineLen( CL );
  if( 0==LL ) return;

  const unsigned CP = CrsChar(); // Cursor position
  if( LL-1 <= CP ) return;

  GoToCrsPos_Write( CL, CP+1 );
}

// This one works better when NOT in visual mode:
void View::Imp::PageDown()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m_fb.NumLines();
  if( !NUM_LINES ) return;

  const unsigned newTopLine = m_topLine + WorkingRows() - 1;
  // Subtracting 1 above leaves one line in common between the 2 pages.

  if( newTopLine < NUM_LINES )
  {
    m_crsCol = 0;
    m_topLine = newTopLine;

    // Dont let cursor go past the end of the file:
    if( NUM_LINES <= CrsLine() )
    {
      // This line places the cursor at the top of the screen, which I prefer:
      m_crsRow = 0;
    }
    Update();
  }
}

void View::Imp::PageUp()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned OCL = CrsLine(); // Old cursor line
  const unsigned OCP = CrsChar(); // Old cursor position

  // Dont scroll if we are at the top of the file:
  if( m_topLine )
  {
    //Leave m_crsRow unchanged.
    m_crsCol = 0;

    // Dont scroll past the top of the file:
    if( m_topLine < WorkingRows() - 1 )
    {
      m_topLine = 0;
    }
    else {
      m_topLine -= WorkingRows() - 1;
    }
    Update();
  }
}

// If past end of line, move back to end of line.
// Returns true if moved, false otherwise.
//
bool View::Imp::MoveInBounds()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CL  = CrsLine();
  const unsigned LL  = m_fb.LineLen( CL );
  const unsigned EOL = LL ? LL-1 : 0;

  if( EOL < CrsChar() ) // Since cursor is now allowed past EOL,
  {                      // it may need to be moved back:
    GoToCrsPos_NoWrite( CL, EOL );
    return true;
  }
  return false;
}

void View::Imp::MoveCurrLineToTop()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_crsRow )
  {
    m_topLine += m_crsRow;
    m_crsRow = 0;
    Update();
  }
}

void View::Imp::MoveCurrLineCenter()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned center = SCast<unsigned>( 0.5*WorkingRows() + 0.5 );

  const unsigned OCL = CrsLine(); // Old cursor line

  if( 0 < OCL && OCL < center && 0 < m_topLine )
  {
    // Cursor line cannot be moved to center, but can be moved closer to center
    // CrsLine() does not change:
    m_crsRow += m_topLine;
    m_topLine = 0;
    Update();
  }
  else if( center <= OCL
        && center != m_crsRow )
  {
    m_topLine += m_crsRow - center;
    m_crsRow = center;
    Update();
  }
}

void View::Imp::MoveCurrLineToBottom()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 < m_topLine )
  {
    const unsigned WR  = WorkingRows();
    const unsigned OCL = CrsLine(); // Old cursor line

    if( WR-1 <= OCL )
    {
      m_topLine -= WR - m_crsRow - 1;
      m_crsRow = WR-1;
      Update();
    }
    else {
      // Cursor line cannot be moved to bottom, but can be moved closer to bottom
      // CrsLine() does not change:
      m_crsRow += m_topLine;
      m_topLine = 0;
      Update();
    }
  }
}

void View::Imp::Do_i()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_inInsertMode = true;
  DisplayBanner();

  if( 0 == m_fb.NumLines() ) m_fb.PushLine();

  const unsigned LL = m_fb.LineLen( CrsLine() );  // Line length

  if( LL < CrsChar() ) // Since cursor is now allowed past EOL,
  {                    // it may need to be moved back:
    // For user friendlyness, move cursor to new position immediately:
    GoToCrsPos_Write( CrsLine(), LL );
  }
  unsigned count = 0;
  for( char c=m_key.In(); c != ESC; c=m_key.In() )
  {
    if     ( IsEndOfLineDelim( c ) ) InsertAddReturn();
    else if( BS  == c
          || DEL == c ) { if( count ) InsertBackspace(); }
    else                InsertAddChar( c );

    if( BS == c || DEL == c ) { if( count ) count--; }
    else count++;
  }
  Remove_Banner();
  m_inInsertMode = false;

  // Move cursor back one space:
  if( m_crsCol )
  {
    m_crsCol--;
    m_fb.Update();
  }
}

void View::Imp::Do_a()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_fb.NumLines()==0 ) { Do_i(); return; }

  const unsigned CL = CrsLine();
  const unsigned LL = m_fb.LineLen( CL );
  if( 0==LL ) { Do_i(); return; }

  const bool CURSOR_AT_EOL = ( CrsChar() == LL-1 );
  if( CURSOR_AT_EOL )
  {
    GoToCrsPos_NoWrite( CL, LL );
  }
  const bool CURSOR_AT_RIGHT_COL = ( m_crsCol == WorkingCols()-1 );

  if( CURSOR_AT_RIGHT_COL )
  {
    // Only need to scroll window right, and then enter insert i:
    m_leftChar++; //< This increments CrsChar()
  }
  else if( !CURSOR_AT_EOL ) // If cursor was at EOL, already moved cursor forward
  {
    // Only need to move cursor right, and then enter insert i:
    m_crsCol += 1; //< This increments CrsChar()
  }
  m_fb.Update();

  Do_i();
}

void View::Imp::Do_A()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToEndOfLine();

  Do_a();
}

void View::Imp::Do_o()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned ONL = m_fb.NumLines();
  const unsigned OCL = CrsLine();

  // Add the new line:
  const unsigned NCL = ( 0 != ONL ) ? OCL+1 : 0;
  m_fb.InsertLine( NCL );
  m_crsCol = 0;
  m_leftChar = 0;
  if     ( 0==ONL ) ; // Do nothing
  else if( OCL < BotLine() ) m_crsRow++;
  else {
    // If we were on the bottom working line, scroll screen down
    // one line so that the cursor line is not below the screen.
    m_topLine++;
  }
  m_fb.Update();

  Do_i();
}

void View::Imp::Do_O()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Add the new line:
  const unsigned new_line_num = CrsLine();
  m_fb.InsertLine( new_line_num );
  m_crsCol = 0;
  m_leftChar = 0;

  m_fb.Update();

  Do_i();
}

void View::Imp::Do_f( const char FAST_CHAR )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m_fb.NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine();           // Old cursor line
  const unsigned LL  = m_fb.LineLen( OCL ); // Line length
  const unsigned OCP = CrsChar();           // Old cursor position

  if( LL-1 <= OCP ) return;

  unsigned NCP = 0;
  bool found_char = false;
  for( unsigned p=OCP+1; !found_char && p<LL; p++ )
  {
    const char C = m_fb.Get( OCL, p );

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

void View::Imp::Do_u()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_fb.Undo( m_view );
}

void View::Imp::Do_U()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_fb.UndoAll( m_view );
}

void View::Imp::Do_Tilda()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==m_fb.NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  const unsigned OCP = CrsChar(); // Old cursor position
  const unsigned LL  = m_fb.LineLen( OCL );

  if( !LL || LL-1 < OCP ) return;

  char c = m_fb.Get( CrsLine(), CrsChar() );
  bool changed = false;
  if     ( isupper( c ) ) { c = tolower( c ); changed = true; }
  else if( islower( c ) ) { c = toupper( c ); changed = true; }

  if( m_crsCol < Min( LL-1, WorkingCols()-1 ) )
  {
    if( changed ) m_fb.Set( CrsLine(), CrsChar(), c );
    // Need to move cursor right:
    m_crsCol++;
  }
  else if( RightChar() < LL-1 )
  {
    // Need to scroll window right:
    if( changed ) m_fb.Set( CrsLine(), CrsChar(), c );
    m_leftChar++;
  }
  else // RightChar() == LL-1
  {
    // At end of line so cant move or scroll right:
    if( changed ) m_fb.Set( CrsLine(), CrsChar(), c );
  }
  m_fb.Update();
}

void View::Imp::Do_x()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If there is nothing to 'x', just return:
  if( !m_fb.NumLines() ) return;

  const unsigned CL = CrsLine();
  const unsigned LL = m_fb.LineLen( CL );

  // If nothing on line, just return:
  if( !LL ) return;

  // If past end of line, move to end of line:
  if( LL-1 < CrsChar() )
  {
    GoToCrsPos_Write( CL, LL-1 );
  }
  const uint8_t C = m_fb.RemoveChar( CL, CrsChar() );

  // Put char x'ed into register:
  Line* nlp = m_vis.BorrowLine( __FILE__,__LINE__ );
  nlp->push(__FILE__,__LINE__, C );
  m_reg.clear();
  m_reg.push( nlp );
  m_vis.SetPasteMode( PM_ST_FN );

  const unsigned NLL = m_fb.LineLen( CL ); // New line length

  // Reposition the cursor:
  if( NLL <= m_leftChar+m_crsCol )
  {
    // The char x'ed is the last char on the line, so move the cursor
    //   back one space.  Above, a char was removed from the line,
    //   but m_crsCol has not changed, so the last char is now NLL.
    // If cursor is not at beginning of line, move it back one more space.
    if( m_crsCol ) m_crsCol--;
  }
  m_fb.Update();
}

bool View::Imp::Do_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_inVisualBlock = false;

  return Do_visualMode();
}

bool View::Imp::Do_V()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_inVisualBlock = true;

  return Do_visualMode();
}

// Go to next pattern
void View::Imp::Do_n()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m_fb.NumLines();

  if( NUM_LINES == 0 ) return;

  CrsPos ncp = { 0, 0 }; // Next cursor position

  if( Do_n_FindNextPattern( ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
  else PrintCursor();
}

// Go to previous pattern
void View::Imp::Do_N()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m_fb.NumLines();

  if( NUM_LINES == 0 ) return;

  CrsPos ncp = { 0, 0 }; // Next cursor position

  if( Do_N_FindPrevPattern( ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

void View::Imp::Do_s()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CL  = CrsLine();
  const unsigned LL  = m_fb.LineLen( CL );
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
      m_fb.PushChar( CL, ' ' );
    }
    Do_a();
  }
  else // CP == EOL
  {
    Do_x();
    Do_a();
  }
}

void View::Imp::Do_cw()
{
  const unsigned result = Do_dw();

  if     ( result==1 ) Do_i();
  else if( result==2 ) Do_a();
}

// If nothing was deleted, return 0.
// If last char on line was deleted, return 2,
// Else return 1.
int View::Imp::Do_dw()
{
  Trace trace( __PRETTY_FUNCTION__ );

  unsigned st_line = CrsLine();
  unsigned st_char = CrsChar();

  const unsigned LL = m_fb.LineLen( st_line );

  // If past end of line, nothing to do
  if( st_char < LL )
  {
    // Determine fn_line, fn_char:
    unsigned fn_line = 0;
    unsigned fn_char = 0;
    if( Do_dw_get_fn( st_line, st_char, fn_line, fn_char ) )
    {
      Do_x_range( st_line, st_char, fn_line, fn_char );
      bool deleted_last_char = fn_char == LL-1;
      return deleted_last_char ? 2 : 1;
    }
  }
  return 0;
}

void View::Imp::Do_dd()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned ONL = m_fb.NumLines(); // Old number of lines

  // If there is nothing to 'dd', just return:
  if( 1 < ONL )
  {
  //if( &m_fb == m_vis.views[0][ BE_FILE ]->GetFB() )
    if( &m_fb == m_vis.GetFileBuf( BE_FILE ) )
    {
      Do_dd_BufferEditor( ONL );
    }
    else {
      Do_dd_Normal( ONL );
    }
  }
}

void View::Imp::Do_yy()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If there is nothing to 'yy', just return:
  if( !m_fb.NumLines() ) return;

  Line l = m_fb.GetLine( CrsLine() );

  m_reg.clear();
  m_reg.push( m_vis.BorrowLine( __FILE__,__LINE__, l ) );

  m_vis.SetPasteMode( PM_LINE );
}

void View::Imp::Do_yw()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If there is nothing to 'yw', just return:
  if( !m_fb.NumLines() ) return;

  unsigned st_line = CrsLine();
  unsigned st_char = CrsChar();

  // Determine fn_line, fn_char:
  unsigned fn_line = 0;
  unsigned fn_char = 0;

  if( Do_dw_get_fn( st_line, st_char, fn_line, fn_char ) )
  {
    m_reg.clear();
    m_reg.push( m_vis.BorrowLine( __FILE__,__LINE__ ) );

    // st_line and fn_line should be the same
    for( unsigned k=st_char; k<=fn_char; k++ )
    {
      m_reg[0]->push(__FILE__,__LINE__, m_fb.Get( st_line, k ) );
    }
    m_vis.SetPasteMode( PM_ST_FN );
  }
}

void View::Imp::Do_D()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m_fb.NumLines();
  const unsigned OCL = CrsLine();  // Old cursor line
  const unsigned OCP = CrsChar();  // Old cursor position
  const unsigned OLL = m_fb.LineLen( OCL );  // Old line length

  // If there is nothing to 'D', just return:
  if( !NUM_LINES || !OLL || OLL-1 < OCP ) return;

  Line* lpd = m_vis.BorrowLine( __FILE__,__LINE__ );

  for( unsigned k=OCP; k<OLL; k++ )
  {
    uint8_t c = m_fb.RemoveChar( OCL, OCP );
    lpd->push(__FILE__,__LINE__, c );
  }
  m_reg.clear();
  m_reg.push( lpd );
  m_vis.SetPasteMode( PM_ST_FN );

  // If cursor is not at beginning of line, move it back one space.
  if( m_crsCol ) m_crsCol--;

  m_fb.Update();
}

void View::Imp::Do_p()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const Paste_Mode PM = m_vis.GetPasteMode();  

  if     ( PM_ST_FN == PM ) return Do_p_or_P_st_fn( PP_After );
  else if( PM_BLOCK == PM ) return Do_p_block();
  else /*( PM_LINE  == PM*/ return Do_p_line();
}

void View::Imp::Do_P()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const Paste_Mode PM = m_vis.GetPasteMode();  

  if     ( PM_ST_FN == PM ) return Do_p_or_P_st_fn( PP_Before );
  else if( PM_BLOCK == PM ) return Do_P_block();
  else /*( PM_LINE  == PM*/ return Do_P_line();
}

void View::Imp::Do_R()
{
  Trace trace( __PRETTY_FUNCTION__ );
  m_inReplaceMode = true;
  DisplayBanner();

  if( m_fb.NumLines()==0 ) m_fb.PushLine();

  for( char c=m_key.In(); c != ESC; c=m_key.In() )
  {
    if( BS == c || DEL == c ) m_fb.Undo( m_view );
    else {
      if( '\n' == c ) ReplaceAddReturn();
      else            ReplaceAddChars( c );
    }
  }
  Remove_Banner();
  m_inReplaceMode = false;

  // Move cursor back one space:
  if( m_crsCol )
  {
    m_crsCol--;  // Move cursor back one space.
  }
  m_fb.Update();
}

void View::Imp::Do_J()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m_fb.NumLines(); // Number of lines
  const unsigned CL        = CrsLine();       // Cursor line

  const bool ON_LAST_LINE = ( CL == NUM_LINES-1 );

  if( ON_LAST_LINE || NUM_LINES < 2 ) return;

  GoToEndOfLine();

  Line* lp = m_fb.RemoveLineP( CL+1 );
  m_fb.AppendLineToLine( CL  , lp );

  // Update() is less efficient than only updating part of the screen,
  //   but it makes the code simpler.
  m_fb.Update();
}

bool View::Imp::InVisualArea( const unsigned line, const unsigned pos )
{
  if( m_inVisualMode )
  {
    if( m_inVisualBlock ) return InVisualBlock( line, pos );
    else                  return InVisualStFn ( line, pos );
  }
  return false;
}

bool View::Imp::InVisualBlock( const unsigned line, const unsigned pos )
{
  return ( v_st_line <= line && line <= v_fn_line && v_st_char <= pos  && pos  <= v_fn_char ) // bot rite
      || ( v_st_line <= line && line <= v_fn_line && v_fn_char <= pos  && pos  <= v_st_char ) // bot left
      || ( v_fn_line <= line && line <= v_st_line && v_st_char <= pos  && pos  <= v_fn_char ) // top rite
      || ( v_fn_line <= line && line <= v_st_line && v_fn_char <= pos  && pos  <= v_st_char );// top left
}

bool View::Imp::InVisualStFn( const unsigned line, const unsigned pos )
{
  if( v_st_line == line && line == v_fn_line )
  {
    return (v_st_char <= pos && pos <= v_fn_char)
        || (v_fn_char <= pos && pos <= v_st_char);
  }
  else if( (v_st_line < line && line < v_fn_line)
        || (v_fn_line < line && line < v_st_line) )
  {
    return true;
  }
  else if( v_st_line == line && line < v_fn_line )
  {
    return v_st_char <= pos;
  }
  else if( v_fn_line == line && line < v_st_line )
  {
    return v_fn_char <= pos;
  }
  else if( v_st_line < line && line == v_fn_line )
  {
    return pos <= v_fn_char;
  }
  else if( v_fn_line < line && line == v_st_line )
  {
    return pos <= v_st_char;
  }
  return false;
}

bool View::Imp::InComment( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m_fb.HasStyle( line, pos, HI_COMMENT );
}

bool View::Imp::InDefine( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m_fb.HasStyle( line, pos, HI_DEFINE );
}

bool View::Imp::InConst( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m_fb.HasStyle( line, pos, HI_CONST );
}

bool View::Imp::InControl( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m_fb.HasStyle( line, pos, HI_CONTROL );
}

bool View::Imp::InVarType( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m_fb.HasStyle( line, pos, HI_VARTYPE );
}

bool View::Imp::InStar( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m_fb.HasStyle( line, pos, HI_STAR );
}

bool  View::Imp::InNonAscii( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m_fb.HasStyle( line, pos, HI_NONASCII );
}

// 1. Re-position window if needed
// 2. Draw borders
// 4. Re-draw working window
// 5. Re-draw status line
// 6. Re-draw file-name line
// 7. Draw command line
// 8. Put cursor back in window
//
void View::Imp::Update()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_fb.Find_Styles( m_topLine + WorkingRows() );
  m_fb.ClearStars();
  m_fb.Find_Stars();

  RepositionView();
  Print_Borders();
  PrintWorkingView();
  PrintStsLine();
  PrintFileLine();
  PrintCmdLine();

  Console::Update();

  if( m_vis.CV() == &m_view ) PrintCursor();

  Console::Flush();
}

void View::Imp::RepositionView()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If a window re-size has taken place, and the window has gotten
  // smaller, change top line and left char if needed, so that the
  // cursor is in the buffer when it is re-drawn
  if( WorkingRows() <= m_crsRow )
  {
    m_topLine += ( m_crsRow - WorkingRows() + 1 );
    m_crsRow  -= ( m_crsRow - WorkingRows() + 1 );
  }
  if( WorkingCols() <= m_crsCol )
  {
    m_leftChar += ( m_crsCol - WorkingCols() + 1 );
    m_crsCol   -= ( m_crsCol - WorkingCols() + 1 );
  }
}

void View::Imp::Print_Borders()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const bool HIGHLIGHT = ( 1 < m_vis.GetNumWins() ) && ( &m_view == m_vis.CV() );

  const Style S = HIGHLIGHT ? S_BORDER_HI : S_BORDER;

  Print_Borders_Top   ( S );
  Print_Borders_Right ( S );
  Print_Borders_Left  ( S );
  Print_Borders_Bottom( S );
}

// Formats the status line.
// Must already be at the status line row.
// Returns the number of char's copied into ptr,
//   not including the terminating NULL.
//
void View::Imp::PrintStsLine()
{
  Trace trace( __PRETTY_FUNCTION__ );
  char buf1[  16]; buf1[0] = 0;
  char buf2[1024]; buf2[0] = 0;

  const unsigned CL = CrsLine(); // Line position
  const unsigned CC = CrsChar(); // Char position
  const unsigned LL = m_fb.NumLines() ? m_fb.LineLen( CL ) : 0;
  const unsigned WC = WorkingCols();

  // When inserting text at the end of a line, CrsChar() == LL
  if( LL && CC < LL ) // Print current char info:
  {
    const int c = m_fb.Get( CL, CC );

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
  const unsigned fileSize = m_fb.GetSize();
  const unsigned  crsByte = m_fb.GetCursorByte( CL, CC );
  char percent = SCast<char>(100*double(crsByte)/double(fileSize) + 0.5);
  // Screen width so far
  char* p = buf2;

  p += sprintf( buf2, "Pos=(%u,%u)  (%i%%, %u/%u)  Char=(%s)  "
                    , CL+1, CC+1, percent, crsByte, m_fb.GetSize(), buf1 );
  const unsigned SW = p - buf2; // Screen width so far

  if     ( SW < WC ) { for( unsigned k=SW; k<WC; k++ ) *p++ = ' '; }
  else if( WC < SW ) { p = buf2 + WC; /* Truncate extra part */ }
  *p = 0;

  Console::SetS( Sts__Line_Row(), Col_Win_2_GL( 0 ), buf2, S_STATUS );
}

void View::Imp::PrintFileLine()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned WC = WorkingCols();
  const unsigned FILE_NAME_LEN = strlen( m_fb.GetFileName() );

  char buf[1024]; buf[0] = 0;
  char* p = buf;

  if( WC < FILE_NAME_LEN )
  {
    // file_name does not fit, so truncate beginning
    p += sprintf( buf, "%s", m_fb.GetFileName() + (FILE_NAME_LEN - WC) );
  }
  else {
    // file_name fits, add spaces at end
    p += sprintf( buf, "%s", m_fb.GetFileName() );
    for( unsigned k=0; k<(WC-FILE_NAME_LEN); k++ ) *p++ = ' ';
  }
  *p = 0;

  Console::SetS( File_Line_Row(), Col_Win_2_GL( 0 ), buf, S_STATUS );
}

void View::Imp::PrintCmdLine()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // Assumes you are already at the command line,
  // and just prints "--INSERT--" banner, and/or clears command line

  unsigned col=0;
  // Draw insert banner if needed
  if( m_inInsertMode )
  {
    col=10; // Strlen of "--INSERT--"
    Console::SetS( Cmd__Line_Row(), Col_Win_2_GL( 0 ), "--INSERT--", S_BANNER );
  }
  else if( m_inReplaceMode )
  {
    col=11; // Strlen of "--REPLACE--"
    Console::SetS( Cmd__Line_Row(), Col_Win_2_GL( 0 ), "--REPLACE--", S_BANNER );
  }
  else if( m_vis.RunningCmd() && m_vis.CV() == &m_view )
  {
    col=11; // Strlen of "--RUNNING--"
    Console::SetS( Cmd__Line_Row(), Col_Win_2_GL( 0 ), "--RUNNING--", S_BANNER );
  }
  const unsigned WC = WorkingCols();

  for( ; col<WC; col++ )
  {
    Console::Set( Cmd__Line_Row(), Col_Win_2_GL( col ), ' ', S_NORMAL );
  }
}

// Moves cursor into position on screen:
//
void View::Imp::PrintCursor()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Either one of these should work:
  Console::Move_2_Row_Col( Row_Win_2_GL( m_crsRow ), Col_Win_2_GL( m_crsCol ) );
  Console::Flush();
}

void View::Imp::PrintWorkingView()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m_fb.NumLines();
  const unsigned WR        = WorkingRows();
  const unsigned WC        = WorkingCols();

  unsigned row = 0;
  for( unsigned k=m_topLine; k<NUM_LINES && row<WR; k++, row++ )
  {
    // Dont allow line wrap:
    const unsigned LL    = m_fb.LineLen( k );
    const unsigned G_ROW = Row_Win_2_GL( row );
    unsigned col=0;
    for( unsigned i=m_leftChar; i<LL && col<WC; i++, col++ )
    {
      Style s    = Get_Style( k, i );
      int   byte = m_fb.Get( k, i );

      PrintWorkingView_Set( LL, G_ROW, col, i, byte, s );
    }
    for( ; col<WC; col++ )
    {
      Console::Set( G_ROW, Col_Win_2_GL( col ), ' ', S_EMPTY );
    }
  }
  // Not enough lines to display, fill in with ~
  for( ; row < WR; row++ )
  {
    const unsigned G_ROW = Row_Win_2_GL( row );

    Console::Set( G_ROW, Col_Win_2_GL( 0 ), '~', S_EMPTY );

    for( unsigned col=1; col<WC; col++ )
    {
      Console::Set( G_ROW, Col_Win_2_GL( col ), ' ', S_EMPTY );
    }
  }
}

void View::Imp::PrintWorkingView_Set( const unsigned LL
                                    , const unsigned G_ROW
                                    , const unsigned col
                                    , const unsigned i
                                    , const unsigned byte
                                    , const Style    s )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( '\r' == byte && i==(LL-1) )
  {
    // For readability, display carriage return at end of line as a space
    Console::Set( G_ROW, Col_Win_2_GL( col ), ' ', S_NORMAL );
  }
  else {
//unsigned G_COL = Col_Win_2_GL( col );
//Log.Log("G_ROW=%d, G_COL=%d\n", G_ROW, G_COL );
    Console::Set( G_ROW, Col_Win_2_GL( col ), byte, s );
  }
}

void View::Imp::DisplayMapping()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char* mapping = "--MAPPING--";
  const int   mapping_len = strlen( mapping );

  // Command line row in window:
  const unsigned WIN_ROW = WorkingRows() + 2;

  const unsigned G_ROW = Row_Win_2_GL( WIN_ROW );
  const unsigned G_COL = Col_Win_2_GL( m_nCols-1-right_border-mapping_len );

  Console::SetS( G_ROW, G_COL, mapping, S_BANNER );

  Console::Update();
  PrintCursor(); // Put cursor back in position.
}

bool View::Imp::GetStsLineNeedsUpdate() const
{
  return m_sts_line_needs_update;
}

bool View::Imp::GetUnSavedChangeSts() const
{
  return m_us_change_sts;
}

void View::Imp::SetStsLineNeedsUpdate( const bool val )
{
  m_sts_line_needs_update = val;
}

void View::Imp::SetUnSavedChangeSts( const bool val )
{
  m_us_change_sts = val;
}

String View::Imp::Do_Star_GetNewPattern()
{
  Trace trace( __PRETTY_FUNCTION__ );
  String new_star;

  if( m_fb.NumLines() == 0 ) return new_star;

  const unsigned CL = CrsLine();
  const unsigned LL = m_fb.LineLen( CL );

  if( LL )
  {
    MoveInBounds();
    const unsigned CC = CrsChar();

    const int c = m_fb.Get( CL,  CC );

    if( isalnum( c ) || c=='_' )
    {
      new_star.push( c );

      // Search forward:
      for( unsigned k=CC+1; k<LL; k++ )
      {
        const int c = m_fb.Get( CL, k );
        if( isalnum( c ) || c=='_' ) new_star.push( c );
        else                         break;
      }
      // Search backward:
      for( int k=CC-1; 0<=k; k-- )
      {
        const int c = m_fb.Get( CL, k );
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

void View::Imp::PrintPatterns( const bool HIGHLIGHT )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m_fb.NumLines();
  const unsigned END_LINE  = Min( m_topLine+WorkingRows(), NUM_LINES );

  for( unsigned l=m_topLine; l<END_LINE; l++ )
  {
    const unsigned LL      = m_fb.LineLen( l );
    const unsigned END_POS = Min( m_leftChar+WorkingCols(), LL );

    for( unsigned p=m_leftChar; p<END_POS; p++ )
    {
      if( InStar( l, p ) )
      {
        Style s = S_STAR;

        if( !HIGHLIGHT )
        {
          s = S_NORMAL;
          if     ( InVisualArea(l,p) ) s = S_VISUAL;
          else if( InDefine    (l,p) ) s = S_DEFINE;
          else if( InConst     (l,p) ) s = S_CONST;
          else if( InControl   (l,p) ) s = S_CONTROL;
          else if( InVarType   (l,p) ) s = S_VARTYPE;
          else if( InComment   (l,p) ) s = S_COMMENT;
          else if( InNonAscii  (l,p) ) s = S_NONASCII;
        }
        int byte = m_fb.Get( l, p );
        Console::Set( Line_2_GL( l ), Char_2_GL( p ), byte, s );
      }
    }
  }
}

bool View::Imp::Has_Context()
{
  return 0 != m_topLine
      || 0 != m_leftChar
      || 0 != m_crsRow
      || 0 != m_crsCol ;
}

void View::Imp::Set_Context( View& vr )
{
  m_topLine  = vr.GetTopLine ();
  m_leftChar = vr.GetLeftChar();
  m_crsRow   = vr.GetCrsRow  ();
  m_crsCol   = vr.GetCrsCol  ();
}

// Change directory to that of file of view:
//
bool View::Imp::GoToDir()
{
  char* fname_str = const_cast<char*>( m_fb.GetFileName() );

  if( m_fb.IsDir() )
  {
    int err = chdir( fname_str );
    if( err ) return false;
  }
  else {
    char f_name_tail[ 1024 ];
    char* const last_slash = strrchr( fname_str, DIR_DELIM );
    if( last_slash ) {
      const int TAIL_LEN = last_slash - fname_str;
      if( TAIL_LEN )
      {
        strncpy( f_name_tail, fname_str, TAIL_LEN );
        f_name_tail[ TAIL_LEN ] = 0;

        int err = chdir( f_name_tail );
        if( err ) return false;
      }
    }
  }
  // If the directory of the view cannot be determined, just return true
  // so that the current directory will be used
  return true;
}

void View::Imp::TilePos_2_x()
{
  const unsigned CON_COLS = Console::Num_Cols();

  // TP_FULL     , TP_BOT__HALF    , TP_LEFT_QTR
  // TP_LEFT_HALF, TP_TOP__LEFT_QTR, TP_TOP__LEFT_8TH
  // TP_TOP__HALF, TP_BOT__LEFT_QTR, TP_BOT__LEFT_8TH
  m_x = 0;

  if( TP_RITE_HALF         == m_tile_pos
   || TP_TOP__RITE_QTR     == m_tile_pos
   || TP_BOT__RITE_QTR     == m_tile_pos
   || TP_RITE_CTR__QTR     == m_tile_pos
   || TP_TOP__RITE_CTR_8TH == m_tile_pos
   || TP_BOT__RITE_CTR_8TH == m_tile_pos )
  {
    m_x = CON_COLS/2;
  }
  else if( TP_LEFT_CTR__QTR     == m_tile_pos
        || TP_TOP__LEFT_CTR_8TH == m_tile_pos
        || TP_BOT__LEFT_CTR_8TH == m_tile_pos )
  {
    m_x = CON_COLS/4;
  }
  else if( TP_RITE_QTR      == m_tile_pos
        || TP_TOP__RITE_8TH == m_tile_pos
        || TP_BOT__RITE_8TH == m_tile_pos )
  {
    m_x = CON_COLS*3/4;
  }
}

void View::Imp::TilePos_2_y()
{
  const unsigned CON_ROWS = Console::Num_Rows();

  // TP_FULL         , TP_LEFT_CTR__QTR
  // TP_LEFT_HALF    , TP_RITE_CTR__QTR
  // TP_RITE_HALF    , TP_RITE_QTR
  // TP_TOP__HALF    , TP_TOP__LEFT_8TH
  // TP_TOP__LEFT_QTR, TP_TOP__LEFT_CTR_8TH
  // TP_TOP__RITE_QTR, TP_TOP__RITE_CTR_8TH
  // TP_LEFT_QTR     , TP_TOP__RITE_8TH
  m_y = 0;

  if( TP_BOT__HALF         == m_tile_pos
   || TP_BOT__LEFT_QTR     == m_tile_pos
   || TP_BOT__RITE_QTR     == m_tile_pos
   || TP_BOT__LEFT_8TH     == m_tile_pos
   || TP_BOT__LEFT_CTR_8TH == m_tile_pos
   || TP_BOT__RITE_CTR_8TH == m_tile_pos
   || TP_BOT__RITE_8TH     == m_tile_pos )
  {
    m_y = CON_ROWS/2;
  }
}

void View::Imp::TilePos_2_nRows()
{
  const unsigned CON_ROWS = Console::Num_Rows();
  const bool     ODD_ROWS = CON_ROWS%2;

  // TP_TOP__HALF        , TP_BOT__HALF        ,
  // TP_TOP__LEFT_QTR    , TP_BOT__LEFT_QTR    ,
  // TP_TOP__RITE_QTR    , TP_BOT__RITE_QTR    ,
  // TP_TOP__LEFT_8TH    , TP_BOT__LEFT_8TH    ,
  // TP_TOP__LEFT_CTR_8TH, TP_BOT__LEFT_CTR_8TH,
  // TP_TOP__RITE_CTR_8TH, TP_BOT__RITE_CTR_8TH,
  // TP_TOP__RITE_8TH    , TP_BOT__RITE_8TH    ,
  m_nRows = CON_ROWS/2;

  if( TP_FULL          == m_tile_pos
   || TP_LEFT_HALF     == m_tile_pos
   || TP_RITE_HALF     == m_tile_pos
   || TP_LEFT_QTR      == m_tile_pos
   || TP_LEFT_CTR__QTR == m_tile_pos
   || TP_RITE_CTR__QTR == m_tile_pos
   || TP_RITE_QTR      == m_tile_pos )
  {
    m_nRows = CON_ROWS;
  }
  if( ODD_ROWS && ( TP_BOT__HALF         == m_tile_pos
                 || TP_BOT__LEFT_QTR     == m_tile_pos
                 || TP_BOT__RITE_QTR     == m_tile_pos
                 || TP_BOT__LEFT_8TH     == m_tile_pos
                 || TP_BOT__LEFT_CTR_8TH == m_tile_pos
                 || TP_BOT__RITE_CTR_8TH == m_tile_pos
                 || TP_BOT__RITE_8TH     == m_tile_pos ) )
  {
    m_nRows++;
  }
}

void View::Imp::TilePos_2_nCols()
{
  const unsigned CON_COLS = Console::Num_Cols();
  const unsigned ODD_COLS = CON_COLS%4;

  // TP_LEFT_QTR     , TP_TOP__LEFT_8TH    , TP_BOT__LEFT_8TH    ,
  // TP_LEFT_CTR__QTR, TP_TOP__LEFT_CTR_8TH, TP_BOT__LEFT_CTR_8TH,
  // TP_RITE_CTR__QTR, TP_TOP__RITE_CTR_8TH, TP_BOT__RITE_CTR_8TH,
  // TP_RITE_QTR     , TP_TOP__RITE_8TH    , TP_BOT__RITE_8TH    ,
  m_nCols = CON_COLS/4;

  if( TP_FULL      == m_tile_pos
   || TP_TOP__HALF == m_tile_pos
   || TP_BOT__HALF == m_tile_pos )
  {
    m_nCols = CON_COLS;
  }
  else if( TP_LEFT_HALF     == m_tile_pos
        || TP_RITE_HALF     == m_tile_pos
        || TP_TOP__LEFT_QTR == m_tile_pos
        || TP_TOP__RITE_QTR == m_tile_pos
        || TP_BOT__LEFT_QTR == m_tile_pos
        || TP_BOT__RITE_QTR == m_tile_pos )
  {
    m_nCols = CON_COLS/2;
  }
  if( ((TP_RITE_HALF         == m_tile_pos) && (ODD_COLS==1 || ODD_COLS==3))
   || ((TP_TOP__RITE_QTR     == m_tile_pos) && (ODD_COLS==1 || ODD_COLS==3))
   || ((TP_BOT__RITE_QTR     == m_tile_pos) && (ODD_COLS==1 || ODD_COLS==3))

   || ((TP_RITE_QTR          == m_tile_pos) && (ODD_COLS==1 || ODD_COLS==2 || ODD_COLS==3))
   || ((TP_TOP__RITE_8TH     == m_tile_pos) && (ODD_COLS==1 || ODD_COLS==2 || ODD_COLS==3))
   || ((TP_BOT__RITE_8TH     == m_tile_pos) && (ODD_COLS==1 || ODD_COLS==2 || ODD_COLS==3))

   || ((TP_LEFT_CTR__QTR     == m_tile_pos) && (ODD_COLS==2 || ODD_COLS==3))
   || ((TP_TOP__LEFT_CTR_8TH == m_tile_pos) && (ODD_COLS==2 || ODD_COLS==3))
   || ((TP_BOT__LEFT_CTR_8TH == m_tile_pos) && (ODD_COLS==2 || ODD_COLS==3))

   || ((TP_RITE_CTR__QTR     == m_tile_pos) && (ODD_COLS==3))
   || ((TP_TOP__RITE_CTR_8TH == m_tile_pos) && (ODD_COLS==3))
   || ((TP_BOT__RITE_CTR_8TH == m_tile_pos) && (ODD_COLS==3)) )
  {
    m_nCols++;
  }
}

void View::Imp::UpdateLines( const unsigned st_line, const unsigned fn_line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Figure out which lines are currently on screen:
  unsigned m_st_line = st_line;
  unsigned m_fn_line = fn_line;

  if( m_st_line < m_topLine   ) m_st_line = m_topLine;
  if( BotLine() < m_fn_line ) m_fn_line = BotLine();
  if( m_fn_line < m_st_line ) return; // Nothing to update

  // Re-draw lines:
  PrintLines( m_st_line, m_fn_line );
}

void View::Imp::InsertAddChar( const char c )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_fb.NumLines()==0 ) m_fb.PushLine();

  m_fb.InsertChar( CrsLine(), CrsChar(), c );

  if( WorkingCols() <= m_crsCol+1 )
  {
    // On last working column, need to scroll right:
    m_leftChar++;
  }
  else {
    m_crsCol += 1;
  }
  m_fb.Update();
}

void View::Imp::InsertAddReturn()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // The lines in fb do not end with '\n's.
  // When the file is written, '\n's are added to the ends of the lines.
  Line new_line(__FILE__,__LINE__);
  const unsigned OLL = m_fb.LineLen( CrsLine() );  // Old line length
  const unsigned OCP = CrsChar();                  // Old cursor position

  for( unsigned k=OCP; k<OLL; k++ )
  {
    const uint8_t C = m_fb.RemoveChar( CrsLine(), OCP );
    bool ok = new_line.push(__FILE__,__LINE__, C );
    ASSERT( __LINE__, ok, "ok" );
  }
  // Truncate the rest of the old line:
  // Add the new line:
  const unsigned new_line_num = CrsLine()+1;
  m_fb.InsertLine( new_line_num, new_line );
  m_crsCol = 0;
  m_leftChar = 0;
  if( CrsLine() < BotLine() ) m_crsRow++;
  else {
    // If we were on the bottom working line, scroll screen down
    // one line so that the cursor line is not below the screen.
    m_topLine++;
  }
  m_fb.Update();
}

void View::Imp::InsertBackspace()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If no lines in buffer, no backspacing to be done
  if( 0==m_fb.NumLines() ) return;

  const unsigned OCL = CrsLine();  // Old cursor line

  const unsigned OCP = CrsChar();            // Old cursor position
  const unsigned OLL = m_fb.LineLen( OCL );  // Old line length

  if( OCP ) InsertBackspace_RmC ( OCL, OCP );
  else      InsertBackspace_RmNL( OCL );
}

void View::Imp::InsertBackspace_RmC( const unsigned OCL
                                   , const unsigned OCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_fb.RemoveChar( OCL, OCP-1 );

  m_crsCol -= 1;

  m_fb.Update();
}

void View::Imp::InsertBackspace_RmNL( const unsigned OCL )
{
  Trace trace( __PRETTY_FUNCTION__ );
  // Cursor Line Position is zero, so:
  // 1. Save previous line, end of line + 1 position
  CrsPos ncp = { OCL-1, m_fb.LineLen( OCL-1 ) };

  // 2. Remove the line
  Line lp(__FILE__, __LINE__);
  m_fb.RemoveLine( OCL, lp );

  // 3. Append rest of line to previous line
  m_fb.AppendLineToLine( OCL-1, lp );

  // 4. Put cursor at the old previous line end of line + 1 position
  const bool MOVE_UP    = ncp.crsLine < m_topLine;
  const bool MOVE_RIGHT = RightChar() < ncp.crsChar;

  if( MOVE_UP ) m_topLine = ncp.crsLine;
                m_crsRow = ncp.crsLine - m_topLine;

  if( MOVE_RIGHT ) m_leftChar = ncp.crsChar - WorkingCols() + 1;
                   m_crsCol   = ncp.crsChar - m_leftChar;

  // 5. Removed a line, so update to re-draw window view
  m_fb.Update();
}

// Returns true if something was changed, else false
//
bool View::Imp::Do_visualMode()
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();
  m_inVisualMode = true;
  DisplayBanner();

  v_st_line = CrsLine();  v_fn_line = v_st_line;
  v_st_char = CrsChar();  v_fn_char = v_st_char;

  // Write current byte in visual:
  Replace_Crs_Char( S_VISUAL );

  while( m_inVisualMode )
  {
    const char c=m_key.In();

    if     ( c == 'l' ) GoRight();
    else if( c == 'h' ) GoLeft();
    else if( c == 'j' ) GoDown();
    else if( c == 'k' ) GoUp();
    else if( c == 'H' ) GoToTopLineInView();
    else if( c == 'L' ) GoToBotLineInView();
    else if( c == 'M' ) GoToMidLineInView();
    else if( c == '0' ) GoToBegOfLine();
    else if( c == '$' ) GoToEndOfLine();
    else if( c == 'g' ) Do_v_Handle_g();
    else if( c == 'G' ) GoToEndOfFile();
    else if( c == 'F' ) PageDown_v();
    else if( c == 'B' ) PageUp_v();
    else if( c == 'b' ) GoToPrevWord();
    else if( c == 'w' ) GoToNextWord();
    else if( c == 'e' ) GoToEndOfWord();
    else if( c == '%' ) GoToOppositeBracket();
    else if( c == 'z' ) m_vis.Handle_z();
    else if( c == 'f' ) m_vis.Handle_f();
    else if( c == ';' ) m_vis.Handle_SemiColon();
    else if( c == 'y' ) { Do_y_v(); goto EXIT_VISUAL; }
    else if( c == 'Y' ) { Do_Y_v(); goto EXIT_VISUAL; }
    else if( c == 'x'
          || c == 'd' ) { Do_x_v(); return true; }
    else if( c == 'D' ) { Do_D_v(); return true; }
    else if( c == 's' ) { Do_s_v(); return true; }
    else if( c == '~' ) { Do_Tilda_v(); return true; }
    else if( c == ESC ) { goto EXIT_VISUAL; }
  }
  return false;

EXIT_VISUAL:
  m_inVisualMode = false;
  Undo_v();
  Remove_Banner();
  return false;
}

void View::Imp::Undo_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned st_line = Min( v_st_line, v_fn_line );
  const unsigned fn_line = Max( v_st_line, v_fn_line );

  UpdateLines( st_line, fn_line );

  m_sts_line_needs_update = true;
}

// Returns true if still in visual mode, else false
//
void View::Imp::Do_v_Handle_g()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC2 = m_key.In();

  if     ( CC2 == 'g' ) GoToTopOfFile();
  else if( CC2 == '0' ) GoToStartOfRow();
  else if( CC2 == '$' ) GoToEndOfRow();
  else if( CC2 == 'f' ) Do_v_Handle_gf();
  else if( CC2 == 'p' ) Do_v_Handle_gp();
}

void View::Imp::Do_v_Handle_gf()
{
  if( v_st_line == v_fn_line )
  {
    const unsigned m_v_st_char = v_st_char < v_fn_char ? v_st_char : v_fn_char;
    const unsigned m_v_fn_char = v_st_char < v_fn_char ? v_fn_char : v_st_char;

    String fname;

    for( unsigned P = m_v_st_char; P<=m_v_fn_char; P++ )
    {
      fname.push( m_fb.Get( v_st_line, P  ) );
    }
    bool went_to_file = m_vis.GoToBuffer_Fname( fname );

    if( went_to_file )
    {
      // If we made it to buffer indicated by fname, no need to Undo_v() or
      // Remove_Banner() because the whole view pane will be redrawn
      m_inVisualMode = false;
    }
  }
}

void View::Imp::Do_v_Handle_gp()
{
  if( v_st_line == v_fn_line )
  {
    const unsigned m_v_st_char = v_st_char < v_fn_char ? v_st_char : v_fn_char;
    const unsigned m_v_fn_char = v_st_char < v_fn_char ? v_fn_char : v_st_char;

    String pattern;

    for( unsigned P = m_v_st_char; P<=m_v_fn_char; P++ )
    {
      pattern.push( m_fb.Get( v_st_line, P  ) );
    }
    m_vis.Handle_Slash_GotPattern( pattern, false );

    m_inVisualMode = false;
    Undo_v();
    Remove_Banner();
  }
}

void View::Imp::DisplayBanner()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Command line row in window:
  const unsigned WIN_ROW = WorkingRows() + 2;
  const unsigned WIN_COL = 0;

  const unsigned G_ROW = Row_Win_2_GL( WIN_ROW );
  const unsigned G_COL = Col_Win_2_GL( WIN_COL );

  if( m_inInsertMode )
  {
    Console::SetS( G_ROW, G_COL, "--INSERT --", S_BANNER );
  }
  else if( m_inReplaceMode )
  {
    Console::SetS( G_ROW, G_COL, "--REPLACE--", S_BANNER );
  }
  else if( m_inVisualMode )
  {
    Console::SetS( G_ROW, G_COL, "--VISUAL --", S_BANNER );
  }
  Console::Update();
  PrintCursor();          // Put cursor back in position.
}

void View::Imp::Remove_Banner()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned WC = WorkingCols();
  const unsigned N = Min( WC, 11 );

  // Command line row in window:
  const unsigned WIN_ROW = WorkingRows() + 2;

  // Clear command line:
  for( unsigned k=0; k<N; k++ )
  {
    Console::Set( Row_Win_2_GL( WIN_ROW )
                , Col_Win_2_GL( k )
                , ' '
                , S_NORMAL );
  }
  Console::Update();
  PrintCursor(); // Put cursor back in position.
}

void View::Imp::ReplaceAddReturn()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // The lines in fb do not end with '\n's.
  // When the file is written, '\n's are added to the ends of the lines.
  Line new_line(__FILE__, __LINE__);
  const unsigned OLL = m_fb.LineLen( CrsLine() );
  const unsigned OCP = CrsChar();

  for( unsigned k=OCP; k<OLL; k++ )
  {
    const uint8_t C = m_fb.RemoveChar( CrsLine(), OCP );
    bool ok = new_line.push(__FILE__,__LINE__, C );
    ASSERT( __LINE__, ok, "ok" );
  }
  // Truncate the rest of the old line:
  // Add the new line:
  const unsigned new_line_num = CrsLine()+1;
  m_fb.InsertLine( new_line_num, new_line );
  m_crsCol = 0;
  m_leftChar = 0;
  if( CrsLine() < BotLine() ) m_crsRow++;
  else {
    // If we were on the bottom working line, scroll screen down
    // one line so that the cursor line is not below the screen.
    m_topLine++;
  }
  m_fb.Update();
}

void View::Imp::ReplaceAddChars( const char C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( m_fb.NumLines()==0 ) m_fb.PushLine();

  const unsigned CL = CrsLine();
  const unsigned CP = CrsChar();
  const unsigned LL = m_fb.LineLen( CL );
  const unsigned EOL = LL ? LL-1 : 0;

  if( EOL < CP )
  {
    // Extend line out to where cursor is:
    for( unsigned k=LL; k<CP; k++ ) m_fb.PushChar( CL, ' ' );
  }
  // Put char back in file buffer
  const bool continue_last_update = false;
  if( CP < LL ) m_fb.Set( CL, CP, C, continue_last_update );
  else {
    m_fb.PushChar( CL, C );
  }
  if( m_crsCol < WorkingCols()-1 )
  {
    m_crsCol++;
  }
  else {
    m_leftChar++;
  }
  m_fb.Update();
}

void View::Imp::Replace_Crs_Char( Style style )
{
  const unsigned LL = m_fb.LineLen( CrsLine() ); // Line length

  if( LL )
  {
    int byte = m_fb.Get( CrsLine(), CrsChar() );

    Console::Set( Row_Win_2_GL( m_crsRow )
                , Col_Win_2_GL( m_crsCol )
                , byte, style );
  }
}

void View::Imp::PrintLines( const unsigned st_line, const unsigned fn_line )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned WC = WorkingCols();

  for( unsigned k=st_line; k<=fn_line; k++ )
  {
    // Dont allow line wrap:
    const unsigned LL    = m_fb.LineLen( k );
    const unsigned G_ROW = Line_2_GL( k );
    unsigned col=0;
    for( unsigned i=m_leftChar; col<WC && i<LL; i++, col++ )
    {
      Style s = Get_Style( k, i );

      int byte = m_fb.Get( k, i );

      PrintWorkingView_Set( LL, G_ROW, col, i, byte, s );
    }
    for( ; col<WC; col++ )
    {
      Console::Set( G_ROW, Col_Win_2_GL( col ), ' ', S_EMPTY );
    }
  }
  Console::Update();
  Console::Flush();
}

void View::Imp::Print_Borders_Top( const Style S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( top___border )
  {
    const uint8_t BORDER_CHAR = m_fb.Changed() ? '+' : ' ';
    const unsigned ROW_G = m_y;

    for( unsigned k=0; k<WinCols(); k++ )
    {
      const unsigned COL_G = m_x + k;

      Console::Set( ROW_G, COL_G, BORDER_CHAR, S );
    }
  }
}

void View::Imp::Print_Borders_Bottom( const Style S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( bottomborder )
  {
    const uint8_t BORDER_CHAR = m_fb.Changed() ? '+' : ' ';
    const unsigned ROW_G = m_y + WinRows() - 1;

    for( unsigned k=0; k<WinCols(); k++ )
    {
      const unsigned COL_G = m_x + k;

      if( ROW_G < Console::Num_Rows()-1
       || COL_G < Console::Num_Cols()-1 )
      {
        // Do not print bottom right hand corner of console, because
        // on some terminals it scrolls the whole console screen up one line:
        Console::Set( ROW_G, COL_G, BORDER_CHAR, S );
      }
    }
  }
}

void View::Imp::Print_Borders_Right( const Style S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( right_border )
  {
    const uint8_t BORDER_CHAR = m_fb.Changed() ? '+' : ' ';
    const unsigned COL_G = m_x + WinCols() - 1;

    for( unsigned k=0; k<WinRows()-1; k++ )
    {
      const unsigned ROW_G = m_y + k;

      if( ROW_G < Console::Num_Rows()-1
       || COL_G < Console::Num_Cols()-1 )
      {
        // Do not print bottom right hand corner of console, because
        // on some terminals it scrolls the whole console screen up one line:
        Console::Set( ROW_G, COL_G, BORDER_CHAR, S );
      }
    }
  }
}

void View::Imp::Print_Borders_Left( const Style S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( left__border )
  {
    const uint8_t BORDER_CHAR = m_fb.Changed() ? '+' : ' ';
    const unsigned COL_G = m_x;

    for( unsigned k=0; k<WinRows(); k++ )
    {
      const unsigned ROW_G = m_y + k;

      Console::Set( ROW_G, COL_G, BORDER_CHAR, S );
    }
  }
}

// Returns true if found next word, else false
//
bool View::Imp::GoToNextWord_GetPosition( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m_fb.NumLines();
  if( 0==NUM_LINES ) return false;

  bool found_space = false;
  bool found_word  = false;
  const unsigned OCL = CrsLine(); // Old cursor line
  const unsigned OCP = CrsChar(); // Old cursor position

  IsWord_Func isWord = IsWord_Ident;

  // Find white space, and then find non-white space
  for( unsigned l=OCL; (!found_space || !found_word) && l<NUM_LINES; l++ )
  {
    const unsigned LL = m_fb.LineLen( l );
    if( LL==0 || OCL<l )
    {
      found_space = true;
      // Once we have encountered a space, word is anything non-space.
      // An empty line is considered to be a space.
      isWord = NotSpace;
    }
    const unsigned START_C = OCL==l ? OCP : 0;

    for( unsigned p=START_C; (!found_space || !found_word) && p<LL; p++ )
    {
      ncp.crsLine = l;
      ncp.crsChar = p;

      const int C = m_fb.Get( l, p );

      if( found_space  )
      {
        if( isWord( C ) ) found_word = true;
      }
      else {
        if( !isWord( C ) ) found_space = true;
      }
      // Once we have encountered a space, word is anything non-space
      if( IsSpace( C ) ) isWord = NotSpace;
    }
  }
  return found_space && found_word;
}

// Return true if new cursor position found, else false
bool View::Imp::GoToPrevWord_GetPosition( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m_fb.NumLines();
  if( 0==NUM_LINES ) return false;

  const int      OCL = CrsLine(); // Old cursor line
  const unsigned LL  = m_fb.LineLen( OCL );

  if( LL < CrsChar() ) // Since cursor is now allowed past EOL,
  {                    // it may need to be moved back:
    if( LL && !IsSpace( m_fb.Get( OCL, LL-1 ) ) )
    {
      // Backed up to non-white space, which is previous word, so return true
      ncp.crsLine = OCL;
      ncp.crsChar = LL-1;
      return true;
    }
    else {
      GoToCrsPos_NoWrite( OCL, LL ? LL-1 : 0 );
    }
  }
  bool found_space = false;
  bool found_word  = false;
  const unsigned OCP = CrsChar(); // Old cursor position

  IsWord_Func isWord = NotSpace;

  // Find word to non-word transition
  for( int l=OCL; (!found_space || !found_word) && -1<l; l-- )
  {
    const int LL = m_fb.LineLen( l );
    if( LL==0 || l<OCL )
    {
      // Once we have encountered a space, word is anything non-space.
      // An empty line is considered to be a space.
      isWord = NotSpace;
    }
    const unsigned START_C = OCL==l ? OCP-1 : LL-1;

    for( int p=START_C; (!found_space || !found_word) && -1<p; p-- )
    {
      ncp.crsLine = l;
      ncp.crsChar = p;

      const int C = m_fb.Get( l, p );

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
    }
    if( found_space && found_word )
    {
      if( ncp.crsChar && ncp.crsChar < LL-1 ) ncp.crsChar++;
    }
  }
  return found_space && found_word;
}

// Returns true if found end of word, else false
// 1. If at end of word, or end of non-word, move to next char
// 2. If on white space, skip past white space
// 3. If on word, go to end of word
// 4. If on non-white-non-word, go to end of non-white-non-word
bool View::Imp::GoToEndOfWord_GetPosition( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m_fb.NumLines();
  if( 0==NUM_LINES ) return false;

  const int      CL = CrsLine(); // Cursor line
  const unsigned LL = m_fb.LineLen( CL );
        unsigned CP = CrsChar(); // Cursor position

  // At end of line, or line too short:
  if( (LL-1) <= CP || LL < 2 ) return false;

  int CC = m_fb.Get( CL, CP );   // Current char
  int NC = m_fb.Get( CL, CP+1 ); // Next char

  // 1. If at end of word, or end of non-word, move to next char
  if( (IsWord_Ident   ( CC ) && !IsWord_Ident   ( NC ))
   || (IsWord_NonIdent( CC ) && !IsWord_NonIdent( NC )) ) CP++;

  // 2. If on white space, skip past white space
  if( IsSpace( m_fb.Get(CL, CP) ) )
  {
    for( ; CP<LL && IsSpace( m_fb.Get(CL, CP) ); CP++ ) ;
    if( LL <= CP ) return false; // Did not find non-white space
  }
  // At this point (CL,CP) should be non-white space
  CC = m_fb.Get( CL, CP );  // Current char

  ncp.crsLine = CL;

  if( IsWord_Ident( CC ) ) // On identity
  {
    // 3. If on word space, go to end of word space
    for( ; CP<LL && IsWord_Ident( m_fb.Get(CL, CP) ); CP++ )
    {
      ncp.crsChar = CP;
    }
  }
  else if( IsWord_NonIdent( CC ) )// On Non-identity, non-white space
  {
    // 4. If on non-white-non-word, go to end of non-white-non-word
    for( ; CP<LL && IsWord_NonIdent( m_fb.Get(CL, CP) ); CP++ )
    {
      ncp.crsChar = CP;
    }
  }
  else { // Should never get here:
    return false;
  }
  return true;
}

void View::Imp::
     GoToOppositeBracket_Forward( const char ST_C, const char FN_C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m_fb.NumLines();
  const unsigned CL = CrsLine();
  const unsigned CC = CrsChar();

  // Search forward
  unsigned level = 0;
  bool     found = false;

  for( unsigned l=CL; !found && l<NUM_LINES; l++ )
  {
    const unsigned LL = m_fb.LineLen( l );

    for( unsigned p=(CL==l)?(CC+1):0; !found && p<LL; p++ )
    {
      const char C = m_fb.Get( l, p );

      if     ( C==ST_C ) level++;
      else if( C==FN_C )
      {
        if( 0 < level ) level--;
        else {
          found = true;

          GoToCrsPos_Write( l, p );
        }
      }
    }
  }
}

void View::Imp::
     GoToOppositeBracket_Backward( const char ST_C, const char FN_C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const int CL = CrsLine();
  const int CC = CrsChar();

  // Search forward
  unsigned level = 0;
  bool     found = false;

  for( int l=CL; !found && 0<=l; l-- )
  {
    const unsigned LL = m_fb.LineLen( l );

    for( int p=(CL==l)?(CC-1):(LL-1); !found && 0<=p; p-- )
    {
      const char C = m_fb.Get( l, p );

      if     ( C==ST_C ) level++;
      else if( C==FN_C )
      {
        if( 0 < level ) level--;
        else {
          found = true;

          GoToCrsPos_Write( l, p );
        }
      }
    }
  }
}

void View::Imp::GoToCrsPos_Write_Visual( const unsigned OCL
                                       , const unsigned OCP
                                       , const unsigned NCL
                                       , const unsigned NCP )
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
  m_crsRow = NCL - m_topLine;
  m_crsCol = NCP - m_leftChar;
  Console::Update();
  PrintCursor();
  m_sts_line_needs_update = true;
}

// Cursor is moving forward
// Write out from (OCL,OCP) up to but not including (NCL,NCP)
void View::Imp::GoToCrsPos_WV_Forward( const unsigned OCL
                                     , const unsigned OCP
                                     , const unsigned NCL
                                     , const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( OCL == NCL ) // Only one line:
  {
    for( unsigned k=OCP; k<NCP; k++ )
    {
      int byte = m_fb.Get( OCL, k );
      Console::Set( Line_2_GL( OCL ), Char_2_GL( k ), byte, Get_Style(OCL,k) );
    }
  }
  else { // Multiple lines
    // Write out first line:
    const unsigned OCLL = m_fb.LineLen( OCL ); // Old cursor line length
    const unsigned END_FIRST_LINE = Min( RightChar()+1, OCLL );
    for( unsigned k=OCP; k<END_FIRST_LINE; k++ )
    {
      int byte = m_fb.Get( OCL, k );
      Console::Set( Line_2_GL( OCL ), Char_2_GL( k ), byte, Get_Style(OCL,k) );
    }
    // Write out intermediate lines:
    for( unsigned l=OCL+1; l<NCL; l++ )
    {
      const unsigned LL = m_fb.LineLen( l ); // Line length
      const unsigned END_OF_LINE = Min( RightChar()+1, LL );
      for( unsigned k=m_leftChar; k<END_OF_LINE; k++ )
      {
        int byte = m_fb.Get( l, k );
        Console::Set( Line_2_GL( l ), Char_2_GL( k ), byte, Get_Style(l,k) );
      }
    }
    // Write out last line:
    // Print from beginning of next line to new cursor position:
    const unsigned NCLL = m_fb.LineLen( NCL ); // Line length
    const unsigned END = Min( NCLL, NCP );
    for( unsigned k=m_leftChar; k<END; k++ )
    {
      int byte = m_fb.Get( NCL, k );
      Console::Set( Line_2_GL( NCL ), Char_2_GL( k ), byte, Get_Style(NCL,k)  );
    }
  }
}

// Cursor is moving backwards
// Write out from (OCL,OCP) back to but not including (NCL,NCP)
void View::Imp::GoToCrsPos_WV_Backward( const unsigned OCL
                                      , const unsigned OCP
                                      , const unsigned NCL
                                      , const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( OCL == NCL ) // Only one line:
  {
    const unsigned LL = m_fb.LineLen( OCL ); // Line length
    if( LL ) {
      const unsigned START = Min( OCP, LL-1 );
      for( unsigned k=START; NCP<k; k-- )
      {
        int byte = m_fb.Get( OCL, k );
        Console::Set( Line_2_GL( OCL ), Char_2_GL( k ), byte, Get_Style(OCL,k) );
      }
    }
  }
  else { // Multiple lines
    // Write out first line:
    const unsigned OCLL = m_fb.LineLen( OCL ); // Old cursor line length
    if( OCLL ) {
      for( int k=Min(OCP,OCLL-1); static_cast<int>(m_leftChar)<=k; k-- )
      {
        int byte = m_fb.Get( OCL, k );
        Console::Set( Line_2_GL( OCL ), Char_2_GL( k ), byte, Get_Style(OCL,k) );
      }
    }
    // Write out intermediate lines:
    for( unsigned l=OCL-1; NCL<l; l-- )
    {
      const unsigned LL = m_fb.LineLen( l ); // Line length
      if( LL ) {
        const unsigned END_OF_LINE = Min( RightChar(), LL-1 );
        for( int k=END_OF_LINE; static_cast<int>(m_leftChar)<=k; k-- )
        {
          int byte = m_fb.Get( l, k );
          Console::Set( Line_2_GL( l ), Char_2_GL( k ), byte, Get_Style(l,k) );
        }
      }
    }
    // Write out last line:
    // Go down to beginning of last line:
    const unsigned NCLL = m_fb.LineLen( NCL ); // New cursor line length
    if( NCLL ) {
      const unsigned END_LAST_LINE = Min( RightChar(), NCLL-1 );

      // Print from beginning of next line to new cursor position:
      for( int k=END_LAST_LINE; static_cast<int>(NCP)<=k; k-- )
      {
        int byte = m_fb.Get( NCL, k );
        Console::Set( Line_2_GL( NCL ), Char_2_GL( k ), byte, Get_Style(NCL,k) );
      }
    }
  }
}

void View::Imp::GoToCrsPos_Write_VisualBlock( const unsigned OCL
                                            , const unsigned OCP
                                            , const unsigned NCL
                                            , const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // v_fn_line == NCL && v_fn_char == NCP, so dont need to include
  // v_fn_line       and v_fn_char in Min and Max calls below:
  const unsigned vis_box_left = Min( v_st_char, Min( OCP, NCP ) );
  const unsigned vis_box_rite = Max( v_st_char, Max( OCP, NCP ) );
  const unsigned vis_box_top  = Min( v_st_line, Min( OCL, NCL ) );
  const unsigned vis_box_bot  = Max( v_st_line, Max( OCL, NCL ) );

  const unsigned draw_box_left = Max( m_leftChar   , vis_box_left );
  const unsigned draw_box_rite = Min( RightChar(), vis_box_rite );
  const unsigned draw_box_top  = Max( m_topLine    , vis_box_top  );
  const unsigned draw_box_bot  = Min( BotLine()  , vis_box_bot  );

  for( unsigned l=draw_box_top; l<=draw_box_bot; l++ )
  {
    const unsigned LL = m_fb.LineLen( l );

    for( unsigned k=draw_box_left; k<LL && k<=draw_box_rite; k++ )
    {
      // On some terminals, the cursor on reverse video on white space does not
      // show up, so to prevent that, do not reverse video the cursor position:
      const int   byte  = m_fb.Get ( l, k );
      const Style style = Get_Style( l, k );

      if( NCL==l && NCP==k )
      {
        if( RV_Style( style ) )
        {
          const Style NonRV_style = RV_Style_2_NonRV( style );

          Console::Set( Line_2_GL( l ), Char_2_GL( k ), byte, NonRV_style );
        }
      }
      else {
        Console::Set( Line_2_GL( l ), Char_2_GL( k ), byte, style );
      }
    }
  }
  m_crsRow = NCL - m_topLine;
  m_crsCol = NCP - m_leftChar;
  Console::Update();
  PrintCursor();
  m_sts_line_needs_update = true;
}

// This one works better when IN visual mode:
void View::Imp::PageDown_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m_fb.NumLines();

  if( 0<NUM_LINES )
  {
    const unsigned OCLd = CrsLine(); // Old cursor line

    unsigned NCLd = OCLd + WorkingRows() - 1; // New cursor line

    // Dont let cursor go past the end of the file:
    if( NUM_LINES-1 < NCLd ) NCLd = NUM_LINES-1;

    GoToCrsPos_Write( NCLd, 0 );
  }
}

void View::Imp::PageUp_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m_fb.NumLines();

  if( 0<NUM_LINES )
  {
    const unsigned OCL = CrsLine(); // Old cursor line

    int NCL = OCL - WorkingRows() + 1; // New cursor line

    // Check for underflow:
    if( NCL < 0 ) NCL = 0;

    GoToCrsPos_Write( NCL, 0 );
  }
}

bool View::Imp::RV_Style( const Style s ) const
{
  return s == S_RV_NORMAL
      || s == S_RV_STAR
      || s == S_RV_DEFINE
      || s == S_RV_COMMENT
      || s == S_RV_CONST
      || s == S_RV_CONTROL
      || s == S_RV_VARTYPE;
}

Style View::Imp::RV_Style_2_NonRV( const Style RVS ) const
{
  Style s = S_NORMAL;

  if     ( RVS == S_RV_STAR    ) s = S_STAR   ;
  else if( RVS == S_RV_DEFINE  ) s = S_DEFINE ;
  else if( RVS == S_RV_COMMENT ) s = S_COMMENT;
  else if( RVS == S_RV_CONST   ) s = S_CONST  ;
  else if( RVS == S_RV_CONTROL ) s = S_CONTROL;
  else if( RVS == S_RV_VARTYPE ) s = S_VARTYPE;

  return s;
}

Style View::Imp::Get_Style( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Style s = S_NORMAL;

  if( InVisualArea( line, pos ) )
  {
    s = S_RV_NORMAL;

    if     ( InStar    ( line, pos ) ) s = S_RV_STAR;
    else if( InDefine  ( line, pos ) ) s = S_RV_DEFINE;
    else if( InComment ( line, pos ) ) s = S_RV_COMMENT;
    else if( InConst   ( line, pos ) ) s = S_RV_CONST;
    else if( InControl ( line, pos ) ) s = S_RV_CONTROL;
    else if( InVarType ( line, pos ) ) s = S_RV_VARTYPE;
    else if( InNonAscii( line, pos ) ) s = S_RV_NONASCII;
  }
  else if( InStar    ( line, pos ) ) s = S_STAR;
  else if( InDefine  ( line, pos ) ) s = S_DEFINE;
  else if( InComment ( line, pos ) ) s = S_COMMENT;
  else if( InConst   ( line, pos ) ) s = S_CONST;
  else if( InControl ( line, pos ) ) s = S_CONTROL;
  else if( InVarType ( line, pos ) ) s = S_VARTYPE;
  else if( InNonAscii( line, pos ) ) s = S_NONASCII;

  return s;
}

bool View::Imp::Do_n_FindNextPattern( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m_fb.NumLines();
//const unsigned STAR_LEN  = m_vis.star.len();

  const unsigned OCL = CrsLine();
  const unsigned OCC = CrsChar();

  unsigned st_l = OCL;
  unsigned st_c = OCC;

  bool found_next_star = false;

  // Move past current star:
  const unsigned LL = m_fb.LineLen( OCL );

  for( ; st_c<LL && InStar(OCL,st_c); st_c++ ) ;

  // Go down to next line
  if( LL <= st_c ) { st_c=0; st_l++; }

  // Search for first star position past current position
  for( unsigned l=st_l; !found_next_star && l<NUM_LINES; l++ )
  {
    const unsigned LL = m_fb.LineLen( l );

    for( unsigned p=st_c
       ; !found_next_star && p<LL
       ; p++ )
    {
      if( InStar(l,p) )
      {
        found_next_star = true;
        ncp.crsLine = l;
        ncp.crsChar = p;
      }
    }
    // After first line, always start at beginning of line
    st_c = 0;
  }
  // Near end of file and did not find any patterns, so go to first pattern in file
  if( !found_next_star )
  {
    for( unsigned l=0; !found_next_star && l<=OCL; l++ )
    {
      const unsigned LL = m_fb.LineLen( l );
      const unsigned END_C = (OCL==l) ? Min( OCC, LL ) : LL;

      for( unsigned p=0; !found_next_star && p<END_C; p++ )
      {
        if( InStar(l,p) )
        {
          found_next_star = true;
          ncp.crsLine = l;
          ncp.crsChar = p;
        }
      }
    }
  }
  return found_next_star;
}

bool View::Imp::Do_N_FindPrevPattern( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();

  const unsigned NUM_LINES = m_fb.NumLines();
//const unsigned STAR_LEN  = m_vis.star.len();

  const unsigned OCL = CrsLine();
  const unsigned OCC = CrsChar();

  bool found_prev_star = false;

  // Search for first star position before current position
  for( int l=OCL; !found_prev_star && 0<=l; l-- )
  {
    const int LL = m_fb.LineLen( l );

    int p=LL-1;
    if( OCL==l ) p = OCC ? OCC-1 : 0;

    for( ; 0<p && !found_prev_star; p-- )
    {
      for( ; 0<=p && InStar(l,p); p-- )
      {
        found_prev_star = true;
        ncp.crsLine = l;
        ncp.crsChar = p;
      }
    }
  }
  // Near beginning of file and did not find any patterns, so go to last pattern in file
  if( !found_prev_star )
  {
    for( int l=NUM_LINES-1; !found_prev_star && OCL<l; l-- )
    {
      const unsigned LL = m_fb.LineLen( l );

      int p=LL-1;
      if( OCL==l ) p = OCC ? OCC-1 : 0;

      for( ; 0<p && !found_prev_star; p-- )
      {
        for( ; 0<=p && InStar(l,p); p-- )
        {
          found_prev_star = true;
          ncp.crsLine = l;
          ncp.crsChar = p;
        }
      }
    }
  }
  return found_prev_star;
}

void View::Imp::Do_x_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_inVisualBlock )
  {
    Do_x_range_block( v_st_line, v_st_char, v_fn_line, v_fn_char );
  }
  else {
    Do_x_range( v_st_line, v_st_char, v_fn_line, v_fn_char );
  }
  Remove_Banner();
}

void View::Imp::Do_s_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Need to know if cursor is at end of line before Do_x_v() is called:
  const unsigned LL = m_fb.LineLen( CrsLine() );
  const bool CURSOR_AT_END_OF_LINE = LL ? CrsChar() == LL-1 : false;

  Do_x_v();

  if( m_inVisualBlock )
  {
    if( CURSOR_AT_END_OF_LINE ) Do_a_vb();
    else                        Do_i_vb(); 
  }
  else {
    if( CURSOR_AT_END_OF_LINE ) Do_a();
    else                        Do_i();
  }
}

void View::Imp::Do_i_vb()
{
  Trace trace( __PRETTY_FUNCTION__ );
  m_inInsertMode = true;
  DisplayBanner();

  unsigned count = 0;
  for( char c=m_key.In(); c != ESC; c=m_key.In() )
  {
    if( IsEndOfLineDelim( c ) )
    {
      ; // Ignore end of line delimiters
    }
    else if( BS  == c || DEL == c )
    {
      if( count )
      {
        InsertBackspace_vb();
        count--;
        m_fb.Update();
      }
    }
    else {
      InsertAddChar_vb( c );
      count++;
      m_fb.Update();
    }
  }
  Remove_Banner();
  m_inInsertMode = false;
}

void View::Imp::Do_a_vb()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CL = CrsLine();
  const unsigned LL = m_fb.LineLen( CL );
  if( 0==LL ) { Do_i_vb(); return; }

  const bool CURSOR_AT_EOL = ( CrsChar() == LL-1 );
  if( CURSOR_AT_EOL )
  {
    GoToCrsPos_NoWrite( CL, LL );
  }
  const bool CURSOR_AT_RIGHT_COL = ( m_crsCol == WorkingCols()-1 );

  if( CURSOR_AT_RIGHT_COL )
  {
    // Only need to scroll window right, and then enter insert i:
    m_leftChar++; //< This increments CrsChar()
  }
  else if( !CURSOR_AT_EOL ) // If cursor was at EOL, already moved cursor forward
  {
    // Only need to move cursor right, and then enter insert i:
    m_crsCol += 1; //< This increments CrsChar()
  }
  m_fb.Update();

  Do_i_vb();
}

void View::Imp::InsertBackspace_vb()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = CrsLine();  // Old cursor line
  const unsigned OCP = CrsChar();  // Old cursor position

  if( OCP )
  {
    const unsigned N_REG_LINES = m_reg.len();

    for( unsigned k=0; k<N_REG_LINES; k++ )
    {
      m_fb.RemoveChar( OCL+k, OCP-1 );
    }
    GoToCrsPos_NoWrite( OCL, OCP-1 );
  }
}

void View::Imp::InsertAddChar_vb( const char c )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = CrsLine();  // Old cursor line
  const unsigned OCP = CrsChar();  // Old cursor position

  const unsigned N_REG_LINES = m_reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    const unsigned LL = m_fb.LineLen( OCL+k );

    if( LL < OCP )
    {
      // Fill in line with white space up to OCP:
      for( unsigned i=0; i<(OCP-LL); i++ )
      {
        // Insert at end of line so undo will be atomic:
        const unsigned NLL = m_fb.LineLen( OCL+k ); // New line length
        m_fb.InsertChar( OCL+k, NLL, ' ' );
      }
    }
    m_fb.InsertChar( OCL+k, OCP, c );
  }
  GoToCrsPos_NoWrite( OCL, OCP+1 );
}

bool View::Imp::Do_dw_get_fn( const int st_line, const int st_char
                            , unsigned& fn_line, unsigned& fn_char )
{
  const unsigned LL = m_fb.LineLen( st_line );
  const uint8_t  C  = m_fb.Get( st_line, st_char );

  if( IsSpace( C )      // On white space
    || ( st_char < LL-1 // On non-white space before white space
     //&& NotSpace( C )
       && IsSpace( m_fb.Get( st_line, st_char+1 ) ) ) )
  {
    // w:
    CrsPos ncp_w = { 0, 0 };
    bool ok = GoToNextWord_GetPosition( ncp_w );
    if( ok && 0 < ncp_w.crsChar ) ncp_w.crsChar--;
    if( ok && st_line == ncp_w.crsLine
           && st_char <= ncp_w.crsChar )
    {
      fn_line = ncp_w.crsLine;
      fn_char = ncp_w.crsChar;
      return true;
    }
  }
  // if not on white space, and
  // not on non-white space before white space,
  // or fell through, try e:
  CrsPos ncp_e = { 0, 0 };
  bool ok = GoToEndOfWord_GetPosition( ncp_e );

  if( ok && st_line == ncp_e.crsLine
         && st_char <= ncp_e.crsChar )
  {
    fn_line = ncp_e.crsLine;
    fn_char = ncp_e.crsChar;
    return true;
  }
  return false;
}

void View::Imp::Do_Tilda_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( v_fn_line < v_st_line ) Swap( v_st_line, v_fn_line );
  if( v_fn_char < v_st_char ) Swap( v_st_char, v_fn_char );

  if( m_inVisualBlock ) Do_Tilda_v_block();
  else                  Do_Tilda_v_st_fn();

  m_inVisualMode = false;
  Remove_Banner();
  Undo_v(); //<- This will cause the tilda'ed characters to be redrawn
}

void View::Imp::Do_Tilda_v_st_fn()
{
  for( unsigned L = v_st_line; L<=v_fn_line; L++ )
  {
    const unsigned LL = m_fb.LineLen( L );
    const unsigned P_st = (L==v_st_line) ? v_st_char : 0;
    const unsigned P_fn = (L==v_fn_line) ? v_fn_char : LL-1;

    for( unsigned P = P_st; P <= P_fn; P++ )
    {
      char c = m_fb.Get( L, P );
      bool changed = false;
      if     ( isupper( c ) ) { c = tolower( c ); changed = true; }
      else if( islower( c ) ) { c = toupper( c ); changed = true; }
      if( changed ) m_fb.Set( L, P, c );
    }
  }
}

void View::Imp::Do_Tilda_v_block()
{
  for( unsigned L = v_st_line; L<=v_fn_line; L++ )
  {
    const unsigned LL = m_fb.LineLen( L );

    for( unsigned P = v_st_char; P<LL && P <= v_fn_char; P++ )
    {
      char c = m_fb.Get( L, P );
      bool changed = false;
      if     ( isupper( c ) ) { c = tolower( c ); changed = true; }
      else if( islower( c ) ) { c = toupper( c ); changed = true; }
      if( changed ) m_fb.Set( L, P, c );
    }
  }
}

void View::Imp::Do_D_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_inVisualBlock )
  {
    Do_x_range_block( v_st_line, v_st_char, v_fn_line, v_fn_char );
    Remove_Banner();
  }
  else {
    Do_D_v_line();
  }
}

void View::Imp::Do_D_v_line()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( v_fn_line < v_st_line ) Swap( v_st_line, v_fn_line );
  if( v_fn_char < v_st_char ) Swap( v_st_char, v_fn_char );

  m_reg.clear();

  bool removed_line = false;
  // 1. If v_st_line==0, fn_line will go negative in the loop below,
  //    so use int's instead of unsigned's
  // 2. Dont remove all lines in file to avoid crashing
  int fn_line = v_fn_line;
  for( int L = v_st_line; 1 < m_fb.NumLines() && L<=fn_line; fn_line-- )
  {
    Line* lp = m_fb.RemoveLineP( L );
    m_reg.push( lp );

    // m_reg will delete nlp
    removed_line = true;
  }
  m_vis.SetPasteMode( PM_LINE );

  m_inVisualMode = false;
  Remove_Banner();
  // D'ed lines will be removed, so no need to Undo_v()

  if( removed_line )
  {
    // Figure out and move to new cursor position:
    const unsigned NUM_LINES = m_fb.NumLines();
    const unsigned OCL       = CrsLine(); // Old cursor line

    unsigned ncl = v_st_line;
    if( NUM_LINES-1 < ncl ) ncl = v_st_line ? v_st_line-1 : 0;

    const unsigned NCLL = m_fb.LineLen( ncl );
    unsigned ncc = 0;
    if( NCLL ) ncc = v_st_char < NCLL ? v_st_char : NCLL-1;

    GoToCrsPos_NoWrite( ncl, ncc );

    m_fb.Update();
  }
}

void View::Imp::Do_dd_BufferEditor( const unsigned ONL )
{
  const unsigned OCL = CrsLine(); // Old cursor line

  // Can only delete one of the user files out of buffer editor
  if( CMD_FILE < OCL )
  {
    Line* lp = m_fb.GetLineP( OCL );

    const char* fname = lp->c_str( 0 );

    if( !m_vis.File_Is_Displayed( fname ) )
    {
      m_vis.ReleaseFileName( fname );

      Do_dd_Normal( ONL );
    }
  }
}

void View::Imp::Do_dd_Normal( const unsigned ONL )
{
  const unsigned OCL = CrsLine();           // Old cursor line
  const unsigned OCP = CrsChar();           // Old cursor position
  const unsigned OLL = m_fb.LineLen( OCL ); // Old line length

  const bool DELETING_LAST_LINE = OCL == ONL-1;

  const unsigned NCL = DELETING_LAST_LINE ? OCL-1 : OCL; // New cursor line
  const unsigned NLL = DELETING_LAST_LINE ? m_fb.LineLen( NCL )
                                          : m_fb.LineLen( NCL + 1 );
  const unsigned NCP = Min( OCP, 0<NLL ? NLL-1 : 0 );

  // Remove line from FileBuf and save in paste register:
  Line* lp = m_fb.RemoveLineP( OCL );
  if( lp ) {
    // m_reg will own nlp
    m_reg.clear();
    m_reg.push( lp );
    m_vis.SetPasteMode( PM_LINE );
  }
  GoToCrsPos_NoWrite( NCL, NCP );

  m_fb.Update();
}

void View::Imp::Do_y_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_reg.clear();

  if( m_inVisualBlock ) Do_y_v_block();
  else                  Do_y_v_st_fn();
}

void View::Imp::Do_y_v_st_fn()
{
  Trace trace( __PRETTY_FUNCTION__ );

  unsigned m_v_st_line = v_st_line;  unsigned m_v_st_char = v_st_char;
  unsigned m_v_fn_line = v_fn_line;  unsigned m_v_fn_char = v_fn_char;

  if( m_v_fn_line < m_v_st_line ) Swap( m_v_st_line, m_v_fn_line );
  if( m_v_fn_char < m_v_st_char ) Swap( m_v_st_char, m_v_fn_char );

  for( unsigned L=m_v_st_line; L<=m_v_fn_line; L++ )
  {
    Line* nlp = m_vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = m_fb.LineLen( L );
    if( LL ) {
      const unsigned P_st = (L==m_v_st_line) ? m_v_st_char : 0;
      const unsigned P_fn = (L==m_v_fn_line) ? Min(LL-1,m_v_fn_char) : LL-1;

      for( unsigned P = P_st; P <= P_fn; P++ )
      {
        nlp->push(__FILE__,__LINE__, m_fb.Get( L, P ) );
      }
    }
    // m_reg will delete nlp
    m_reg.push( nlp );
  }
  m_vis.SetPasteMode( PM_ST_FN );
}

void View::Imp::Do_y_v_block()
{
  Trace trace( __PRETTY_FUNCTION__ );

  unsigned m_v_st_line = v_st_line;  unsigned m_v_st_char = v_st_char;
  unsigned m_v_fn_line = v_fn_line;  unsigned m_v_fn_char = v_fn_char;

  if( m_v_fn_line < m_v_st_line ) Swap( m_v_st_line, m_v_fn_line );
  if( m_v_fn_char < m_v_st_char ) Swap( m_v_st_char, m_v_fn_char );

  for( unsigned L=m_v_st_line; L<=m_v_fn_line; L++ )
  {
    Line* nlp = m_vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = m_fb.LineLen( L );

    for( unsigned P = m_v_st_char; P<LL && P <= m_v_fn_char; P++ )
    {
      nlp->push(__FILE__,__LINE__, m_fb.Get( L, P ) );
    }
    // m_reg will delete nlp
    m_reg.push( nlp );
  }
  m_vis.SetPasteMode( PM_BLOCK );

  // Try to put cursor at (v_st_line, v_st_char), but
  // make sure the cursor is in bounds after the deletion:
  const unsigned NUM_LINES = m_fb.NumLines();
  unsigned ncl = v_st_line;
  if( NUM_LINES <= ncl ) ncl = NUM_LINES-1;
  const unsigned NLL = m_fb.LineLen( ncl );
  unsigned ncc = 0;
  if( NLL ) ncc = NLL <= v_st_char ? NLL-1 : v_st_char;

  GoToCrsPos_NoWrite( ncl, ncc );
}

void View::Imp::Do_Y_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m_reg.clear();

  if( m_inVisualBlock ) Do_y_v_block();
  else                  Do_Y_v_st_fn();
}

void View::Imp::Do_Y_v_st_fn()
{
  unsigned m_v_st_line = v_st_line;
  unsigned m_v_fn_line = v_fn_line;

  if( m_v_fn_line < m_v_st_line ) Swap( m_v_st_line, m_v_fn_line );

  for( unsigned L=m_v_st_line; L<=m_v_fn_line; L++ )
  {
    Line* nlp = m_vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = m_fb.LineLen(L);

    if( LL )
    {
      for( unsigned P = 0; P <= LL-1; P++ )
      {
        nlp->push(__FILE__,__LINE__, m_fb.Get( L, P ) );
      }
    }
    // m_reg will delete nlp
    m_reg.push( nlp );
  }
  m_vis.SetPasteMode( PM_LINE );
}

void View::Imp::Do_p_line()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = CrsLine();  // Old cursor line

  const unsigned NUM_LINES = m_reg.len();

  for( unsigned k=0; k<NUM_LINES; k++ )
  {
    // Put reg on line below:
    m_fb.InsertLine( OCL+k+1, *(m_reg[k]) );
  }
  // Update current view after other views,
  // so that the cursor will be put back in place
  m_fb.Update();
}

void View::Imp::Do_p_or_P_st_fn( Paste_Pos paste_pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned N_REG_LINES = m_reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    const unsigned NLL = m_reg[k]->len();  // New line length
    const unsigned OCL = CrsLine();               // Old cursor line

    if( 0 == k ) // Add to current line
    {
      MoveInBounds();
      const unsigned OLL = m_fb.LineLen( OCL );
      const unsigned OCP = CrsChar();               // Old cursor position

      // If line we are pasting to is zero length, dont paste a space forward
    //const unsigned add_pos = OLL ? 1 : 0;
      const unsigned forward = OLL ? ( paste_pos==PP_After ? 1 : 0 ) : 0;

      for( unsigned i=0; i<NLL; i++ )
      {
        uint8_t C = m_reg[k]->get(i);

        m_fb.InsertChar( OCL, OCP+i+forward, C );
      }
      if( 1 < N_REG_LINES && OCP+forward < OLL ) // Move rest of first line onto new line below
      {
        m_fb.InsertLine( OCL+1 );
        for( unsigned i=0; i<(OLL-OCP-forward); i++ )
        {
          uint8_t C = m_fb.RemoveChar( OCL, OCP + NLL+forward );
          m_fb.PushChar( OCL+1, C );
        }
      }
    }
    else if( N_REG_LINES-1 == k )
    {
      // Insert a new line if at end of file:
      if( m_fb.NumLines() == OCL+k ) m_fb.InsertLine( OCL+k );

      for( unsigned i=0; i<NLL; i++ )
      {
        uint8_t C = m_reg[k]->get(i);

        m_fb.InsertChar( OCL+k, i, C );
      }
    }
    else {
      // Put reg on line below:
      m_fb.InsertLine( OCL+k, *(m_reg[k]) );
    }
  }
  // Update current view after other views, so that the cursor will be put back in place
  m_fb.Update();
}

void View::Imp::Do_p_block()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = CrsLine();           // Old cursor line
  const unsigned OCP = CrsChar();           // Old cursor position
  const unsigned OLL = m_fb.LineLen( OCL ); // Old line length
  const unsigned ISP = OCP ? OCP+1          // Insert position
                           : ( OLL ? 1:0 ); // If at beginning of line,
                                            // and LL is zero insert at 0,
                                            // else insert at 1
  const unsigned N_REG_LINES = m_reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    if( m_fb.NumLines()<=OCL+k ) m_fb.InsertLine( OCL+k );

    const unsigned LL = m_fb.LineLen( OCL+k );

    if( LL < ISP )
    {
      // Fill in line with white space up to ISP:
      for( unsigned i=0; i<(ISP-LL); i++ )
      {
        // Insert at end of line so undo will be atomic:
        const unsigned NLL = m_fb.LineLen( OCL+k ); // New line length
        m_fb.InsertChar( OCL+k, NLL, ' ' );
      }
    }
    Line& reg_line = *(m_reg[k]);
    const unsigned RLL = reg_line.len();

    for( unsigned i=0; i<RLL; i++ )
    {
      uint8_t C = reg_line.get(i);

      m_fb.InsertChar( OCL+k, ISP+i, C );
    }
  }
  m_fb.Update();
}

void View::Imp::Do_P_line()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = CrsLine();  // Old cursor line

  const unsigned NUM_LINES = m_reg.len();

  for( unsigned k=0; k<NUM_LINES; k++ )
  {
    // Put reg on line above:
    m_fb.InsertLine( OCL+k, *(m_reg[k]) );
  }
  m_fb.Update();
}

void View::Imp::Do_P_block()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = CrsLine();  // Old cursor line
  const unsigned OCP = CrsChar();  // Old cursor position

  const unsigned N_REG_LINES = m_reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    if( m_fb.NumLines()<=OCL+k ) m_fb.InsertLine( OCL+k );

    const unsigned LL = m_fb.LineLen( OCL+k );

    if( LL < OCP )
    {
      // Fill in line with white space up to OCP:
      for( unsigned i=0; i<(OCP-LL); i++ ) m_fb.InsertChar( OCL+k, LL, ' ' );
    }
    Line& reg_line = *(m_reg[k]);
    const unsigned RLL = reg_line.len();

    for( unsigned i=0; i<RLL; i++ )
    {
      uint8_t C = reg_line.get(i);

      m_fb.InsertChar( OCL+k, OCP+i, C );
    }
  }
  m_fb.Update();
}

void View::Imp::Do_x_range( unsigned st_line, unsigned st_char
                          , unsigned fn_line, unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Do_x_range_pre( st_line, st_char, fn_line, fn_char );

  if( st_line == fn_line )
  {
    Do_x_range_single( st_line, st_char, fn_char );
  }
  else {
    Do_x_range_multiple( st_line, st_char, fn_line, fn_char );
  }
  Do_x_range_post( st_line, st_char );
}

void View::Imp::Do_x_range_block( unsigned st_line, unsigned st_char
                                , unsigned fn_line, unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Do_x_range_pre( st_line, st_char, fn_line, fn_char );

  for( unsigned L = st_line; L<=fn_line; L++ )
  {
    Line* nlp = m_vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = m_fb.LineLen( L );

    for( unsigned P = st_char; P<LL && P <= fn_char; P++ )
    {
      nlp->push(__FILE__,__LINE__, m_fb.RemoveChar( L, st_char ) );
    }
    m_reg.push( nlp );
  }
  Do_x_range_post( st_line, st_char );
}

void View::Imp::Do_x_range_pre( unsigned& st_line, unsigned& st_char
                              , unsigned& fn_line, unsigned& fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_inVisualBlock )
  {
    if( fn_line < st_line ) Swap( st_line, fn_line );
    if( fn_char < st_char ) Swap( st_char, fn_char );
  }
  else {
    if( fn_line < st_line
     || (fn_line == st_line && fn_char < st_char) )
    {
      Swap( st_line, fn_line );
      Swap( st_char, fn_char );
    }
  }
  m_reg.clear();
}

void View::Imp::Do_x_range_post( const unsigned st_line
                               , const unsigned st_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m_inVisualBlock ) m_vis.SetPasteMode( PM_BLOCK );
  else                  m_vis.SetPasteMode( PM_ST_FN );

  // Try to put cursor at (st_line, st_char), but
  // make sure the cursor is in bounds after the deletion:
  const unsigned NUM_LINES = m_fb.NumLines();
  unsigned ncl = st_line;
  if( NUM_LINES <= ncl ) ncl = NUM_LINES-1;
  const unsigned NLL = m_fb.LineLen( ncl );
  unsigned ncc = 0;
  if( NLL ) ncc = NLL <= st_char ? NLL-1 : st_char;

  GoToCrsPos_NoWrite( ncl, ncc );

  m_inVisualMode = false;

  m_fb.Update(); //<- No need to Undo_v() or Remove_Banner() because of this
}

void View::Imp::Do_x_range_single( const unsigned L
                                 , const unsigned st_char
                                 , const unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OLL = m_fb.LineLen( L ); // Original line length

  if( 0<OLL )
  {
    Line* nlp = m_vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned P_st = Min( st_char, OLL-1 ); 
    const unsigned P_fn = Min( fn_char, OLL-1 );  

    unsigned LL = OLL;

    // Dont remove a single line, or else Q wont work right
    for( unsigned P = P_st; P_st < LL && P <= P_fn; P++ )
    {
      nlp->push(__FILE__,__LINE__, m_fb.RemoveChar( L, P_st ) );

      LL = m_fb.LineLen( L ); // Removed a char, so re-calculate LL
    }
    m_reg.push( nlp );
  }
}

void View::Imp::Do_x_range_multiple( const unsigned st_line
                                   , const unsigned st_char
                                   , const unsigned fn_line
                                   , const unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool started_in_middle = false;
  bool ended___in_middle = false;

  unsigned n_fn_line = fn_line; // New finish line

  for( unsigned L = st_line; L<=n_fn_line; )
  {
    Line* nlp = m_vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned OLL = m_fb.LineLen( L ); // Original line length

    const unsigned P_st = (L==  st_line) ? Min(st_char, OLL-1) : 0;
    const unsigned P_fn = (L==n_fn_line) ? Min(fn_char, OLL-1) : OLL-1;

    if(   st_line == L && 0    < P_st  ) started_in_middle = true;
    if( n_fn_line == L && P_fn < OLL-1 ) ended___in_middle = true;

    unsigned LL = OLL;

    for( unsigned P = P_st; P_st < LL && P <= P_fn; P++ )
    {
      nlp->push(__FILE__,__LINE__, m_fb.RemoveChar( L, P_st ) );

      LL = m_fb.LineLen( L ); // Removed a char, so re-calculate LL
    }
    if( 0 == P_st && OLL-1 == P_fn ) // Removed entire line
    {
      m_fb.RemoveLine( L );
      n_fn_line--;
    }
    else L++;

    m_reg.push( nlp );
  }
  if( started_in_middle && ended___in_middle )
  {
    Line* lp = m_fb.RemoveLineP( st_line+1 );
    m_fb.AppendLineToLine( st_line, lp );
  }
}

View::View( Vis& vis, Key& key, FileBuf& fb, LinesList& reg )
  : m( *new(__FILE__, __LINE__) Imp( vis, key, fb, *this, reg ) )
{
}

View::~View()
{
  delete &m;
}

FileBuf* View::GetFB() const { return &m.m_fb; }

unsigned View::WinCols() const { return m.WinCols(); }
unsigned View::WinRows() const { return m.WinRows(); }
unsigned View::X() const { return m.X(); }
unsigned View::Y() const { return m.Y(); }

unsigned View::GetTopLine () const { return m.GetTopLine (); }
unsigned View::GetLeftChar() const { return m.GetLeftChar(); }
unsigned View::GetCrsRow  () const { return m.GetCrsRow  (); }
unsigned View::GetCrsCol  () const { return m.GetCrsCol  (); }

void View::SetTopLine ( const unsigned val ) { m.SetTopLine ( val ); }
void View::SetLeftChar( const unsigned val ) { m.SetLeftChar( val ); }
void View::SetCrsRow  ( const unsigned val ) { m.SetCrsRow  ( val ); }
void View::SetCrsCol  ( const unsigned val ) { m.SetCrsCol  ( val ); }

unsigned View::WorkingRows() const { return m.WorkingRows(); }
unsigned View::WorkingCols() const { return m.WorkingCols(); }
unsigned View::CrsLine  () const { return m.CrsLine  (); }
unsigned View::BotLine  () const { return m.BotLine  (); }
unsigned View::CrsChar  () const { return m.CrsChar  (); }
unsigned View::RightChar() const { return m.RightChar(); }

unsigned View::Row_Win_2_GL( const unsigned win_row ) const
{
  return m.Row_Win_2_GL( win_row );
}

// Translates zero based working view column to zero based global column
unsigned View::Col_Win_2_GL( const unsigned win_col ) const
{
  return m.Col_Win_2_GL( win_col );
}

// Translates zero based file line number to zero based global row
unsigned View::Line_2_GL( const unsigned file_line ) const
{
  return m.Line_2_GL( file_line );
}

// Translates zero based file line char position to zero based global column
unsigned View::Char_2_GL( const unsigned line_char ) const
{
  return m.Char_2_GL( line_char );
}

unsigned View::Sts__Line_Row() const
{
  return m.Sts__Line_Row();
}

unsigned View::File_Line_Row() const
{
  return m.File_Line_Row();
}

unsigned View::Cmd__Line_Row() const
{
  return m.Cmd__Line_Row();
}

Tile_Pos View::GetTilePos() const
{
  return m.GetTilePos();
}

void View::SetTilePos( const Tile_Pos tp )
{
  m.SetTilePos( tp );
}

void View::SetViewPos()
{
  m.SetViewPos();
}

bool View::GetInsertMode() const { return m.GetInsertMode(); }
void View::SetInsertMode( const bool val ) { m.SetInsertMode( val ); }
bool View::GetReplaceMode() const { return m.GetReplaceMode(); }
void View::SetReplaceMode( const bool val ) { m.SetReplaceMode( val ); }

void View::GoToBegOfLine()
{
  m.GoToBegOfLine();
}

void View::GoToEndOfLine()
{
  m.GoToEndOfLine();
}

void View::GoToBegOfNextLine()
{
  m.GoToBegOfNextLine();
}

void View::GoToTopLineInView()
{
  m.GoToTopLineInView();
}

void View::GoToMidLineInView()
{
  m.GoToMidLineInView();
}

void View::GoToBotLineInView()
{
  m.GoToBotLineInView();
}

void View::GoToEndOfFile()
{
  m.GoToEndOfFile();
}

void View::GoToPrevWord()
{
  m.GoToPrevWord();
}

void View::GoToNextWord()
{
  m.GoToNextWord();
}

void View::GoToEndOfWord()
{
  m.GoToEndOfWord();
}

void View::GoToLine( const unsigned user_line_num )
{
  m.GoToLine( user_line_num );
}

void View::GoToStartOfRow()
{
  m.GoToStartOfRow();
}

void View::GoToEndOfRow()
{
  m.GoToEndOfRow();
}

void View::GoToTopOfFile()
{
  m.GoToTopOfFile();
}

void View::GoToOppositeBracket()
{
  m.GoToOppositeBracket();
}

void View::GoToLeftSquigglyBracket()
{
  m.GoToLeftSquigglyBracket();
}

void View::GoToRightSquigglyBracket()
{
  m.GoToRightSquigglyBracket();
}

void View::GoToCrsPos_NoWrite( const unsigned ncp_crsLine
                             , const unsigned ncp_crsChar )
{
  m.GoToCrsPos_NoWrite( ncp_crsLine, ncp_crsChar );
}

void View::GoToCrsPos_Write( const unsigned ncp_crsLine
                           , const unsigned ncp_crsChar )
{
  m.GoToCrsPos_Write( ncp_crsLine, ncp_crsChar );
}

bool View::GoToFile_GetFileName( String& fname )
{
  return m.GoToFile_GetFileName( fname );
}

void View::GoToCmdLineClear( const char* S )
{
  m.GoToCmdLineClear( S );
}

void View::GoUp()     { m.GoUp()    ; }
void View::GoDown()   { m.GoDown()  ; }
void View::GoLeft()   { m.GoLeft()  ; }
void View::GoRight()  { m.GoRight() ; }
void View::PageDown() { m.PageDown(); }
void View::PageUp()   { m.PageUp()  ; }

bool View::MoveInBounds()  { return m.MoveInBounds()        ; }
void View::MoveCurrLineToTop()    { m.MoveCurrLineToTop()   ; }
void View::MoveCurrLineCenter()   { m.MoveCurrLineCenter()  ; }
void View::MoveCurrLineToBottom() { m.MoveCurrLineToBottom(); }

void View::Do_i() { m.Do_i(); }
bool View::Do_v() { return m.Do_v(); }
bool View::Do_V() { return m.Do_V(); }
void View::Do_a() { m.Do_a(); }
void View::Do_A() { m.Do_A(); }
void View::Do_o() { m.Do_o(); }
void View::Do_O() { m.Do_O(); }
void View::Do_x() { m.Do_x(); }
void View::Do_s() { m.Do_s(); }
void View::Do_cw() { m.Do_cw(); }
void View::Do_D() { m.Do_D(); }
void View::Do_f( const char FAST_CHAR ) { m.Do_f( FAST_CHAR ); }
void View::Do_n() { m.Do_n(); }
void View::Do_N() { m.Do_N(); }
void View::Do_dd() { m.Do_dd(); }
int  View::Do_dw() { return m.Do_dw(); }
void View::Do_yy() { m.Do_yy(); }
void View::Do_yw() { m.Do_yw(); }
void View::Do_p() { m.Do_p(); }
void View::Do_P() { m.Do_P(); }
void View::Do_R() { m.Do_R(); }
void View::Do_J() { m.Do_J(); }
void View::Do_Tilda() { m.Do_Tilda(); }
void View::Do_u() { m.Do_u(); }
void View::Do_U() { m.Do_U(); }

bool View::InVisualArea( const unsigned line, const unsigned pos )
{
  return m.InVisualArea( line, pos );
}

bool View::InVisualStFn( const unsigned line, const unsigned pos )
{
  return m.InVisualStFn( line, pos );
}

bool View::InVisualBlock( const unsigned line, const unsigned pos )
{
  return m.InVisualBlock( line, pos );
}

bool View::InComment( const unsigned line, const unsigned pos )
{
  return m.InComment( line, pos );
}

bool View::InDefine( const unsigned line, const unsigned pos )
{
  return m.InDefine( line, pos );
}

bool View::InConst( const unsigned line, const unsigned pos )
{
  return m.InConst( line, pos );
}

bool View::InControl( const unsigned line, const unsigned pos )
{
  return m.InControl( line, pos );
}

bool View::InVarType( const unsigned line, const unsigned pos )
{
  return m.InVarType( line, pos );
}

bool View::InStar( const unsigned line, const unsigned pos )
{
  return m.InStar( line, pos );
}

bool View::InNonAscii( const unsigned line, const unsigned pos )
{
  return m.InNonAscii( line, pos );
}

void View::Update() { m.Update(); }
void View::RepositionView() { m.RepositionView(); }
void View::Print_Borders() { m.Print_Borders(); }
void View::PrintStsLine() { m.PrintStsLine(); }
void View::PrintFileLine() { m.PrintFileLine(); }
void View::PrintCmdLine() { m.PrintCmdLine(); }
void View::PrintWorkingView() { m.PrintWorkingView(); }

void View::PrintWorkingView_Set( const unsigned LL
                               , const unsigned G_ROW
                               , const unsigned col
                               , const unsigned i
                               , const unsigned byte
                               , const Style    s )
{
  m.PrintWorkingView_Set( LL, G_ROW, col, i, byte, s );
}

void View::PrintCursor() { m.PrintCursor(); }
void View::DisplayMapping() { m.DisplayMapping(); }

bool View::GetStsLineNeedsUpdate() const { return m.GetStsLineNeedsUpdate(); }
bool View::GetUnSavedChangeSts() const { return m.GetUnSavedChangeSts(); }
void View::SetStsLineNeedsUpdate( const bool val ) { m.SetStsLineNeedsUpdate( val ); }
void View::SetUnSavedChangeSts( const bool val ) { m.SetUnSavedChangeSts( val ); }

String View::Do_Star_GetNewPattern() { return m.Do_Star_GetNewPattern(); }
void   View::PrintPatterns( const bool HIGHLIGHT ) { m.PrintPatterns( HIGHLIGHT ); }

bool View::Has_Context() { return m.Has_Context(); }
void View::Set_Context( View& vr ) { m.Set_Context( vr ); }

bool View::GoToDir() { return m.GoToDir(); }

