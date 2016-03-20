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

// This string is designed for simplicity and to minimize allocation of data.
// Each string has its own copy of data. * The size increases as needed,
// but does not decrease until the destructor is called. * When the default
// constructor is used, the size and length are initially zero.
// See the .cc file for explanations of individual functions.

#ifndef __STRING__HH__
#define __STRING__HH__

#include <stdlib.h>

class String
{
public:
  String()                  : data(0), size(0), length(0) {}
  String( const char* cp  ) : data(0), size(0), length(0) { copy( cp ); }
  String( const String& s ) : data(0), size(0), length(0) { copy( s  ); }
  ~String() { destruct(); }

  unsigned sz () const { return size; }
  unsigned len() const { return length; }
  void   clear();

  const char* c_str() const { return data ? data : ""; }

  String& operator=( const char* cp ) {
    copy( cp );
    return *this;
  }
  String& operator=( const String& s ) {
    if( this != &s ) copy( s );
    return *this;
  }
  bool  operator==( const char*  cp ) const;
  bool  operator==( const String& s ) const;
  bool  operator!=( const String& s ) const;

  char  get( unsigned i ) const;
  bool  set( unsigned i, char c );
  char  get_end( unsigned i=0 ) const;

  bool    append( const char* cp );
  bool    append( const String& s );
  String& operator+=( const char*  cp ) { append(cp);        return *this; }
  String& operator+=( const String& s ) { append(s.c_str()); return *this; }

  bool push( char c );
  bool insert( unsigned p, char c );
  bool insert( unsigned p, const char* cp );
  char pop();
  char pop_at( unsigned index=0 );
  int  shift( unsigned num_chars );

  // replace the first occurance of s1 with s2,
  // returning 1 on success and 0 on failure
  bool replace( const String& s1, const String& s2 );

  // Returns the number of chars changed from lower case to upper case
  int to_upper();
  // Returns the number of chars changed from upper case to lower case
  int to_lower();

  // If this string has pat at or after pos, returns 1, else returns 0
  // If pos is non-zero:
  //   If returning 1 *pos is set to index of pat
  //   If returning 0 pos is un-changed
  bool has( const char* pat, unsigned* pos=0 );

  // If this string has pat at pos, returns 1, else returns 0
  bool has_at( const char* pat, unsigned pos );

  int esc_len();
  int chomp();
  int trim    ( const char* trim_chars=" \t\r\n" );
  int trim_end( const char* trim_chars=" \t\r\n" );
  int trim_beg( const char* trim_chars=" \t\r\n" );
  int trim_inv    ( const char* trim_chars );
  int trim_inv_end( const char* trim_chars );
  int trim_inv_beg( const char* trim_chars );
  bool split( const char* delim, String& part );

private:
  char*  data  ;
  unsigned size  ;  // size of buffer pointed to by data
  unsigned length;  // length of data

  void destruct();
  bool inc_size( unsigned new_size, bool keep_data=false );

  bool copy( const char*   cp );
  bool copy( const String& s ) { return copy( s.c_str() ); }
};

#endif /* !__STRING__HH__ */

