//
// Copyright (c) 2020 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the MIT License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#include <gtest/gtest.h>

#include "../include/jay/frame.hpp"

//Lib
#include "canary/raw.hpp"
#include <canary/interface_index.hpp>

//Linux
#include <linux/can.h>

TEST(Jay_Frame_Test, Jay_Frame_Create_Test)
{
  ///TODO: Find some way to get init array size?
  jay::frame f{jay::frame_header{7, true, 0xAF, 0xFF, 0x02, 2}, {0xFF, 0x00}};

  ::can_frame cf{};
  static_assert(sizeof(f) == sizeof(cf), "Frame header layout must match native can_frame struct.");

  ASSERT_EQ(f.header.id(), 0x1DAFFF02);
  ASSERT_EQ(f.header.payload_length(), 2);
  ASSERT_EQ(f.payload.size(), 8);
  ASSERT_EQ(f.payload[0], 0xFF);
  ASSERT_EQ(f.payload[1], 0x00);
}

TEST(Jay_Frame_Test, Jay_Frame_Sync_Send_Test)
{
  canary::net::io_context ctx{1};
  canary::raw::socket sock1{ctx, canary::raw::endpoint{canary::get_interface_index("vcan0")}};
  canary::raw::socket sock2{ctx, canary::raw::endpoint{canary::get_interface_index("vcan0")}};

  jay::frame out_frame{{3, false, 0xBB, 0xFE, 0xFE, 8}, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
  sock1.send(canary::net::buffer(&out_frame, sizeof(out_frame)));

  jay::frame in_frame{};
  sock2.receive(canary::net::buffer(&in_frame, sizeof(in_frame)));

  ASSERT_EQ(in_frame.header.id(), out_frame.header.id());
  ASSERT_EQ(in_frame.header.payload_length(), out_frame.header.payload_length());

  for(unsigned int i = 0; i < in_frame.header.payload_length(); i++)
  {
    ASSERT_EQ(in_frame.payload[i], out_frame.payload[i]);
  }
}

TEST(Jay_Frame_Test, Jay_Frame_Sync_SendTo_Test)
{
  // Use vcan1 so that we can tell apart a default constructed endpoint and
  // one filled by receive_from()

  canary::net::io_context ctx{1};
  canary::raw::socket sock1{ctx, canary::raw::endpoint{canary::get_interface_index("vcan1")}};
  canary::raw::socket sock2{ctx, canary::raw::endpoint{canary::get_interface_index("vcan1")}};

  canary::raw::endpoint const ep1{canary::get_interface_index("vcan1")};
  canary::raw::endpoint ep2;

  jay::frame out_frame{{3, false, 0xBB, 0xFE, 0xFE, 8}, {0xFA, 0xFA, 0xFA, 0xFA, 0xFF, 0xFF, 0xFF, 0xFF}};
  sock1.send_to(canary::net::buffer(&out_frame, sizeof(out_frame)), ep1);

  jay::frame in_frame{};
  sock2.receive_from(canary::net::buffer(&in_frame, sizeof(in_frame)), ep2);

  ASSERT_EQ(in_frame.header.id(), out_frame.header.id());
  ASSERT_EQ(in_frame.header.payload_length(), out_frame.header.payload_length());

  for(unsigned int i = 0; i < in_frame.header.payload_length(); i++)
  {
    ASSERT_EQ(in_frame.payload[i], out_frame.payload[i]);
  }
  
  ASSERT_EQ(ep1.interface_index(), ep2.interface_index());
}
