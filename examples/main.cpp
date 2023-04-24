//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#include <iostream>

#include "boost/asio/io_context.hpp"
#include "boost/asio/signal_set.hpp"

#include "../include/jay/network.hpp"
#include "../include/jay/address_manager.hpp"
#include "../include/jay/network_manager.hpp"

#include "j1939_connection.hpp"

int main()
{
  boost::asio::io_context io_layer{};

  // ------- Setup Shutdown signal ------- //
  boost::asio::signal_set signals{io_layer, SIGINT, SIGTERM};
  signals.async_wait([&io_layer](boost::system::error_code ec, int sig)
  {
    //Signal triggerd by ctrl-c or shutdown, should cause program to terminate gracefully
    if(!ec)
    {
      io_layer.stop();
    }
  });

  // ------- Create network components ------- //

  jay::network vcan0_network{"vcan0"};
  auto j1939_connection = std::make_shared<J1939Connection>(io_layer, vcan0_network);
  jay::network_manager net_mngr{vcan0_network};
  jay::address_manager addr_mngr {io_layer, jay::name{0x7758}, vcan0_network};

  // ------- Connect components with callbacks ------- //

  net_mngr.insert(addr_mngr);
  net_mngr.set_callback([](jay::name name, uint8_t address) -> void 
    {
      std::cout << std::hex << static_cast<uint64_t>(name) << " is new, with address: " << address << std::endl;
    });

  j1939_connection->SetCallbacks(J1939Connection::Callbacks{
      //J1939Connection -> OnStart Callback
      [](auto )
      {
        std::cout << "Listening for can messages..." << std::endl;
      },

      //J1939Connection -> OnDestroy Callback
      [](auto )
      {
        std::cout << "J1939 Connection closed" << std::endl;
      },

      //J1939Connection -> OnData Callback
      [&net_mngr](auto frame)
      {
        net_mngr.process(frame);
      },

      //J1939Connection -> OnFail Callback
      [](auto what, auto ec)
      {
        std::cout << what << " " << ec.message() << std::endl;
      }
  });

  addr_mngr.set_callbacks( 
    jay::address_manager::callbacks{
      [](jay::name name, uint8_t address) -> void 
      {
        std::cout << std::hex << static_cast<uint64_t>(name) << " local ctrl gained address: " << address << std::endl;
      },
      [](jay::name name) -> void 
      {
        std::cout << std::hex << static_cast<uint64_t>(name) << " local ctrl lost address" << std::endl;
      },
      [connection = std::weak_ptr<J1939Connection>(j1939_connection)](jay::frame frame) -> void 
      {
        std::cout << "Output frame: " << frame.to_string() << std::endl;
        if(auto shared = connection.lock(); shared)
        {
          return shared->SendRaw(frame);
        }
      },
      [](std::string what, std::string error) -> void 
      {
        std::cout << what << " " << error << std::endl;
      }
  });

  // ------- Run context ------- //

  ///TODO: Insert filter for connection
  if(!j1939_connection->Open({}))
  {
    return -1;
  }

  j1939_connection->Start();
  addr_mngr.start_address_claim(0x44);
  io_layer.run();

  return 0;
}