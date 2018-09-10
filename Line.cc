
#include "Utilities.hh"
#include "String.hh"
#include "Line.hh"

const unsigned m_primes[] = {  43, 101, 149, 193, 241, 293, 353, 409, 461
                            , 521, 587, 641, 691, 757, 823, 881, 947 };

const unsigned m_num_primes = sizeof( m_primes )/sizeof( unsigned );

struct Line::Data
{
  Data();
  Data( const unsigned cap );
  Data( const unsigned len, const uint8_t fill );

  String s;

  bool     chksum_valid;
  unsigned chksum;
};

Line::Data::Data()
  : s()
  , chksum_valid( false )
  , chksum( 0 )
{
}

Line::Data::Data( const unsigned cap )
  : s()
  , chksum_valid( false )
  , chksum( 0 )
{
  s.inc_cap( cap );
}

Line::Data::Data( const unsigned len
                , const uint8_t  fill )
  : chksum_valid( false )
  , chksum( 0 )
{
  s.inc_cap( len );

  for( unsigned k=0; k<len; k++ ) s.push( fill );
}

int skip_white_beg( Line::Data& m, int start )
{
  const unsigned LEN = m.s.len();

  if( 0<LEN )
  {
    for( uint8_t C = m.s.get( start )
       ; start<LEN && (' '==C || '\t'==C || '\r'==C); )
    {
      start++;
      if( start<LEN ) C = m.s.get( start );
    }
  }
  return start;
}

int skip_white_end( Line::Data& m
                  , const int start
                  ,       int finish )
{
  if( -1<finish )
  {
    for( uint8_t C = m.s.get( finish )
       ; start<=finish && (' '==C || '\t'==C || '\r'==C); )
    {
      finish--;
      if( start<=finish ) C = m.s.get( finish );
    }
  }
  return finish;
}

int calc_chksum( Line::Data& m )
{
  unsigned chk_sum = 0;

  int start = 0;
  int finish = 0<m.s.len() ? m.s.len()-1 : -1;

  start  = skip_white_beg( m, start );
  finish = skip_white_end( m, start, finish );

  for( int i=start; i<=finish; i++ )
  {
    chk_sum ^= m_primes[(i-start)%m_num_primes] ^ m.s.get( i );
    chk_sum = ((chk_sum << 13)&0xFFFFE000)
            | ((chk_sum >> 19)&0x00001FFF);
  }
  return chk_sum;
}

Line::Line()
  : m( *new(__FILE__,__LINE__) Data() )
{
}

Line::Line( unsigned cap )
  : m( *new(__FILE__,__LINE__) Data( cap ) )
{
}

Line::Line( const unsigned len, const uint8_t  fill )
  : m( *new(__FILE__, __LINE__) Data( len, fill ) )
{
}

Line::Line( const Line& a )
  : m( *new(__FILE__,__LINE__) Data( a.len() ) )
{
  copy( a );
}

Line::~Line()
{
  MemMark(__FILE__,__LINE__); delete &m;
}

void Line::clear()
{
  m.chksum_valid = false;

  m.s.clear();
}

unsigned Line::len() const { return m.s.len(); }
unsigned Line::cap() const { return m.s.cap(); }

bool Line::set_len( const unsigned new_len )
{
  if( !inc_cap( new_len ) ) return false;

  // Fill in new values with zero:
  for( unsigned k=m.s.len(); k<new_len; k++ ) m.s.push( 0 );

  // If s is too long, remove values from end:
  for( unsigned k=m.s.len(); new_len<k; k-- )
  {
    m.s.pop();
  }
  return true;
}

bool Line::inc_cap( unsigned new_cap )
{
  return m.s.inc_cap( new_cap );
}

bool Line::copy( const Line& a )
{
  if( this == &a ) return true;

  m.chksum_valid = false;
  m.s = a.m.s;

  return true;
}

bool Line::operator==( const Line& a ) const
{
  return m.s == a.m.s;
}

uint8_t Line::get( const unsigned p ) const
{
  return m.s.get(p);
}

void Line::set( const unsigned p, const uint8_t C )
{
  m.chksum_valid = false;

  m.s.set(p, C);
}

const char* Line::c_str( const unsigned p ) const
{
  if( p < m.s.len() )
  {
    return RCast<const char*>( m.s.c_str() + p );
  }
  return 0;
}

const String& Line::toString() const
{
  return m.s;
}

bool Line::insert( const unsigned p
                 , const uint8_t  C )
{
  if( p<=m.s.len() )
  {
    m.s.insert( p, C );
    return true;
  }
  return false;
}

bool Line::push( uint8_t C )
{
  m.s.push( C );
  return true;
}

bool Line::remove( const unsigned p )
{
  if( p<m.s.len() )
  {
    m.chksum_valid = false;

    m.s.remove( p );

    return true;
  }
  return false;
}

bool Line::remove( const unsigned p, uint8_t& C )
{
  if( p<m.s.len() )
  {
    C = m.s.get(p);
  }
  return remove( p );
}

bool Line::pop( uint8_t& C )
{
  return 0<m.s.len() ? remove( m.s.len()-1, C ) : false;
}

bool Line::pop()
{
  return 0<m.s.len() ? remove( m.s.len()-1 ) : false;
}

bool Line::append( const Line& a )
{
  m.chksum_valid = false;

  if( this == &a ) return false;

  m.s.append( a.m.s );

  return true;
}

// Return -1 if this is less    than a,
// Return  1 if this is greater than a,
// Return  0 if this is equal   to   a
int Line::compareTo( const Line& a ) const
{
//if     ( m.s < a.m.s ) return -1;
//else if( a.m.s < m.s ) return  1;
//
//return 0;
  return m.s.compareTo( a.m.s );
}

// Returns true if this is greater than a
bool Line::gt( const Line& a ) const
{
//return a.m.s < m.s;
  return m.s.gt( a.m.s );
}

// Returns true if this is less than a
bool Line::lt( const Line& a ) const
{
//return m.s < a.m.s;
  return m.s.lt( a.m.s );
}

// Return true if this is equal to a
bool Line::eq( const Line& a ) const
{
//return m.s == a.m.s;
  return m.s.eq( a.m.s );
}

bool Line::ends_with( const uint8_t C )
{
  if( 0 < m.s.len() )
  {
    return C == m.s.get( m.s.len()-1 );
  }
  return false;
}

unsigned Line::chksum() const
{
  if( !m.chksum_valid )
  {
    m.chksum = calc_chksum(m);

    m.chksum_valid = true;
  }
  return m.chksum;
}

