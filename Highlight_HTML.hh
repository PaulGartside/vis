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

#ifndef __HIGHLIGHT_HTML_HH__
#define __HIGHLIGHT_HTML_HH__

#include "Highlight_Base.hh"

class Edges;

class Highlight_HTML : public Highlight_Base
{
public:
  Highlight_HTML( FileBuf& rfb );

private:
  enum Hi_State
  {
    St_In_None       ,
    St_OpenTag_ElemName,
    St_OpenTag_AttrName,
    St_OpenTag_AttrVal,
    St_CloseTag      ,
    St_XML_Comment   ,
    St_SingleQuote   ,
    St_DoubleQuote   ,
    St_NumberBeg     ,
    St_NumberDec     ,
    St_NumberHex     ,
    St_NumberExponent,
    St_NumberFraction,
    St_NumberTypeSpec,

    St_JS_None       ,
    St_JS_Define     ,
    St_JS_C_Comment  ,
    St_JS_CPP_Comment,
    St_JS_SingleQuote,
    St_JS_DoubleQuote,
    St_JS_NumberBeg  ,
    St_JS_NumberDec  ,
    St_JS_NumberHex  ,
    St_JS_NumberExponent,
    St_JS_NumberFraction,
    St_JS_NumberTypeSpec,

    St_CS_None       ,
    St_CS_C_Comment  ,
    St_CS_SingleQuote,
    St_CS_DoubleQuote,

    St_Done
  };
  void Run_Range( const CrsPos st, const unsigned fn );
  void Find_Styles_Keys_In_Range( const CrsPos st, const unsigned fn );

  Hi_State Run_Range_Get_Initial_State( const CrsPos st );

  bool Get_Initial_State( const CrsPos st
                        , Array_t<Edges> edges_1
                        , Array_t<Edges> edges_2 );
  bool JS_State( const Hi_State state );
  void Run_State();

  void Hi_In_None         ();
  void Hi_XML_Comment     ();
  void Hi_CloseTag        ();
  void Hi_NumberBeg       ();
  void Hi_NumberHex       ();
  void Hi_NumberDec       ();
  void Hi_NumberExponent  ();
  void Hi_NumberFraction  ();
  void Hi_NumberTypeSpec  ();
  void Hi_OpenTag_ElemName();
  void Hi_OpenTag_AttrName();
  void Hi_OpenTag_AttrVal ();
  void Hi_SingleQuote     ();
  void Hi_DoubleQuote     ();

  void Hi_JS_None       ();
  void Hi_JS_Define     ();
  void Hi_C_Comment     ();
  void Hi_JS_CPP_Comment();

  void Hi_CS_None();

  unsigned Has_HTTP_Tag_At( const Line* lp, unsigned pos );

  const char* State_2_str( const Hi_State state );

  Hi_State m_state; // Current state
  Hi_State m_qtXSt; // Quote exit state
  Hi_State m_ccXSt; // C comment exit state
  Hi_State m_numXSt; // Number exit state
  unsigned m_l; // Line
  unsigned m_p; // Position on line

  // Variables to go in and out of JS:
  bool m_OpenTag_was_script;
  bool m_OpenTag_was_style;
  Array_t<Edges> m_JS_edges;
  Array_t<Edges> m_CS_edges;

  static const char* m_HTML_Tags[];
  static HiKeyVal m_JS_HiPairs[];
};

#endif

