//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#include <iostream>
#include <memory>

#include "boost/asio/io_context.hpp"
#include "boost/asio/signal_set.hpp"

#include "../include/jay/address_claimer.hpp"
#include "../include/jay/filters.hpp"
#include "../include/jay/network.hpp"

#include "j1939_connection.hpp"

int main()
{
  boost::asio::io_context io_layer{};

  // ------- Setup Shutdown signal ------- //
  boost::asio::signal_set signals{ io_layer, SIGINT, SIGTERM };
  signals.async_wait([&io_layer](boost::system::error_code ec, int sig) {
    // Signal triggerd by ctrl-c or shutdown, should cause program to terminate gracefully
    if (!ec) { io_layer.stop(); }
  });

  // ------- Create network components ------- //
  jay::name local_device{ 0x7758 };
  jay::network vcan0_network{ "vcan0" };
  vcan0_network.on_new_name_callback(
    [](jay::name name) -> void { std::cout << std::hex << static_cast<uint64_t>(name) << " is new" << std::endl; });

  // ------- Connect components with callbacks ------- //

  /// Create the connection
  std::shared_ptr<j1939_connection> address_connection = std::make_shared<j1939_connection>(io_layer, vcan0_network);
  address_connection->local_name(local_device);

  /// Create the address claimer and attach the callbacks to the connection right away, as it will want to queue frames
  jay::address_claimer addr_mngr{ io_layer,
    local_device,
    0x44,
    vcan0_network,
    jay::address_claimer::callbacks{ [](jay::name name, uint8_t address) -> void {
                                      std::cout << std::hex << static_cast<uint64_t>(name)
                                                << " local ctrl gained address: " << address << std::endl;
                                    },
      [](jay::name name) -> void {
        std::cout << std::hex << static_cast<uint64_t>(name) << " local ctrl lost address" << std::endl;
      },
      [connection = std::weak_ptr<j1939_connection>(address_connection)](jay::frame frame) -> void {
        std::cout << "Output frame: " << frame.to_string() << std::endl;
        if (auto shared = connection.lock(); shared) { return shared->send_raw(frame); }
      },
      [](std::string what, auto error) -> void { std::cout << what << " " << error.message() << std::endl; } } };

  /// Create the connection callbacks, needed set after creating address claimer
  address_connection->bind_callbacks(j1939_connection::callbacks{ // J1939Connection -> OnStart Callback
    [](auto) { std::cout << "Listening for can messages..." << std::endl; },

    // J1939Connection -> OnDestroy Callback
    [](auto) { std::cout << "J1939 Connection closed" << std::endl; },

    // J1939Connection -> OnData Callback
    [&addr_mngr](auto frame) { addr_mngr.process(frame); },

    // J1939Connection -> OnSend Callback
    [](auto frame) { std::cout << "Sent frame: " << frame.to_string() << std::endl; },

    // J1939Connection -> OnFail Callback
    [](auto what, auto ec) { std::cout << what << " " << ec.message() << std::endl; } });

  // ------- Run context ------- //

  /// Will now only accept address claim and address request frames
  if (!address_connection->open({ jay::make_address_claim_filter(), jay::make_address_request_filter() })) {
    return -1;
  }

  /// Should now check if the none filter and broadcast frames are for us
  address_connection->start();
  io_layer.run();

  return 0;
}