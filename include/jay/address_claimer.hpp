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
#include <functional>

// Lib
#include "boost/sml.hpp"

// Local
#include "frame.hpp"
#include "name.hpp"
#include "network.hpp"


namespace jay {

/**
 * @brief boost::sml state machine class for dynamic j1939 address claiming
 * @note Each state machine is responsible for only one name address pair
 * @note The state machine does not (cant?) manage delays / timeouts internally
 */
class address_claimer
{
public:
  using self = address_claimer;

  /**
   * @brief Callbacks for generating outputs
   * @todo remove name from callbacks as its probably not needed?
   */
  struct callbacks
  {
    std::function<void(jay::name, std::uint8_t)> on_address;
    std::function<void(jay::name)> on_lose_address;
    std::function<void()> on_begin_claiming;
    std::function<void(jay::name, std::uint8_t)> on_address_claim;
    std::function<void(jay::name)> on_cannot_claim;
  };

  /**
   * @brief Constructor
   * @param name to get an address for
   */
  explicit address_claimer(name name) : name_(name), callbacks_() {}

  /**
   * @brief Constructor
   * @param name to get an address for
   * @param callbacks
   */
  address_claimer(name name, callbacks &&callbacks) : name_(name), callbacks_(std::move(callbacks))
  {
    assert(callbacks_.on_address);
    assert(callbacks_.on_lose_address);
    assert(callbacks_.on_begin_claiming);
    assert(callbacks_.on_address_claim);
    assert(callbacks_.on_cannot_claim);
  }

  /**
   * @brief Set callbacks
   * @param callbacks
   */
  void set_callbacks(callbacks &&callbacks) { callbacks_ = std::move(callbacks); }

  /**
   * @brief get the name that we are claiming and address for
   * @return name
   */
  jay::name get_name() const noexcept { return name_; }

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                             States                             @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * @brief Starting state of the state machine, In this state the name has no address
   */
  struct st_no_address
  {
  };

  /**
   * @brief In this state the name is in the process of claiming and address
   */
  struct st_claiming
  {
    std::uint8_t address{ J1939_NO_ADDR };
  };

  /**
   * @brief In this state the name has an address
   */
  struct st_has_address
  {
    std::uint8_t address{ J1939_NO_ADDR };
  };

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                                 Events                         @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * @brief Event used to start the address claiming process
   */
  struct ev_start_claim
  {
    std::uint8_t pref_address{};
  };

  /**
   * @brief Event used when receiving an address claim from another controller/device
   */
  struct ev_address_claim
  {
    std::uint64_t name{};
    std::uint8_t address{};
  };

  /**
   * @brief Event used when another controller/device is requesting our address
   */
  struct ev_address_request
  {
  };

  /**
   * @brief Event used when time has run out for address claiming
   */
  struct ev_timeout
  {
  };

private:
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                             Guards                             @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  ///

  /**
   * @brief Checks if no addresses are available to claim
   * @param network to check for available addresses
   */
  bool no_address_available(const network &network) const { return network.full(); }

  /**
   * @brief Checks if addresses are available to claim
   * @param network to check for available addresses
   */
  bool address_available(const network &network) const { return !network.full(); }

  /**
   * @brief Check if our name has priority
   * @param name to check local name against
   * @return false if local name is larger
   * @return true if local name is less
   */
  bool address_priority(jay::name name) const { return name_ < name; }

  /**
   * @brief Checks if addresses conflict
   * @param l_address
   * @param r_address
   * @return false if addresses are not the same
   * @return true if addresses are the same
   */
  bool address_conflict(std::uint8_t l_address, std::uint8_t r_address) const { return l_address == r_address; }

  /**
   * @brief Checks if addresses change is required
   * @param name to check local name again
   * @param l_address address associated with name
   * @param r_address local address
   * @return false if addresses does not conflict or if local name has priority
   * @return true if addresses conflict and local name does not have priority
   */
  bool address_change_required(jay::name name, std::uint8_t l_address, std::uint8_t r_address) const
  {
    return address_conflict(l_address, r_address) && !address_priority(name);
  }

  /// State specific guards

  /**
   * @brief Check if claiming state has priority over address claim
   * @param claiming state
   * @param address_claim event
   * @return false if addresses dont conflict or local name does not have priority
   * @return true if addresses conflict and local name has priority
   */
  bool claiming_priority(st_claiming &claiming, const ev_address_claim &address_claim) const
  {
    return address_conflict(claiming.address, address_claim.address) && address_priority(address_claim.name);
  }

  /**
   * @brief Check if claiming state has priority over address claim event and that
   * new addresses are available
   * @param claiming state
   * @param address_claim event
   * @param network of name address pairs
   * @return false if address change not required or no addresses are available
   * @return true if address change is required and addresses are available
   */
  bool claiming_loss(st_claiming &claiming, const ev_address_claim &address_claim, const network &network) const
  {
    return address_change_required(address_claim.name, address_claim.address, claiming.address)
           && address_available(network);
  }

  /**
   * @brief Check if claiming state loses priority over address claim event and
   * if there are no available addresses
   * @param claiming state
   * @param address_claim event
   * @param network of name address pairs
   * @return false if address change not required or addresses are available
   * @return true if address change is required and no addresses are available
   */
  bool claiming_failure(st_claiming &claiming, const ev_address_claim &address_claim, const network &network)
  {
    return address_change_required(address_claim.name, address_claim.address, claiming.address)
           && no_address_available(network);
  }

  /**
   * @brief Check if claimed state has priority over address claim event
   * @param has_address state
   * @param address_claim event
   * @return false if addresses dont conflict or local name does not have priority
   * @return true if addresses conflict and local name has priority
   */
  bool claimed_priority(st_has_address &has_address, const ev_address_claim &address_claim) const
  {
    return address_conflict(has_address.address, address_claim.address) && address_priority(address_claim.name);
  }

  /**
   * @brief Check if claimed state has priority over address claim event and that
   * new addresses are available
   * @param has_address state
   * @param address_claim event
   * @param network
   * @return false if address change not required or no addresses are available
   * @return true if address change is required and addresses are available
   */
  bool claimed_loss(st_has_address &has_address, const ev_address_claim &address_claim, const network &network) const
  {
    return address_change_required(address_claim.name, address_claim.address, has_address.address)
           && address_available(network);
  }

  /**
   * @brief Check if claimed state loses priority over address claim event and
   * if there are no available addresses
   * @param has_address state
   * @param address_claim event
   * @param network
   * @return false if address change not required or addresses are available
   * @return true if address change is required and no addresses are available
   */
  bool claimed_failure(st_has_address &has_address, const ev_address_claim &address_claim, const network &network) const
  {
    return address_change_required(address_claim.name, address_claim.address, has_address.address)
           && no_address_available(network);
  }

  /**
   * @brief Check if the claiming address is available in network
   * @param claiming state
   * @param network of name address pairs
   * @return false if claiming address is not available in network and name
   * does not have an address in the network
   * @return true if claiming address is available in network or name
   * already has an address in the network
   * @todo add boolean for taking address or not? replace claimable with available?
   */
  bool valid_address(st_claiming &claiming, const network &network) const
  {
    return network.claimable(claiming.address, name_) || network.get_address(name_) < J1939_IDLE_ADDR;
  }

  /**
   * @brief Check if that claiming address is not available in network
   * @param claiming state
   * @param network of name address pairs
   * @return false if claiming address is available in network or name
   * already has an address in the network
   * @return true if claiming address is not available in network and and name
   * does not have an address in the network
   * @todo add boolean for taking address or not? replace claimable with available?
   */
  bool no_valid_address(st_claiming &claiming, const network &network) const
  {
    return !network.claimable(claiming.address, name_) && network.get_address(name_) == J1939_IDLE_ADDR;
  }

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                             Actions                            @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /// Data transfree actions

  /**
   * @brief Set preferred address when entering claiming state
   * @param claiming state
   * @param ev_st_claim event, contains preferred address
   */
  void set_pref_address(st_claiming &claiming, const ev_start_claim &ev_st_claim)
  {
    claiming.address = ev_st_claim.pref_address;
  }

  /**
   * @brief Set claimed address from claiming state to has_address state
   * @param src - claiming state
   * @param dst - has_address state
   */
  void set_claimed_address(st_claiming &src, st_has_address &dst) { dst.address = src.address; }

  /**
   * @brief Set claimed address from claiming state to has_address state
   * @param src - claiming state
   * @param dst - has_address state
   */
  void set_claiming_address(st_has_address &src, st_claiming &dst) { dst.address = src.address; }

  /// Output actions

  /**
   * @brief Sends an address claim to on frame callback
   * @param address to claim
   */
  void send_address_claim(std::uint8_t address) const
  {
    if (callbacks_.on_address_claim) { callbacks_.on_address_claim(name_, address); }
  }

  /**
   * @brief Find an address and send address claim
   * @param claiming state
   * @param network of name address pairs
   */
  void begin_claiming_address(st_claiming &claiming, const network &network) const
  {
    if (callbacks_.on_begin_claiming) { callbacks_.on_begin_claiming(); }
    claim_address(claiming, network);
  }

  /**
   * @brief Find an address and send address claim
   * @param claiming state
   * @param network of name address pairs
   */
  void claim_address(st_claiming &claiming, const network &network) const
  {
    claiming.address = network.find_address(name_, claiming.address, false);
    send_address_claim(claiming.address);
  }

  /**
   * @brief Send address claim with address from claiming state
   * @param claiming state
   */
  void send_claiming(st_claiming &claiming) const { send_address_claim(claiming.address); }

  /**
   * @brief Send address claim with address from has_address state
   * @param has_address state
   */
  void send_claimed(st_has_address &has_address) const { send_address_claim(has_address.address); }

  /**
   * @brief send cannot claim address message
   * @note Requires a random 0 - 153 ms delay to prevent bus errors
   */
  void send_cannot_claim() const
  {
    if (callbacks_.on_cannot_claim) { callbacks_.on_cannot_claim(name_); }
  }

  /**
   * @brief Notify though callback which address has been claimed
   * @param has_address state
   */
  void notify_address_gain(st_has_address &has_address)
  {
    if (callbacks_.on_address) { callbacks_.on_address(name_, has_address.address); }
  };

  /**
   * @brief Notify though callback that address has been lost
   */
  void notify_address_loss()
  {
    if (callbacks_.on_lose_address) { callbacks_.on_lose_address(name_); }
  };

  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                       Transition table                         @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

public:
  /**
   * Creates transition table
   */
  auto operator()() const
  {
    return boost::sml::make_transition_table(

      /// Start without address, start_claim sets preferred address

      /// TODO: Check if the name already has an address at start up??

      *boost::sml::state<st_no_address>
        + boost::sml::on_entry<boost::sml::_>[&self::no_address_available] / &self::send_cannot_claim,
      boost::sml::state<st_no_address> + boost::sml::event<ev_address_request> / &self::send_cannot_claim,
      boost::sml::state<st_no_address>
        + boost::sml::event<ev_start_claim>[&self::address_available] / &self::set_pref_address =
        boost::sml::state<st_claiming>,
      boost::sml::state<st_no_address>
        + boost::sml::event<ev_start_claim>[&self::no_address_available] / &self::send_cannot_claim,

      boost::sml::state<st_claiming> + boost::sml::on_entry<boost::sml::_> / &self::begin_claiming_address,
      boost::sml::state<st_claiming> + boost::sml::event<ev_address_request> / &self::send_claiming,
      boost::sml::state<st_claiming>
        + boost::sml::event<ev_address_claim>[&self::claiming_priority] / &self::send_claiming,
      boost::sml::state<st_claiming> + boost::sml::event<ev_address_claim>[&self::claiming_loss] / &self::claim_address,
      boost::sml::state<st_claiming> + boost::sml::event<ev_address_claim>[&self::claiming_failure] =
        boost::sml::state<st_no_address>,
      boost::sml::state<st_claiming>
        + boost::sml::event<ev_timeout>[&self::valid_address] / &self::set_claimed_address =
        boost::sml::state<st_has_address>,
      boost::sml::state<st_claiming> + boost::sml::event<ev_timeout>[&self::no_valid_address] =
        boost::sml::state<st_no_address>,

      boost::sml::state<st_has_address> + boost::sml::on_entry<boost::sml::_> / &self::notify_address_gain,
      boost::sml::state<st_has_address> + boost::sml::event<ev_address_request> / &self::send_claimed,
      boost::sml::state<st_has_address>
        + boost::sml::event<ev_address_claim>[&self::claimed_priority] / &self::send_claimed,
      boost::sml::state<st_has_address>
        + boost::sml::event<ev_address_claim>[&self::claimed_loss] / &self::set_claiming_address =
        boost::sml::state<st_claiming>,
      boost::sml::state<st_has_address> + boost::sml::event<ev_address_claim>[&self::claimed_failure] =
        boost::sml::state<st_no_address>,
      boost::sml::state<st_has_address> + boost::sml::on_exit<boost::sml::_> / &self::notify_address_loss

    );
  }

private:
  // Global state data

  // Name of the device whome we are claiming an address for
  const name name_{};

  // Callbacks for notifying users of state machine
  callbacks callbacks_;
};

}// namespace jay
