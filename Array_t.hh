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

#include <vector>

using std::vector;

template <class T>
class Array_t
{
public:
  Array_t();
  Array_t( const unsigned size );
  Array_t( const unsigned len, const T fill );
  Array_t( const Array_t<T>& a );

  virtual ~Array_t();

  Array_t& operator=( const Array_t& a );

  unsigned len() const { return v.size(); }
  unsigned cap() const { return v.capacity(); }
  void clear() { v.clear(); }

  // Only use operator[] if you know 0<i && i<length
  T& operator[]( unsigned i ) { return v[i]; }
  T  operator[]( const unsigned i ) const { return v[i]; }
  T  at( const unsigned i ) const { return v[i]; }

  bool set_len( const unsigned new_len );
  bool inc_cap( const unsigned new_cap );

  T* get( const unsigned i );
  bool get( const unsigned i, T& t ) const;
  bool set( const unsigned i, const T t );
  void set_all( const T t );

  bool insert( const unsigned i, const T t );
  bool push( const T t );

  bool pop( T& t ) { return 0<v.size() ? remove( v.size()-1, t ) : false; }
  bool pop()       { return 0<v.size() ? remove( v.size()-1 )    : false; }

  bool remove( const unsigned i );
  bool remove( const unsigned i, T& t );

  bool remove_n( const unsigned i, unsigned num );

  bool append( const Array_t& a );

  // Returns 1 if the two elements were swapped, else 0
  bool swap( unsigned i, unsigned j );

protected:
  vector<T> v;
};

template <class T>
Array_t<T>::Array_t()
: v()
{
}

template <class T>
Array_t<T>::Array_t( const unsigned cap )
: v()
{
  if( 0 < cap ) v.reserve( cap );
}

template <class T>
Array_t<T>::Array_t( const unsigned len, const T fill )
: v( len, fill )
{
}

template <class T>
Array_t<T>::Array_t( const Array_t<T>& a )
: v( a.v )
{
}

template <class T>
Array_t<T>::~Array_t()
{
}

template <class T>
Array_t<T>& Array_t<T>::operator=( const Array_t& a )
{
  if( this != &a ) v = a.v;

  return *this;
}

template <class T>
bool Array_t<T>::set_len( const unsigned new_len )
{
  if( !inc_cap( new_len ) ) return false;

  // Fill in new values with zero:
  for( unsigned k=v.size(); k<new_len; k++ ) v.push_back( T() );

  // If s is too long, remove values from end:
  for( unsigned k=v.size(); new_len<k; k-- ) v.pop_back();

  return true;
}

template <class T>
bool Array_t<T>::inc_cap( unsigned new_cap )
{
  bool ok = true;

  if( v.capacity() < new_cap )
  {
    v.reserve( new_cap );
  }
  return ok;
}

template <class T>
T* Array_t<T>::get( const unsigned i )
{
  if( i < v.size() )
  {
    return &v[i];
  }
  return 0;
}

template <class T>
bool Array_t<T>::get( const unsigned i, T& t ) const
{
  if( i < v.size() )
  {
    t = v[i];
    return true;
  }
  return false;
}

template <class T>
bool Array_t<T>::set( const unsigned i, const T t )
{
  if( i < v.size() )
  {
    v[i] = t;
    return true;
  }
  return false;
}

template <class T>
void Array_t<T>::set_all( const T t )
{
  for( unsigned k=0; k<v.size(); k++ ) v[k] = t;
}

template <class T>
bool Array_t<T>::insert( const unsigned i, const T t )
{
  if( i<=v.size() )
  {
    v.insert( v.begin() + i, t );
    return true;
  }
  return false;
}

template <class T>
bool Array_t<T>::push( const T t )
{
  v.push_back( t );

  return true;
}

template <class T>
bool Array_t<T>::remove( const unsigned i )
{
  if( i<v.size() )
  {
    // Remove the i'th element
    v.erase( v.begin() + i );

    return true;
  }
  return false;
}

template <class T>
bool Array_t<T>::remove( const unsigned i, T& t )
{
  if( !get( i, t ) ) return false;

  return remove( i );
}

template <class T>
bool Array_t<T>::remove_n( const unsigned i, const unsigned num )
{
  if( i+num<v.size() )
  {
    for( unsigned k=0; k<i; k++ )
    {
      v.erase( v.begin() + i );
    }
    return true;
  }
  return false;
}

template <class T>
bool Array_t<T>::append( const Array_t& a )
{
  if( this != &a )
  {
    v.append( a.v );

    return true;
  }
  return false;
}

template <class T>
bool Array_t<T>::swap( unsigned i, unsigned j )
{
  if( i!=j && i<v.size() && j<v.size() )
  {
    T t = v[i];
    v[i] = v[j];
    v[j] = t;
    return true;
  }
  return false;
}

#endif // __ARRAY_T_HH__

