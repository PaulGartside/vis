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
  String();
  String( const char* cp  );
  String( const String& a );
  ~String();

  unsigned len() const;
  unsigned cap() const;
  void     clear();

  const char* c_str() const;

  String& operator=( const char* cp );
  String& operator=( const String& a );

  bool  operator==( const char*  cp ) const;
  bool  operator==( const String& a ) const;
  bool  operator!=( const String& a ) const;

  char  get( const unsigned p ) const;
  bool  set( const unsigned p, char C );
  char  get_end( const unsigned p=0 ) const;

  bool    append( const char* cp );
  bool    append( const String& a );
  String& operator+=( const char*  cp );
  String& operator+=( const String& a );

  bool inc_cap( unsigned new_cap );
  bool push( const char C );
  bool insert( const unsigned p, const char C );
  bool insert( const unsigned p, const char* cp );
  char remove( const unsigned p );
  char pop();

  // replace the first occurance of s1 with s2,
  // returning 1 on success and 0 on failure
  bool replace( const String& s1, const String& s2 );

  // If this string has pat at pos, returns 1, else returns 0
  bool has_at( const char* pat, unsigned pos ) const;
  bool ends_with( const char* pat ) const;
  bool split( const char* delim, String& part );

  unsigned esc_len();
  unsigned trim    ( const char* trim_chars=" \t\r\n" );
  unsigned trim_end( const char* trim_chars=" \t\r\n" );
  unsigned trim_beg( const char* trim_chars=" \t\r\n" );

  int  compareTo( const String& a ) const;
  int  compareToIgnoreCase( const String& a ) const;
  bool gt( const String& a ) const;
  bool lt( const String& a ) const;
  bool eq( const String& a ) const;

  struct Data;

private:
  Data& m;
};

#endif // __STRING__HH__

