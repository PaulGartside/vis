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
#include "Highlight_JS.hh"

static HiKeyVal HiPairs[] =
{
  // Keywords:
  { "break"              , HI_CONTROL },
  { "break"              , HI_CONTROL },
  { "catch"              , HI_CONTROL },
  { "continue"           , HI_CONTROL },
  { "debugger"           , HI_CONTROL },
  { "default"            , HI_CONTROL },
  { "delete"             , HI_CONTROL },
  { "do"                 , HI_CONTROL },
  { "else"               , HI_CONTROL },
  { "finally"            , HI_CONTROL },
  { "for"                , HI_CONTROL },
  { "function"           , HI_CONTROL },
  { "if"                 , HI_CONTROL },
  { "in"                 , HI_CONTROL },
  { "instanceof"         , HI_CONTROL },
  { "new"                , HI_VARTYPE },
  { "return"             , HI_CONTROL },
  { "switch"             , HI_CONTROL },
  { "throw"              , HI_CONTROL },
  { "try"                , HI_CONTROL },
  { "typeof"             , HI_VARTYPE },
  { "var"                , HI_VARTYPE },
  { "void"               , HI_VARTYPE },
  { "while"              , HI_CONTROL },
  { "with"               , HI_CONTROL },

  // Keywords in strict mode:
  { "implements"         , HI_CONTROL },
  { "interface"          , HI_CONTROL },
  { "let"                , HI_VARTYPE },
  { "package"            , HI_DEFINE },
  { "private"            , HI_CONTROL },
  { "protected"          , HI_CONTROL },
  { "public"             , HI_CONTROL },
  { "static"             , HI_VARTYPE },
  { "yield"              , HI_CONTROL },

  // Constants:
  { "false"              , HI_CONST   },
  { "null"               , HI_CONST   },
  { "true"               , HI_CONST   },

  // Global variables and functions:
  { "arguments"          , HI_VARTYPE   },
  { "Array"              , HI_VARTYPE   },
  { "Boolean"            , HI_VARTYPE   },
  { "Date"               , HI_CONTROL   },
  { "decodeURI"          , HI_CONTROL   },
  { "decodeURIComponent" , HI_CONTROL   },
  { "encodeURI"          , HI_CONTROL   },
  { "encodeURIComponent" , HI_CONTROL   },
  { "Error"              , HI_VARTYPE   },
  { "eval"               , HI_CONTROL   },
  { "EvalError"          , HI_CONTROL   },
  { "Function"           , HI_CONTROL   },
  { "Infinity"           , HI_CONST     },
  { "isFinite"           , HI_CONTROL   },
  { "isNaN"              , HI_CONTROL   },
  { "JSON"               , HI_CONTROL   },
  { "Math"               , HI_CONTROL   },
  { "NaN"                , HI_CONST     },
  { "Number"             , HI_VARTYPE   },
  { "Object"             , HI_VARTYPE   },
  { "parseFloat"         , HI_CONTROL   },
  { "parseInt"           , HI_CONTROL   },
  { "RangeError"         , HI_VARTYPE   },
  { "ReferenceError"     , HI_VARTYPE   },
  { "RegExp"             , HI_CONTROL   },
  { "String"             , HI_VARTYPE   },
  { "SyntaxError"        , HI_VARTYPE   },
  { "TypeError"          , HI_VARTYPE   },
  { "undefined"          , HI_CONST     },
  { "URIError"           , HI_VARTYPE   },

  { 0 }
};

Highlight_JS::Highlight_JS( FileBuf& rfb )
  : Highlight_Code( rfb )
{
}

void Highlight_JS::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Hi_FindKey_In_Range( HiPairs, st, fn );
}

