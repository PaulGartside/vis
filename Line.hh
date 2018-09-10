////////////////////////////////////////////////////////////////////////////////
// VI-Simplified (vis) C++ Implementation                                     //
// Copyright (c) 27 Dec 2016 Paul J. Gartside                                 //
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
// Line's are arrays of uint8_t's that are NULL terminated
// so they can be used as C-strings.

#ifndef __LINE_HH__
#define __LINE_HH__

typedef unsigned char  uint8_t;

class String;

class Line
{
public:
  Line();
  Line( const unsigned cap );
  Line( const unsigned len, const uint8_t fill );
  Line( const Line& a );

  ~Line();

  void clear();

  unsigned len() const;
  unsigned cap() const;

  bool set_len( unsigned new_len );

  bool inc_cap( unsigned new_cap );
  bool copy( const Line& a );

  bool operator==( const Line& a ) const;

  uint8_t get( const unsigned p ) const;
  void    set( const unsigned p, const uint8_t C );

  const char* c_str( unsigned p ) const;
  const String& toString() const;

  bool insert( const unsigned p, const uint8_t C );

  bool push( uint8_t C );

  bool remove( const unsigned p );
  bool remove( const unsigned p, uint8_t& C );

  bool pop( uint8_t& C );
  bool pop();

  bool append( const Line& a );

  int  compareTo( const Line& a ) const;
  bool gt( const Line& a ) const;
  bool lt( const Line& a ) const;
  bool eq( const Line& a ) const;

  bool ends_with( const uint8_t C );

  unsigned chksum() const;

  struct Data;

private:
  Data& m;
};

#endif

