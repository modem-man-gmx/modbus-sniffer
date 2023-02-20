CXXFLAGS=-O2 -std=c++11 -Wall -Wextra -pedantic
LDFLAGS=

INCSEARCH=-I.
LIBSEARCH=
LIB_LINK=

sniffer: sniffer.o read_modbus_definitions.o
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LIBSEARCH) $(LIB_LINK) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $(INCSEARCH) -c $<

clean:
	rm -f sniffer sniffer.o read_modbus_definitions.o
