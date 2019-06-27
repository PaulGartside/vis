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

#ifndef __TYPES_HH__
#define __TYPES_HH__

#include "Array_t.hh"
#include "gArray_t.hh"
#include "Line.hh"

const unsigned MAX_WINS = 8;  // Maximum number of sub-windows

#ifdef WIN32
const char  DIR_DELIM = '\\';
#else
const char  DIR_DELIM = '/';
#endif

class View;
class FileBuf;
class LineChange;

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

enum Paste_Mode
{
  PM_LINE,  // Whole line from start line to finish line
  PM_ST_FN, // Visual mode from start to finish
  PM_BLOCK  // Visual mode rectangular block spanning one or more lines
};

enum Paste_Pos
{
  PP_Before, PP_After
};

// HI for Highlight
enum HighlightType
{
  HI_STAR     = 0x01, // Search pattern
  HI_COMMENT  = 0x02, // Comment
  HI_DEFINE   = 0x04, // #define
  HI_CONST    = 0x08, // C constant, '...' or "...", true, false
  HI_CONTROL  = 0x10, // C flow control, if, else, etc.
  HI_VARTYPE  = 0x20, // C variable type, char, int, etc.
  HI_NONASCII = 0x40 // Non-ascii character
};

enum Style
{
  S_NORMAL   , S_RV_NORMAL   ,
  S_STATUS   , S_RV_STATUS   ,
  S_BORDER   , S_RV_BORDER   ,
  S_BORDER_HI, S_RV_BORDER_HI,
  S_BANNER   , S_RV_BANNER   ,
  S_STAR     , S_RV_STAR     ,
  S_COMMENT  , S_RV_COMMENT  ,
  S_DEFINE   , S_RV_DEFINE   ,
  S_CONST    , S_RV_CONST    ,
  S_CONTROL  , S_RV_CONTROL  ,
  S_VARTYPE  , S_RV_VARTYPE  ,
  S_VISUAL   , S_RV_VISUAL   ,
  S_NONASCII , S_RV_NONASCII ,
  S_EMPTY    ,
  S_EOF      ,
  S_DIFF_DEL    ,
  S_DIFF_NORMAL ,
  S_DIFF_STAR   ,
  S_DIFF_COMMENT,
  S_DIFF_DEFINE ,
  S_DIFF_CONST  ,
  S_DIFF_CONTROL,
  S_DIFF_VARTYPE,
  S_DIFF_VISUAL ,
  S_UNKNOWN
};

enum Color
{
  Black,  White, Gray,
  Red,    Green, Blue,
  Yellow, Cyan,  Magenta
};

struct CrsPos // CursorPosition
{
  unsigned crsLine; // absolute cursor line in file.
  unsigned crsChar; // absolute cursor char position on current line.
};

struct CmntPos
{
  CrsPos st;
  CrsPos fn;
};

typedef  Array_t<View*>       ViewList;
typedef  Array_t<FileBuf*>    FileList;
typedef  Array_t<const char*> ConstCharList;
typedef  Array_t<unsigned>    unsList;
typedef  Array_t<bool>        boolList;
typedef gArray_t<Line*>       LinesList;
typedef  Array_t<CrsPos>      PosList;
typedef  Array_t<CmntPos>     CmntList;
typedef gArray_t<LineChange*> ChangeList;

enum ChangeType
{
   Insert_Line,
   Remove_Line,
   Insert_Text,
   Remove_Text,
  Replace_Text
};

enum Encoding
{
  ENC_BYTE,
  ENC_HEX
};

const char* ChangeType_Str( const ChangeType ct );
const char* Encoding_Str( const Encoding E );

enum File_Type
{
  FT_UNKNOWN,
  FT_BASH,
  FT_BUFFER_EDITOR,
  FT_CPP,
  FT_DIR,
  FT_HTML,
  FT_IDL,
  FT_JAVA,
  FT_JS,
  FT_MAKE,
  FT_MIB,
  FT_CMAKE,
  FT_ODB,
  FT_PY,
  FT_SQL,
  FT_STL,
  FT_SWIFT,
  FT_TCL,
  FT_TEXT,
  FT_XML
};

enum Tile_Pos
{
  TP_NONE,
  // 2 x 2 tiles:
  TP_FULL,
  TP_LEFT_HALF,
  TP_RITE_HALF,
  TP_TOP__HALF,
  TP_BOT__HALF,
  TP_TOP__LEFT_QTR,
  TP_TOP__RITE_QTR,
  TP_BOT__LEFT_QTR,
  TP_BOT__RITE_QTR,
  // Extra tiles needed for 2 x 4:
  TP_LEFT_QTR,
  TP_RITE_QTR,
  TP_LEFT_CTR__QTR,
  TP_RITE_CTR__QTR,
  TP_TOP__LEFT_8TH,
  TP_TOP__RITE_8TH,
  TP_TOP__LEFT_CTR_8TH,
  TP_TOP__RITE_CTR_8TH,
  TP_BOT__LEFT_8TH,
  TP_BOT__RITE_8TH,
  TP_BOT__LEFT_CTR_8TH,
  TP_BOT__RITE_CTR_8TH,
  // 1 x 3 tiles:
  TP_LEFT_THIRD,
  TP_CTR__THIRD,
  TP_RITE_THIRD,
  TP_LEFT_TWO_THIRDS,
  TP_RITE_TWO_THIRDS
};

#endif

