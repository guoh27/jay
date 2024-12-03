//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#include <gtest/gtest.h>

#include "../include/jay/network.hpp"

/// TODO: Add test for callback

/// TODO: Check test with the new changes to find_address!

TEST(Jay_Network_Test, Jay_Network_Insert_Test)
{
  std::string interface_name{ "vcan0" };
  jay::network j1939_network{ interface_name };
  jay::name controller_1{ 0xa00c81045a20021b };/// Temp hardwire names 11532734601480897051
  jay::name controller_2{ 0xa00c810c5a20021b };/// Temp hardwire names 11532734635840635419
  uint8_t address_1{ 0x96 };
  uint8_t address_2{ 0x97 };

  ASSERT_TRUE(j1939_network.try_address_claim(controller_1, address_1));
  ASSERT_TRUE(j1939_network.try_address_claim(controller_2, address_2));

  // Checking network internals
  ASSERT_EQ(interface_name, j1939_network.interface_name());
  ASSERT_TRUE(j1939_network.in_network(controller_1));
  ASSERT_TRUE(j1939_network.in_network(controller_2));
  ASSERT_FALSE(j1939_network.available(address_1));
  ASSERT_FALSE(j1939_network.available(address_2));
  ASSERT_EQ(j1939_network.find_address(controller_1), address_1);
  ASSERT_EQ(j1939_network.find_address(controller_2), address_2);
  auto opt_addr = j1939_network.find_name(address_1);
  ASSERT_TRUE(opt_addr);
  ASSERT_EQ(opt_addr.value(), controller_1);
  opt_addr = j1939_network.find_name(address_2);
  ASSERT_TRUE(opt_addr);
  ASSERT_EQ(opt_addr.value(), controller_2);
  ASSERT_EQ(j1939_network.address_size(), 2);
  ASSERT_EQ(j1939_network.name_size(), 2);

  // Removing addresses and controllers
  j1939_network.release(controller_1);
  ASSERT_TRUE(j1939_network.in_network(controller_1));
  ASSERT_TRUE(j1939_network.available(address_1));
  ASSERT_EQ(j1939_network.find_address(controller_1), J1939_IDLE_ADDR);
  ASSERT_FALSE(j1939_network.find_name(address_1));
  ASSERT_EQ(j1939_network.address_size(), 1);
  ASSERT_EQ(j1939_network.name_size(), 2);

  j1939_network.erase(controller_2);
  ASSERT_FALSE(j1939_network.in_network(controller_2));
  ASSERT_TRUE(j1939_network.available(address_2));
  ASSERT_EQ(j1939_network.find_address(controller_2), J1939_NO_ADDR);
  ASSERT_FALSE(j1939_network.find_name(address_2));
  ASSERT_EQ(j1939_network.address_size(), 0);
  ASSERT_EQ(j1939_network.name_size(), 1);

  // Insert with override address
  ASSERT_TRUE(j1939_network.try_address_claim(controller_1, address_1));
  ASSERT_EQ(j1939_network.find_address(controller_1), address_1);
  ASSERT_EQ(j1939_network.address_size(), 1);
  ASSERT_EQ(j1939_network.name_size(), 1);

  // Controller 2 is larger therfor cannot claim
  ASSERT_TRUE(j1939_network.try_address_claim(controller_2, address_1));
  ASSERT_EQ(j1939_network.find_address(controller_2), J1939_IDLE_ADDR);
  ASSERT_EQ(j1939_network.address_size(), 1);
  ASSERT_EQ(j1939_network.name_size(), 2);

  ASSERT_TRUE(j1939_network.try_address_claim(controller_2, address_2));
  ASSERT_EQ(j1939_network.find_address(controller_2), address_2);
  ASSERT_EQ(j1939_network.address_size(), 2);
  ASSERT_EQ(j1939_network.name_size(), 2);

  uint64_t controller_3{ controller_2 - 1 };
  uint64_t controller_4{ controller_2 + 1 };

  // Controller 3 is smaller an can take address
  ASSERT_TRUE(j1939_network.try_address_claim(controller_3, address_2));
  ASSERT_EQ(j1939_network.find_address(controller_3), address_2);
  ASSERT_EQ(j1939_network.find_address(controller_2), J1939_IDLE_ADDR);
  ASSERT_EQ(j1939_network.address_size(), 2);
  ASSERT_EQ(j1939_network.name_size(), 3);

  // Inserting address null
  ASSERT_TRUE(j1939_network.try_address_claim(controller_4, J1939_IDLE_ADDR));
  ASSERT_EQ(j1939_network.find_address(controller_4), J1939_IDLE_ADDR);
  ASSERT_EQ(j1939_network.address_size(), 2);
  ASSERT_EQ(j1939_network.name_size(), 4);

  // Inserting existing with global address should not change anything
  ASSERT_FALSE(j1939_network.try_address_claim(controller_3, J1939_NO_ADDR));
  ASSERT_EQ(j1939_network.find_address(controller_3), address_2);
  ASSERT_EQ(j1939_network.address_size(), 2);
  ASSERT_EQ(j1939_network.name_size(), 4);
}

TEST(Jay_Network_Test, Jay_Network_Fill_Test)
{
  jay::network j1939_network{ "vcan0" };
  ASSERT_FALSE(j1939_network.is_full());

  for (uint8_t i = 0; i < J1939_NO_ADDR; i++) { ASSERT_TRUE(j1939_network.try_address_claim(i, i)); }

  ASSERT_EQ(j1939_network.address_size(), J1939_MAX_UNICAST_ADDR + 1);
  ASSERT_EQ(j1939_network.name_size(), J1939_NO_ADDR);
  ASSERT_TRUE(j1939_network.is_full());
}

TEST(Jay_Network_Test, Jay_Network_Search_Test)
{
  jay::network j1939_network{ "vcan0" };
  ASSERT_FALSE(j1939_network.is_full());

  jay::name controller{ 200 };
  uint8_t address = 100;

  uint64_t ctrl{ 100 };
  for (uint8_t i = 0; i < J1939_IDLE_ADDR; i++) {// Fill network
    ASSERT_TRUE(j1939_network.try_address_claim(ctrl, i));
    ctrl++;
  }

  // No address available
  ASSERT_EQ(j1939_network.find_available_address(0), J1939_NO_ADDR);

  // Check if name 200 has address 100
  ASSERT_EQ(j1939_network.find_address(controller), address);

  // Remove name and address
  j1939_network.erase(controller);
  ASSERT_TRUE(j1939_network.available(address));

  // Look search if address is available
  ASSERT_EQ(j1939_network.find_available_address(0), address);

  // Look again starting at address 1 over available address
  ASSERT_EQ(j1939_network.find_available_address(0, address + 1), address);

  // Insert again
  ASSERT_TRUE(j1939_network.try_address_claim(controller, address));

  // Claim first address
  ASSERT_EQ(j1939_network.find_available_address(0, 0, true), 0);
  ASSERT_EQ(j1939_network.find_available_address(controller, 0, true), address + 1);
}