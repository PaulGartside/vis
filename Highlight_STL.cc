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

#include "Utilities.hh"
#include "FileBuf.hh"
#include "Highlight_STL.hh"

static HiKeyVal HiPairs[] =
{
  { "if"          , HI_CONTROL },
  { "else"        , HI_CONTROL },
  { "for"         , HI_CONTROL },
  { "while"       , HI_CONTROL },
  { "return"      , HI_CONTROL },
  { "sizeof"      , HI_CONTROL },
  { "die"         , HI_CONTROL },
  { "uchar"       , HI_VARTYPE },
  { "ushort"      , HI_VARTYPE },
  { "uint"        , HI_VARTYPE },
  { "int"         , HI_VARTYPE },
  { "void"        , HI_VARTYPE },
  { "bool"        , HI_VARTYPE },
  { "const"       , HI_VARTYPE },
  { "float"       , HI_VARTYPE },
  { "double"      , HI_VARTYPE },
  { "string"      , HI_VARTYPE },
  { "List"        , HI_VARTYPE },
  { "FILE"        , HI_VARTYPE },
  { "DIR"         , HI_VARTYPE },
  { "class"       , HI_VARTYPE },
  { "sub"         , HI_VARTYPE },
  { "true"        , HI_CONST   },
  { "false"       , HI_CONST   },
  { "__FUNCTION__", HI_DEFINE  },
  { "__FILE__"    , HI_DEFINE  },
  { "__LINE__"    , HI_DEFINE  },
  { 0 }
};

Highlight_STL::Highlight_STL( FileBuf& rfb )
  : Highlight_Code( rfb )
{
}

void Highlight_STL::Find_Styles_Keys()
{
  Hi_FindKey( HiPairs );
}

void Highlight_STL::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Hi_FindKey_In_Range( HiPairs, st, fn );
}

