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

#include <stdio.h>     // printf, stderr, FILE, fopen, fclose
#include <dirent.h>

#include "String.hh"
#include "Types.hh"
#include "ChangeHist.hh"

class Highlight_Base;
class Highlight_HTML;

// Similar to LineChange, but tells what line
// needs to be updated and in what way
struct LineUpdate 
{
  ChangeType type;
  unsigned   lnum;
  unsigned   cpos;
  unsigned   nchars;

  LineUpdate()
    : type( Insert_Text )
    , lnum( 0 )
    , cpos( 0 )
    , nchars( 0 )
  {}
  LineUpdate( const ChangeType type
            , const unsigned   lnum
            , const unsigned   cpos
            , const unsigned   nchars )
    : type( type )
    , lnum( lnum )
    , cpos( cpos )
    , nchars( nchars )
  {}
};

typedef Array_t<LineUpdate> UpdateList;

// Basic description of the file buffer
class FileBuf
{
public:
  FileBuf( const char* const FILE_NAME, const bool MUTABLE, const File_Type FT );
  FileBuf( const char* const FILE_NAME, const FileBuf& rfb );
  ~FileBuf();

  void Find_File_Type_Suffix();
  bool Find_File_Type_Bash();
  bool Find_File_Type_CPP();
  bool Find_File_Type_IDL();
  bool Find_File_Type_Java();
  bool Find_File_Type_HTML();
  bool Find_File_Type_XML();
  bool Find_File_Type_JS();
  bool Find_File_Type_ODB();
  bool Find_File_Type_SQL();
  bool Find_File_Type_STL();
  bool Find_File_Type_Swift();
  bool Find_File_Type_TCL();

  void Find_File_Type_FirstLine();
  void Set_File_Type( const char* syn );

  void AddView( View* v );

  void ReadString( const char* const STR );
  void ReadArray( const Line& line );
  void ReadFile();
  void ReReadFile();
  void ReadExistingFile( FILE* fp );
  void ReadExistingDir ( DIR * dp, String dir_path );
  void ReadExistingDir_AddLink( const String& dir_path_fname, const unsigned LINE_NUM );
  void ReadExistingDir_Sort();
  void BufferEditor_Sort();
  void Write();

  // File bytes container methods
  unsigned NumLines() const;
  unsigned LineLen( const unsigned line_num ) const;
  uint8_t  Get( const unsigned l_num, const unsigned c_num ) const;
  uint8_t  GetEnd( const unsigned l_num ) const;
  void     Set( const unsigned l_num, const unsigned c_num, const uint8_t C, const bool continue_last_update=true );
  Line     GetLine( const unsigned l_num ) const;
  Line     GetStyle( const unsigned l_num ) const;
  void     GetLine( const unsigned l_num, Line& l ) const;
  Line*    GetLineP( const unsigned l_num ) const;
  void     InsertLine( const unsigned l_num, const Line& line );
  void     InsertLine( const unsigned l_num, Line* const pLine );
  void     InsertLine( const unsigned l_num );
  void     InsertLine_Adjust_Views_topLines( const unsigned l_num );
  void     InsertChar( const unsigned l_num, const unsigned c_num, const uint8_t C );
  void     PushLine( const Line& line );
  void     PushLine( Line* const pLine );
  void     PushLine();
  void     PushChar( const unsigned l_num, const uint8_t C );
  void     PushChar( const uint8_t C );
  void     RemoveLine( const unsigned l_num, Line& line );
  Line*    RemoveLineP( const unsigned l_num );
  void     RemoveLine( const unsigned l_num );
  void     RemoveLine_Adjust_Views_topLines( const unsigned l_num );
  uint8_t  RemoveChar( const unsigned l_num, const unsigned c_num );
  void     PopLine( Line& line );
  void     PopLine();
  uint8_t  PopChar( const unsigned l_num );
  void     AppendLineToLine( const unsigned l_num, const Line& line );
  void     AppendLineToLine( const unsigned l_num, const Line* pLine );
  void     SwapLines( const unsigned l_num_1, const unsigned l_num_2 );
  unsigned GetSize();
  unsigned GetCursorByte( const unsigned CL, const unsigned CC );
  bool     Has_LF_at_EOF() { return LF_at_EOF; }
  bool     Changed() const;
  void     ClearChanged();
  void     ClearLines();
  void     Undo( View* const pV );
  void     UndoAll( View* const pV );
  void     Update();
  void ClearStyles();
  void ClearStyles_In_Range( const CrsPos st, const CrsPos fn );
  CrsPos Update_Styles_Find_St( const unsigned first_line );
  void Find_Styles_In_Range( const CrsPos st, const int fn );
  void Find_Styles( const unsigned up_to_line );
  void Find_Styles_Keys_In_Range( const CrsPos st, const CrsPos fn );
  typedef void (FileBuf::*HiStateFunc) ( unsigned&, unsigned& );
  void Find_Stars();
  void Find_Stars_In_Range( const CrsPos st, const int fn );

  void ClearStars();
  void ClearStars_In_Range( const CrsPos st, const int fn );

  void ClearAllStyles( const unsigned l_num, const unsigned c_num );
  void ClearSyntaxStyles( const unsigned l_num, const unsigned c_num );
  void ClearStarStyle( const unsigned l_num, const unsigned c_num );
  void SetSyntaxStyle( const unsigned l_num, const unsigned c_num, const unsigned style );
  void SetStarStyle( const unsigned l_num, const unsigned c_num );

  bool HasStyle( const unsigned l_num, const unsigned c_num, const unsigned style );
  bool SavingHist() const;

  String     file_name;
  bool       is_dir;
  double     mod_time;
  ViewList   views; // List of views that display this file
  bool       need_2_find_stars;
  bool       need_2_clear_stars;

private:
  void ChangedLine( const unsigned line_num );

  ChangeHist history;
  bool       save_history;
  unsList    lineOffsets; // absolute byte offset of beginning of line in file
  LinesList  lines;    // list of file lines.
  LinesList  styles;   // list of file styles.
  unsigned   hi_touched_line; // Line before which highlighting is valid
  bool       LF_at_EOF; // Line feed at end of file
  File_Type  file_type;
  Highlight_Base* pHi;
  const bool m_mutable;
};

#endif

