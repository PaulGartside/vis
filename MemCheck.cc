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

#include <assert.h>
#include <string.h>    // memcpy, memset
#include <stdio.h>     // printf, stderr, FILE, fopen, fclose

#include "Utilities.hh"
#include "MemCheck.hh"

#ifndef WIN32
extern unsigned gl_bytes_out;
#endif

const bool MEM_CHECK = false; // Whether to validate malloc/free

unsigned num_mallocs  = 0;
unsigned tot_malloced = 0;
unsigned cur_malloced = 0;
unsigned max_malloced = 0;

// Keep track of memory allocation/free to "prove" cleanup.
struct MEM
{
  const char* file; // malloc'd in which source file
  unsigned    line; // malloc'd on which source line
  unsigned    size; // number of bytes allocated
  // allocated data is here
};

// MemArray - a hybrid between an array and a list
class MemArray
{
public:
  bool inc_size( unsigned new_size );
  bool set_len ( unsigned new_len );

  MemArray& operator=( MemArray& a )
  {
    if( this != &a ) copy( a );
    return *this;
  }
  bool copy( const MemArray& a );
  bool append( const MemArray& a );

  MemArray( unsigned size=0 );

  MemArray( const MemArray& a );

  virtual ~MemArray();

  void clear() { length = 0; }

  unsigned len() const { return length; }
  unsigned sz () const { return size  ; }

  // Only use operator[] if you know 0<i && i<length
  MEM*& operator[]( unsigned i ) const { return data[i]; }

  MEM** get( unsigned i ) const;
  bool get( unsigned i, MEM*& t ) const;
  bool set( unsigned i, MEM* t );

  bool insert( unsigned i, MEM* t );
  bool push( MEM* t ) { return insert( length, t ); }

  bool remove( const unsigned i );
  bool remove( const unsigned i, unsigned num );
  bool remove( const unsigned i, MEM*& t );

  bool pop( MEM*& t ) { return remove( length-1, t ); }
  bool pop()          { return remove( length-1 ); }

  // Returns 1 if the two elements were swapped, else 0
  bool swap( unsigned i, unsigned j );

private:
  MEM**    data;
  unsigned size;   // size of data buffer
  unsigned length; // # of elements in list
};

// MemSet - a MEM* set based on a MemArray
class MemSet
{
public:
  MemSet() {}
  virtual ~MemSet() {}

  int len() const { return keys.len(); }

  // index() is for fast lookup when incrementing
  // through the array, and in general should not be used.
  // Only use index() if you know 0<i && i<len()
  MEM* index( int i ) const { return keys[i]; }

  // If key is found in map, returns 1, else returns 0
  // If key is found in map, the value is copied into val
  bool has( MEM* key ) const;
  bool remove( MEM* key );

  // Returns 1 if key/value pair was added, else returns 0
  bool add( MEM* key );

  // Returns 1 if key is found in map, else returns 0
  // If key is found, idx is set to the position of the key
  bool position( MEM* key, unsigned& idx ) const;

private:
  // The compare function must return an integer
  // greater than, equal to, or less than 0
  // according to whether the first argument is
  // greater than, equal to, or less than the
  // second argument, respectively.
  int compare( MEM* p1, MEM* p2 ) const;

  MemArray keys;
};

// void* - Pointer to allocated memory
// bool  - Whether or not memory has been freed
MemSet MemMap;

void* MemAlloc( const unsigned size
              , const char*    file
              , const unsigned line )
{
  assert( file && size );

  const unsigned MALLOC_SZ = MEM_CHECK ? size + sizeof(MEM)
                                       : size;
  MEM* p = (MEM*)malloc( MALLOC_SZ );
  if( 0==p )
  {
    ASSERT( __LINE__, 0, "%s: malloc( %u ) Failed @ %s(%u)\n"
                       , __FUNCTION__, MALLOC_SZ, file, line );
    MemClean();
    exit( 0 );
  }
  memset( p, 0, MALLOC_SZ );
  num_mallocs ++;
  tot_malloced += MALLOC_SZ;
  cur_malloced += MALLOC_SZ;
  if( max_malloced < cur_malloced ) max_malloced = cur_malloced;

  if( !MEM_CHECK ) return p;

  p->file = file;
  p->line = line;
  p->size = size;

  if( MemMap.has( p ) )
  {
    // Should never get here:
    ASSERT( __LINE__, 0, "%s: malloc returned location already in use @ %s(%u)\n"
                       , __FUNCTION__, file, line );
    MemClean();
    exit( 0 );
  }
  else { // Memory location p not in map, so add it:
    if( !MemMap.add( p ) )
    {
      ASSERT( __LINE__, 0, "%s: Failed to add to MemMap @ %s(%u)\n"
                         , __FUNCTION__, file, line );
      MemClean();
      exit( 0 );
    }
  }
  // Return first byte of user memory:
  return p + 1;
}

void MemFree( void* const    userPtr
            , const char*    file
            , const unsigned line )
{
  if( !userPtr || !MEM_CHECK )
  {
    free( userPtr );
  }
  else {
    // After decrementing u, it points to the originally allocated MEM struct,
    // not the user portion of memory pointed to by userPtr
    MEM* u = (MEM*)userPtr; u -= 1;

    if( MemMap.remove( u ) )
    {
      cur_malloced -= u->size + sizeof(MEM);
      free( u );
    }
    else {
      fprintf( stderr, "%s: %p[%u] from %s(%u) NOT FOUND IN LIST\n"
                     , __FUNCTION__, u, u->size, file, line );
    }
  }
}

// Called at the end of time to clean up allocated memory
// that should already have been cleaned up
void MemClean()
{
  // All memory allocated by MemAlloc SHOULD have been MemFree'd by now!
  const int LEN = MemMap.len();

  for( int k=0; k<LEN; k++ )
  {
    MEM* p = MemMap.index( k );
    fprintf( stderr, "%s: Failed to free %p[%u] from %s(%u)\n"
                   , __FUNCTION__
                   , p, p->size, p->file, p->line );
    free( p );
  }
#ifndef WIN32
  fprintf( stdout, "mallocs:(num=%u,total=%u,max=%u), bytes-out:(%u)\n"
                 , num_mallocs, tot_malloced, max_malloced, gl_bytes_out );
#endif
}

static const char*    m_file = "Unkown";
static       unsigned m_line = 0;

void MemMark( const char* file, const unsigned line )
{
  m_file = file;
  m_line = line;
}

void* operator new( size_t size, const char* file, const unsigned line )
{
  return MemAlloc( size, file, line );
}

void* operator new[]( size_t size, const char* file, const unsigned line )
{
  return MemAlloc( size, file, line );
}

//void operator delete( void* p, const char* file, const unsigned line )
#if defined( SUNOS )
void operator delete( void* p )
#else
void operator delete( void* p ) throw()
#endif
{
  MemFree( p, m_file, m_line );
}

//void operator delete[]( void* p, const char* file, const unsigned line )
#if defined( SUNOS )
void operator delete[]( void* p )
#else
void operator delete[]( void* p ) throw()
#endif
{
  MemFree( p, m_file, m_line );
}

bool MemArray::inc_size( unsigned new_size )
{
  if( size < new_size )
  {
    unsigned my_new_size = unsigned( size*1.25+4 ) ;
    if( new_size < my_new_size ) new_size = my_new_size ;

    MEM** new_data = (MEM**)malloc( sizeof(MEM*)*new_size );
    if( !new_data ) return false;

    for( unsigned k=0; k<length; k++ ) new_data[k] = data[k];

    if( size ) free( data );
    data = new_data;
    size = new_size;
  }
  return true;
}

bool MemArray::set_len( unsigned new_len )
{
  if( size < new_len ) { if( !inc_size( new_len ) ) return false; }
  length = new_len;
  return true;
}

bool MemArray::copy( const MemArray& a )
{
  if( this == &a ) return true;

  unsigned a_len = a.len();

  if( a_len ) {
    if( size < a_len ) { if( !inc_size( a_len ) ) return false; }
    for( unsigned k=0; k<a_len; k++ ) data[k] = a[k];
  }
  length = a_len;
  return true;
}

bool MemArray::append( const MemArray& a )
{
  if( this == &a ) return 0;

  unsigned a_len = a.len();

  if( a_len ) {
    if( size < length + a_len ) { if( !inc_size( length + a_len ) ) return false; }
    for( unsigned k=0; k<a_len; k++ ) data[ length + k ] = a[k];
  }
  length += a_len;
  return true;
}

MemArray::MemArray( unsigned size )
:
  data  (0),
  size  (size),
  length(0)
{
  if( size ) {
    data = (MEM**)malloc( sizeof(MEM*)*size );
    if( !data ) size = 0;
  }
}

MemArray::MemArray( const MemArray& a )
:
  data  (0),
  size  (0),
  length(0)
{
  copy( a );
}

MemArray::~MemArray()
{
  if( size ) free( data );
  data = 0;
  size = length = 0;
}

bool MemArray::insert( unsigned i, MEM* t )
{
  if( i<=length )
  {
    if( length < size || inc_size( size+1 ) )
    {
      for( unsigned k=length; k>i; k-- ) data[k] = data[k-1];
      data[ i ] = t;
      length += 1;
      return true;
    }
  }
  return false;
}

MEM** MemArray::get( unsigned i ) const
{
  if( i < length )
  {
    return &data[i];
  }
  return 0;
}

bool MemArray::get( unsigned i, MEM*& t ) const
{
  if( i < length )
  {
    t = data[i];
    return true;
  }
  return false;
}

bool MemArray::set( unsigned i, MEM* t )
{
  if( i < length )
  {
    data[i] = t;
    return true;
  }
  return false;
}

bool MemArray::remove( const unsigned i )
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

bool MemArray::remove( const unsigned i, const unsigned num )
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

bool MemArray::remove( const unsigned i, MEM*& t )
{
  if( !get( i, t ) ) return 0;

  return remove( i );
}

bool MemArray::swap( unsigned i, unsigned j )
{
  if( i!=j && i<length && j<length )
  {
    MEM* t = data[i];
    data[i] = data[j];
    data[j] = t;
    return true;
  }
  return false;
}

bool MemSet::has( MEM* key ) const
{
  unsigned pos = 0;
  if( !position( key, pos ) ) return false;

  return true;
}

bool MemSet::remove( MEM* key )
{
  unsigned pos = 0;
  if( !position( key, pos ) )
  {
    return false;
  }
  if( !keys.remove( pos ) )
  {
    return false;
  }
  return true;
}

bool MemSet::add( MEM* key )
{
  // In a map the keys are unique, so dont add an existing key
  unsigned pos = 0;
  if( position( key, pos ) ) return false;

  if( !keys.push( key ) ) return false;

  // Move the new pair into the proper place in the list
  for( int k=keys.len()-1; k>0; k-- )
  {
    if( compare( keys[k-1], keys[k] )>0 )
    {
      keys.swap( k, k-1 );
    }
    else break;
  }
  return true;
}

bool MemSet::position( MEM* key, unsigned& idx ) const
{
  if( keys.len()==0 ) return 0;

  unsigned max = keys.len()-1;
  unsigned min = 0;
  unsigned pos = (int)(0.5*(max+min));

  while( min<=pos && pos<=max )
  {
    int cmp = compare( key, keys[pos] );

    if     ( cmp == 0 ) { idx = pos; return 1; }
    else if( min==max ) return 0;
    else if( cmp <  0 ) max = pos-1;
    else  /* cmp >  0*/ min = pos+1;

    pos = (int)(0.5*(max+min));
  }
  return 0;
}

int MemSet::compare( MEM* p1, MEM* p2 ) const
{
  if( p2 < p1 ) return  1;
  if( p1 < p2 ) return -1;
                return  0;
}

