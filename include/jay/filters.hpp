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

// Libraries
#include <canary/filter.hpp>

// Local
#include "j1939_type.hpp"

namespace jay {

inline canary::filter make_address_request_filter() noexcept
{
  canary::filter filter{};
  filter.id(J1939_PGN_REQUEST << 8);
  filter.id_mask(J1939_PGN_PDU1_MAX << 8);
  filter.remote_transmission(false);
  filter.extended_format(true);
  filter.negation(false);
  return filter;
}

/**
 * @brief
 *
 * @return canary::filter
 * @todo Add the mask to jay as a J1939_ID_PF_PDU1_MAX?
 */
inline canary::filter make_address_claim_filter() noexcept
{
  canary::filter filter{};
  filter.id(J1939_PGN_ADDRESS_CLAIMED << 8);
  filter.id_mask(J1939_PGN_PDU1_MAX << 8);
  filter.remote_transmission(false);
  filter.extended_format(true);
  filter.negation(false);
  return filter;
}
}// namespace jay
