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

#ifndef __FILE_BUF_HH__
#define __FILE_BUF_HH__

#include "Types.hh"

class String;
class Vis;

// Basic description of the file buffer
class FileBuf
{
public:
  FileBuf( Vis& vis
         , const char* const FILE_NAME
         , const bool MUTABLE
         , const File_Type FT );
  FileBuf( Vis& vis
         , const char* const FILE_NAME
         , const FileBuf& rfb );
  ~FileBuf();

  bool IsDir() const;
  double GetModTime() const;
  void SetModTime( const double mt );
  const char* GetFileName() const;
  const char* GetPathName() const;
  const char* GetHeadName() const;
  void NeedToFindStars();
  void NeedToClearStars();

  unsigned NumLines() const;
  unsigned LineLen( const unsigned line_num ) const;
  Line     GetLine( const unsigned l_num ) const;
  Line     GetStyle( const unsigned l_num ) const;
  void     GetLine( const unsigned l_num, Line& l ) const;
  Line*    GetLineP( const unsigned l_num ) const;
  void     InsertLine( const unsigned l_num, const Line& line );
  void     InsertLine( const unsigned l_num, Line* const pLine );
  void     InsertLine( const unsigned l_num );
  void     InsertLine_Adjust_Views_topLines( const unsigned l_num );
  void     InsertChar( const unsigned l_num, const unsigned c_num
                     , const uint8_t C );
  void     PushLine( const Line& line );
  void     PushLine( Line* const pLine );
  void     PushLine();
  void     PushChar( const unsigned l_num, const uint8_t C );
  void     PushChar( const uint8_t C );
  uint8_t  PopChar( const unsigned l_num );
  void     RemoveLine( const unsigned l_num, Line& line );
  Line*    RemoveLineP( const unsigned l_num );
  void     RemoveLine( const unsigned l_num );
  uint8_t  RemoveChar( const unsigned l_num, const unsigned c_num );
  void     PopLine( Line& line );
  void     PopLine();
  void     AppendLineToLine( const unsigned l_num, const Line& line );
  void     AppendLineToLine( const unsigned l_num, const Line* pLine );
  unsigned GetSize();
  unsigned GetCursorByte( const unsigned CL, const unsigned CC );
  bool     Changed() const;
  void     ClearChanged();
  void     ClearLines();
  void     Undo( View& rV );
  void     UndoAll( View& rV );
  void     Update();
  void Set_File_Type( const char* syn );

  void AddView( View* v );

  void ReadString( const char* const STR );
  void ReadArray( const Line& line );
  void ReadFile();
  void ReReadFile();
  void Write();
  void BufferEditor_Sort();

  // File bytes container methods
  uint8_t  Get( const unsigned l_num, const unsigned c_num ) const;
  void     Set( const unsigned l_num, const unsigned c_num
              , const uint8_t C, const bool continue_last_update=true );
  bool     Has_LF_at_EOF();
  void ClearStyles();
  void Find_Styles( const unsigned up_to_line );
  void Find_Stars();
  void ClearStars();
  void ClearSyntaxStyles( const unsigned l_num, const unsigned c_num );
  void SetSyntaxStyle( const unsigned l_num, const unsigned c_num
                     , const unsigned style );
  bool HasStyle( const unsigned l_num, const unsigned c_num
               , const unsigned style );
  struct Data;

private:
  Data& m;
};

#endif

