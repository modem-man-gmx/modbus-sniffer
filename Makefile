.PHONY: all clean
CXXFLAGS=-g -O2 -std=c++11 -Wall -Wextra -pedantic
LDFLAGS=

INCSEARCH=-I.
LIBSEARCH=
LIB_LINK=

PROGRAM=sniffer
OBJECTS=sniffer.o read_modbus_definitions.o ttyuart.o pcap_writer.o

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBSEARCH) $(LIB_LINK) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $(INCSEARCH) -c $<

clean:
	rm -f $(PROGRAM) $(OBJECTS)
