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
#include "Highlight_IDL.hh"

static HiKeyVal HiPairs[] =
{
  { "abstract"   , HI_VARTYPE },
  { "any"        , HI_VARTYPE },
  { "attribute"  , HI_VARTYPE },
  { "boolean"    , HI_VARTYPE },
  { "case"       , HI_CONTROL },
  { "char"       , HI_VARTYPE },
  { "component"  , HI_CONTROL },
  { "const"      , HI_VARTYPE },
  { "consumes"   , HI_CONTROL },
  { "context"    , HI_CONTROL },
  { "custom"     , HI_CONTROL },
  { "default"    , HI_CONTROL },
  { "double"     , HI_VARTYPE },
  { "exception"  , HI_CONTROL },
  { "emits"      , HI_CONTROL },
  { "enum"       , HI_VARTYPE },
  { "eventtype"  , HI_CONTROL },
  { "factory"    , HI_CONTROL },
  { "FALSE"      , HI_CONST   },
  { "finder"     , HI_CONTROL },
  { "fixed"      , HI_CONTROL },
  { "float"      , HI_CONTROL },
  { "getraises"  , HI_CONTROL },
  { "home"       , HI_CONTROL },
  { "import"     , HI_DEFINE  },
  { "in"         , HI_CONTROL },
  { "inout"      , HI_CONTROL },
  { "interface"  , HI_VARTYPE },
  { "local"      , HI_VARTYPE },
  { "long"       , HI_VARTYPE },
  { "module"     , HI_VARTYPE },
  { "multiple"   , HI_CONTROL },
  { "native"     , HI_VARTYPE },
  { "Object"     , HI_VARTYPE },
  { "octet"      , HI_VARTYPE },
  { "oneway"     , HI_CONTROL },
  { "out"        , HI_CONTROL },
  { "primarykey" , HI_VARTYPE },
  { "private"    , HI_CONTROL },
  { "provides"   , HI_CONTROL },
  { "public"     , HI_CONTROL },
  { "publishes"  , HI_CONTROL },
  { "raises"     , HI_CONTROL },
  { "readonly"   , HI_VARTYPE },
  { "setraises"  , HI_CONTROL },
  { "sequence"   , HI_CONTROL },
  { "short"      , HI_VARTYPE },
  { "string"     , HI_VARTYPE },
  { "struct"     , HI_VARTYPE },
  { "supports"   , HI_CONTROL },
  { "switch"     , HI_CONTROL },
  { "TRUE"       , HI_CONST   },
  { "truncatable", HI_CONTROL },
  { "typedef"    , HI_VARTYPE },
  { "typeid"     , HI_VARTYPE },
  { "typeprefix" , HI_VARTYPE },
  { "unsigned"   , HI_VARTYPE },
  { "union"      , HI_VARTYPE },
  { "uses"       , HI_CONTROL },
  { "ValueBase"  , HI_VARTYPE },
  { "valuetype"  , HI_VARTYPE },
  { "void"       , HI_VARTYPE },
  { "wchar"      , HI_VARTYPE },
  { "wstring"    , HI_VARTYPE },
  { 0 }
};

Highlight_IDL::Highlight_IDL( FileBuf& rfb )
  : Highlight_Code( rfb )
{
}

void Highlight_IDL::Find_Styles_Keys()
{
  Trace trace( __PRETTY_FUNCTION__ );

  Hi_FindKey( HiPairs );
}

void Highlight_IDL::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Trace trace( __PRETTY_FUNCTION__ );

  Hi_FindKey_In_Range( HiPairs, st, fn );
}

