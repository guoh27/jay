//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef JAY_J1939_NAME_H
#define JAY_J1939_NAME_H

#pragma once

//C++
#include <array>

//Local
#include "j1939.hpp" // name_t , cstdint

namespace jay
{

/**
 * @brief Class for manipulating J1939 industry names belonging to
 * an electronic control unit (ecu)
 * 
 * Remember order is [63] ... [0] this is not an array where 0 is the first index
 * Bytes are also ordered [0] ... [7]
 * 
 * Self config. address = [63] ... [63] 1 bit, byte  #7
 * Industry group       = [62] ... [60] 3 bit, byte  #7
 * Device class instace = [59] ... [56] 4 bit, byte  #7
 * Device class         = [55] ... [49] 7 bit, byte  #6
 * Reserved             = [48] ... [48] 1 bit, byte  #6 
 * Function             = [47] ... [40] 8 bit, byte  #5
 * Function instance    = [39] ... [35] 5 bit, byte  #4
 * ECU instance         = [34] ... [32] 3 bit, byte  #4
 * Manufacturer Code    = [31] ... [21] 11 bit, bytes #3 - 2
 * Identity Number      = [20] ... [0]  21 bit, bytes #2 - 0
 * 
 * Example: 
 * (Tags):   SCA   IG   DCI   DC        R    F         FI     ECUI    MC              IN
 * (Hex) :   0x1   0x2  0x0   0x6 	    0x0  0x81	     0x1    0x4     0x2D1           0x0
 * (Binary): 1     010  0000  0000110   0    10000001  00001  100 	  01011010001     000000000000000000000
 * 
 * @todo Should it be called device_name, ecu_name or something like that
*/
class name
{
public:
  
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        Construction                            @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * @brief Default consturctor, creates an empty ecu name
  */
  name() = default;

  /**
   * @brief Constuct name from it all its elements
   * @param id_num, identity number 21 bit
   * @param man_code, manufacturer Code 11 bit
   * @param ecu_inst, ecu instance 3 bit
   * @param func_inst, function instance 5 bit
   * @param func, function 8 bit
   * @param dev_cls, device class / vehicle system 7 bit
   * @param dev_cls_inst, device class instace / vehicle system instance 4 bit
   * @param ind_grp, industry group 3 bit
   * @param slf_cfg_addr, self configuring address, 1 bit
  */
  name(std::uint32_t id_num, std::uint16_t man_code, std::uint8_t ecu_inst,
    std::uint8_t func_inst, std::uint8_t func, std::uint8_t dev_cls, 
    std::uint8_t dev_cls_inst, std::uint8_t ind_grp, std::uint8_t slf_cfg_addr) :
    name_(
      ((static_cast<name_t>(slf_cfg_addr) << slf_cfg_addr_start_bit ) & slf_cfg_addr_mask) |
      ((static_cast<name_t>(ind_grp     ) << ind_grp_start_bit      ) & ind_grp_mask)      |
      ((static_cast<name_t>(dev_cls_inst) << dev_cls_inst_start_bit ) & dev_cls_inst_mask) |
      ((static_cast<name_t>(dev_cls     ) << dev_cls_start_bit      ) & dev_cls_mask)      |
      ((static_cast<name_t>(func        ) << func_start_bit         ) & func_mask)         |
      ((static_cast<name_t>(func_inst   ) << func_inst_start_bit    ) & func_inst_mask)    |
      ((static_cast<name_t>(ecu_inst    ) << ecu_inst_start_bit     ) & ecu_inst_mask)     |
      ((static_cast<name_t>(man_code    ) << man_code_start_bit     ) & man_code_mask)     |
      ((static_cast<name_t>(id_num      ) << id_num_start_bit       ) & id_num_mask)
    )
    ///NOTE: Dont need to set reserved bit as it defaults to 0
  {}
/*
      ((static_cast<name_t>(slf_cfg_addr) << slf_cfg_addr_start_bit) & slf_cfg_addr_mask) |
      ((static_cast<name_t>(ind_grp)      & ind_grp_mask)      << ind_grp_start_bit)      |
      ((static_cast<name_t>(dev_cls_inst) & dev_cls_inst_mask) << dev_cls_inst_start_bit) |
      ((static_cast<name_t>(dev_cls)      & dev_cls_mask)      << dev_cls_start_bit)      |
      ((static_cast<name_t>(func)         & func_mask)         << func_start_bit)         |
      ((static_cast<name_t>(func_inst)    & func_inst_mask)    << func_inst_start_bit)    |
      ((static_cast<name_t>(ecu_inst)     & ecu_inst_mask)     << ecu_inst_start_bit)     |
      ((static_cast<name_t>(man_code)     & man_code_mask)     << man_code_start_bit)     |
      ((static_cast<name_t>(id_num)       & id_num_mask)       << id_num_start_bit)
*/
  /**
   * Constuct name from existing name
   * @param name to construct from
  */
  name(const name_t name) : name_(name)
  {}


  /**
  * Construct name from j1939 frame payload
  * @param payload containing name
  */
  name(const std::array<uint8_t, 8>& payload) : name_()
  {
    name_ |= static_cast<name_t>(payload[0]);
    name_ |= static_cast<name_t>(payload[1]) << 8;
    name_ |= static_cast<name_t>(payload[2]) << 16;
    name_ |= static_cast<name_t>(payload[3]) << 24;
    name_ |= static_cast<name_t>(payload[4]) << 32;
    name_ |= static_cast<name_t>(payload[5]) << 40;
    name_ |= static_cast<name_t>(payload[6]) << 48;
    name_ |= static_cast<name_t>(payload[7]) << 56;
  }

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        Set Functions                           @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * Set the Identity Number of the j1939 device name
   * @param id_num - 21 bit Identity Number
   * @return reference to self
  */
  name& identity_number(const std::uint32_t id_num) noexcept
  {
    //             Combine
    // Clear existing | ((place bits correctly) & remove extra bits)
    name_ = (name_ & ~id_num_mask) | 
      (static_cast<name_t>(id_num << id_num_start_bit) & id_num_mask);
    return *this;
  }

  /**
   * Set the Manufacturer Code of the j1939 device name
   * @param man_code - 11 bit Manufacturer Code
   * @return reference to self
  */
  name& manufacturer_code(std::uint16_t man_code)
  {
    name_ = (name_ & ~man_code_mask) | 
      ((static_cast<name_t>(man_code) << man_code_start_bit) & man_code_mask);
    return *this;
  }

  /**
   * Set the ECU instance of the j1939 device name
   * @param ecu_inst - 3 bit ECU instance
   * @return reference to self
  */
  name& ecu_instance(std::uint8_t ecu_inst) noexcept
  {
    name_ = (name_ & ~ecu_inst_mask) | 
      ((static_cast<name_t>(ecu_inst) << ecu_inst_start_bit) & ecu_inst_mask);
    return *this;
  }

  /**
   * Set the Function instance of the j1939 device name
   * @param func_inst - 5 bit Function instance
   * @return reference to self
  */
  name& function_instance(std::uint8_t func_inst) noexcept
  {
    name_ = (name_ & ~func_inst_mask) | 
      ((static_cast<name_t>(func_inst) << func_inst_start_bit) & func_inst_mask);
    return *this;
  }

    /**
   * Set the Function of the j1939 device name
   * @param func - 8 bit Function
   * @return reference to self
  */
  name& function(std::uint8_t func) noexcept
  {
    name_ = (name_ & ~func_mask) | 
      ((static_cast<name_t>(func) << func_start_bit) & func_mask);
    return *this;
  }

  /**
   * Set the Device class of the j1939 device name
   * @param dev_cls - 7 bit Device class
   * @return reference to self
  */
  name& device_class(std::uint8_t dev_cls) noexcept
  {
    name_ = (name_ & ~dev_cls_mask) | 
      ((static_cast<name_t>(dev_cls) << dev_cls_start_bit) & dev_cls_mask);
    return *this;
  }

  /**
   * Set the Device class instace of the j1939 device name
   * @param dev_cls_inst - 4 bit Device class instace
   * @return reference to self
  */
  name& device_class_instace(std::uint8_t dev_cls_inst) noexcept
  {
    name_ = (name_ & ~dev_cls_inst_mask) | 
      ((static_cast<name_t>(dev_cls_inst) << dev_cls_inst_start_bit) & dev_cls_inst_mask);
    return *this;
  }

  /**
   * Set the Industry group  of the j1939 device name
   * @param ind_grp - 3 bit Industry group
   * @return reference to self
  */
  name& industry_group(std::uint8_t ind_grp) noexcept
  {
    name_ = (name_ & ~ind_grp_mask) | 
      ((static_cast<name_t>(ind_grp) << ind_grp_start_bit) & ind_grp_mask);
    return *this;
  }

  /**
   * Set the Self config. address of the j1939 device name
   * @param slf_cfg_addr - 1 bit Manufacturer Self config. address
   * @return reference to self
  */
  name& self_config_address(std::uint8_t slf_cfg_addr) noexcept
  {
    name_ = (name_ & ~slf_cfg_addr_mask) | 
      ((static_cast<name_t>(slf_cfg_addr) << slf_cfg_addr_start_bit) & slf_cfg_addr_mask);
    return *this;
  }


  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        Get Functions                           @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * Get the Identity Number of the j1939 device name
   * @return Identity Number, 21-bits
  */
  std::uint32_t identity_number() const noexcept
  {
    return static_cast<std::uint32_t>(name_ & id_num_mask);
  }

  /**
   * Get the Manufacturer Code of the j1939 device name
   * @return Manufacturer Code, 11-bits
  */
  std::uint16_t manufacturer_code() const noexcept
  {
    return static_cast<std::uint16_t>((name_ & man_code_mask) >> man_code_start_bit);
  }

  /**
   * Get the ECU instance of the j1939 device name
   * @return ECU instance, 3-bits
  */
  std::uint8_t ecu_instance() const noexcept
  {
    return static_cast<std::uint8_t>((name_ & ecu_inst_mask) >> ecu_inst_start_bit);
  }

  /**
   * Get the Function instance of the j1939 device name
   * @return Function instance, 5-bits
  */
  std::uint8_t function_instance() const noexcept
  {
    return static_cast<std::uint8_t>((name_ & func_inst_mask) >> func_inst_start_bit);
  }

  /**
   * Get the Function of the j1939 device name
   * @return Function, 8-bits
  */
  std::uint8_t function() const noexcept
  {
    return static_cast<std::uint8_t>((name_ & func_mask) >> func_start_bit);
  }

  /**
   * Get the Device class of the j1939 device name
   * @return Device class, 7-bits
  */
  std::uint8_t device_class() const noexcept
  {
    return static_cast<std::uint8_t>((name_ & dev_cls_mask) >> dev_cls_start_bit);
  }

  /**
   * Get the Device class instace of the j1939 device name
   * @return Device class instace, 4-bits
  */
  std::uint8_t device_class_instace() const noexcept
  {
    return static_cast<std::uint8_t>((name_ & dev_cls_inst_mask) >> dev_cls_inst_start_bit);
  }

  /**
   * Get the Industry group  of the j1939 device name
   * @return Industry group , 3-bits
  */
  std::uint8_t industry_group () const noexcept
  {
    return static_cast<std::uint8_t>((name_ & ind_grp_mask) >> ind_grp_start_bit);
  }

  /**
   * Get the Self config. address of the j1939 device name
   * @return Self config. address, 1-bit
  */
  std::uint8_t self_config_address() const noexcept
  {
    return static_cast<std::uint8_t>((name_ & slf_cfg_addr_mask) >> slf_cfg_addr_start_bit);
  }

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                            Overloads                           @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  bool operator < ( const name& rhs ) const noexcept
  {
    return ( name_ < rhs.name_ );
  }

  bool operator > ( const name& rhs ) const noexcept
  {
    return ( name_ > rhs.name_ );
  }

  bool operator == ( const name& rhs ) const noexcept
  {
    return ( name_ == rhs.name_ );
  }

  bool operator < ( const name_t rhs ) const noexcept
  {
    return ( name_ < rhs);
  }

  bool operator > ( const name_t rhs ) const noexcept
  {
    return ( name_ > rhs);
  }

  bool operator == ( const name_t rhs ) const noexcept
  {
    return ( name_ == rhs );
  }

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        Hash Function                           @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * @brief Struct containing hash function
   */
  struct hash
  {
    size_t operator()(const jay::name& name) const
    {
      return std::hash<name_t>()(name.name_);
    }
  };

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        Data Convertion                         @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  //Dont require explicite static_cast when converting to int
  /**
  *  @brief Convert name to int value
  */
  operator name_t() const noexcept
  {
    return name_;
  }

  /**
  * @brief Convert name to array
  */
  operator std::array<std::uint8_t, 8>() const noexcept
  {
    return std::array<std::uint8_t, 8>{
      static_cast<std::uint8_t>(name_),       //#0
      static_cast<std::uint8_t>(name_ >> 8),  //#1
      static_cast<std::uint8_t>(name_ >> 16), //#2
      static_cast<std::uint8_t>(name_ >> 24), //#3
      static_cast<std::uint8_t>(name_ >> 32), //#4
      static_cast<std::uint8_t>(name_ >> 40), //#5
      static_cast<std::uint8_t>(name_ >> 48), //#6
      static_cast<std::uint8_t>(name_ >> 56), //#7
    };
  }

  ///TODO: Implement a to_string opterator?

private:

  static constexpr name_t slf_cfg_addr_mask =  0x80'00'00'00'00'00'00'00ULL;
  static constexpr name_t ind_grp_mask =       0x70'00'00'00'00'00'00'00ULL;
  static constexpr name_t dev_cls_inst_mask =  0x0F'00'00'00'00'00'00'00ULL;
  static constexpr name_t dev_cls_mask =       0x00'FE'00'00'00'00'00'00ULL;
  static constexpr name_t func_mask =          0x00'00'FF'00'00'00'00'00ULL;
  static constexpr name_t func_inst_mask =     0x00'00'00'F8'00'00'00'00ULL;
  static constexpr name_t ecu_inst_mask =      0x00'00'00'07'00'00'00'00ULL;
  static constexpr name_t man_code_mask =      0x00'00'00'00'FF'E0'00'00ULL;
  static constexpr name_t id_num_mask =        0x00'00'00'00'00'1F'FF'FFULL;

  static constexpr std::uint8_t slf_cfg_addr_start_bit =  63;
  static constexpr std::uint8_t ind_grp_start_bit =       60;
  static constexpr std::uint8_t dev_cls_inst_start_bit =  56;
  static constexpr std::uint8_t dev_cls_start_bit =       49;
  static constexpr std::uint8_t func_start_bit =          40;
  static constexpr std::uint8_t func_inst_start_bit =     35;
  static constexpr std::uint8_t ecu_inst_start_bit =      32;
  static constexpr std::uint8_t man_code_start_bit =      21;
  static constexpr std::uint8_t id_num_start_bit =        0;

  name_t name_{J1939_NO_NAME};
};

static_assert(sizeof(name) == sizeof(name_t),
              "Size of name must be exactly 8 bytes");

} // namespace jay

#endif