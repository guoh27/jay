//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no), 2025 Hong.guo (hong.guo@advantech.com.cn)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/guoh27/jay
//
#pragma once

// C++
#include <cstdlib>//rand
#include <random>//std::mt19937, std::uniform_int_distribution
#include <string>//std::string

// Lib
#include "boost/asio/deadline_timer.hpp"//boost::asio::deadline_timer
#include "boost/asio/io_context.hpp"//boost::asio::io_context
#include "boost/asio/post.hpp"//boost::asio::post
#include "boost/format.hpp"

// Local
#include "address_state_machine.hpp"
#include "j1939_type.hpp"
#include "network.hpp"

namespace jay {

namespace detail {
  template<class T> struct is_on_exit : std::false_type
  {
  };
  template<class S, class E> struct is_on_exit<boost::ext::sml::v1_1_11::back::on_exit<S, E>> : std::true_type
  {
  };

  template<class T> struct is_on_entry : std::false_type
  {
  };
  template<class S, class E> struct is_on_entry<boost::ext::sml::v1_1_11::back::on_entry<S, E>> : std::true_type
  {
  };

  // trait to detect static constexpr `tag` inside an action type
  template<class, class = void> struct has_static_tag : std::false_type
  {
  };
  template<class T> struct has_static_tag<T, std::void_t<decltype(T::tag)>> : std::true_type
  {
  };
}// namespace detail

/**
 * @brief Wrapper class for sml state machine that implements timeout events
 * as i have not found a way to implement timeouts internally in address claimer
 */
class address_claimer
{
public:
  /**
   * @brief Constructor
   * @param context from boost asio
   * @param context name to claim address for
   * @param network containing name address pairs
   * @note remember to add callbacks for getting data out of object
   */
  address_claimer(boost::asio::io_context &context, jay::name name, jay::network &network)
    : context_(context), network_(network), addr_claimer_(name), claim_state_(), has_address_state_(), logger_(on_log_),
      state_machine_(addr_claimer_,
        network_,
        claim_state_,
        has_address_state_,
        jay::address_state_machine::ev_start_claim{},
        logger_),
      timeout_timer_(context_)
  {
    addr_claimer_.set_callbacks(
      jay::address_state_machine::callbacks{ [this](auto name, auto address) -> void { on_address(name, address); },
        [this](auto name) -> void { on_address_loss(name); },
        [this]() -> void { on_begin_claiming(); },
        [this](auto name, auto address) -> void { on_address_claim(name, address); },
        [this]() { on_address_request(); },
        [this](auto name) -> void { on_cannot_claim(name); } });
  }

  void on_log(J1939OnLog log) { on_log_ = log; }

  /**
   * @brief Set the callbacks for address manager
   * @param callbacks to set
   */
  void on_address_claimed(std::function<void(jay::name, std::uint8_t)> on_address)
  {
    on_address_ = std::move(on_address);
  }

  /**
   * @brief Set the on lose address callback
   * @param on_lose_address callback to call when an address is lost
   */
  void on_address_lost(std::function<void(jay::name)> on_lose_address)
  {
    on_lose_address_ = std::move(on_lose_address);
  }

  /**
   * @brief Set the on frame callback
   * @param on_frame callback to call when a frame is ready to be sent
   */
  void on_frame(J1939OnFrame on_frame) { on_frame_ = std::move(on_frame); }

  /**
   * @brief Set the on error callback
   * @param on_error callback to call when an error occurs
   */
  void on_error(J1939OnError on_error) { on_error_ = std::move(on_error); }


  /**
   * @brief Get the name object
   * @return jay::name
   */
  jay::name get_name() const noexcept { return addr_claimer_.get_name(); }

  /**
   * @brief Process a 1939 frame, containing address claim or request
   *
   * @param frame containing address claim or request
   * @note frames are processed on local strand
   */
  void process(const jay::frame &frame)
  {
    if (frame.header.is_claim()) {
      process_claim(jay::name(frame.payload), frame.header.source_address());
    } else if (frame.header.is_request()) {
      process_request(frame.header.pdu_specific());
    }
  }

  /**
   * @brief H
   *
   * @param name
   * @param destination_address
   * @param address_claimed
   */
  inline void process_claim(const jay::name name, const std::uint8_t address_claimed)
  {
    if (!network_.insert(name, address_claimed)) {
      if (on_error_) { on_error_("on_frame_address_claim", boost::asio::error::address_in_use); }
      return;
    }
    address_claim(jay::address_state_machine::ev_address_claim{ name, address_claimed });
  }

  /**
   * @brief
   *
   * @param destination_address
   */
  void process_request(const std::uint8_t destination_address = J1939_NO_ADDR)
  {
    address_request(jay::address_state_machine::ev_address_request{ destination_address });
  }

  /**
   * @brief Start the address claiming process
   * @param preferred_address to claim
   * @note event is posted to context
   */
  void start_address_claim(std::uint8_t preferred_address)
  {
    if (!state_machine_.is(boost::sml::state<jay::address_state_machine::st_no_address>)) { return; }

    boost::asio::post(context_, [this, preferred_address]() -> void {
      state_machine_.process_event(jay::address_state_machine::ev_start_claim{ preferred_address });
    });
  }


  /**
   * @brief processes to address request event in state machine
   * @param request event
   * @note event is posted tp context
   */
  void address_request(jay::address_state_machine::ev_address_request request)
  {
    boost::asio::post(context_, [this, request]() -> void { state_machine_.process_event(request); });
  }


  /**
   * @brief processes an address claim event in state machine
   * @param claim event
   * @note event is posted tp context
   */
  void address_claim(jay::address_state_machine::ev_address_claim claim)
  {
    boost::asio::post(context_, [this, claim]() -> void { state_machine_.process_event(claim); });
  }

private:
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                   Address claimer callbacks                    @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * @brief Callbacks for when a state machine has acquired an address
   *
   * @param name of controller in state machine
   * @param address claimed by state machine
   */
  void on_address(jay::name name, std::uint8_t address)
  {
    network_.insert(name, address);
    if (on_address_) { on_address_(name, address); }
  }

  /**
   * @brief Callbacks for when a state machine has lost their address
   *
   * @param name of controller in state machine
   */
  void on_address_loss(jay::name name)
  {
    network_.release(name);
    if (on_lose_address_) { on_lose_address_(name); }
  }

  /**
   * @brief Callbacks for when a state machine has started claiming and address
   *
   * @param name of controller in state machine
   */
  void on_begin_claiming()
  {
    /// TODO: Note CAs between 0 - 127 and 248 253 may omit 250ms delay
    timeout_timer_.expires_from_now(boost::posix_time::millisec(250));
    timeout_timer_.async_wait([this](auto error_code) {
      if (error_code) { return on_fail("on_claim_timeout", error_code); }
      state_machine_.process_event(jay::address_state_machine::ev_timeout{});
    });
  }

  /**
   * @brief Callbacks for when a state machine needs to send address claiming frames
   *
   * @param name of controller in state machine
   * @param address to claim
   */
  void on_address_claim(jay::name name, std::uint8_t address)
  {
    if (on_frame_) on_frame_(jay::frame::make_address_claim(name, address));
  }

  void on_address_request()
  {
    if (on_frame_) on_frame_(jay::frame::make_address_request());
  }

  /**
   * @brief Callbacks for when a state machine needs to send cannot claim address frames
   * @param name of controller in state machine
   */
  void on_cannot_claim(jay::name name)
  {
    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(0, 150);
    auto rand_delay = dist(rng);// Add a random 0 - 150 ms delay
    timeout_timer_.expires_from_now(boost::posix_time::millisec(rand_delay));
    timeout_timer_.async_wait([name, this](auto error_code) {
      if (error_code) { return on_fail("on_claim_timeout", error_code); }
      if (on_frame_) on_frame_(jay::frame::make_cannot_claim(static_cast<jay::payload>(name)));

      if (name.self_config_address()) {
        boost::asio::post(
          context_, [this]() { state_machine_.process_event(jay::address_state_machine::ev_random_retry{}); });
      }
    });
  }

  /**
   * @brief Checks the for ignorable errors. Errors cant be ignored
   * the error code is sent to the on_error_ callback
   *
   * @param what - function name the error happened in
   * @param error_code - containing information regarding the error
   */
  void on_fail(char const *what, boost::system::error_code error_code)
  {
    // Don't report these
    if (error_code == boost::asio::error::operation_aborted) { return; }
    if (on_error_) { on_error_(what, error_code); }
  }

  struct address_claimer_logger
  {
    explicit address_claimer_logger(J1939OnLog &on_log) : on_log_(on_log) {}

    template<class SM, class Event> void log_process_event(const Event &)
    {
      if (on_log_) {
        boost::format f("[%1%] process %2%");
        on_log_((f % boost::sml::aux::get_type_name<SM>() % boost::sml::aux::get_type_name<Event>()).str());
      }
    }
    template<class SM, class Guard, class Event> void log_guard(const Guard &, const Event &, bool pass)
    {
      if (on_log_) {
        boost::format f("[%1%] guard  %2%  [%3%]");
        on_log_((
          f % boost::sml::aux::get_type_name<SM>() % boost::sml::aux::get_type_name<Guard>() % (pass ? "OK" : "Reject"))
                  .str());
      }
    }
    template<class SM, class Action, class Event> void log_action(const Action &, const Event &)
    {
      if (on_log_) {
        boost::format f("[%1%] action %2%");
        on_log_((f % boost::sml::aux::get_type_name<SM>() % boost::sml::aux::get_type_name<Action>()).str());
      }
    }
    template<class SM, class Src, class Dst> void log_state_change(const Src &s, const Dst &d)
    {
      if (on_log_) {
        boost::format f("[%1%] %2%  →  %3%");
        on_log_((f % boost::sml::aux::get_type_name<SM>() % s.c_str() % d.c_str()).str());
      }
    }

    J1939OnLog &on_log_;
  };


private:
  /// TODO: Instead of using timeout could use tick?

  // Injected
  boost::asio::io_context &context_;
  jay::network &network_;

  // Internal

  jay::address_state_machine addr_claimer_;
  jay::address_state_machine::st_claiming claim_state_;
  jay::address_state_machine::st_has_address has_address_state_;

  address_claimer_logger logger_;
  boost::sml::sm<jay::address_state_machine, boost::sml::logger<address_claimer_logger>> state_machine_;
  boost::asio::deadline_timer timeout_timer_;

  // Called when a local controller has claimed an address
  std::function<void(jay::name, std::uint8_t)> on_address_;
  // Called when a local controller loses their claimed an address
  std::function<void(jay::name)> on_lose_address_;
  // Called when a claim frame or cannot claim frame needs to be sent
  J1939OnFrame on_frame_;
  // Called when an internal error occurs, used for debugging
  J1939OnError on_error_;

  J1939OnLog on_log_;
};


}// namespace jay
