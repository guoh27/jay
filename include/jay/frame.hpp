//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef JAY_J1939_FRAME_H
#define JAY_J1939_FRAME_H

#pragma once

// C++
#include <array>
#include <sstream>
#include <string>

// Libraries
#include "header.hpp"
#include "name.hpp"

// Linux
#include <linux/can.h>


///
/// TODO: Is this class really needed. Could just use a linux can_frame instead?
/// TODO: Only really need to read from the header data.
/// TODO: This class might generate confusion regarding payload size?
///

namespace jay {

using payload = std::array<std::uint8_t, 8>;

/**
 *
 */
struct frame
{

  /**
   * Constructor
   */
  frame() = default;

  /**
   * Constructor
   * @param in_header
   * @param in_payload
   */
  frame(const frame_header &in_header, const payload &in_payload) : header(in_header), payload(in_payload)
  {
    // Cant use this as size will always be 8 and that will not match
    // All usecases
    // header.SetPayloadLength(in_payload.size());
  }


  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        Frame Archetypes                        @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * Create an address request j1939 frame
   * Used to get the address of devices on the network
   * @note sets PS to NO_ADDR thereby requesting address of all devices
   * @return address request j1939 frame
   */
  static frame make_address_request() { return make_address_request(J1939_NO_ADDR); }

  /**
   * Create an address request j1939 frame
   * Used to get the address of a devices on the network
   * @param PS of the message, when not NO_ADDR request address claim
   * from a specific address
   * @return address request j1939 frame
   */
  static frame make_address_request(std::uint8_t PS)
  {
    return { frame_header(static_cast<std::uint8_t>(6), false, PF_REQUEST, PS, J1939_IDLE_ADDR, 3),
      { 0x00, 0xEE, 0x00 } };
  }

  /**
   * Create an address claim j1939 frame
   * @param name of the device
   * @param address to claim
   * @return address claim j1939 frame
   */
  static frame make_address_claim(jay::name name, std::uint8_t address)
  {
    /// TODO: Replace payload with name type
    return { frame_header(static_cast<std::uint8_t>(6), false, PF_ADDRESS_CLAIM, J1939_NO_ADDR, address, 8), name };
  }

  /**
   * @brief Creates a cannot claim frame
   * @param name of the device
   * @return address cannot claim j1939 frame
   */
  static frame make_cannot_claim(jay::name name)
  {
    /// TODO: Replace payload with name type
    return { frame_header(static_cast<std::uint8_t>(6), false, PF_ADDRESS_CLAIM, J1939_NO_ADDR, J1939_IDLE_ADDR, 8),
      name };
  }

  /// TODO: Look into commanded address command

  /// TODO: Create statics for other known frame types


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
    for (auto byte : payload) {
      /// Use + as uint8 is alias for char, the ouput is written as symbols instead of number
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

  /// TODO: Make into class and make sure that payload size change is updated in header

  frame_header header{};
  jay::payload payload{};
};

}// namespace jay


#endif