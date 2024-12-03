//
// Copyright (c) 2024 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
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
/// TODO: Is this class realy needed. Could just use a linux can_frame instead?
/// TODO: Only realy need to read from the header data.
/// TODO: This class might genereate confusion regarding payload size?
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
    // Cant use this as size will allways be 8 and that will not match
    // All usecases
    // header.SetPayloadLength(in_payload.size());
  }


  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        Frame Archetypes                        @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * Create an address request j1939 frame
   * Used to get the the name address pairs on the network
   * @param pdu_specific default is J1939_NO_ADDR for global request, otherwise the address to request
   * the name of.
   * @return address request j1939 frame
   */
  [[nodiscard]] static frame make_address_request(const std::uint8_t pdu_specific = J1939_NO_ADDR) noexcept
  {
    return { frame_header(static_cast<std::uint8_t>(6), false, PF_REQUEST, pdu_specific, J1939_IDLE_ADDR, 3),
      { 0x00, 0xEE, 0x00 } };
  }

  /**
   * Create an address claim j1939 frame
   * @param name of the device
   * @param address to claim
   * @return address claim j1939 frame
   */
  [[nodiscard]] static frame make_address_claim(const jay::name name, const std::uint8_t address) noexcept
  {
    return { frame_header(static_cast<std::uint8_t>(6), false, PF_ADDRESS_CLAIM, J1939_NO_ADDR, address, 8), name };
  }

  /**
   * @brief Creates a cannot claim frame
   * @param name of the device
   * @return address cannot claim j1939 frame
   */
  [[nodiscard]] static frame make_cannot_claim(const jay::name name) noexcept
  {
    return { frame_header(static_cast<std::uint8_t>(6), false, PF_ADDRESS_CLAIM, J1939_NO_ADDR, J1939_IDLE_ADDR, 8),
      name };
  }

  /// TODO: Look into cmmanded address command

  /// TODO: Create statics for other known frame types


  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        Frame Conversion                        @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * Converts object to string
   * @return struct as string
   */
  [[nodiscard]] std::string to_string() const
  {
    std::stringstream ss{};
    ss << std::hex << header.id() << ":";
    for (auto byte : payload) {
      /// Use + as uint8 is alias for char, the ouput is writen as sybols instead of number
      ss << std::hex << static_cast<std::uint32_t>(byte) << "'";
    }
    return ss.str();
  }

  /**
   *TODO: Not needed?
   */
  [[nodiscard]] static can_frame to_can(frame j1939_frame)
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