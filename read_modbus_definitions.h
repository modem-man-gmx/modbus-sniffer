#ifndef READ_MODBUS_DEFINITIONS_H
#define READ_MODBUS_DEFINITIONS_H

#include <map>
#include <string>
#include <exception>

struct ModbusRegister_t
{
  uint16_t Register;
  uint16_t len; // len of Answer in Bytes (a typical register is always WORD, so len=2
  std::string Orientation; // ABCD is big endian 32 bit, AB is big endian 16 bit, DCBA is little Endian 32 bit a.s.o.
  std::string DataType; // of (void, dump, bit, bits, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, float)
  std::string Unit; // physical unit of the answer, if not converted to preferred Unit
  double FactorToPrefUnit; // multiply Value with this to get PrefUnit
  std::string PrefUnit; // preferred Unit for displaying numerical values
  std::string Name;
  std::string Description;
};


struct ModbusCommand_t
{
  uint8_t CommandNb;
  std::string Name;
  uint16_t maxAtOnce;
  uint16_t minAddr;
  uint16_t maxAddr;
  std::string Description;
};

typedef std::map<uint16_t /*RegisterNb*/, ModbusRegister_t /*record*/> RegisterDefinition_t;
typedef std::map<uint8_t /*CommandByte*/, ModbusCommand_t /*record*/> CommandNames_t;

RegisterDefinition_t read_ModbusRegisterDefinitions(const char* inputfile);
CommandNames_t read_ModbusCommands(const char* inputfile);

#endif // READ_MODBUS_DEFINITIONS_H
