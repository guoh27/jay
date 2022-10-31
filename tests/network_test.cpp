//
// Copyright (c) 2020 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the MIT License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#include <gtest/gtest.h>


#include "../include/jay/network.hpp"

TEST(Jay_Network_Test, Jay_Network_Insert_Test)
{
  jay::network j1939_network{};
  uint64_t controller_1{0xa00c81045a20021b}; ///Temp hardwire names 11532734601480897051
  uint64_t controller_2{0xa00c810c5a20021b}; ///Temp hardwire names 11532734635840635419
  uint8_t address_1{0x96};
  uint8_t address_2{0x97};

  ASSERT_TRUE(j1939_network.insert(controller_1, address_1));
  ASSERT_TRUE(j1939_network.insert(controller_2, address_2));

  //Checking network internals
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
  ASSERT_EQ(j1939_network.controller_count(), 2);

  //Removing addresses and controllers
  j1939_network.release(controller_1);
  ASSERT_TRUE(j1939_network.in_network(controller_1));
  ASSERT_TRUE(j1939_network.available(address_1));
  ASSERT_EQ(j1939_network.get_address(controller_1), jay::ADDRESS_NULL);
  ASSERT_FALSE(j1939_network.get_name(address_1));
  ASSERT_EQ(j1939_network.address_count(), 1);
  ASSERT_EQ(j1939_network.controller_count(), 2);

  j1939_network.remove(controller_2);
  ASSERT_FALSE(j1939_network.in_network(controller_2));
  ASSERT_TRUE(j1939_network.available(address_2));
  ASSERT_EQ(j1939_network.get_address(controller_2), jay::ADDRESS_NULL);
  ASSERT_FALSE(j1939_network.get_name(address_2));
  ASSERT_EQ(j1939_network.address_count(), 0);
  ASSERT_EQ(j1939_network.controller_count(), 1);

  //Insert with override address
  ASSERT_TRUE(j1939_network.insert(controller_1, address_1));
  ASSERT_EQ(j1939_network.get_address(controller_1), address_1);
  ASSERT_EQ(j1939_network.address_count(), 1);
  ASSERT_EQ(j1939_network.controller_count(), 1);

  // Controller 2 is larger therfor cannot claim
  ASSERT_FALSE(j1939_network.insert(controller_2, address_1));
  ASSERT_EQ(j1939_network.get_address(controller_2), jay::ADDRESS_NULL);
  ASSERT_EQ(j1939_network.address_count(), 1);
  ASSERT_EQ(j1939_network.controller_count(), 2);

  ASSERT_TRUE(j1939_network.insert(controller_2, address_2));
  ASSERT_EQ(j1939_network.get_address(controller_2), address_2);
  ASSERT_EQ(j1939_network.address_count(), 2);
  ASSERT_EQ(j1939_network.controller_count(), 2);

  uint64_t controller_3{controller_2 - 1};
  uint64_t controller_4{controller_2 + 1};

  // Controller 3 is smaller an can take address
  ASSERT_TRUE(j1939_network.insert(controller_3, address_2));
  ASSERT_EQ(j1939_network.get_address(controller_3), address_2);
  ASSERT_EQ(j1939_network.get_address(controller_2), jay::ADDRESS_NULL);
  ASSERT_EQ(j1939_network.address_count(), 2);
  ASSERT_EQ(j1939_network.controller_count(), 3);

  // Inserting address null
  ASSERT_TRUE(j1939_network.insert(controller_4, jay::ADDRESS_NULL));
  ASSERT_EQ(j1939_network.get_address(controller_4), jay::ADDRESS_NULL);
  ASSERT_EQ(j1939_network.address_count(), 2);
  ASSERT_EQ(j1939_network.controller_count(), 4);

  // Inserting address null to release existing address
  ASSERT_TRUE(j1939_network.insert(controller_3, jay::ADDRESS_NULL));
  ASSERT_EQ(j1939_network.get_address(controller_3), jay::ADDRESS_NULL);
  ASSERT_EQ(j1939_network.address_count(), 1);
  ASSERT_EQ(j1939_network.controller_count(), 4);

  //Catch insert exception
  ASSERT_THROW(j1939_network.insert(controller_2, jay::ADDRESS_GLOBAL), std::invalid_argument);
}

TEST(Jay_Network_Test, Jay_Network_Fill_Test)
{
  jay::network j1939_network{};
  ASSERT_FALSE(j1939_network.full());

  for(uint8_t i = 0; i < jay::ADDRESS_NULL; i++)
  {
    ASSERT_TRUE(j1939_network.insert(i, i));
  }

  ASSERT_EQ(j1939_network.address_count(), jay::ADDRESS_NULL);
  ASSERT_EQ(j1939_network.controller_count(), jay::ADDRESS_NULL);
  ASSERT_TRUE(j1939_network.full());
}

TEST(Jay_Network_Test, Jay_Network_Search_Test)
{
  jay::network j1939_network{};
  ASSERT_FALSE(j1939_network.full());

  uint64_t controller{200};
  uint8_t address = 100;

  uint64_t ctrl{100};
  for(uint8_t i = 0; i < jay::ADDRESS_NULL; i++)
  {
    ASSERT_TRUE(j1939_network.insert(ctrl, i));
    ctrl++;
  }

  ASSERT_EQ(j1939_network.find_address(0), jay::ADDRESS_NULL);

  ASSERT_EQ(j1939_network.get_address(controller), address);
  j1939_network.remove(controller);
  ASSERT_TRUE(j1939_network.available(address));

  ASSERT_EQ(j1939_network.find_address(0), address);
  ASSERT_EQ(j1939_network.find_address(0, address + 1), address);

  ASSERT_TRUE(j1939_network.insert(controller, address));

  //Claim first address
  ASSERT_EQ(j1939_network.find_address(0, 0, true), 0);
  ASSERT_EQ(j1939_network.find_address(controller, 0, true), address + 1);
}