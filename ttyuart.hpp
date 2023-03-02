#ifndef _TTYUART_HPP_
#  define _TTYUART_HPP_
// TTY/UART abstraction to handle portability here.

// not exisiting in MinGW64 -> #include <sys/select.h>
#if defined( __MINGW32__ ) || defined( __MINGW64__ ) // for select(), fd_set and struct timeval
#  include <winsock2.h>
#elif defined( __CYGWIN__ )
#  include <sys/select.h>
#endif

#if defined( __linux__ )
#  include <sys/ioctl.h>
#  include <linux/serial.h>
#endif /*__linux__*/

#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <string>
#include <stdexcept>

#if defined( __CYGWIN__ )
#  include <termios.h>
#elif defined( __MSYS__ )
#  error "must use normal cygwin compiler! MSYS does not have termios!"
#elif defined( __MINGW32__ ) || defined( __MINGW64__ )
#  error "must use normal cygwin compiler! MinGW of MSYS does not have termios!"
#else // real linux:
#  include <termios.h>
#endif



struct TtyUartOpening   : public std::runtime_error { TtyUartOpening()   : std::runtime_error( "ERROR: can not open port" ) {}; };
struct TtyUartNotOpen   : public std::runtime_error { TtyUartNotOpen()   : std::runtime_error( "ERROR: port not open" ) {}; };
struct TtyUartParameter : public std::runtime_error { TtyUartParameter() : std::runtime_error( "ERROR: Baudrate not supported" ) {}; };
struct TtyUartLowLatency: public std::runtime_error { TtyUartLowLatency(): std::runtime_error( "ERROR: Low latency mode not supported" ) {}; };
struct TtyUartAttributes: public std::runtime_error { TtyUartAttributes(): std::runtime_error( "ERROR: port attributes failed" ) {}; };
struct TtyUartSelect    : public std::runtime_error { TtyUartSelect()    : std::runtime_error( "ERROR: select() on port failed" ) {}; };
struct TtyUartReadData  : public std::runtime_error { TtyUartReadData()  : std::runtime_error( "ERROR: read() on port failed" ) {}; };
struct TtyUartWriteData : public std::runtime_error { TtyUartWriteData() : std::runtime_error( "ERROR: write() on port failed" ) {}; };



class TtyUart
{
public:
  TtyUart() : handle(-1) {};
  TtyUart( const std::string& portname ) : TtyUart() { this->open( portname ); };
  ~TtyUart() { this->close(); };

  void open( const std::string& portname );
  void close() { ::close( handle ); };
  void configure( unsigned int baud, unsigned int data_bits, char parity, unsigned int stop_bits, bool low_latency );

  int wait( uint32_t bytes_time_interval_us );
  size_t read( unsigned char* buffer, size_t length );

private:
  static speed_t get_baud( uint32_t baud );

private:
  int handle;
  fd_set read_set;
};




#if !defined( __MINGW32__ ) && !defined( __MINGW64__ ) // while raw MSYS2 shell has termios.h, the full MinGW does not have at least the libraries. Weird!
// has termios.h
#else
#  define CBAUD    0x0100f
#  define B0       0x00000
#  define B50      0x00001
#  define B75      0x00002
#  define B110     0x00003
#  define B134     0x00004
#  define B150     0x00005
#  define B200     0x00006
#  define B300     0x00007
#  define B600     0x00008
#  define B1200    0x00009
#  define B1800    0x0000a
#  define B2400    0x0000b
#  define B4800    0x0000c
#  define B9600    0x0000d
#  define B19200   0x0000e
#  define B38400   0x0000f
/* Extended baud rates above 37K. */
#  define CBAUDEX  0x0100f
#  define B57600   0x01001
#  define B115200  0x01002
#  define B128000  0x01003
#  define B230400  0x01004
#  define B256000  0x01005
#  define B460800  0x01006
#  define B500000  0x01007
#  define B576000  0x01008
#  define B921600  0x01009
#  define B1000000 0x0100a
#  define B1152000 0x0100b
#  define B1500000 0x0100c
#  define B2000000 0x0100d
#  define B2500000 0x0100e
#  define B3000000 0x0100f

#  define CRTSXOFF 0x04000
#  define CRTSCTS  0x08000
#  define CMSPAR   0x40000000 /* Mark or space (stick) parity.  */

#  define CLOCAL   0x00800

/* lflag bits */
#  define ISIG    0x0001
#  define ICANON  0x0002
#  define ECHO    0x0004
#  define ECHOE   0x0008
#  define ECHOK   0x0010
#  define ECHONL  0x0020
#  define NOFLSH  0x0040
#  define TOSTOP  0x0080
#  define IEXTEN  0x0100
#  define FLUSHO  0x0200
#  define ECHOKE  0x0400
#  define ECHOCTL 0x0800

/* iflag bits */
#  define IGNBRK  0x00001
#  define BRKINT  0x00002

#  define ISTRIP  0x00020
#  define INLCR   0x00040
#  define IGNCR   0x00080
#  define ICRNL   0x00100

#  define IXON    0x00400
#  define IXOFF   0x01000
#  define IUCLC   0x04000
#  define IXANY   0x08000
#  define PARMRK  0x10000

/* oflag bits */
#  define OPOST   0x00001
#  define OLCUC   0x00002
#  define OCRNL   0x00004
#  define ONLCR   0x00008
#  define ONOCR   0x00010


#  define CSIZE    0x00030
#  define CS5      0x00000
#  define CS6      0x00010
#  define CS7      0x00020
#  define CS8      0x00030
#  define CSTOPB   0x00040
#  define CREAD    0x00080
#  define PARENB   0x00100
#  define PARODD   0x00200
//#  define HUPCL    0x00400
//#  define CLOCAL   0x00800

#  define VMIN            9
#  define VTIME           16

#  define TCSAFLUSH       1
#  define TCSANOW         2
#  define TCSADRAIN       3
#  define TCSADFLUSH      4


#endif // !defined( __MINGW32__ ) && !defined( __MINGW64__ ) // while raw MSYS2 shell has termios.h, the full MinGW does not have at least the libraries. Weird!

#endif //def _TTYUART_HPP_
