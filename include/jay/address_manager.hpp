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
    std::function<void(std::string, std::string)> on_error;
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
    [this](auto ex)
    {
      on_claim_timout(ex);
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
    [name, this](auto ex)
    {
      on_random_timeout(jay::frame::make_cannot_claim(static_cast<jay::payload>(name)), ex);
    });
  }

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                  Timeout callbacks implementation              @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
  * @brief callback function triggered when delay has finished for
  * claiming an address in the network
  * @param name of the controller claiming its address
  * @param error code if issue came up while waiting
  */
  void on_claim_timout(const boost::system::error_code& error)
  {
    if(error)
    {
      if (error != boost::asio::error::operation_aborted)
      {
        if(callbacks_.on_error)
        {
          callbacks_.on_error("on_claim_timout", error.message());
        }
      }
    }
    state_machine_.process_event(jay::address_claimer::ev_timeout{});
  }

  /**
  * @brief callback function triggered when random delay has finished for
  * cannot claim address frame
  * @param frame containing cannot claim message
  * @param error code if issue came up while waiting for the delay
  */
  void on_random_timeout(const jay::frame& frame, const boost::system::error_code& error) const
  { 
    if(error)
    {
      if (error != boost::asio::error::operation_aborted)
      {
        if(callbacks_.on_error)
        {
          callbacks_.on_error("on_random_timeout", error.message());
        }
      }
    }
    callbacks_.on_frame(frame);
  }


private:

  ///TODO: Okay so for some reason the address claimer that is referenced in
  ///state machine the address claimer is empty. In the while the
  ///data in the map is still valid? not sure how this is happening.
  ///Maybe time to change back to vector, change the situator, by making controller its own
  ///class and ignoring the storage completly. Maybe another class that handles insertion
  ///into network and new controllers

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