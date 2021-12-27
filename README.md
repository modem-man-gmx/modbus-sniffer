# Modbus RTU sniffer

A sniffer for the Modbus RTU protocol.

This programs allows you to snif packets from a Modbus RTU serial
comunication and save them in a .pcap file that you can open with
a program like Wireshark.

## Usage

Compile the program with `make`. The only dependency is a C compiler
and a POSIX operating system.

You can specify the options with the command line:

```
Usage: ./sniffer [-h] [-o out_dir] [-p port] [-s speed]
                 [-P parity] [-S stop_bits] [-b bits]

 -o, --output-dir   directory where to save the output
 -p, --serial-port  serial port to use
 -s, --speed        serial port speed (default 9600)
 -b, --bits         number of bits (default 8)
 -P, --parity       parity to use (default 'N')
 -S, --stop-bits    stop bits to use (default 1)
```

By default files are saved in the output directory with filename in
the format `modbus_YYYY-mm-dd_HH:MM:SS.pcap`.

By sending to the program a `SIGUSR1` the capture is rotated, i.e. the pcap
file is closed and another one is initiated. By default a .pcap file contains
maximum 10000 entries: after that the log is rotate. You can tweak this parameter
by editing the `MAX_CAPTURE_FILE_PACKETS` in the source code.

To capture the packets, you need a standard RS485 to TTL serial converter.
I tested the capture on a Raspberry Pi model 3B+. If you also use
a Raspberry, make sure to enable the hardware UART for better performance
by disabling the Bluetooth interface.

## Note on FTDI FT232R and similar USB chips (vheat)

On using FTDI FT232RL in a simple sniffer mode, so trying to sniff requests and responses, 
there was no separation from the MODBUS PDUs. It looked a next req PDU started at the middle 
of a received response.
It turned out that this is from a collection timer in Linux kernel, which tries to optimize 
load of USB transfers. This is located at the usb-serial module and can be impacted by setting
the latency_timer parameter. This can be done by
root# echo 1 > /sys/bus/usb-serial/devices/ttyUSB<your number>/latency_timer
Default of Linux is 16 which means 16 ms collection/latency timer.
The optimization is Linux is fine for big data transfers, but distracting for small messages 
like MODBUS PDU.
To give the full use case: A Elli Wallbox Connect Pro (Electrical Vehicle Charger) with a built
in MID certified energy meter shall get followed by just sniffing the data from a far end (12 m 
cable, observe the 120 Ohm resistors for termination). This has a 19200 bd 8E1 format on the 
MODBUS link. So the 3.5 character pause between the MODBUS messages is somewhat  
11 bits / 19200 bd * 3.5 symbols -> 2 ms
There for the 1 ms latency_timer always hits a pause.
The default timer of 1.5 ms timeout for select in sniffer.c will expire now at least once between 
PDUs and so separate the PDUs.
Observation shows that this works pretty well. Other speeds and formats may need a different alignment.

The submitted sniffer.c implements a basic interpretation of some of the PDU types which are 
involved when powering on the wallbox (FW / Serial number) and doing the actual charging 
(accumulated Active Import). The energy meter build into the wallbox is an ABB B23-112-10P.

If Elli or EVBox guys read this,... As the MODBUS link is not secured towards this remote link it might 
be possible to fake measurements from the energy meter to the wallbox main board and so cause higher 
reimbursements for the charging. 
The box brings an accounting of charging by means of applying different RFID cards.
Think now the card with reimbursement would get an higher energy amount than the car with private cost.
Of course this would be fraud calling for prosecution... but it looks to be a lack by using the not 
secured MODBUS.
   
