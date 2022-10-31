

#include <iostream>

#include "boost/asio/io_context.hpp"
#include "boost/asio/signal_set.hpp"

#include "../include/jay/network.hpp"
#include "../include/jay/address_manager.hpp"

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

  // ------- Create CAN-Bus Acceptor ------- //

  auto j1939_network = std::make_shared<jay::network>();

  jay::address_manager addr_mngr {io_layer, *j1939_network, 
    jay::address_manager::callbacks{
      [](jay::name name, uint8_t address) -> void 
      {
        std::cout << std::hex << static_cast<uint64_t>(name) << " is new, with address: " << address << std::endl;
      },
      [](jay::name name, uint8_t address) -> void 
      {
        std::cout << std::hex << static_cast<uint64_t>(name) << " local ctrl gained address: " << address << std::endl;
      },
      [](jay::name name) -> void 
      {
        std::cout << std::hex << static_cast<uint64_t>(name) << " local ctrl lost address" << std::endl;
      },
      [](jay::frame frame) -> void 
      {
        std::cout << frame.to_string() << std::endl;
      },
      [](std::string what, std::string error) -> void 
      {
        std::cout << what << " " << error << std::endl;
      }
  }};

  io_layer.run();

  return 0;
}