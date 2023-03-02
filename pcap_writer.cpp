#include "pcap_writer.hpp"

#include <ctime>
#include <cstdio>


void PcapWriter::open( const std::string& pcap_filename, uint32_t custom_network_type )
{
  if( !pcap_filename.empty() && ('-' != *pcap_filename.cbegin()) )
  {
    handle = fopen( pcap_filename.c_str(), "wb+" );
    if( NULL==handle )
    {
      throw PcapOpening();
    }
  }
  else
  {
    this->tty = true;
  }

  write_header( custom_network_type );
}



void PcapWriter::write_header( uint32_t custom_network_type )
{
  if( NULL==handle )
  {
    throw PcapNotOpen();
  }

  struct pcap_global_header header = \
  {
    /*.magic_number =*/ 0xa1b2c3d4,
    /*.version_major =*/ 2,
    /*.version_minor =*/ 4,
    /*.thiszone =*/ 0,
    /*.sigfigs =*/ 0,
    /*.snaplen =*/ 1024,
    /*.network =*/ custom_network_type /* custom USER */
  };

  if( fwrite( &header, sizeof(header), 1, this->handle ) != 1 )
  {
    throw PcapHeader();
  }
}


void PcapWriter::write_packet( unsigned char const * const buffer, size_t length )
{
  if( NULL==handle )
  {
    throw PcapNotOpen();
  }

  struct timespec t;
  struct pcap_packet_header header;

  clock_gettime( CLOCK_REALTIME, &t );

  header.ts_sec = t.tv_sec;
  header.ts_usec = t.tv_nsec / 1000;
  header.incl_len = length;
  header.orig_len = length;

  if( fwrite( &header, sizeof(header), 1, this->handle ) != 1 )
  {
    throw PcapHeader();
  }

  if( fwrite( buffer, 1, length, this->handle ) != length )
  {
    throw PcapDataWrite();
  }

  fflush( this->handle );
}
