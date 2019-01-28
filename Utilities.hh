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

#ifndef __UTILITIES_HH__
#define __UTILITIES_HH__

#include <stdio.h> // printf, stderr, FILE, fopen, fclose
#include <signal.h>
#include "Types.hh"

class FileBuf;
class String;

// The built in cast functions are too long, so create shorter versions:
template <class ToType, class FromType>
ToType SCast( FromType from )
{
  return static_cast<ToType>( from );
}
template <class ToType, class FromType>
ToType CCast( FromType from )
{
  return const_cast<ToType>( from );
}
template <class ToType, class FromType>
ToType DCast( FromType from )
{
  return dynamic_cast<ToType>( from );
}
template <class ToType, class FromType>
ToType RCast( FromType from )
{
  return reinterpret_cast<ToType>( from );
}

typedef bool (*IsWord_Func)( const int );

void DIE( const char* msg );
void DBG( const int line, const char* msg=0, ... );
void ASSERT( const int line, bool condition, const char* msg, ... );
unsigned Min( const unsigned a, const unsigned b );
unsigned Max( const unsigned a, const unsigned b );
unsigned LLM1( const unsigned LL );
void Swap( unsigned& A, unsigned& B );
void Safe_Strcpy( char* dst, const char* src, const size_t dst_size );
void RemoveSpaces( char* cp );
void Shift( char* cp, const unsigned SHIFT_LEN );
int  my_stat( const char* fname, struct stat& sbuf );
bool FileExists( const char* fname );
size_t FileSize( const char* fname );
bool IsReg( const char* fname );
bool IsDir( const char* fname );
double ModificationTime( const char* fname );
void Normalize_Full_Path( const char* path );
bool FindFullFileNameRel2CWD( String& in_out_fname );
bool FindFullFileNameRel2( const char* rel_2_path, String& in_out_fname );
void GetFnameHeadAndTail( const char* in_fname, String& head, String& tail );
void GetFnameHeadAndTail( const String& in_fname, String& head, String& tail );
String GetFnameHead( const char* in_full_fname );
String GetFnameTail( const char* in_full_fname );
const char* DirDelimStr();
void ReplaceEnvVars( String& in_out_fname );
void EnvKeys2Vals( String& in_out_fname );
#ifndef WIN32
FILE* POpenRead( const char* cmd, pid_t& child_pid );
int   PClose( FILE* fp, const pid_t child_pid );
void  ExecShell( const char* cmd );
#endif
double GetTimeSeconds();

bool IsWord_Ident( const int C );
bool IsWord_NonIdent( const int C );
bool IsSpace( const int C );
bool NotSpace( const int C );
bool IsIdent( const int C );
bool IsXML_Ident( const char C );
bool IsFileNameChar( const int C );
bool IsEndOfLineDelim( const int C );

char MS_Hex_Digit( const char Ci );
char LS_Hex_Digit( const char Ci );
bool IsHexDigit( const char C );
int  Hex_Char_2_Int_Val( const char C );
char Hex_Chars_2_Byte( const char C1, const char C2 );

bool line_start_or_prev_C_non_ident( const Line& line
                                   , const unsigned p );
bool line_end_or_non_ident( const Line& line
                          , const unsigned LL
                          , const unsigned p );

bool Files_Are_Same( const char* fname_s, const char* fname_l );
bool Files_Are_Same( const FileBuf& fb_s, const FileBuf& fb_l );
bool Line_Has_Regex( const Line& line, const String& regex );

class Trace
{
public:
  Trace( const char* func_name );
  ~Trace();

  static void Allocate();
  static void Cleanup();

  static void Print();
private:
  static ConstCharList* mp_Call_Stack;
};

#endif

