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
#include "Highlight_TCL.hh"

static HiKeyVal HiPairs[] =
{
  { "tell"           , HI_CONTROL },
  { "socket"         , HI_CONTROL },
  { "subst"          , HI_CONTROL },
  { "open"           , HI_CONTROL },
  { "eof"            , HI_CONTROL },
  { "pwd"            , HI_CONTROL },
  { "glob"           , HI_CONTROL },
  { "list"           , HI_VARTYPE },
  { "pid"            , HI_VARTYPE },
  { "exec"           , HI_CONTROL },
  { "auto_load_index", HI_CONTROL },
  { "time"           , HI_CONTROL },
  { "unknown"        , HI_VARTYPE },
  { "eval"           , HI_CONTROL },
  { "lassign"        , HI_CONTROL },
  { "lrange"         , HI_CONTROL },
  { "fblocked"       , HI_CONTROL },
  { "lsearch"        , HI_CONTROL },
  { "auto_import"    , HI_CONTROL },
  { "gets"           , HI_CONTROL },
  { "case"           , HI_CONTROL },
  { "lappend"        , HI_CONTROL },
  { "proc"           , HI_CONTROL },
  { "break"          , HI_CONTROL },
  { "variable"       , HI_VARTYPE },
  { "llength"        , HI_CONTROL },
  { "auto_execok"    , HI_CONTROL },
  { "return"         , HI_CONTROL },
  { "linsert"        , HI_CONTROL },
  { "error"          , HI_CONTROL },
  { "catch"          , HI_CONTROL },
  { "clock"          , HI_VARTYPE },
  { "info"           , HI_CONTROL },
  { "split"          , HI_CONTROL },
  { "array"          , HI_VARTYPE },
  { "if"             , HI_CONTROL },
  { "else"           , HI_CONTROL },
  { "fconfigure"     , HI_CONTROL },
  { "concat"         , HI_CONTROL },
  { "join"           , HI_CONTROL },
  { "lreplace"       , HI_CONTROL },
  { "source"         , HI_CONTROL },
  { "fcopy"          , HI_CONTROL },
  { "global"         , HI_VARTYPE },
  { "switch"         , HI_CONTROL },
  { "auto_qualify"   , HI_CONTROL },
  { "update"         , HI_CONTROL },
  { "close"          , HI_CONTROL },
  { "cd"             , HI_CONTROL },
  { "for"            , HI_CONTROL },
  { "auto_load"      , HI_CONTROL },
  { "file"           , HI_VARTYPE },
  { "append"         , HI_CONTROL },
  { "lreverse"       , HI_CONTROL },
  { "format"         , HI_CONTROL },
  { "unload"         , HI_CONTROL },
  { "read"           , HI_CONTROL },
  { "package"        , HI_VARTYPE },
  { "set"            , HI_CONTROL },
  { "binary"         , HI_CONTROL },
  { "namespace"      , HI_VARTYPE },
  { "scan"           , HI_CONTROL },
  { "trace"          , HI_CONTROL },
  { "seek"           , HI_CONTROL },
  { "while"          , HI_CONTROL },
  { "chan"           , HI_CONTROL },
  { "flush"          , HI_CONTROL },
  { "after"          , HI_CONTROL },
  { "vwait"          , HI_CONTROL },
  { "dict"           , HI_VARTYPE },
  { "continue"       , HI_CONTROL },
  { "uplevel"        , HI_CONTROL },
  { "foreach"        , HI_CONTROL },
  { "lset"           , HI_CONTROL },
  { "rename"         , HI_CONTROL },
  { "fileevent"      , HI_CONTROL },
  { "regexp"         , HI_CONTROL },
  { "lrepeat"        , HI_CONTROL },
  { "upvar"          , HI_CONTROL },
  { "encoding"       , HI_CONTROL },
  { "expr"           , HI_CONTROL },
  { "unset"          , HI_CONTROL },
  { "load"           , HI_CONTROL },
  { "regsub"         , HI_CONTROL },
  { "history"        , HI_CONTROL },
  { "interp"         , HI_CONTROL },
  { "exit"           , HI_CONTROL },
  { "puts"           , HI_CONTROL },
  { "incr"           , HI_CONTROL },
  { "lindex"         , HI_CONTROL },
  { "lsort"          , HI_CONTROL },
  { "tclLog"         , HI_CONTROL },
  { "string"         , HI_VARTYPE },
  { 0 }
};

Highlight_TCL::Highlight_TCL( FileBuf& rfb )
  : Highlight_Code( rfb )
{
}

void Highlight_TCL::
     Find_Styles_Keys_In_Range( const CrsPos   st
                              , const unsigned fn )
{
  Hi_FindKey_In_Range( HiPairs, st, fn );
}

