#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>

using std::endl;
using std::string;

#include "read_modbus_definitions.h"

inline string& ltrim(string& s, const char* t = " \t\n\r\f\v"); // trim from left
inline string& rtrim(string& s, const char* t = " \t\n\r\f\v"); // trim from right
inline string& trim(string& s, const char* t = " \t\n\r\f\v"); // trim from left & right


RegisterDefinition_t read_ModbusRegisterDefinitions( const char* inputfile, std::ostream& warn /* default is std::cerr*/ )
{
  RegisterDefinition_t Result;

  std::fstream infile( inputfile, std::ios::in );
  if( !infile.is_open() )
  {
    throw std::runtime_error( "Input file not found!" );
  }

  unsigned int LineNb=0;
  string Line;
  while( getline( infile, Line ) )
  {
    ++LineNb;
    trim( Line );
    // skip commented-out lines
    if( 0==Line.find(';') || 0==Line.find('#') )
      continue;
    // warn illegal lines
    if( string::npos==Line.find(',') )
    {
      warn << "invalid line #" << LineNb << ": " << Line << endl;
      continue;
    }

    std::istringstream csvStream( Line );
    ModbusRegister_t Fields;
    string Element;
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
        case 2: /*string Orientation*/
          std::swap( Fields.Orientation, Element );
          break;
        case 3: /*string DataType*/
          std::swap( Fields.DataType, Element ); // of (void, dump, bit, bits, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, float)
          break;
        case 4: /*string Unit*/
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
            warn << "invalid line #" << LineNb << ", token #" << ElementNb << ": " << Line << endl;
            warn << "invalid line #" << LineNb << ", expecting double (floating point) value, got \"" << Element << "\" instead" << endl;
            continue;
          }
          Fields.FactorToPrefUnit = val;
        };break;
        case 6: /*string PrefUnit*/
          std::swap( Fields.PrefUnit, Element );
          break;
        case 7: /*string Name*/
          std::swap( Fields.Name, Element );
          break;
        case 8: /*string Description*/
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


CommandNames_t read_ModbusCommands( const char* inputfile, std::ostream& warn /* default is std::cerr*/ )
{
  CommandNames_t Result;

  std::fstream infile( inputfile, std::ios::in );
  if( !infile.is_open() )
  {
    throw std::runtime_error( "Input file not found!" );
  }

  string Line;
  while( getline( infile, Line ) )
  {
    trim( Line );
    // skip commented-out lines
    if( 0==Line.find(';') || 0==Line.find('#') )
      continue;
    // warn illegal lines
    if( string::npos==Line.find(',') )
    {
      warn << "invalid line: " << Line << endl;
      continue;
    }

    std::istringstream csvStream( Line );
    ModbusCommand_t Fields;
    string Element;
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
        case 1: /*string Name*/
          std::swap( Fields.Name, Element );
          break;
        case 2: /*uint16_t maxAtOnce*/
          Fields.maxAtOnce = std::stoi( Element, nullptr, 0 ); // 0 to allow decimal, octal and 0x hex writing
          break;
        case 3: /*uint16_t minAddr - uint16_t maxAddr*/
        {
          std::istringstream RangeStream( Element );
          string Part;
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
        case 4: /*string Description*/
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


// ToDo: perhaps disable this fuction by #ifdef TESTING or the like?
void dump_ModbusRegisterDefinitions( const RegisterDefinition_t& RecordsByRegnum, std::ostream& out /* default is std::cout*/ )
{
  for( const auto& Record : RecordsByRegnum )
  {
    out << std::setw(5) << Record.first <<  " => " << std::setw(5) << Record.second.Register
        << ", L=" << std::setw(3) << Record.second.len
        << ", " << std::setw(4) << Record.second.Orientation
        << ",(" << Record.second.DataType
        << "), " << Record.second.Unit;

    if( Record.second.FactorToPrefUnit != 0.0 )
    {
      out << " * " << std::fixed << std::setprecision(3) << Record.second.FactorToPrefUnit
          << "=> " << Record.second.PrefUnit;
    }

    out << ", Name:" << Record.second.Name;
    if( ! Record.second.Description.empty() )
    {
      out << ", Descr:" << Record.second.Description;
    }

    out << endl;
  }
  return;
} // end-of dump_ModbusRegisterDefinitions()


// ToDo: perhaps disable this fuction by #ifdef TESTING or the like?
void dump_ModbusCommands( const CommandNames_t& Commands, std::ostream& out /* default is std::cout*/ )
{
  for( const auto& Record : Commands )
  {
    auto Cmd = Record.second;
    if( Cmd.maxAtOnce > 0 )
    {
      out << std::setw(2) << static_cast<int>( Record.first ) << " => " << std::setw(2) << static_cast<int>( Cmd.CommandNb )
          << ", Max@Once=" << std::setw(4) << Cmd.maxAtOnce
          << ", from=" << std::setw(5) << Cmd.minAddr
          << ", to=" << std::setw(5) << Cmd.maxAddr
          << ", Name:" << Cmd.Name;

      if( ! Cmd.Description.empty() )
      {
        out << ", Descr:" << Cmd.Description;
      }
      out << endl;
    }
  }
  return;
} // end-of dump_ModbusCommands()


// trim from left
inline string& ltrim(string& s, const char* t)
{
  s.erase(0, s.find_first_not_of(t));
  return s;
}

// trim from right
inline string& rtrim(string& s, const char* t)
{
  s.erase(s.find_last_not_of(t) + 1);
  return s;
}

// trim from left & right
inline string& trim(string& s, const char* t)
{
  return ltrim(rtrim(s, t), t);
}
