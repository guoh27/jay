//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/guoh27/jay
//

#include <gtest/gtest.h>

#include "jay/frame.hpp"

// Lib
#include "canary/raw.hpp"
#include <canary/interface_index.hpp>

TEST(Jay_TP_Test, Jay_TP_Test)
{
  /// TODO: Find some way to get init array size?
  jay::frame f{ jay::frame_header{ 7, true, 0xAF, 0xFF, 0x02, 2 }, { 0xFF, 0x00 } };
}
