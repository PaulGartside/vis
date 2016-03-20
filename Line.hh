
#ifndef __LINE_HH__
#define __LINE_HH__

#include "Array_t.hh"

typedef unsigned char  uint8_t;

class Line
{
public:
  Line();
  Line( const char* _FILE_, const unsigned _LINE_, unsigned size=0 );
  Line( const char* _FILE_, const unsigned _LINE_, unsigned size, uint8_t fill );

  void clear();

  bool set_len ( const char* _FILE_, const unsigned _LINE_, unsigned new_len )
  {
    return m_data.set_len( _FILE_, _LINE_, new_len );
  }
  unsigned len() const { return m_data.len(); }
  unsigned sz () const { return m_data.sz(); }

  bool inc_size( const char* _FILE_, const unsigned _LINE_, unsigned new_size )
  {
    return m_data.inc_size( _FILE_, _LINE_, new_size );
  }
  bool copy( const Line& a );

  uint8_t get( const unsigned i ) const;
  void    set( const unsigned i, const uint8_t C );

  const char* c_str( unsigned i ) const;

  bool insert( const char* _FILE_, const unsigned _LINE_, unsigned i, uint8_t t );

  bool push( const char* _FILE_, const unsigned _LINE_, uint8_t t );

  bool remove( const unsigned i );
  bool remove( const unsigned i, uint8_t& t );

  bool pop( uint8_t& t );
  bool pop();

  bool append( const char* _FILE_, const unsigned _LINE_, const Line& a );

  unsigned chksum();

private:
  int calc_chksum() const;
  int skip_white_beg( int start ) const;
  int skip_white_end( const int start, int finish ) const;

  Array_t<uint8_t> m_data;

  bool     m_chksum_valid;
  unsigned m_chksum;

  static const unsigned m_primes[];
  static const unsigned m_num_primes;
};

#endif

