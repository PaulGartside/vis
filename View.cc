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

#include "Console.hh"
#include "MemLog.hh"
#include "FileBuf.hh"
#include "Utilities.hh"
#include "Key.hh"
#include "Vis.hh"
#include "View.hh"

extern Vis* gl_pVis;
extern Key* gl_pKey;
extern MemLog<MEM_LOG_BUF_SIZE> Log;

View::View( FileBuf* _pfb )
  : pfb  ( _pfb )
  , sts_line_needs_update( false )
  , us_change_sts( false )
  , tile_pos( TP_FULL )
  , nCols( Console::Num_Cols() )
  , nRows( Console::Num_Rows() )
  , x( 0 )
  , y( 0 )
  , topLine( 0 )
  , topLine_displayed( topLine )
  , leftChar( 0 )
  , leftChar_displayed( leftChar )
  , crsRow( 0 )
  , crsRow_displayed( crsRow )
  , crsCol( 0 )
  , crsCol_displayed( crsCol )
  , v_st_line( 0 )
  , v_st_char( 0 )
  , v_fn_line( 0 )
  , v_fn_char( 0 )
  , inInsertMode( false )
  , inReplaceMode( false )
  , inVisualMode( false )
  , inVisualBlock( false )
{
}

// Translates zero based working view row to zero based global row
unsigned View::Row_Win_2_GL( const unsigned win_row ) const
{
  return y + top___border + win_row;
}

// Translates zero based working view column to zero based global column
unsigned View::Col_Win_2_GL( const unsigned win_col ) const
{
  return x + left__border + win_col;
}

// Translates zero based file line number to zero based global row
unsigned View::Line_2_GL( const unsigned file_line ) const
{
  return y + top___border + file_line - topLine;
}

// Translates zero based file line char position to zero based global column
unsigned View::Char_2_GL( const unsigned line_char ) const
{
  return x + left__border + line_char - leftChar;
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

void View::SetTilePos( const Tile_Pos tp )
{
  tile_pos = tp;

  SetViewPos();
}

void View::SetViewPos()
{
  TilePos_2_x();
  TilePos_2_y();
  TilePos_2_nRows();
  TilePos_2_nCols();
}

void View::TilePos_2_x()
{
  const unsigned CON_COLS = Console::Num_Cols();

  // TP_FULL     , TP_BOT__HALF    , TP_LEFT_QTR
  // TP_LEFT_HALF, TP_TOP__LEFT_QTR, TP_TOP__LEFT_8TH
  // TP_TOP__HALF, TP_BOT__LEFT_QTR, TP_BOT__LEFT_8TH
  x = 0;

  if( TP_RITE_HALF         == tile_pos
   || TP_TOP__RITE_QTR     == tile_pos
   || TP_BOT__RITE_QTR     == tile_pos
   || TP_RITE_CTR__QTR     == tile_pos
   || TP_TOP__RITE_CTR_8TH == tile_pos
   || TP_BOT__RITE_CTR_8TH == tile_pos )
  {
    x = CON_COLS/2;
  }
  else if( TP_LEFT_CTR__QTR     == tile_pos
        || TP_TOP__LEFT_CTR_8TH == tile_pos
        || TP_BOT__LEFT_CTR_8TH == tile_pos )
  {
    x = CON_COLS/4;
  }
  else if( TP_RITE_QTR      == tile_pos
        || TP_TOP__RITE_8TH == tile_pos
        || TP_BOT__RITE_8TH == tile_pos )
  {
    x = CON_COLS*3/4;
  }
}

void View::TilePos_2_y()
{
  const unsigned CON_ROWS = Console::Num_Rows();

  // TP_FULL         , TP_LEFT_CTR__QTR
  // TP_LEFT_HALF    , TP_RITE_CTR__QTR
  // TP_RITE_HALF    , TP_RITE_QTR
  // TP_TOP__HALF    , TP_TOP__LEFT_8TH
  // TP_TOP__LEFT_QTR, TP_TOP__LEFT_CTR_8TH
  // TP_TOP__RITE_QTR, TP_TOP__RITE_CTR_8TH
  // TP_LEFT_QTR     , TP_TOP__RITE_8TH
  y = 0;

  if( TP_BOT__HALF         == tile_pos
   || TP_BOT__LEFT_QTR     == tile_pos
   || TP_BOT__RITE_QTR     == tile_pos
   || TP_BOT__LEFT_8TH     == tile_pos
   || TP_BOT__LEFT_CTR_8TH == tile_pos
   || TP_BOT__RITE_CTR_8TH == tile_pos
   || TP_BOT__RITE_8TH     == tile_pos )
  {
    y = CON_ROWS/2;
  }
}

void View::TilePos_2_nRows()
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
  nRows = CON_ROWS/2;

  if( TP_FULL          == tile_pos
   || TP_LEFT_HALF     == tile_pos
   || TP_RITE_HALF     == tile_pos
   || TP_LEFT_QTR      == tile_pos
   || TP_LEFT_CTR__QTR == tile_pos
   || TP_RITE_CTR__QTR == tile_pos
   || TP_RITE_QTR      == tile_pos )
  {
    nRows = CON_ROWS;
  }
  if( ODD_ROWS && ( TP_BOT__HALF         == tile_pos
                 || TP_BOT__LEFT_QTR     == tile_pos
                 || TP_BOT__RITE_QTR     == tile_pos
                 || TP_BOT__LEFT_8TH     == tile_pos
                 || TP_BOT__LEFT_CTR_8TH == tile_pos
                 || TP_BOT__RITE_CTR_8TH == tile_pos
                 || TP_BOT__RITE_8TH     == tile_pos ) )
  {
    nRows++;
  }
}

void View::TilePos_2_nCols()
{
  const unsigned CON_COLS = Console::Num_Cols();
  const unsigned ODD_COLS = CON_COLS%4;

  // TP_LEFT_QTR     , TP_TOP__LEFT_8TH    , TP_BOT__LEFT_8TH    ,
  // TP_LEFT_CTR__QTR, TP_TOP__LEFT_CTR_8TH, TP_BOT__LEFT_CTR_8TH,
  // TP_RITE_CTR__QTR, TP_TOP__RITE_CTR_8TH, TP_BOT__RITE_CTR_8TH,
  // TP_RITE_QTR     , TP_TOP__RITE_8TH    , TP_BOT__RITE_8TH    ,
  nCols = CON_COLS/4;

  if( TP_FULL      == tile_pos
   || TP_TOP__HALF == tile_pos
   || TP_BOT__HALF == tile_pos )
  {
    nCols = CON_COLS;
  }
  else if( TP_LEFT_HALF     == tile_pos
        || TP_RITE_HALF     == tile_pos
        || TP_TOP__LEFT_QTR == tile_pos
        || TP_TOP__RITE_QTR == tile_pos
        || TP_BOT__LEFT_QTR == tile_pos
        || TP_BOT__RITE_QTR == tile_pos )
  {
    nCols = CON_COLS/2;
  }
  if( ((TP_RITE_HALF         == tile_pos) && (ODD_COLS==1 || ODD_COLS==3))
   || ((TP_TOP__RITE_QTR     == tile_pos) && (ODD_COLS==1 || ODD_COLS==3))
   || ((TP_BOT__RITE_QTR     == tile_pos) && (ODD_COLS==1 || ODD_COLS==3))

   || ((TP_RITE_QTR          == tile_pos) && (ODD_COLS==1 || ODD_COLS==2 || ODD_COLS==3))
   || ((TP_TOP__RITE_8TH     == tile_pos) && (ODD_COLS==1 || ODD_COLS==2 || ODD_COLS==3))
   || ((TP_BOT__RITE_8TH     == tile_pos) && (ODD_COLS==1 || ODD_COLS==2 || ODD_COLS==3))

   || ((TP_LEFT_CTR__QTR     == tile_pos) && (ODD_COLS==2 || ODD_COLS==3))
   || ((TP_TOP__LEFT_CTR_8TH == tile_pos) && (ODD_COLS==2 || ODD_COLS==3))
   || ((TP_BOT__LEFT_CTR_8TH == tile_pos) && (ODD_COLS==2 || ODD_COLS==3))

   || ((TP_RITE_CTR__QTR     == tile_pos) && (ODD_COLS==3))
   || ((TP_TOP__RITE_CTR_8TH == tile_pos) && (ODD_COLS==3))
   || ((TP_BOT__RITE_CTR_8TH == tile_pos) && (ODD_COLS==3)) )
  {
    nCols++;
  }
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

  pfb->Find_Styles( topLine + WorkingRows() );
  pfb->ClearStars();
  pfb->Find_Stars();

  RepositionView();
  Print_Borders();
  PrintWorkingView();
  PrintStsLine();
  PrintFileLine();
  PrintCmdLine();

  Console::Update();

  if( gl_pVis->CV() == this ) PrintCursor();

  Console::Flush();
}

void View::UpdateLines( const unsigned st_line, const unsigned fn_line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Figure out which lines are currently on screen:
  unsigned m_st_line = st_line;
  unsigned m_fn_line = fn_line;

  if( m_st_line < topLine   ) m_st_line = topLine;
  if( BotLine() < m_fn_line ) m_fn_line = BotLine();
  if( m_fn_line < m_st_line ) return; // Nothing to update

  // Re-draw lines:
  PrintLines( m_st_line, m_fn_line );
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

void View::Do_i()
{
  Trace trace( __PRETTY_FUNCTION__ );

  inInsertMode = true;
  DisplayBanner();

  if( 0 == pfb->NumLines() ) pfb->PushLine();

  const unsigned LL = pfb->LineLen( CrsLine() );  // Line length

  if( LL < CrsChar() ) // Since cursor is now allowed past EOL,
  {                    // it may need to be moved back:
    // For user friendlyness, move cursor to new position immediately:
    GoToCrsPos_Write( CrsLine(), LL );
  }
  unsigned count = 0;
  for( char c=gl_pKey->In(); c != ESC; c=gl_pKey->In() )
  {
    if     ( IsEndOfLineDelim( c ) ) InsertAddReturn();
    else if( BS  == c
          || DEL == c ) { if( count ) InsertBackspace(); }
    else                InsertAddChar( c );

    if( BS == c || DEL == c ) { if( count ) count--; }
    else count++;
  }
  Remove_Banner();
  inInsertMode = false;

  // Move cursor back one space:
  if( crsCol )
  {
    crsCol--;
    pfb->Update();
  }
}

void View::Do_a()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( pfb->NumLines()==0 ) { Do_i(); return; }

  const unsigned CL = CrsLine();
  const unsigned LL = pfb->LineLen( CL );
  if( 0==LL ) { Do_i(); return; }

  const bool CURSOR_AT_EOL = ( CrsChar() == LL-1 );
  if( CURSOR_AT_EOL )
  {
    GoToCrsPos_NoWrite( CL, LL );
  }
  const bool CURSOR_AT_RIGHT_COL = ( crsCol == WorkingCols()-1 );

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
  pfb->Update();

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

  const unsigned ONL = pfb->NumLines();
  const unsigned OCL = CrsLine();

  // Add the new line:
  const unsigned NCL = ( 0 != ONL ) ? OCL+1 : 0;
  pfb->InsertLine( NCL );
  crsCol = 0;
  leftChar = 0;
  if     ( 0==ONL ) ; // Do nothing
  else if( OCL < BotLine() ) crsRow++;
  else {
    // If we were on the bottom working line, scroll screen down
    // one line so that the cursor line is not below the screen.
    topLine++;
  }
  pfb->Update();

  Do_i();
}

void View::Do_O()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Add the new line:
  const unsigned new_line_num = CrsLine();
  pfb->InsertLine( new_line_num );
  crsCol = 0;
  leftChar = 0;

  pfb->Update();

  Do_i();
}

void View::InsertAddChar( const char c )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( pfb->NumLines()==0 ) pfb->PushLine();

  pfb->InsertChar( CrsLine(), CrsChar(), c );

  if( WorkingCols() <= crsCol+1 )
  {
    // On last working column, need to scroll right:
    leftChar++;
  }
  else {
    crsCol += 1;
  }
  pfb->Update();
}

void View::InsertAddReturn()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // The lines in fb do not end with '\n's.
  // When the file is written, '\n's are added to the ends of the lines.
  Line new_line(__FILE__,__LINE__);
  const unsigned OLL = pfb->LineLen( CrsLine() );  // Old line length
  const unsigned OCP = CrsChar();                  // Old cursor position

  for( unsigned k=OCP; k<OLL; k++ )
  {
    const uint8_t C = pfb->RemoveChar( CrsLine(), OCP );
    bool ok = new_line.push(__FILE__,__LINE__, C );
    ASSERT( __LINE__, ok, "ok" );
  }
  // Truncate the rest of the old line:
  // Add the new line:
  const unsigned new_line_num = CrsLine()+1;
  pfb->InsertLine( new_line_num, new_line );
  crsCol = 0;
  leftChar = 0;
  if( CrsLine() < BotLine() ) crsRow++;
  else {
    // If we were on the bottom working line, scroll screen down
    // one line so that the cursor line is not below the screen.
    topLine++;
  }
  pfb->Update();
}

void View::InsertBackspace()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If no lines in buffer, no backspacing to be done
  if( 0==pfb->NumLines() ) return;

  const unsigned OCL = CrsLine();  // Old cursor line

  const unsigned OCP = CrsChar();            // Old cursor position
  const unsigned OLL = pfb->LineLen( OCL );  // Old line length

  if( OCP ) InsertBackspace_RmC ( OCL, OCP );
  else      InsertBackspace_RmNL( OCL );
}

void View::InsertBackspace_RmC( const unsigned OCL
                              , const unsigned OCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  pfb->RemoveChar( OCL, OCP-1 );

  crsCol -= 1;

  pfb->Update();
}

void View::InsertBackspace_RmNL( const unsigned OCL )
{
  Trace trace( __PRETTY_FUNCTION__ );
  // Cursor Line Position is zero, so:
  // 1. Save previous line, end of line + 1 position
  CrsPos ncp = { OCL-1, pfb->LineLen( OCL-1 ) };

  // 2. Remove the line
  Line lp(__FILE__, __LINE__);
  pfb->RemoveLine( OCL, lp );

  // 3. Append rest of line to previous line
  pfb->AppendLineToLine( OCL-1, lp );

  // 4. Put cursor at the old previous line end of line + 1 position
  const bool MOVE_UP    = ncp.crsLine < topLine;
  const bool MOVE_RIGHT = RightChar() < ncp.crsChar;

  if( MOVE_UP ) topLine = ncp.crsLine;
                crsRow = ncp.crsLine - topLine;

  if( MOVE_RIGHT ) leftChar = ncp.crsChar - WorkingCols() + 1;
                   crsCol = ncp.crsChar - leftChar;

  // 5. Removed a line, so update to re-draw window view
  pfb->Update();
}

bool View::Do_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  inVisualBlock = false;

  return Do_visualMode();
}

bool View::Do_V()
{
  Trace trace( __PRETTY_FUNCTION__ );

  inVisualBlock = true;

  return Do_visualMode();
}

// Returns true if something was changed, else false
//
bool View::Do_visualMode()
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();
  inVisualMode = true;
  DisplayBanner();

  v_st_line = CrsLine();  v_fn_line = v_st_line;
  v_st_char = CrsChar();  v_fn_char = v_st_char;

  // Write current byte in visual:
  Replace_Crs_Char( S_VISUAL );

  while( inVisualMode )
  {
    const char c=gl_pKey->In();

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
    else if( c == 'z' ) gl_pVis->Handle_z();
    else if( c == 'f' ) gl_pVis->Handle_f();
    else if( c == ';' ) gl_pVis->Handle_SemiColon();
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
  inVisualMode = false;
  Undo_v();
  Remove_Banner();
  return false;
}

void View::Undo_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned st_line = Min( v_st_line, v_fn_line );
  const unsigned fn_line = Max( v_st_line, v_fn_line );

  UpdateLines( st_line, fn_line );

  sts_line_needs_update = true;
}

// Returns true if still in visual mode, else false
//
void View::Do_v_Handle_g()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char CC2 = gl_pKey->In();

  if     ( CC2 == 'g' ) GoToTopOfFile();
  else if( CC2 == '0' ) GoToStartOfRow();
  else if( CC2 == '$' ) GoToEndOfRow();
  else if( CC2 == 'f' ) Do_v_Handle_gf();
  else if( CC2 == 'p' ) Do_v_Handle_gp();
}

void View::Do_v_Handle_gf()
{
  if( v_st_line == v_fn_line )
  {
    const unsigned m_v_st_char = v_st_char < v_fn_char ? v_st_char : v_fn_char;
    const unsigned m_v_fn_char = v_st_char < v_fn_char ? v_fn_char : v_st_char;

    String fname;

    for( unsigned P = m_v_st_char; P<=m_v_fn_char; P++ )
    {
      fname.push( pfb->Get( v_st_line, P  ) );
    }
    bool went_to_file = gl_pVis->GoToFile_GoToBuffer( fname );

    if( went_to_file )
    {
      // If we made it to buffer indicated by fname, no need to Undo_v() or
      // Remove_Banner() because the whole view pane will be redrawn
      inVisualMode = false;
    }
  }
}

void View::Do_v_Handle_gp()
{
  if( v_st_line == v_fn_line )
  {
    const unsigned m_v_st_char = v_st_char < v_fn_char ? v_st_char : v_fn_char;
    const unsigned m_v_fn_char = v_st_char < v_fn_char ? v_fn_char : v_st_char;

    String pattern;

    for( unsigned P = m_v_st_char; P<=m_v_fn_char; P++ )
    {
      pattern.push( pfb->Get( v_st_line, P  ) );
    }
    gl_pVis->Handle_Slash_GotPattern( pattern, false );

    inVisualMode = false;
    Undo_v();
    Remove_Banner();
  }
}

void View::DisplayBanner()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Command line row in window:
  const unsigned WIN_ROW = WorkingRows() + 2;
  const unsigned WIN_COL = 0;

  const unsigned G_ROW = Row_Win_2_GL( WIN_ROW );
  const unsigned G_COL = Col_Win_2_GL( WIN_COL );

  if( inInsertMode )
  {
    Console::SetS( G_ROW, G_COL, "--INSERT --", S_BANNER );
  }
  else if( inReplaceMode )
  {
    Console::SetS( G_ROW, G_COL, "--REPLACE--", S_BANNER );
  }
  else if( inVisualMode )
  {
    Console::SetS( G_ROW, G_COL, "--VISUAL --", S_BANNER );
  }
  Console::Update();
  PrintCursor();          // Put cursor back in position.
}

void View::Remove_Banner()
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

void View::DisplayMapping()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const char* mapping = "--MAPPING--";
  const int   mapping_len = strlen( mapping );

  // Command line row in window:
  const unsigned WIN_ROW = WorkingRows() + 2;

  const unsigned G_ROW = Row_Win_2_GL( WIN_ROW );
  const unsigned G_COL = Col_Win_2_GL( nCols-1-right_border-mapping_len );

  Console::SetS( G_ROW, G_COL, mapping, S_BANNER );

  Console::Update();
  PrintCursor(); // Put cursor back in position.
}

void View::ReplaceAddReturn()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // The lines in fb do not end with '\n's.
  // When the file is written, '\n's are added to the ends of the lines.
  Line new_line(__FILE__, __LINE__);
  const unsigned OLL = pfb->LineLen( CrsLine() );
  const unsigned OCP = CrsChar();

  for( unsigned k=OCP; k<OLL; k++ )
  {
    const uint8_t C = pfb->RemoveChar( CrsLine(), OCP );
    bool ok = new_line.push(__FILE__,__LINE__, C );
    ASSERT( __LINE__, ok, "ok" );
  }
  // Truncate the rest of the old line:
  // Add the new line:
  const unsigned new_line_num = CrsLine()+1;
  pfb->InsertLine( new_line_num, new_line );
  crsCol = 0;
  leftChar = 0;
  if( CrsLine() < BotLine() ) crsRow++;
  else {
    // If we were on the bottom working line, scroll screen down
    // one line so that the cursor line is not below the screen.
    topLine++;
  }
  pfb->Update();
}

void View::ReplaceAddChars( const char C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( pfb->NumLines()==0 ) pfb->PushLine();

  const unsigned CL = CrsLine();
  const unsigned CP = CrsChar();
  const unsigned LL = pfb->LineLen( CL );
  const unsigned EOL = LL ? LL-1 : 0;

  if( EOL < CP )
  {
    // Extend line out to where cursor is:
    for( unsigned k=LL; k<CP; k++ ) pfb->PushChar( CL, ' ' );
  }
  // Put char back in file buffer
  const bool continue_last_update = false;
  if( CP < LL ) pfb->Set( CL, CP, C, continue_last_update );
  else {
    pfb->PushChar( CL, C );
  }
  if( crsCol < WorkingCols()-1 )
  {
    crsCol++;
  }
  else {
    leftChar++;
  }
  pfb->Update();
}

void View::RepositionView()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If a window re-size has taken place, and the window has gotten
  // smaller, change top line and left char if needed, so that the
  // cursor is in the buffer when it is re-drawn
  if( WorkingRows() <= crsRow )
  {
    topLine += ( crsRow - WorkingRows() + 1 );
    crsRow  -= ( crsRow - WorkingRows() + 1 );
  }
  if( WorkingCols() <= crsCol )
  {
    leftChar += ( crsCol - WorkingCols() + 1 );
    crsCol   -= ( crsCol - WorkingCols() + 1 );
  }
}

void View::Replace_Crs_Char( Style style )
{
  const unsigned LL = pfb->LineLen( CrsLine() ); // Line length

  if( LL )
  {
    int byte = pfb->Get( CrsLine(), CrsChar() );

    Console::Set( Row_Win_2_GL( crsRow )
                , Col_Win_2_GL( crsCol )
                , byte, style );
  }
}

void View::PrintWorkingView()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = pfb->NumLines();
  const unsigned WR        = WorkingRows();
  const unsigned WC        = WorkingCols();

  unsigned row = 0;
  for( unsigned k=topLine; k<NUM_LINES && row<WR; k++, row++ )
  {
    // Dont allow line wrap:
    const unsigned LL    = pfb->LineLen( k );
    const unsigned G_ROW = Row_Win_2_GL( row );
    unsigned col=0;
    for( unsigned i=leftChar; i<LL && col<WC; i++, col++ )
    {
      Style s    = Get_Style( k, i );
      int   byte = pfb->Get( k, i );

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

void View::PrintLines( const unsigned st_line, const unsigned fn_line )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned WC = WorkingCols();

  for( unsigned k=st_line; k<=fn_line; k++ )
  {
    // Dont allow line wrap:
    const unsigned LL    = pfb->LineLen( k );
    const unsigned G_ROW = Line_2_GL( k );
    unsigned col=0;
    for( unsigned i=leftChar; col<WC && i<LL; i++, col++ )
    {
      Style s = Get_Style( k, i );

      int byte = pfb->Get( k, i );

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

void View::Print_Borders()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const bool HIGHLIGHT = ( 1 < gl_pVis->num_wins ) && ( this == gl_pVis->CV() );

  const Style S = HIGHLIGHT ? S_BORDER_HI : S_BORDER;

  Print_Borders_Top   ( S );
  Print_Borders_Right ( S );
  Print_Borders_Left  ( S );
  Print_Borders_Bottom( S );
}

void View::Print_Borders_Top( const Style S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( top___border )
  {
    const uint8_t BORDER_CHAR = pfb->Changed() ? '+' : ' ';
    const unsigned ROW_G = y;

    for( unsigned k=0; k<WinCols(); k++ )
    {
      const unsigned COL_G = x + k;

      Console::Set( ROW_G, COL_G, BORDER_CHAR, S );
    }
  }
}

void View::Print_Borders_Bottom( const Style S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( bottomborder )
  {
    const uint8_t BORDER_CHAR = pfb->Changed() ? '+' : ' ';
    const unsigned ROW_G = y + WinRows() - 1;

    for( unsigned k=0; k<WinCols(); k++ )
    {
      const unsigned COL_G = x + k;

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

void View::Print_Borders_Right( const Style S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( right_border )
  {
    const uint8_t BORDER_CHAR = pfb->Changed() ? '+' : ' ';
    const unsigned COL_G = x + WinCols() - 1;

    for( unsigned k=0; k<WinRows()-1; k++ )
    {
      const unsigned ROW_G = y + k;

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

void View::Print_Borders_Left( const Style S )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( left__border )
  {
    const uint8_t BORDER_CHAR = pfb->Changed() ? '+' : ' ';
    const unsigned COL_G = x;

    for( unsigned k=0; k<WinRows(); k++ )
    {
      const unsigned ROW_G = y + k;

      Console::Set( ROW_G, COL_G, BORDER_CHAR, S );
    }
  }
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
  const unsigned LL = pfb->NumLines() ? pfb->LineLen( CL ) : 0;
  const unsigned WC = WorkingCols();

  // When inserting text at the end of a line, CrsChar() == LL
  if( LL && CC < LL ) // Print current char info:
  {
    const int c = pfb->Get( CL, CC );

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
  const unsigned  crsByte = pfb->GetCursorByte( CL, CC );
  char percent = SCast<char>(100*double(crsByte)/double(fileSize) + 0.5);
  // Screen width so far
  char* p = buf2;

  p += sprintf( buf2, "Pos=(%u,%u)  (%i%%, %u/%u)  Char=(%s)  "
                    , CL+1, CC+1, percent, crsByte, pfb->GetSize(), buf1 );
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
  const unsigned FILE_NAME_LEN = strlen( pfb->file_name.c_str() );

  char buf[1024]; buf[0] = 0;
  char* p = buf;

  if( WC < FILE_NAME_LEN )
  {
    // file_name does not fit, so truncate beginning
    p += sprintf( buf, "%s", pfb->file_name.c_str() + (FILE_NAME_LEN - WC) );
  }
  else {
    // file_name fits, add spaces at end
    p += sprintf( buf, "%s", pfb->file_name.c_str() );
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
  if( inInsertMode )
  {
    col=10; // Strlen of "--INSERT--"
    Console::SetS( Cmd__Line_Row(), Col_Win_2_GL( 0 ), "--INSERT--", S_BANNER );
  }
  else if( inReplaceMode )
  {
    col=11; // Strlen of "--REPLACE--"
    Console::SetS( Cmd__Line_Row(), Col_Win_2_GL( 0 ), "--REPLACE--", S_BANNER );
  }
  else if( gl_pVis->m_run_mode && gl_pVis->CV() == this )
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
void View::PrintCursor()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Either one of these should work:
  Console::Move_2_Row_Col( Row_Win_2_GL( crsRow ), Col_Win_2_GL( crsCol ) );
  Console::Flush();
}

void View::GoUp()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = pfb->NumLines();
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
  const unsigned NUM_LINES = pfb->NumLines();
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
  if( 0==pfb->NumLines() ) return;

  const unsigned CL = CrsLine(); // Cursor line
  const unsigned CP = CrsChar(); // Cursor position

  if( CP == 0 ) return;

  GoToCrsPos_Write( CL, CP-1 );
}

void View::GoRight()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==pfb->NumLines() ) return;

  const unsigned CL = CrsLine(); // Cursor line
  const unsigned LL = pfb->LineLen( CL );
  if( 0==LL ) return;

  const unsigned CP = CrsChar(); // Cursor position
  if( LL-1 <= CP ) return;

  GoToCrsPos_Write( CL, CP+1 );
}

bool View::GoToFile_GetFileName( String& fname )
{
  Trace trace( __PRETTY_FUNCTION__ );

  bool got_filename = false;

  const unsigned CL = CrsLine();
  const unsigned LL = pfb->LineLen( CL );

  if( LL )
  {
    MoveInBounds();
    const int CC = CrsChar();
    char c = pfb->Get( CL, CC );

    if( IsFileNameChar( c ) )
    {
      // Get the file name:
      got_filename = true;

      fname.push( c );

      // Search backwards, until white space is found:
      for( int k=CC-1; -1<k; k-- )
      {
        c = pfb->Get( CL, k );

        if( !IsFileNameChar( c ) ) break;
        else fname.insert( 0, c );
      }
      // Search forwards, until white space is found:
      for( unsigned k=CC+1; k<LL; k++ )
      {
        c = pfb->Get( CL, k );

        if( !IsFileNameChar( c ) ) break;
        else fname.push( c );
      }
      EnvKeys2Vals( fname );
    }
  }
  return got_filename;
}

void View::GoToLine( const unsigned user_line_num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Internal line number is 1 less than user line number:
  const unsigned NCL = user_line_num - 1; // New cursor line number

  if( pfb->NumLines() <= NCL )
  {
    PrintCursor();
  }
  else {
    GoToCrsPos_Write( NCL, 0 );
  }
}

void View::GoToTopLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToCrsPos_Write( topLine, 0 );
}

void View::GoToBotLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = pfb->NumLines();

  unsigned bottom_line_in_view = topLine + WorkingRows()-1;

  bottom_line_in_view = Min( NUM_LINES-1, bottom_line_in_view );

  GoToCrsPos_Write( bottom_line_in_view, 0 );
}

void View::GoToMidLineInView()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = pfb->NumLines();

  // Default: Last line in file is not in view
  unsigned NCL = topLine + WorkingRows()/2; // New cursor line

  if( NUM_LINES-1 < BotLine() )
  {
    // Last line in file above bottom of view
    NCL = topLine + (NUM_LINES-1 - topLine)/2;
  }
  GoToCrsPos_Write( NCL, 0 );
}

void View::GoToBegOfLine()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==pfb->NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line

  GoToCrsPos_Write( OCL, 0 );
}

void View::GoToEndOfLine()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0==pfb->NumLines() ) return;

  const unsigned LL = pfb->LineLen( CrsLine() );

  const unsigned OCL = CrsLine(); // Old cursor line

  if( inVisualMode && inVisualBlock )
  {
    // In Visual Block, $ puts cursor at the position
    // of the end of the longest line in the block
    unsigned max_LL = LL;

    for( unsigned L=v_st_line; L<=v_fn_line; L++ )
    {
      max_LL = Max( max_LL, pfb->LineLen( L ) );
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
  const unsigned NUM_LINES = pfb->NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  if( (NUM_LINES-1) <= OCL ) return; // On last line, so cant go down

  GoToCrsPos_Write( OCL+1, 0 );
}

void View::GoToStartOfRow()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==pfb->NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line

  GoToCrsPos_Write( OCL, leftChar );
}

void View::GoToEndOfRow()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==pfb->NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line

  const unsigned LL = pfb->LineLen( OCL );
  if( 0==LL ) return;

  const unsigned NCP = Min( LL-1, leftChar + WorkingCols() - 1 );

  GoToCrsPos_Write( OCL, NCP );
}

void View::GoToTopOfFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  GoToCrsPos_Write( 0, 0 );
}

void View::GoToEndOfFile()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = pfb->NumLines();

  GoToCrsPos_Write( NUM_LINES-1, 0 );
}

void View::GoToNextWord()
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
bool View::GoToNextWord_GetPosition( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = pfb->NumLines();
  if( 0==NUM_LINES ) return false;

  bool found_space = false;
  bool found_word  = false;
  const unsigned OCL = CrsLine(); // Old cursor line
  const unsigned OCP = CrsChar(); // Old cursor position

  IsWord_Func isWord = IsWord_Ident;

  // Find white space, and then find non-white space
  for( unsigned l=OCL; (!found_space || !found_word) && l<NUM_LINES; l++ )
  {
    const unsigned LL = pfb->LineLen( l );
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

      const int C = pfb->Get( l, p );

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

void View::GoToPrevWord()
{
  Trace trace( __PRETTY_FUNCTION__ );

  CrsPos ncp = { 0, 0 };

  if( GoToPrevWord_GetPosition( ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

// Return true if new cursor position found, else false
bool View::GoToPrevWord_GetPosition( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = pfb->NumLines();
  if( 0==NUM_LINES ) return false;

  const int      OCL = CrsLine(); // Old cursor line
  const unsigned LL  = pfb->LineLen( OCL );

  if( LL < CrsChar() ) // Since cursor is now allowed past EOL,
  {                    // it may need to be moved back:
    if( LL && !IsSpace( pfb->Get( OCL, LL-1 ) ) )
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
    const int LL = pfb->LineLen( l );
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

      const int C = pfb->Get( l, p );

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

void View::GoToEndOfWord()
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
bool View::GoToEndOfWord_GetPosition( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = pfb->NumLines();
  if( 0==NUM_LINES ) return false;

  const int      CL = CrsLine(); // Cursor line
  const unsigned LL = pfb->LineLen( CL );
        unsigned CP = CrsChar(); // Cursor position

  // At end of line, or line too short:
  if( (LL-1) <= CP || LL < 2 ) return false;

  int CC = pfb->Get( CL, CP );   // Current char
  int NC = pfb->Get( CL, CP+1 ); // Next char

  // 1. If at end of word, or end of non-word, move to next char
  if( (IsWord_Ident   ( CC ) && !IsWord_Ident   ( NC ))
   || (IsWord_NonIdent( CC ) && !IsWord_NonIdent( NC )) ) CP++;

  // 2. If on white space, skip past white space
  if( IsSpace( pfb->Get(CL, CP) ) )
  {
    for( ; CP<LL && IsSpace( pfb->Get(CL, CP) ); CP++ ) ;
    if( LL <= CP ) return false; // Did not find non-white space
  }
  // At this point (CL,CP) should be non-white space
  CC = pfb->Get( CL, CP );  // Current char

  ncp.crsLine = CL;

  if( IsWord_Ident( CC ) ) // On identity
  {
    // 3. If on word space, go to end of word space
    for( ; CP<LL && IsWord_Ident( pfb->Get(CL, CP) ); CP++ )
    {
      ncp.crsChar = CP;
    }
  }
  else if( IsWord_NonIdent( CC ) )// On Non-identity, non-white space
  {
    // 4. If on non-white-non-word, go to end of non-white-non-word
    for( ; CP<LL && IsWord_NonIdent( pfb->Get(CL, CP) ); CP++ )
    {
      ncp.crsChar = CP;
    }
  }
  else { // Should never get here:
    return false;
  }
  return true;
}

void View::GoToOppositeBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();
  const unsigned NUM_LINES = pfb->NumLines();
  const unsigned CL = CrsLine();
  const unsigned CC = CrsChar();
  const unsigned LL = pfb->LineLen( CL );

  if( 0==NUM_LINES || 0==LL ) return;

  const char C = pfb->Get( CL, CC );

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

void View::GoToLeftSquigglyBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();

  const char  start_char = '}';
  const char finish_char = '{';
  GoToOppositeBracket_Backward( start_char, finish_char );
}

void View::GoToRightSquigglyBracket()
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();

  const char  start_char = '{';
  const char finish_char = '}';
  GoToOppositeBracket_Forward( start_char, finish_char );
}

void View::GoToOppositeBracket_Forward( const char ST_C, const char FN_C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = pfb->NumLines();
  const unsigned CL = CrsLine();
  const unsigned CC = CrsChar();

  // Search forward
  unsigned level = 0;
  bool     found = false;

  for( unsigned l=CL; !found && l<NUM_LINES; l++ )
  {
    const unsigned LL = pfb->LineLen( l );

    for( unsigned p=(CL==l)?(CC+1):0; !found && p<LL; p++ )
    {
      const char C = pfb->Get( l, p );

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

void View::GoToOppositeBracket_Backward( const char ST_C, const char FN_C )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const int CL = CrsLine();
  const int CC = CrsChar();

  // Search forward
  unsigned level = 0;
  bool     found = false;

  for( int l=CL; !found && 0<=l; l-- )
  {
    const unsigned LL = pfb->LineLen( l );

    for( int p=(CL==l)?(CC-1):(LL-1); !found && 0<=p; p-- )
    {
      const char C = pfb->Get( l, p );

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
  if( inVisualMode )
  {
    v_fn_line = NCL;
    v_fn_char = NCP;
  }
  // These moves refer to View of buffer:
  const bool MOVE_DOWN  = BotLine()   < NCL;
  const bool MOVE_RIGHT = RightChar() < NCP;
  const bool MOVE_UP    = NCL < topLine;
  const bool MOVE_LEFT  = NCP < leftChar;

  const bool redraw = MOVE_DOWN || MOVE_RIGHT || MOVE_UP || MOVE_LEFT;

  if( redraw )
  {
    if     ( MOVE_DOWN ) topLine = NCL - WorkingRows() + 1;
    else if( MOVE_UP   ) topLine = NCL;

    if     ( MOVE_RIGHT ) leftChar = NCP - WorkingCols() + 1;
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

    PrintCursor();  // Put cursor into position.

    sts_line_needs_update = true;
  }
}

void View::GoToCrsPos_Write_Visual( const unsigned OCL, const unsigned OCP
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
  PrintCursor();
  sts_line_needs_update = true;
}

// Cursor is moving forward
// Write out from (OCL,OCP) up to but not including (NCL,NCP)
void View::GoToCrsPos_WV_Forward( const unsigned OCL, const unsigned OCP
                                , const unsigned NCL, const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( OCL == NCL ) // Only one line:
  {
    for( unsigned k=OCP; k<NCP; k++ )
    {
      int byte = pfb->Get( OCL, k );
      Console::Set( Line_2_GL( OCL ), Char_2_GL( k ), byte, Get_Style(OCL,k) );
    }
  }
  else { // Multiple lines
    // Write out first line:
    const unsigned OCLL = pfb->LineLen( OCL ); // Old cursor line length
    const unsigned END_FIRST_LINE = Min( RightChar()+1, OCLL );
    for( unsigned k=OCP; k<END_FIRST_LINE; k++ )
    {
      int byte = pfb->Get( OCL, k );
      Console::Set( Line_2_GL( OCL ), Char_2_GL( k ), byte, Get_Style(OCL,k) );
    }
    // Write out intermediate lines:
    for( unsigned l=OCL+1; l<NCL; l++ )
    {
      const unsigned LL = pfb->LineLen( l ); // Line length
      const unsigned END_OF_LINE = Min( RightChar()+1, LL );
      for( unsigned k=leftChar; k<END_OF_LINE; k++ )
      {
        int byte = pfb->Get( l, k );
        Console::Set( Line_2_GL( l ), Char_2_GL( k ), byte, Get_Style(l,k) );
      }
    }
    // Write out last line:
    // Print from beginning of next line to new cursor position:
    const unsigned NCLL = pfb->LineLen( NCL ); // Line length
    const unsigned END = Min( NCLL, NCP );
    for( unsigned k=leftChar; k<END; k++ )
    {
      int byte = pfb->Get( NCL, k );
      Console::Set( Line_2_GL( NCL ), Char_2_GL( k ), byte, Get_Style(NCL,k)  );
    }
  }
}

// Cursor is moving backwards
// Write out from (OCL,OCP) back to but not including (NCL,NCP)
void View::GoToCrsPos_WV_Backward( const unsigned OCL, const unsigned OCP
                                 , const unsigned NCL, const unsigned NCP )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( OCL == NCL ) // Only one line:
  {
    const unsigned LL = pfb->LineLen( OCL ); // Line length
    if( LL ) {
      const unsigned START = Min( OCP, LL-1 );
      for( unsigned k=START; NCP<k; k-- )
      {
        int byte = pfb->Get( OCL, k );
        Console::Set( Line_2_GL( OCL ), Char_2_GL( k ), byte, Get_Style(OCL,k) );
      }
    }
  }
  else { // Multiple lines
    // Write out first line:
    const unsigned OCLL = pfb->LineLen( OCL ); // Old cursor line length
    if( OCLL ) {
      for( int k=Min(OCP,OCLL-1); static_cast<int>(leftChar)<=k; k-- )
      {
        int byte = pfb->Get( OCL, k );
        Console::Set( Line_2_GL( OCL ), Char_2_GL( k ), byte, Get_Style(OCL,k) );
      }
    }
    // Write out intermediate lines:
    for( unsigned l=OCL-1; NCL<l; l-- )
    {
      const unsigned LL = pfb->LineLen( l ); // Line length
      if( LL ) {
        const unsigned END_OF_LINE = Min( RightChar(), LL-1 );
        for( int k=END_OF_LINE; static_cast<int>(leftChar)<=k; k-- )
        {
          int byte = pfb->Get( l, k );
          Console::Set( Line_2_GL( l ), Char_2_GL( k ), byte, Get_Style(l,k) );
        }
      }
    }
    // Write out last line:
    // Go down to beginning of last line:
    const unsigned NCLL = pfb->LineLen( NCL ); // New cursor line length
    if( NCLL ) {
      const unsigned END_LAST_LINE = Min( RightChar(), NCLL-1 );

      // Print from beginning of next line to new cursor position:
      for( int k=END_LAST_LINE; static_cast<int>(NCP)<=k; k-- )
      {
        int byte = pfb->Get( NCL, k );
        Console::Set( Line_2_GL( NCL ), Char_2_GL( k ), byte, Get_Style(NCL,k) );
      }
    }
  }
}

void View::GoToCrsPos_Write_VisualBlock( const unsigned OCL
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

  const unsigned draw_box_left = Max( leftChar   , vis_box_left );
  const unsigned draw_box_rite = Min( RightChar(), vis_box_rite );
  const unsigned draw_box_top  = Max( topLine    , vis_box_top  );
  const unsigned draw_box_bot  = Min( BotLine()  , vis_box_bot  );

  for( unsigned l=draw_box_top; l<=draw_box_bot; l++ )
  {
    const unsigned LL = pfb->LineLen( l );

    for( unsigned k=draw_box_left; k<LL && k<=draw_box_rite; k++ )
    {
      // On some terminals, the cursor on reverse video on white space does not
      // show up, so to prevent that, do not reverse video the cursor position:
      const int   byte  = pfb->Get ( l, k );
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
  crsRow = NCL - topLine;
  crsCol = NCP - leftChar;
  Console::Update();
  PrintCursor();
  sts_line_needs_update = true;
}

void View::GoToCrsPos_NoWrite( const unsigned ncp_crsLine
                             , const unsigned ncp_crsChar )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // These moves refer to View of buffer:
  const bool MOVE_DOWN  = BotLine()   < ncp_crsLine;
  const bool MOVE_RIGHT = RightChar() < ncp_crsChar;
  const bool MOVE_UP    = ncp_crsLine < topLine;
  const bool MOVE_LEFT  = ncp_crsChar < leftChar;

  if     ( MOVE_DOWN ) topLine = ncp_crsLine - WorkingRows() + 1;
  else if( MOVE_UP   ) topLine = ncp_crsLine;
  crsRow  = ncp_crsLine - topLine;

  if     ( MOVE_RIGHT ) leftChar = ncp_crsChar - WorkingCols() + 1;
  else if( MOVE_LEFT  ) leftChar = ncp_crsChar;
  crsCol   = ncp_crsChar - leftChar;
}

// Change directory to that of file of view:
//
bool View::GoToDir()
{
  char* fname_str = const_cast<char*>( pfb->file_name.c_str() );

  if( pfb->is_dir )
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

// If past end of line, move back to end of line.
// Returns true if moved, false otherwise.
//
bool View::MoveInBounds()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CL  = CrsLine();
  const unsigned LL  = pfb->LineLen( CL );
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

  if( crsRow )
  {
    topLine += crsRow;
    crsRow = 0;
    Update();
  }
}

void View::MoveCurrLineCenter()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned center = SCast<unsigned>( 0.5*WorkingRows() + 0.5 );

  const unsigned OCL = CrsLine(); // Old cursor line

  if( 0 < OCL && OCL < center && 0 < topLine )
  {
    // Cursor line cannot be moved to center, but can be moved closer to center
    // CrsLine() does not change:
    crsRow += topLine;
    topLine = 0;
    Update();
  }
  else if( center <= OCL
        && center != crsRow )
  {
    topLine += crsRow - center;
    crsRow = center;
    Update();
  }
}

void View::MoveCurrLineToBottom()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( 0 < topLine )
  {
    const unsigned WR  = WorkingRows();
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

// This one works better when NOT in visual mode:
void View::PageDown()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = pfb->NumLines();
  if( !NUM_LINES ) return;

  const unsigned newTopLine = topLine + WorkingRows() - 1;
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

// This one works better when IN visual mode:
void View::PageDown_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = pfb->NumLines();

  if( 0<NUM_LINES )
  {
    const unsigned OCLd = CrsLine(); // Old cursor line

    unsigned NCLd = OCLd + WorkingRows() - 1; // New cursor line

    // Dont let cursor go past the end of the file:
    if( NUM_LINES-1 < NCLd ) NCLd = NUM_LINES-1;

    GoToCrsPos_Write( NCLd, 0 );
  }
}

void View::PageUp()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned OCL = CrsLine(); // Old cursor line
  const unsigned OCP = CrsChar(); // Old cursor position

  // Dont scroll if we are at the top of the file:
  if( topLine )
  {
    //Leave crsRow unchanged.
    crsCol = 0;

    // Dont scroll past the top of the file:
    if( topLine < WorkingRows() - 1 )
    {
      topLine = 0;
    }
    else {
      topLine -= WorkingRows() - 1;
    }
    Update();
  }
}

void View::PageUp_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = pfb->NumLines();

  if( 0<NUM_LINES )
  {
    const unsigned OCL = CrsLine(); // Old cursor line

    int NCL = OCL - WorkingRows() + 1; // New cursor line

    // Check for underflow:
    if( NCL < 0 ) NCL = 0;

    GoToCrsPos_Write( NCL, 0 );
  }
}

String View::Do_Star_GetNewPattern()
{
  Trace trace( __PRETTY_FUNCTION__ );
  String new_star;

  if( pfb->NumLines() == 0 ) return new_star;

  const unsigned CL = CrsLine();
  const unsigned LL = pfb->LineLen( CL );

  if( LL )
  {
    MoveInBounds();
    const unsigned CC = CrsChar();

    const int c = pfb->Get( CL,  CC );

    if( isalnum( c ) || c=='_' )
    {
      new_star.push( c );

      // Search forward:
      for( unsigned k=CC+1; k<LL; k++ )
      {
        const int c = pfb->Get( CL, k );
        if( isalnum( c ) || c=='_' ) new_star.push( c );
        else                         break;
      }
      // Search backward:
      for( int k=CC-1; 0<=k; k-- )
      {
        const int c = pfb->Get( CL, k );
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

  const unsigned NUM_LINES = pfb->NumLines();
  const unsigned END_LINE  = Min( topLine+WorkingRows(), NUM_LINES );

  for( unsigned l=topLine; l<END_LINE; l++ )
  {
    const unsigned LL      = pfb->LineLen( l );
    const unsigned END_POS = Min( leftChar+WorkingCols(), LL );

    for( unsigned p=leftChar; p<END_POS; p++ )
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
        int byte = pfb->Get( l, p );
        Console::Set( Line_2_GL( l ), Char_2_GL( p ), byte, s );
      }
    }
  }
}

// Go to next pattern
void View::Do_n()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = pfb->NumLines();

  if( NUM_LINES == 0 ) return;

  CrsPos ncp = { 0, 0 }; // Next cursor position

  if( Do_n_FindNextPattern( ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
  else PrintCursor();
}

bool View::RV_Style( const Style s ) const
{
  return s == S_RV_NORMAL
      || s == S_RV_STAR
      || s == S_RV_DEFINE
      || s == S_RV_COMMENT
      || s == S_RV_CONST
      || s == S_RV_CONTROL
      || s == S_RV_VARTYPE;
}

Style View::RV_Style_2_NonRV( const Style RVS ) const
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

Style View::Get_Style( const unsigned line, const unsigned pos )
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

bool View::InVisualArea( const unsigned line, const unsigned pos )
{
  if( inVisualMode )
  {
    if( inVisualBlock ) return InVisualBlock( line, pos );
    else                return InVisualStFn ( line, pos );
  }
  return false;
}

bool View::InVisualBlock( const unsigned line, const unsigned pos )
{
  return ( v_st_line <= line && line <= v_fn_line && v_st_char <= pos  && pos  <= v_fn_char ) // bot rite
      || ( v_st_line <= line && line <= v_fn_line && v_fn_char <= pos  && pos  <= v_st_char ) // bot left
      || ( v_fn_line <= line && line <= v_st_line && v_st_char <= pos  && pos  <= v_fn_char ) // top rite
      || ( v_fn_line <= line && line <= v_st_line && v_fn_char <= pos  && pos  <= v_st_char );// top left
}

bool View::InVisualStFn( const unsigned line, const unsigned pos )
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

bool View::InComment( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return pfb->HasStyle( line, pos, HI_COMMENT );
}

bool View::InDefine( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return pfb->HasStyle( line, pos, HI_DEFINE );
}

bool View::InConst( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return pfb->HasStyle( line, pos, HI_CONST );
}

bool View::InControl( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return pfb->HasStyle( line, pos, HI_CONTROL );
}

bool View::InVarType( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return pfb->HasStyle( line, pos, HI_VARTYPE );
}

bool View::InStar( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return pfb->HasStyle( line, pos, HI_STAR );
}

bool  View::InNonAscii( const unsigned line, const unsigned pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  return pfb->HasStyle( line, pos, HI_NONASCII );
}

// Go to previous pattern
void View::Do_N()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = pfb->NumLines();

  if( NUM_LINES == 0 ) return;

  CrsPos ncp = { 0, 0 }; // Next cursor position

  if( Do_N_FindPrevPattern( ncp ) )
  {
    GoToCrsPos_Write( ncp.crsLine, ncp.crsChar );
  }
}

bool View::Do_n_FindNextPattern( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_LINES = pfb->NumLines();
  const unsigned STAR_LEN  = gl_pVis->star.len();

  const unsigned OCL = CrsLine();
  const unsigned OCC = CrsChar();

  unsigned st_l = OCL;
  unsigned st_c = OCC;

  bool found_next_star = false;

  // Move past current star:
  const unsigned LL = pfb->LineLen( OCL );

  for( ; st_c<LL && InStar(OCL,st_c); st_c++ ) ;

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
      const unsigned LL = pfb->LineLen( l );
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

bool View::Do_N_FindPrevPattern( CrsPos& ncp )
{
  Trace trace( __PRETTY_FUNCTION__ );
  MoveInBounds();

  const unsigned NUM_LINES = pfb->NumLines();
  const unsigned STAR_LEN  = gl_pVis->star.len();

  const unsigned OCL = CrsLine();
  const unsigned OCC = CrsChar();

  bool found_prev_star = false;

  // Search for first star position before current position
  for( int l=OCL; !found_prev_star && 0<=l; l-- )
  {
    const int LL = pfb->LineLen( l );

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
      const unsigned LL = pfb->LineLen( l );

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

void View::Do_f( const char FAST_CHAR )
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = pfb->NumLines();
  if( 0==NUM_LINES ) return;

  const unsigned OCL = CrsLine();           // Old cursor line
  const unsigned LL  = pfb->LineLen( OCL ); // Line length
  const unsigned OCP = CrsChar();           // Old cursor position

  if( LL-1 <= OCP ) return;

  unsigned NCP = 0;
  bool found_char = false;
  for( unsigned p=OCP+1; !found_char && p<LL; p++ )
  {
    const char C = pfb->Get( OCL, p );

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

void View::Do_u()
{
  Trace trace( __PRETTY_FUNCTION__ );

  pfb->Undo( this );
}

void View::Do_U()
{
  Trace trace( __PRETTY_FUNCTION__ );

  pfb->UndoAll( this );
}

void View::Do_Tilda()
{
  Trace trace( __PRETTY_FUNCTION__ );
  if( 0==pfb->NumLines() ) return;

  const unsigned OCL = CrsLine(); // Old cursor line
  const unsigned OCP = CrsChar(); // Old cursor position
  const unsigned LL  = pfb->LineLen( OCL );

  if( !LL || LL-1 < OCP ) return;

  char c = pfb->Get( CrsLine(), CrsChar() );
  bool changed = false;
  if     ( isupper( c ) ) { c = tolower( c ); changed = true; }
  else if( islower( c ) ) { c = toupper( c ); changed = true; }

  if( crsCol < Min( LL-1, WorkingCols()-1 ) )
  {
    if( changed ) pfb->Set( CrsLine(), CrsChar(), c );
    // Need to move cursor right:
    crsCol++;
  }
  else if( RightChar() < LL-1 )
  {
    // Need to scroll window right:
    if( changed ) pfb->Set( CrsLine(), CrsChar(), c );
    leftChar++;
  }
  else // RightChar() == LL-1
  {
    // At end of line so cant move or scroll right:
    if( changed ) pfb->Set( CrsLine(), CrsChar(), c );
  }
  pfb->Update();
}

void View::Do_x()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If there is nothing to 'x', just return:
  if( !pfb->NumLines() ) return;

  const unsigned CL = CrsLine();
  const unsigned LL = pfb->LineLen( CL );

  // If nothing on line, just return:
  if( !LL ) return;

  // If past end of line, move to end of line:
  if( LL-1 < CrsChar() )
  {
    GoToCrsPos_Write( CL, LL-1 );
  }
  const uint8_t C = pfb->RemoveChar( CL, CrsChar() );

  // Put char x'ed into register:
  Line* nlp = gl_pVis->BorrowLine( __FILE__,__LINE__ );
  nlp->push(__FILE__,__LINE__, C );
  gl_pVis->reg.clear();
  gl_pVis->reg.push( nlp );
  gl_pVis->paste_mode = PM_ST_FN;

  const unsigned NLL = pfb->LineLen( CL ); // New line length

  // Reposition the cursor:
  if( NLL <= leftChar+crsCol )
  {
    // The char x'ed is the last char on the line, so move the cursor
    //   back one space.  Above, a char was removed from the line,
    //   but crsCol has not changed, so the last char is now NLL.
    // If cursor is not at beginning of line, move it back one more space.
    if( crsCol ) crsCol--;
  }
  pfb->Update();
}

void View::Do_x_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( inVisualBlock )
  {
    Do_x_range_block( v_st_line, v_st_char, v_fn_line, v_fn_char );
  }
  else {
    Do_x_range( v_st_line, v_st_char, v_fn_line, v_fn_char );
  }
  Remove_Banner();
}

void View::Do_s()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CL  = CrsLine();
  const unsigned LL  = pfb->LineLen( CL );
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
      pfb->PushChar( CL, ' ' );
    }
    Do_a();
  }
  else // CP == EOL
  {
    Do_x();
    Do_a();
  }
}

void View::Do_s_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Need to know if cursor is at end of line before Do_x_v() is called:
  const unsigned LL = pfb->LineLen( CrsLine() );
  const bool CURSOR_AT_END_OF_LINE = LL ? CrsChar() == LL-1 : false;

  Do_x_v();

  if( inVisualBlock )
  {
    if( CURSOR_AT_END_OF_LINE ) Do_a_vb();
    else                        Do_i_vb(); 
  }
  else {
    if( CURSOR_AT_END_OF_LINE ) Do_a();
    else                        Do_i();
  }
}

void View::Do_i_vb()
{
  Trace trace( __PRETTY_FUNCTION__ );
  inInsertMode = true;
  DisplayBanner();

  unsigned count = 0;
  for( char c=gl_pKey->In(); c != ESC; c=gl_pKey->In() )
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
        pfb->Update();
      }
    }
    else {
      InsertAddChar_vb( c );
      count++;
      pfb->Update();
    }
  }
  Remove_Banner();
  inInsertMode = false;
}

void View::Do_a_vb()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned CL = CrsLine();
  const unsigned LL = pfb->LineLen( CL );
  if( 0==LL ) { Do_i_vb(); return; }

  const bool CURSOR_AT_EOL = ( CrsChar() == LL-1 );
  if( CURSOR_AT_EOL )
  {
    GoToCrsPos_NoWrite( CL, LL );
  }
  const bool CURSOR_AT_RIGHT_COL = ( crsCol == WorkingCols()-1 );

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
  pfb->Update();

  Do_i_vb();
}

void View::InsertBackspace_vb()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = CrsLine();  // Old cursor line
  const unsigned OCP = CrsChar();  // Old cursor position

  if( OCP )
  {
    const unsigned N_REG_LINES = gl_pVis->reg.len();

    for( unsigned k=0; k<N_REG_LINES; k++ )
    {
      pfb->RemoveChar( OCL+k, OCP-1 );
    }
    GoToCrsPos_NoWrite( OCL, OCP-1 );
  }
}

void View::InsertAddChar_vb( const char c )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = CrsLine();  // Old cursor line
  const unsigned OCP = CrsChar();  // Old cursor position

  const unsigned N_REG_LINES = gl_pVis->reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    const unsigned LL = pfb->LineLen( OCL+k );

    if( LL < OCP )
    {
      // Fill in line with white space up to OCP:
      for( unsigned i=0; i<(OCP-LL); i++ )
      {
        // Insert at end of line so undo will be atomic:
        const unsigned NLL = pfb->LineLen( OCL+k ); // New line length
        pfb->InsertChar( OCL+k, NLL, ' ' );
      }
    }
    pfb->InsertChar( OCL+k, OCP, c );
  }
  GoToCrsPos_NoWrite( OCL, OCP+1 );
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

  unsigned st_line = CrsLine();
  unsigned st_char = CrsChar();

  const unsigned LL = pfb->LineLen( st_line );

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

bool View::Do_dw_get_fn( const int st_line, const int st_char
                       , unsigned& fn_line, unsigned& fn_char  )
{
  const unsigned LL = pfb->LineLen( st_line );
  const uint8_t  C  = pfb->Get( st_line, st_char );

  if( IsSpace( C )      // On white space
    || ( st_char < LL-1 // On non-white space before white space
     //&& NotSpace( C )
       && IsSpace( pfb->Get( st_line, st_char+1 ) ) ) )
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

void View::Do_Tilda_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( v_fn_line < v_st_line ) Swap( v_st_line, v_fn_line );
  if( v_fn_char < v_st_char ) Swap( v_st_char, v_fn_char );

  if( inVisualBlock ) Do_Tilda_v_block();
  else                Do_Tilda_v_st_fn();

  inVisualMode = false;
  Remove_Banner();
  Undo_v(); //<- This will cause the tilda'ed characters to be redrawn
}

void View::Do_Tilda_v_st_fn()
{
  for( unsigned L = v_st_line; L<=v_fn_line; L++ )
  {
    const unsigned LL = pfb->LineLen( L );
    const unsigned P_st = (L==v_st_line) ? v_st_char : 0;
    const unsigned P_fn = (L==v_fn_line) ? v_fn_char : LL-1;

    for( unsigned P = P_st; P <= P_fn; P++ )
    {
      char c = pfb->Get( L, P );
      bool changed = false;
      if     ( isupper( c ) ) { c = tolower( c ); changed = true; }
      else if( islower( c ) ) { c = toupper( c ); changed = true; }
      if( changed ) pfb->Set( L, P, c );
    }
  }
}

void View::Do_Tilda_v_block()
{
  for( unsigned L = v_st_line; L<=v_fn_line; L++ )
  {
    const unsigned LL = pfb->LineLen( L );

    for( unsigned P = v_st_char; P<LL && P <= v_fn_char; P++ )
    {
      char c = pfb->Get( L, P );
      bool changed = false;
      if     ( isupper( c ) ) { c = tolower( c ); changed = true; }
      else if( islower( c ) ) { c = toupper( c ); changed = true; }
      if( changed ) pfb->Set( L, P, c );
    }
  }
}

void View::Do_D_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( inVisualBlock )
  {
    Do_x_range_block( v_st_line, v_st_char, v_fn_line, v_fn_char );
    Remove_Banner();
  }
  else {
    Do_D_v_line();
  }
}

void View::Do_D_v_line()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( v_fn_line < v_st_line ) Swap( v_st_line, v_fn_line );
  if( v_fn_char < v_st_char ) Swap( v_st_char, v_fn_char );

  gl_pVis->reg.clear();

  bool removed_line = false;
  // 1. If v_st_line==0, fn_line will go negative in the loop below,
  //    so use int's instead of unsigned's
  // 2. Dont remove all lines in file to avoid crashing
  int fn_line = v_fn_line;
  for( int L = v_st_line; 1 < pfb->NumLines() && L<=fn_line; fn_line-- )
  {
    Line* lp = pfb->RemoveLineP( L );
    gl_pVis->reg.push( lp );

    // gl_pVis->reg will delete nlp
    removed_line = true;
  }
  gl_pVis->paste_mode = PM_LINE;

  inVisualMode = false;
  Remove_Banner();
  // D'ed lines will be removed, so no need to Undo_v()

  if( removed_line )
  {
    // Figure out and move to new cursor position:
    const unsigned NUM_LINES = pfb->NumLines();
    const unsigned OCL       = CrsLine(); // Old cursor line

    unsigned ncl = v_st_line;
    if( NUM_LINES-1 < ncl ) ncl = v_st_line ? v_st_line-1 : 0;

    const unsigned NCLL = pfb->LineLen( ncl );
    unsigned ncc = 0;
    if( NCLL ) ncc = v_st_char < NCLL ? v_st_char : NCLL-1;

    GoToCrsPos_NoWrite( ncl, ncc );

    pfb->Update();
  }
}

//void View::Do_dd()
//{
//  Trace trace( __PRETTY_FUNCTION__ );
//  // If there is nothing to 'dd', just return:
//  if( pfb->NumLines() < 2 ) return;
//
//  const unsigned OCL = CrsLine();           // Old cursor line
//  const unsigned OCP = CrsChar();           // Old cursor position
//  const unsigned ONL = pfb->NumLines();     // Old number of lines
//  const unsigned OLL = pfb->LineLen( OCL ); // Old line length
//
//  const bool DELETING_LAST_LINE = OCL == ONL-1;
//
//  const unsigned NCL = DELETING_LAST_LINE ? OCL-1 : OCL; // New cursor line
//  const unsigned NLL = DELETING_LAST_LINE ? pfb->LineLen( NCL )
//                                          : pfb->LineLen( NCL + 1 );
//  // Deleting last line of file, so move to line above:
//  if( DELETING_LAST_LINE )
//  {
//    crsRow--;
//  }
//  // Move cursor to new location if needed:
//  if( 0 == NLL )
//  {
//    leftChar = 0;
//    crsCol = 0;
//  }
//  else if( NLL <= OCP )
//  {
//    // Shift left char if needed, and move cursor to end of its new line:
//    if( NLL <= leftChar ) leftChar = NLL-1;
//    crsCol = NLL-1 - leftChar;
//  }
//  // Remove line from FileBuf and save in paste register:
//  Line* lp = pfb->RemoveLineP( OCL );
//  if( lp ) {
//    // gl_pVis->reg will own nlp
//    gl_pVis->reg.clear();
//    gl_pVis->reg.push( lp );
//
//    gl_pVis->paste_mode = PM_LINE;
//  }
//  pfb->Update();
//}
void View::Do_dd()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned ONL = pfb->NumLines(); // Old number of lines

  // If there is nothing to 'dd', just return:
  if( 1 < ONL )
  {
    if( pfb == gl_pVis->views[0][ BE_FILE ]->pfb )
    {
      Do_dd_BufferEditor( ONL );
    }
    else {
      Do_dd_Normal( ONL );
    }
  }
}

void View::Do_dd_BufferEditor( const unsigned ONL )
{
  const unsigned OCL = CrsLine(); // Old cursor line

  // Can only delete one of the user files out of buffer editor
  if( CMD_FILE < OCL )
  {
    Line* lp = pfb->GetLineP( OCL );

    const char* fname = lp->c_str( 0 );

    if( !gl_pVis->File_Is_Displayed( fname ) )
    {
      gl_pVis->ReleaseFileName( fname );

      Do_dd_Normal( ONL );
    }
  }
}

void View::Do_dd_Normal( const unsigned ONL )
{
  const unsigned OCL = CrsLine();           // Old cursor line
  const unsigned OCP = CrsChar();           // Old cursor position
  const unsigned OLL = pfb->LineLen( OCL ); // Old line length

  const bool DELETING_LAST_LINE = OCL == ONL-1;

  const unsigned NCL = DELETING_LAST_LINE ? OCL-1 : OCL; // New cursor line
  const unsigned NLL = DELETING_LAST_LINE ? pfb->LineLen( NCL )
                                          : pfb->LineLen( NCL + 1 );
  const unsigned NCP = Min( OCP, 0<NLL ? NLL-1 : 0 );

  // Remove line from FileBuf and save in paste register:
  Line* lp = pfb->RemoveLineP( OCL );
  if( lp ) {
    // gl_pVis->reg will own nlp
    gl_pVis->reg.clear();
    gl_pVis->reg.push( lp );
    gl_pVis->paste_mode = PM_LINE;
  }
  GoToCrsPos_NoWrite( NCL, NCP );

  pfb->Update();
}

void View::Do_yy()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If there is nothing to 'yy', just return:
  if( !pfb->NumLines() ) return;

  Line l = pfb->GetLine( CrsLine() );

  gl_pVis->reg.clear();
  gl_pVis->reg.push( gl_pVis->BorrowLine( __FILE__,__LINE__, l ) );

  gl_pVis->paste_mode = PM_LINE;
}

void View::Do_yw()
{
  Trace trace( __PRETTY_FUNCTION__ );
  // If there is nothing to 'yw', just return:
  if( !pfb->NumLines() ) return;

  unsigned st_line = CrsLine();
  unsigned st_char = CrsChar();

  // Determine fn_line, fn_char:
  unsigned fn_line = 0;
  unsigned fn_char = 0;

  if( Do_dw_get_fn( st_line, st_char, fn_line, fn_char ) )
  {
    gl_pVis->reg.clear();
    gl_pVis->reg.push( gl_pVis->BorrowLine( __FILE__,__LINE__ ) );

    // st_line and fn_line should be the same
    for( unsigned k=st_char; k<=fn_char; k++ )
    {
      gl_pVis->reg[0]->push(__FILE__,__LINE__, pfb->Get( st_line, k ) );
    }
    gl_pVis->paste_mode = PM_ST_FN;
  }
}

void View::Do_y_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  gl_pVis->reg.clear();

  if( inVisualBlock ) Do_y_v_block();
  else                Do_y_v_st_fn();
}

void View::Do_y_v_st_fn()
{
  Trace trace( __PRETTY_FUNCTION__ );

  unsigned m_v_st_line = v_st_line;  unsigned m_v_st_char = v_st_char;
  unsigned m_v_fn_line = v_fn_line;  unsigned m_v_fn_char = v_fn_char;

  if( m_v_fn_line < m_v_st_line ) Swap( m_v_st_line, m_v_fn_line );
  if( m_v_fn_char < m_v_st_char ) Swap( m_v_st_char, m_v_fn_char );

  for( unsigned L=m_v_st_line; L<=m_v_fn_line; L++ )
  {
    Line* nlp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = pfb->LineLen( L );
    if( LL ) {
      const unsigned P_st = (L==m_v_st_line) ? m_v_st_char : 0;
      const unsigned P_fn = (L==m_v_fn_line) ? Min(LL-1,m_v_fn_char) : LL-1;

      for( unsigned P = P_st; P <= P_fn; P++ )
      {
        nlp->push(__FILE__,__LINE__, pfb->Get( L, P ) );
      }
    }
    // gl_pVis->reg will delete nlp
    gl_pVis->reg.push( nlp );
  }
  gl_pVis->paste_mode = PM_ST_FN;
}

void View::Do_y_v_block()
{
  Trace trace( __PRETTY_FUNCTION__ );

  unsigned m_v_st_line = v_st_line;  unsigned m_v_st_char = v_st_char;
  unsigned m_v_fn_line = v_fn_line;  unsigned m_v_fn_char = v_fn_char;

  if( m_v_fn_line < m_v_st_line ) Swap( m_v_st_line, m_v_fn_line );
  if( m_v_fn_char < m_v_st_char ) Swap( m_v_st_char, m_v_fn_char );

  for( unsigned L=m_v_st_line; L<=m_v_fn_line; L++ )
  {
    Line* nlp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = pfb->LineLen( L );

    for( unsigned P = m_v_st_char; P<LL && P <= m_v_fn_char; P++ )
    {
      nlp->push(__FILE__,__LINE__, pfb->Get( L, P ) );
    }
    // gl_pVis->reg will delete nlp
    gl_pVis->reg.push( nlp );
  }
  gl_pVis->paste_mode = PM_BLOCK;

  // Try to put cursor at (v_st_line, v_st_char), but
  // make sure the cursor is in bounds after the deletion:
  const unsigned NUM_LINES = pfb->NumLines();
  unsigned ncl = v_st_line;
  if( NUM_LINES <= ncl ) ncl = NUM_LINES-1;
  const unsigned NLL = pfb->LineLen( ncl );
  unsigned ncc = 0;
  if( NLL ) ncc = NLL <= v_st_char ? NLL-1 : v_st_char;

  GoToCrsPos_NoWrite( ncl, ncc );
}
void View::Do_Y_v()
{
  Trace trace( __PRETTY_FUNCTION__ );

  gl_pVis->reg.clear();

  if( inVisualBlock ) Do_y_v_block();
  else                Do_Y_v_st_fn();
}

void View::Do_Y_v_st_fn()
{
  unsigned m_v_st_line = v_st_line;
  unsigned m_v_fn_line = v_fn_line;

  if( m_v_fn_line < m_v_st_line ) Swap( m_v_st_line, m_v_fn_line );

  for( unsigned L=m_v_st_line; L<=m_v_fn_line; L++ )
  {
    Line* nlp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = pfb->LineLen(L);

    if( LL )
    {
      for( unsigned P = 0; P <= LL-1; P++ )
      {
        nlp->push(__FILE__,__LINE__, pfb->Get( L, P ) );
      }
    }
    // gl_pVis->reg will delete nlp
    gl_pVis->reg.push( nlp );
  }
  gl_pVis->paste_mode = PM_LINE;
}

void View::Do_D()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = pfb->NumLines();
  const unsigned OCL = CrsLine();  // Old cursor line
  const unsigned OCP = CrsChar();  // Old cursor position
  const unsigned OLL = pfb->LineLen( OCL );  // Old line length

  // If there is nothing to 'D', just return:
  if( !NUM_LINES || !OLL || OLL-1 < OCP ) return;

  Line* lpd = gl_pVis->BorrowLine( __FILE__,__LINE__ );

  for( unsigned k=OCP; k<OLL; k++ )
  {
    uint8_t c = pfb->RemoveChar( OCL, OCP );
    lpd->push(__FILE__,__LINE__, c );
  }
  gl_pVis->reg.clear();
  gl_pVis->reg.push( lpd );
  gl_pVis->paste_mode = PM_ST_FN;

  // If cursor is not at beginning of line, move it back one space.
  if( crsCol ) crsCol--;

  pfb->Update();
}

void View::Do_p()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( PM_ST_FN == gl_pVis->paste_mode ) return Do_p_or_P_st_fn( PP_After );
  else if( PM_BLOCK == gl_pVis->paste_mode ) return Do_p_block();
  else /*( PM_LINE  == gl_pVis->paste_mode*/ return Do_p_line();
}

void View::Do_p_line()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = CrsLine();  // Old cursor line

  const unsigned NUM_LINES = gl_pVis->reg.len();

  for( unsigned k=0; k<NUM_LINES; k++ )
  {
    // Put reg on line below:
    pfb->InsertLine( OCL+k+1, *(gl_pVis->reg[k]) );
  }
  // Update current view after other views,
  // so that the cursor will be put back in place
  pfb->Update();
}

void View::Do_p_or_P_st_fn( Paste_Pos paste_pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned N_REG_LINES = gl_pVis->reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    const unsigned NLL = gl_pVis->reg[k]->len();  // New line length
    const unsigned OCL = CrsLine();               // Old cursor line

    if( 0 == k ) // Add to current line
    {
      MoveInBounds();
      const unsigned OLL = pfb->LineLen( OCL );
      const unsigned OCP = CrsChar();               // Old cursor position

      // If line we are pasting to is zero length, dont paste a space forward
    //const unsigned add_pos = OLL ? 1 : 0;
      const unsigned forward = OLL ? ( paste_pos==PP_After ? 1 : 0 ) : 0;

      for( unsigned i=0; i<NLL; i++ )
      {
        uint8_t C = gl_pVis->reg[k]->get(i);

        pfb->InsertChar( OCL, OCP+i+forward, C );
      }
      if( 1 < N_REG_LINES && OCP+forward < OLL ) // Move rest of first line onto new line below
      {
        pfb->InsertLine( OCL+1 );
        for( unsigned i=0; i<(OLL-OCP-forward); i++ )
        {
          uint8_t C = pfb->RemoveChar( OCL, OCP + NLL+forward );
          pfb->PushChar( OCL+1, C );
        }
      }
    }
    else if( N_REG_LINES-1 == k )
    {
      // Insert a new line if at end of file:
      if( pfb->NumLines() == OCL+k ) pfb->InsertLine( OCL+k );

      for( unsigned i=0; i<NLL; i++ )
      {
        uint8_t C = gl_pVis->reg[k]->get(i);

        pfb->InsertChar( OCL+k, i, C );
      }
    }
    else {
      // Put reg on line below:
      pfb->InsertLine( OCL+k, *(gl_pVis->reg[k]) );
    }
  }
  // Update current view after other views, so that the cursor will be put back in place
  pfb->Update();
}

void View::Do_p_block()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = CrsLine();           // Old cursor line
  const unsigned OCP = CrsChar();           // Old cursor position
  const unsigned OLL = pfb->LineLen( OCL ); // Old line length
  const unsigned ISP = OCP ? OCP+1          // Insert position
                           : ( OLL ? 1:0 ); // If at beginning of line,
                                            // and LL is zero insert at 0,
                                            // else insert at 1
  const unsigned N_REG_LINES = gl_pVis->reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    if( pfb->NumLines()<=OCL+k ) pfb->InsertLine( OCL+k );

    const unsigned LL = pfb->LineLen( OCL+k );

    if( LL < ISP )
    {
      // Fill in line with white space up to ISP:
      for( unsigned i=0; i<(ISP-LL); i++ )
      {
        // Insert at end of line so undo will be atomic:
        const unsigned NLL = pfb->LineLen( OCL+k ); // New line length
        pfb->InsertChar( OCL+k, NLL, ' ' );
      }
    }
    Line& reg_line = *(gl_pVis->reg[k]);
    const unsigned RLL = reg_line.len();

    for( unsigned i=0; i<RLL; i++ )
    {
      uint8_t C = reg_line.get(i);

      pfb->InsertChar( OCL+k, ISP+i, C );
    }
  }
  pfb->Update();
}

void View::Do_P()
{
  Trace trace( __PRETTY_FUNCTION__ );

  if     ( PM_ST_FN == gl_pVis->paste_mode ) return Do_p_or_P_st_fn( PP_Before );
  else if( PM_BLOCK == gl_pVis->paste_mode ) return Do_P_block();
  else /*( PM_LINE  == gl_pVis->paste_mode*/ return Do_P_line();
}

void View::Do_P_line()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = CrsLine();  // Old cursor line

  const unsigned NUM_LINES = gl_pVis->reg.len();

  for( unsigned k=0; k<NUM_LINES; k++ )
  {
    // Put reg on line above:
    pfb->InsertLine( OCL+k, *(gl_pVis->reg[k]) );
  }
  pfb->Update();
}

void View::Do_P_block()
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OCL = CrsLine();  // Old cursor line
  const unsigned OCP = CrsChar();  // Old cursor position

  const unsigned N_REG_LINES = gl_pVis->reg.len();

  for( unsigned k=0; k<N_REG_LINES; k++ )
  {
    if( pfb->NumLines()<=OCL+k ) pfb->InsertLine( OCL+k );

    const unsigned LL = pfb->LineLen( OCL+k );

    if( LL < OCP )
    {
      // Fill in line with white space up to OCP:
      for( unsigned i=0; i<(OCP-LL); i++ ) pfb->InsertChar( OCL+k, LL, ' ' );
    }
    Line& reg_line = *(gl_pVis->reg[k]);
    const unsigned RLL = reg_line.len();

    for( unsigned i=0; i<RLL; i++ )
    {
      uint8_t C = reg_line.get(i);

      pfb->InsertChar( OCL+k, OCP+i, C );
    }
  }
  pfb->Update();
}

void View::Do_R()
{
  Trace trace( __PRETTY_FUNCTION__ );
  inReplaceMode = true;
  DisplayBanner();

  if( pfb->NumLines()==0 ) pfb->PushLine();

  for( char c=gl_pKey->In(); c != ESC; c=gl_pKey->In() )
  {
    if( BS == c || DEL == c ) pfb->Undo( this );
    else {
      if( '\n' == c ) ReplaceAddReturn();
      else            ReplaceAddChars( c );
    }
  }
  Remove_Banner();
  inReplaceMode = false;

  // Move cursor back one space:
  if( crsCol )
  {
    crsCol--;  // Move cursor back one space.
  }
  pfb->Update();
}

void View::Do_J()
{
  Trace trace( __PRETTY_FUNCTION__ );
  const unsigned NUM_LINES = pfb->NumLines(); // Number of lines
  const unsigned CL        = CrsLine();       // Cursor line

  const bool ON_LAST_LINE = ( CL == NUM_LINES-1 );

  if( ON_LAST_LINE || NUM_LINES < 2 ) return;

  GoToEndOfLine();

  Line* lp = pfb->RemoveLineP( CL+1 );
  pfb->AppendLineToLine( CL  , lp );

  // Update() is less efficient than only updating part of the screen,
  //   but it makes the code simpler.
  pfb->Update();
}

void View::Do_x_range( unsigned st_line, unsigned st_char
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

void View::Do_x_range_block( unsigned st_line, unsigned st_char
                           , unsigned fn_line, unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Do_x_range_pre( st_line, st_char, fn_line, fn_char );

  for( unsigned L = st_line; L<=fn_line; L++ )
  {
    Line* nlp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

    const unsigned LL = pfb->LineLen( L );

    for( unsigned P = st_char; P<LL && P <= fn_char; P++ )
    {
      nlp->push(__FILE__,__LINE__, pfb->RemoveChar( L, st_char ) );
    }
    gl_pVis->reg.push( nlp );
  }
  Do_x_range_post( st_line, st_char );
}

void View::Do_x_range_pre( unsigned& st_line, unsigned& st_char
                         , unsigned& fn_line, unsigned& fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( fn_line < st_line ) Swap( st_line, fn_line );
  if( fn_char < st_char ) Swap( st_char, fn_char );

  gl_pVis->reg.clear();
}

void View::Do_x_range_post( const unsigned st_line, const unsigned st_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  if( inVisualBlock ) gl_pVis->paste_mode = PM_BLOCK;
  else                gl_pVis->paste_mode = PM_ST_FN;

  // Try to put cursor at (st_line, st_char), but
  // make sure the cursor is in bounds after the deletion:
  const unsigned NUM_LINES = pfb->NumLines();
  unsigned ncl = st_line;
  if( NUM_LINES <= ncl ) ncl = NUM_LINES-1;
  const unsigned NLL = pfb->LineLen( ncl );
  unsigned ncc = 0;
  if( NLL ) ncc = NLL <= st_char ? NLL-1 : st_char;

  GoToCrsPos_NoWrite( ncl, ncc );

  inVisualMode = false;

  pfb->Update(); //<- No need to Undo_v() or Remove_Banner() because of this
}

void View::Do_x_range_single( const unsigned L
                            , const unsigned st_char
                            , const unsigned fn_char )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned OLL = pfb->LineLen( L ); // Original line length

  if( 0<OLL )
  {
    Line* nlp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

    const unsigned P_st = Min( st_char, OLL-1 ); 
    const unsigned P_fn = Min( fn_char, OLL-1 );  

    unsigned LL = OLL;

    // Dont remove a single line, or else Q wont work right
    for( unsigned P = P_st; P_st < LL && P <= P_fn; P++ )
    {
      nlp->push(__FILE__,__LINE__, pfb->RemoveChar( L, P_st ) );

      LL = pfb->LineLen( L ); // Removed a char, so re-calculate LL
    }
    gl_pVis->reg.push( nlp );
  }
}

void View::Do_x_range_multiple( const unsigned st_line
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
    Line* nlp = gl_pVis->BorrowLine( __FILE__,__LINE__ );

    const unsigned OLL = pfb->LineLen( L ); // Original line length

    const unsigned P_st = (L==  st_line) ? Min(st_char, OLL-1) : 0;
    const unsigned P_fn = (L==n_fn_line) ? Min(fn_char, OLL-1) : OLL-1;

    if(   st_line == L && 0    < P_st  ) started_in_middle = true;
    if( n_fn_line == L && P_fn < OLL-1 ) ended___in_middle = true;

    unsigned LL = OLL;

    for( unsigned P = P_st; P_st < LL && P <= P_fn; P++ )
    {
      nlp->push(__FILE__,__LINE__, pfb->RemoveChar( L, P_st ) );

      LL = pfb->LineLen( L ); // Removed a char, so re-calculate LL
    }
    if( 0 == P_st && OLL-1 == P_fn ) // Removed entire line
    {
      pfb->RemoveLine( L );
      n_fn_line--;
    }
    else L++;

    gl_pVis->reg.push( nlp );
  }
  if( started_in_middle && ended___in_middle )
  {
    Line* lp = pfb->RemoveLineP( st_line+1 );
    pfb->AppendLineToLine( st_line, lp );
  }
}

bool View::Has_Context()
{
  return 0 != topLine
      || 0 != leftChar
      || 0 != crsRow
      || 0 != crsCol ;
}

void View::Set_Context( View& vr )
{
  topLine  = vr.topLine ;
  leftChar = vr.leftChar;
  crsRow   = vr.crsRow  ;
  crsCol   = vr.crsCol  ;
}

