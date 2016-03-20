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

class Line;
class String;

void DIE( const char* msg );
void DBG( const int line, const char* msg=0, ... );
void ASSERT( const int line, bool condition, const char* msg, ... );
unsigned Min( const unsigned a, const unsigned b );
unsigned Max( const unsigned a, const unsigned b );
void Swap( unsigned& A, unsigned& B );
void RemoveSpaces( char* cp );
int  my_stat( const char* fname, struct stat& sbuf );
bool FileExists( const char* fname );
double ModificationTime( const char* fname );
bool FindFullFileName( String& in_out_fname );
void GetFnameHeadAndTail( const String& in_fname, String& head, String& tail );
String GetFnameHead( const String& in_fname );
void ReplaceEnvVars( String& in_out_fname );
void EnvKeys2Vals( String& in_out_fname );
#ifndef WIN32
FILE* POpenRead( const char* cmd, pid_t& child_pid );
int   PClose( FILE* fp, const pid_t child_pid );
void  ExecShell( const char* cmd );
#endif

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

bool IsWord_Ident( const int C );
bool IsWord_NonIdent( const int C );
bool IsSpace( const int C );
bool NotSpace( const int C );
bool IsIdent( const int C );
bool IsFileNameChar( const int C );
bool IsEndOfLineDelim( const int C );

bool line_start_or_non_ident( const Line& line
                            , const unsigned LL
                            , const unsigned p );
bool line_end_or_non_ident( const Line& line
                          , const unsigned LL
                          , const unsigned p );

class Trace
{
public:
  Trace( const char* func_name );
  ~Trace();

  static void Print();
};

#endif

