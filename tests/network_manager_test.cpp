//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#include <gtest/gtest.h>

#include "../include/jay/network_manager.hpp"

//C++
#include <iostream>
#include <queue>
#include <chrono>

class NetworkManagerTest : public testing::Test
{
protected:

  // 
  NetworkManagerTest()
  {
    ///Outputs used for debugging
    net_mng.set_callback([this](auto name, auto address) ->void {
      new_controller_queue.push({name, address});
    });
  }

  // 
  virtual ~NetworkManagerTest()
  {
  }

  // 
  virtual void SetUp() override
  {
    //frame_queue = std::queue<jay::frame>{};
  }

  // 
  virtual void TearDown() override
  {
    j1939_network.clear();

  }

public:
  std::queue<std::pair<jay::name, std::uint8_t>> new_controller_queue{};

  jay::network j1939_network{"vcan0"};
  jay::network_manager net_mng{j1939_network};

};

TEST_F(NetworkManagerTest, Jay_Network_Manager_Test)
{

  boost::asio::io_context context;
  std::queue<jay::frame> frame_queue{};

  jay::address_manager address_one{context, {0xAFFU}, j1939_network,
    jay::address_manager::callbacks{
      [](jay::name /*name*/, std::uint8_t /*address*/) -> void 
      { //On address
        //std::cout << "Controller claimed: " << std::hex << 
        //  static_cast<uint64_t>(name) << ", address: " << std::hex << static_cast<uint64_t>(address) << std::endl;
      },
      [](jay::name /*name*/) -> void 
      { //Lost address
        //std::cout << "Controller Lost:" << std::hex << static_cast<uint64_t>(name) << std::endl;
      },
      [&frame_queue](jay::frame frame) -> void 
      { //On output frame
        //std::cout << "Sending: " << frame.to_string() << std::endl;
        frame_queue.push(frame);
      },
      [](std::string what, std::string error) -> void 
      { //ON error
        std::cout << what << " : " << error << std::endl;
      }}
  };

  jay::address_manager address_two{context, {0xBFFU}, j1939_network,
    jay::address_manager::callbacks{
      [](jay::name /*name*/, std::uint8_t /*address*/) -> void 
      { //On address
        //std::cout << "Controller claimed: " << std::hex << 
        //  static_cast<uint64_t>(name) << ", address: " << std::hex << static_cast<uint64_t>(address) << std::endl;
      },
      [](jay::name /*name*/) -> void 
      { //Lost address
        //std::cout << "Controller Lost:" << std::hex << static_cast<uint64_t>(name) << std::endl;
      },
      [&frame_queue](jay::frame frame) -> void 
      { //On output frame
        //std::cout << "Sending: " << frame.to_string() << std::endl;
        frame_queue.push(frame);
      },
      [](std::string what, std::string error) -> void 
      { //ON error
        std::cout << what << " : " << error << std::endl;
      }}
  };

  ///TODO: Have address manager get access to name, so dont need to insert
  ///it aswell, also add if check to insert

  net_mng.insert(address_one);
  net_mng.insert(address_two);

  ASSERT_EQ(net_mng.size(), 2);
  
  //Request address
  net_mng.process(jay::frame::make_address_request());

  context.run_for(std::chrono::milliseconds(260)); //Enought time for timeout to trigger
  context.restart();

  //Frame queue should contain 2 cannot claim address messages
  ASSERT_EQ(frame_queue.size(), 2);

  auto frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), J1939_NO_ADDR);
  ASSERT_EQ(frame.header.source_adderess(), J1939_IDLE_ADDR);
  frame_queue.pop();

  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), J1939_NO_ADDR);
  ASSERT_EQ(frame.header.source_adderess(), J1939_IDLE_ADDR);
  frame_queue.pop();

  address_one.start_address_claim(0x0U);
  address_two.start_address_claim(0x1U);

  context.run_for(std::chrono::milliseconds(300)); //Enought time for timeout to trigger
  context.restart();

  ASSERT_EQ(frame_queue.size(), 2);
  frame_queue.pop();
  frame_queue.pop();

  for(std::uint8_t i = 0; i < 50; i++)
  {
    //Should insert into network and require current devices to change name
    net_mng.process(jay::frame::make_address_claim(jay::name{i}, i));
    context.run_for(std::chrono::milliseconds(300)); //Enought time for timeout to trigger
    context.restart();

    //A new controller is added in callback
    ASSERT_EQ(new_controller_queue.size(), 1);
    auto new_controller = new_controller_queue.front();
    ASSERT_EQ(new_controller.first, jay::name{static_cast<std::uint64_t>(i)});
    ASSERT_EQ(new_controller.second, i);
    new_controller_queue.pop();

    //One of our controllers lose an address
    ASSERT_EQ(frame_queue.size(), 1);
    frame_queue.pop();
  }

  ///TODO: Fill network?

}