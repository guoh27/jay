//
// Copyright (c) 2022 Bj\303\270rn Fuglestad, Jaersense AS
//
// Distributed under the Boost Software License, Version 1.0.
// Official repository: https://github.com/guoh27/jay
//

#include <gtest/gtest.h>

#include "jay/network.hpp"
#include "jay/address_claimer.hpp"

#include <chrono>
#include <queue>

class NetworkProcessTest : public testing::Test {
protected:
  NetworkProcessTest()
  {
    network = std::make_shared<jay::network>("vcan0");
  }

  void SetUp() override {}
  void TearDown() override { network->clear(); }

  std::shared_ptr<jay::network> network;
};

TEST_F(NetworkProcessTest, ManualProcess)
{
  boost::asio::io_context ctx;
  std::queue<jay::frame> out_queue{};

  jay::address_manager a1{ ctx, {0xAFFU}, network };
  a1.on_frame([&](jay::frame f) { out_queue.push(f); });

  jay::address_manager a2{ ctx, {0xBFFU}, network };
  a2.on_frame([&](jay::frame f) { out_queue.push(f); });

  // simulate address request broadcast
  a1.address_request(jay::address_claimer::ev_address_request{});
  a2.address_request(jay::address_claimer::ev_address_request{});
  ctx.run_for(std::chrono::milliseconds(300));
  ctx.restart();
  ASSERT_EQ(out_queue.size(), 2);
  out_queue.pop();
  out_queue.pop();

  // start claiming addresses
  a1.start_address_claim(0x10);
  a2.start_address_claim(0x11);
  ctx.run_for(std::chrono::milliseconds(300));
  ctx.restart();
  ASSERT_EQ(out_queue.size(), 2);
}

