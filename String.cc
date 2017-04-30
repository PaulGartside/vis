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

#include "String.hh"
#include "MemCheck.hh"

void String::destruct()
{
  if( size )
  {
    MemMark(__FILE__,__LINE__);
    delete[] data;
  }
  data   = 0;
  size   = 0;
  length = 0;
}

void String::clear()
{
  if( length )
  {
    data[0] = 0;
    length  = 0;
  }
}

// inc_size() does not assume that data points to anything because
// initially data does not poing to anything. * However, when
// inc_size() returns successfully data does point to something.
// returns 1 on success and 0 failure
bool String::inc_size( unsigned new_size, bool keep_data )
{
  if( new_size > size )
  {
    unsigned my_new_size = int( size*1.125+8 ) ;
    if( new_size < my_new_size ) new_size = my_new_size ;

    char* new_data = new(__FILE__,__LINE__) char[new_size];
    if( !new_data ) return false;

    if( !keep_data ) new_data[0] = 0;
    else {
      for( unsigned k=0; k<length; k++ ) new_data[k] = data[k];
      new_data[length] = 0;
    }
    MemMark(__FILE__,__LINE__);
    if( size ) delete[] data;
    data = new_data;
    size = new_size;
  }
  return true;
}

// copy() does no assume that data points to something because
// copy() is called from a constructor. * However, when
// copy() returns data does point to something.
bool String::copy( const char* cp )
{
  unsigned s_len = cp ? strlen(cp) : 0 ;

  if( s_len ) {
    if( size < s_len+1 ) { if( !inc_size( s_len+1 ) ) return false; }
    for( unsigned k=0; k<s_len; k++ ) data[k] = cp[k];
  }
  length = s_len ;
  if( size ) data[ length ] = 0;

  return true;
}

bool String::operator==( const char* cp ) const
{
  // NULL pointer cannot equal anything
  if( cp )
  {
    const unsigned cp_len = strlen( cp );

    if( cp_len == length )
    {
      // Two zero length strings are considered equal
      if( 0<length )
      {
        return strncmp( data, cp, length )==0;
      }
      return true;
    }
  }
  return false;
}

bool String::operator==( const String& s ) const
{
  if( s.len() == length )
  {
    // Two zero length strings are considered equal
    if( 0<length )
    {
      return strncmp( data, s.c_str(), length )==0;
    }
    return true;
  }
  return false;
}

bool String::operator!=( const String& s ) const
{
  return !operator==( s );
}

char String::get( unsigned i ) const
{
  if( i<length ) return data[i];
  return false;
}

bool String::set( unsigned i, char c )
{
  if( i<length )
  {
    if( c==0 ) length = i ;
    data[i] = c ;
    return true;
  }
  else if( i==length )
  {
    return push( c );
  }
  return false;
}

char String::get_end( unsigned i ) const
{
  const int index = length-1-i;
  if( 0 <= index && index < length ) return data[index];
  return 0;
}

bool String::append( const char* cp )
{
  if( !cp ) return false;

  unsigned cp_len = strlen(cp);
  if( cp_len ) {
    unsigned new_size = length + cp_len + 1 ;
    // if inc_size() fails, this does not change
    if( size < new_size && !inc_size( new_size, true ) ) return false;
    for( unsigned k=0; k<cp_len; k++ ) data[length+k] = cp[k] ;
    length += cp_len ;
    data[length] = 0 ;
  }
  return true;
}

bool String::append( const String& s )
{
  unsigned s_len = s.len();
  if( s_len ) {
    unsigned new_size = length + s_len + 1 ;
    // if inc_size() fails, this does not change
    if( size < new_size && !inc_size( new_size, true ) ) return false;
    for( unsigned k=0; k<s_len; k++ ) data[length+k] = s.get(k);
    length += s_len ;
    data[length] = 0 ;
  }
  return true;
}

bool String::push( char c )
{
  if( !c ) return false;

  // size must be at least current length + 1 for c + 1 for NULL
  if( size < length+2 && !inc_size( size+1, true ) ) return false;

  data[length++] = c;
  data[length  ] = 0;

  return true;
}

bool String::insert( unsigned p, char c )
{
  if( !c || length < p ) return false;

  if( size <= length+1 && !inc_size( size+1, true ) ) return false;

  // Shift up:
  for( unsigned k=length; p<k; k-- )
  {
    data[k] = data[k-1];
  }
  data[p] = c;
  length++;
  data[length] = 0; // Add terminating NULL
  return true;
}

bool String::insert( unsigned p, const char* cp )
{
  const unsigned cp_len = strlen( cp );

  if( !cp || !cp_len || length < p ) return false;

  if( size <= length+cp_len && !inc_size( size+cp_len, true ) ) return false;

  // Shift up:
  for( unsigned k=length-1+cp_len; p+cp_len<=k; k-- )
  {
    data[k] = data[k-cp_len];
  }
  // Insert cp:
  for( unsigned k=0; k<cp_len; k++ )
  {
    data[p+k] = cp[k];
  }
  length+=cp_len;
  data[length] = 0; // Add terminating NULL
  return true;
}

char String::pop()
{
  if( length ) {
    char c = data[length-1];
    data[--length] = 0;
    return c;
  }
  return 0;
}
// Returns char pop-ed from index
char String::pop_at( unsigned index )
{
  if( index>=length ) return 0;

  char c = data[index];

  // Shift down, including terminating NULL:
  for( unsigned k=index; k<length; k+=1 )
  {
    // k+1 ensures terminating NULL gets copied
    data[k] = data[k+1];
  }
  length--;
  return c;
}

// Shifts the string down by num_chars.
// Returns number of chars shifted out of the string.
int String::shift( unsigned num_chars )
{
  if( num_chars==0 ) return 0;

  if( length<=num_chars )
  {
    data[0]   = 0;
    length    = 0;
    num_chars = length;
  }
  else {
    // k<=length ensures terminating NULL gets copied
    for( unsigned k=num_chars; k<=length; k+=1 )
    {
      data[k-num_chars] = data[k];
    }
    length -= num_chars;
  }
  return num_chars;
}

// replace the first occurance of s1 with s2,
// returning 1 on success and 0 on failure
bool String::replace( const String& s1, const String& s2 )
{
  if( length==0 ) return false;

  const char* str1 = s1.c_str(); int str1_len = s1.len();
  const char* str2 = s2.c_str(); int str2_len = s2.len();

  // If s1 has zero length, or is not in data, there is nothing to replace.
  // When s2 is a null string s1 is simply removed from data.
  if( str1_len==0 || strstr(data, str1)==0 ) return false;

  int S1L = strstr( data, str1 ) - data; // Segment 1 length
  int S2L = str2_len;                    // Segment 2 length
  int S3L = length - S1L - str1_len;     // Segment 3 length

  if( str2_len <= str1_len )
  {
    for( int k=0; k<S2L; k++ ) data[ k+S1L ] = str2[ k ];
    if( str2_len < str1_len ) {
      // Move segment 3 down
      for( int k=0; k<S3L; k++ ) data[ k+S1L+S2L ] = data[ k+S1L+str1_len ];
    }
  }
  else /* str2_len > str1_len */
  {
    if( int(size) < S1L+S2L+S3L+1 ) {
      bool keep_data = true;
      if( !inc_size( S1L+S2L+S3L+1, keep_data ) ) return false;
    }
    // Move segment 3 up to make room for a larger segment 2
    for( int k=S3L-1; k>=0; k-- ) data[ k+S1L+S2L ] = data[ k+S1L+str1_len ];
    for( int k=0;    k<S2L; k++ ) data[ k+S1L ]     = str2[ k ];
  }
  length = S1L+S2L+S3L ;
  data[ length ] = 0 ;

  return true;
}

// Returns the number of chars changed from lower case to upper case
int String::to_upper()
{
  int changed = 0;
  for( unsigned k=0; k<length; k++ )
  {
    int c = data[k];
    int c2 = toupper( c );
    if( c != c2 )
    {
      data[k] = c2;
      changed ++ ;
    }
  }
  return changed;
}

// Returns the number of chars changed from upper case to lower case
int String::to_lower()
{
  int changed = 0;
  for( unsigned k=0; k<length; k++ )
  {
    int c = data[k];
    int c2 = tolower( c );
    if( c != c2 )
    {
      data[k] = c2;
      changed ++ ;
    }
  }
  return changed;
}

// If this string has pat at or after pos, returns 1, else returns 0
// If pos is non-zero:
//   If returning 0 pos is un-changed
//   If returning 1 *pos is set to index of pat
bool String::has( const char* pat, unsigned* pos )
{
  // No pattern, or zero length pattern or no data, or zero length data
  if( !pat || !pat[0] || !data || !data[0] ) return false;

  unsigned offset = pos ? *pos : 0;

  if( offset < length )
  {
    const char* result = strstr( data+offset, pat );
    // If pat is not in data, result is 0.
    // In that case dont change *pos
    if( pos && result ) *pos = result - data;
    return result ? 1 : 0;
  }
  return false;
}

//// If this string has pat at pos, returns 1, else returns 0
//bool String::has_at( const char* pat, unsigned pos )
//{
//  // No pattern, or zero length pattern or no data, or zero length data
//  if( !pat || !pat[0] || !data || !data[0] ) return false;
//
//  if( pos < length )
//  {
//    unsigned pat_len = strlen( pat );
//    return ! strncmp( data+pos, pat, pat_len );
//  }
//  return false;
//}

// If this string has pat at pos, returns 1, else returns 0
bool String::has_at( const char* pat, unsigned pos )
{
  // No pattern, or zero length pattern or no data, or zero length data
  if( !pat || !pat[0] || !data || !data[0] ) return false;

  unsigned pat_len = strlen( pat );

  if( pos+pat_len <= length )
  {
    return ! strncmp( data+pos, pat, pat_len );
  }
  return false;
}

bool String::ends_with( const char* pat )
{
  const unsigned pat_len = strlen( pat );

  return length < pat_len ? false : has_at( pat, length-pat_len );
}

// Returns print length of string with the escape sequences
//   not counted in the length
int String::esc_len()
{
  const char escape = 27;

  int escape_len = 0;
  for( unsigned k=0; k<length; k++ )
  {
    char c = data[k];
    if( c != escape ) escape_len++;
    else {
      // Search for end of escape sequence
      for( k++; k<length; k++ )
      {
        char c2 = data[k];
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

int String::chomp()
{
  int num_chomps = 0;
  while( data[ length-1 ] == '\n' )
  {
    data[ --length ] = 0;
    num_chomps += 1;
  }
  return num_chomps;
}

int String::trim( const char* trim_chars )
{
  int num_trims = trim_end( trim_chars );
      num_trims+= trim_beg( trim_chars );
  return num_trims;
}

int String::trim_inv( const char* trim_chars )
{
  int num_trims = trim_inv_end( trim_chars );
      num_trims+= trim_inv_beg( trim_chars );
  return num_trims;
}

int String::trim_end( const char* trim_chars )
{
  int num_trims = 0;
  bool done = false;
  for( int k=length-1; k>0 && !done; k-- )
  {
    int c = data[ k ];
    if( strchr( trim_chars, c ) )
    {
      data[ k ] = 0;
      length = k;
      num_trims += 1;
    }
    else done = true;
  }
  return num_trims;
}

int String::trim_inv_end( const char* trim_chars )
{
  int num_trims = 0;
  bool done = false;
  for( int k=length-1; k>0 && !done; k-- )
  {
    int c = data[ k ];
    if( !strchr( trim_chars, c ) )
    {
      data[ k ] = 0;
      length = k;
      num_trims += 1;
    }
    else done = true;
  }
  return num_trims;
}

int String::trim_beg( const char* trim_chars )
{
  int num_trims = 0;
  unsigned new_start = 0;
  bool done = false;
  for( unsigned k=0; k<length && !done; k++ )
  {
    char c = data[ k ];
    if( !strchr( trim_chars, c ) )
    {
      new_start = k;
      done = true;
    }
  }
  if( done == false && length )
  {
    // Whole string is getting trimmed
    num_trims = length;
    length = 0;
    data[0] = 0;
  }
  else {
    if( new_start )
    {
      for( unsigned k=new_start; k<=length; k++ )
      {
        data[ k-new_start ] = data[ k ];
      }
      num_trims += new_start;
      length    -= new_start;
    }
  }
  return num_trims;
}

int String::trim_inv_beg( const char* trim_chars )
{
  int num_trims = 0;
  unsigned new_start = 0;
  bool done = false;
  for( unsigned k=0; k<length && !done; k++ )
  {
    char c = data[ k ];
    if( strchr( trim_chars, c ) )
    {
      new_start = k;
      done = true;
    }
  }
  if( done == false && length )
  {
    // Whole string is getting trimmed
    num_trims = length;
    length = 0;
    data[0] = 0;
  }
  else {
    if( new_start )
    {
      for( unsigned k=new_start; k<=length; k++ )
      {
        data[ k-new_start ] = data[ k ];
      }
      num_trims += new_start;
      length    -= new_start;
    }
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
  if( 0<length && 0!=delim )
  {
    const char* location = strstr( data, delim );

    if( 0 == location )
    {
      // delim not found, split off whole string:
      part.copy( data );

      clear();
    }
    else {
      // delim found, split off from beginning up to delim:
      part.clear();
      // Make sure part has enough space for new data:
      part.inc_size( location-data );

      for( unsigned k=0; k<location-data; k++ )
      {
        part.push( data[k] );
      }
      // Remove part put into part and delim:
      shift( location-data + strlen( delim ) );
    }
    return true;
  }
  return false;
}

