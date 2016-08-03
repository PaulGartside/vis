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

struct View::Data
{
  View&      view;
  Vis&       vis;
  Key&       key;
  FileBuf&   fb;
  LinesList& reg;

  unsigned nCols;     // number of rows in buffer view
  unsigned nRows;     // number of columns in buffer view
  unsigned x;         // Top left x-position of buffer view in parent window
  unsigned y;         // Top left y-position of buffer view in parent window
  unsigned topLine;   // top  of buffer view line number.
  unsigned leftChar;  // left of buffer view character number.
  unsigned crsRow;    // cursor row    in buffer view. 0 <= m.crsRow < WorkingRows().
  unsigned crsCol;    // cursor column in buffer view. 0 <= m.crsCol < WorkingCols().
  Tile_Pos tile_pos;

  unsigned v_st_line; // Visual start line number
  unsigned v_st_char; // Visual start char number on line
  unsigned v_fn_line; // Visual ending line number
  unsigned v_fn_char; // Visual ending char number on line

  bool inInsertMode;  // true if in insert  mode, else false
  bool inReplaceMode;
  bool inVisualMode;
  bool inVisualBlock;

  bool sts_line_needs_update;
  bool us_change_sts; // un-saved change status, true if '+", false if ' '

  Data( View& view
      , Vis& vis
      , Key& key
      , FileBuf& fb
      , LinesList& reg );
  ~Data();
};

View::Data::Data( View& view
                , Vis& vis
                , Key& key
                , FileBuf& fb
                , LinesList& reg )
  : view( view )
  , vis( vis )
  , key( key )
  , fb( fb )
  , reg( reg )
  , nCols( Console::Num_Cols() )
  , nRows( Console::Num_Rows() )
  , x( 0 )
  , y( 0 )
  , topLine( 0 )
  , leftChar( 0 )
  , crsRow( 0 )
  , crsCol( 0 )
  , tile_pos( TP_FULL )
  , v_st_line( 0 )
  , v_st_char( 0 )
  , v_fn_line( 0 )
  , v_fn_char( 0 )
  , inInsertMode( false )
  , inReplaceMode( false )
  , inVisualMode( false )
  , inVisualBlock( false )
  , sts_line_needs_update( false )
  , us_change_sts( false )
{
}

View::Data::~Data()
{
}

void TilePos_2_x( View::Data& m )
{
  const unsigned CON_COLS = Console::Num_Cols();

  // TP_FULL     , TP_BOT__HALF    , TP_LEFT_QTR
  // TP_LEFT_HALF, TP_TOP__LEFT_QTR, TP_TOP__LEFT_8TH
  // TP_TOP__HALF, TP_BOT__LEFT_QTR, TP_BOT__LEFT_8TH
  m.x = 0;

  if( TP_RITE_HALF         == m.tile_pos
   || TP_TOP__RITE_QTR     == m.tile_pos
   || TP_BOT__RITE_QTR     == m.tile_pos
   || TP_RITE_CTR__QTR     == m.tile_pos
   || TP_TOP__RITE_CTR_8TH == m.tile_pos
   || TP_BOT__RITE_CTR_8TH == m.tile_pos )
  {
    m.x = CON_COLS/2;
  }
  else if( TP_LEFT_CTR__QTR     == m.tile_pos
        || TP_TOP__LEFT_CTR_8TH == m.tile_pos
        || TP_BOT__LEFT_CTR_8TH == m.tile_pos )
  {
    m.x = CON_COLS/4;
  }
  else if( TP_RITE_QTR      == m.tile_pos
        || TP_TOP__RITE_8TH == m.tile_pos
        || TP_BOT__RITE_8TH == m.tile_pos )
  {
    m.x = CON_COLS*3/4;
  }
}

void TilePos_2_y( View::Data& m )
{
  const unsigned CON_ROWS = Console::Num_Rows();

  // TP_FULL         , TP_LEFT_CTR__QTR
  // TP_LEFT_HALF    , TP_RITE_CTR__QTR
  // TP_RITE_HALF    , TP_RITE_QTR
  // TP_TOP__HALF    , TP_TOP__LEFT_8TH
  // TP_TOP__LEFT_QTR, TP_TOP__LEFT_CTR_8TH
  // TP_TOP__RITE_QTR, TP_TOP__RITE_CTR_8TH
  // TP_LEFT_QTR     , TP_TOP__RITE_8TH
  m.y = 0;

  if( TP_BOT__HALF         == m.tile_pos
   || TP_BOT__LEFT_QTR     == m.tile_pos
   || TP_BOT__RITE_QTR     == m.tile_pos
   || TP_BOT__LEFT_8TH     == m.tile_pos
   || TP_BOT__LEFT_CTR_8TH == m.tile_pos
   || TP_BOT__RITE_CTR_8TH == m.tile_pos
   || TP_BOT__RITE_8TH     == m.tile_pos )
  {
    m.y = CON_ROWS/2;
  }
}

void TilePos_2_nRows( View::Data& m )
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
  m.nRows = CON_ROWS/2;

  if( TP_FULL          == m.tile_pos
   || TP_LEFT_HALF     == m.tile_pos
   || TP_RITE_HALF     == m.tile_pos
   || TP_LEFT_QTR      == m.tile_pos
   || TP_LEFT_CTR__QTR == m.tile_pos
   || TP_RITE_CTR__QTR == m.tile_pos
   || TP_RITE_QTR      == m.tile_pos )
  {
    m.nRows = CON_ROWS;
  }
  if( ODD_ROWS && ( TP_BOT__HALF         == m.tile_pos
                 || TP_BOT__LEFT_QTR     == m.tile_pos
                 || TP_BOT__RITE_QTR     == m.tile_pos
                 || TP_BOT__LEFT_8TH     == m.tile_pos
                 || TP_BOT__LEFT_CTR_8TH == m.tile_pos
                 || TP_BOT__RITE_CTR_8TH == m.tile_pos
                 || TP_BOT__RITE_8TH     == m.tile_pos ) )
  {
    m.nRows++;
  }
}

void TilePos_2_nCols( View::Data& m )
{
  const unsigned CON_COLS = Console::Num_Cols();
  const unsigned ODD_COLS = CON_COLS%4;

  // TP_LEFT_QTR     , TP_TOP__LEFT_8TH    , TP_BOT__LEFT_8TH    ,
  // TP_LEFT_CTR__QTR, TP_TOP__LEFT_CTR_8TH, TP_BOT__LEFT_CTR_8TH,
  // TP_RITE_CTR__QTR, TP_TOP__RITE_CTR_8TH, TP_BOT__RITE_CTR_8TH,
  // TP_RITE_QTR     , TP_TOP__RITE_8TH    , TP_BOT__RITE_8TH    ,
  m.nCols = CON_COLS/4;

  if( TP_FULL      == m.tile_pos
   || TP_TOP__HALF == m.tile_pos
   || TP_BOT__HALF == m.tile_pos )
  {
    m.nCols = CON_COLS;
  }
  else if( TP_LEFT_HALF     == m.tile_pos
        || TP_RITE_HALF     == m.tile_pos
        || TP_TOP__LEFT_QTR == m.tile_pos
        || TP_TOP__RITE_QTR == m.tile_pos
        || TP_BOT__LEFT_QTR == m.tile_pos
        || TP_BOT__RITE_QTR == m.tile_pos )
  {
    m.nCols = CON_COLS/2;
  }
  if( ((TP_RITE_HALF         == m.tile_pos) && (ODD_COLS==1 || ODD_COLS==3))
   || ((TP_TOP__RITE_QTR     == m.tile_pos) && (ODD_COLS==1 || ODD_COLS==3))
   || ((TP_BOT__RITE_QTR     == m.tile_pos) && (ODD_COLS==1 || ODD_COLS==3))

   || ((TP_RITE_QTR          == m.tile_pos) && (ODD_COLS==1 || ODD_COLS==2 || ODD_COLS==3))
   || ((TP_TOP__RITE_8TH     == m.tile_pos) && (ODD_COLS==1 || ODD_COLS==2 || ODD_COLS==3))
   || ((TP_BOT__RITE_8TH     == m.tile_pos) && (ODD_COLS==1 || ODD_COLS==2 || ODD_COLS==3))

   || ((TP_LEFT_CTR__QTR     == m.tile_pos) && (ODD_COLS==2 || ODD_COLS==3))
   || ((TP_TOP__LEFT_CTR_8TH == m.tile_pos) && (ODD_COLS==2 || ODD_COLS==3))
   || ((TP_BOT__LEFT_CTR_8TH == m.tile_pos) && (ODD_COLS==2 || ODD_COLS==3))

   || ((TP_RITE_CTR__QTR     == m.tile_pos) && (ODD_COLS==3))
   || ((TP_TOP__RITE_CTR_8TH == m.tile_pos) && (ODD_COLS==3))
   || ((TP_BOT__RITE_CTR_8TH == m.tile_pos) && (ODD_COLS==3)) )
  {
    m.nCols++;
  }
}

void SetViewPos( View::Data& m )
{
  TilePos_2_x(m);
  TilePos_2_y(m);
  TilePos_2_nRows(m);
  TilePos_2_nCols(m);
}

Style Get_Style( View::Data& m
               , const unsigned line
               , const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Style s = S_NORMAL;

  if( m.view.InVisualArea( line, pos ) )
  {
    s = S_RV_NORMAL;

    if     ( m.view.InStar    ( line, pos ) ) s = S_RV_STAR;
    else if( m.view.InDefine  ( line, pos ) ) s = S_RV_DEFINE;
    else if( m.view.InComment ( line, pos ) ) s = S_RV_COMMENT;
    else if( m.view.InConst   ( line, pos ) ) s = S_RV_CONST;
    else if( m.view.InControl ( line, pos ) ) s = S_RV_CONTROL;
    else if( m.view.InVarType ( line, pos ) ) s = S_RV_VARTYPE;
    else if( m.view.InNonAscii( line, pos ) ) s = S_RV_NONASCII;
  }
  else if( m.view.InStar    ( line, pos ) ) s = S_STAR;
  else if( m.view.InDefine  ( line, pos ) ) s = S_DEFINE;
  else if( m.view.InComment ( line, pos ) ) s = S_COMMENT;
  else if( m.view.InConst   ( line, pos ) ) s = S_CONST;
  else if( m.view.InControl ( line, pos ) ) s = S_CONTROL;
  else if( m.view.InVarType ( line, pos ) ) s = S_VARTYPE;
  else if( m.view.InNonAscii( line, pos ) ) s = S_NONASCII;

  return s;
}

void PrintLines( View::Data& m
               , const unsigned st_line
               , const unsigned fn_line )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned WC = m.view.WorkingCols();

  for( unsigned k=st_line; k<=fn_line; k++ )
  {
    // Dont allow line wrap:
    const unsigned LL    = m.fb.LineLen( k );
    const unsigned G_ROW = m.view.Line_2_GL( k );
    unsigned col=0;
    for( unsigned i=m.leftChar; col<WC && i<LL; i++, col++ )
    {
      Style s = Get_Style( m, k, i );

      int byte = m.fb.Get( k, i );

      m.view.PrintWorkingView_Set( LL, G_ROW, col, i, byte, s );
    }
    for( ; col<WC; col++ )
    {
      Console::Set( G_ROW, m.view.Col_Win_2_GL( col ), ' ', S_EMPTY );
    }
  }
  Console::Update();
  Console::Flush();
}

void UpdateLines( View::Data& m
                , const unsigned st_line
                , const unsigned fn_line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Figure out which lines are currently on screen:
  unsigned m_st_line = st_line;
  unsigned m_fn_line = fn_line;

  if( m_st_line < m.topLine ) m_st_line = m.topLine;
  if( m.view.BotLine() < m_fn_line ) m_fn_line = m.view.BotLine();
  if( m_fn_line < m_st_line ) return; // Nothing to update

  // Re-draw lines:
  PrintLines( m, m_st_line, m_fn_line );
}

void InsertAddChar( View::Data& m
                  , const char c )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.fb.NumLines()==0 ) m.fb.PushLine();

  m.fb.InsertChar( m.view.CrsLine(), m.view.CrsChar(), c );

  if( m.view.WorkingCols() <= m.crsCol+1 )
  {
    // On last working column, need to scroll right:
    m.leftChar++;
  }
  else {
    m.crsCol += 1;
  }
  m.fb.Update();
}

void InsertAddReturn( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  // The lines in fb do not end with '\n's.
  // When the file is written, '\n's are added to the ends of the lines.
  Line new_line(__FILE__,__LINE__);
  const unsigned OLL = m.fb.LineLen( m.view.CrsLine() );  // Old line length
  const unsigned OCP = m.view.CrsChar();                  // Old cursor position

  for( unsigned k=OCP; k<OLL; k++ )
  {
    const uint8_t C = m.fb.RemoveChar( m.view.CrsLine(), OCP );
    bool ok = new_line.push(__FILE__,__LINE__, C );
    ASSERT( __LINE__, ok, "ok" );
  }
  // Truncate the rest of the old line:
  // Add the new line:
  const unsigned new_line_num = m.view.CrsLine()+1;
  m.fb.InsertLine( new_line_num, new_line );
  m.crsCol = 0;
  m.leftChar = 0;
  if( m.view.CrsLine() < m.view.BotLine() ) m.crsRow++;
  else {
    // If we were on the bottom working line, scroll screen down
    // one line so that the cursor line is not below the screen.
    m.topLine++;
  }
  m.fb.Update();
}

void InsertBackspace_RmC( View::Data& m
                        , const unsigned OCL
                        , const unsigned OCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.fb.RemoveChar( OCL, OCP-1 );

  m.crsCol -= 1;

  m.fb.Update();
}

void InsertBackspace_RmNL( View::Data& m, const unsigned OCL )
{
  Trace trace( __PRETTY_FUNCTION__ );
  // Cursor Line Position is zero, so:
  // 1. Save previous line, end of line + 1 position
  CrsPos ncp = { OCL-1, m.fb.LineLen( OCL-1 ) };

  // 2. Remove the line
  Line lp(__FILE__, __LINE__);
  m.fb.RemoveLine( OCL, lp );

  // 3. Append rest of line to previous line
  m.fb.AppendLineToLine( OCL-1, lp );

  // 4. Put cursor at the old previous line end of line + 1 position
  const bool MOVE_UP    = ncp.crsLine < m.topLine;
  const bool MOVE_RIGHT = m.view.RightChar() < ncp.crsChar;

  if( MOVE_UP ) m.topLine = ncp.crsLine;
                m.crsRow = ncp.crsLine - m.topLine;

  if( MOVE_RIGHT ) m.leftChar = ncp.crsChar - m.view.WorkingCols() + 1;
                   m.crsCol   = ncp.crsChar - m.leftChar;

  // 5. Removed a line, so update to re-draw window view
  m.fb.Update();
}

void InsertBackspace( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If no lines in buffer, no backspacing to be done
  if( 0==m.fb.NumLines() ) return;

  const unsigned OCL = m.view.CrsLine();    // Old cursor line
  const unsigned OCP = m.view.CrsChar();    // Old cursor position
  const unsigned OLL = m.fb.LineLen( OCL ); // Old line length

  if( OCP ) InsertBackspace_RmC ( m, OCL, OCP );
  else      InsertBackspace_RmNL( m, OCL );
}

void DisplayBanner( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Command line row in window:
  const unsigned WIN_ROW = m.view.WorkingRows() + 2;
  const unsigned WIN_COL = 0;

  const unsigned G_ROW = m.view.Row_Win_2_GL( WIN_ROW );
  const unsigned G_COL = m.view.Col_Win_2_GL( WIN_COL );

  if( m.inInsertMode )
  {
    Console::SetS( G_ROW, G_COL, "--INSERT --", S_BANNER );
  }
  else if( m.inReplaceMode )
  {
    Console::SetS( G_ROW, G_COL, "--REPLACE--", S_BANNER );
  }
  else if( m.inVisualMode )
  {
    Console::SetS( G_ROW, G_COL, "--VISUAL --", S_BANNER );
  }
  Console::Update();
  m.view.PrintCursor(); // Put cursor back in position.
}

void Replace_Crs_Char( View::Data& m, Style style )
{
  const unsigned LL = m.fb.LineLen( m.view.CrsLine() ); // Line length

  if( LL )
  {
    int byte = m.fb.Get( m.view.CrsLine(), m.view.CrsChar() );

    Console::Set( m.view.Row_Win_2_GL( m.crsRow )
                , m.view.Col_Win_2_GL( m.crsCol )
                , byte, style );
  }
}

void Undo_v( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned st_line = Min( m.v_st_line, m.v_fn_line );
  const unsigned fn_line = Max( m.v_st_line, m.v_fn_line );

  UpdateLines( m, st_line, fn_line );

  m.sts_line_needs_update = true;
}

void Remove_Banner( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned WC = m.view.WorkingCols();
  const unsigned N = Min( WC, 11 );

  // Command line row in window:
  const unsigned WIN_ROW = m.view.WorkingRows() + 2;

  // Clear command line:
  for( unsigned k=0; k<N; k++ )
  {
    Console::Set( m.view.Row_Win_2_GL( WIN_ROW )
                , m.view.Col_Win_2_GL( k )
                , ' '
                , S_NORMAL );
  }
  Console::Update();
  m.view.PrintCursor(); // Put cursor back in position.
}

void Do_v_Handle_gf( View::Data& m )
{
  if( m.v_st_line == m.v_fn_line )
  {
    const unsigned v_st_char = m.v_st_char < m.v_fn_char
                             ? m.v_st_char : m.v_fn_char;
    const unsigned v_fn_char = m.v_st_char < m.v_fn_char
                             ? m.v_fn_char : m.v_st_char;
    String fname;

    for( unsigned P = v_st_char; P<=v_fn_char; P++ )
    {
      fname.push( m.fb.Get( m.v_st_line, P  ) );
    }
    bool went_to_file = m.vis.GoToBuffer_Fname( fname );

    if( went_to_file )
    {
      // If we made it to buffer indicated by fname, no need to Undo_v() or
      // Remove_Banner() because the whole view pane will be redrawn
      m.inVisualMode = false;
    }
  }
}

void Swap_Visual_St_Fn_If_Needed( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.inVisualBlock )
  {
    if( m.v_fn_line < m.v_st_line ) Swap( m.v_st_line, m.v_fn_line );
    if( m.v_fn_char < m.v_st_char ) Swap( m.v_st_char, m.v_fn_char );
  }
  else {
    if( m.v_fn_line < m.v_st_line
     || (m.v_fn_line == m.v_st_line && m.v_fn_char < m.v_st_char) )
    {
      // Visual mode went backwards over multiple lines, or
      // Visual mode went backwards over one line
      Swap( m.v_st_line, m.v_fn_line );
      Swap( m.v_st_char, m.v_fn_char );
    }
  }
}

void Do_v_Handle_gp( View::Data& m )
{
  if( m.v_st_line == m.v_fn_line )
  {
    Swap_Visual_St_Fn_If_Needed(m);

    String pattern;

    for( unsigned P = m.v_st_char; P<=m.v_fn_char; P++ )
    {
      pattern.push( m.fb.Get( m.v_st_line, P ) );
    }
    m.vis.Handle_Slash_GotPattern( pattern, false );

    m.inVisualMode = false;
    Undo_v(m);
    Remove_Banner(m);
  }
}

// Returns true if still in visual mode, else false
//
void Do_v_Handle_g( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC2 = m.key.In();

  if     ( CC2 == 'g' ) m.view.GoToTopOfFile();
  else if( CC2 == '0' ) m.view.GoToStartOfRow();
  else if( CC2 == '$' ) m.view.GoToEndOfRow();
  else if( CC2 == 'f' ) Do_v_Handle_gf(m);
  else if( CC2 == 'p' ) Do_v_Handle_gp(m);
}

// This one works better when IN visual mode:
void PageDown_v( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  if( 0<NUM_LINES )
  {
    const unsigned OCLd = m.view.CrsLine(); // Old cursor line

    unsigned NCLd = OCLd + m.view.WorkingRows() - 1; // New cursor line

    // Dont let cursor go past the end of the file:
    if( NUM_LINES-1 < NCLd ) NCLd = NUM_LINES-1;

    m.view.GoToCrsPos_Write( NCLd, 0 );
  }
}

void PageUp_v( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  if( 0<NUM_LINES )
  {
    const unsigned OCL = m.view.CrsLine(); // Old cursor line

    int NCL = OCL - m.view.WorkingRows() + 1; // New cursor line

    // Check for underflow:
    if( NCL < 0 ) NCL = 0;

    m.view.GoToCrsPos_Write( NCL, 0 );
  }
}

void Do_y_v_block( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned old_v_st_line = m.v_st_line;
  const unsigned old_v_st_char = m.v_st_char;

  Swap_Visual_St_Fn_If_Needed( m );

  for( unsigned L=m.v_st_line; L<=m.v_fn_line; L++ )
  {
    Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = m.fb.LineLen( L );

    for( unsigned P = m.v_st_char; P<LL && P <= m.v_fn_char; P++ )
    {
      nlp->push(__FILE__,__LINE__, m.fb.Get( L, P ) );
    }
    // m.reg will delete nlp
    m.reg.push( nlp );
  }
  m.vis.SetPasteMode( PM_BLOCK );

  // Try to put cursor at (old_v_st_line, old_v_st_char), but
  // make sure the cursor is in bounds after the deletion:
  const unsigned NUM_LINES = m.fb.NumLines();
  unsigned ncl = old_v_st_line;
  if( NUM_LINES <= ncl ) ncl = NUM_LINES-1;
  const unsigned NLL = m.fb.LineLen( ncl );
  unsigned ncc = 0;
  if( NLL ) ncc = NLL <= old_v_st_char ? NLL-1 : old_v_st_char;

  m.view.GoToCrsPos_NoWrite( ncl, ncc );
}

void Do_y_v_st_fn( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Swap_Visual_St_Fn_If_Needed( m );

  for( unsigned L=m.v_st_line; L<=m.v_fn_line; L++ )
  {
    Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = m.fb.LineLen( L );
    if( LL ) {
      const unsigned P_st = (L==m.v_st_line) ? m.v_st_char : 0;
      const unsigned P_fn = (L==m.v_fn_line) ? Min(LL-1,m.v_fn_char) : LL-1;

      for( unsigned P = P_st; P <= P_fn; P++ )
      {
        nlp->push(__FILE__,__LINE__, m.fb.Get( L, P ) );
      }
    }
    // m.reg will delete nlp
    m.reg.push( nlp );
  }
  m.vis.SetPasteMode( PM_ST_FN );
}

void Do_y_v( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.reg.clear();

  if( m.inVisualBlock ) Do_y_v_block(m);
  else                  Do_y_v_st_fn(m);
}

void Do_Y_v_st_fn( View::Data& m )
{
  if( m.v_fn_line < m.v_st_line ) Swap( m.v_st_line, m.v_fn_line );

  for( unsigned L=m.v_st_line; L<=m.v_fn_line; L++ )
  {
    Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = m.fb.LineLen(L);

    if( LL )
    {
      for( unsigned P = 0; P <= LL-1; P++ )
      {
        nlp->push(__FILE__,__LINE__, m.fb.Get( L, P ) );
      }
    }
    // m.reg will delete nlp
    m.reg.push( nlp );
  }
  m.vis.SetPasteMode( PM_LINE );
}

void Do_Y_v( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.reg.clear();

  if( m.inVisualBlock ) Do_y_v_block(m);
  else                  Do_Y_v_st_fn(m);
}

void Do_x_range_pre( View::Data& m
                   , unsigned& st_line, unsigned& st_char
                   , unsigned& fn_line, unsigned& fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.inVisualBlock )
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
  m.reg.clear();
}

void Do_x_range_post( View::Data& m
                    , const unsigned st_line
                    , const unsigned st_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.inVisualBlock ) m.vis.SetPasteMode( PM_BLOCK );
  else                  m.vis.SetPasteMode( PM_ST_FN );

  // Try to put cursor at (st_line, st_char), but
  // make sure the cursor is in bounds after the deletion:
  const unsigned NUM_LINES = m.fb.NumLines();
  unsigned ncl = st_line;
  if( NUM_LINES <= ncl ) ncl = NUM_LINES-1;
  const unsigned NLL = m.fb.LineLen( ncl );
  unsigned ncc = 0;
  if( NLL ) ncc = NLL <= st_char ? NLL-1 : st_char;

  m.view.GoToCrsPos_NoWrite( ncl, ncc );

  m.inVisualMode = false;

  m.fb.Update(); //<- No need to Undo_v() or Remove_Banner() because of this
}

void Do_x_range_block( View::Data& m
                     , unsigned st_line, unsigned st_char
                     , unsigned fn_line, unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Do_x_range_pre( m, st_line, st_char, fn_line, fn_char );

  for( unsigned L = st_line; L<=fn_line; L++ )
  {
    Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = m.fb.LineLen( L );

    for( unsigned P = st_char; P<LL && P <= fn_char; P++ )
    {
      nlp->push(__FILE__,__LINE__, m.fb.RemoveChar( L, st_char ) );
    }
    m.reg.push( nlp );
  }
  Do_x_range_post( m, st_line, st_char );
}

void Do_x_range_single( View::Data& m
                      , const unsigned L
                      , const unsigned st_char
                      , const unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OLL = m.fb.LineLen( L ); // Original line length

  if( 0<OLL )
  {
    Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned P_st = Min( st_char, OLL-1 );
    const unsigned P_fn = Min( fn_char, OLL-1 );

    unsigned LL = OLL;

    // Dont remove a single line, or else Q wont work right
    for( unsigned P = P_st; P_st < LL && P <= P_fn; P++ )
    {
      nlp->push(__FILE__,__LINE__, m.fb.RemoveChar( L, P_st ) );

      LL = m.fb.LineLen( L ); // Removed a char, so re-calculate LL
    }
    m.reg.push( nlp );
  }
}

void Do_x_range_multiple( View::Data& m
                        , const unsigned st_line
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
    Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );

    const unsigned OLL = m.fb.LineLen( L ); // Original line length

    const unsigned P_st = (L==  st_line) ? Min(st_char, OLL-1) : 0;
    const unsigned P_fn = (L==n_fn_line) ? Min(fn_char, OLL-1) : OLL-1;

    if(   st_line == L && 0    < P_st  ) started_in_middle = true;
    if( n_fn_line == L && P_fn < OLL-1 ) ended___in_middle = true;

    unsigned LL = OLL;

    for( unsigned P = P_st; P_st < LL && P <= P_fn; P++ )
    {
      nlp->push(__FILE__,__LINE__, m.fb.RemoveChar( L, P_st ) );

      LL = m.fb.LineLen( L ); // Removed a char, so re-calculate LL
    }
    if( 0 == P_st && OLL-1 == P_fn ) // Removed entire line
    {
      m.fb.RemoveLine( L );
      n_fn_line--;
    }
    else L++;

    m.reg.push( nlp );
  }
  if( started_in_middle && ended___in_middle )
  {
    Line* lp = m.fb.RemoveLineP( st_line+1 );
    m.fb.AppendLineToLine( st_line, lp );
  }
}

void Do_x_range( View::Data& m
               , unsigned st_line, unsigned st_char
               , unsigned fn_line, unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Do_x_range_pre( m, st_line, st_char, fn_line, fn_char );

  if( st_line == fn_line )
  {
    Do_x_range_single( m, st_line, st_char, fn_char );
  }
  else {
    Do_x_range_multiple( m, st_line, st_char, fn_line, fn_char );
  }
  Do_x_range_post( m, st_line, st_char );
}

void Do_x_v( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.inVisualBlock )
  {
    Do_x_range_block( m, m.v_st_line, m.v_st_char, m.v_fn_line, m.v_fn_char );
  }
  else {
    Do_x_range( m, m.v_st_line, m.v_st_char, m.v_fn_line, m.v_fn_char );
  }
  Remove_Banner(m);
}

void Do_D_v_line( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Swap_Visual_St_Fn_If_Needed(m);

  m.reg.clear();

  bool removed_line = false;
  // 1. If m.v_st_line==0, fn_line will go negative in the loop below,
  //    so use int's instead of unsigned's
  // 2. Dont remove all lines in file to avoid crashing
  int fn_line = m.v_fn_line;
  for( int L = m.v_st_line; 1 < m.fb.NumLines() && L<=fn_line; fn_line-- )
  {
    Line* lp = m.fb.RemoveLineP( L );
    m.reg.push( lp );

    // m.reg will delete nlp
    removed_line = true;
  }
  m.vis.SetPasteMode( PM_LINE );

  m.inVisualMode = false;
  Remove_Banner(m);
  // D'ed lines will be removed, so no need to Undo_v()

  if( removed_line )
  {
    // Figure out and move to new cursor position:
    const unsigned NUM_LINES = m.fb.NumLines();
    const unsigned OCL       = m.view.CrsLine(); // Old cursor line

    unsigned ncl = m.v_st_line;
    if( NUM_LINES-1 < ncl ) ncl = m.v_st_line ? m.v_st_line-1 : 0;

    const unsigned NCLL = m.fb.LineLen( ncl );
    unsigned ncc = 0;
    if( NCLL ) ncc = m.v_st_char < NCLL ? m.v_st_char : NCLL-1;

    m.view.GoToCrsPos_NoWrite( ncl, ncc );

    m.fb.Update();
  }
}

void Do_D_v( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.inVisualBlock )
  {
    Do_x_range_block( m, m.v_st_line, m.v_st_char, m.v_fn_line, m.v_fn_char );
    Remove_Banner(m);
  }
  else {
    Do_D_v_line(m);
  }
}

void InsertBackspace_vb( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = m.view.CrsLine();  // Old cursor line
  const unsigned OCP = m.view.CrsChar();  // Old cursor position

  if( OCP )
  {
    const unsigned N_REG_LINES = m.reg.len();

    for( unsigned k=0; k<N_REG_LINES; k++ )
    {
      m.fb.RemoveChar( OCL+k, OCP-1 );
    }
    m.view.GoToCrsPos_NoWrite( OCL, OCP-1 );
  }
}

void InsertAddChar_vb( View::Data& m, const char c )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = m.view.CrsLine();  // Old cursor line
  const unsigned OCP = m.view.CrsChar();  // Old cursor position

  const unsigned N_REG_LINES = m.reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    const unsigned LL = m.fb.LineLen( OCL+k );

    if( LL < OCP )
    {
      // Fill in line with white space up to OCP:
      for( unsigned i=0; i<(OCP-LL); i++ )
      {
        // Insert at end of line so undo will be atomic:
        const unsigned NLL = m.fb.LineLen( OCL+k ); // New line length
        m.fb.InsertChar( OCL+k, NLL, ' ' );
      }
    }
    m.fb.InsertChar( OCL+k, OCP, c );
  }
  m.view.GoToCrsPos_NoWrite( OCL, OCP+1 );
}

void Do_i_vb( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m.inInsertMode = true;
  DisplayBanner(m);

  unsigned count = 0;
  for( char c=m.key.In(); c != ESC; c=m.key.In() )
  {
    if( IsEndOfLineDelim( c ) )
    {
      ; // Ignore end of line delimiters
    }
    else if( BS  == c || DEL == c )
    {
      if( count )
      {
        InsertBackspace_vb(m);
        count--;
        m.fb.Update();
      }
    }
    else {
      InsertAddChar_vb( m, c );
      count++;
      m.fb.Update();
    }
  }
  Remove_Banner(m);
  m.inInsertMode = false;
}

void Do_a_vb( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CL = m.view.CrsLine();
  const unsigned LL = m.fb.LineLen( CL );
  if( 0==LL ) { Do_i_vb(m); return; }

  const bool CURSOR_AT_EOL = ( m.view.CrsChar() == LL-1 );
  if( CURSOR_AT_EOL )
  {
    m.view.GoToCrsPos_NoWrite( CL, LL );
  }
  const bool CURSOR_AT_RIGHT_COL = ( m.crsCol == m.view.WorkingCols()-1 );

  if( CURSOR_AT_RIGHT_COL )
  {
    // Only need to scroll window right, and then enter insert i:
    m.leftChar++; //< This increments m.view.CrsChar()
  }
  else if( !CURSOR_AT_EOL ) // If cursor was at EOL, already moved cursor forward
  {
    // Only need to move cursor right, and then enter insert i:
    m.crsCol += 1; //< This increments m.view.CrsChar()
  }
  m.fb.Update();

  Do_i_vb(m);
}

void Do_s_v( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Need to know if cursor is at end of line before Do_x_v() is called:
  const unsigned LL = m.fb.LineLen( m.view.CrsLine() );
  const bool CURSOR_AT_END_OF_LINE = LL ? m.view.CrsChar() == LL-1 : false;

  Do_x_v(m);

  if( m.inVisualBlock )
  {
    if( CURSOR_AT_END_OF_LINE ) Do_a_vb(m);
    else                        Do_i_vb(m);
  }
  else {
    if( CURSOR_AT_END_OF_LINE ) m.view.Do_a();
    else                        m.view.Do_i();
  }
}

void Do_Tilda_v_st_fn( View::Data& m )
{
  for( unsigned L = m.v_st_line; L<=m.v_fn_line; L++ )
  {
    const unsigned LL = m.fb.LineLen( L );
    const unsigned P_st = (L==m.v_st_line) ? m.v_st_char : 0;
    const unsigned P_fn = (L==m.v_fn_line) ? m.v_fn_char : LL-1;

    for( unsigned P = P_st; P <= P_fn; P++ )
    {
      char c = m.fb.Get( L, P );
      bool changed = false;
      if     ( isupper( c ) ) { c = tolower( c ); changed = true; }
      else if( islower( c ) ) { c = toupper( c ); changed = true; }
      if( changed ) m.fb.Set( L, P, c );
    }
  }
}

void Do_Tilda_v_block( View::Data& m )
{
  for( unsigned L = m.v_st_line; L<=m.v_fn_line; L++ )
  {
    const unsigned LL = m.fb.LineLen( L );

    for( unsigned P = m.v_st_char; P<LL && P <= m.v_fn_char; P++ )
    {
      char c = m.fb.Get( L, P );
      bool changed = false;
      if     ( isupper( c ) ) { c = tolower( c ); changed = true; }
      else if( islower( c ) ) { c = toupper( c ); changed = true; }
      if( changed ) m.fb.Set( L, P, c );
    }
  }
}

void Do_Tilda_v( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Swap_Visual_St_Fn_If_Needed( m );

  if( m.inVisualBlock ) Do_Tilda_v_block(m);
  else                  Do_Tilda_v_st_fn(m);

  m.inVisualMode = false;
  Remove_Banner(m);
  Undo_v(m); //<- This will cause the tilda'ed characters to be redrawn
}

// Returns true if something was changed, else false
//
bool Do_visualMode( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m.view.MoveInBounds();
  m.inVisualMode = true;
  DisplayBanner(m);

  m.v_st_line = m.view.CrsLine();  m.v_fn_line = m.v_st_line;
  m.v_st_char = m.view.CrsChar();  m.v_fn_char = m.v_st_char;

  // Write current byte in visual:
  Replace_Crs_Char( m, S_VISUAL );

  while( m.inVisualMode )
  {
    const char c=m.key.In();

    if     ( c == 'l' ) m.view.GoRight();
    else if( c == 'h' ) m.view.GoLeft();
    else if( c == 'j' ) m.view.GoDown();
    else if( c == 'k' ) m.view.GoUp();
    else if( c == 'H' ) m.view.GoToTopLineInView();
    else if( c == 'L' ) m.view.GoToBotLineInView();
    else if( c == 'M' ) m.view.GoToMidLineInView();
    else if( c == '0' ) m.view.GoToBegOfLine();
    else if( c == '$' ) m.view.GoToEndOfLine();
    else if( c == 'g' ) Do_v_Handle_g(m);
    else if( c == 'G' ) m.view.GoToEndOfFile();
    else if( c == 'F' ) PageDown_v(m);
    else if( c == 'B' ) PageUp_v(m);
    else if( c == 'b' ) m.view.GoToPrevWord();
    else if( c == 'w' ) m.view.GoToNextWord();
    else if( c == 'e' ) m.view.GoToEndOfWord();
    else if( c == '%' ) m.view.GoToOppositeBracket();
    else if( c == 'z' ) m.vis.Handle_z();
    else if( c == 'f' ) m.vis.Handle_f();
    else if( c == ';' ) m.vis.Handle_SemiColon();
    else if( c == 'y' ) { Do_y_v(m); goto EXIT_VISUAL; }
    else if( c == 'Y' ) { Do_Y_v(m); goto EXIT_VISUAL; }
    else if( c == 'x'
          || c == 'd' ) { Do_x_v(m); return true; }
    else if( c == 'D' ) { Do_D_v(m); return true; }
    else if( c == 's' ) { Do_s_v(m); return true; }
    else if( c == '~' ) { Do_Tilda_v(m); return true; }
    else if( c == ESC ) { goto EXIT_VISUAL; }
  }
  return false;

EXIT_VISUAL:
  m.inVisualMode = false;
  Undo_v(m);
  Remove_Banner(m);
  return false;
}

void ReplaceAddReturn( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );
  // The lines in fb do not end with '\n's.
  // When the file is written, '\n's are added to the ends of the lines.
  Line new_line(__FILE__, __LINE__);
  const unsigned OLL = m.fb.LineLen( m.view.CrsLine() );
  const unsigned OCP = m.view.CrsChar();

  for( unsigned k=OCP; k<OLL; k++ )
  {
    const uint8_t C = m.fb.RemoveChar( m.view.CrsLine(), OCP );
    bool ok = new_line.push(__FILE__,__LINE__, C );
    ASSERT( __LINE__, ok, "ok" );
  }
  // Truncate the rest of the old line:
  // Add the new line:
  const unsigned new_line_num = m.view.CrsLine()+1;
  m.fb.InsertLine( new_line_num, new_line );
  m.crsCol = 0;
  m.leftChar = 0;
  if( m.view.CrsLine() < m.view.BotLine() ) m.crsRow++;
  else {
    // If we were on the bottom working line, scroll screen down
    // one line so that the cursor line is not below the screen.
    m.topLine++;
  }
  m.fb.Update();
}

void ReplaceAddChars( View::Data& m, const char C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( m.fb.NumLines()==0 ) m.fb.PushLine();

  const unsigned CL = m.view.CrsLine();
  const unsigned CP = m.view.CrsChar();
  const unsigned LL = m.fb.LineLen( CL );
  const unsigned EOL = LL ? LL-1 : 0;

  if( EOL < CP )
  {
    // Extend line out to where cursor is:
    for( unsigned k=LL; k<CP; k++ ) m.fb.PushChar( CL, ' ' );
  }
  // Put char back in file buffer
  const bool continue_last_update = false;
  if( CP < LL ) m.fb.Set( CL, CP, C, continue_last_update );
  else {
    m.fb.PushChar( CL, C );
  }
  if( m.crsCol < m.view.WorkingCols()-1 )
  {
    m.crsCol++;
  }
  else {
    m.leftChar++;
  }
  m.fb.Update();
}

void Print_Borders_Top( View::Data& m, const Style S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( top___border )
  {
    const uint8_t BORDER_CHAR = m.fb.Changed() ? '+' : ' ';
    const unsigned ROW_G = m.y;

    for( unsigned k=0; k<m.nCols; k++ )
    {
      const unsigned COL_G = m.x + k;

      Console::Set( ROW_G, COL_G, BORDER_CHAR, S );
    }
  }
}

void Print_Borders_Bottom( View::Data& m, const Style S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( bottomborder )
  {
    const uint8_t BORDER_CHAR = m.fb.Changed() ? '+' : ' ';
    const unsigned ROW_G = m.y + m.nRows - 1;

    for( unsigned k=0; k<m.nCols; k++ )
    {
      const unsigned COL_G = m.x + k;

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

void Print_Borders_Right( View::Data& m, const Style S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( right_border )
  {
    const uint8_t BORDER_CHAR = m.fb.Changed() ? '+' : ' ';
    const unsigned COL_G = m.x + m.nCols - 1;

    for( unsigned k=0; k<m.nRows-1; k++ )
    {
      const unsigned ROW_G = m.y + k;

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

void Print_Borders_Left( View::Data& m, const Style S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( left__border )
  {
    const uint8_t BORDER_CHAR = m.fb.Changed() ? '+' : ' ';
    const unsigned COL_G = m.x;

    for( unsigned k=0; k<m.nRows; k++ )
    {
      const unsigned ROW_G = m.y + k;

      Console::Set( ROW_G, COL_G, BORDER_CHAR, S );
    }
  }
}

// Returns true if found next word, else false
//
bool GoToNextWord_GetPosition( View::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  if( 0==NUM_LINES ) return false;

  bool found_space = false;
  bool found_word  = false;
  const unsigned OCL = m.view.CrsLine(); // Old cursor line
  const unsigned OCP = m.view.CrsChar(); // Old cursor position

  IsWord_Func isWord = IsWord_Ident;

  // Find white space, and then find non-white space
  for( unsigned l=OCL; (!found_space || !found_word) && l<NUM_LINES; l++ )
  {
    const unsigned LL = m.fb.LineLen( l );
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

      const int C = m.fb.Get( l, p );

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
bool GoToPrevWord_GetPosition( View::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  if( 0==NUM_LINES ) return false;

  const int      OCL = m.view.CrsLine(); // Old cursor line
  const unsigned LL  = m.fb.LineLen( OCL );

  if( LL < m.view.CrsChar() ) // Since cursor is now allowed past EOL,
  {                           // it may need to be moved back:
    if( LL && !IsSpace( m.fb.Get( OCL, LL-1 ) ) )
    {
      // Backed up to non-white space, which is previous word, so return true
      ncp.crsLine = OCL;
      ncp.crsChar = LL-1;
      return true;
    }
    else {
      m.view.GoToCrsPos_NoWrite( OCL, LL ? LL-1 : 0 );
    }
  }
  bool found_space = false;
  bool found_word  = false;
  const unsigned OCP = m.view.CrsChar(); // Old cursor position

  IsWord_Func isWord = NotSpace;

  // Find word to non-word transition
  for( int l=OCL; (!found_space || !found_word) && -1<l; l-- )
  {
    const int LL = m.fb.LineLen( l );
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

      const int C = m.fb.Get( l, p );

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
bool GoToEndOfWord_GetPosition( View::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  if( 0==NUM_LINES ) return false;

  const int      CL = m.view.CrsLine(); // Cursor line
  const unsigned LL = m.fb.LineLen( CL );
        unsigned CP = m.view.CrsChar(); // Cursor position

  // At end of line, or line too short:
  if( (LL-1) <= CP || LL < 2 ) return false;

  int CC = m.fb.Get( CL, CP );   // Current char
  int NC = m.fb.Get( CL, CP+1 ); // Next char

  // 1. If at end of word, or end of non-word, move to next char
  if( (IsWord_Ident   ( CC ) && !IsWord_Ident   ( NC ))
   || (IsWord_NonIdent( CC ) && !IsWord_NonIdent( NC )) ) CP++;

  // 2. If on white space, skip past white space
  if( IsSpace( m.fb.Get(CL, CP) ) )
  {
    for( ; CP<LL && IsSpace( m.fb.Get(CL, CP) ); CP++ ) ;
    if( LL <= CP ) return false; // Did not find non-white space
  }
  // At this point (CL,CP) should be non-white space
  CC = m.fb.Get( CL, CP );  // Current char

  ncp.crsLine = CL;

  if( IsWord_Ident( CC ) ) // On identity
  {
    // 3. If on word space, go to end of word space
    for( ; CP<LL && IsWord_Ident( m.fb.Get(CL, CP) ); CP++ )
    {
      ncp.crsChar = CP;
    }
  }
  else if( IsWord_NonIdent( CC ) )// On Non-identity, non-white space
  {
    // 4. If on non-white-non-word, go to end of non-white-non-word
    for( ; CP<LL && IsWord_NonIdent( m.fb.Get(CL, CP) ); CP++ )
    {
      ncp.crsChar = CP;
    }
  }
  else { // Should never get here:
    return false;
  }
  return true;
}

void GoToOppositeBracket_Forward( View::Data& m
                                , const char ST_C
                                , const char FN_C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  const unsigned CL = m.view.CrsLine();
  const unsigned CC = m.view.CrsChar();

  // Search forward
  unsigned level = 0;
  bool     found = false;

  for( unsigned l=CL; !found && l<NUM_LINES; l++ )
  {
    const unsigned LL = m.fb.LineLen( l );

    for( unsigned p=(CL==l)?(CC+1):0; !found && p<LL; p++ )
    {
      const char C = m.fb.Get( l, p );

      if     ( C==ST_C ) level++;
      else if( C==FN_C )
      {
        if( 0 < level ) level--;
        else {
          found = true;

          m.view.GoToCrsPos_Write( l, p );
        }
      }
    }
  }
}

void GoToOppositeBracket_Backward( View::Data& m
                                 , const char ST_C
                                 , const char FN_C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const int CL = m.view.CrsLine();
  const int CC = m.view.CrsChar();

  // Search forward
  unsigned level = 0;
  bool     found = false;

  for( int l=CL; !found && 0<=l; l-- )
  {
    const unsigned LL = m.fb.LineLen( l );

    for( int p=(CL==l)?(CC-1):(LL-1); !found && 0<=p; p-- )
    {
      const char C = m.fb.Get( l, p );

      if     ( C==ST_C ) level++;
      else if( C==FN_C )
      {
        if( 0 < level ) level--;
        else {
          found = true;

          m.view.GoToCrsPos_Write( l, p );
        }
      }
    }
  }
}

// Cursor is moving forward
// Write out from (OCL,OCP) up to but not including (NCL,NCP)
void GoToCrsPos_WV_Forward( View::Data& m
                          , const unsigned OCL
                          , const unsigned OCP
                          , const unsigned NCL
                          , const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( OCL == NCL ) // Only one line:
  {
    for( unsigned k=OCP; k<NCP; k++ )
    {
      int byte = m.fb.Get( OCL, k );
      Console::Set( m.view.Line_2_GL( OCL )
                  , m.view.Char_2_GL( k ), byte, Get_Style(m,OCL,k) );
    }
  }
  else { // Multiple lines
    // Write out first line:
    const unsigned OCLL = m.fb.LineLen( OCL ); // Old cursor line length
    const unsigned END_FIRST_LINE = Min( m.view.RightChar()+1, OCLL );
    for( unsigned k=OCP; k<END_FIRST_LINE; k++ )
    {
      int byte = m.fb.Get( OCL, k );
      Console::Set( m.view.Line_2_GL( OCL )
                  , m.view.Char_2_GL( k ), byte, Get_Style(m,OCL,k) );
    }
    // Write out intermediate lines:
    for( unsigned l=OCL+1; l<NCL; l++ )
    {
      const unsigned LL = m.fb.LineLen( l ); // Line length
      const unsigned END_OF_LINE = Min( m.view.RightChar()+1, LL );
      for( unsigned k=m.leftChar; k<END_OF_LINE; k++ )
      {
        int byte = m.fb.Get( l, k );
        Console::Set( m.view.Line_2_GL( l )
                    , m.view.Char_2_GL( k ), byte, Get_Style(m,l,k) );
      }
    }
    // Write out last line:
    // Print from beginning of next line to new cursor position:
    const unsigned NCLL = m.fb.LineLen( NCL ); // Line length
    const unsigned END = Min( NCLL, NCP );
    for( unsigned k=m.leftChar; k<END; k++ )
    {
      int byte = m.fb.Get( NCL, k );
      Console::Set( m.view.Line_2_GL( NCL )
                  , m.view.Char_2_GL( k ), byte, Get_Style(m,NCL,k)  );
    }
  }
}

// Cursor is moving backwards
// Write out from (OCL,OCP) back to but not including (NCL,NCP)
void GoToCrsPos_WV_Backward( View::Data& m
                           , const unsigned OCL
                           , const unsigned OCP
                           , const unsigned NCL
                           , const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( OCL == NCL ) // Only one line:
  {
    const unsigned LL = m.fb.LineLen( OCL ); // Line length
    if( LL ) {
      const unsigned START = Min( OCP, LL-1 );
      for( unsigned k=START; NCP<k; k-- )
      {
        int byte = m.fb.Get( OCL, k );
        Console::Set( m.view.Line_2_GL( OCL )
                    , m.view.Char_2_GL( k ), byte, Get_Style(m,OCL,k) );
      }
    }
  }
  else { // Multiple lines
    // Write out first line:
    const unsigned OCLL = m.fb.LineLen( OCL ); // Old cursor line length
    if( OCLL ) {
      for( int k=Min(OCP,OCLL-1); static_cast<int>(m.leftChar)<=k; k-- )
      {
        int byte = m.fb.Get( OCL, k );
        Console::Set( m.view.Line_2_GL( OCL )
                    , m.view.Char_2_GL( k ), byte, Get_Style(m,OCL,k) );
      }
    }
    // Write out intermediate lines:
    for( unsigned l=OCL-1; NCL<l; l-- )
    {
      const unsigned LL = m.fb.LineLen( l ); // Line length
      if( LL ) {
        const unsigned END_OF_LINE = Min( m.view.RightChar(), LL-1 );
        for( int k=END_OF_LINE; static_cast<int>(m.leftChar)<=k; k-- )
        {
          int byte = m.fb.Get( l, k );
          Console::Set( m.view.Line_2_GL( l )
                      , m.view.Char_2_GL( k ), byte, Get_Style(m,l,k) );
        }
      }
    }
    // Write out last line:
    // Go down to beginning of last line:
    const unsigned NCLL = m.fb.LineLen( NCL ); // New cursor line length
    if( NCLL ) {
      const unsigned END_LAST_LINE = Min( m.view.RightChar(), NCLL-1 );

      // Print from beginning of next line to new cursor position:
      for( int k=END_LAST_LINE; static_cast<int>(NCP)<=k; k-- )
      {
        int byte = m.fb.Get( NCL, k );
        Console::Set( m.view.Line_2_GL( NCL )
                    , m.view.Char_2_GL( k ), byte, Get_Style(m,NCL,k) );
      }
    }
  }
}

void GoToCrsPos_Write_Visual( View::Data& m
                            , const unsigned OCL
                            , const unsigned OCP
                            , const unsigned NCL
                            , const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // (old cursor pos) < (new cursor pos)
  const bool OCP_LT_NCP = OCL < NCL || (OCL == NCL && OCP < NCP);

  if( OCP_LT_NCP ) // Cursor moved forward
  {
    GoToCrsPos_WV_Forward( m, OCL, OCP, NCL, NCP );
  }
  else // NCP_LT_OCP // Cursor moved backward
  {
    GoToCrsPos_WV_Backward( m, OCL, OCP, NCL, NCP );
  }
  m.crsRow = NCL - m.topLine;
  m.crsCol = NCP - m.leftChar;
  Console::Update();
  m.view.PrintCursor();
  m.sts_line_needs_update = true;
}

bool RV_Style( const Style s )
{
  return s == S_RV_NORMAL
      || s == S_RV_STAR
      || s == S_RV_DEFINE
      || s == S_RV_COMMENT
      || s == S_RV_CONST
      || s == S_RV_CONTROL
      || s == S_RV_VARTYPE;
}

Style RV_Style_2_NonRV( const Style RVS )
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

void GoToCrsPos_Write_VisualBlock( View::Data& m
                                 , const unsigned OCL
                                 , const unsigned OCP
                                 , const unsigned NCL
                                 , const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // m.v_fn_line == NCL && v_fn_char == NCP, so dont need to include
  // m.v_fn_line       and v_fn_char in Min and Max calls below:
  const unsigned vis_box_left = Min( m.v_st_char, Min( OCP, NCP ) );
  const unsigned vis_box_rite = Max( m.v_st_char, Max( OCP, NCP ) );
  const unsigned vis_box_top  = Min( m.v_st_line, Min( OCL, NCL ) );
  const unsigned vis_box_bot  = Max( m.v_st_line, Max( OCL, NCL ) );

  const unsigned draw_box_left = Max( m.leftChar   , vis_box_left );
  const unsigned draw_box_rite = Min( m.view.RightChar(), vis_box_rite );
  const unsigned draw_box_top  = Max( m.topLine    , vis_box_top  );
  const unsigned draw_box_bot  = Min( m.view.BotLine()  , vis_box_bot  );

  for( unsigned l=draw_box_top; l<=draw_box_bot; l++ )
  {
    const unsigned LL = m.fb.LineLen( l );

    for( unsigned k=draw_box_left; k<LL && k<=draw_box_rite; k++ )
    {
      // On some terminals, the cursor on reverse video on white space does not
      // show up, so to prevent that, do not reverse video the cursor position:
      const int   byte  = m.fb.Get ( l, k );
      const Style style = Get_Style( m, l, k );

      if( NCL==l && NCP==k )
      {
        if( RV_Style( style ) )
        {
          const Style NonRV_style = RV_Style_2_NonRV( style );

          Console::Set( m.view.Line_2_GL( l )
                      , m.view.Char_2_GL( k ), byte, NonRV_style );
        }
      }
      else {
        Console::Set( m.view.Line_2_GL( l )
                    , m.view.Char_2_GL( k ), byte, style );
      }
    }
  }
  m.crsRow = NCL - m.topLine;
  m.crsCol = NCP - m.leftChar;
  Console::Update();
  m.view.PrintCursor();
  m.sts_line_needs_update = true;
}

bool Do_n_FindNextPattern( View::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  const unsigned OCL = m.view.CrsLine();
  const unsigned OCC = m.view.CrsChar();

  unsigned st_l = OCL;
  unsigned st_c = OCC;

  bool found_next_star = false;

  // Move past current star:
  const unsigned LL = m.fb.LineLen( OCL );

  for( ; st_c<LL && m.view.InStar(OCL,st_c); st_c++ ) ;

  // Go down to next line
  if( LL <= st_c ) { st_c=0; st_l++; }

  // Search for first star position past current position
  for( unsigned l=st_l; !found_next_star && l<NUM_LINES; l++ )
  {
    const unsigned LL = m.fb.LineLen( l );

    for( unsigned p=st_c
       ; !found_next_star && p<LL
       ; p++ )
    {
      if( m.view.InStar(l,p) )
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
      const unsigned LL = m.fb.LineLen( l );
      const unsigned END_C = (OCL==l) ? Min( OCC, LL ) : LL;

      for( unsigned p=0; !found_next_star && p<END_C; p++ )
      {
        if( m.view.InStar(l,p) )
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

bool Do_N_FindPrevPattern( View::Data& m, CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  m.view.MoveInBounds();

  const unsigned NUM_LINES = m.fb.NumLines();

  const unsigned OCL = m.view.CrsLine();
  const unsigned OCC = m.view.CrsChar();

  bool found_prev_star = false;

  // Search for first star position before current position
  for( int l=OCL; !found_prev_star && 0<=l; l-- )
  {
    const int LL = m.fb.LineLen( l );

    int p=LL-1;
    if( OCL==l ) p = OCC ? OCC-1 : 0;

    for( ; 0<p && !found_prev_star; p-- )
    {
      for( ; 0<=p && m.view.InStar(l,p); p-- )
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
      const unsigned LL = m.fb.LineLen( l );

      int p=LL-1;
      if( OCL==l ) p = OCC ? OCC-1 : 0;

      for( ; 0<p && !found_prev_star; p-- )
      {
        for( ; 0<=p && m.view.InStar(l,p); p-- )
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

bool Do_dw_get_fn( View::Data& m
                 , const unsigned  st_line, const unsigned  st_char
                 ,       unsigned& fn_line,       unsigned& fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned LL = m.fb.LineLen( st_line );
  const uint8_t  C  = m.fb.Get( st_line, st_char );

  if( IsSpace( C )      // On white space
    || ( st_char < LLM1(LL) // On non-white space before white space
       && IsSpace( m.fb.Get( st_line, st_char+1 ) ) ) )
  {
    // w:
    CrsPos ncp_w = { 0, 0 };
    bool ok = GoToNextWord_GetPosition( m, ncp_w );
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
  bool ok = GoToEndOfWord_GetPosition( m, ncp_e );

  if( ok && st_line == ncp_e.crsLine
         && st_char <= ncp_e.crsChar )
  {
    fn_line = ncp_e.crsLine;
    fn_char = ncp_e.crsChar;
    return true;
  }
  return false;
}

void Do_dd_Normal( View::Data& m, const unsigned ONL )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned OCL = m.view.CrsLine();           // Old cursor line
  const unsigned OCP = m.view.CrsChar();           // Old cursor position
  const unsigned OLL = m.fb.LineLen( OCL ); // Old line length

  const bool DELETING_LAST_LINE = OCL == ONL-1;

  const unsigned NCL = DELETING_LAST_LINE ? OCL-1 : OCL; // New cursor line
  const unsigned NLL = DELETING_LAST_LINE ? m.fb.LineLen( NCL )
                                          : m.fb.LineLen( NCL + 1 );
  const unsigned NCP = Min( OCP, 0<NLL ? NLL-1 : 0 );

  // Remove line from FileBuf and save in paste register:
  Line* lp = m.fb.RemoveLineP( OCL );
  if( lp ) {
    // m.reg will own nlp
    m.reg.clear();
    m.reg.push( lp );
    m.vis.SetPasteMode( PM_LINE );
  }
  m.view.GoToCrsPos_NoWrite( NCL, NCP );

  m.fb.Update();
}

void Do_dd_BufferEditor( View::Data& m, const unsigned ONL )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned OCL = m.view.CrsLine(); // Old cursor line

  // Can only delete one of the user files out of buffer editor
  if( CMD_FILE < OCL )
  {
    Line* lp = m.fb.GetLineP( OCL );

    const char* fname = lp->c_str( 0 );

    if( !m.vis.File_Is_Displayed( fname ) )
    {
      m.vis.ReleaseFileName( fname );

      Do_dd_Normal( m, ONL );
    }
  }
}

void Do_p_line( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = m.view.CrsLine();  // Old cursor line

  const unsigned NUM_LINES = m.reg.len();

  for( unsigned k=0; k<NUM_LINES; k++ )
  {
    // Put reg on line below:
    m.fb.InsertLine( OCL+k+1, *(m.reg[k]) );
  }
  // Update current view after other views,
  // so that the cursor will be put back in place
  m.fb.Update();
}

void Do_p_or_P_st_fn( View::Data& m, Paste_Pos paste_pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned N_REG_LINES = m.reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    const unsigned NLL = m.reg[k]->len();  // New line length
    const unsigned OCL = m.view.CrsLine();               // Old cursor line

    if( 0 == k ) // Add to current line
    {
      m.view.MoveInBounds();
      const unsigned OLL = m.fb.LineLen( OCL );
      const unsigned OCP = m.view.CrsChar();               // Old cursor position

      // If line we are pasting to is zero length, dont paste a space forward
    //const unsigned add_pos = OLL ? 1 : 0;
      const unsigned forward = OLL ? ( paste_pos==PP_After ? 1 : 0 ) : 0;

      for( unsigned i=0; i<NLL; i++ )
      {
        uint8_t C = m.reg[k]->get(i);

        m.fb.InsertChar( OCL, OCP+i+forward, C );
      }
      if( 1 < N_REG_LINES && OCP+forward < OLL ) // Move rest of first line onto new line below
      {
        m.fb.InsertLine( OCL+1 );
        for( unsigned i=0; i<(OLL-OCP-forward); i++ )
        {
          uint8_t C = m.fb.RemoveChar( OCL, OCP + NLL+forward );
          m.fb.PushChar( OCL+1, C );
        }
      }
    }
    else if( N_REG_LINES-1 == k )
    {
      // Insert a new line if at end of file:
      if( m.fb.NumLines() == OCL+k ) m.fb.InsertLine( OCL+k );

      for( unsigned i=0; i<NLL; i++ )
      {
        uint8_t C = m.reg[k]->get(i);

        m.fb.InsertChar( OCL+k, i, C );
      }
    }
    else {
      // Put reg on line below:
      m.fb.InsertLine( OCL+k, *(m.reg[k]) );
    }
  }
  // Update current view after other views, so that the cursor will be put back in place
  m.fb.Update();
}

void Do_p_block( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = m.view.CrsLine();    // Old cursor line
  const unsigned OCP = m.view.CrsChar();    // Old cursor position
  const unsigned OLL = m.fb.LineLen( OCL ); // Old line length
  const unsigned ISP = OCP ? OCP+1          // Insert position
                           : ( OLL ? 1:0 ); // If at beginning of line,
                                            // and LL is zero insert at 0,
                                            // else insert at 1
  const unsigned N_REG_LINES = m.reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    if( m.fb.NumLines()<=OCL+k ) m.fb.InsertLine( OCL+k );

    const unsigned LL = m.fb.LineLen( OCL+k );

    if( LL < ISP )
    {
      // Fill in line with white space up to ISP:
      for( unsigned i=0; i<(ISP-LL); i++ )
      {
        // Insert at end of line so undo will be atomic:
        const unsigned NLL = m.fb.LineLen( OCL+k ); // New line length
        m.fb.InsertChar( OCL+k, NLL, ' ' );
      }
    }
    Line& reg_line = *(m.reg[k]);
    const unsigned RLL = reg_line.len();

    for( unsigned i=0; i<RLL; i++ )
    {
      uint8_t C = reg_line.get(i);

      m.fb.InsertChar( OCL+k, ISP+i, C );
    }
  }
  m.fb.Update();
}

void Do_P_line( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = m.view.CrsLine();  // Old cursor line

  const unsigned NUM_LINES = m.reg.len();

  for( unsigned k=0; k<NUM_LINES; k++ )
  {
    // Put reg on line above:
    m.fb.InsertLine( OCL+k, *(m.reg[k]) );
  }
  m.fb.Update();
}

void Do_P_block( View::Data& m )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = m.view.CrsLine();  // Old cursor line
  const unsigned OCP = m.view.CrsChar();  // Old cursor position

  const unsigned N_REG_LINES = m.reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    if( m.fb.NumLines()<=OCL+k ) m.fb.InsertLine( OCL+k );

    const unsigned LL = m.fb.LineLen( OCL+k );

    if( LL < OCP )
    {
      // Fill in line with white space up to OCP:
      for( unsigned i=0; i<(OCP-LL); i++ ) m.fb.InsertChar( OCL+k, LL, ' ' );
    }
    Line& reg_line = *(m.reg[k]);
    const unsigned RLL = reg_line.len();

    for( unsigned i=0; i<RLL; i++ )
    {
      uint8_t C = reg_line.get(i);

      m.fb.InsertChar( OCL+k, OCP+i, C );
    }
  }
  m.fb.Update();
}

View::View( Vis& vis
          , Key& key
          , FileBuf& fb
          , LinesList& reg )
  : m( *new(__FILE__, __LINE__) Data( *this, vis, key, fb, reg ) )
{
}

View::~View()
{
  delete &m;
}

FileBuf* View::GetFB() const { return &m.fb; }

unsigned View::WinRows() const { return m.nRows; }
unsigned View::WinCols() const { return m.nCols; }
unsigned View::X() const { return m.x; }
unsigned View::Y() const { return m.y; }

unsigned View::GetTopLine () const { return m.topLine ; }
unsigned View::GetLeftChar() const { return m.leftChar; }
unsigned View::GetCrsRow  () const { return m.crsRow  ; }
unsigned View::GetCrsCol  () const { return m.crsCol  ; }

void View::SetTopLine ( const unsigned val ) { m.topLine  = val; }
void View::SetLeftChar( const unsigned val ) { m.leftChar = val; }
void View::SetCrsRow  ( const unsigned val ) { m.crsRow   = val; }
void View::SetCrsCol  ( const unsigned val ) { m.crsCol   = val; }

unsigned View::WorkingRows() const { return m.nRows-3-top___border-bottomborder; }
unsigned View::WorkingCols() const { return m.nCols-  left__border-right_border; }
unsigned View::CrsLine  () const { return m.topLine  + m.crsRow; }
unsigned View::BotLine  () const { return m.topLine  + WorkingRows()-1; }
unsigned View::CrsChar  () const { return m.leftChar + m.crsCol; }
unsigned View::RightChar() const { return m.leftChar + WorkingCols()-1; }

// Translates zero based working view row to zero based global row
unsigned View::Row_Win_2_GL( const unsigned win_row ) const
{
  return m.y + top___border + win_row;
}

// Translates zero based working view column to zero based global column
unsigned View::Col_Win_2_GL( const unsigned win_col ) const
{
  return m.x + left__border + win_col;
}

// Translates zero based file line number to zero based global row
unsigned View::Line_2_GL( const unsigned file_line ) const
{
  return m.y + top___border + file_line - m.topLine;
}

// Translates zero based file line char position to zero based global column
unsigned View::Char_2_GL( const unsigned line_char ) const
{
  return m.x + left__border + line_char - m.leftChar;
}

unsigned View::Sts__Line_Row() const
{
  return Row_Win_2_GL( WorkingRows() );
}

unsigned View::File_Line_Row() const
{
  return Row_Win_2_GL( WorkingRows() + 1 );
}

unsigned View::Cmd__Line_Row() const
{
  return Row_Win_2_GL( WorkingRows() + 2 );
}

Tile_Pos View::GetTilePos() const
{
  return m.tile_pos;
}

void View::SetTilePos( const Tile_Pos tp )
{
  m.tile_pos = tp;

  SetViewPos();
}

void View::SetViewPos()
{
  TilePos_2_x(m);
  TilePos_2_y(m);
  TilePos_2_nRows(m);
  TilePos_2_nCols(m);
}

bool View::GetInsertMode() const { return m.inInsertMode; }
void View::SetInsertMode( const bool val ) { m.inInsertMode = val; }
bool View::GetReplaceMode() const { return m.inReplaceMode; }
void View::SetReplaceMode( const bool val ) { m.inReplaceMode = val; }

void View::GoUp()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  if( OCL == 0 ) return;

  const unsigned NCL = OCL-1; // New cursor line

  const unsigned OCP = CrsChar(); // Old cursor position
        unsigned NCP = OCP;

  GoToCrsPos_Write( NCL, NCP );
}

void View::GoDown()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  if( OCL == NUM_LINES-1 ) return;

  const unsigned NCL = OCL+1; // New cursor line

  const unsigned OCP = CrsChar(); // Old cursor position
        unsigned NCP = OCP;

  GoToCrsPos_Write( NCL, NCP );
}

void View::GoLeft()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==m.fb.NumLines() ) return;

  const unsigned CL = CrsLine(); // Cursor line
  const unsigned CP = CrsChar(); // Cursor position

  if( CP == 0 ) return;

  GoToCrsPos_Write( CL, CP-1 );
}

void View::GoRight()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==m.fb.NumLines() ) return;

  const unsigned CL = CrsLine(); // Cursor line
  const unsigned LL = m.fb.LineLen( CL );
  if( 0==LL ) return;

  const unsigned CP = CrsChar(); // Cursor position
  if( LL-1 <= CP ) return;

  GoToCrsPos_Write( CL, CP+1 );
}

// This one works better when NOT in visual mode:
void View::PageDown()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();
  if( !NUM_LINES ) return;

  const unsigned newTopLine = m.topLine + WorkingRows() - 1;
  // Subtracting 1 above leaves one line in common between the 2 pages.

  if( newTopLine < NUM_LINES )
  {
    m.crsCol = 0;
    m.topLine = newTopLine;

    // Dont let cursor go past the end of the file:
    if( NUM_LINES <= CrsLine() )
    {
      // This line places the cursor at the top of the screen, which I prefer:
      m.crsRow = 0;
    }
    Update();
  }
}

void View::PageUp()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned OCL = CrsLine(); // Old cursor line
  const unsigned OCP = CrsChar(); // Old cursor position

  // Dont scroll if we are at the top of the file:
  if( m.topLine )
  {
    //Leave m.crsRow unchanged.
    m.crsCol = 0;

    // Dont scroll past the top of the file:
    if( m.topLine < WorkingRows() - 1 )
    {
      m.topLine = 0;
    }
    else {
      m.topLine -= WorkingRows() - 1;
    }
    Update();
  }
}

void View::GoToBegOfLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m.fb.NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line

  GoToCrsPos_Write( OCL, 0 );
}

void View::GoToEndOfLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m.fb.NumLines() ) return;

  const unsigned LL = m.fb.LineLen( CrsLine() );

  const unsigned OCL = CrsLine(); // Old cursor line

  if( m.inVisualMode && m.inVisualBlock )
  {
    // In Visual Block, $ puts cursor at the position
    // of the end of the longest line in the block
    unsigned max_LL = LL;

    for( unsigned L=m.v_st_line; L<=m.v_fn_line; L++ )
    {
      max_LL = Max( max_LL, m.fb.LineLen( L ) );
    }
    GoToCrsPos_Write( OCL, max_LL ? max_LL-1 : 0 );
  }
  else {
    GoToCrsPos_Write( OCL, LL ? LL-1 : 0 );
  }
}

void View::GoToBegOfNextLine()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = m.fb.NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  if( (NUM_LINES-1) <= OCL ) return; // On last line, so cant go down

  GoToCrsPos_Write( OCL+1, 0 );
}

void View::GoToTopLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToCrsPos_Write( m.topLine, 0 );
}

void View::GoToMidLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  // Default: Last line in file is not in view
  unsigned NCL = m.topLine + WorkingRows()/2; // New cursor line

  if( NUM_LINES-1 < BotLine() )
  {
    // Last line in file above bottom of view
    NCL = m.topLine + (NUM_LINES-1 - m.topLine)/2;
  }
  GoToCrsPos_Write( NCL, 0 );
}

void View::GoToBotLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  unsigned bottom_line_in_view = m.topLine + WorkingRows()-1;

  bottom_line_in_view = Min( NUM_LINES-1, bottom_line_in_view );

  GoToCrsPos_Write( bottom_line_in_view, 0 );
}

void View::GoToLine( const unsigned user_line_num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Internal line number is 1 less than user line number:
  const unsigned NCL = user_line_num - 1; // New cursor line number

  if( m.fb.NumLines() <= NCL )
  {
    PrintCursor();
  }
  else {
    GoToCrsPos_Write( NCL, 0 );
  }
}

void View::GoToTopOfFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToCrsPos_Write( 0, 0 );
}

void View::GoToEndOfFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  GoToCrsPos_Write( NUM_LINES-1, 0 );
}

void View::GoToNextWord()
{
  Trace trace( __PRETTY_FUNCTION__ );
  CrsPos ncp = { 0, 0 };

  if( GoToNextWord_GetPosition( m, ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

void View::GoToPrevWord()
{
  Trace trace( __PRETTY_FUNCTION__ );

  CrsPos ncp = { 0, 0 };

  if( GoToPrevWord_GetPosition( m, ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

void View::GoToEndOfWord()
{
  Trace trace( __PRETTY_FUNCTION__ );

  CrsPos ncp = { 0, 0 };

  if( GoToEndOfWord_GetPosition( m, ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

void View::GoToStartOfRow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m.fb.NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line

  GoToCrsPos_Write( OCL, m.leftChar );
}

void View::GoToEndOfRow()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m.fb.NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line

  const unsigned LL = m.fb.LineLen( OCL );
  if( 0==LL ) return;

  const unsigned NCP = Min( LL-1, m.leftChar + WorkingCols() - 1 );

  GoToCrsPos_Write( OCL, NCP );
}

void View::GoToOppositeBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();
  const unsigned NUM_LINES = m.fb.NumLines();
  const unsigned CL = CrsLine();
  const unsigned CC = CrsChar();
  const unsigned LL = m.fb.LineLen( CL );

  if( 0==NUM_LINES || 0==LL ) return;

  const char C = m.fb.Get( CL, CC );

  if( C=='{' || C=='[' || C=='(' )
  {
    char finish_char = 0;
    if     ( C=='{' ) finish_char = '}';
    else if( C=='[' ) finish_char = ']';
    else if( C=='(' ) finish_char = ')';
    else              ASSERT( __LINE__, 0, "Un-handled case" );

    GoToOppositeBracket_Forward( m, C, finish_char );
  }
  else if( C=='}' || C==']' || C==')' )
  {
    char finish_char = 0;
    if     ( C=='}' ) finish_char = '{';
    else if( C==']' ) finish_char = '[';
    else if( C==')' ) finish_char = '(';
    else              ASSERT( __LINE__, 0, "Un-handled case" );

    GoToOppositeBracket_Backward( m, C, finish_char );
  }
}

void View::GoToLeftSquigglyBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();

  const char  start_char = '}';
  const char finish_char = '{';
  GoToOppositeBracket_Backward( m, start_char, finish_char );
}

void View::GoToRightSquigglyBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();

  const char  start_char = '{';
  const char finish_char = '}';
  GoToOppositeBracket_Forward( m, start_char, finish_char );
}

void View::GoToCrsPos_NoWrite( const unsigned ncp_crsLine
                             , const unsigned ncp_crsChar )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // These moves refer to View of buffer:
  const bool MOVE_DOWN  = BotLine()   < ncp_crsLine;
  const bool MOVE_RIGHT = RightChar() < ncp_crsChar;
  const bool MOVE_UP    = ncp_crsLine < m.topLine;
  const bool MOVE_LEFT  = ncp_crsChar < m.leftChar;

  if     ( MOVE_DOWN ) m.topLine = ncp_crsLine - WorkingRows() + 1;
  else if( MOVE_UP   ) m.topLine = ncp_crsLine;
  m.crsRow  = ncp_crsLine - m.topLine;

  if     ( MOVE_RIGHT ) m.leftChar = ncp_crsChar - WorkingCols() + 1;
  else if( MOVE_LEFT  ) m.leftChar = ncp_crsChar;
  m.crsCol   = ncp_crsChar - m.leftChar;
}

void View::GoToCrsPos_Write( const unsigned ncp_crsLine
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
  if( m.inVisualMode )
  {
    m.v_fn_line = NCL;
    m.v_fn_char = NCP;
  }
  // These moves refer to View of buffer:
  const bool MOVE_DOWN  = BotLine()   < NCL;
  const bool MOVE_RIGHT = RightChar() < NCP;
  const bool MOVE_UP    = NCL < m.topLine;
  const bool MOVE_LEFT  = NCP < m.leftChar;

  const bool redraw = MOVE_DOWN || MOVE_RIGHT || MOVE_UP || MOVE_LEFT;

  if( redraw )
  {
    if     ( MOVE_DOWN ) m.topLine = NCL - WorkingRows() + 1;
    else if( MOVE_UP   ) m.topLine = NCL;

    if     ( MOVE_RIGHT ) m.leftChar = NCP - WorkingCols() + 1;
    else if( MOVE_LEFT  ) m.leftChar = NCP;

    // m.crsRow and m.crsCol must be set to new values before calling CalcNewCrsByte
    m.crsRow = NCL - m.topLine;
    m.crsCol = NCP - m.leftChar;

    Update();
  }
  else if( m.inVisualMode )
  {
    if( m.inVisualBlock ) GoToCrsPos_Write_VisualBlock( m, OCL, OCP, NCL, NCP );
    else                  GoToCrsPos_Write_Visual     ( m, OCL, OCP, NCL, NCP );
  }
  else {
    // m.crsRow and m.crsCol must be set to new values before calling CalcNewCrsByte and PrintCursor
    m.crsRow = NCL - m.topLine;
    m.crsCol = NCP - m.leftChar;

    PrintCursor();  // Put cursor into position.

    m.sts_line_needs_update = true;
  }
}

bool View::GoToFile_GetFileName( String& fname )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool got_filename = false;

  const unsigned CL = CrsLine();
  const unsigned LL = m.fb.LineLen( CL );

  if( LL )
  {
    MoveInBounds();
    const int CC = CrsChar();
    char c = m.fb.Get( CL, CC );

    if( IsFileNameChar( c ) )
    {
      // Get the file name:
      got_filename = true;

      fname.push( c );

      // Search backwards, until white space is found:
      for( int k=CC-1; -1<k; k-- )
      {
        c = m.fb.Get( CL, k );

        if( !IsFileNameChar( c ) ) break;
        else fname.insert( 0, c );
      }
      // Search forwards, until white space is found:
      for( unsigned k=CC+1; k<LL; k++ )
      {
        c = m.fb.Get( CL, k );

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
void View::GoToCmdLineClear( const char* S )
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

// If past end of line, move back to end of line.
// Returns true if moved, false otherwise.
//
bool View::MoveInBounds()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CL  = CrsLine();
  const unsigned LL  = m.fb.LineLen( CL );
  const unsigned EOL = LL ? LL-1 : 0;

  if( EOL < CrsChar() ) // Since cursor is now allowed past EOL,
  {                      // it may need to be moved back:
    GoToCrsPos_NoWrite( CL, EOL );
    return true;
  }
  return false;
}

void View::MoveCurrLineToTop()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.crsRow )
  {
    m.topLine += m.crsRow;
    m.crsRow = 0;
    Update();
  }
}

void View::MoveCurrLineCenter()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned center = SCast<unsigned>( 0.5*WorkingRows() + 0.5 );

  const unsigned OCL = CrsLine(); // Old cursor line

  if( 0 < OCL && OCL < center && 0 < m.topLine )
  {
    // Cursor line cannot be moved to center, but can be moved closer to center
    // CrsLine() does not change:
    m.crsRow += m.topLine;
    m.topLine = 0;
    Update();
  }
  else if( center <= OCL
        && center != m.crsRow )
  {
    m.topLine += m.crsRow - center;
    m.crsRow = center;
    Update();
  }
}

void View::MoveCurrLineToBottom()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 < m.topLine )
  {
    const unsigned WR  = WorkingRows();
    const unsigned OCL = CrsLine(); // Old cursor line

    if( WR-1 <= OCL )
    {
      m.topLine -= WR - m.crsRow - 1;
      m.crsRow = WR-1;
      Update();
    }
    else {
      // Cursor line cannot be moved to bottom, but can be moved closer to bottom
      // CrsLine() does not change:
      m.crsRow += m.topLine;
      m.topLine = 0;
      Update();
    }
  }
}

void View::Do_i()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.inInsertMode = true;
  DisplayBanner(m);

  if( 0 == m.fb.NumLines() ) m.fb.PushLine();

  const unsigned LL = m.fb.LineLen( CrsLine() );  // Line length

  if( LL < CrsChar() ) // Since cursor is now allowed past EOL,
  {                    // it may need to be moved back:
    // For user friendlyness, move cursor to new position immediately:
    GoToCrsPos_Write( CrsLine(), LL );
  }
  unsigned count = 0;
  for( char c=m.key.In(); c != ESC; c=m.key.In() )
  {
    if     ( IsEndOfLineDelim( c ) ) InsertAddReturn(m);
    else if( BS  == c
          || DEL == c ) { if( count ) InsertBackspace(m); }
    else                InsertAddChar( m, c );

    if( BS == c || DEL == c ) { if( count ) count--; }
    else count++;
  }
  Remove_Banner(m);
  m.inInsertMode = false;

  // Move cursor back one space:
  if( m.crsCol )
  {
    m.crsCol--;
    m.fb.Update();
  }
}

void View::Do_a()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.fb.NumLines()==0 ) { Do_i(); return; }

  const unsigned CL = CrsLine();
  const unsigned LL = m.fb.LineLen( CL );
  if( 0==LL ) { Do_i(); return; }

  const bool CURSOR_AT_EOL = ( CrsChar() == LL-1 );
  if( CURSOR_AT_EOL )
  {
    GoToCrsPos_NoWrite( CL, LL );
  }
  const bool CURSOR_AT_RIGHT_COL = ( m.crsCol == WorkingCols()-1 );

  if( CURSOR_AT_RIGHT_COL )
  {
    // Only need to scroll window right, and then enter insert i:
    m.leftChar++; //< This increments CrsChar()
  }
  else if( !CURSOR_AT_EOL ) // If cursor was at EOL, already moved cursor forward
  {
    // Only need to move cursor right, and then enter insert i:
    m.crsCol += 1; //< This increments CrsChar()
  }
  m.fb.Update();

  Do_i();
}

void View::Do_A()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToEndOfLine();

  Do_a();
}

void View::Do_o()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned ONL = m.fb.NumLines();
  const unsigned OCL = CrsLine();

  // Add the new line:
  const unsigned NCL = ( 0 != ONL ) ? OCL+1 : 0;
  m.fb.InsertLine( NCL );
  m.crsCol = 0;
  m.leftChar = 0;
  if     ( 0==ONL ) ; // Do nothing
  else if( OCL < BotLine() ) m.crsRow++;
  else {
    // If we were on the bottom working line, scroll screen down
    // one line so that the cursor line is not below the screen.
    m.topLine++;
  }
  m.fb.Update();

  Do_i();
}

void View::Do_O()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Add the new line:
  const unsigned new_line_num = CrsLine();
  m.fb.InsertLine( new_line_num );
  m.crsCol = 0;
  m.leftChar = 0;

  m.fb.Update();

  Do_i();
}

bool View::Do_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.inVisualBlock = false;

  return Do_visualMode(m);
}

bool View::Do_V()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.inVisualBlock = true;

  return Do_visualMode(m);
}

void View::Do_x()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // If there is nothing to 'x', just return:
  if( !m.fb.NumLines() ) return;

  const unsigned CL = CrsLine();
  const unsigned LL = m.fb.LineLen( CL );

  // If nothing on line, just return:
  if( !LL ) return;

  // If past end of line, move to end of line:
  if( LL-1 < CrsChar() )
  {
    GoToCrsPos_Write( CL, LL-1 );
  }
  const uint8_t C = m.fb.RemoveChar( CL, CrsChar() );

  // Put char x'ed into register:
  Line* nlp = m.vis.BorrowLine( __FILE__,__LINE__ );
  nlp->push(__FILE__,__LINE__, C );
  m.reg.clear();
  m.reg.push( nlp );
  m.vis.SetPasteMode( PM_ST_FN );

  const unsigned NLL = m.fb.LineLen( CL ); // New line length

  // Reposition the cursor:
  if( NLL <= m.leftChar+m.crsCol )
  {
    // The char x'ed is the last char on the line, so move the cursor
    //   back one space.  Above, a char was removed from the line,
    //   but m.crsCol has not changed, so the last char is now NLL.
    // If cursor is not at beginning of line, move it back one more space.
    if( m.crsCol ) m.crsCol--;
  }
  m.fb.Update();
}

void View::Do_s()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CL  = CrsLine();
  const unsigned LL  = m.fb.LineLen( CL );
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
      m.fb.PushChar( CL, ' ' );
    }
    Do_a();
  }
  else // CP == EOL
  {
    Do_x();
    Do_a();
  }
}

void View::Do_cw()
{
  const unsigned result = Do_dw();

  if     ( result==1 ) Do_i();
  else if( result==2 ) Do_a();
}

// If nothing was deleted, return 0.
// If last char on line was deleted, return 2,
// Else return 1.
int View::Do_dw()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned st_line = CrsLine();
  const unsigned st_char = CrsChar();

  const unsigned LL = m.fb.LineLen( st_line );

  // If past end of line, nothing to do
  if( st_char < LL )
  {
    // Determine fn_line, fn_char:
    unsigned fn_line = 0;
    unsigned fn_char = 0;

    if( Do_dw_get_fn( m, st_line, st_char, fn_line, fn_char ) )
    {
      Do_x_range( m, st_line, st_char, fn_line, fn_char );

      bool deleted_last_char = fn_char == LL-1;

      return deleted_last_char ? 2 : 1;
    }
  }
  return 0;
}

void View::Do_D()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();
  const unsigned OCL = CrsLine();  // Old cursor line
  const unsigned OCP = CrsChar();  // Old cursor position
  const unsigned OLL = m.fb.LineLen( OCL );  // Old line length

  // If there is nothing to 'D', just return:
  if( !NUM_LINES || !OLL || OLL-1 < OCP ) return;

  Line* lpd = m.vis.BorrowLine( __FILE__,__LINE__ );

  for( unsigned k=OCP; k<OLL; k++ )
  {
    uint8_t c = m.fb.RemoveChar( OCL, OCP );
    lpd->push(__FILE__,__LINE__, c );
  }
  m.reg.clear();
  m.reg.push( lpd );
  m.vis.SetPasteMode( PM_ST_FN );

  // If cursor is not at beginning of line, move it back one space.
  if( m.crsCol ) m.crsCol--;

  m.fb.Update();
}

void View::Do_f( const char FAST_CHAR )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine();           // Old cursor line
  const unsigned LL  = m.fb.LineLen( OCL ); // Line length
  const unsigned OCP = CrsChar();           // Old cursor position

  if( LL-1 <= OCP ) return;

  unsigned NCP = 0;
  bool found_char = false;
  for( unsigned p=OCP+1; !found_char && p<LL; p++ )
  {
    const char C = m.fb.Get( OCL, p );

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

// Go to next pattern
void View::Do_n()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  if( NUM_LINES == 0 ) return;

  CrsPos ncp = { 0, 0 }; // Next cursor position

  if( Do_n_FindNextPattern( m, ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
  else PrintCursor();
}

// Go to previous pattern
void View::Do_N()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();

  if( NUM_LINES == 0 ) return;

  CrsPos ncp = { 0, 0 }; // Next cursor position

  if( Do_N_FindPrevPattern( m, ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

void View::Do_dd()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned ONL = m.fb.NumLines(); // Old number of lines

  // If there is nothing to 'dd', just return:
  if( 1 < ONL )
  {
    if( &m.fb == m.vis.GetFileBuf( BE_FILE ) )
    {
      Do_dd_BufferEditor( m, ONL );
    }
    else {
      Do_dd_Normal( m, ONL );
    }
  }
}

void View::Do_yy()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // If there is nothing to 'yy', just return:
  if( !m.fb.NumLines() ) return;

  Line l = m.fb.GetLine( CrsLine() );

  m.reg.clear();
  m.reg.push( m.vis.BorrowLine( __FILE__,__LINE__, l ) );

  m.vis.SetPasteMode( PM_LINE );
}

void View::Do_yw()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // If there is nothing to 'yw', just return:
  if( !m.fb.NumLines() ) return;

  const unsigned st_line = CrsLine();
  const unsigned st_char = CrsChar();

  // Determine fn_line, fn_char:
  unsigned fn_line = 0;
  unsigned fn_char = 0;

  if( Do_dw_get_fn( m, st_line, st_char, fn_line, fn_char ) )
  {
    m.reg.clear();
    m.reg.push( m.vis.BorrowLine( __FILE__,__LINE__ ) );

    // st_line and fn_line should be the same
    for( unsigned k=st_char; k<=fn_char; k++ )
    {
      m.reg[0]->push(__FILE__,__LINE__, m.fb.Get( st_line, k ) );
    }
    m.vis.SetPasteMode( PM_ST_FN );
  }
}

void View::Do_p()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const Paste_Mode PM = m.vis.GetPasteMode();

  if     ( PM_ST_FN == PM ) return Do_p_or_P_st_fn( m, PP_After );
  else if( PM_BLOCK == PM ) return Do_p_block(m);
  else /*( PM_LINE  == PM*/ return Do_p_line(m);
}

void View::Do_P()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const Paste_Mode PM = m.vis.GetPasteMode();

  if     ( PM_ST_FN == PM ) return Do_p_or_P_st_fn( m, PP_Before );
  else if( PM_BLOCK == PM ) return Do_P_block(m);
  else /*( PM_LINE  == PM*/ return Do_P_line(m);
}

void View::Do_R()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.inReplaceMode = true;
  DisplayBanner(m);

  if( m.fb.NumLines()==0 ) m.fb.PushLine();

  for( char c=m.key.In(); c != ESC; c=m.key.In() )
  {
    if( BS == c || DEL == c ) m.fb.Undo( m.view );
    else {
      if( '\n' == c ) ReplaceAddReturn(m);
      else            ReplaceAddChars( m, c );
    }
  }
  Remove_Banner(m);
  m.inReplaceMode = false;

  // Move cursor back one space:
  if( m.crsCol )
  {
    m.crsCol--;  // Move cursor back one space.
  }
  m.fb.Update();
}

void View::Do_J()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines(); // Number of lines
  const unsigned CL        = CrsLine();       // Cursor line

  const bool ON_LAST_LINE = ( CL == NUM_LINES-1 );

  if( ON_LAST_LINE || NUM_LINES < 2 ) return;

  GoToEndOfLine();

  Line* lp = m.fb.RemoveLineP( CL+1 );
  m.fb.AppendLineToLine( CL  , lp );

  // Update() is less efficient than only updating part of the screen,
  //   but it makes the code simpler.
  m.fb.Update();
}

void View::Do_Tilda()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==m.fb.NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  const unsigned OCP = CrsChar(); // Old cursor position
  const unsigned LL  = m.fb.LineLen( OCL );

  if( !LL || LL-1 < OCP ) return;

  char c = m.fb.Get( CrsLine(), CrsChar() );
  bool changed = false;
  if     ( isupper( c ) ) { c = tolower( c ); changed = true; }
  else if( islower( c ) ) { c = toupper( c ); changed = true; }

  if( m.crsCol < Min( LL-1, WorkingCols()-1 ) )
  {
    if( changed ) m.fb.Set( CrsLine(), CrsChar(), c );
    // Need to move cursor right:
    m.crsCol++;
  }
  else if( RightChar() < LL-1 )
  {
    // Need to scroll window right:
    if( changed ) m.fb.Set( CrsLine(), CrsChar(), c );
    m.leftChar++;
  }
  else // RightChar() == LL-1
  {
    // At end of line so cant move or scroll right:
    if( changed ) m.fb.Set( CrsLine(), CrsChar(), c );
  }
  m.fb.Update();
}

void View::Do_u()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.fb.Undo( m.view );
}

void View::Do_U()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.fb.UndoAll( m.view );
}

bool View::InVisualArea( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.inVisualMode )
  {
    if( m.inVisualBlock ) return InVisualBlock( line, pos );
    else                  return InVisualStFn ( line, pos );
  }
  return false;
}

bool View::InVisualStFn( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( m.v_st_line == line && line == m.v_fn_line )
  {
    return (m.v_st_char <= pos && pos <= m.v_fn_char)
        || (m.v_fn_char <= pos && pos <= m.v_st_char);
  }
  else if( (m.v_st_line < line && line < m.v_fn_line)
        || (m.v_fn_line < line && line < m.v_st_line) )
  {
    return true;
  }
  else if( m.v_st_line == line && line < m.v_fn_line )
  {
    return m.v_st_char <= pos;
  }
  else if( m.v_fn_line == line && line < m.v_st_line )
  {
    return m.v_fn_char <= pos;
  }
  else if( m.v_st_line < line && line == m.v_fn_line )
  {
    return pos <= m.v_fn_char;
  }
  else if( m.v_fn_line < line && line == m.v_st_line )
  {
    return pos <= m.v_st_char;
  }
  return false;
}

bool View::InVisualBlock( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return ( m.v_st_line <= line && line <= m.v_fn_line
        && m.v_st_char <= pos  && pos  <= m.v_fn_char ) // bot rite
      || ( m.v_st_line <= line && line <= m.v_fn_line
        && m.v_fn_char <= pos  && pos  <= m.v_st_char ) // bot left
      || ( m.v_fn_line <= line && line <= m.v_st_line
        && m.v_st_char <= pos  && pos  <= m.v_fn_char ) // top rite
      || ( m.v_fn_line <= line && line <= m.v_st_line
        && m.v_fn_char <= pos  && pos  <= m.v_st_char );// top left
}

bool View::InComment( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_COMMENT );
}

bool View::InDefine( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_DEFINE );
}

bool View::InConst( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_CONST );
}

bool View::InControl( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_CONTROL );
}

bool View::InVarType( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_VARTYPE );
}

bool View::InStar( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_STAR );
}

bool View::InNonAscii( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return m.fb.HasStyle( line, pos, HI_NONASCII );
}

// 1. Re-position window if needed
// 2. Draw borders
// 4. Re-draw working window
// 5. Re-draw status line
// 6. Re-draw file-name line
// 7. Draw command line
// 8. Put cursor back in window
//
void View::Update()
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.fb.Find_Styles( m.topLine + WorkingRows() );
  m.fb.ClearStars();
  m.fb.Find_Stars();

  RepositionView();
  Print_Borders();
  PrintWorkingView();
  PrintStsLine();
  PrintFileLine();
  PrintCmdLine();

  Console::Update();

  if( m.vis.CV() == &m.view ) PrintCursor();

  Console::Flush();
}

void View::RepositionView()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If a window re-size has taken place, and the window has gotten
  // smaller, change top line and left char if needed, so that the
  // cursor is in the buffer when it is re-drawn
  if( WorkingRows() <= m.crsRow )
  {
    m.topLine += ( m.crsRow - WorkingRows() + 1 );
    m.crsRow  -= ( m.crsRow - WorkingRows() + 1 );
  }
  if( WorkingCols() <= m.crsCol )
  {
    m.leftChar += ( m.crsCol - WorkingCols() + 1 );
    m.crsCol   -= ( m.crsCol - WorkingCols() + 1 );
  }
}

void View::Print_Borders()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const bool HIGHLIGHT = ( 1 < m.vis.GetNumWins() ) && ( &m.view == m.vis.CV() );

  const Style S = HIGHLIGHT ? S_BORDER_HI : S_BORDER;

  Print_Borders_Top   ( m, S );
  Print_Borders_Right ( m, S );
  Print_Borders_Left  ( m, S );
  Print_Borders_Bottom( m, S );
}

// Formats the status line.
// Must already be at the status line row.
// Returns the number of char's copied into ptr,
//   not including the terminating NULL.
//
void View::PrintStsLine()
{
  Trace trace( __PRETTY_FUNCTION__ );
  char buf1[  16]; buf1[0] = 0;
  char buf2[1024]; buf2[0] = 0;

  const unsigned CL = CrsLine(); // Line position
  const unsigned CC = CrsChar(); // Char position
  const unsigned LL = m.fb.NumLines() ? m.fb.LineLen( CL ) : 0;
  const unsigned WC = WorkingCols();

  // When inserting text at the end of a line, CrsChar() == LL
  if( LL && CC < LL ) // Print current char info:
  {
    const int c = m.fb.Get( CL, CC );

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
  const unsigned fileSize = m.fb.GetSize();
  const unsigned  crsByte = m.fb.GetCursorByte( CL, CC );
  char percent = SCast<char>(100*double(crsByte)/double(fileSize) + 0.5);
  // Screen width so far
  char* p = buf2;

  p += sprintf( buf2, "Pos=(%u,%u)  (%i%%, %u/%u)  Char=(%s)  "
                    , CL+1, CC+1, percent, crsByte, m.fb.GetSize(), buf1 );
  const unsigned SW = p - buf2; // Screen width so far

  if     ( SW < WC ) { for( unsigned k=SW; k<WC; k++ ) *p++ = ' '; }
  else if( WC < SW ) { p = buf2 + WC; /* Truncate extra part */ }
  *p = 0;

  Console::SetS( Sts__Line_Row(), Col_Win_2_GL( 0 ), buf2, S_STATUS );
}

void View::PrintFileLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned WC = WorkingCols();
  const unsigned FILE_NAME_LEN = strlen( m.fb.GetFileName() );

  char buf[1024]; buf[0] = 0;
  char* p = buf;

  if( WC < FILE_NAME_LEN )
  {
    // file_name does not fit, so truncate beginning
    p += sprintf( buf, "%s", m.fb.GetFileName() + (FILE_NAME_LEN - WC) );
  }
  else {
    // file_name fits, add spaces at end
    p += sprintf( buf, "%s", m.fb.GetFileName() );
    for( unsigned k=0; k<(WC-FILE_NAME_LEN); k++ ) *p++ = ' ';
  }
  *p = 0;

  Console::SetS( File_Line_Row(), Col_Win_2_GL( 0 ), buf, S_STATUS );
}

void View::PrintCmdLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Assumes you are already at the command line,
  // and just prints "--INSERT--" banner, and/or clears command line

  unsigned col=0;
  // Draw insert banner if needed
  if( m.inInsertMode )
  {
    col=10; // Strlen of "--INSERT--"
    Console::SetS( Cmd__Line_Row(), Col_Win_2_GL( 0 ), "--INSERT--", S_BANNER );
  }
  else if( m.inReplaceMode )
  {
    col=11; // Strlen of "--REPLACE--"
    Console::SetS( Cmd__Line_Row(), Col_Win_2_GL( 0 ), "--REPLACE--", S_BANNER );
  }
  else if( m.vis.RunningCmd() && m.vis.CV() == &m.view )
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

void View::PrintWorkingView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();
  const unsigned WR        = WorkingRows();
  const unsigned WC        = WorkingCols();

  unsigned row = 0;
  for( unsigned k=m.topLine; k<NUM_LINES && row<WR; k++, row++ )
  {
    // Dont allow line wrap:
    const unsigned LL    = m.fb.LineLen( k );
    const unsigned G_ROW = Row_Win_2_GL( row );
    unsigned col=0;
    for( unsigned i=m.leftChar; i<LL && col<WC; i++, col++ )
    {
      Style s    = Get_Style( m, k, i );
      int   byte = m.fb.Get( k, i );

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


void View::PrintWorkingView_Set( const unsigned LL
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
    Console::Set( G_ROW, Col_Win_2_GL( col ), byte, s );
  }
}

// Moves cursor into position on screen:
//
void View::PrintCursor()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Either one of these should work:
  Console::Move_2_Row_Col( Row_Win_2_GL( m.crsRow ), Col_Win_2_GL( m.crsCol ) );
  Console::Flush();
}

void View::DisplayMapping()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char* mapping = "--MAPPING--";
  const int   mapping_len = strlen( mapping );

  // Command line row in window:
  const unsigned WIN_ROW = WorkingRows() + 2;

  const unsigned G_ROW = Row_Win_2_GL( WIN_ROW );
  const unsigned G_COL = Col_Win_2_GL( m.nCols-1-right_border-mapping_len );

  Console::SetS( G_ROW, G_COL, mapping, S_BANNER );

  Console::Update();
  PrintCursor(); // Put cursor back in position.
}

bool View::GetStsLineNeedsUpdate() const
{
  return m.sts_line_needs_update;
}

bool View::GetUnSavedChangeSts() const
{
  return m.us_change_sts;
}

void View::SetStsLineNeedsUpdate( const bool val )
{
  m.sts_line_needs_update = val;
}

void View::SetUnSavedChangeSts( const bool val )
{
  m.us_change_sts = val;
}

String View::Do_Star_GetNewPattern()
{
  Trace trace( __PRETTY_FUNCTION__ );
  String new_star;

  if( m.fb.NumLines() == 0 ) return new_star;

  const unsigned CL = CrsLine();
  const unsigned LL = m.fb.LineLen( CL );

  if( LL )
  {
    MoveInBounds();
    const unsigned CC = CrsChar();

    const int c = m.fb.Get( CL,  CC );

    if( isalnum( c ) || c=='_' )
    {
      new_star.push( c );

      // Search forward:
      for( unsigned k=CC+1; k<LL; k++ )
      {
        const int c = m.fb.Get( CL, k );
        if( isalnum( c ) || c=='_' ) new_star.push( c );
        else                         break;
      }
      // Search backward:
      for( int k=CC-1; 0<=k; k-- )
      {
        const int c = m.fb.Get( CL, k );
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

void View::PrintPatterns( const bool HIGHLIGHT )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = m.fb.NumLines();
  const unsigned END_LINE  = Min( m.topLine+WorkingRows(), NUM_LINES );

  for( unsigned l=m.topLine; l<END_LINE; l++ )
  {
    const unsigned LL      = m.fb.LineLen( l );
    const unsigned END_POS = Min( m.leftChar+WorkingCols(), LL );

    for( unsigned p=m.leftChar; p<END_POS; p++ )
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
        int byte = m.fb.Get( l, p );
        Console::Set( Line_2_GL( l ), Char_2_GL( p ), byte, s );
      }
    }
  }
}

void View::Clear_Context()
{
  m.topLine  = 0;
  m.leftChar = 0;
  m.crsRow   = 0;
  m.crsCol   = 0;
}

bool View::Has_Context()
{
  return 0 != m.topLine
      || 0 != m.leftChar
      || 0 != m.crsRow
      || 0 != m.crsCol ;
}

void View::Set_Context( View& vr )
{
  Trace trace( __PRETTY_FUNCTION__ );

  m.topLine  = vr.GetTopLine ();
  m.leftChar = vr.GetLeftChar();
  m.crsRow   = vr.GetCrsRow  ();
  m.crsCol   = vr.GetCrsCol  ();
}

// Change directory to that of file of view:
//
bool View::GoToDir()
{
  Trace trace( __PRETTY_FUNCTION__ );

  char* fname_str = const_cast<char*>( m.fb.GetFileName() );

  if( m.fb.IsDir() )
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

