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

#ifndef __CHANGE_HIST_HH__
#define __CHANGE_HIST_HH__

class Vis;
class View;
class FileBuf;

#include "Types.hh"

struct LineChange
{
  ChangeType type;
  unsigned   lnum;
  unsigned   cpos;
  Line       line;

  LineChange( const ChangeType type
            , const unsigned   lnum
            , const unsigned   cpos )
    : type( type )
    , lnum( lnum )
    , cpos( cpos )
    , line()
  {}
};

class ChangeHist
{
public:
  ChangeHist( Vis& vis, FileBuf& fb );

  void Clear();
  bool Has_Changes() const;

  void Undo( View& rV );
  void UndoAll( View& rV );

  void Save_Set( const unsigned l_num
               , const unsigned c_pos
               , const uint8_t  old_C
               , const bool     continue_last_update=true );

  void Save_InsertLine( const unsigned l_num );
  void Save_InsertChar( const unsigned l_num
                      , const unsigned c_pos );
  void Save_RemoveLine( const unsigned l_num
                      , const Line&    line );
  void Save_RemoveChar( const unsigned l_num
                      , const unsigned c_pos
                      , const uint8_t  old_C );
  void Save_SwapLines( const unsigned l_num_1
                     , const unsigned l_num_2 );
private:
  void Undo_InsertLine( LineChange* plc, View& rV );
  void Undo_RemoveLine( LineChange* plc, View& rV );
  void Undo_InsertChar( LineChange* plc, View& rV );
  void Undo_RemoveChar( LineChange* plc, View& rV );
  void Undo_Set       ( LineChange* plc, View& rV );

  void Undo_InsertLine_Diff( LineChange* plc, View& rV );
  void Undo_RemoveLine_Diff( LineChange* plc, View& rV );
  void Undo_InsertChar_Diff( LineChange* plc, View& rV );
  void Undo_RemoveChar_Diff( LineChange* plc, View& rV );
  void Undo_Set_Diff       ( LineChange* plc, View& rV );

  LineChange* BorrowLineChange( const ChangeType type
                              , const unsigned   lnum
                              , const unsigned   cpos );
  void  ReturnLineChange( LineChange* lcp );

  Vis&       m_vis;
  FileBuf&   m_fb;
  ChangeList changes;
};

#endif

