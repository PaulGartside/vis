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

#include <stdarg.h>    // va_list, va_start, va_end

#include "MemLog.hh"
#include "Utilities.hh"
#include "FileBuf.hh"
#include "Vis.hh"
#include "View.hh"
#include "ChangeHist.hh"

extern MemLog<MEM_LOG_BUF_SIZE> Log;

ChangeHist::ChangeHist( Vis& vis, FileBuf& fb )
  : m_vis( vis )
  , m_fb( fb )
  , changes()
{}

void ChangeHist::Clear()
{
  Trace trace( __PRETTY_FUNCTION__ );

  LineChange* plc = 0;

  while( changes.pop( plc ) )
  {
    m_vis.ReturnLineChange( plc );
  }
}

bool ChangeHist::Has_Changes() const
{
  return !! changes.len();
}

void ChangeHist::Undo( View& rV )
{
  Trace trace( __PRETTY_FUNCTION__ );

  LineChange* plc = 0;

  if( changes.pop( plc ) )
  {
    const ChangeType ct = plc->type;
    if     ( ct ==  Insert_Line ) Undo_InsertLine( plc, rV );
    else if( ct ==  Remove_Line ) Undo_RemoveLine( plc, rV );
    else if( ct ==  Insert_Text ) Undo_InsertChar( plc, rV );
    else if( ct ==  Remove_Text ) Undo_RemoveChar( plc, rV );
    else if( ct == Replace_Text ) Undo_Set       ( plc, rV );

    m_vis.ReturnLineChange( plc );
  }
}

void ChangeHist::UndoAll( View& rV )
{
  Trace trace( __PRETTY_FUNCTION__ );

  while( 0 < changes.len() )
  {
    Undo( rV );
  }
}

void ChangeHist::Save_Set( const unsigned l_num
                         , const unsigned c_pos
                         , const uint8_t  old_C
                         , const bool     continue_last_update )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_CHANGES = changes.len();

  if( NUM_CHANGES
   && continue_last_update
   && c_pos
   && Replace_Text == changes[NUM_CHANGES-1]->type
   && l_num        == changes[NUM_CHANGES-1]->lnum
   && c_pos        == ( changes[NUM_CHANGES-1]->cpos
                      + changes[NUM_CHANGES-1]->line.len() ) )
  {
    // Continuation of previous replacement:
    changes[NUM_CHANGES-1]->line.push( __FILE__, __LINE__, old_C );
  }
  else {
    // Start of new replacement:
    LineChange* lc = m_vis.BorrowLineChange( Replace_Text, l_num, c_pos );
    lc->line.push( __FILE__, __LINE__, old_C );

    changes.push( lc );
  }
}

void ChangeHist::Save_InsertLine( const unsigned l_num )
{
  Trace trace( __PRETTY_FUNCTION__ );

  LineChange* lc = m_vis.BorrowLineChange( Insert_Line, l_num, 0 );

  changes.push( lc );
}

void ChangeHist::Save_InsertChar( const unsigned l_num
                                , const unsigned c_pos )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_CHANGES = changes.len();

  if( NUM_CHANGES
   && c_pos
   && Insert_Text == changes[NUM_CHANGES-1]->type
   && l_num       == changes[NUM_CHANGES-1]->lnum
   && c_pos       == ( changes[NUM_CHANGES-1]->cpos
                     + changes[NUM_CHANGES-1]->line.len() ) )
  {
    // Continuation of previous insertion:
    changes[NUM_CHANGES-1]->line.push( __FILE__, __LINE__, 0 );
  }
  else {
    // Start of new insertion:
    LineChange* lc = m_vis.BorrowLineChange( Insert_Text, l_num, c_pos );
    lc->line.push( __FILE__, __LINE__, 0 );

    changes.push( lc );
  }
}

void ChangeHist::Save_RemoveLine( const unsigned l_num
                                , const Line&    line )
{
  Trace trace( __PRETTY_FUNCTION__ );

  LineChange* lc = m_vis.BorrowLineChange( Remove_Line, l_num, 0 );

  // Copy line into lc-Line:
  lc->line.clear();
  for( unsigned k=0; k<line.len(); k++ ) lc->line.push( __FILE__, __LINE__, line.get(k) );

  changes.push( lc );
}

void ChangeHist::Save_RemoveChar( const unsigned l_num
                                , const unsigned c_pos
                                , const uint8_t  old_C )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned NUM_CHANGES = changes.len();

  if( NUM_CHANGES
   && Remove_Text == changes[NUM_CHANGES-1]->type
   && l_num       == changes[NUM_CHANGES-1]->lnum
   && c_pos       == changes[NUM_CHANGES-1]->cpos )
  {
    // Continuation of previous removal:
    changes[NUM_CHANGES-1]->line.push( __FILE__, __LINE__, old_C );
  }
  else {
    // Start of new removal:
    LineChange* lc = m_vis.BorrowLineChange( Remove_Text, l_num, c_pos );
    lc->line.push( __FILE__, __LINE__, old_C );

    changes.push( lc );
  }
}

void ChangeHist::Save_SwapLines( const unsigned l_num_1
                               , const unsigned l_num_2 )
{
}

void ChangeHist::Undo_Set( LineChange* plc, View& rV )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned LINE_LEN = plc->line.len();

  for( unsigned k=0; k<LINE_LEN; k++ )
  {
    const uint8_t C = plc->line.get(k);

    m_fb.Set( plc->lnum, plc->cpos+k, C );
  }
  rV.GoToCrsPos_Write( plc->lnum, plc->cpos );

  m_fb.Update();
}

void ChangeHist::Undo_InsertLine( LineChange* plc, View& rV )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Undo an inserted line by removing the inserted line
  m_fb.RemoveLine( plc->lnum, plc->line );

  // If last line of file was just removed, plc->lnum is out of range,
  // so go to NUM_LINES-1 instead:
  const unsigned NUM_LINES = m_fb.NumLines();
  const unsigned LINE_NUM  = plc->lnum < NUM_LINES ? plc->lnum : NUM_LINES-1;

  rV.GoToCrsPos_Write( LINE_NUM, plc->cpos );

  m_fb.Update();
}

void ChangeHist::Undo_RemoveLine( LineChange* plc, View& rV )
{
  Trace trace( __PRETTY_FUNCTION__ );

  // Undo a removed line by inserting the removed line
  m_fb.InsertLine( plc->lnum, plc->line );

  rV.GoToCrsPos_Write( plc->lnum, plc->cpos );

  m_fb.Update();
}

void ChangeHist::Undo_InsertChar( LineChange* plc, View& rV )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned LINE_LEN = plc->line.len();

  // Undo inserted chars by removing the inserted chars
  for( unsigned k=0; k<LINE_LEN; k++ )
  {
    m_fb.RemoveChar( plc->lnum, plc->cpos );
  }
  rV.GoToCrsPos_Write( plc->lnum, plc->cpos );

  m_fb.Update();
}

void ChangeHist::Undo_RemoveChar( LineChange* plc, View& rV )
{
  Trace trace( __PRETTY_FUNCTION__ );

  const unsigned LINE_LEN = plc->line.len();

  // Undo removed chars by inserting the removed chars
  for( unsigned k=0; k<LINE_LEN; k++ )
  {
    const uint8_t C = plc->line.get(k);

    m_fb.InsertChar( plc->lnum, plc->cpos+k, C );
  }
  rV.GoToCrsPos_Write( plc->lnum, plc->cpos );

  m_fb.Update();
}

