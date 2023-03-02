/*
 * A sniffer for the Modbus protocol
 * (c) 2020-2022 Alessandro Righi - released under the MIT license
 * (c) 2021 vheat - released under the MIT license
 */

#define _DEFAULT_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "read_modbus_definitions.h"
#include "ttyuart.hpp"
#include "pcap_writer.hpp"

#define DIE(err) do { perror(err); exit(EXIT_FAILURE); } while (0)

/*
 * maximum Modbus packet size. By the standard is 300 bytes
 */
#define MODBUS_MAX_PACKET_SIZE 300

struct cli_args {
    const char *serial_port;
    const char *output_file;
    char parity;
    int bits;
    uint32_t speed;
    int stop_bits;
    uint32_t bytes_time_interval_us;
    bool low_latency;
    bool ignore_crc;
    int max_packet_per_capture;
    const char* definition_cfg;
    const char* commands_cfg;
};

struct option long_options[] = {
    { "serial-port", required_argument, NULL, 'p' },
    { "output",      required_argument, NULL, 'o' },
    { "speed",       required_argument, NULL, 's' },
    { "parity",      required_argument, NULL, 'P' },
    { "bits",        required_argument, NULL, 'b' },
    { "stop-bits",   required_argument, NULL, 'S' },
    { "interval",    required_argument, NULL, 't' },
    { "max-packets", required_argument, NULL, 'm' },
    { "low-latency", no_argument,       NULL, 'l' },
    { "help",        no_argument,       NULL, 'h' },
    { "ignore-crc",  no_argument,       NULL, 'i' },
    { "registers-def",optional_argument,NULL, 'r' },
    { "commands-def",optional_argument, NULL, 'c' },
    { NULL,          0,                 NULL,  0  },
};

volatile int rotate_log = 1;


uint16_t crc16_table[] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040,
};

int crc_check(uint8_t *buffer, int length, uint16_t* return_crc)
{
    uint8_t byte;
    uint16_t crc = 0xFFFF;
    int valid_crc;

   while (length-- > 2) {
      byte = *buffer++ ^ crc;
      crc >>= 8;
      crc ^= crc16_table[byte];
   }

   valid_crc = ((crc >> 8) == (buffer[1] & 0xFF))  && ((crc & 0xFF) == (buffer[0] & 0xFF)) ;

   fprintf(stderr, "CRC: %04X = %02X%02X [%s]\n", crc, buffer[1] & 0xFF, buffer[0] & 0xFF, valid_crc ? "OK" : "FAIL");
   if(return_crc) *return_crc = crc;
   return valid_crc;
}


void usage(FILE *fp, char *progname, int exit_code)
{
    int n;

    fprintf(fp, "Usage: %s %n[-hl] [-o output] [-p port] [-s speed]\n", progname, &n);
    fprintf(fp, "%*c[-P parity] [-S stop_bits] [-b bits]\n\n", n, ' ');
    fprintf(fp, " -h, --help          print help like this\n");
    fprintf(fp, " -o, --output        output file to use (defaults to stdout, file will be truncated if already existing)\n");
    fprintf(fp, " -p, --serial-port   serial port to use\n");
    fprintf(fp, " -s, --speed         serial port speed (default 9600)\n");
    fprintf(fp, " -b, --bits          number of bits (default 8)\n");
    fprintf(fp, " -P, --parity        parity to use (default 'N')\n");
    fprintf(fp, " -S, --stop-bits     stop bits to use (default 1)\n");
    fprintf(fp, " -t, --interval      time interval between packets (default 1500 us)\n");  // <7291.66_us@4800 <3645.833_us@9600, <1822.9166_us@19200, <911.45833_us@38400, ...
    fprintf(fp, " -i, --ignore-crc    dump also broken packages\n");
    fprintf(fp, " -m, --max-packets   maximum number of packets in capture file (default 10000)\n");
    fprintf(fp, " -r, --registers-def definition file with modbus registers specification\n");
    fprintf(fp, " -c, --commands-def  definition file with modbus commands specification\n");

#ifdef __linux__
    fprintf(fp, " -l, --low-latency  try to enable serial port low-latency mode (Linux-only)\n");
#endif

    exit(exit_code);
}

void parse_args(int argc, char **argv, struct cli_args *args)
{
    int opt;

    /* default values */
    args->serial_port = "/dev/ttyAMA0";
    args->output_file = "-";
    args->parity = 'N';
    args->bits = 8;
    args->speed = 9600;
    args->stop_bits = 1;
    args->bytes_time_interval_us = 1500;
    args->low_latency = false;
    args->ignore_crc = false;
    args->max_packet_per_capture = 10000;
    args->definition_cfg = nullptr;
    args->commands_cfg = nullptr;


    while ((opt = getopt_long(argc, argv, "o:p:s:b:P:S:t:hlim:r:c:", long_options, NULL)) >= 0) {
        switch (opt) {
        case 'o':
            args->output_file = optarg;
            break;
        case 'p':
            args->serial_port = optarg;
            break;
        case 's':
            args->speed = strtoul(optarg, NULL, 10);
            break;
        case 'b':
            args->bits = atoi(optarg);
            break;
        case 'P':
            args->parity = optarg[0];
            break;
        case 'S':
            args->stop_bits = atoi(optarg);
            break;
        case 't':
            args->bytes_time_interval_us = strtoul(optarg, NULL, 10);
            break;
        case 'h':
            usage(stdout, argv[0], EXIT_SUCCESS);
            break;
        case 'l':
            args->low_latency = true;
            break;
        case 'i':
            args->ignore_crc = true;
            break;
        case 'm':
            args->max_packet_per_capture = atoi(optarg);
            break;
        case 'r':
            args->definition_cfg = optarg;
            break;
        case 'c':
            args->commands_cfg = optarg;
            break;
        default:
            usage(stderr, argv[0], EXIT_FAILURE);
        }
    }

    fprintf(stderr, "output file: %s\n", args->output_file);
    fprintf(stderr, "serial port: %s\n", args->serial_port);
    fprintf(stderr, "port type: %d%c%d %d baud\n", args->bits, args->parity, args->stop_bits, args->speed);
    fprintf(stderr, "time interval: %d\n", args->bytes_time_interval_us);
    fprintf(stderr, "maximum packets in capture: %d\n", args->max_packet_per_capture);
    if (args->commands_cfg)
      fprintf(stderr, "reading command definition from: %s\n", args->commands_cfg );
    if (args->definition_cfg)
      fprintf(stderr, "reading register definition from: %s\n", args->definition_cfg );
}




long get_frame_gap( const struct cli_args *args )
{
    long bits = 1 + args->bits + ((args->parity == 'N') ? 0 : 1) + args->stop_bits;
    long bits_per_3dot5 = (35 * bits) / 10;
    long millisec = (bits_per_3dot5 * 1000)/(args->speed);
    return millisec;
}


long get_elapsed_time( const struct timespec t0, const struct timespec t1 )
{
    long delta_ms;
    delta_ms = 1000 * (t1.tv_sec - t0.tv_sec);
    delta_ms += (t1.tv_nsec - t0.tv_nsec) / 1000000L;
    return delta_ms;
}

long get_shortest_time( long delta_ms )
{
    static long shortest_so_far = __LONG_MAX__;
    if (delta_ms>0 && delta_ms<shortest_so_far ) {
        shortest_so_far=delta_ms;
    }
    return shortest_so_far;
}

long get_longest_time( long delta_ms )
{
    static long longest_so_far = 0;
    if (delta_ms>0 && delta_ms>longest_so_far ) {
        longest_so_far=delta_ms;
    }
    return longest_so_far;
}

long get_average_time( long delta_ms )
{
    static long long summup=0;
    static unsigned long long count = 0;
    static int loga=0;

    if (count==__LONG_LONG_MAX__) { // switch to logarithmic average, because overflow
        if (!loga) {
            summup = summup/count;
            loga=1;
        }
        summup = (summup + delta_ms)/2;
        return summup;
    } else { // normal average
        summup += delta_ms;
        count++;
        return summup / count;
    }
}

void print_timestamp( const struct timespec t0, const struct timespec t1, int as_delta )
{
    long milliseconds = 0; //full milliseconds, can be multiple seconds or more
    if (as_delta==1) {
        milliseconds = get_elapsed_time(t0,t1);
    } else if (as_delta==0) {
        milliseconds = t1.tv_sec * 1000L + (t1.tv_nsec+500000L) / 1000000L;
    } else { // with clock
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char calendar[64];
        strftime(calendar, sizeof(calendar), "%Y-%m-%d %H:%M:%S", tm);
        calendar[ sizeof(calendar)-1 ]=0;
        fprintf(stderr, "%s,%06lu: ", calendar, ((t1.tv_nsec+500) / 1000ul));  // uSec
        return;
    }
    fprintf(stderr, "%08ld: ", milliseconds%1000);
}




void signal_handler(int) // handler for SIGUSR1: just create a new trace file
{
    rotate_log = 1;
}

void dump_buffer(uint8_t *buffer, uint16_t length, const char* prefix_txt)
{
	int i;
	if(prefix_txt) {
		fprintf(stderr, "%s: ", prefix_txt);
    }
	for (i=0; i < length; i++) {
		fprintf(stderr, " %02X", (uint8_t)buffer[i]);
	}
	fprintf(stderr, "\n");
}


int broken_answer( uint8_t *answer, uint16_t answer_length, uint8_t *request, uint16_t request_length )
{
    if (request_length<8 || answer_length<3) {
        return 0;
    }
    // ChINT Hoymiles bug: [2C] 03 20 06 00 2C [A9 AB] -->  [2C] 03 58 45 13 80 00 45 14 00 00 45 13 [B0 00]
    uint8_t bad_req1[] = { 0x03, 0x20, 0x06, 0x00, 0x2C };
    uint8_t bad_ans1[] = { 0x03, 0x03, 0x58, 0x45, 0x13, 0x80, 0x00, 0x45, 0x14, 0x00, 0x00, 0x45, 0x13};
    if (0==memcmp(request+1,bad_req1,request_length-3) && 0==memcmp(answer+1,bad_ans1,answer_length-3) ) {
      return 1;
    }
    return 0;
}


#define DECODE_DONE_WELL 0
#define DECODE_NEEDS_DATA 1
#define DECODE_HAS_DATA_LEFT -1
#define DECODE_DIRECTION_WRONG -2
int decode_buffer(uint8_t *buffer, uint16_t length,
                  uint8_t *prev_buf, uint16_t prev_length,
                  const CommandNames_t& CommandsByNum,
                  const RegisterDefinition_t& RegistersByNum,
                  int& isAnswer,
                  uint16_t& LastRegNum,
                  size_t& Remaining)
{
    uint16_t length_original=length;
    uint16_t BytesAnswered=0, RegCount=0;
    size_t Idx=0;
    ModbusCommand_t Cmd;
    ModbusRegister_t Reg;

	fprintf(stderr, "\tDECODE: ");

    /* === Block A, common for Request and Answer === */
    /* --- Block A.1: the ID of the client being asked for or answering back --- */
    if (length>=1) {
      uint8_t ID = buffer[Idx];
      fprintf(stderr, "%c ID: %02u (0x%02x), ", (isAnswer)?'!':'?', ID, ID );
      length--;Idx++;
    }

    /* --- Block A.2: the Modbus Command --- */
    if (length>=1) {
      uint8_t Command = buffer[Idx];
      const auto found = CommandsByNum.find(Command);
      if (CommandsByNum.end() != found) {
        Cmd = found->second;
        fprintf(stderr, "%s, ", Cmd.Name.c_str());
      } else {
        fprintf(stderr, "Cmd_%02X, ", Command);
      }
      length--;Idx++;
    }

    /* === Block B, Request-Type Packets === */
    if (!isAnswer) { // <-- REQUEST PACKAGE
      /* --- Block B.1: the (first) register we want to read --- */
        if (length>=2) {
          uint8_t RegHi = buffer[Idx];
          uint8_t RegLo = buffer[Idx+1];
          LastRegNum = (RegHi << 8) + RegLo;
          const auto found = RegistersByNum.find(LastRegNum);
          if (RegistersByNum.end() != found) {
            Reg = found->second;
            fprintf(stderr, "%s, ", Reg.Name.c_str());
          } else {
            fprintf(stderr, "Reg%04X, ", LastRegNum);
          }
          length-=2;Idx+=2;
        }
        /* --- Block B.2: the amount of 16-bit register we want to read --- */
        if (length>=2) {
          uint8_t CntHi = buffer[Idx];
          uint8_t CntLo = buffer[Idx+1];
          RegCount = (CntHi << 8) + CntLo;
          BytesAnswered = 2 * RegCount;
          if( RegCount <= Cmd.maxAtOnce ) {
            fprintf(stderr, "%u Registers (%u Bytes), ", RegCount, BytesAnswered);
          } else {
            fprintf(stderr, "invalid attempt to request %u Registers (%u Bytes). ", RegCount, BytesAnswered);
          }
          length-=2;Idx+=2;
        }
    }
    /* === Block C, Answer-Type Packets === */
    else if (isAnswer) {
        /* --- Block C.1: the amount of Bytes (2* 16-Bit-Registers) the message contains --- */
        if (length>=1) {
          uint8_t Bytes = buffer[Idx];
          BytesAnswered = Bytes;
          if (0==BytesAnswered) { // really seen such package, but it was a second request, not an answer, so the decoder was wrong here and need retry
            fprintf(stderr, "couldn't be an answer, try request decoding instead\n");
            return DECODE_DIRECTION_WRONG;
          }
          // tentative crc validation to see, if this could be an complete packet already
          fprintf(stderr, "check plausibility of length %u/%u\n", BytesAnswered, length);
          if (BytesAnswered>length) {
              int crc_ok = crc_check(buffer, length_original, 0);
              if ( broken_answer( buffer, length_original, prev_buf, prev_length ) && crc_ok ) {
                fprintf(stderr, "couldn't be right length %u, setting to %u\n", BytesAnswered, length_original);
                // code to come
                BytesAnswered = length-2;
              }
          }
          RegCount = BytesAnswered / 2;
          fprintf(stderr, "%u Bytes, ", BytesAnswered);
          length--;Idx++;
        }
        /* --- Block C.2: the 1..RegCount Register(s) content the message contains --- */
        if (length>=BytesAnswered) {
          // RegCount times dump...
      for(uint16_t RegNo = LastRegNum; RegNo < LastRegNum + RegCount; RegNo++) {
            const auto found = RegistersByNum.find(RegNo);
            if (RegistersByNum.end() != found) {
              Reg = found->second;
              fprintf(stderr, "%s: ", found->second.Name.c_str());
            } else {
              fprintf(stderr, "Reg%04X: ", RegNo);
            }

            if( Reg.len>length-2 ) {
              fprintf(stderr, "invalid attempt to dump %u Bytes, have only %u. ", static_cast<uint16_t>(BytesAnswered), length);
            }

            if (Reg.DataType==std::string("void")) {
            }
            else if (Reg.DataType==std::string("dump")) {
              for (int i=0; i < Reg.len; i++) {
                fprintf(stderr, "%02X ", (uint8_t)buffer[Idx+i]);
              }
            }
            else if (Reg.DataType==std::string("bit")) {
              fprintf(stderr, "%c ", (buffer[Idx]>0) ? '1' : '0');
            }
            else if (Reg.DataType==std::string("bits")) {
              for (int i=0; i < (7+Reg.len)/8; i++) {
                uint8_t Byte = buffer[Idx+i];
                for (int b=0; i < 8; b++) {
                  fprintf(stderr, "%c", ((Byte) & (1<<b)) ? '1' : '0' );
                }
              }
            }
            else if (Reg.DataType==std::string("uint8_t")) {
              uint8_t Byte = static_cast<uint8_t>(buffer[Idx]);
              fprintf(stderr, "{%u} ", static_cast<uint16_t>(Byte) );
            }
            else if (Reg.DataType==std::string("int8_t")) {
              int8_t Byte = static_cast<int8_t>(buffer[Idx]);
              fprintf(stderr, "{%d} ", static_cast<int16_t>(Byte) );
            }
            else if (Reg.DataType==std::string("uint16_t")) {
              uint8_t ValHi = buffer[Idx];
              uint8_t ValLo = buffer[Idx+1];
              uint16_t Value = (ValHi << 8) + ValLo;
              fprintf(stderr, "{%u} ", Value );
            }
            else if (Reg.DataType==std::string("int16_t")) {
              uint8_t ValHi = buffer[Idx];
              uint8_t ValLo = buffer[Idx+1];
              uint16_t Value = (ValHi << 8) + ValLo;
              fprintf(stderr, "{%d} ", static_cast<int16_t>(Value) );
            }
            else if (Reg.DataType==std::string("uint32_t")) {
              uint8_t ValA = buffer[Idx];
              uint8_t ValB = buffer[Idx+1];
              uint8_t ValC = buffer[Idx+2];
              uint8_t ValD = buffer[Idx+3];
              uint32_t Value = (ValA << 24) + (ValB << 16) + (ValC << 8) + ValD;
              fprintf(stderr, "{%ul} ", Value );
            }
            else if (Reg.DataType==std::string("int32_t")) {
              uint8_t ValA = buffer[Idx];
              uint8_t ValB = buffer[Idx+1];
              uint8_t ValC = buffer[Idx+2];
              uint8_t ValD = buffer[Idx+3];
              uint32_t Value = (ValA << 24) + (ValB << 16) + (ValC << 8) + ValD;
              fprintf(stderr, "{%d} ", static_cast<int32_t>(Value) );
            }
            else if (Reg.DataType==std::string("float")) {
              uint8_t ValA = buffer[Idx];
              uint8_t ValB = buffer[Idx+1];
              uint8_t ValC = buffer[Idx+2];
              uint8_t ValD = buffer[Idx+3];
              uint32_t Value = (ValA << 24) + (ValB << 16) + (ValC << 8) + ValD;
              float *pFloat = reinterpret_cast<float*>(&Value);
              fprintf(stderr, "{%f} ", *pFloat );
            }
            if (RegNo < LastRegNum + RegCount) {
              fprintf(stderr, "\n\t" );
            }
            length -= Reg.len;
            Idx += Reg.len;
          }
        } else {
          fprintf(stderr, "[????] incomplete! need:%u, had %u\n", BytesAnswered, length);
          Remaining = BytesAnswered - length;
          return DECODE_NEEDS_DATA; // failed a bit
        }
    }

    /* === Block D, Closing Checksum === */
    if (length>=2) {
      uint8_t CrcHi = buffer[Idx];
      uint8_t CrcLo = buffer[Idx+1];
      uint16_t CRC = (CrcHi << 8) + CrcLo;
      fprintf(stderr, "[%04X]\n", CRC);

      // Answer Package done, next is response? Or vvs?
      isAnswer = (isAnswer) ? 0 : 1;
      length-=2;Idx++;

      if(length>0) {
        Remaining=length;
        return DECODE_HAS_DATA_LEFT; // perhaps next packet connected?
      }
      Remaining=0;
      return DECODE_DONE_WELL; // all fine
    }

    fprintf(stderr, "[????] incomplete? had:%u, needed more.\n", length);
    Remaining=length;
    return DECODE_NEEDS_DATA; // failed a bit
}


int main( int argc, char **argv )
{
  try
  {
    struct cli_args args = {};
    int n_bytes = -1, res, n_packets = 0;
    size_t size = 0, size_prev = 0;
    uint8_t buffer[MODBUS_MAX_PACKET_SIZE];
    uint8_t buffer_prev[MODBUS_MAX_PACKET_SIZE];

#   if !defined( __MINGW32__ ) && !defined( __MINGW64__ )
    signal( SIGUSR1, signal_handler ); // not emulated on MinGW, not existing in Windows
#   endif

    parse_args(argc, argv, &args);

    fprintf(stderr, "starting modbus sniffer\n");

    CommandNames_t CommandsByNum;
    if (args.commands_cfg) {
      fprintf(stderr, "reading %s: ", args.commands_cfg );
      CommandsByNum = read_ModbusCommands( args.commands_cfg );
      fprintf(stderr, "OK\n" );
    } else {
      fprintf(stderr, "  no command decoding wanted.\n");
    }

    RegisterDefinition_t RegistersByNum;
    if (args.definition_cfg) {
      fprintf(stderr, "reading %s: ", args.definition_cfg );
      RegistersByNum = read_ModbusRegisterDefinitions( args.definition_cfg );
      fprintf(stderr, "OK\n" );
    } else {
      fprintf(stderr, "  no register decoding wanted.\n" );
    }

    TtyUart port;
    try
    {
      port.open( args.serial_port );
      port.configure( args.speed, args.bits, args.parity, args.stop_bits, args.low_latency );
    } catch(...)
    {
      DIE("open port");
    }


    int isAnswer=0, Loops=0;
    uint16_t LastRegNum=0;
    size_t Remaining=0;
    int crc_ok=0, new_data=0;
    uint16_t crc;
    long elapsed_ms=0, shortest_ms=0, longest_ms=0, avrg_ms=0;
    struct timespec t0, t1;
    clock_gettime(CLOCK_REALTIME, &t0);
    int decode_res = DECODE_NEEDS_DATA;

    long modbus_gap_ms = get_frame_gap( &args );

    PcapWriter pcap( args.output_file );

    while (n_bytes != 0) {
        if (rotate_log || !pcap.is_open()) {
            pcap.close();
            pcap.open( args.output_file );
            rotate_log = 0;
        }

        new_data=0;

        try
        {
          res = port.wait( args.bytes_time_interval_us );
        }
        catch(...)
        {
          DIE("select");
        };

        if (res > 0)
        {
          /* there is something to read... if more than 32 Byte and using an USB/FTDI dongle, you'll likely get 32 byte chunks :-( */
          try
          {
            n_bytes = port.read( buffer + size, MODBUS_MAX_PACKET_SIZE - size );
          }
          catch(...)
          {
            DIE("read port");
          };

          size += n_bytes;
          if (n_bytes > 0) {
              new_data=1;
              clock_gettime(CLOCK_REALTIME, &t1);
              elapsed_ms = get_elapsed_time( t0, t1 );
              shortest_ms=get_shortest_time( elapsed_ms );
              longest_ms =get_longest_time( elapsed_ms );
              avrg_ms    =get_average_time( elapsed_ms );

              print_timestamp( t0, t1, -1 );
              fprintf(stderr, "captured packet %d: len=%zu, t_min=%ld, t=%ld, t_max=%ld, t_avg=%ld, req=%ld\n"
                            , n_packets, size
                            , shortest_ms, elapsed_ms, longest_ms, avrg_ms
                            , modbus_gap_ms );
              t0 = t1;
          }
        }

        if (DECODE_NEEDS_DATA == decode_res && !new_data) {
            continue;
        }

        /* captured an entire (???) packet */
//this is wrong!
//      if (size > 0 && (res == 0 || size >= MODBUS_MAX_PACKET_SIZE || n_bytes == 0)) {
        if (size > 0) {
            ++n_packets;

            print_timestamp( t0, t1, -1 );
            fprintf(stderr, "GOT new block - size=%zu, res=%d, n_bytes=%d\n", size, res, n_bytes );

            if (n_packets % args.max_packet_per_capture == 0)
                rotate_log = 1;

            dump_buffer(buffer, size, "\tREAD");

            decode_res = decode_buffer( buffer, size, buffer_prev, size_prev, CommandsByNum, RegistersByNum, isAnswer, LastRegNum, Remaining );
            if (DECODE_NEEDS_DATA == decode_res) {
                // Remaining could say, how much is missing
                fprintf(stderr, "DECODE_NEEDS_DATA length = %zu, had = %zu\n", Remaining, size);
                continue;
            }

            if (DECODE_DIRECTION_WRONG == decode_res && Loops < 4) {
                Loops++;
                isAnswer = (isAnswer) ? 0 : 1;
                fprintf(stderr, "DECODE_DIRECTION_WRONG, try decoding as %s instead\n", (isAnswer)?"answer":"request" );
                continue;
            }
            Loops=0;

            // Here we have: (DECODE_HAS_DATA_LEFT==decode_res) || (DECODE_DONE_WELL==decode_res)
            // Remaining tells, how much is over (likely parts of next package)

            if (Remaining) {
                fprintf(stderr, "\tDECODE_HAS_DATA_LEFT length = %zu\n", Remaining);
            }

            size_t eaten = size - Remaining;
            crc_ok = crc_check(buffer, eaten, &crc);
            fprintf(stderr, "CRC: %04X = %02X%02X [%s]\n", crc, buffer[eaten-2] & 0xFF, buffer[eaten-1] & 0xFF, crc_ok ? "OK" : "FAIL");
            if (crc_ok) {
                size_prev = eaten;
                memcpy(buffer_prev,buffer,eaten);
            }
            if (crc_ok || args.ignore_crc) {
                /* was not able to decode? then at least dump it */
                dump_buffer(buffer, eaten, "\tDONE");
                //dump_buffer(buffer+eaten, Remaining, "\tNEXT");
            }

            pcap.write_packet( buffer, eaten );

            if (DECODE_HAS_DATA_LEFT == decode_res) {
                fprintf(stderr, "\tDECODE_HAS_DATA_LEFT length = %zu of %zu, move <- %zu to buffer start\n", Remaining, size, eaten);
                memmove(buffer,buffer+eaten,Remaining);
                size = Remaining;
            } else {
                size = 0;
            }
        } else { // end-if of buffer timed out (unreliable on some hardware) || buffer is too full.
            print_timestamp( t0, t1, -1 );
            fprintf(stderr, "NO new block - size=%zu, res=%d, n_bytes=%d\n", size, res, n_bytes );
        }
    } // while nothing got read
  }
  catch(std::exception &e)
  {
    fprintf(stderr, "Some unexpected things happened: %s\n", e.what());
    exit(EXIT_FAILURE);
  }
  catch(...)
  {
    fprintf(stderr, "Something unknown got wrong. Could not see, what.\n");
    exit(EXIT_FAILURE);
  }
  return EXIT_SUCCESS;
}
