//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/guoh27/jay
//

#include <gtest/gtest.h>

#include "jay/address_manager.hpp"

// C++
#include <chrono>
#include <iostream>
#include <queue>

class AddressManagerTest : public testing::Test
{
protected:
  //
  explicit AddressManagerTest()
  {

    j1939_network = std::make_shared<jay::network>("vcan0");
    addr_mng = new jay::address_manager(io, local_name, j1939_network);
    /// Outputs used for debugging
    addr_mng->on_frame([this](jay::frame frame) -> void {// On output frame
      // std::cout << "Sending: " << frame.to_string() << std::endl;
      frame_queue.push(frame);
    });
    addr_mng->on_error([](std::string what, auto error) -> void {// ON error
      std::cout << what << " : " << error.message() << std::endl;
    });
  }

  //
  ~AddressManagerTest() override { delete addr_mng; };

  //
  void SetUp() override
  {
    // frame_queue = std::queue<jay::frame>{};
  }

  //
  void TearDown() override { j1939_network->clear(); }

public:
  std::queue<jay::frame> frame_queue{};

  boost::asio::io_context io{};
  jay::name local_name{ 0xFF };
  std::shared_ptr<jay::network> j1939_network;
  jay::address_manager *addr_mng{};
};

TEST_F(AddressManagerTest, Jay_Address_Manager_Test)
{
  std::cout << "This test will take up to a min please be patient..." << std::endl;
  ASSERT_EQ(frame_queue.size(), 0);

  // Return cannot claim address
  addr_mng->address_request(jay::address_claimer::ev_address_request{});

  // Enought time for timeout event to trigger
  io.run_for(std::chrono::milliseconds(260));
  io.restart();

  ASSERT_EQ(frame_queue.size(), 1);

  // First frame is cannot claim because of request
  auto frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::J1939_NO_ADDR);
  ASSERT_EQ(frame.header.source_address(), jay::J1939_IDLE_ADDR);
  frame_queue.pop();

  // Does nothing as we have not started claiming address
  jay::name controller_1{ 0xa00c81045a20021b };
  std::uint8_t address_1{ 0x10U };
  addr_mng->address_claim(jay::address_claimer::ev_address_claim{ controller_1, address_1 });

  // Enough time for timeout event to trigger
  io.run_for(std::chrono::milliseconds(260));
  io.restart();

  // Should claim address 0x1
  std::uint8_t address_0{ 0x00U };
  addr_mng->start_address_claim(address_0);

  /// TODO: Test running start address claim again

  // Enough time for timeout event to trigger
  io.run_for(std::chrono::milliseconds(260));
  io.restart();

  ASSERT_EQ(frame_queue.size(), 1);

  /// Address claim frame
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::J1939_NO_ADDR);
  ASSERT_EQ(frame.header.source_address(), address_0);
  frame_queue.pop();

  /// Confirm name and address is registered in network
  ASSERT_TRUE(j1939_network->in_network(local_name));
  ASSERT_FALSE(j1939_network->available(address_0));
  ASSERT_EQ(j1939_network->get_address(local_name), address_0);

  // Should return address claim 1 frame
  addr_mng->address_request(jay::address_claimer::ev_address_request{});

  io.run_for(std::chrono::milliseconds(20));
  io.restart();

  // Check for requested address claim
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::J1939_NO_ADDR);
  ASSERT_EQ(frame.header.source_address(), address_0);
  frame_queue.pop();

  for (std::uint8_t i = 0; i < jay::J1939_MAX_UNICAST_ADDR; i++) {
    // Insert claim into network
    j1939_network->insert(i, i);

    // Conflicting claim should change to new address
    addr_mng->address_claim(jay::address_claimer::ev_address_claim{ jay::name{ i }, static_cast<std::uint8_t>(i) });

    io.run_for(std::chrono::milliseconds(260));// Give timeout time to trigger
    io.restart();

    ASSERT_EQ(frame_queue.size(), 1);
    frame = frame_queue.front();
    ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
    ASSERT_EQ(frame.header.pdu_specific(), jay::J1939_NO_ADDR);
    ASSERT_EQ(frame.header.source_address(), i + 1);
    frame_queue.pop();

    ASSERT_TRUE(j1939_network->in_network(local_name));
    ASSERT_EQ(j1939_network->get_address(local_name), i + 1);
  }

  // Insert claim into network
  j1939_network->insert(jay::J1939_MAX_UNICAST_ADDR, jay::J1939_MAX_UNICAST_ADDR);

  // Conflicting claim should change to new address
  addr_mng->address_claim(jay::address_claimer::ev_address_claim{
    jay::name{ jay::J1939_MAX_UNICAST_ADDR }, static_cast<std::uint8_t>(jay::J1939_MAX_UNICAST_ADDR) });

  io.run_for(std::chrono::milliseconds(260));// Give timeout time to trigger
  io.restart();

  ASSERT_TRUE(j1939_network->full());

  // Check cannot claim address frame
  ASSERT_EQ(frame_queue.size(), 1);
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::J1939_NO_ADDR);
  ASSERT_EQ(frame.header.source_address(), jay::J1939_IDLE_ADDR);
  frame_queue.pop();

  /// TODO: Test running start address claim again
}