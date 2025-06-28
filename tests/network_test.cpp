//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/guoh27/jay
//

#include <gtest/gtest.h>

#include "jay/network.hpp"

TEST(Jay_Network_Test, Jay_Network_Insert_Test)
{
  std::string interface_name{ "vcan0" };
  jay::network j1939_network{ interface_name };
  uint64_t controller_1{ 0xa00c81045a20021b };/// Temp hardware names 11532734601480897051
  uint64_t controller_2{ 0xa00c810c5a20021b };/// Temp hardware names 11532734635840635419
  uint8_t address_1{ 0x96 };
  uint8_t address_2{ 0x97 };

  ASSERT_TRUE(j1939_network.insert(controller_1, address_1));
  ASSERT_TRUE(j1939_network.insert(controller_2, address_2));

  // Checking network internals
  ASSERT_EQ(interface_name, j1939_network.get_interface_name());
  ASSERT_TRUE(j1939_network.in_network(controller_1));
  ASSERT_TRUE(j1939_network.in_network(controller_2));
  ASSERT_FALSE(j1939_network.available(address_1));
  ASSERT_FALSE(j1939_network.available(address_2));
  ASSERT_EQ(j1939_network.get_address(controller_1), address_1);
  ASSERT_EQ(j1939_network.get_address(controller_2), address_2);
  auto opt_addr = j1939_network.get_name(address_1);
  ASSERT_TRUE(opt_addr);
  ASSERT_EQ(opt_addr.value(), controller_1);
  opt_addr = j1939_network.get_name(address_2);
  ASSERT_TRUE(opt_addr);
  ASSERT_EQ(opt_addr.value(), controller_2);
  ASSERT_EQ(j1939_network.address_count(), 2);
  ASSERT_EQ(j1939_network.name_count(), 2);

  // Removing addresses and controllers
  j1939_network.release(controller_1);
  ASSERT_TRUE(j1939_network.in_network(controller_1));
  ASSERT_TRUE(j1939_network.available(address_1));
  ASSERT_EQ(j1939_network.get_address(controller_1), jay::J1939_IDLE_ADDR);
  ASSERT_FALSE(j1939_network.get_name(address_1));
  ASSERT_EQ(j1939_network.address_count(), 1);
  ASSERT_EQ(j1939_network.name_count(), 2);

  j1939_network.remove(controller_2);
  ASSERT_FALSE(j1939_network.in_network(controller_2));
  ASSERT_TRUE(j1939_network.available(address_2));
  ASSERT_EQ(j1939_network.get_address(controller_2), jay::J1939_NO_ADDR);
  ASSERT_FALSE(j1939_network.get_name(address_2));
  ASSERT_EQ(j1939_network.address_count(), 0);
  ASSERT_EQ(j1939_network.name_count(), 1);

  // Insert with override address
  ASSERT_TRUE(j1939_network.insert(controller_1, address_1));
  ASSERT_EQ(j1939_network.get_address(controller_1), address_1);
  ASSERT_EQ(j1939_network.address_count(), 1);
  ASSERT_EQ(j1939_network.name_count(), 1);

  // Controller 2 is larger therefore cannot claim
  ASSERT_FALSE(j1939_network.insert(controller_2, address_1));
  ASSERT_EQ(j1939_network.get_address(controller_2), jay::J1939_IDLE_ADDR);
  ASSERT_EQ(j1939_network.address_count(), 1);
  ASSERT_EQ(j1939_network.name_count(), 2);

  ASSERT_TRUE(j1939_network.insert(controller_2, address_2));
  ASSERT_EQ(j1939_network.get_address(controller_2), address_2);
  ASSERT_EQ(j1939_network.address_count(), 2);
  ASSERT_EQ(j1939_network.name_count(), 2);

  uint64_t controller_3{ controller_2 - 1 };
  uint64_t controller_4{ controller_2 + 1 };

  // Controller 3 is smaller an can take address
  ASSERT_TRUE(j1939_network.insert(controller_3, address_2));
  ASSERT_EQ(j1939_network.get_address(controller_3), address_2);
  ASSERT_EQ(j1939_network.get_address(controller_2), jay::J1939_IDLE_ADDR);
  ASSERT_EQ(j1939_network.address_count(), 1);
  ASSERT_EQ(j1939_network.name_count(), 3);

  // Inserting address null
  ASSERT_TRUE(j1939_network.insert(controller_4, jay::J1939_IDLE_ADDR));
  ASSERT_EQ(j1939_network.get_address(controller_4), jay::J1939_IDLE_ADDR);
  ASSERT_EQ(j1939_network.address_count(), 1);
  ASSERT_EQ(j1939_network.name_count(), 4);

  // Inserting existing with global address should not change anything
  ASSERT_FALSE(j1939_network.insert(controller_3, jay::J1939_NO_ADDR));
  ASSERT_EQ(j1939_network.get_address(controller_3), address_2);
  ASSERT_EQ(j1939_network.address_count(), 1);
  ASSERT_EQ(j1939_network.name_count(), 4);
}

TEST(Jay_Network_Test, Jay_Network_Fill_Test)
{
  jay::network j1939_network{ "vcan0" };
  ASSERT_FALSE(j1939_network.full());

  for (uint8_t i = 0; i < jay::J1939_NO_ADDR; i++) { ASSERT_TRUE(j1939_network.insert(i, i)); }

  ASSERT_EQ(j1939_network.address_count(), jay::J1939_MAX_UNICAST_ADDR + 1);
  ASSERT_EQ(j1939_network.name_count(), jay::J1939_NO_ADDR);
  ASSERT_TRUE(j1939_network.full());
}

TEST(Jay_Network_Test, Jay_Network_Search_Test)
{
  jay::network j1939_network{ "vcan0" };
  ASSERT_FALSE(j1939_network.full());

  jay::name controller{ 200 };
  controller.self_config_address(1);
  uint8_t address = 100;

  jay::name ctrl{ 100 };
  ctrl.self_config_address(1);
  for (uint8_t i = 0; i < jay::J1939_IDLE_ADDR; i++) {// Fill network
    ASSERT_TRUE(j1939_network.insert(ctrl, i));
    ctrl = jay::name_t(ctrl) + 1;
  }

  auto name = jay::name(0);
  name.self_config_address(1);

  // No address available
  ASSERT_EQ(j1939_network.find_address(name), jay::J1939_NO_ADDR);

  // Check if name 200 has address 100
  ASSERT_EQ(j1939_network.get_address(controller), address);

  // Remove name and address
  j1939_network.remove(controller);
  ASSERT_TRUE(j1939_network.available(address));

  // Look search if address is available
  ASSERT_EQ(j1939_network.find_address(name), address);

  // Look again starting at address 1 over available address
  ASSERT_EQ(j1939_network.find_address(name, address + 1), address);

  // Insert again
  ASSERT_TRUE(j1939_network.insert(controller, address));
}

TEST(Jay_Network_Test, Jay_Network_Get_Name_Set_Test)
{
  jay::network j1939_network{ "vcan0" };
  std::vector<jay::name> names{ jay::name{ 0x1U }, jay::name{ 0x2U }, jay::name{ 0x3U } };

  uint8_t address = 0x80;
  for (auto n : names) { j1939_network.insert(n, address++); }

  auto name_set = j1939_network.get_name_set();
  ASSERT_EQ(name_set.size(), names.size());
  for (auto n : names) { ASSERT_TRUE(name_set.count(n)); }
}