//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/guoh27/jay
//

#include <gtest/gtest.h>

#include "jay/header.hpp"

TEST(Jay_Header_Test, Jay_Header_Getters_Tests)
{

  jay::frame_header header{ 7, true, 0xAF, 0xFF, 0x02, 1 };

  /// TODO: Make it possible to get the entire frame header
  /// TODO: Make sure that the extended error and rtr bits are set
  /// correctly for each constructor
  ASSERT_FALSE(header.is_broadcast());
  ASSERT_EQ(header.id(), 0x1D'AF'FF'02);
  ASSERT_EQ(header.priority(), 7);
  ASSERT_EQ(header.data_page(), true);
  ASSERT_EQ(header.pdu_format(), 0xAF);
  ASSERT_EQ(header.pdu_specific(), 0xFF);
  ASSERT_EQ(header.pgn(), 0x00'01'AF'00);
  ASSERT_EQ(header.source_address(), 0x02);
  ASSERT_EQ(header.payload_length(), 1);

  jay::frame_header header1{ 10, 0x0FAF0, 0x64, 5 };

  ASSERT_TRUE(header1.is_broadcast());
  ASSERT_EQ(header1.id(), 0x1C'FA'F0'64);
  ASSERT_EQ(header1.priority(), 7);
  ASSERT_EQ(header1.data_page(), false);
  ASSERT_EQ(header1.pdu_format(), 0xFA);
  ASSERT_EQ(header1.pdu_specific(), 0xF0);
  ASSERT_EQ(header1.pgn(), 0x00'00'FA'F0);
  ASSERT_EQ(header1.source_address(), 0x64);
  ASSERT_EQ(header1.payload_length(), 5);

  jay::frame_header header2{ 0xFD'FF'FF'FF };

  ASSERT_TRUE(header2.is_broadcast());
  ASSERT_EQ(header2.id(), 0x1D'FF'FF'FF);
  ASSERT_EQ(header2.priority(), 7);
  ASSERT_EQ(header2.data_page(), true);
  ASSERT_EQ(header2.pdu_format(), 0xFF);
  ASSERT_EQ(header2.pdu_specific(), 0xFF);
  ASSERT_EQ(header2.pgn(), 0x00'01'FF'FF);
  ASSERT_EQ(header2.source_address(), 0xFF);
  ASSERT_EQ(header2.payload_length(), 0);
}

TEST(Jay_Header_Test, Jay_Header_Setters_Tests)
{
  jay::frame_header header{ 7, true, 0xAF, 0xFF, 0x02, 1 };

  header.id(0x1D'E8'A5'01);
  ASSERT_EQ(0x1D'E8'A5'01, header.id());
  header.priority(1);
  ASSERT_EQ(1, header.priority());
  header.data_page(false);
  ASSERT_EQ(false, header.data_page());
  header.pdu_format(0x23);
  ASSERT_EQ(0x23, header.pdu_format());
  header.pdu_specific(0x28);
  ASSERT_EQ(0x28, header.pdu_specific());
  header.source_address(0xFF);
  ASSERT_EQ(0xFF, header.source_address());
  header.payload_length(8);
  ASSERT_EQ(8, header.payload_length());
}

TEST(Jay_Header_Test, Jay_Header_Other_Tests)
{
  /// TODO: Test if the extended error and rtr bits are set correctly
  jay::frame_header header{ 3, false, 0xBB, 0xFE, 0xFE, 8 };

  uint32_t source_address{ 100 };
  uint32_t destination_address{ 0x97 };

  header.source_address(source_address);
  header.pdu_specific(destination_address);

  ASSERT_EQ(0x0C'bb'97'64, header.id());
  ASSERT_EQ(0xBB, header.pdu_format());
  ASSERT_EQ(destination_address, header.pdu_specific());
  ASSERT_EQ(source_address, header.source_address());
}