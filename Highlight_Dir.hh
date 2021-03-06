////////////////////////////////////////////////////////////////////////////////
// VI-Simplified (vis) C++ Implementation                                     //
// Copyright (c) 24 May 2016 Paul J. Gartside                                 //
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

#ifndef __HIGHLIGHT_DIR_HH__
#define __HIGHLIGHT_DIR_HH__

#include "Highlight_Base.hh"

class Highlight_Dir : public Highlight_Base
{
public:
  Highlight_Dir( FileBuf& rfb );

private:
  void Run_Range( const CrsPos st, const unsigned fn );

  void Hi_In_None( unsigned& l, unsigned& p );
  void Hi_In_None_Dir( const unsigned l, const unsigned LL );
  void Hi_In_None_File( const unsigned l, const unsigned LL );

  typedef Highlight_Dir ME;
  typedef void (Highlight_Dir::*HiStateFunc) ( unsigned&, unsigned& );

  HiStateFunc m_state;
};

#endif

