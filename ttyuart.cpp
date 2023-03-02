// TTY/UART abstraction to handleclTabCtrl portability here.

#include <errno.h>
#include <iostream> // cerr

#include "ttyuart.hpp"

using std::string;


void TtyUart::open( const string& portname )
  {
    handle = ::open( portname.c_str(), O_RDONLY);
    if( handle < 0 )
    {
      switch(errno)
      {
        case ENOENT:
          std::cerr << "probably wrong device name." << std::endl;
#         ifdef __CYGWIN__
          std::cerr << "try one of \"ls /dev/tty*\"" << std::endl;
#         endif
          break;
        case EPERM : // fallthrough
        case EACCES:
          std::cerr << "probably missed to set \"sudo adduser $USER dialout\"" << std::endl;
          break;
      }
      throw TtyUartOpening();
    }
  }




/* https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp */
void TtyUart::configure( unsigned int baud, unsigned int data_bits, char parity, unsigned int stop_bits,
#                      if defined( __linux__ )
                         bool low_latency )
#                      else
                         bool /*no nam, is unused*/ )
#                      endif
{
  if( handle < 0 )
    throw std::runtime_error( "ERROR: port not open" );

  struct termios tty;

#if defined( __linux__ )
  if( low_latency )
  {
    struct serial_struct serial;

    if( ioctl( handle, TIOCGSERIAL, &serial) < 0 )
    {
      throw TtyUartLowLatency();
    }
    else
    {
      serial.flags |= ASYNC_LOW_LATENCY;
      if( ioctl( handle, TIOCSSERIAL, &serial ) < 0 )
          throw TtyUartLowLatency();
    }
  }
#endif /*__linux__*/

  if( tcgetattr( handle, &tty ) < 0 )
      throw TtyUartAttributes();

  /* set parity */
  if( parity == 'N' )
      tty.c_cflag &= ~PARENB;

  if( parity == 'E' )
      tty.c_cflag |= PARENB;

  if( parity == 'O' )
      tty.c_cflag |= PARODD | PARENB;

  /* set stop bits */
  if( stop_bits == 2 )
      tty.c_cflag |= CSTOPB;
  else
      tty.c_cflag &= ~CSTOPB;

  /* set bits */
  tty.c_cflag &= ~CSIZE;

  switch( data_bits )
  {
    case 5: tty.c_cflag |= CS5; break;
    case 6: tty.c_cflag |= CS6; break;
    case 7: tty.c_cflag |= CS7; break;
    default: tty.c_cflag |= CS8; break;
  }

  /* disable RTS/CTS hardware flow control */
  tty.c_cflag &= ~CRTSCTS;

  /* turn on READ & ignore ctrl lines (CLOCAL = 1) */
  tty.c_cflag |= CREAD | CLOCAL;

  /* disable canonical mode */
  tty.c_lflag &= ~ICANON;

  /* disable echo */
  tty.c_lflag &= ~ECHO;

  /* disable erasure */
  tty.c_lflag &= ~ECHOE;

  /* disable new-line echo */
  tty.c_lflag &= ~ECHONL;

  /* disable interpretation of INTR, QUIT and SUSP */
  tty.c_lflag &= ~ISIG;

  /* turn off s/w flow ctrl */
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);

  /* disable any special handling of received bytes */
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

  /* prevent special interpretation of output bytes (e.g. newline chars) */
  tty.c_oflag &= ~OPOST;

  /* prevent conversion of newline to carriage return/line feed */
  tty.c_oflag &= ~ONLCR;

#if defined(__linux__) || defined(__CYGWIN__)
  /* prevent conversion of tabs to spaces */
  tty.c_oflag &= ~XTABS; // on GNU/Linux systems it is available as XTABS.
#else
  /* prevent conversion of tabs to spaces */
  tty.c_oflag &= ~OXTABS; // This bit exists only on BSD systems and GNU/Hurd systems; on GNU/Linux systems it is available as XTABS.

  /* prevent removal of C-d chars (0x004) in output */
  tty.c_oflag &= ~ONOEOT; //  This bit exists only on BSD systems and GNU/Hurd systems.
#endif

  /* how much to wait for a read */
  tty.c_cc[VTIME] = 0;

  /* minimum read size: 1 byte */
  tty.c_cc[VMIN] = 0;

  /* set port speed */
  cfsetispeed( &tty, get_baud( baud ) );
  cfsetospeed( &tty, get_baud( baud ) );

  if( tcsetattr( handle, TCSANOW, &tty ) < 0 )
      throw TtyUartAttributes();

  return;
}


int TtyUart::wait( uint32_t bytes_time_interval_us )
{
  /* RTFM! these are overwritten after each select call and thus must be inizialized again */
  FD_ZERO( &this->read_set );
  FD_SET( this->handle, &this->read_set );

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = bytes_time_interval_us;

  int res = select( this->handle + 1, &this->read_set, NULL, NULL, &timeout );

  FD_ISSET( this->handle, &this->read_set );
  /* also timeout can hold remaining wait time in Linux */

  if( res < 0 && errno != EINTR )
  {
    throw TtyUartSelect();
  }

  return res;
}


size_t TtyUart::read( unsigned char* buffer, size_t length )
{
  int res = ::read( this->handle, buffer, length );
  if( res < 0 )
  {
    throw TtyUartReadData();
  }
  return (size_t) res;
}


/* https://stackoverflow.com/questions/47311500/how-to-efficiently-convert-baudrate-from-int-to-speed-t */
speed_t TtyUart::get_baud( unsigned int baud )
{
  switch( baud )
  {
    case 50:
    	return B50; // not supported on i8250 / i16450 / i16550 @ x86 PC architecture
    case 75:
    	return B75;
    case 110:
    	return B110; // not supported on i8250 / i16450 / i16550 @ x86 PC architecture
    case 134:
    	return B134; // not supported on i8250 / i16450 / i16550 @ x86 PC architecture
    case 150:
    	return B150;
    case 200:
    	return B200; // not supported on i8250 / i16450 / i16550 @ x86 PC architecture
    case 300:
    	return B300;
    case 600:
    	return B600; // unusual or rarely supported on i8250 / i16450 / i16550 @ x86 PC architecture
    case 1200:
    	return B1200;
    case 1800:
    	return B1800; // not supported on i8250 / i16450 / i16550 @ x86 PC architecture
    case 2400:
    	return B2400;
    case 4800:
    	return B4800;
    case 9600:
    	return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
#   if defined( CBAUDEX )
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400; // not supported on legacy i8250 / i16450 and original i16550 @ x86 PC architecture, because of clock divider. But avail on later hardware
    case 460800:
        return B460800; // not supported on legacy
    case 500000:
        return B500000; // not supported on legacy
    case 576000:
        return B576000; // not supported on legacy
    case 921600:
        return B921600; // not supported on legacy
    case 1000000:
        return B1000000; // not supported on legacy
    case 1152000:
        return B1152000; // not supported on legacy
    case 1500000:
        return B1500000; // not supported on legacy
    case 2000000:
        return B2000000; // not supported on legacy
    case 2500000:
        return B2500000; // not supported on legacy
    case 3000000:
        return B3000000; // not supported on legacy
#   endif // defined( CBAUDEX )
    default:
        throw std::runtime_error( "ERROR: Baudrate not supported" );
  }
  return -1;
}
