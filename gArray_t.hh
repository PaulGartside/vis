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

#ifndef __GARRAY_HH__
#define __GARRAY_HH__

#include "MemCheck.hh"

// garbage Array_t -
// Array_t whose data points to objects that will be deleted
// in its destructor.  Only use gArray_t<T> if T is a pointer.
//
template <class T>
class gArray_t
{
protected:
  T*       data;
  unsigned size;   // size of data buffer
  unsigned length; // # of elements in list

public:
  bool inc_size( unsigned new_size );

  gArray_t& operator=( gArray_t& a )
  {
    if( this != &a ) copy( a );
    return *this;
  }
  bool copy( const gArray_t& a );
  bool append( const gArray_t& a );

  gArray_t( unsigned size=0 );

  gArray_t( const gArray_t<T>& a );

  virtual ~gArray_t();

  void Cleanup( const char* _FILE_, const unsigned _LINE_ );
  void clear();

  unsigned len() const { return length; }
  unsigned sz () const { return size  ; }

  // Only use operator[] if you know 0<i && i<length
  T& operator[]( unsigned i ) const { return data[i]; }

  T* get( unsigned i ) const
  {
    if( 0 <= i && i < length )
    {
      return &data[i];
    }
    return false;
  }
  bool get( unsigned i, T& t ) const
  {
    if( 0 <= i && i < length )
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

  bool insert( unsigned i, T t );
  bool push( T t ) { return insert( length, t ); }

  bool remove( unsigned i );
  bool remove( unsigned i, T& t );

  bool pop( T& t ) { return remove( length-1, t ); }
  bool pop()       { return remove( length-1 ); }

  // Returns 1 if the two elements were swapped, else 0
  bool swap( unsigned i, unsigned j );
};

template <class T>
bool gArray_t<T>::inc_size( unsigned new_size )
{
  if( size < new_size )
  {
    unsigned my_new_size = unsigned( size*1.25+4 ) ;
    if( new_size < my_new_size ) new_size = my_new_size ;

    T* new_data = new(__FILE__,__LINE__) T[new_size];
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
bool gArray_t<T>::copy( const gArray_t& a )
{
  if( this == &a ) return true;

  const unsigned a_len = a.len();

  if( a_len )
  {
    if( size < a_len ) { if( !inc_size( a_len ) ) return false; }

    // Copy constructor of T called here to properly copy data:
    for( unsigned k=0; k<a_len; k++ ) data[k] = a[k];
  }
  length = a_len;
  return true;
}

template <class T>
bool gArray_t<T>::append( const gArray_t& a )
{
  if( this == &a ) return false;

  unsigned a_len = a.len();

  if( a_len ) {
    if( size < length + a_len ) { if( !inc_size( length + a_len ) ) return false; }
    for( unsigned k=0; k<a_len; k++ ) data[ length + k ] = a[k];
  }
  length += a_len;
  return true;
}

template <class T>
gArray_t<T>::gArray_t( unsigned size )
:
  data  (0),
  size  (size),
  length(0)
{
  if( size ) {
    data = new(__FILE__,__LINE__) T[size];
    if( !data ) size = 0;
  }
}

template <class T>
gArray_t<T>::gArray_t( const gArray_t<T>& a )
:
  data  (0),
  size  (0),
  length(0)
{
  copy( a );
}

template <class T>
gArray_t<T>::~gArray_t()
{
  Cleanup(__FILE__,__LINE__);
}

template <class T>
void gArray_t<T>::Cleanup( const char* _FILE_, const unsigned _LINE_ )
{
  clear();

  MemMark(_FILE_,_LINE_);
  if( size ) delete[] data;
  data = 0;
  size = length = 0;
}

template <class T>
void gArray_t<T>::clear()
{
  for( unsigned k=0; k<length; k++ )
  {
    MemMark(__FILE__,__LINE__);
    delete data[k];
    data[k] = 0;
  }
  length = 0;
}

// t will now be owned by gArray_t<T>
template <class T>
bool gArray_t<T>::insert( unsigned i, T t )
{
  if( /*0<=i &&*/ i<=length )
  {
    if( length < size || inc_size( size+1 ) )
    {
      for( unsigned k=length; i<k; k-- ) data[k] = data[k-1];
      data[ i ] = t;
      length += 1;
      return true;
    }
  }
  return false;
}

template <class T>
bool gArray_t<T>::remove( unsigned i )
{
  if( 0<=i && i<length )
  {
    // Remove the i'th element
    MemMark(__FILE__,__LINE__);
    delete data[i];

    length -= 1;
    for( unsigned k=i; k<length; k++ ) data[k] = data[k+1];
    return true;
  }
  return false;
}

// Caller of remove() owns t
template <class T>
bool gArray_t<T>::remove( unsigned i, T& t )
{
  if( /*0 <= i &&*/ i < length )
  {
    t = data[i];

    length -= 1;
    for( unsigned k=i; k<length; k++ ) data[k] = data[k+1];
    return true;
  }
  return false;
}

template <class T>
bool gArray_t<T>::swap( unsigned i, unsigned j )
{
  if( i!=j && /*0<=i &&*/ i<length
           && /*0<=j &&*/ j<length )
  {
    T t = data[i];
    data[i] = data[j];
    data[j] = t;
    return true;
  }
  return false;
}

#endif // __GARRAY_HH__

