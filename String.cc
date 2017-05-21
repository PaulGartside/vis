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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "String.hh"
#include "MemCheck.hh"

using std::string;

struct String::Data
{
  Data();
  Data( const char* cp );
  Data( const String& a );

  string s;
};

String::Data::Data()
  : s()
{
}

String::Data::Data( const char* cp )
  : s( cp )
{
}

String::Data::Data( const String& a )
  : s( a.m.s )
{
}

String::String()
  : m( *new(__FILE__,__LINE__) Data() )
{}

String::String( const char* cp  )
  : m( *new(__FILE__,__LINE__) Data( cp ) )
{
}

String::String( const String& a )
  : m( *new(__FILE__,__LINE__) Data( a ) )
{
}

String::~String()
{
  MemMark(__FILE__,__LINE__); delete &m;
}

unsigned String::len() const { return m.s.length(); }
unsigned String::cap() const { return m.s.capacity(); }
void     String::clear() { m.s.clear(); }

const char* String::c_str() const
{
  return m.s.c_str();
}

String& String::operator=( const char* cp )
{
  m.s = cp;

  return *this;
}

String& String::operator=( const String& a )
{
  if( this != &a )
  {
    m.s = a.m.s;
  }
  return *this;
}

bool String::operator==( const char* cp ) const
{
  return m.s == cp;
}

bool String::operator==( const String& a ) const
{
  return m.s == a.m.s;
}

bool String::operator!=( const String& a ) const
{
  return m.s != a.m.s;
}

char String::get( const unsigned i ) const
{
  if( i<m.s.length() ) return m.s[i];

  return 0;
}

bool String::set( unsigned i, char C )
{
  if( i<m.s.length() )
  {
    m.s[i] = C;

    return true;
  }
  else if( i==m.s.length() )
  {
    return push( C );
  }
  return false;
}

char String::get_end( unsigned i ) const
{
  const int index = m.s.length()-1-i;

  if( 0 <= index && index < m.s.length() ) return m.s[index];

  return 0;
}

bool String::append( const char* cp )
{
  if( cp )
  {
    m.s.append( cp );

    return true;
  }
  return false;
}

bool String::append( const String& a )
{
  if( this == &a ) return false;

  m.s.append( a.m.s );

  return true;
}

String& String::operator+=( const char* cp )
{
  append( cp );

  return *this;
}

String& String::operator+=( const String& a )
{
  append( a.c_str() );

  return *this;
}

bool String::inc_cap( const unsigned new_cap )
{
  bool ok = true;

  if( m.s.capacity() < new_cap )
  {
    m.s.reserve( new_cap );
  }
  return ok;
}

bool String::push( const char C )
{
  m.s.append( 1, C );

  return true;
}

bool String::insert( const unsigned p, const char C )
{
  if( p<=m.s.length() )
  {
    m.s.insert( p, 1, C );
    return true;
  }
  return false;
}

bool String::insert( const unsigned p, const char* cp )
{
  if( p<=m.s.length() )
  {
    m.s.insert( p, cp );
    return true;
  }
  return false;
}

// Returns char removed from p
char String::remove( const unsigned p )
{
  char C = 0;

  if( p<m.s.length() )
  {
    C = m.s[p];

    m.s.erase( m.s.begin() + p );
  }
  return C;
}

char String::pop()
{
  if( 0<m.s.length() )
  {
    return remove( m.s.length()-1 );
  }
  return 0;
}

// replace the first occurance of s1 with s2,
// returning true on success and false on failure
bool String::replace( const String& s1, const String& s2 )
{
  if( 0 < m.s.length() && 0 < s1.len() )
  {
    // If s1 has zero length, or is not in s, there is nothing to replace.
    // When s2 is a null string, s1 is simply removed from s.
    string::size_type start = m.s.find(s1.m.s);

    if( string::npos != start )
    {
      m.s.replace( start, s1.len(), s2.m.s );

      return true;
    }
  }
  return false;
}

// If this string has pat at pos, returns 1, else returns 0
bool String::has_at( const char* pat, unsigned pos )
{
  if( pat && pat[0] )
  {
    const unsigned pat_len = strlen( pat );

    if( pos+pat_len <= m.s.length() )
    {
      return 0==m.s.compare( pos, pat_len, pat );
    }
  }
  return false;
}

bool String::ends_with( const char* pat )
{
  if( pat && pat[0] )
  {
    const unsigned pat_len = strlen( pat );

    if( pat_len <= m.s.length() )
    {
      return 0==m.s.compare( m.s.length()-pat_len, pat_len, pat );
    }
  }
  return false;
}

// Returns print length of string with the escape sequences
//   not counted in the length
unsigned String::esc_len()
{
  const char escape = 27;

  unsigned escape_len = 0;
  for( unsigned k=0; k<m.s.length(); k++ )
  {
    char C = m.s[k];
    if( C != escape ) escape_len++;
    else {
      // Search for end of escape sequence
      for( k++; k<m.s.length(); k++ )
      {
        char c2 = m.s[k];
        if( (('A' <= c2) && (c2 <= 'Z'))
         || (('a' <= c2) && (c2 <= 'z')) )
        {
          // Found end of escape sequence
          break;
        }
      }
    }
  }
  return escape_len;
}

unsigned String::trim( const char* trim_chars )
{
  return trim_end( trim_chars ) + trim_beg( trim_chars );
}

unsigned String::trim_end( const char* trim_chars )
{
  unsigned num_trims = 0;
  bool done = false;
  while( !done && 0<m.s.length() )
  {
    const unsigned back_idx = m.s.length()-1;
  //int C = m.s.back();
    int C = m.s[ back_idx ];
    if( strchr( trim_chars, C ) )
    {
    //m.s.pop_back();
      m.s.erase( m.s.begin() + back_idx );
      num_trims += 1;
    }
    else done = true;
  }
  return num_trims;
}

unsigned String::trim_beg( const char* trim_chars )
{
  unsigned num_trims = 0;

  bool done = false;
  while( !done && 0<m.s.length() )
  {
  //const int C = m.s.front();
    const int C = m.s[ 0 ];
    if( strchr( trim_chars, C ) )
    {
      m.s.erase( 0, 1 );
      num_trims += 1;
    }
    else done = true;
  }
  return num_trims;
}

// Splits chars off from beginning of string up to delim into part.
// Chars put into part and delim are removed from string.
// Returns true if split was considered successful, else false.
// If delim is at very beginning, part returns empty,
//   delim is removed, and return value is true.
// If delim is not found, entire string is copied into part,
//   string becomes empty, and return value is true.
bool String::split( const char* delim, String& part )
{
  if( 0<m.s.length() && 0!=delim )
  {
    string::size_type location = m.s.find( delim );

    if( string::npos == location )
    {
      // delim not found, split off whole string:
      part = m.s.c_str();

      m.s.clear();
    }
    else {
      // delim found, split off from beginning up to delim:
      part.clear();
      // Make sure part has enough space for new data:
      if( part.cap() < location ) part.inc_cap( location );

      for( string::size_type k=0; k<location; k++ )
      {
        part.push( m.s[k] );
      }
      // Remove part put into part and delim:
      m.s.erase( 0, location + strlen( delim ) );
    }
    return true;
  }
  return false;
}

