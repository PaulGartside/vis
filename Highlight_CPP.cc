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
#include "Highlight_CPP.hh"

static HiKeyVal HiPairs[] =
{
  { "if"                 , HI_CONTROL },
  { "else"               , HI_CONTROL },
  { "for"                , HI_CONTROL },
  { "while"              , HI_CONTROL },
  { "do"                 , HI_CONTROL },
  { "return"             , HI_CONTROL },
  { "switch"             , HI_CONTROL },
  { "case"               , HI_CONTROL },
  { "break"              , HI_CONTROL },
  { "default"            , HI_CONTROL },
  { "continue"           , HI_CONTROL },
  { "template"           , HI_CONTROL },
  { "public"             , HI_CONTROL },
  { "protected"          , HI_CONTROL },
  { "private"            , HI_CONTROL },
  { "typedef"            , HI_CONTROL },
  { "delete"             , HI_CONTROL },
  { "operator"           , HI_CONTROL },
  { "sizeof"             , HI_CONTROL },
  { "using"              , HI_CONTROL },
  { "namespace"          , HI_CONTROL },
  { "goto"               , HI_CONTROL },
  { "friend"             , HI_CONTROL },
  { "try"                , HI_CONTROL },
  { "catch"              , HI_CONTROL },
  { "throw"              , HI_CONTROL },
  { "and"                , HI_CONTROL },
  { "or"                 , HI_CONTROL },
  { "not"                , HI_CONTROL },
  { "new"                , HI_CONTROL },
  { "const_cast"         , HI_CONTROL },
  { "static_cast"        , HI_CONTROL },
  { "dynamic_cast"       , HI_CONTROL },
  { "reinterpret_cast"   , HI_CONTROL },

  { "int"                , HI_VARTYPE },
  { "long"               , HI_VARTYPE },
  { "void"               , HI_VARTYPE },
  { "this"               , HI_VARTYPE },
  { "bool"               , HI_VARTYPE },
  { "char"               , HI_VARTYPE },
  { "const"              , HI_VARTYPE },
  { "short"              , HI_VARTYPE },
  { "float"              , HI_VARTYPE },
  { "double"             , HI_VARTYPE },
  { "signed"             , HI_VARTYPE },
  { "unsigned"           , HI_VARTYPE },
  { "extern"             , HI_VARTYPE },
  { "static"             , HI_VARTYPE },
  { "enum"               , HI_VARTYPE },
  { "uint8_t"            , HI_VARTYPE },
  { "uint16_t"           , HI_VARTYPE },
  { "uint32_t"           , HI_VARTYPE },
  { "uint64_t"           , HI_VARTYPE },
  { "size_t"             , HI_VARTYPE },
  { "int8_t"             , HI_VARTYPE },
  { "int16_t"            , HI_VARTYPE },
  { "int32_t"            , HI_VARTYPE },
  { "int64_t"            , HI_VARTYPE },
  { "float32_t"          , HI_VARTYPE },
  { "float64_t"          , HI_VARTYPE },
  { "FILE"               , HI_VARTYPE },
  { "DIR"                , HI_VARTYPE },
  { "class"              , HI_VARTYPE },
  { "struct"             , HI_VARTYPE },
  { "union"              , HI_VARTYPE },
  { "typename"           , HI_VARTYPE },
  { "virtual"            , HI_VARTYPE },
  { "inline"             , HI_VARTYPE },
  { "true"               , HI_CONST   },
  { "false"              , HI_CONST   },
  { "NULL"               , HI_CONST   },
  { "nullptr"            , HI_CONST   },
  { "__FUNCTION__"       , HI_DEFINE  },
  { "__PRETTY_FUNCTION__", HI_DEFINE  },
  { "__TIMESTAMP__"      , HI_DEFINE  },
  { "__FILE__"           , HI_DEFINE  },
  { "__func__"           , HI_DEFINE  },
  { "__LINE__"           , HI_DEFINE  },
  { 0 }
};

Highlight_CPP::Highlight_CPP( FileBuf& rfb )
  : Highlight_Code( rfb )
{
}

void Highlight_CPP::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Hi_FindKey_In_Range( HiPairs, st, fn );
}

