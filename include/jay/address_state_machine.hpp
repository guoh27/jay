//
// Copyright (c) 2024 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef JAY_ADDRESS_STATE_MACHINE_H
#define JAY_ADDRESS_STATE_MACHINE_H

#pragma once

// C++
#include <functional>// std::function

// Lib
#include "boost/sml.hpp"// boost::sml

// Local
#include "frame.hpp"// jay::frame
#include "name.hpp"// jay::name
#include "network.hpp"// jay::network


namespace jay {

/**
 * @brief boost::sml state machine class for dynamic j1939 address claiming
 * @note Each state machine is responsible for only one name address pair
 * @note The state machine does not (cant?) manage delays / timeouts internaly
 * for that @see address_claimer
 * @todo in theory shoul add a error or message collision event
 * @todo guard for address claims with the same name in case of echo or loopback?
 */
class address_state_machine
{
public:
  using self = address_state_machine;

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

  address_state_machine(const jay::name name, const std::uint8_t preffered_address) noexcept
    : name_(name), preffered_address_(preffered_address)
  {}

  /**
   * @brief Constructor
   * @param name to get an address for
   * @param callbacks
   */
  address_state_machine(const jay::name name, const std::uint8_t preffered_address, const callbacks &callbacks) noexcept
    : name_(name), preffered_address_(preffered_address), callbacks_(callbacks)
  {
    assert(callbacks_.on_address);
    assert(callbacks_.on_lose_address);
    assert(callbacks_.on_begin_claiming);
    assert(callbacks_.on_address_claim);
    assert(callbacks_.on_cannot_claim);
  }

  /**
   * @brief Constructor
   * @param name to get an address for
   * @param callbacks
   */
  address_state_machine(const jay::name name, const std::uint8_t preffered_address, callbacks &&callbacks) noexcept
    : name_(name), preffered_address_(preffered_address), callbacks_(std::move(callbacks))
  {
    assert(callbacks_.on_address);
    assert(callbacks_.on_lose_address);
    assert(callbacks_.on_begin_claiming);
    assert(callbacks_.on_address_claim);
    assert(callbacks_.on_cannot_claim);
  }

  /**
   * @brief Set the callbacks
   * @param callbacks
   */
  inline void bind_callbacks(const callbacks &callbacks) noexcept { callbacks_ = callbacks; }

  /**
   * @brief Set callbacks
   * @param callbacks
   */
  inline void bind_callbacks(callbacks &&callbacks) noexcept { callbacks_ = std::move(callbacks); }

  /**
   * @brief get the name that we are claiming and address for
   * @return name
   */
  [[nodiscard]] inline jay::name get_name() const noexcept { return name_; }

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
   * @brief Event used when recieving an address claim from another controller/device
   */
  struct ev_address_claim
  {
    std::uint64_t name{};// Const?
    std::uint8_t address{};
  };

  /**
   * @brief Event used when another controller/device is requesting our address
   */
  struct ev_address_request
  {
    std::uint8_t destination_address{};
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

  /// TODO: Should the gaurds be public?

  [[nodiscard]] inline bool is_global_address_req(const ev_address_request &address_request) const
  {
    return address_request.destination_address == J1939_NO_ADDR;
  }

  /**
   * @brief Checks if addresses change is requred
   * @param other name to check local name agains
   * @param l_address address assiosiated with name
   * @param r_address local address
   * @return false if addresses does not conflict or if local name has priority
   * @return true if addresses conflict and local name does not have priority
   */
  [[nodiscard]] inline bool address_change_required(const jay::name other,
    const std::uint8_t l_address,
    const std::uint8_t r_address) const noexcept
  {
    return l_address == r_address && !name_.has_priority_over(other);
  }

  ///// State specific guards

  /// Claiming specific guards

  /**
   * @brief Check if claiming state has proirity over address claim
   * @param claiming state
   * @param address_claim event
   * @return false if addresses dont conflict or local name does not have priority
   * @return true if addresses conflict and local name has priority
   */
  [[nodiscard]] inline bool claiming_priority(st_claiming &claiming,
    const ev_address_claim &address_claim) const noexcept
  {
    return !address_change_required(address_claim.name, address_claim.address, claiming.address);
    // return claiming.address == address_claim.address && name_.has_priority_over(address_claim.name);
  }

  /**
   * @brief Check if claiming state has proirity over address claim event and that
   * new addresses are avaialble
   * @param claiming state
   * @param address_claim event
   * @param network of name address pairs
   * @return false if address change not required or no addresses are available
   * @return true if address change is required and addresses are available
   */
  [[nodiscard]] inline bool
    claiming_loss(st_claiming &claiming, const ev_address_claim &address_claim, const network &network) const noexcept
  {
    return address_change_required(address_claim.name, address_claim.address, claiming.address) && !network.is_full();
  }

  /**
   * @brief Check if claiming state loses proirity over address claim event and
   * if there are no available addresses
   * @param claiming state
   * @param address_claim event
   * @param network of name address pairs
   * @return false if address change not required or addresses are available
   * @return true if address change is required and no addresses are available
   */
  [[nodiscard]] inline bool
    claiming_failure(st_claiming &claiming, const ev_address_claim &address_claim, const network &network) noexcept
  {
    return address_change_required(address_claim.name, address_claim.address, claiming.address) && network.is_full();
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
  [[nodiscard]] inline bool valid_address(st_claiming &claiming, const network &network) const
  {
    return network.claimable(claiming.address, name_) || network.find_address(name_) < J1939_IDLE_ADDR;
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
  [[nodiscard]] inline bool no_valid_address(st_claiming &claiming, const network &network) const
  {
    return !network.claimable(claiming.address, name_) && network.find_address(name_) == J1939_IDLE_ADDR;
  }

  /// Has address specific guards

  /**
   * @brief Check if claimed state has proirity over address claim event
   * @param has_address state
   * @param address_claim event
   * @return false if addresses dont conflict or local name does not have priority
   * @return true if addresses conflict and local name has priority
   */
  [[nodiscard]] inline bool claimed_priority(st_has_address &has_address, const ev_address_claim &address_claim) const
  {
    return !address_change_required(address_claim.name, address_claim.address, has_address.address);
    // return has_address.address == address_claim.address && name_.has_priority_over(address_claim.name);
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
  [[nodiscard]] inline bool
    claimed_loss(st_has_address &has_address, const ev_address_claim &address_claim, const network &network) const
  {
    return address_change_required(address_claim.name, address_claim.address, has_address.address)
           && !network.is_full();
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
  [[nodiscard]] inline bool
    claimed_failure(st_has_address &has_address, const ev_address_claim &address_claim, const network &network) const
  {
    return address_change_required(address_claim.name, address_claim.address, has_address.address) && network.is_full();
  }

  /**
   * @brief Check if address request is valid
   *
   * @param has_address state
   * @param address_request event
   * @return true if the address request is for the address in has_address state or is a global request
   * @return false if the address request is not for the address in has_address state and is not a global request
   */
  [[nodiscard]] inline bool valid_address_request(st_has_address &has_address,
    const ev_address_request &address_request) const
  {
    return address_request.destination_address == has_address.address || is_global_address_req(address_request);
  }

  /// No address specific guards


  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                             Actions                            @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /// Data transfere actions

  /**
   * @brief Set claimed address from claiming state to has_address state
   * @param src - claiming state
   * @param dst - has_address state
   */
  inline void claimed_address(st_claiming &src, st_has_address &dst) const noexcept { dst.address = src.address; }

  /**
   * @brief Set claimed address from claiming state to has_address state
   * @param src - claiming state
   * @param dst - has_address state
   */
  inline void claiming_address(st_has_address &src, st_claiming &dst) const noexcept { dst.address = src.address; }

  /// Output actions

  /**
   * @brief Sends an address claim to on frame callback
   * @param address to claim
   */
  inline void send_address_claim(std::uint8_t address) const noexcept
  {
    if (callbacks_.on_address_claim) { callbacks_.on_address_claim(name_, address); }
  }

  /**
   * @brief Find an address and send address claim
   * @param claiming state
   * @param network of name address pairs
   */
  inline void begin_claiming_address(st_claiming &claiming, const network &network) const noexcept
  {
    if (callbacks_.on_begin_claiming) { callbacks_.on_begin_claiming(); }
    claim_address(claiming, network);
  }

  /**
   * @brief Find an address and send address claim
   * @param claiming state
   * @param network of name address pairs
   */
  inline void claim_address(st_claiming &claiming, const network &network) const noexcept
  {
    /// NOTE: In theory find_available_address could return J1939_IDLE_ADDR, but that should not happen
    ///  as the guards should prevent it. But should we add an assert here just in case?
    claiming.address = network.find_available_address(name_, preffered_address_, false);
    send_address_claim(claiming.address);
  }

  /**
   * @brief Send address claim with address from claiming state
   * @param claiming state
   */
  inline void send_claiming(st_claiming &claiming) const noexcept { send_address_claim(claiming.address); }

  /**
   * @brief Send address claim with address from has_address state
   * @param has_address state
   */
  inline void send_claimed(st_has_address &has_address) const noexcept { send_address_claim(has_address.address); }

  /**
   * @brief send cannot claim address message
   * @note Requires a random 0 - 153 ms delay to prevent bus errors
   */
  inline void send_cannot_claim() const noexcept
  {
    if (callbacks_.on_cannot_claim) { callbacks_.on_cannot_claim(name_); }
  }

  /**
   * @brief Notify though callback which address has been claimed
   * @param has_address state
   */
  inline void notify_address_gain(st_has_address &has_address) const noexcept
  {
    if (callbacks_.on_address) { callbacks_.on_address(name_, has_address.address); }
  };

  /**
   * @brief Notify though callback that address has been lost
   */
  inline void notify_address_loss() const noexcept
  {
    if (callbacks_.on_lose_address) { callbacks_.on_lose_address(name_); }
  };

public:
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//
  //@                       Transition table                         @//
  //@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@//

  /**
   * Creates trasition table
   * @note Did in an earlier version start in the st_no_address state and had a start event
   * that allowed us to manually check if there were any addresses available. But that was
   * removed as its standard to start in the claiming state and then claim an address.
   */
  auto operator()() const
  {
    namespace sml = boost::sml;
    return sml::make_transition_table(

      // clang-format off
      *sml::state<st_claiming> + sml::on_entry<sml::_> / &self::begin_claiming_address,
      sml::state<st_claiming> + sml::event<ev_address_claim>[&self::claiming_priority] / &self::send_claiming,
      sml::state<st_claiming> + sml::event<ev_address_claim>[&self::claiming_loss] / &self::claim_address,
      sml::state<st_claiming> + sml::event<ev_address_claim>[&self::claiming_failure] = sml::state<st_no_address>,
      sml::state<st_claiming> + sml::event<ev_timeout>[&self::valid_address] / &self::claimed_address = sml::state<st_has_address>,
      sml::state<st_claiming> + sml::event<ev_timeout>[&self::no_valid_address] = sml::state<st_no_address>,

      sml::state<st_has_address> + sml::on_entry<sml::_> / &self::notify_address_gain,
      sml::state<st_has_address> + sml::event<ev_address_request>[&self::valid_address_request] / &self::send_claimed,
      sml::state<st_has_address> + sml::event<ev_address_claim>[&self::claimed_priority] / &self::send_claimed,
      sml::state<st_has_address> + sml::event<ev_address_claim>[&self::claimed_loss] / &self::claiming_address = sml::state<st_claiming>,
      sml::state<st_has_address> + sml::event<ev_address_claim>[&self::claimed_failure] = sml::state<st_no_address>,
      sml::state<st_has_address> + sml::on_exit<sml::_> / &self::notify_address_loss,

      sml::state<st_no_address> + sml::on_entry<sml::_> / &self::send_cannot_claim,
      sml::state<st_no_address> + sml::event<ev_address_request>[&self::is_global_address_req] / &self::send_cannot_claim
      // clang-format on
    );
  }

private:
  // Global state data

  // Name of the device whome we are claiming an address for
  const jay::name name_{};
  const std::uint8_t preffered_address_{ J1939_NO_ADDR };

  // Callbacks for notifying users of state machine
  callbacks callbacks_;
};

}// namespace jay

#endif