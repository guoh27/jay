//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#include <gtest/gtest.h>

#include "../include/jay/address_manager.hpp"

//C++
#include <iostream>
#include <queue>
#include <chrono>

class AddressManagerTest : public testing::Test
{
protected:

  // 
  AddressManagerTest()
  {
    ///Outputs used for debugging
    addr_mng.set_callbacks(jay::address_manager::callbacks{
      [](auto /*name*/, auto /*address*/) -> void 
      { //On address
        //std::cout << "Controller claimed: " << std::hex << 
        // static_cast<uint64_t>(name) << ", address: " << std::hex << static_cast<uint64_t>(address) << std::endl;
      },
      [](jay::name /*name*/) -> void 
      { //Lost address
        //std::cout << "Controller Lost:" << std::hex << static_cast<uint64_t>(name) << std::endl;
      },
      [this](jay::frame frame) -> void 
      { //On output frame
        //std::cout << "Sending: " << frame.to_string() << std::endl;
        frame_queue.push(frame);
      },
      [](std::string what, auto error) -> void 
      { //ON error
        std::cout << what << " : " << error.message() << std::endl;
      }
  });
  }

  // 
  virtual ~AddressManagerTest()
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
  std::queue<jay::frame> frame_queue{};

  boost::asio::io_context io{};
  jay::name local_name {0xFF};
  jay::network j1939_network{"vcan0"};
  jay::address_manager addr_mng{io, local_name, j1939_network};

};

TEST_F(AddressManagerTest, Jay_Address_Manager_Test)
{
  std::cout << "This test will take up to a min please be patient..." << std::endl;
  ASSERT_EQ(frame_queue.size(), 0);

  //Return cannot claim address
  addr_mng.address_request(jay::address_claimer::ev_address_request{});

  //Enought time for timeout event to trigger
  io.run_for(std::chrono::milliseconds(260));
  io.restart();

  ASSERT_EQ(frame_queue.size(), 1);

  //First frame is cannot claim because of request
  auto frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), J1939_NO_ADDR);
  ASSERT_EQ(frame.header.source_adderess(), J1939_IDLE_ADDR);
  frame_queue.pop();

  //Does nothing as we have not started claiming address
  jay::name controller_1{0xa00c81045a20021b};
  std::uint8_t address_1{0x10U};
  addr_mng.address_claim(jay::address_claimer::ev_address_claim{controller_1, address_1});

  //Enought time for timeout event to trigger
  io.run_for(std::chrono::milliseconds(260));
  io.restart();

  //Should claim address 0x1
  std::uint8_t address_0{0x00U};
  addr_mng.start_address_claim(address_0);
  
  ///TODO: Test runngin start address claim again

  //Enought time for timeout event to trigger
  io.run_for(std::chrono::milliseconds(260));
  io.restart();

  ASSERT_EQ(frame_queue.size(), 1);

  ///Address claim frame
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), J1939_NO_ADDR);
  ASSERT_EQ(frame.header.source_adderess(), address_0);
  frame_queue.pop();
  
  ///Confirm name and address is registeded in network
  ASSERT_TRUE(j1939_network.in_network(local_name));
  ASSERT_FALSE(j1939_network.available(address_0));
  ASSERT_EQ(j1939_network.get_address(local_name), address_0);

  //Should return address claim 1 frame
  addr_mng.address_request(jay::address_claimer::ev_address_request{});

  io.run_for(std::chrono::milliseconds(20));
  io.restart();

  //Check for requested address claim
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), J1939_NO_ADDR);
  ASSERT_EQ(frame.header.source_adderess(), address_0);
  frame_queue.pop();

  for(std::uint8_t i = 0; i < J1939_MAX_UNICAST_ADDR; i++)
  { 
    //Insert claim into network
    j1939_network.insert(i, i);

    //Conficting claim should change to new address
    addr_mng.address_claim(jay::address_claimer::ev_address_claim
      {jay::name{i}, static_cast<std::uint8_t>(i)});

    io.run_for(std::chrono::milliseconds(260)); //Give timeout time to trigger
    io.restart();

    ASSERT_EQ(frame_queue.size(), 1);
    frame = frame_queue.front();
    ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
    ASSERT_EQ(frame.header.pdu_specific(), J1939_NO_ADDR);
    ASSERT_EQ(frame.header.source_adderess(), i + 1);
    frame_queue.pop();

    ASSERT_TRUE(j1939_network.in_network(local_name));
    ASSERT_EQ(j1939_network.get_address(local_name), i + 1);
  }

  //Insert claim into network
  j1939_network.insert(J1939_MAX_UNICAST_ADDR, J1939_MAX_UNICAST_ADDR);

  //Conficting claim should change to new address
  addr_mng.address_claim(jay::address_claimer::ev_address_claim
    {jay::name{J1939_MAX_UNICAST_ADDR}, static_cast<std::uint8_t>(J1939_MAX_UNICAST_ADDR)});

  io.run_for(std::chrono::milliseconds(260)); //Give timeout time to trigger
  io.restart();

  ASSERT_TRUE(j1939_network.full());

  //Check cannot claim address frame
  ASSERT_EQ(frame_queue.size(), 1);
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), J1939_NO_ADDR);
  ASSERT_EQ(frame.header.source_adderess(), J1939_IDLE_ADDR);
  frame_queue.pop();


 ///TODO: Test runngin start address claim again

}