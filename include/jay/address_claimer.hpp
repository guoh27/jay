//
// Copyright (c) 2020 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the MIT License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef JAY_STATE_MACHINE_H
#define JAY_STATE_MACHINE_H

#pragma once

//C++
#include <functional>

//Lib
#include "boost/sml.hpp"

//Local
#include "network.hpp"
#include "frame.hpp"
#include "name.hpp"


namespace jay
{

/**
* boost::sml state machine class for dynamic j1939 address claiming
* @todo finish documentation
* @todo finish send_cannot_claim
* @todo finish send_release_claim
* @todo finish valid_address
*/
class address_claimer
{
public:
  using self = address_claimer;

  /**
  * @brief Callbacks for generating outputs
  */
  struct callbacks
  {
    std::function<void(jay::name, uint8_t)> on_address;
    std::function<void(jay::name)> on_lose_address;
    std::function<void(void)> on_begin_claiming;
    std::function<void(jay::frame)> on_address_claim;
    std::function<void(jay::frame)> on_cannot_claim;
  };

  /**
  * @brief Constructor
  * @param name to get an address for
  */
  address_claimer(name name) : 
    name_(name), callbacks_()
  {}

  /**
  * @brief Constructor
  * @param name to get an address for
  * @param callbacks
  */
  address_claimer(name name, callbacks callbacks) : 
    name_(name), callbacks_(callbacks)
  {}

  /**
  * @brief Set callbacks
  * @param callbacks
  */
  void set_callbacks(callbacks callbacks)
  {
    callbacks_ = callbacks;
  }

  /**
  * @brief get the name that we are claiming and address for
  * @return name
  */
  name get_name() const noexcept
  {
    return name_;
  }

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                             States                             @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  
  /**
  * Starting state of the state machine
  * In this state the name has no address
  */
  struct st_no_address{};

  /**
  * In this state the name is in the process of claiming and address
  */
  struct st_claiming{
    uint8_t address{ADDRESS_NULL};
  };

  /**
  * In this state the name has an address
  */
  struct st_has_address{
    uint8_t address{ADDRESS_NULL};
  };

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                                 Events                         @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
  * @brief Event used to start the address claiming process
  */
  struct ev_start_claim
  {
    uint8_t pref_address{};
  };

  /**
  * @brief Event used when recieving an address claim from another controller/device
  */
  struct ev_address_claim
  { 
    uint64_t name{}; 
    uint8_t address{};
  };

  /**
  * @brief Event used when another controller/device is requesting our address
  */
  struct ev_address_request{};

  /**
  * @brief Event used when time has run out for completing 
  */
  struct ev_timeout{};

private:

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                             Guards                             @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  
  /**
  * @brief Checks if no addresses are awailable to claim
  * @param network to check for available addresses
  */
  bool no_address_available(const network & network) const
  {
    return network.full();
  }
  
  /**
  * @brief Checks if addresses are awailable to claim
  * @param network to check for available addresses
  */
  bool address_available(const network & network) const
  {
    return !network.full();
  }
  
  /**
  * @brief Check if our name has priority
  * @param name to check local name agains
  * @return false if local name is larger
  * @return true if local name is less
  */
  bool address_priority(jay::name name) const
  {
    return name_ < name;
  }

  /**
  * @brief Checks if addresses conflick
  * @param l_address
  * @param r_address
  * @return false if addresses are not the same
  * @return true if addresses are the same
  */
  bool address_conflict(uint8_t l_address, uint8_t r_address) const
  {
    return l_address == r_address;
  }

  /**
  * @brief Checks if addresses change is requred
  * @param name to check local name agains
  * @param l_address address assiosiated with name
  * @param r_address local address
  * @return false if addresses does not conflict or if local name has priority
  * @return true if addresses conflict and local name does not have priority
  */
  bool address_change_required(jay::name name, uint8_t l_address, uint8_t r_address) const
  {
    return address_conflict(l_address, r_address) && !address_priority(name);
  }

  ///

  /**
  * @brief Check if claiming state has proirity over address claim
  * @param claiming state
  * @param address_claim event
  * @return false if
  * @return true if 
  */
  bool claiming_priority(st_claiming& claiming, 
    const ev_address_claim& address_claim) const
  {
    return address_conflict(claiming.address, address_claim.address) 
      && address_priority(address_claim.name);
  }

  /**
  * @brief Check if claiming state has proirity over address claim event and that
  * new addresses are avaialble
  * @param claiming state
  * @param address_claim event
  * @param network 
  * @return false if address change not required or no addresses are available
  * @return true if address change is required and addresses are available
  */
  bool claiming_loss(st_claiming& claiming, 
    const ev_address_claim& address_claim, const network & network) const
  {
    return address_change_required(address_claim.name, address_claim.address, 
      claiming.address) && address_available(network);
  }

  /**
  * @brief Check if claiming state loses proirity over address claim event and
  * if there are no available addresses
  * @param claiming state
  * @param address_claim event
  * @param network
  * @return false if address change not required or addresses are available
  * @return true if address change is required and no addresses are available
  */
  bool claiming_failure(st_claiming& claiming, 
    const ev_address_claim& address_claim, const network & network)
  {
    return address_change_required(address_claim.name, address_claim.address, 
      claiming.address) && no_address_available(network);
  }

  /**
  * @brief Check if claimed state has proirity over address claim event
  * @param has_address state
  * @param address_claim event
  * @return false if
  * @return true if 
  */
  bool claimed_priority(st_has_address& has_address, 
    const ev_address_claim& address_claim) const
  {
    return address_conflict(has_address.address, address_claim.address) 
      && address_priority(address_claim.name);
  }

  /**
  * @brief Check if claimed state has proirity over address claim event and that
  * new addresses are avaialble
  * @param has_address state
  * @param address_claim event
  * @param network 
  * @return false if address change not required or no addresses are available
  * @return true if address change is required and addresses are available
  */
  bool claimed_loss(st_has_address& has_address, 
    const ev_address_claim& address_claim, const network & network) const 
  {
    return address_change_required(address_claim.name, address_claim.address, 
      has_address.address) && address_available(network);
  }

  /**
  * @brief Check if claimed state loses proirity over address claim event and
  * if there are no available addresses
  * @param has_address state
  * @param address_claim event
  * @param network
  * @return false if address change not required or addresses are available
  * @return true if address change is required and no addresses are available
  */
  bool claimed_failure(st_has_address& has_address, 
    const ev_address_claim& address_claim, const network & network) const
  {
    return address_change_required(address_claim.name, address_claim.address, 
      has_address.address) && no_address_available(network);
  }

  /**
  * @brief Check if that claiming address is available in network
  * @param claiming state
  * @param network
  * @return false if claiming address is not available in network and name
  * does not have an address in the network
  * @return true if claiming address is available in network or name
  * already has an address in the network
  */
  bool valid_address(st_claiming& claiming, const network & network) const
  {
    ///TODO: Make sure that its possible to take address we have priority over
    ///TODO: Add an || and check if name is registered in the network with an address?
    return network.available(claiming.address) || 
      network.get_address(name_) != jay::ADDRESS_NULL;
  }

  /**
  * @brief Check if that claiming address is not available in network
  * @param claiming state
  * @param network
  * @return false if claiming address is available in network or name
  * already has an address in the network 
  * @return true if claiming address is not available in network and and name
  * does not have an address in the network
  */
  bool no_valid_address(st_claiming& claiming, const network & network) const
  {
    return !network.available(claiming.address) && 
      network.get_address(name_) == jay::ADDRESS_NULL;
  }

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                             Actions                            @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /// Data transfere actions

  /**
  * @brief Set preffered address when entering claiming state
  * @param claiming
  * @param ev_st_claim
  */
  void set_pref_address(st_claiming& claiming, const ev_start_claim& ev_st_claim)
  {
    claiming.address = ev_st_claim.pref_address;
  }

  /**
  * @brief Set claimed address from claiming state to has_address state
  * @param src - claiming state
  * @param dst - has_address state
  */
  void set_claimed_address(st_claiming& src, st_has_address& dst)
  {
    dst.address = src.address;
  }

  /**
  * @brief Set claimed address from claiming state to has_address state
  * @param src - claiming state
  * @param dst - has_address state
  */
  void set_claiming_address(st_has_address& src, st_claiming& dst)
  {
    dst.address = src.address;
  }

  /// Output actions

  /**
  * @brief Sends an address claim to on frame callback
  * @param address to claim
  */
  void send_address_claim(uint8_t address) const
  {
    if(callbacks_.on_address_claim)
    { 
      callbacks_.on_address_claim(jay::frame::make_address_claim(
          address, static_cast<jay::payload>(name_)));
    }
  }

  /**
  * @brief Find an address and send address claim
  * @param claiming
  * @param network
  */
  void begin_claiming_address(st_claiming& claiming, const network & network) const
  {
    if(callbacks_.on_begin_claiming) { callbacks_.on_begin_claiming(); }
    claim_address(claiming, network);
  }

  /**
  * @brief Find an address and send address claim
  * @param claiming
  * @param network
  */
  void claim_address(st_claiming& claiming, const network & network) const
  {
    claiming.address = network.find_address(name_, claiming.address, false);
    send_address_claim(claiming.address);
  }

  /**
  * @brief
  * @param claiming
  */
  void send_claiming(st_claiming& claiming) const
  {
    send_address_claim(claiming.address);
  }

  /**
  * @brief
  * @param has_address
  */
  void send_claimed(st_has_address& has_address) const
  {
    send_address_claim(has_address.address);
  }

  /**
  * @brief
  */
  void send_cannot_claim() const
  {
    ///TODO: Requires a random 0 - 153 ms delay to prevent bus errors
    if(callbacks_.on_cannot_claim)
    {
      ///TODO: Implement making cannot_claim frame in frame.hpp
      callbacks_.on_cannot_claim(jay::frame::make_cannot_claim(static_cast<jay::payload>(name_)));
    }
  }

  /**
  * @brief Notify though callback which address has been claimed
  * @param has_address state
  */
  void notify_address_gain(st_has_address& has_address)
  {
    ///TODO: insert into network here?
    if(callbacks_.on_address)
    {
      callbacks_.on_address(name_, has_address.address);
    }
  };

  /**
  * @brief Notify though callback that address has been lost
  */
  void notify_address_loss()
  {
    if(callbacks_.on_lose_address)
    {
      callbacks_.on_lose_address(name_);
    }
  };

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                       Transition table                         @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

public:



  /**
  * Crates trasition table
  */
  auto operator()() const {
    return boost::sml::make_transition_table(
      
      /// Start without address, start_claim sets prefered address

      * boost::sml::state<st_no_address> + boost::sml::on_entry<boost::sml::_>[&self::no_address_available] / &self::send_cannot_claim,
      boost::sml::state<st_no_address> + boost::sml::event<ev_address_request>[&self::no_address_available] / &self::send_cannot_claim,
      boost::sml::state<st_no_address> + boost::sml::event<ev_start_claim>[&self::address_available] / &self::set_pref_address = boost::sml::state<st_claiming>,

      boost::sml::state<st_claiming> + boost::sml::on_entry<boost::sml::_> / &self::begin_claiming_address,
      boost::sml::state<st_claiming> + boost::sml::event<ev_address_claim>[&self::claiming_priority] / &self::send_claiming,
      boost::sml::state<st_claiming> + boost::sml::event<ev_address_claim>[&self::claiming_loss] / &self::claim_address,
      boost::sml::state<st_claiming> + boost::sml::event<ev_address_claim>[&self::claiming_failure] = boost::sml::state<st_no_address>,
      boost::sml::state<st_claiming> + boost::sml::event<ev_timeout>[&self::valid_address] / &self::set_claimed_address = boost::sml::state<st_has_address>,
      boost::sml::state<st_claiming> + boost::sml::event<ev_timeout>[&self::no_valid_address] = boost::sml::state<st_no_address>,

      boost::sml::state<st_has_address> + boost::sml::on_entry<boost::sml::_> / &self::notify_address_gain,
      boost::sml::state<st_has_address> + boost::sml::event<ev_address_request> / &self::send_claimed,
      boost::sml::state<st_has_address> + boost::sml::event<ev_address_claim>[&self::claimed_priority] / &self::send_claimed,
      boost::sml::state<st_has_address> + boost::sml::event<ev_address_claim>[&self::claimed_loss] / &self::set_claiming_address = boost::sml::state<st_claiming>,
      boost::sml::state<st_has_address> + boost::sml::event<ev_address_claim>[&self::claimed_failure] = boost::sml::state<st_no_address>,
      boost::sml::state<st_has_address> + boost::sml::on_exit<boost::sml::_> / &self::notify_address_loss

      ///TODO: Check if the name already has an address?
    );
  }

private:

  //Global state data

  //Name of the device whome we are claiming an address for
  const name name_{};

  //Callbacks for notifying users of state machine
  callbacks callbacks_;

};

} //namespace jay

#endif