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

#ifndef __COLON_HH__
#define __COLON_HH__

typedef unsigned char  uint8_t;

class FileBuf;
class String;
class Vis;

enum ColonOp
{ 
  OP_unknown,
  OP_e,
  OP_w
};

class Colon
{
public:
  Colon( Vis& vis );
  void GetCommand( const unsigned MSG_LEN, const bool HIDE=false );

  void b();
  void e();
  void w();
  void hi();
  void MapStart();
  void MapEnd();
  void MapShow();
  void Cover();
  void CoverKey();

private:
  void Reset_File_Name_Completion_Variables();

  void HandleNormal( const unsigned MSG_LEN
                   , const bool     HIDE
                   , const uint8_t  c
                   ,       char*&   p );

  void HandleTab( const unsigned  MSG_LEN
                ,       char*&    p );

  bool Find_File_Name_Completion_Variables();
  bool Have_File_Name_Completion_Variables();

  bool FindFileBuf();

  void DisplaySbuf( char*& p );

  Vis&     m_vis;
  View*    m_cv;
  FileBuf* m_fb;
  char*    m_cbuf;
  String&  m_sbuf;
  String   m_cover_key;
  Line     m_cover_buf;
  unsigned m_file_index;
  String   m_partial_path;
  String   m_search__head;
  ColonOp  m_colon_op;
};

#endif

