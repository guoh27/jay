//
// Copyright (c) 2024 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef JAY_ADDRESS_CLAIMER_H
#define JAY_ADDRESS_CLAIMER_H

#pragma once

// C++
#include <cstdlib>//std::rand
#include <string>//std::string

// Lib
#include "boost/asio/deadline_timer.hpp"//boost::asio::deadline_timer
#include "boost/asio/io_context.hpp"//boost::asio::io_context
#include "boost/asio/post.hpp"//boost::asio::post
#include "boost/asio/strand.hpp"//boost::asio::strand

// Local
#include "address_state_machine.hpp"// jay::address_state_machine

namespace jay {

/**
 * @brief Wrapper class for sml state machine that implements timeout events
 * as i have not found a way to implement timeouts internaly in address claimer
 * @todo instead of using a timer, maybe we can use a tick function instead?
 * @note should be multi-thread safe by using strands, just make sure to not to change the callbacks
 * after starting the state machine and processing events.
 * @todo Do we need the strand?
 *
 */
class address_claimer
{
public:
  /**
   * @brief Callbacks used by the address manager
   */
  struct callbacks
  {
    // Called when a local controller has claimed an address
    std::function<void(const jay::name, const std::uint8_t)> on_address;

    // Called when a local controller loses their claimed an address
    std::function<void(const jay::name)> on_lose_address;

    // Called when a claim frame or cannot claim frame needs to be sent
    std::function<void(jay::frame)> on_frame;

    // Called when an internal error occurs, used for debugging
    std::function<void(const std::string, const boost::system::error_code &)> on_error;
  };

  /**
   * @brief Constructor
   * @param context from boost asio
   * @param context name to claim address for
   * @param network containing name address pairs
   * @note remember to add callbacks for getting data out of object
   */
  address_claimer(boost::asio::io_context &context,
    const jay::name name,
    const std::uint8_t preffered_address,
    jay::network &network)
    : strand_(boost::asio::make_strand(context)), network_(network), callbacks_(),
      global_state_machine_data_(name, preffered_address), claim_state_(), has_address_state_(),
      sml_address_state_machine_(global_state_machine_data_, network, claim_state_, has_address_state_),
      timeout_timer_(strand_)
  {
    bind_state_machine_callbacks();
    network.try_emplace(name);
  }

  /**
   * @brief Constructor with callbacks
   * @param context from boost asio
   * @param context name to claim address for
   * @param network containing name address pairs
   * @param callbacks for getting data out of object
   */
  address_claimer(boost::asio::io_context &context,
    const jay::name name,
    const std::uint8_t preffered_address,
    jay::network &network,
    const callbacks &callbacks)
    : strand_(boost::asio::make_strand(context)), network_(network), callbacks_(callbacks),
      global_state_machine_data_(name, preffered_address), claim_state_(), has_address_state_(),
      sml_address_state_machine_(global_state_machine_data_, network, claim_state_, has_address_state_),
      timeout_timer_(strand_)
  {
    bind_state_machine_callbacks();
    network.try_emplace(name);
  }

  /**
   * @brief Constructor with callbacks
   * @param context from boost asio
   * @param context name to claim address for
   * @param network containing name address pairs
   * @param callbacks for getting data out of object
   * @todo throw error if we fail to emplace name in network?
   */
  address_claimer(boost::asio::io_context &context,
    const jay::name name,
    const std::uint8_t preffered_address,
    jay::network &network,
    callbacks &&callbacks)
    : strand_(boost::asio::make_strand(context)), network_(network), callbacks_(std::move(callbacks)),
      global_state_machine_data_(name, preffered_address), claim_state_(), has_address_state_(),
      sml_address_state_machine_(global_state_machine_data_, network, claim_state_, has_address_state_),
      timeout_timer_(strand_)
  {
    bind_state_machine_callbacks();
    network.try_emplace(name);
  }

  /**
   * @brief Set the callbacks for the address mananger
   * @param callbacks for getting data out of the object
   */
  inline void bind_callbacks(const callbacks &callbacks) noexcept { callbacks_ = callbacks; }

  /**
   * @brief set the callbacks for the address mananger
   * @param callbacks for getting data out of the object
   * @todo should we place the move into a strand post?
   */
  inline void bind_callbacks(callbacks &&callbacks) noexcept { callbacks_ = std::move(callbacks); }

  /**
   * @brief Get the name object
   * @return jay::name
   */
  [[nodiscard]] inline jay::name get_name() const noexcept { return global_state_machine_data_.get_name(); }

  /**
   * @brief Process a 1939 frame, containing address claim or request
   *
   * @param frame containing address claim or request
   * @note frames are processed on local strand
   */
  inline void process(const jay::frame &frame)
  {
    if (frame.header.is_claim()) {
      claim(jay::name(frame.payload), frame.header.source_adderess());
    } else if (frame.header.is_request()) {
      request(frame.header.source_adderess());
    }
  }

  /**
   * @brief H
   *
   * @param name
   * @param destination_address
   * @param address_claimed
   */
  inline void claim(const jay::name name, const std::uint8_t address_claimed)
  {
    boost::asio::post(
      strand_, [this, name, address_claimed]() -> void { on_frame_address_claim(name, address_claimed); });
  }

  /**
   * @brief
   *
   * @param destination_address
   */
  inline void request(const std::uint8_t destination_address = J1939_NO_ADDR)
  {
    boost::asio::post(strand_, [this, destination_address]() -> void {
      sml_address_state_machine_.process_event(jay::address_state_machine::ev_address_request{ destination_address });
    });
  }

private:
  /**
   * @internal
   * @brief
   */
  void bind_state_machine_callbacks() noexcept
  {
    global_state_machine_data_.bind_callbacks(
      jay::address_state_machine::callbacks{ [this](auto name, auto address) -> void { on_address(name, address); },
        [this](auto name) -> void { on_address_loss(name); },
        [this]() -> void { on_begin_claiming(); },
        [this](auto name, auto address) -> void { on_address_claim(name, address); },
        [this](auto name) -> void { on_cannot_claim(name); } });
  }

  /**
   * @internal
   * @brief Inserts address claims into network and converts claim to state machine event
   *
   * @param name of the controller sending the claim
   * @param address_claimed by the name
   * @todo see if we can refactor the adding of new controllers to the network.
   * One way could be to have a callback that is called when a new controller is added
   * to the network in the network class. Or let the user handle it.
   * But then they would need to process the address claim frames themself as well as in this class.
   * The only down side with having it in the network class is that we would need to ignore when we add
   * our self to the network.
   */
  void on_frame_address_claim(const jay::name name, const std::uint8_t address_claimed)
  {
    if (!network_.try_address_claim(name, address_claimed)) {
      if (callbacks_.on_error) { callbacks_.on_error("on_frame_address_claim", boost::asio::error::address_in_use); }
      return;
    }
    jay::address_state_machine::ev_address_claim claim{ name, address_claimed };
    sml_address_state_machine_.process_event(claim);
  }

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                   Address claimer callbacks                    @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * @brief Callbacks for when a state machine has aquired an address
   *
   * @param name of controller in state machine
   * @param address claimed by state machine
   */
  void on_address(const jay::name name, const std::uint8_t address)
  {
    /// Should not fail as the address state machine should have checked if the address was valid!
    if (network_.try_address_claim(name, address)) {
      if (callbacks_.on_error) { callbacks_.on_error("on_address", boost::asio::error::address_in_use); }
      return;
    }
    if (callbacks_.on_address) { callbacks_.on_address(name, address); }
  }

  /**
   * @brief Callbacks for when a state machine has lost their address
   *
   * @param name of controller in state machine
   */
  inline void on_address_loss(const jay::name name) noexcept
  {
    /// Might not need to call release as the incomming claim frame should have released the address
    network_.release(name);
    if (callbacks_.on_lose_address) { callbacks_.on_lose_address(name); }
  }

  /**
   * @brief Callbacks for when a state machine has started claiming and address
   *
   * @param name of controller in state machine
   * @todo Claiming addressess between 0 - 127 and 248 253 may omit 250ms delay
   */
  void on_begin_claiming()
  {
    timeout_timer_.expires_from_now(boost::posix_time::millisec(250));
    timeout_timer_.async_wait([this](auto error_code) {
      if (error_code) { return on_fail("on_claim_timeout", error_code); }
      sml_address_state_machine_.process_event(jay::address_state_machine::ev_timeout{});
    });
  }

  /**
   * @brief Callbacks for when a state machine needs to send address claiming frames
   *
   * @param name of controller in state machine
   * @param address to claim
   */
  inline void on_address_claim(const jay::name name, const std::uint8_t address) const noexcept
  {
    if (callbacks_.on_frame) { callbacks_.on_frame(jay::frame::make_address_claim(name, address)); }
  }

  /**
   * @brief Callbacks for when a state machine needs to send cannot claim address frames
   * @param name of controller in state machine
   */
  void on_cannot_claim(const jay::name name)
  {
    auto rand_delay = std::rand() % 153;// Add a random 0 - 153 ms delay
    timeout_timer_.expires_from_now(boost::posix_time::millisec(rand_delay));
    timeout_timer_.async_wait([name, this](auto error_code) {
      if (error_code) { return on_fail("on_claim_timeout", error_code); }
      callbacks_.on_frame(jay::frame::make_cannot_claim(static_cast<jay::payload>(name)));
    });
  }

  /**
   * @brief Checks the for ignorable errors. Errors cant be ignored
   * the error code is sent to the on_error callback
   *
   * @param what - function name the error happened in
   * @param error_code - containing information regarding the error
   */
  inline void on_fail(char const *what, const boost::system::error_code error_code) noexcept
  {
    // Don't report these
    if (error_code == boost::asio::error::operation_aborted) { return; }
    if (callbacks_.on_error) { callbacks_.on_error(what, error_code); }
  }

private:
  // Injected
  boost::asio::strand<boost::asio::io_context::executor_type> strand_;
  jay::network &network_;
  callbacks callbacks_;

  // Internal
  jay::address_state_machine global_state_machine_data_;
  jay::address_state_machine::st_claiming claim_state_;
  jay::address_state_machine::st_has_address has_address_state_;
  boost::sml::sm<jay::address_state_machine> sml_address_state_machine_;
  boost::asio::deadline_timer timeout_timer_;
};


}// namespace jay


#endif