//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/guoh27/jay
//

#include <gtest/gtest.h>

#include "jay/network_manager.hpp"

// C++
#include <chrono>
#include <iostream>
#include <queue>

class NetworkManagerTest : public testing::Test
{
protected:
  //
  NetworkManagerTest()
  {

    j1939_network = std::make_shared<jay::network>("vcan0");
    net_mng = new jay::network_manager(j1939_network);
    /// Outputs used for debugging
    net_mng->set_callback([this](auto name, auto address) -> void { new_controller_queue.push({ name, address }); });
  }

  //
  ~NetworkManagerTest() override { delete net_mng; }

  //
  void SetUp() override
  {
    // frame_queue = std::queue<jay::frame>{};
  }

  //
  void TearDown() override { j1939_network->clear(); }

public:
  std::queue<std::pair<jay::name, std::uint8_t>> new_controller_queue{};

  std::shared_ptr<jay::network> j1939_network;
  jay::network_manager *net_mng{};
};

TEST_F(NetworkManagerTest, Jay_Network_Manager_Test)
{

  boost::asio::io_context context;
  std::queue<jay::frame> frame_queue{};

  jay::address_manager address_one{ context, { 0xAFFU }, j1939_network };

  address_one.on_frame([&frame_queue](jay::frame frame) -> void {// On output frame
    // std::cout << "Sending: " << frame.to_string() << std::endl;
    frame_queue.push(frame);
  });
  address_one.on_error([](std::string what, auto error) -> void {// ON error
    std::cout << what << " : " << error.message() << std::endl;
  });

  jay::address_manager address_two{ context, { 0xBFFU }, j1939_network };

  address_two.on_frame([&frame_queue](jay::frame frame) -> void {// On output frame
    // std::cout << "Sending: " << frame.to_string() << std::endl;
    frame_queue.push(frame);
  });
  address_two.on_error([](std::string what, auto error) -> void {// ON error
    std::cout << what << " : " << error.message() << std::endl;
  });

  /// TODO: Have address manager get access to name, so dont need to insert
  /// it as well, also add if check to insert

  net_mng->insert(address_one);
  net_mng->insert(address_two);

  ASSERT_EQ(net_mng->size(), 2);

  // Request address
  net_mng->process(jay::frame::make_address_request());

  context.run_for(std::chrono::milliseconds(260));// Enough time for timeout to trigger
  context.restart();

  // Frame queue should contain 2 cannot claim address messages
  ASSERT_EQ(frame_queue.size(), 2);

  auto frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::J1939_NO_ADDR);
  ASSERT_EQ(frame.header.source_address(), jay::J1939_IDLE_ADDR);
  frame_queue.pop();

  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::J1939_NO_ADDR);
  ASSERT_EQ(frame.header.source_address(), jay::J1939_IDLE_ADDR);
  frame_queue.pop();

  address_one.start_address_claim(0x0U);
  address_two.start_address_claim(0x1U);

  context.run_for(std::chrono::milliseconds(300));// Enough time for timeout to trigger
  context.restart();

  ASSERT_EQ(frame_queue.size(), 2);
  frame_queue.pop();
  frame_queue.pop();

  for (std::uint8_t i = 0; i < 50; i++) {
    // Should insert into network and require current devices to change name
    net_mng->process(jay::frame::make_address_claim(jay::name{ i }, i));
    context.run_for(std::chrono::milliseconds(300));// Enough time for timeout to trigger
    context.restart();

    // A new controller is added in callback
    ASSERT_EQ(new_controller_queue.size(), 1);
    auto new_controller = new_controller_queue.front();
    ASSERT_EQ(new_controller.first, jay::name{ static_cast<std::uint64_t>(i) });
    ASSERT_EQ(new_controller.second, i);
    new_controller_queue.pop();

    // One of our controllers lose an address
    ASSERT_EQ(frame_queue.size(), 1);
    frame_queue.pop();
  }

  /// TODO: Fill network?
}