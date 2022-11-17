//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#include <gtest/gtest.h>

#include "../include/jay/name.hpp"

#include <iostream>

TEST(Jay_Name_Test, Jay_Name_Create_Tests)
{
  jay::name name{};
  ASSERT_EQ(name, 0UL);

  name = jay::name{ 0x1AC85DU, 0x4FAU, 0x7U, 0x1AU, 0xDCU, 0x12U, 0x4U, 0x2U, 0x0U };
  ASSERT_EQ(name.identity_number(),       0x1AC85DU);
  ASSERT_EQ(name.manufacturer_code(),     0x4FAU);
  ASSERT_EQ(name.ecu_instance(),          0x7U);
  ASSERT_EQ(name.function_instance(),     0x1AU);
  ASSERT_EQ(name.function(),              0xDCU);
  ASSERT_EQ(name.device_class(),          0x12U);
  ASSERT_EQ(name.device_class_instace(),  0x4U);
  ASSERT_EQ(name.industry_group(),        0x2U);
  ASSERT_EQ(name.self_config_address(),   0x0U);

  jay::name name2{};

  name2.identity_number(0x1AC85DU).manufacturer_code(0x4FAU)
  .ecu_instance(0x7U).function_instance(0x1AU)
  .function(0xDCU).device_class(0x12U).device_class_instace(0x4U)
  .industry_group(0x2U).self_config_address(0x0U);

  ASSERT_EQ(name2.identity_number(),       0x1AC85DU);
  ASSERT_EQ(name2.manufacturer_code(),     0x4FAU);
  ASSERT_EQ(name2.ecu_instance(),          0x7U);
  ASSERT_EQ(name2.function_instance(),     0x1AU);
  ASSERT_EQ(name2.function(),              0xDCU);
  ASSERT_EQ(name2.device_class(),          0x12U);
  ASSERT_EQ(name2.device_class_instace(),  0x4U);
  ASSERT_EQ(name2.industry_group(),        0x2U);
  ASSERT_EQ(name2.self_config_address(),   0x0U);

  ASSERT_EQ(name, name2);

  name = jay::name{0xC880808480100000UL};
  ASSERT_EQ(name.identity_number(),       0x100000U);
  ASSERT_EQ(name.manufacturer_code(),     0x400U);
  ASSERT_EQ(name.ecu_instance(),          0x4U);
  ASSERT_EQ(name.function_instance(),     0x10U);
  ASSERT_EQ(name.function(),              0x80U);
  ASSERT_EQ(name.device_class(),          0x40U);
  ASSERT_EQ(name.device_class_instace(),  0x8U);
  ASSERT_EQ(name.industry_group(),        0x4U);
  ASSERT_EQ(name.self_config_address(),   0x1U);

}

TEST(Jay_Name_Test, Jay_Name_Payload_Tests)
{
  jay::name name1{0xC880808480200000UL};
  std::array<std::uint8_t, 8> array = name1;
  ///TODO: maybe add explicit when converting to array to force cast?

  ASSERT_EQ(array[0], 0x0U);
  ASSERT_EQ(array[1], 0x0U);
  ASSERT_EQ(array[2], 0x20U);
  ASSERT_EQ(array[3], 0x80U);
  ASSERT_EQ(array[4], 0x84U);
  ASSERT_EQ(array[5], 0x80U);
  ASSERT_EQ(array[6], 0x80U);
  ASSERT_EQ(array[7], 0xC8U);

  jay::name name2{array};
  ASSERT_EQ(name1, name2);
}