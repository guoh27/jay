//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no), 2025 Hong.Guo (hong.guo@advantech.com.cn)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/guoh27/jay
//

#include <iostream>

#include "boost/asio/io_context.hpp"
#include "boost/asio/signal_set.hpp"

#include "jay/address_manager.hpp"
#include "jay/j1939_connection.hpp"
#include "jay/network.hpp"
#include "jay/network_manager.hpp"

int main()
{
  boost::asio::io_context io_layer{};

  // ------- Setup Shutdown signal ------- //
  boost::asio::signal_set signals{ io_layer, SIGINT, SIGTERM };
  signals.async_wait([&io_layer](boost::system::error_code ec, int sig) {
    // Signal triggered by ctrl-c or shutdown, should cause program to terminate gracefully
    if (!ec) { io_layer.stop(); }
  });

  // ------- Create network components ------- //

  auto vcan0_network = std::make_shared<jay::network>("vcan0");
  auto conn = std::make_shared<jay::j1939_connection>(io_layer, vcan0_network);
  jay::network_manager net_mngr{ vcan0_network };
  jay::address_manager addr_mngr{ io_layer, jay::name{ 0x7758 }, vcan0_network };

  // ------- Connect components with callbacks ------- //

  net_mngr.insert(addr_mngr);
  net_mngr.set_callback([](jay::name name, uint8_t address) -> void {
    std::cout << std::hex << static_cast<uint64_t>(name) << " is new, with address: " << address << std::endl;
  });

  conn->on_start([](auto) { std::cout << "Listening for can messages..." << std::endl; });
  conn->on_close([](auto) { std::cout << "J1939 Connection closed" << std::endl; });
  conn->on_read([&net_mngr](auto frame) { net_mngr.process(frame); });
  conn->on_send([](auto frame) { std::cout << "Sent frame: " << frame.to_string() << std::endl; });
  conn->on_error([](auto what, auto ec) { std::cout << what << " " << ec.message() << std::endl; });


  addr_mngr.on_address_claimed([](jay::name name, uint8_t address) -> void {
    std::cout << std::hex << static_cast<uint64_t>(name) << " local ctrl gained address: " << address << std::endl;
  });

  addr_mngr.on_address_lost([](jay::name name) -> void {
    std::cout << std::hex << static_cast<uint64_t>(name) << " local ctrl lost address" << std::endl;
  });

  addr_mngr.on_frame([connection = std::weak_ptr<jay::j1939_connection>(conn)](jay::frame frame) -> void {
    std::cout << "Output frame: " << frame.to_string() << std::endl;
    if (auto shared = connection.lock(); shared) { shared->send_raw(frame); }
  });

  addr_mngr.on_error(
    [](std::string what, auto error) -> void { std::cout << what << " " << error.message() << std::endl; });

  // ------- Run context ------- //

  /// TODO: Insert filter for connection
  if (!conn->open()) { return -1; }

  conn->start();
  addr_mngr.start_address_claim(0x44);
  io_layer.run();

  return 0;
}