//
// Copyright (c) 2020 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the MIT License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#include <gtest/gtest.h>

#include "../include/jay/address_claimer.hpp"

//C++
#include <queue>

//Lib
#include "boost/sml.hpp"

//Linux
#include <linux/can.h>

class StateMachineTest : public testing::Test
{
protected:

  // 
  StateMachineTest()
  {
    addr_claimer.set_callbacks(jay::address_claimer::callbacks{
    [this](jay::name name, uint8_t address) -> void {
      std::cout << std::hex << static_cast<uint64_t>(name) << " gained address: " << address << std::endl;
      //logger_->info("{0:x} gained address: {1:x}", static_cast<uint64_t>(name), address);
      j1939_network.insert(static_cast<uint64_t>(name), address);
    },
    [this](jay::name name) -> void {
      std::cout << std::hex << static_cast<uint64_t>(name) << " lost address" << std::endl;
      //logger_->info("{0:x} lost address", static_cast<uint64_t>(name));
      j1939_network.release(static_cast<uint64_t>(name));
      ///TODO: Check if this function is needed
    },
    [this]() -> void {
      std::cout << "Starting address claim" << std::endl;
      //logger_->info("Starting address claim");
    },
    [this](jay::frame frame) -> void {
      std::cout << "Sending claim: " << frame.to_string() << std::endl;
      //logger_->info("Sending claim: {}", frame.to_string());
      frame_queue.push(frame);
    },
    [this](jay::frame frame) -> void {
      std::cout << "Sending cannot claim: {} " << frame.to_string() << std::endl;
      //logger_->info("Sending cannot claim: {}", frame.to_string());
      frame_queue.push(frame);
    }});
  }

  // 
  virtual ~StateMachineTest()
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
  uint64_t local_name{0xFFU};
  uint8_t address{0xAAU};
  jay::network j1939_network{};
  std::queue<jay::frame> frame_queue{};

  jay::address_claimer addr_claimer{local_name};
  boost::sml::sm<jay::address_claimer, boost::sml::testing> state_machine{addr_claimer, j1939_network};

};

TEST_F(StateMachineTest, Jay_State_Machine_NoAddress_Test)
{
  //Check state
  state_machine.set_current_states(boost::sml::state<jay::address_claimer::st_no_address>);
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  //No address request created
  state_machine.process_event(jay::address_claimer::ev_address_request{});
  ASSERT_EQ(frame_queue.size(), 0);

  //Start address claim - creates address claim frame
  state_machine.process_event(jay::address_claimer::ev_start_claim{address});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));

  //Check address claim frame
  auto frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), address);
  frame_queue.pop();

  //Change state back
  state_machine.set_current_states(boost::sml::state<jay::address_claimer::st_no_address>);
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  for(uint8_t i = 0U; i < 200; i++)
  { ///Fill network
    j1939_network.insert(static_cast<uint64_t>(i), i);
  }

  state_machine.process_event(jay::address_claimer::ev_address_request{});
  ASSERT_EQ(frame_queue.size(), 0);

  //Start address claim - but preffered address cant be claimed address required
  state_machine.process_event(jay::address_claimer::ev_start_claim{address});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));

  //Check address claim frame
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), 200);
  frame_queue.pop();

  //Change state back
  state_machine.set_current_states(boost::sml::state<jay::address_claimer::st_no_address>);
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  for(uint8_t i = 200; i < jay::ADDRESS_NULL; i++)
  { ///Fill network
    j1939_network.insert(static_cast<uint64_t>(i), i);
  }

  ASSERT_TRUE(j1939_network.full());

  //Creates cannot claim address frame
  state_machine.process_event(jay::address_claimer::ev_address_request{});
  ASSERT_EQ(frame_queue.size(), 1);

  //Check cannot claim address frame
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), jay::ADDRESS_NULL);
  frame_queue.pop();

  //Try to start claiming
  state_machine.process_event(jay::address_claimer::ev_start_claim{address}); // fails to change
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));
}

TEST_F(StateMachineTest, Jay_State_Machine_Claiming_Test)
{
  ASSERT_FALSE(j1939_network.full());

  //Set state
  state_machine.set_current_states(boost::sml::state<jay::address_claimer::st_no_address>);
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  //Insert names into network
  for(uint8_t i = 0U; i < 0xB5U; i++)
  { ///Fill network
    j1939_network.insert(static_cast<uint64_t>(i), i);
  }

  //Change to claiming though name is allready taken so change address to available
  state_machine.process_event(jay::address_claimer::ev_start_claim{address});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));
  auto frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM); //Needed?
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), 0xB5U);
  frame_queue.pop();

  //Non competing address claim
  state_machine.process_event(jay::address_claimer::ev_address_claim{0x0, 150});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));
  ASSERT_EQ(frame_queue.size(), 0);

  //Get competing address claim from device with higher priority
  j1939_network.insert(static_cast<uint64_t>(0x0), 0xB5U);
  state_machine.process_event(jay::address_claimer::ev_address_claim{0x0, 0xB5U});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), 0xB6U); //Create new address claim with new address
  frame_queue.pop();
  
  //Get competing address claim from device with lower priority
  state_machine.process_event(jay::address_claimer::ev_address_claim{0xFFFF, 0xB6U});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), 0xB6U); //Claim own address again
  frame_queue.pop();

  //Dont respond to request when claiming?
  state_machine.process_event(jay::address_claimer::ev_address_request{});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));
  ASSERT_EQ(frame_queue.size(), 0);

  //Has address
  state_machine.process_event(jay::address_claimer::ev_timeout{});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_has_address>));

  j1939_network.release(local_name);

  //Change back
  state_machine.set_current_states(boost::sml::state<jay::address_claimer::st_no_address>);
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  state_machine.process_event(jay::address_claimer::ev_start_claim{0xAA});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), 0xB6U); //First available
  frame_queue.pop();

  //Fill remainging addresses
  for(uint8_t i = 0xB6U; i < jay::ADDRESS_NULL; i++)
  { ///Fill network
    j1939_network.insert(static_cast<uint64_t>(i), i);
    state_machine.process_event(jay::address_claimer::ev_address_claim{i, i});
    frame = frame_queue.front();
    ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
    ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
    ASSERT_EQ(frame.header.source_adderess(), i + 1); //next address
    frame_queue.pop();
  }

  ASSERT_TRUE(j1939_network.full());

  //test state change
  state_machine.process_event(jay::address_claimer::ev_timeout{});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  frame = frame_queue.back();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), jay::ADDRESS_NULL);
}

TEST_F(StateMachineTest, Jay_State_Machine_HasAddress_Test)
{
  state_machine.set_current_states(boost::sml::state<jay::address_claimer::st_no_address>);
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  //Change to claiming
  state_machine.process_event(jay::address_claimer::ev_start_claim{address});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));
  auto frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), address);
  ASSERT_EQ(frame_queue.size(), 1);
  frame_queue.pop();

  //Change to has address, make sure that the address is set
  state_machine.process_event(jay::address_claimer::ev_timeout{});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_has_address>));
  ASSERT_EQ(j1939_network.get_address(local_name), address);

  //Address request event
  state_machine.process_event(jay::address_claimer::ev_address_request{});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_has_address>));
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), address);
  ASSERT_EQ(frame_queue.size(), 1);
  frame_queue.pop();

  //Non competing address claim
  state_machine.process_event(jay::address_claimer::ev_address_claim{0x0, 150});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_has_address>));
  ASSERT_EQ(frame_queue.size(), 0);

  //Get competing address claim from device with higher priority
  j1939_network.insert(static_cast<uint64_t>(170), 170);
  state_machine.process_event(jay::address_claimer::ev_address_claim{170, 170});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), 171); //Claim address one up if address is sent back propperly
  frame_queue.pop();

  //Change to has address, make sure that the address is set
  state_machine.process_event(jay::address_claimer::ev_timeout{});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_has_address>));
  ASSERT_EQ(j1939_network.get_address(local_name), 171);
  
  //Get competing address claim from device with lower priority
  state_machine.process_event(jay::address_claimer::ev_address_claim{0xFFFF, 171});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_has_address>));
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), 171); //Claim own address again
  frame_queue.pop();

  //Fill remainging addresses
  for(uint8_t i = 0; i < jay::ADDRESS_NULL; i++)
  { ///Fill network
    j1939_network.insert(static_cast<uint64_t>(i), i);
  }
  
  state_machine.process_event(jay::address_claimer::ev_address_claim{0x0, 171});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));
  frame = frame_queue.front();
  ASSERT_EQ(frame.header.pdu_format(), jay::PF_ADDRESS_CLAIM);
  ASSERT_EQ(frame.header.pdu_specific(), jay::ADDRESS_GLOBAL);
  ASSERT_EQ(frame.header.source_adderess(), jay::ADDRESS_NULL);
  frame_queue.pop();
}