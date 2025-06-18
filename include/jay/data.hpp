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
#include <sstream>
#include <string>
#include <vector>

#include "header.hpp"

namespace jay {

using data_payload = std::vector<std::uint8_t>;

/**
 *
 */
struct data
{

  /**
   * Constructor
   */
  data() = default;

  /**
   * Constructor
   * @param in_header
   * @param in_payload
   */
  data(const frame_header &in_header, const data_payload &in_payload) : header(in_header), payload(in_payload) {}

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

  frame_header header{};
  jay::data_payload payload{};
};

}// namespace jay
