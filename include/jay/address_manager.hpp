//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef JAY_ADDRESS_MANAGER_H
#define JAY_ADDRESS_MANAGER_H

#pragma once

//C++
#include <cstdlib>  //rand
#include <string>   //std::string

//Lib
#include "boost/asio/io_context.hpp"     //boost::asio::io_context
#include "boost/asio/post.hpp"           //boost::asio::post
#include "boost/asio/deadline_timer.hpp" //boost::asio::deadline_timer

//Local
#include "address_claimer.hpp"

namespace jay
{

/**
 * @brief Wrapper class for sml state machine that implements timeout events 
 * as i have not found a way to implement timeouts internaly in address claimer
 */
class address_manager
{
public:
 
  /**
  * @brief Callbacks used by the address manager
  */
  struct callbacks
  {   
    //Called when a local controller has claimed an address
    std::function<void(jay::name, std::uint8_t)> on_address;
    
    //Called when a local controller loses their claimed an address
    std::function<void(jay::name)> on_lose_address;
    
    //Called when a claim frame or cannot claim frame needs to be sent
    std::function<void(jay::frame)> on_frame;
    
    //Called when an internal error occurs, used for debugging
    std::function<void(std::string, const boost::system::error_code&)> on_error;
  };

  /**
  * @brief Constructor
  * @param context from boost asio
  * @param context name to claim address for
  * @param network containing name address pairs
  * @note remember to add callbacks for getting data out of object
  */
  address_manager(boost::asio::io_context& context, jay::name name, jay::network& network) : 
    context_(context), network_(network), addr_claimer_(name), 
    state_machine_(addr_claimer_, network), timeout_timer_(context)
  {
    addr_claimer_.set_callbacks(jay::address_claimer::callbacks{
      [this](auto name, auto address) -> void {on_address(name, address);},
      [this](auto name) -> void               {on_address_loss(name);},
      [this]() -> void                        {on_begin_claiming();},
      [this](auto name, auto address) -> void {on_address_claim(name, address);},
      [this](auto name) -> void               {on_cannot_claim(name);}
    });
  }

  /**
  * @brief Constructor with callbacks
  * @param context from boost asio
  * @param context name to claim address for
  * @param network containing name address pairs
  * @param callbacks for getting data out of object
  */
  address_manager(boost::asio::io_context& context, jay::name name, jay::network& network, callbacks&& callbacks) : 
    context_(context), network_(network), addr_claimer_(name), 
    state_machine_(addr_claimer_, network), timeout_timer_(context), 
    callbacks_(std::move(callbacks))
  {
    addr_claimer_.set_callbacks(jay::address_claimer::callbacks{
      [this](auto name, auto address) -> void {on_address(name, address);},
      [this](auto name) -> void               {on_address_loss(name);},
      [this]() -> void                        {on_begin_claiming();},
      [this](auto name, auto address) -> void {on_address_claim(name, address);},
      [this](auto name) -> void               {on_cannot_claim(name);}
    });
  }

  /**
  * @brief set the callbacks for the address mananger
  * @param callbacks for getting data out of the object
  */
  void set_callbacks(callbacks&& callbacks)
  {
    callbacks_ = std::move(callbacks);
  }

  /**
   * @brief Get the name object
   * @return jay::name 
   */
  jay::name get_name() const noexcept
  {
    return addr_claimer_.get_name();
  }

  /**
   * @brief Start the address claiming process
   * @param preffered_address to claim
   * @note event is posted to context
   */
  void start_address_claim(std::uint8_t preffered_address)
  {
    if(!state_machine_.is(boost::sml::state<jay::address_claimer::st_no_address>))
    {
      return;
    }

    boost::asio::post(context_, [this, preffered_address]() -> void {
      state_machine_.process_event(
        jay::address_claimer::ev_start_claim{preffered_address});
    });
  }

  /**
   * @brief processes to address request event in state machine
   * @param request event
   * @note event is posted tp context
   */
  void address_request(jay::address_claimer::ev_address_request request)
  {
    boost::asio::post(context_, [this, request]() -> void {
      state_machine_.process_event(request);
    });
  }

  /**
   * @brief processes an address claim event in state machine
   * @param claim event
   * @note event is posted tp context
   */
  void address_claim(jay::address_claimer::ev_address_claim claim)
  {
      boost::asio::post(context_, [this, claim]() -> void {
      state_machine_.process_event(claim);
    });
  }

private:

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                   Address claimer callbacks                    @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * @brief Callbacks for when a state machine has aquired an address
   * 
   * @param name of controller in state machine
   * @param address claimed by state machine
   */
  void on_address(jay::name name, std::uint8_t address)
  {
    network_.insert(name, address);
    if(callbacks_.on_address){callbacks_.on_address(name, address);}
  }

  /**
   * @brief Callbacks for when a state machine has lost their address
   * 
   * @param name of controller in state machine
   */
  void on_address_loss(jay::name name)
  {
    network_.release(name);
    if(callbacks_.on_lose_address){callbacks_.on_lose_address(name);}
  }

  /**
   * @brief Callbacks for when a state machine has started claiming and address
   * 
   * @param name of controller in state machine
   */
  void on_begin_claiming()
  {
    ///TODO: Note CAs between 0 - 127 and 248 253 may omit 250ms delay
    timeout_timer_.expires_from_now(boost::posix_time::millisec(250));
    timeout_timer_.async_wait(
    [this](auto error_code)
    {
      if(error_code){return on_fail("on_claim_timeout", error_code);}
      state_machine_.process_event(jay::address_claimer::ev_timeout{});
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
    if(callbacks_.on_frame){callbacks_.on_frame(
      jay::frame::make_address_claim(name, address));}
  }

  /**
   * @brief Callbacks for when a state machine needs to send cannot claim address frames
   * @param name of controller in state machine
   */
  void on_cannot_claim(jay::name name)
  {
    auto rand_delay = rand() % 153; //Add a random 0 -150 ms delay
    timeout_timer_.expires_from_now(boost::posix_time::millisec(rand_delay));
    timeout_timer_.async_wait(
    [name, this](auto error_code)
    {
      if(error_code){return on_fail("on_claim_timeout", error_code);}
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
  void on_fail(char const *what, boost::system::error_code error_code)
  {
    // Don't report these
    if (error_code == boost::asio::error::operation_aborted) {return;}
    if (callbacks_.on_error){ callbacks_.on_error(what, error_code); }
  }


private:

  ///TODO: Instead of using timeout could use tick?


  //Injected
  boost::asio::io_context& context_;
  jay::network& network_;

  //Internal

  jay::address_claimer addr_claimer_;
  boost::sml::sm<jay::address_claimer> state_machine_;
  boost::asio::deadline_timer timeout_timer_;

  callbacks callbacks_;

};


} //namespace jay


#endif