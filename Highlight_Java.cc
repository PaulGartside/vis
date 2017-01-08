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
#include "Highlight_Java.hh"

static HiKeyVal HiPairs[] =
{
  { "abstract"           , HI_VARTYPE },
  { "boolean"            , HI_VARTYPE },
  { "Boolean"            , HI_VARTYPE },
  { "break"              , HI_CONTROL },
  { "byte"               , HI_VARTYPE },
  { "Byte"               , HI_VARTYPE },
  { "case"               , HI_CONTROL },
  { "catch"              , HI_CONTROL },
  { "char"               , HI_VARTYPE },
  { "Character"          , HI_VARTYPE },
  { "class"              , HI_VARTYPE },
  { "const"              , HI_VARTYPE },
  { "continue"           , HI_CONTROL },
  { "default"            , HI_CONTROL },
  { "do"                 , HI_CONTROL },
  { "double"             , HI_VARTYPE },
  { "Double"             , HI_VARTYPE },
  { "else"               , HI_CONTROL },
  { "enum"               , HI_VARTYPE },
  { "extends"            , HI_VARTYPE },
  { "final"              , HI_VARTYPE },
  { "float"              , HI_VARTYPE },
  { "Float"              , HI_VARTYPE },
  { "finally"            , HI_CONTROL },
  { "for"                , HI_CONTROL },
  { "goto"               , HI_CONTROL },
  { "if"                 , HI_CONTROL },
  { "implements"         , HI_VARTYPE },
  { "import"             , HI_DEFINE  },
  { "instanceof"         , HI_VARTYPE },
  { "int"                , HI_VARTYPE },
  { "Integer"            , HI_VARTYPE },
  { "interface"          , HI_VARTYPE },
  { "Iterator"           , HI_VARTYPE },
  { "long"               , HI_VARTYPE },
  { "Long"               , HI_VARTYPE },
  { "main"               , HI_DEFINE  },
  { "native"             , HI_VARTYPE },
  { "new"                , HI_VARTYPE },
  { "package"            , HI_DEFINE  },
  { "private"            , HI_CONTROL },
  { "protected"          , HI_CONTROL },
  { "public"             , HI_CONTROL },
  { "return"             , HI_CONTROL },
  { "short"              , HI_VARTYPE },
  { "Short"              , HI_VARTYPE },
  { "static"             , HI_VARTYPE },
  { "strictfp"           , HI_VARTYPE },
  { "String"             , HI_VARTYPE },
  { "System"             , HI_DEFINE  },
  { "super"              , HI_VARTYPE },
  { "switch"             , HI_CONTROL },
  { "synchronized"       , HI_CONTROL },
  { "this"               , HI_VARTYPE },
  { "throw"              , HI_CONTROL },
  { "throws"             , HI_CONTROL },
  { "transient"          , HI_VARTYPE },
  { "try"                , HI_CONTROL },
  { "void"               , HI_VARTYPE },
  { "Void"               , HI_VARTYPE },
  { "volatile"           , HI_VARTYPE },
  { "while"              , HI_CONTROL },
  { "virtual"            , HI_VARTYPE },
  { "true"               , HI_CONST   },
  { "false"              , HI_CONST   },
  { "null"               , HI_CONST   },
  { "@Deprecated"        , HI_DEFINE  },
  { "@Override"          , HI_DEFINE  },
  { "@SuppressWarnings"  , HI_DEFINE  },
  { 0 }
};

Highlight_Java::Highlight_Java( FileBuf& rfb )
  : Highlight_Code( rfb )
{
}

void Highlight_Java::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Hi_FindKey_In_Range( HiPairs, st, fn );
}

