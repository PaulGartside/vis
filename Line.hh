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

  uint8_t get( const unsigned i ) const;
  void    set( const unsigned i, const uint8_t C );

  const char* c_str( unsigned i ) const;

  bool insert( const unsigned i, const uint8_t t );

  bool push( uint8_t t );

  bool remove( const unsigned i );
  bool remove( const unsigned i, uint8_t& t );

  bool pop( uint8_t& t );
  bool pop();

  bool append( const Line& a );

  bool ends_with( const uint8_t C );

  unsigned chksum() const;

  struct Data;

private:
  Data& m;
};

#endif

