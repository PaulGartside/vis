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

#ifndef __ARRAY_T_HH__
#define __ARRAY_T_HH__

#include "MemCheck.hh"

template <class T>
class Array_t
{
protected:
  T*       data;
  unsigned size;   // size of data buffer
  unsigned length; // # of elements in list

public:
  bool inc_size( const char* _FILE_, const unsigned _LINE_
               , unsigned new_size );
  bool set_len ( const char* _FILE_, const unsigned _LINE_
               , unsigned new_len );

  Array_t& operator=( Array_t& a )
  {
    if( this != &a ) copy( a );
    return *this;
  }
  bool copy( const Array_t& a );
  bool append( const char* _FILE_, const unsigned _LINE_
             , const Array_t& a );

  Array_t();
  Array_t( const char* _FILE_, const unsigned _LINE_, unsigned size=0 );
  Array_t( const char* _FILE_, const unsigned _LINE_, unsigned size, T fill );

  Array_t( const Array_t<T>& a );

  virtual ~Array_t();

  void Cleanup( const char* _FILE_, const unsigned _LINE_ );
  void clear() { length = 0; }

  unsigned len() const { return length; }
  unsigned sz () const { return size  ; }

  // Only use operator[] if you know 0<i && i<length
  T& operator[]( unsigned i ) const { return data[i]; }

  T* get( unsigned i ) const
  {
    if( /*0 <= i &&*/ i < length )
    {
      return &data[i];
    }
    return 0;
  }
  bool get( unsigned i, T& t ) const
  {
    if( /*0 <= i &&*/ i < length )
    {
      t = data[i];
      return true;
    }
    return false;
  }
  bool set( unsigned i, T t )
  {
    if( 0 <= i && i < length )
    {
      data[i] = t;
      return true;
    }
    return false;
  }
  void set_all( T t )
  {
    for( unsigned k=0; k<length; k++ ) data[k] = t;
  }
  bool eq_exact ( const Array_t& a ) const;
  bool eq_begin ( const Array_t& a ) const;
  bool eq_middle( const Array_t& a ) const;

  bool insert( const char* _FILE_, const unsigned _LINE_
             , unsigned i, T t );
  bool push( const char* _FILE_, const unsigned _LINE_, T t )
       { return insert( _FILE_, _LINE_, length, t ); }

  bool remove( const unsigned i );
  bool remove( const unsigned i, T& t )
  {
    if( !get( i, t ) ) return false;

    return remove( i );
  }
  bool remove_n( const unsigned i, unsigned num );
  bool pop( T& t ) { return 0<length ? remove( length-1, t ) : false; }
  bool pop()       { return 0<length ? remove( length-1 )    : false; }

  // Returns 1 if the two elements were swapped, else 0
  bool swap( unsigned i, unsigned j );

private:
  bool skip_white( T* d, unsigned& index, const unsigned LEN ) const;
  void skip_white_beg( T* d, int& start, const unsigned LEN ) const;
  void skip_white_end( T* d, const int start, int& finish, const unsigned LEN ) const;
};

template <class T>
bool Array_t<T>::inc_size( const char* _FILE_, const unsigned _LINE_
                         , unsigned new_size )
{
  if( size < new_size )
  {
    unsigned my_new_size = unsigned( size*1.25+4 ) ;
    if( new_size < my_new_size ) new_size = my_new_size ;

    T* new_data = new(_FILE_,_LINE_) T[new_size];
    if( !new_data ) return false;

    for( unsigned k=0; k<length; k++ ) new_data[k] = data[k];

    MemMark(__FILE__,__LINE__);
    if( size ) delete[] data;
    data = new_data;
    size = new_size;
  }
  return true;
}

template <class T>
bool Array_t<T>::set_len( const char* _FILE_, const unsigned _LINE_, unsigned new_len )
{
  if( size < new_len ) { if( !inc_size( _FILE_, _LINE_, new_len ) ) return false; }
  length = new_len;
  return true;
}

template <class T>
bool Array_t<T>::copy( const Array_t& a )
{
  if( this == &a ) return true;

  unsigned a_len = a.len();

  if( a_len ) {
    if( size < a_len ) { if( !inc_size( __FILE__, __LINE__, a_len ) ) return false; }
    for( unsigned k=0; k<a_len; k++ ) data[k] = a[k];
  }
  length = a_len;
  return true;
}

template <class T>
bool Array_t<T>::append( const char* _FILE_, const unsigned _LINE_, const Array_t& a )
{
  if( this == &a ) return false;

  unsigned a_len = a.len();

  if( a_len ) {
    if( size < length + a_len ) { if( !inc_size( _FILE_, _LINE_, length + a_len ) ) return false; }
    for( unsigned k=0; k<a_len; k++ ) data[ length + k ] = a[k];
  }
  length += a_len;
  return true;
}

template <class T>
Array_t<T>::Array_t()
:
  data  (0),
  size  (0),
  length(0)
{
}

template <class T>
Array_t<T>::Array_t( const char* _FILE_, const unsigned _LINE_
                   , unsigned size )
:
  data  (0),
  size  (size),
  length(0)
{
  if( size ) {
    data = new(_FILE_,_LINE_) T[size];
    if( !data ) size = 0;
  }
}

template <class T>
Array_t<T>::Array_t( const char* _FILE_, const unsigned _LINE_
                   , unsigned size, T fill )
:
  data  (0),
  size  (size),
  length(0)
{
  if( size ) {
    data = new(_FILE_,_LINE_) T[size];
    if( !data ) size = 0;
    else {
      for( unsigned k=0; k<size; k++ ) data[k] = fill;
      length = size;
    }
  }
}

template <class T>
Array_t<T>::Array_t( const Array_t<T>& a )
:
  data  (0),
  size  (0),
  length(0)
{
  copy( a );
}

template <class T>
Array_t<T>::~Array_t()
{
  Cleanup(__FILE__,__LINE__);
}

template <class T>
void Array_t<T>::Cleanup( const char* _FILE_, const unsigned _LINE_ )
{
  MemMark(_FILE_,_LINE_);
  if( size ) delete[] data;
  data = 0;
  size = length = 0;
}

template <class T>
bool Array_t<T>::insert( const char* _FILE_, const unsigned _LINE_, unsigned i, T t )
{
  if( i<=length )
  {
    if( length < size || inc_size( _FILE_, _LINE_, size+1 ) )
    {
      for( unsigned k=length; k>i; k-- ) data[k] = data[k-1];
      data[ i ] = t;
      length += 1;
      return true;
    }
  }
  return false;
}

template <class T>
bool Array_t<T>::remove( const unsigned i )
{
  if( i<length )
  {
    // Remove the i'th element
    length -= 1;
    for( unsigned k=i; k<length; k++ ) data[k] = data[k+1];
    return true;
  }
  return false;
}

template <class T>
bool Array_t<T>::remove_n( const unsigned i, const unsigned num )
{
  if( i+num<length )
  {
    // Remove the i'th element
    length -= num;
    for( unsigned k=i; k<length; k++ ) data[k] = data[k+num];
    return true;
  }
  return false;
}

template <class T>
bool Array_t<T>::swap( unsigned i, unsigned j )
{
  if( i!=j && 0<=i && i<length
           && 0<=j && j<length )
  {
    T t = data[i];
    data[i] = data[j];
    data[j] = t;
    return true;
  }
  return false;
}

// Returns true if exact match, else false
template <class T>
bool Array_t<T>::eq_exact( const Array_t& a ) const
{
  if( this == &a ) return true;
  if( len() != a.len() ) return false;

  for( unsigned k=0; k<length; k++ )
  {
    if( data[k] != a.data[k] ) return false;
  }
  return true;
}

// Ignores ending white space.
template <class T>
bool Array_t<T>::eq_begin( const Array_t& a ) const
{
  if( this == &a ) return true;

  unsigned i_data = 0;
  unsigned i_a    = 0;

  while( i_data<length && i_a<a.length )
  {
    if( data[i_data] != a.data[i_a] ) return false;

    i_data++; i_a++;
  }
  bool data_end =   length<=i_data;
  bool    a_end = a.length<=i_a   ;

  if( data_end != a_end )
  {
    // Skip past white space in longer line, and see if it ends afterwards:
    if( !data_end )
         { skip_white(   data, i_data,   length ); data_end =   length<=i_data; }
    else { skip_white( a.data, i_a   , a.length );    a_end = a.length<=i_a   ; }

    // No, longer line did not end after skipping white space:
    if( data_end != a_end ) return false;

    // Yes, longer line ended after skipping white space:
  }
  // Did not find a difference, so lines must be the same:
  return true;
}

// Ignores initial and ending white space.
template <class T>
bool Array_t<T>::eq_middle( const Array_t& a ) const
{
  if( this == &a ) return true;

  int st_data = 0;
  int st_a    = 0;
  int fn_data = 0<  length ?   length-1 : -1;
  int fn_a    = 0<a.length ? a.length-1 : -1;

  skip_white_beg(   data, st_data,   length );
  skip_white_beg( a.data, st_a   , a.length );
  skip_white_end(   data, st_data, fn_data,   length );
  skip_white_end( a.data, st_a   , fn_a   , a.length );

  const int len_mid_data = fn_data - st_data + 1;
  const int len_mid_a    = fn_a    - st_a    + 1;

  if( len_mid_data == len_mid_a )
  {
    for( int k=0; k<len_mid_data; k++ )
    {
      if( data[st_data+k] != a.data[st_a+k] ) return false;
    }
    return true;
  }
  return false;
}

template <class T>
bool Array_t<T>::skip_white( T* d, unsigned& index, const unsigned LEN ) const
{
  // Skip past white space in data:
  bool skipped_white   = false;
  bool found_non_white = false;

  while( !found_non_white && index<LEN )
  {
    const T c = d[index];

    if( c != ' ' && c != '\t' && c != '\r' ) found_non_white = true;
    else {
      index++;
      skipped_white = true;
    }
  }
  return skipped_white;
}

template <class T>
void Array_t<T>::skip_white_beg( T* d
                               , int& start
                               , const unsigned LEN ) const
{
  if( 0<LEN )
  for( char C = d[start]
     ; start<LEN && (' '==C || '\t'==C || '\r'==C); )
  {
    start++;
    if( start<LEN ) C = d[start];
  }
}

template <class T>
void Array_t<T>::skip_white_end( T* d
                               , const int start
                               , int& finish
                               , const unsigned LEN ) const
{
  if( -1<finish )
  for( char C = d[finish]
     ; start<=finish && (' '==C || '\t'==C || '\r'==C); )
  {
    finish--;
    if( start<=finish ) C = d[finish];
  }
}

#endif // __ARRAY_T_HH__

