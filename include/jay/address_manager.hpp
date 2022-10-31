//
// Copyright (c) 2020 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the MIT License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef JAY_ADDRESS_MANAGER_H
#define JAY_ADDRESS_MANAGER_H

#pragma once

//C++
#include <cstdlib>
#include <functional>
#include <string>

//Lib
#include "boost/asio/io_context.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/deadline_timer.hpp"

//Local
#include "address_claimer.hpp"

namespace jay
{

class address_manager
{
public:
 
  using self = address_manager;

  /**
  * @brief Callbacks used by the address manager
  */
  struct callbacks
  {
    //Called when a controller that is not in the network is found
    std::function<void(jay::name, uint8_t)> on_new_controller;
    
    //Called when a local controller has claimed an address
    std::function<void(jay::name, uint8_t)> on_address;
    
    //Called when a local controller loses their claimed an address
    std::function<void(jay::name)> on_lose_address;
    
    //Called when a claim frame or cannot claim frame needs to be sent
    std::function<void(jay::frame)> on_frame;
    
    //Called when an internal error occurs, used for debugging
    std::function<void(std::string, std::string)> on_error;
  };

  /**
  * @brief Constructor
  */
  address_manager(boost::asio::io_context& context, jay::network& network) : 
    context_(context), network_(network)
  {

  }

  /**
  * @brief Constructor with callbacks
  */
  address_manager(boost::asio::io_context& context, jay::network& network, callbacks&& callbacks) : 
    context_(context), network_(network), callbacks_(std::move(callbacks))
  {

  }

  /**
  * @brief set the callbacks for the address mananger
  */
  void set_callbacks(callbacks&& callbacks)
  {
    callbacks_ = std::move(callbacks);
  }

  /**
  * @brief Creates a controller object with a state machine for aquiring an address
  * @param name of the controller
  * @param preffered_address to claim
  */
  void aquire(jay::name name, uint8_t preffered_address)
  {
    boost::asio::post(context_, std::bind(&address_manager::post_aquire, this, name, preffered_address));
  }

  /**
  * @brief Processes address claim and address request frames
  * by tuning them into events and passing them to the state machine
  * @note also registes new controllers into the new and updates their address
  * @param frame containing and address claim or address request, other frames are ignored
  */
  void process(const jay::frame& frame)
  {
    if(frame.header.pgn() == jay::PGN_ADDRESS_CLAIM) //Address claim
    {
      jay::name name(frame.payload);
      boost::asio::post(context_, std::bind(&address_manager::post_address_claim, this, 
        name, frame.header.source_adderess()));
      return;
    }

    if(frame.header.pgn() == jay::PGN_REQUEST_ADDRESS) //Address request
    {
      boost::asio::post(context_, std::bind(&address_manager::post_address_request, this));
    }
  }

private:

  void post_aquire(jay::name name, uint8_t preffered_address)
  {
    if(network_.in_network(name))
    {
      return;
    }

    auto id = vec_controllers_.size();
    vec_controllers_.emplace_back(context_, network_, name, 
    jay::address_claimer::callbacks{
      [this](jay::name name, uint8_t address) -> void 
      { //On_address
        network_.insert(name, address);
        if(callbacks_.on_address){callbacks_.on_address(name, address);}
      },
      [this](jay::name name) -> void 
      { //On_loss
        network_.release(name);
        if(callbacks_.on_lose_address){callbacks_.on_lose_address(name);}
      },
      [id, this]() -> void 
      { //on_begin_claiming
        vec_controllers_[id].timeout_timer.expires_from_now(boost::posix_time::millisec(250));
        vec_controllers_[id].timeout_timer.async_wait(
        [id, this](auto ex)
        {
          on_claim_timout(id, ex);
        });
      },
      [this](jay::frame frame) -> void 
      {
        ///NOTE: Should address claims here be registered in network?
        callbacks_.on_frame(frame);
      },
      [id, this](jay::frame frame) -> void 
      { //On_cannot_claim
        auto rand_delay = rand() % 153; //Add a random 0 -150 ms delay
        vec_controllers_[id].timeout_timer.expires_from_now(boost::posix_time::millisec(rand_delay));
        vec_controllers_[id].timeout_timer.async_wait(
        [frame, this](auto ex)
        {
          on_random_timeout(frame, ex);
        });
      }
    });

    vec_controllers_[id].state_machine.process_event(jay::address_claimer::ev_start_claim{preffered_address});
  }

  void post_address_claim(jay::name name, uint8_t source_adderess)
  {
    if(source_adderess < jay::ADDRESS_GLOBAL)
    {
      auto in = network_.in_network(name);
      network_.insert(name, source_adderess);
      if(!in)
      { //Insert controller then notify with callback
        if(callbacks_.on_new_controller)
        {
          callbacks_.on_new_controller(
            name, source_adderess);
        }
      }
    }
    jay::address_claimer::ev_address_claim claim{
      name, source_adderess
    };
    for(auto& ctrl : vec_controllers_)
    {
      ctrl.state_machine.process_event(claim);
    }
  }

  void post_address_request()
  {
    jay::address_claimer::ev_address_request req{};
    for(auto& ctrl : vec_controllers_)
    {
      ctrl.state_machine.process_event(req);
    }
  }

  /**
  * @brief callback function triggered when delay has finished for
  * claim an address in the network
  * @param id of the controller claiming its address
  * @param error code if issue came up while waiting
  */
  void on_claim_timout(size_t id, const boost::system::error_code& error)
  {
    if(error)
    {
      if (error != boost::asio::error::operation_aborted)
      {
        if(callbacks_.on_error)
        {
          callbacks_.on_error("where: on_claim_timout", error.message());
        }
      }
    }
    vec_controllers_[id].state_machine.process_event(jay::address_claimer::ev_timeout{});
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
          callbacks_.on_error("where: on_random_timeout", error.message());
        }
      }
    }
    callbacks_.on_frame(frame);
  }


private:

  /**
  * Internal structure for combinging
  * address claimer, state machine and timer.
  * Usesful for placing in containers
  */
  struct controller
  {
    controller(boost::asio::io_context& context, 
      jay::network& network, jay::name name) : 
      addr_claimer(name), 
      state_machine(addr_claimer, network),
      timeout_timer(context)
    {
    }

    controller(boost::asio::io_context& context, jay::network& network, 
      jay::name name, jay::address_claimer::callbacks&& callbacks) : 
      addr_claimer(name, std::move(callbacks)), 
      state_machine(addr_claimer, network),
      timeout_timer(context)
    {
    }

    jay::address_claimer addr_claimer;
    boost::sml::sm<jay::address_claimer> state_machine;
    boost::asio::deadline_timer timeout_timer;
  };

private:

  //Injected

  boost::asio::io_context& context_;
  jay::network& network_;
  callbacks callbacks_;

  // Internal

  std::vector<controller> vec_controllers_{};

};


} //namespace jay


#endif

