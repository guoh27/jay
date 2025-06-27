//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no), 2025 Hong.Guo (hong.guo@advantech.com.cn)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/guoh27/jay
//
#pragma once

// C++
#include <cstdint>//std::uint8_t, std::uint32_t, std::uint64_t

namespace jay {
/*
 * Consts that are not included by linux j1939.h but are usefull to have
 */

/**
 * Max pdu format (pf) a addressable message can have
 * higher means that the message can only be broadcast (PDU2)
 * and the PS field contains a Group Extension.
 */
constexpr std::uint8_t PF_PDU1_MAX{ 0xEFU };

// Address Related consts
constexpr std::uint8_t PF_ADDRESS_CLAIM{ 0xEEU };
constexpr std::uint8_t PF_REQUEST{ 0xEAU };
constexpr std::uint8_t PF_ACKNOWLEDGE{ 0xE8U };

/* J1939 Priority
 *
 * bit 0-2	: Priority (P)
 * bit 3-7	: set to zero
 */
typedef std::uint8_t priority_t;

/**
 * @brief J1939 Address
 * 0-253 are usable addresses, 254 is idle address and 255 is broadcast address
 *
 */
typedef std::uint8_t address_t;

/* J1939 NAME
 *
 * bit 0-20	: Identity Number
 * bit 21-31	: Manufacturer Code
 * bit 32-34	: ECU Instance
 * bit 35-39	: Function Instance
 * bit 40-47	: Function
 * bit 48	: Reserved
 * bit 49-55	: Vehicle System
 * bit 56-59	: Vehicle System Instance
 * bit 60-62	: Industry Group
 * bit 63	: Arbitrary Address Capable
 */
typedef std::uint64_t name_t;

/* J1939 Parameter Group Number
 *
 * bit 0-7	: PDU Specific (PS)
 * bit 8-15	: PDU Format (PF)
 * bit 16	: Data Page (DP)
 * bit 17	: Reserved (R)
 * bit 19-31	: set to zero
 */
typedef std::uint32_t pgn_t;

/*
 * Highest usable unique addresses (253), a total of 254
 * addresses can exist in a network 0 - 253
 */
constexpr jay::address_t J1939_MAX_UNICAST_ADDR{ 0xFDU };

/*
 * Idle or null address
 */
constexpr jay::address_t J1939_IDLE_ADDR{ 0xFEU };

/*
 * Broadcast / global or no address
 */
constexpr jay::address_t J1939_NO_ADDR{ 0xFFU };

/*
 *
 */
constexpr name_t J1939_NO_NAME{ 0UL };


/*
 * Request PG, between 0 and 239 is addressable, PS will contain target address or broadcast.
 * Would use J1939_PGN_PDU1_MAX on incoming message before comparing to check if is J1939_PGN_REQUEST
 * Then check what the PS value is by inverting ~J1939_PGN_PDU1_MAX
 */
constexpr pgn_t J1939_PGN_REQUEST{ 0x0EA00U };

/*
 * Address Claimed PGN, between 0 and 239 so its addressable, PS will contain target address or broadcast
 * Would use J1939_PGN_PDU1_MAX on incoming message before comparing to check if is J1939_PGN_ADDRESS_CLAIMED
 * Then check what the PS value is by inverting ~J1939_PGN_PDU1_MAX
 */
constexpr pgn_t J1939_PGN_ADDRESS_CLAIMED{ 0x0EE00U };

/* Commanded Address */
constexpr pgn_t J1939_PGN_ADDRESS_COMMANDED{ 0x0FED8U };

/// TODO: Implement commanded address functionality

/*
 * In other documentation PDU1 indicates that the message is addressable, though the value for this could be used as
 * a mask for reserved, data page and pdu format (PF). So if you wanted a mask for those you could use this.
 */
constexpr pgn_t J1939_PGN_PDU1_MAX{ 0x3FF00U };

/*
 * PGN max value and mask
 */
constexpr pgn_t J1939_PGN_MAX{ 0x3FFFFU };

}// namespace jay
