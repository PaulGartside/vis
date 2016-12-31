
#include "Utilities.hh"
#include "Line.hh"

const unsigned m_primes[] = {  43, 101, 149, 193, 241, 293, 353, 409, 461
                            , 521, 587, 641, 691, 757, 823, 881, 947 };

const unsigned m_num_primes = sizeof( m_primes )/sizeof( unsigned );

struct Line::Data
{
  Data();
  Data( const char*    _FILE_
      , const unsigned _LINE_
      , const unsigned cap );
  Data( const char*    _FILE_
      , const unsigned _LINE_
      , const unsigned cap
      , const uint8_t  fill );
  void Init( const uint8_t fill=0 );

  unsigned capacity; // capacity of data buffer = size-1
  unsigned length;   // # of elements in data buffer
  uint8_t* data;

  bool     chksum_valid;
  unsigned chksum;
};

Line::Data::Data()
  : capacity( 0 )
  , length( 0 )
  , data( new(__FILE__,__LINE__) uint8_t[capacity+1] )
  , chksum_valid( false )
  , chksum( 0 )
{
  Init();
}

Line::Data::Data( const char*    _FILE_
                , const unsigned _LINE_
                , const unsigned cap )
  : capacity( cap )
  , length( 0 )
  , data( new(_FILE_,_LINE_) uint8_t[capacity+1] )
  , chksum_valid( false )
  , chksum( 0 )
{
  Init();
}

Line::Data::Data( const char*    _FILE_
                , const unsigned _LINE_
                , const unsigned len
                , const uint8_t  fill )
  : capacity( len )
  , length( len )
  , data( new(_FILE_,_LINE_) uint8_t[capacity+1] )
  , chksum_valid( false )
  , chksum( 0 )
{
  Init( fill );
}

void Line::Data::Init( const uint8_t fill )
{
  for( unsigned k=0; k<length; k++ ) data[k] = fill;
  data[ length ] = 0;
}

int skip_white_beg( Line::Data& m, int start )
{
  const unsigned LEN = m.length;

  if( 0<LEN )
  {
    for( uint8_t C = m.data[ start ]
       ; start<LEN && (' '==C || '\t'==C || '\r'==C); )
    {
      start++;
      if( start<LEN ) C = m.data[ start ];
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
    for( uint8_t C = m.data[ finish ]
       ; start<=finish && (' '==C || '\t'==C || '\r'==C); )
    {
      finish--;
      if( start<=finish ) C = m.data[ finish ];
    }
  }
  return finish;
}

int calc_chksum( Line::Data& m )
{
  unsigned chk_sum = 0;

  int start = 0;
  int finish = 0<m.length ? m.length-1 : -1;

  start  = skip_white_beg( m, start );
  finish = skip_white_end( m, start, finish );

  for( int i=start; i<=finish; i++ )
  {
    chk_sum ^= m_primes[(i-start)%m_num_primes] ^ m.data[ i ];
    chk_sum = ((chk_sum << 13)&0xFFFFE000)
            | ((chk_sum >> 19)&0x00001FFF);
  }
  return chk_sum;
}

Line::Line()
  : m( *new(__FILE__,__LINE__) Data() )
{
}

Line::Line( const char* _FILE_, const unsigned _LINE_, unsigned cap )
  : m( *new(__FILE__,__LINE__) Data( _FILE_, _LINE_, cap ) )
{
}

Line::Line( const char*    _FILE_
          , const unsigned _LINE_
          , const unsigned len
          , const uint8_t  fill )
  : m( *new(__FILE__, __LINE__) Data( _FILE_, _LINE_, len, fill ) )
{
}

Line::Line( const Line& a )
//: m( *new(__FILE__,__LINE__) Data() )
  : m( *new(__FILE__,__LINE__) Data( __FILE__, __LINE__, a.len() ) )
{
  copy( a );
}

Line::~Line()
{
  MemMark(__FILE__,__LINE__); delete[] m.data;
  MemMark(__FILE__,__LINE__); delete &m;
}

void Line::clear()
{
  m.chksum_valid = false;

  m.length = 0;
  m.data[ m.length ] = 0;
}

unsigned Line::len() const { return m.length; }
unsigned Line::cap() const { return m.capacity; }

bool Line::set_len( const char*    _FILE_
                  , const unsigned _LINE_
                  , const unsigned new_len )
{
  if( new_len != m.length )
  {
    m.chksum_valid = false;

    if( m.capacity < new_len )
    {
      if( !inc_cap( _FILE_, _LINE_, new_len ) ) return false;
    }
    // Fill in new values with zero:
    for( unsigned k=m.length; k<new_len; k++ ) m.data[k] = 0;

    m.length = new_len;
    m.data[ m.length ] = 0;
  }
  return true;
}

bool Line::inc_cap( const char*    _FILE_
                  , const unsigned _LINE_
                  ,       unsigned new_cap )
{
  bool ok = true;

  if( m.capacity < new_cap )
  {
    const unsigned my_new_cap = unsigned( m.capacity*1.25+4 );
    if( new_cap < my_new_cap ) new_cap = my_new_cap;

    uint8_t* new_data = new(_FILE_,_LINE_) uint8_t[new_cap+1];

    if( 0 == new_data ) ok = false;
    else {
      for( unsigned k=0; k<m.length; k++ ) new_data[k] = m.data[k];
      new_data[ m.length ] = 0;

      MemMark(__FILE__,__LINE__); delete[] m.data;

      m.data     = new_data;
      m.capacity = new_cap;
    }
  }
  return ok;
}

bool Line::copy( const Line& a )
{
  if( this == &a ) return true;

  m.chksum_valid = false;

  const unsigned a_len = a.len();

  if( 0<a_len ) {
    if( m.capacity < a_len )
    {
      if( !inc_cap( __FILE__, __LINE__, a_len ) ) return false;
    }
    for( unsigned k=0; k<a_len; k++ ) m.data[k] = a.m.data[k];
  }
  m.length = a_len;
  m.data[ m.length ] = 0;

  return true;
}

uint8_t Line::get( const unsigned i ) const
{
  return m.data[i];
}

void Line::set( const unsigned i, const uint8_t C )
{
  m.chksum_valid = false;

  m.data[i] = C;
}

const char* Line::c_str( const unsigned i ) const
{
  if( i < m.length )
  {
    return RCast<const char*>( &m.data[i] );
  }
  return 0;
}

bool Line::insert( const char*    _FILE_
                 , const unsigned _LINE_
                 , const unsigned i
                 , const uint8_t  t )
{
  if( i<=m.length )
  {
    if( m.length < m.capacity || inc_cap( _FILE_, _LINE_, m.capacity+1 ) )
    {
      m.chksum_valid = false;

      for( unsigned k=m.length; k>i; k-- ) m.data[k] = m.data[k-1];
      m.data[ i ] = t;
      m.length += 1;
      m.data[ m.length ] = 0;
      return true;
    }
  }
  return false;
}

bool Line::push( const char* _FILE_, const unsigned _LINE_, uint8_t t )
{
  return insert( _FILE_, _LINE_, m.length, t );
}

bool Line::remove( const unsigned i )
{
  if( i<m.length )
  {
    m.chksum_valid = false;

    // Remove the i'th element
    m.length -= 1;
    for( unsigned k=i; k<m.length; k++ ) m.data[k] = m.data[k+1];
    m.data[ m.length ] = 0;
    return true;
  }
  return false;
}

bool Line::remove( const unsigned i, uint8_t& t )
{
  if( i<m.length )
  {
    t = m.data[i];
  }
  return remove( i );
}

bool Line::pop( uint8_t& t )
{
  return 0<m.length ? remove( m.length-1, t ) : false;
}

bool Line::pop()
{
  return 0<m.length ? remove( m.length-1 ) : false;
}

bool Line::append( const char* _FILE_, const unsigned _LINE_, const Line& a )
{
  m.chksum_valid = false;

  if( this == &a ) return false;

  const unsigned a_len = a.len();

  if( 0<a_len ) {
    if( m.capacity < m.length + a_len )
    {
      if( !inc_cap( _FILE_, _LINE_, m.length + a_len ) ) return false;
    }
    for( unsigned k=0; k<a_len; k++ ) m.data[ m.length + k ] = a.m.data[k];
  }
  m.length += a_len;
  m.data[ m.length ] = 0;

  return true;
}

bool Line::ends_with( const uint8_t C )
{
  if( 0 < m.length )
  {
    return C == m.data[ m.length-1 ];
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

