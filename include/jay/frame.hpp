//
// Copyright (c) 2020 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the MIT License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef JAY_J1939_FRAME_H
#define JAY_J1939_FRAME_H

#pragma once

//C++
#include <array>
#include <string>
#include <sstream>

//Libraries
#include "header.hpp"

//Linux
#include <linux/can.h>

namespace jay
{

  using payload = std::array<std::uint8_t, 8>;

/**
 * 
*/
struct frame
{

  /**
   * 
  */
  frame() = default;

  /**
   * 
  */
  frame(const frame_header& in_header, const payload& in_payload) :
    header(in_header), payload(in_payload)
  {
    //Cant use this as size will allways be 8 and that will not match
    //All usecases
    //header.SetPayloadLength(in_payload.size());
  }


  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        Frame Archetypes                        @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * Create an address request j1939 frame
   * Used to get the address of devices on the network
   * @return address request j1939 frame
  */
  static frame make_address_request()
  {
    return { frame_header(static_cast<uint8_t>(6), false, PF_REQUEST, 
      ADDRESS_GLOBAL, ADDRESS_NULL, 3), {0x00, 0xEE, 0x00} };
  }
  
  /**
   * Create an address claim j1939 frame
   * @param name of the device
   * @param address to claim
   * @return address claim j1939 frame
  */
  static frame make_address_claim(uint8_t address, payload name)
  {
    ///TODO: Replace payload with name type
    return {frame_header(static_cast<uint8_t>(6), false, PF_ADDRESS_CLAIM, 
      ADDRESS_GLOBAL, address, 8) , name};
  }

  static frame make_cannot_claim(payload name)
  {
    ///TODO: Replace payload with name type
    return {frame_header(static_cast<uint8_t>(6), false, PF_ADDRESS_CLAIM, 
      ADDRESS_GLOBAL, ADDRESS_NULL, 8) , name};
  }

  ///TODO: Look into cmmanded address command

  ///TODO: Create statics for other known frame types


  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        Frame Conversion                        @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * Converts object to string
   * @return struct as string
  */
  std::string to_string() const
  {
    std::stringstream ss{};
    ss << std::hex << header.id() << ":";
    for(auto byte : payload)
    {
      ///Use + as uint8 is alias for char, the ouput is writen as sybols instead of number
      ss << std::hex << static_cast<std::uint32_t>(byte) << "'";
    }
    return ss.str();
  }


  /**
   *TODO: Not needed?
   */
  static can_frame to_can(frame j1939_frame)
  {
    can_frame frame{};
    std::memcpy(&frame, &j1939_frame, sizeof(j1939_frame));
    return std::move(frame);
  }

  ///TODO: Make into class and make sure that payload size change is updated in header

  frame_header header{};
  jay::payload payload{};
};

} // namespace jay


#endif