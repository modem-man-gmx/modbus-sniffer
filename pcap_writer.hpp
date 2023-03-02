#ifndef _PCAP_WRITER_
#define _PCAP_WRITER_

#include <cstdint>
#include <string>
#include <stdexcept>

#ifndef PCAP_CUSTOM_NW_TYPE
#define PCAP_CUSTOM_NW_TYPE 147
#endif

struct pcap_global_header
{
    uint32_t magic_number;  /* magic number */
    uint16_t version_major; /* major version number */
    uint16_t version_minor; /* minor version number */
    int32_t  thiszone;      /* GMT to local correction */
    uint32_t sigfigs;       /* accuracy of timestamps */
    uint32_t snaplen;       /* max length of captured packets, in octets */
    uint32_t network;       /* data link type */
} __attribute__((packed));


struct pcap_packet_header
{
    uint32_t ts_sec;   /* timestamp seconds */
    uint32_t ts_usec;  /* timestamp microseconds */
    uint32_t incl_len; /* number of octets of packet saved in file */
    uint32_t orig_len; /* actual length of packet */
} __attribute__((packed));


struct PcapOpening  : public std::runtime_error { PcapOpening()   : std::runtime_error( "ERROR: can not open pcap output file" ) {}; };
struct PcapNotOpen  : public std::runtime_error { PcapNotOpen()   : std::runtime_error( "ERROR: pcap file not open" ) {}; };
struct PcapHeader   : public std::runtime_error { PcapHeader()    : std::runtime_error( "ERROR: can not write pcap output file header" ) {}; };
struct PcapDataWrite: public std::runtime_error { PcapDataWrite() : std::runtime_error( "ERROR: can not write pcap output file data" ) {}; };


class PcapWriter
{
public:
  PcapWriter() : tty(false), handle(NULL) {};
  PcapWriter( const std::string& pcap_filename, uint32_t custom_network_type = PCAP_CUSTOM_NW_TYPE )
    : PcapWriter()
    { this->open( pcap_filename, custom_network_type ); };
  ~PcapWriter() { this->close(); };

  void open( const std::string& pcap_filename, uint32_t custom_network_type = PCAP_CUSTOM_NW_TYPE );
  void close() { ::fclose( handle ); };
  bool is_open() const { return (NULL != this->handle); };
  bool is_a_tty() const { return this->tty; };

  void write_packet( unsigned char const * const buffer, size_t length );

private:
  void write_header( uint32_t custom_network_type ); // recent Ethereal complains about and no understanding what to do better...

private:
  bool tty;
  FILE *handle;
};




#endif // _PCAP_WRITER_
