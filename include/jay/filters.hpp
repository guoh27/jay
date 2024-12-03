#ifndef JAY_FILTERS_HPP
#define JAY_FILTERS_HPP

#pragma once

// C++

// Libraries
#include <canary/filter.hpp>

// Local
#include "j1939.hpp"

namespace jay {

[[nodiscard]] inline canary::filter make_address_request_filter() noexcept
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
[[nodiscard]] inline canary::filter make_address_claim_filter() noexcept
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
#endif