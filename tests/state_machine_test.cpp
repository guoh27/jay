//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/guoh27/jay
//

#include <gtest/gtest.h>

#include "jay/address_claimer.hpp"

// C++
#include <queue>

// Lib
#include "boost/sml.hpp"

// Linux
#include <linux/can.h>

class StateMachineTest : public testing::Test
{
protected:
  //
  StateMachineTest()
  {
    /// Cout used for debugging
    addr_claimer.set_callbacks(jay::address_claimer::callbacks{ [this](jay::name name, uint8_t address) -> void {
                                                                 // std::cout << std::hex << (name) << " gained address:
                                                                 // " << std::hex << address << std::endl;
                                                                 j1939_network.insert(name, address);
                                                               },
      [this](jay::name name) -> void {
        // std::cout << std::hex << (name) << " lost address" << std::endl;
        j1939_network.release((name));
      },
      [this]() -> void {
        // std::cout << std::hex << name << " starting claim" << std::endl;
      },
      [this](jay::name name, std::uint8_t address) -> void {
        // std::cout << std::hex << name << " claiming address: " << std::hex << address << std::endl;
        claim_queue.push({ name, address });
      },
      [this]() {

      },
      [this](jay::name name) -> void {
        // std::cout << std::hex << name << " cant claim address" << std::endl;
        cannot_claim_queue.push(name);
      } });
  }

  //
  ~StateMachineTest() override = default;

  //
  void SetUp() override
  {
    // Clear queues
    claim_queue = std::queue<std::pair<jay::name, std::uint8_t>>{};
    cannot_claim_queue = std::queue<jay::name>{};
  }

  //
  void TearDown() override { j1939_network.clear(); }

public:
  uint64_t local_name{ 0xFFU };
  uint8_t address{ 0xAAU };
  jay::network j1939_network{ "vcan0" };
  std::queue<std::pair<jay::name, std::uint8_t>> claim_queue{};
  std::queue<jay::name> cannot_claim_queue{};

  jay::address_claimer addr_claimer{ local_name };
  jay::address_claimer::st_claiming claim_state_{};
  jay::address_claimer::st_has_address has_address_state_{};

  boost::sml::sm<jay::address_claimer, boost::sml::testing> state_machine{ addr_claimer,
    j1939_network,
    claim_state_,
    has_address_state_,
    jay::address_claimer::ev_start_claim{} };
};

TEST_F(StateMachineTest, Jay_State_Machine_NoAddress_Test)
{
  // Check state
  state_machine.set_current_states(boost::sml::state<jay::address_claimer::st_no_address>);
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  // No address, trigger cannot claim, should return a claim frame with idle address
  // which is the same as a cannot claim address frame
  state_machine.process_event(jay::address_claimer::ev_address_request{});
  ASSERT_EQ(cannot_claim_queue.size(), 1);
  ASSERT_EQ(cannot_claim_queue.front(), local_name);
  cannot_claim_queue.pop();

  auto addresses_used = j1939_network.address_count();
  auto names_inserted = j1939_network.name_count();

  // Process timeout, which should do nothing
  state_machine.process_event(jay::address_claimer::ev_timeout{});
  ASSERT_EQ(claim_queue.size(), 0);
  ASSERT_EQ(cannot_claim_queue.size(), 0);
  ASSERT_EQ(j1939_network.address_count(), addresses_used);
  ASSERT_EQ(j1939_network.name_count(), names_inserted);

  // Process address claim, which should do nothing
  state_machine.process_event(jay::address_claimer::ev_address_claim{ 0xAA, address });
  ASSERT_EQ(claim_queue.size(), 0);
  ASSERT_EQ(cannot_claim_queue.size(), 0);
  ASSERT_EQ(j1939_network.address_count(), addresses_used);
  ASSERT_EQ(j1939_network.name_count(), names_inserted);

  // Start address claim - creates address claim frame
  state_machine.process_event(jay::address_claimer::ev_start_claim{ address });
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));

  // Check address claim frame
  auto claim_pair = claim_queue.front();
  ASSERT_EQ(claim_pair.first, local_name);
  ASSERT_EQ(claim_pair.second, address);
  claim_queue.pop();

  // Change state back
  state_machine.set_current_states(boost::sml::state<jay::address_claimer::st_no_address>);
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  for (uint8_t i = 0U; i < 200; i++) {/// Fill network
    j1939_network.insert(i, i);
  }

  // Start address claim - but proffered address cant be claimed address required
  state_machine.process_event(jay::address_claimer::ev_start_claim{ address });
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));

  // Check address claim frame
  claim_pair = claim_queue.front();
  ASSERT_EQ(claim_pair.first, local_name);
  ASSERT_EQ(claim_pair.second, 200);
  claim_queue.pop();

  // Change state back
  state_machine.set_current_states(boost::sml::state<jay::address_claimer::st_no_address>);
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  for (uint8_t i = 200; i < jay::J1939_NO_ADDR; i++) {/// Fill network
    j1939_network.insert(i, i);
  }

  ASSERT_TRUE(j1939_network.full());

  // Try to start claiming
  state_machine.process_event(jay::address_claimer::ev_start_claim{ address });// fails to change
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  // Sends cannot claim
  ASSERT_EQ(cannot_claim_queue.size(), 1);
  ASSERT_EQ(cannot_claim_queue.front(), local_name);
  cannot_claim_queue.pop();
}

TEST_F(StateMachineTest, Jay_State_Machine_Claiming_Test)
{
  ASSERT_FALSE(j1939_network.full());

  // Set state
  state_machine.set_current_states(boost::sml::state<jay::address_claimer::st_no_address>);
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  // Insert names into network
  for (uint8_t i = 0U; i < 0xB5U; i++) {/// Fill network
    j1939_network.insert(i, i);
  }

  // Change to claiming though name is already taken so change address to available
  state_machine.process_event(jay::address_claimer::ev_start_claim{ address });
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));
  auto claim_pair = claim_queue.front();
  ASSERT_EQ(claim_pair.first, local_name);
  ASSERT_EQ(claim_pair.second, 0xB5U);
  claim_queue.pop();

  // Non competing address claim, no response needed
  state_machine.process_event(jay::address_claimer::ev_address_claim{ 0x0, 150 });
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));
  ASSERT_EQ(claim_queue.size(), 0);

  // Get competing address claim from device with higher priority
  j1939_network.insert(0x0U, 0xB5U);
  state_machine.process_event(jay::address_claimer::ev_address_claim{ 0x0, 0xB5U });
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));

  ASSERT_EQ(claim_queue.size(), 1);
  claim_pair = claim_queue.front();
  ASSERT_EQ(claim_pair.first, local_name);
  ASSERT_EQ(claim_pair.second, 0xB6U);// Create new address claim with new address
  claim_queue.pop();

  // Get competing address claim from device with lower priority
  state_machine.process_event(jay::address_claimer::ev_address_claim{ 0xFFFF, 0xB6U });
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));

  ASSERT_EQ(claim_queue.size(), 1);
  claim_pair = claim_queue.front();
  ASSERT_EQ(claim_pair.first, local_name);
  ASSERT_EQ(claim_pair.second, 0xB6U);// Claim own address again
  claim_queue.pop();

  // Respond to request when claiming
  state_machine.process_event(jay::address_claimer::ev_address_request{});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));
  ASSERT_EQ(claim_queue.size(), 1);
  claim_pair = claim_queue.front();
  ASSERT_EQ(claim_pair.first, local_name);
  ASSERT_EQ(claim_pair.second, 0xB6U);
  claim_queue.pop();

  // Timeout event into Has address state
  state_machine.process_event(jay::address_claimer::ev_timeout{});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_has_address>));
  ASSERT_TRUE(j1939_network.in_network(local_name));

  // Change to no address state and manually release name from network
  j1939_network.release(local_name);
  state_machine.set_current_states(boost::sml::state<jay::address_claimer::st_no_address>);
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  // Should not send cannot claim as addresses are available
  ASSERT_EQ(cannot_claim_queue.size(), 0);

  // Start claiming
  state_machine.process_event(jay::address_claimer::ev_start_claim{ 0xAA });
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));

  ASSERT_EQ(claim_queue.size(), 1);
  claim_pair = claim_queue.front();
  ASSERT_EQ(claim_pair.first, local_name);
  ASSERT_EQ(claim_pair.second, 0xB6U);// First available
  claim_queue.pop();

  // Fill remaining addresses, until none are available
  for (uint8_t i = 0xB6U; i < jay::J1939_IDLE_ADDR; i++) {/// Fill network
    j1939_network.insert(i, i);
    state_machine.process_event(jay::address_claimer::ev_address_claim{ i, i });
  }

  ASSERT_EQ(claim_queue.size(), jay::J1939_IDLE_ADDR - 0xB6U - 1);

  for (uint8_t i = 0xB6U; i < jay::J1939_IDLE_ADDR - 1; i++) {// Claiming until max address reached
    claim_pair = claim_queue.front();
    ASSERT_EQ(claim_pair.first, local_name);
    ASSERT_EQ(claim_pair.second, i + 1);// next address
    claim_queue.pop();
  }

  ASSERT_EQ(cannot_claim_queue.size(), 1);
  ASSERT_EQ(cannot_claim_queue.front(), local_name);
  cannot_claim_queue.pop();

  ASSERT_TRUE(j1939_network.full());

  // No address available means falling to no address state
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_address_lost>));
}

TEST_F(StateMachineTest, Jay_State_Machine_HasAddress_Test)
{
  state_machine.set_current_states(boost::sml::state<jay::address_claimer::st_no_address>);
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_no_address>));

  // Change to claiming
  state_machine.process_event(jay::address_claimer::ev_start_claim{ address });
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));

  auto claim_pair = claim_queue.front();
  ASSERT_EQ(claim_queue.size(), 1);
  ASSERT_EQ(claim_pair.first, local_name);
  ASSERT_EQ(claim_pair.second, address);
  claim_queue.pop();

  // Change to has address, make sure that the address is set
  state_machine.process_event(jay::address_claimer::ev_timeout{});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_has_address>));
  ASSERT_EQ(j1939_network.get_address(local_name), address);

  // Address request event
  state_machine.process_event(jay::address_claimer::ev_address_request{});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_has_address>));
  claim_pair = claim_queue.front();
  ASSERT_EQ(claim_queue.size(), 1);
  ASSERT_EQ(claim_pair.first, local_name);
  ASSERT_EQ(claim_pair.second, address);
  claim_queue.pop();

  // Non competing address claim
  state_machine.process_event(jay::address_claimer::ev_address_claim{ 0x0, 150 });
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_has_address>));
  ASSERT_EQ(claim_queue.size(), 0);

  // Get competing address claim from device with higher priority
  j1939_network.insert(170, 170);
  state_machine.process_event(jay::address_claimer::ev_address_claim{ 170, 170 });
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_claiming>));

  claim_pair = claim_queue.front();
  ASSERT_EQ(claim_queue.size(), 1);
  ASSERT_EQ(claim_pair.first, local_name);
  ASSERT_EQ(claim_pair.second, 171);
  claim_queue.pop();

  // Change to has address, make sure that the address is set
  state_machine.process_event(jay::address_claimer::ev_timeout{});
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_has_address>));
  ASSERT_EQ(j1939_network.get_address(local_name), 171);

  // Get competing address claim from device with lower priority
  state_machine.process_event(jay::address_claimer::ev_address_claim{ 0xFFFF, 171 });
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_has_address>));

  claim_pair = claim_queue.front();
  ASSERT_EQ(claim_queue.size(), 1);
  ASSERT_EQ(claim_pair.first, local_name);
  ASSERT_EQ(claim_pair.second, 171);
  claim_queue.pop();

  // Fill remaining addresses
  for (uint8_t i = 0; i < jay::J1939_NO_ADDR; i++) {/// Fill network
    j1939_network.insert(i, i);
  }

  state_machine.process_event(jay::address_claimer::ev_address_claim{ 0x0, 171 });
  ASSERT_TRUE(state_machine.is(boost::sml::state<jay::address_claimer::st_address_lost>));

  // Sends cannot claim as no address is available
  ASSERT_EQ(cannot_claim_queue.size(), 1);
  ASSERT_EQ(cannot_claim_queue.front(), local_name);
  cannot_claim_queue.pop();
}