//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef JAY_NETWORK_MANAGER_H
#define JAY_NETWORK_MANAGER_H

#pragma once

// C++

// Lib

// Local
#include "address_claimer.hpp"

namespace jay {

class service
{
public:
  /**
   * @brief Callbacks used by the service
   */
  struct callbacks
  {
    // Called when a new controller / ecu is added
    std::function<void(jay::name, std::uint8_t)> on_new_controller;

    // Called when a claim frame or cannot claim frame needs to be sent
    std::function<void(jay::frame)> on_frame;

    // Called when an internal error occurs, used for debugging
    std::function<void(const std::string, const boost::system::error_code &)> on_error;
  };

  /**
   * @brief Construct a new service object
   * @param network
   */
  service(boost::asio::io_context &io_context, const std::string &interface_name)
    : io_context_(io_context), network_(interface_name)
  {}

  /**
   * @brief Construct a new service object
   * @param network
   * @param on_new_controller callback when new controllers / ecu s are added
   */
  service(boost::asio::io_context &io_context, const std::string &interface_name, callbacks callbacks)
    : io_context_(io_context), network_(interface_name), callbacks_(callbacks)
  {}

  inline void bind_callback(callbacks new_callbacks) noexcept { callbacks_ = new_callbacks; }

  /**
   * @brief Inserts address manager into internal map
   * @param addr_man to insert
   */
  void register_name(const jay::name name,
    const uint8_t address,
    std::function<void(const jay::name, const std::uint8_t)> on_address,
    std::function<void(const jay::name)> on_lose_address)
  {
    // Check if name already exists
    for (auto &claimer : address_claimers) {
      if (claimer.get_name() == name) { return; }
    }
    assert(callbacks_.on_frame);
    assert(callbacks_.on_error);
    address_claimers.emplace_back(io_context_,
      name,
      network_,
      address_claimer::callbacks{ on_address, on_lose_address, callbacks_.on_frame, callbacks_.on_error });
  }

  /**
   * @brief Processes address claim and address request frames
   * by tuning them into events and passing them to the state machine
   * @param frame containing and address claim or address request, other frames are ignored
   * @note also registes new controllers into the newtork and updates their address
   */
  inline void process(const jay::frame &frame)
  {
    for (auto &claimer : address_claimers) { claimer.process(frame); }
  }


  /**
   * @brief Number of controller addresses being managed
   * @return std::size_t
   */
  [[nodiscard]] inline std::size_t size() const noexcept { return address_claimers.size(); }

  /**
   * @brief Check if there are any controllers being managed
   * @return bool
   */
  [[nodiscard]] inline bool empty() const noexcept { return address_claimers.empty(); }

  /**
   * @brief Get the read access to network
   * @return const jay::network&
   */
  [[nodiscard]] inline const jay::network &get_network() const noexcept { return network_; }

private:
  // Injected
  boost::asio::io_context &io_context_;

  jay::network network_;
  callbacks callbacks_;

  // Internal

  std::vector<address_claimer> address_claimers{};
};


}// namespace jay

#endif