////////////////////////////////////////////////////////////////////////////////
// VI-Simplified (vis) C++ Implementation                                     //
// Copyright (c) 02 Jul 2024 Paul J. Gartside                                 //
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

#ifndef __HIGHLIGHT_GO_HH__
#define __HIGHLIGHT_GO_HH__

#include "Highlight_Base.hh"

class Highlight_Go : public Highlight_Base
{
public:
  Highlight_Go( FileBuf& rfb );

private:
  void Run_Range( const CrsPos st, const unsigned fn );

  void Hi_In_None       ( unsigned& l, unsigned& p );
  void Hi_In_Define     ( unsigned& l, unsigned& p );
  void Hi_BegC_Comment  ( unsigned& l, unsigned& p );
  void Hi_In_C_Comment  ( unsigned& l, unsigned& p );
  void Hi_EndC_Comment  ( unsigned& l, unsigned& p );
  void Hi_BegCPP_Comment( unsigned& l, unsigned& p );
  void Hi_In_CPP_Comment( unsigned& l, unsigned& p );
  void Hi_EndCPP_Comment( unsigned& l, unsigned& p );
  void Hi_In_SingleQuote( unsigned& l, unsigned& p );
  void Hi_In_DoubleQuote( unsigned& l, unsigned& p );
  void Hi_In_Back__Quote( unsigned& l, unsigned& p );
  void Hi_NumberBeg     ( unsigned& l, unsigned& p );
  void Hi_NumberIn      ( unsigned& l, unsigned& p );
  void Hi_NumberHex     ( unsigned& l, unsigned& p );
  void Hi_NumberFraction( unsigned& l, unsigned& p );
  void Hi_NumberExponent( unsigned& l, unsigned& p );
  void Hi_NumberTypeSpec( unsigned& l, unsigned& p );

  typedef Highlight_Go ME;
  typedef void (ME::*HiStateFunc) ( unsigned&, unsigned& );

  virtual void Find_Styles_Keys_In_Range( const CrsPos st, const unsigned fn );

  HiStateFunc m_state;
};

#endif

