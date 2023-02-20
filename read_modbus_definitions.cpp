#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>

#include "read_modbus_definitions.h"

inline std::string& ltrim(std::string& s, const char* t = " \t\n\r\f\v"); // trim from left
inline std::string& rtrim(std::string& s, const char* t = " \t\n\r\f\v"); // trim from right
inline std::string& trim(std::string& s, const char* t = " \t\n\r\f\v"); // trim from left & right

RegisterDefinition_t read_ModbusRegisterDefinitions( const char* inputfile )
{
  RegisterDefinition_t Result;

  std::fstream infile( inputfile, std::ios::in );
  if( !infile.is_open() )
  {
    throw std::runtime_error( "Input file not found!" );
  }

  unsigned int LineNb=0;
  std::string Line;
  while( getline( infile, Line ) )
  {
    ++LineNb;
    trim( Line );
    // skip commented-out lines
    if( 0==Line.find(';') || 0==Line.find('#') )
      continue;
    // warn illegal lines
    if( std::string::npos==Line.find(',') )
    {
      std::cerr << "invalid line #" << LineNb << ": " << Line << std::endl;
      continue;
    }

    std::istringstream csvStream( Line );
    ModbusRegister_t Fields;
    std::string Element;
    unsigned int ElementNb = 0;

    // read every element from the line that is seperated by commas and put it into the vector or strings
    while( getline( csvStream, Element, ',' ) )
    {
      trim( Element );
      switch( ElementNb++ )
      {
        case 0: /*uint16_t Register*/
          Fields.Register = std::stoi( Element, nullptr, 0 ); // 0 to allow decimal, octal and 0x hex writing
          break;
        case 1: /*uint16_t len*/
          Fields.len = std::stoi( Element, nullptr, 0 ); // 0 to allow decimal, octal and 0x hex writing
          break;
        case 2: /*std::string Orientation*/
          std::swap( Fields.Orientation, Element );
          break;
        case 3: /*std::string DataType*/
          std::swap( Fields.DataType, Element ); // of (void, dump, bit, bits, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, float)
          break;
        case 4: /*std::string Unit*/
          std::swap( Fields.Unit, Element );
          break;
        case 5: /*double FactorToPrefUnit*/
        {
          double val(0.0);
          try
          {
          if( !Element.empty() )
            val = std::stod( Element );
          } catch(...)
          {
            std::cerr << "invalid line #" << LineNb << ", token #" << ElementNb << ": " << Line << std::endl;
            std::cerr << "invalid line #" << LineNb << ", expecting double (floating point) value, got \"" << Element << "\" instead" << std::endl;
            continue;
          }
          Fields.FactorToPrefUnit = val;
        };break;
        case 6: /*std::string PrefUnit*/
          std::swap( Fields.PrefUnit, Element );
          break;
        case 7: /*std::string Name*/
          std::swap( Fields.Name, Element );
          break;
        case 8: /*std::string Description*/
          std::swap( Fields.Description, Element );
          break;
        default: // accidential delimitted freeform text, because it contained comma
          Fields.Description.append(", ");
          Fields.Description.append(Element);
          break;
      }
    }
    Result.emplace( std::make_pair( Fields.Register, Fields ) );
  }

  return Result;
}


CommandNames_t read_ModbusCommands(const char* inputfile)
{
  CommandNames_t Result;

  std::fstream infile( inputfile, std::ios::in );
  if( !infile.is_open() )
  {
    throw std::runtime_error( "Input file not found!" );
  }

  std::string Line;
  while( getline( infile, Line ) )
  {
    trim( Line );
    // skip commented-out lines
    if( 0==Line.find(';') || 0==Line.find('#') )
      continue;
    // warn illegal lines
    if( std::string::npos==Line.find(',') )
    {
      std::cerr << "invalid line: " << Line << std::endl;
      continue;
    }

    std::istringstream csvStream( Line );
    ModbusCommand_t Fields;
    std::string Element;
    unsigned int ElementNb = 0;

    // read every element from the line that is seperated by commas and put it into the vector or strings
    while( getline( csvStream, Element, ',' ) )
    {
      trim( Element );
      switch( ElementNb++ )
      {
        case 0: /*uint8_t CommandNb*/
          Fields.CommandNb = std::stoi( Element, nullptr, 0 ); // 0 to allow decimal, octal and 0x hex writing
          break;
        case 1: /*std::string Name*/
          std::swap( Fields.Name, Element );
          break;
        case 2: /*uint16_t maxAtOnce*/
          Fields.maxAtOnce = std::stoi( Element, nullptr, 0 ); // 0 to allow decimal, octal and 0x hex writing
          break;
        case 3: /*uint16_t minAddr - uint16_t maxAddr*/
        {
          std::istringstream RangeStream( Element );
          std::string Part;
          unsigned int PartNb=0;
          while( getline( RangeStream, Part, '-' ) )
          {
            trim( Part );
            switch( PartNb++ )
            {
              case 0: /*uint16_t minAddr*/
                Fields.minAddr = std::stoi( Part, nullptr, 0 ); // 0 to allow decimal, octal and 0x hex writing
                break;
              case 1: /*uint16_t maxAddr*/
                Fields.maxAddr = std::stoi( Part, nullptr, 0 ); // 0 to allow decimal, octal and 0x hex writing
                break;
              default: break;
            }
          };
        }; break;
        case 4: /*std::string Description*/
          std::swap( Fields.Description, Element );
          break;
        default: // accidential delimitted freeform text, because it contained comma
          Fields.Description.append(", ");
          Fields.Description.append(Element);
          break;
      }
    }
    Result.emplace( std::make_pair( Fields.CommandNb, Fields ) );
  }

  return Result;
}



// trim from left
inline std::string& ltrim(std::string& s, const char* t)
{
  s.erase(0, s.find_first_not_of(t));
  return s;
}

// trim from right
inline std::string& rtrim(std::string& s, const char* t)
{
  s.erase(s.find_last_not_of(t) + 1);
  return s;
}

// trim from left & right
inline std::string& trim(std::string& s, const char* t)
{
  return ltrim(rtrim(s, t), t);
}
