//
// Copyright (c) 2020 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the MIT License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef JAY_J1939_HEADER_H
#define JAY_J1939_HEADER_H

#pragma once

//C++
#include <cassert>
#include <cstdint>
#include <algorithm>

//Libraries
#include "canary/frame_header.hpp"

namespace jay
{
  ///TODO: Place in own header?

  //Address Related consts
  constexpr uint8_t ADDRESS_GLOBAL{0xFFU};
  constexpr uint8_t ADDRESS_NULL{0xFEU};

  constexpr uint8_t J1939_ADDRESS_CLAIM{0xA0U};
  constexpr uint8_t J1939_ADDRESS_REQUEST{0xA1U};

  constexpr uint8_t PF_ADDRESS_CLAIM{0xEEU};
  constexpr uint32_t PGN_ADDRESS_CLAIM{0xEE'FFU};

  constexpr uint8_t PF_REQUEST{0xEAU};
  constexpr uint32_t PGN_REQUEST_ADDRESS{0xEA'FEU};

  constexpr uint8_t PF_ACKNOWLEDGE{0xE8U};
  constexpr uint32_t PGN_ACKNOWLEDGE{0xE8'FFU};

/**
 * Class that for manipulating a J1939 frame
 * Class is a wrapper around canary frame_header class
 * 
 * Remember order is [31] ... [0] this is not an array where 0 is the first index
 * First three bits are EFF/RTR/ERR flags then followed by 29-bit message
 * Frame format flag (EFF)          = [31] 1-bit, 0 = stardard 11-bit, 1 = extended 29-bit, always 1 in j1939 
 * Remote transmition request (RTR) = [30] 1-bit, 1 indicates rtr frame with no data, always 0 in j1939
 * Error frame (ERR)                = [29] 1-bit, indicates if the frame is an error frame or not, always 0 in j1939
 * 
 * 
 * Priority (Prio)      = [28] ... [26] 3 bit, indicating priority of j1939 frame
 * Reserved Bit (R)     = [25]          1 bit, Reserved bit for future use 
 * Data Page (DP)       = [24]          1 bit, data page bit used to extend PDU numbers
 * PDU format (PF)      = [23] ... [16] 8 -bit, if between 0-239 then frame is p2p and PS is destination adress, 
 *                                              if between 240-255 then frame is broadcast and PS is group extension
 * PDU specific (PS)    = [15] ... [8]  8 -bit, is destination address or group extention
 * Source Address (SA)  = [7]  ... [0]  8 -bit, address of sending device
 * 
 * Parameter Group Number (PGN) - Bits [25] ... [8], consists of R, DP, PF, PS
 * Protocol Data Unit (PDU) - Consists of PF and PS
 * 
 * Example: 
 * (Tags):   --- 	Prio 	R 	DP 	PF 	      PS 	      SA
 * (Hex) :   0x0C 	            0xF0 	    0x04 	    0xEE
 * (Binary): 000 	011 	0 	0 	11110000 	00000100 	11101110
 * 
*/
class frame_header
{
public:

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        Construction                            @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * Default construction of empty frame
  */
  frame_header() : header_() 
  {
    //Allways the case with j1939 headers
    //Make sure that any other constructors invoke this constructor
    header_.extended_format(true);
    header_.error(false);
    header_.remote_transmission(false);
  }

 /**
  * Construct a j1939 using defined values
  * @param priority of the frame, is only 3 bits so can have values from 0...7
  * A value of 0 is highest prioritys.
  * @param data_page bit expands the PDU address by adding another bit of possible addresses
  * @param pdu_format (PF) @see pdu_format for more info
  * @param pdu_specific (PS) @see pdu_specific for more info
  * @param source_address of the frame
  */
  frame_header(const uint8_t priority, const bool data_page, 
    const uint8_t pdu_format, const uint8_t pdu_specific, 
    const uint8_t source_address, const std::size_t payload = 0) : 
    frame_header(
      priority,
      (static_cast<uint32_t>(data_page)  << 16) |
      (static_cast<uint32_t>(pdu_format) << 8)  |
      (static_cast<uint32_t>(pdu_specific)) ,
      source_address, payload
    )
  {
  }

  /**
   * @param priority of the frame, is only 3 bits so can have values from 0...7
   * A value of 0 is highest prioritys.
   * @param pgn (Parameter group number (PGN)) - 18 bit that contains
   * reserved bit - data page bit - PF - PS
   * @param source_address of the frame
  */
  frame_header(const uint8_t priority, const uint32_t pgn, 
    const uint8_t source_address, const std::size_t payload = 0) : 
  frame_header(
      (std::clamp(static_cast<uint32_t>(priority), 0U, 7U) << 26) |
      (pgn << 8)  | source_address, payload)
  {

  }
  
  /**
   * @param header_data containing the 29 bits that j1939 structures use
  */
  frame_header(const uint32_t header_data, const uint8_t payload = 0) : frame_header()
  {
    header_.id(header_data);
    header_.payload_length(payload);
  }

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        State                                   @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * Check if PDU format is broadcast or peer-to-peer
   * This function does not take into cosideration the data page bit
   * @return true if broadcast, false if peer-to-peer
   */
  bool is_broadcast() const
  {
    return pdu_format() > 0xEF;
  }

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        Set Functions                           @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * Sets the J1939 ID of this frame. J1939 IDs are 29-bit integers.
  */
  frame_header& id(const std::uint32_t id)
  {
    header_.id(id);
    return *this;
  }

  /**
   * Set the priority of the j1939 frame
   * @param priority, ranging from 0 highest and 7 lowest
   * @note function will clamp priority to a value between 0 - 7
  */
  frame_header& priority(const uint8_t priority)
  {
    //Use mask to only get the last 3 bytes, thereby clamping value between 0 - 7
    //priority_ = priority & static_cast<uint8_t>(0b111);
    //return *this;

    id( (id() & ~prio_mask) | (static_cast<uint32_t>(priority) << 26));
    return *this;
  }

  /**
   * Set the data page bit to 0 or 1, the bit expands the PDU address by adding another bit of possible addresses
   * @param data_page_bit between 0 and 1, is clamped if needed.
  */
  frame_header& data_page(const bool data_page_bit)
  {
    //data_page_bit_ = (data_page_bit) ? 1 : 0;
    id( (id() & ~data_page_mask) | (static_cast<uint32_t>(data_page_bit ? 1 : 0) << 24));
    return *this;
  }

  /**
  * @param pdu_format (PF), if the pdu format is 0 - 239 then the frame
  * is peer-to-peer communication and the PS is the destination adress.
  * If PF is 240-255 then the frame is broadcasted and PS is a PSU group extension.
  * SAE and Proprietary usage:
  * 0x00 − 0xEE: Peer-to-Peer messages defined by SAE
  * 0xEF – 0xEF: Peer-to-Peer message for proprietary use
  * 0xF0 – 0xFE: Broadcast messages defined by SAE
  * 0xFF – 0xFF: Broadcast messages for proprietary use
  */
  frame_header& pdu_format(const uint8_t pdu_format)
  {
    id( (id() & ~pf_mask) | (static_cast<uint32_t>(pdu_format) << 16));
    return *this;
  }

  /**
  * @param pdu_specific (PS) is either the destination address if the PDU format (PF) is 
  * a peer-to-peer massage or a PSU group extention if the PF is a broadcast message.
  */
  frame_header& pdu_specific(const uint8_t pdu_specific)
  {
    id( (id() & ~ps_mask) | (static_cast<uint32_t>(pdu_specific) << 8));
    return *this;
  }

  /**
   * Set the source address of the can frame.
   * @param source_address of the frame 
   */
  frame_header& source_adderess(const uint8_t source_address)
  {
    id( (id() & ~ad_mask) | static_cast<uint32_t>(source_address));
    return *this;
  }

  /**
   * Set length of the data that is assiociated with the header
   * @param source_address of the frame 
   */
  frame_header& payload_length(const std::size_t length)
  {
    header_.payload_length(length);
    return *this;
  }

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                        Get Functions                           @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /// Gets the CAN ID of this frame. CAN IDs are 29-bit integers for
  /// extended-format frames and 11-bit for standard-format frames.
  /// \returns The CAN ID of this frame.
  /**
   * Returns 
  */
  std::uint32_t id() const noexcept
  {
    return header_.id();
  }

  /**
   * Get the priority of the j1939 frame
   * @return priority of frame, 0 highest and 7 lowest
  */
  uint8_t priority() const noexcept
  {
    return static_cast<uint8_t>((header_.id() & prio_mask) >> 26);
  }

  /**
   * Get the data page bit
   * @return data page bit, 0 if not set, 1 if set
  */
  uint8_t data_page() const noexcept
  {
    return static_cast<uint8_t>((header_.id() & data_page_mask) >> 24);
  }

  /**
  * @brief Get the 18-bit parameter group number (PGN) of the header
  * the PGN consists of reserved bit, data page, pdu format(pf) and pdu specific (ps)
  * @return 18 - bit, paramter group number 
  */
  uint32_t pgn() const noexcept
  {
    return static_cast<uint32_t>((header_.id() & pgn_mask) >> 8);
  }

  /**
   * Get the PDU Format data of the J1939 frame.
   * @return 8bit PDU Format 
  */
  uint8_t pdu_format() const noexcept
  {
    return static_cast<uint8_t>(header_.id() >> 16); //Return bits 23 - 16, third byte
  }

  /**
   * Get the PDU Specifier data of the J1939 frame.
   * In the case that the message is peer-to-peer the PS will contain
   * the destination address of the frame
   * @return 8bit PDU Specifier
  */
  uint8_t pdu_specific() const noexcept
  {
    return static_cast<uint8_t>(header_.id() >> 8); //Return bits 15 - 8, second byte
  }

  /**
   * Get the source address of the frame
   * @return 8 bit soruce address 
  */
  uint8_t source_adderess() const noexcept
  {
    return static_cast<uint8_t>(header_.id()); //Return bits 7 - 0, first byte
  }

  /**
   * Get length of the data that is assiociated with the header
   * @param source_address of the frame 
   */
  std::size_t payload_length() const noexcept
  {
    return header_.payload_length();
  }

  ///TODO: Add a tuple return for getting each component
  ///TODO: Add a tuple for getting PDU components
  ///TODO: Add return for PGN & PDU
  ///TODO: Check if IsBroadcast is correct

private:

  static constexpr std::uint32_t prio_mask =      0x1C'00'00'00U;
  static constexpr std::uint32_t data_page_mask = 0x01'00'00'00U;
  static constexpr std::uint32_t pgn_mask =       0x03'FF'FF'00U;
  static constexpr std::uint32_t pf_mask =        0x00'FF'00'00U;
  static constexpr std::uint32_t ps_mask =        0x00'00'FF'00U;
  static constexpr std::uint32_t ad_mask =        0x00'00'00'FFU;

  canary::frame_header header_{};
};

static_assert(sizeof(frame_header) == sizeof(std::uint64_t),
              "Size of frame header must be exactly 8 bytes");

} // namespace jay


#endif