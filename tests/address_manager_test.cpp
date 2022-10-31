//
// Copyright (c) 2020 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the MIT License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#include <gtest/gtest.h>

#include "../include/jay/address_manager.hpp"

//C++
#include <queue>
#include <thread>
#include <chrono>

//Lib
#include "spdlog/spdlog.h"

//Linux

class AddressManagerTest : public testing::Test
{
protected:

  // 
  AddressManagerTest()
  {
    auto logger_ = spdlog::get("CONSOLE");
    addr_mng.set_callbacks(jay::address_manager::callbacks{
      [/*logger_*/](jay::name /*name*/, uint8_t /*address*/) -> void 
      { //New controller
        //logger_->info("New controller: {0:x}, {1:x}", name, address);
      },
      [logger_](jay::name name, uint8_t address) -> void 
      { //On address
        logger_->info("Controller claimed: {0:x}, {1:x}", name, address);
      },
      [logger_](jay::name name) -> void 
      { //Lost address
        logger_->info("Controller Lost: {0:x}", name);
      },
      [logger_, this](jay::frame frame) -> void 
      { //On output frame
        logger_->info("Output frame: {}", frame.to_string());
        frame_queue.push(frame);
      },
      [logger_](std::string what, std::string error) -> void 
      { //ON error
        logger_->error("{} : {}", what, error);
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
    frame_queue = std::queue<jay::frame>{};
  }

  // 
  virtual void TearDown() override
  {
    j1939_network.clear();

  }

public:
  std::queue<jay::frame> frame_queue{};

  boost::asio::io_context io{};
  jay::network j1939_network{};
  jay::address_manager addr_mng{io, j1939_network};

};

TEST_F(AddressManagerTest, Jay_Address_Manager_Test)
{
  ASSERT_EQ(frame_queue.size(), 0);

  jay::name local_name{0xFFU};
  uint8_t address_0{0xAAU};

  jay::name controller_1{0xa00c81045a20021b};
  uint8_t address_1{0x96};

  auto address_request = jay::frame::make_address_request();
  addr_mng.process(address_request); //Should do nothing

  auto address_claim = jay::frame::make_address_claim(address_1, controller_1.to_payload());
  addr_mng.process(address_claim); //Should register in network

  address_claim = jay::frame::make_address_claim(address_1, controller_1.to_payload());
  addr_mng.process(address_claim); //Should do nothing

  for(uint8_t i = 0U; i < 200; i++)
  { ///Fill network
    jay::name tmp(i);
    address_claim = jay::frame::make_address_claim(i, tmp.to_payload());
    addr_mng.process(address_claim); //Should each in network
  }

  addr_mng.aquire(local_name, address_0); //Should claim address 200

  io.run_for(std::chrono::milliseconds(260));
  io.restart();

  ASSERT_EQ(frame_queue.size(), 1);

  ///Confirm the frames in the queue are correct
  auto frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), 200);
  frame_queue.pop();
  
  ///Confirm that network data is correct
  ASSERT_TRUE(j1939_network.in_network(controller_1));
  ASSERT_FALSE(j1939_network.available(address_1));
  ASSERT_EQ(j1939_network.get_address(controller_1), jay::ADDRESS_NULL);

  ASSERT_TRUE(j1939_network.in_network(local_name));
  ASSERT_FALSE(j1939_network.available(address_1));
  ASSERT_EQ(j1939_network.get_address(local_name), 200);

  ///TODO: Requesting address is currently failing to return address!?
  addr_mng.process(address_request); //Should return 1 frame

  //Should generate 10 frames
  for(uint8_t i = 200U; i < 210; i++)
  { ///Fill network more, should cause address loss state change
    jay::name tmp(i);
    address_claim = jay::frame::make_address_claim(i, tmp.to_payload());
    addr_mng.process(address_claim);
  }

  io.run_for(std::chrono::milliseconds(260));
  io.restart();

  ASSERT_EQ(frame_queue.size(), 10);

  //Handle queue, first should be address claim in return for request
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), 200);
  frame_queue.pop();

  for(uint8_t i = 1U; i < 11; i++)
  {
    frame = frame_queue.front();
    ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
    ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
    ASSERT_EQ(frame.header.source_adderess(), 200 + i);
    frame_queue.pop();
  }

  ASSERT_TRUE(j1939_network.in_network(local_name));
  ASSERT_FALSE(j1939_network.available(address_1));
  ASSERT_EQ(j1939_network.get_address(local_name), 210);

  ///Fill Remaining

  for(uint8_t i = 210; i < jay::ADDRESS_NULL; i++)
  { ///Fill network
    jay::name tmp(i);
    address_claim = jay::frame::make_address_claim(i, tmp.to_payload());
    addr_mng.process(address_claim); //Should each in network
  }

  io.run_for(std::chrono::milliseconds(260));

  ASSERT_TRUE(j1939_network.full());

  //Check cannot claim address frame
  frame = frame_queue.back();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), jay::ADDRESS_NULL);

}