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
#include "Highlight_Swift.hh"

static HiKeyVal HiPairs[] =
{
  // Keywords used in declarations:
  { "class"            , HI_VARTYPE },
  { "deinit"           , HI_VARTYPE },
  { "enum"             , HI_VARTYPE },
  { "extension"        , HI_VARTYPE },
  { "func"             , HI_VARTYPE },
  { "import"           , HI_VARTYPE },
  { "init"             , HI_VARTYPE },
  { "let"              , HI_VARTYPE },
  { "protocol"         , HI_VARTYPE },
  { "static"           , HI_VARTYPE },
  { "struct"           , HI_VARTYPE },
  { "subscript"        , HI_VARTYPE },
  { "typealias"        , HI_VARTYPE },
  { "var"              , HI_VARTYPE },

  { "get"              , HI_VARTYPE },
  { "set"              , HI_VARTYPE },
  { "willSet"          , HI_VARTYPE },
  { "didSet"           , HI_VARTYPE },

  // Declarations
  { "mutating"         , HI_VARTYPE },
  { "nonmutating"      , HI_VARTYPE },
  { "override"         , HI_VARTYPE },
  { "static"           , HI_VARTYPE },
  { "unowned"          , HI_VARTYPE },
  { "unowned(safe)"    , HI_VARTYPE },
  { "unowned(unsafe)"  , HI_VARTYPE },
  { "weak"             , HI_VARTYPE },

  { "Int"              , HI_VARTYPE },
  { "Int8"             , HI_VARTYPE },
  { "Int16"            , HI_VARTYPE },
  { "Int32"            , HI_VARTYPE },
  { "Int64"            , HI_VARTYPE },
  { "UInt"             , HI_VARTYPE },
  { "UInt8"            , HI_VARTYPE },
  { "UInt16"           , HI_VARTYPE },
  { "UInt32"           , HI_VARTYPE },
  { "UInt64"           , HI_VARTYPE },
  { "Float"            , HI_VARTYPE },
  { "Double"           , HI_VARTYPE },
  { "String"           , HI_VARTYPE },

  // Keywords used in statements:
  { "break"            , HI_CONTROL },
  { "case"             , HI_CONTROL },
  { "continue"         , HI_CONTROL },
  { "default"          , HI_CONTROL },
  { "do"               , HI_CONTROL },
  { "else"             , HI_CONTROL },
  { "fallthrough"      , HI_CONTROL },
  { "if"               , HI_CONTROL },
  { "in"               , HI_CONTROL },
  { "for"              , HI_CONTROL },
  { "return"           , HI_CONTROL },
  { "switch"           , HI_CONTROL },
  { "where"            , HI_CONTROL },
  { "while"            , HI_CONTROL },

  // Keywords used in expressions and types:
  { "as"               , HI_CONTROL },
  { "dynamicType"      , HI_CONTROL },
  { "is"               , HI_CONTROL },
  { "new"              , HI_CONTROL },
  { "super"            , HI_CONTROL },
  { "self"             , HI_CONTROL },
  { "Self"             , HI_CONTROL },
  { "Type"             , HI_CONTROL },

  { "import"           , HI_DEFINE  },
  { "__FUNCTION__"     , HI_DEFINE  },
  { "__COLUMN__"       , HI_DEFINE  },
  { "__FILE__"         , HI_DEFINE  },
  { "__LINE__"         , HI_DEFINE  },

  { "true"             , HI_CONST   },
  { "false"            , HI_CONST   },
  { "nil"              , HI_CONST   },
  { 0 }
};

Highlight_Swift::Highlight_Swift( FileBuf& rfb )
  : Highlight_Code( rfb )
{
}

void Highlight_Swift::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Hi_FindKey_In_Range( HiPairs, st, fn );
}

