//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
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
  ASSERT_EQ(f.payload.size(), 8); //Would it be better if it was 2?
  ASSERT_EQ(f.payload[0], 0xFF);
  ASSERT_EQ(f.payload[1], 0x00);
  ///TODO: Would checking any further result in segmentation fault, or are there empty values here

  auto addr_req = jay::frame::make_address_request();
  ASSERT_TRUE(addr_req.header.is_request());
  ASSERT_EQ(addr_req.header.id(), 0x18'EA'FF'FE);
  ASSERT_EQ(addr_req.header.priority(), 6);
  ASSERT_EQ(addr_req.header.data_page(), false);
  ASSERT_EQ(addr_req.header.pdu_format(), jay::PF_REQUEST);
  ASSERT_EQ(addr_req.header.pdu_specific(), J1939_NO_ADDR);
  ASSERT_EQ(addr_req.header.pgn(), J1939_PGN_REQUEST | J1939_NO_ADDR);
  ASSERT_EQ(addr_req.header.source_adderess(), J1939_IDLE_ADDR);
  ASSERT_EQ(addr_req.header.payload_length(), 3);
  ASSERT_EQ(addr_req.payload[0], 0x00);
  ASSERT_EQ(addr_req.payload[1], 0xEE);
  ASSERT_EQ(addr_req.payload[2], 0x00);

  ///TODO: Add static asserts regarding less then 8 size on payload for make functions

  auto cant_claim = jay::frame::make_cannot_claim(jay::name{0x00'00'00'00'00'00'00'00});

  ASSERT_TRUE(cant_claim.header.is_claim());
  ASSERT_EQ(cant_claim.header.id(), 0x18'EE'FF'FE);
  ASSERT_EQ(cant_claim.header.priority(), 6);
  ASSERT_EQ(cant_claim.header.data_page(), false);
  ASSERT_EQ(cant_claim.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(cant_claim.header.pdu_specific(), J1939_NO_ADDR);
  ASSERT_EQ(cant_claim.header.pgn(), J1939_PGN_ADDRESS_CLAIMED | J1939_NO_ADDR);
  ASSERT_EQ(cant_claim.header.source_adderess(), J1939_IDLE_ADDR);
  ASSERT_EQ(cant_claim.header.payload_length(), 8);

  auto claim = jay::frame::make_address_claim(jay::name{00}, 0xAA);

  ASSERT_TRUE(claim.header.is_claim());
  ASSERT_EQ(claim.header.id(), 0x18'EE'FF'AA);
  ASSERT_EQ(claim.header.priority(), 6);
  ASSERT_EQ(claim.header.data_page(), false);
  ASSERT_EQ(claim.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(claim.header.pdu_specific(), J1939_NO_ADDR);
  ASSERT_EQ(claim.header.pgn(), J1939_PGN_ADDRESS_CLAIMED | J1939_NO_ADDR);
  ASSERT_EQ(claim.header.source_adderess(), 0xAA);
  ASSERT_EQ(claim.header.payload_length(), 8);
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

  jay::frame out_frame{{3, false, 0xBB, 0xFE, 0xFE, 7}, {0xFA, 0xFA, 0xFA, 0xFF, 0xFF, 0xFF, 0xFF}};
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
