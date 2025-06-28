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
    std::function<void()> on_request;
    std::function<void(jay::name)> on_cannot_claim;
  };

  /**
   * @brief Constructor
   * @param name to get an address for
   */
  explicit address_state_machine(name name) : name_(name), callbacks_() {}

  /**
   * @brief Constructor
   * @param name to get an address for
   * @param callbacks
   */
  address_state_machine(name name, callbacks &&callbacks) : name_(name), callbacks_(std::move(callbacks))
  {
    assert(callbacks_.on_address);
    assert(callbacks_.on_lose_address);
    assert(callbacks_.on_begin_claiming);
    assert(callbacks_.on_address_claim);
    assert(callbacks_.on_request);
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

  struct st_address_lost
  {
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
    std::uint8_t destination_address{ jay::J1939_NO_ADDR };
  };

  /**
   * @brief Event used when time has run out for address claiming
   */
  struct ev_timeout
  {
  };

  struct ev_random_retry
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

  struct guard_no_address_available
  {
    static constexpr const char *tag = "no_address_available";
    bool operator()(self &self, const network &network) const { return self.no_address_available(network); }
  };

  /**
   * @brief Checks if addresses are available to claim
   * @param network to check for available addresses
   */
  bool address_available(const network &network) const { return !network.full(); }

  struct guard_address_available
  {
    static constexpr const char *tag = "address_available";
    bool operator()(self &self, const network &network) const { return self.address_available(network); }
  };

  /**
   * @brief Check if our name has priority
   * @param name to check local name against
   * @return false if local name is larger
   * @return true if local name is less
   */
  bool address_priority(jay::name name) const { return name_ < name; }

  struct guard_address_priority
  {
    static constexpr const char *tag = "address_priority";
    bool operator()(self &self, jay::name name) const { return self.address_priority(name); }
  };

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

  /**
   * @brief Check name is allowed retry claim
   *
   * @param net
   * @return true
   * @return false
   */
  bool retry_allowed(const network &net) const { return name_.self_config_address() && address_available(net); }

  struct guard_retry_allowed
  {
    static constexpr const char *tag = "retry_allowed";
    bool operator()(self &self, const network &net) const { return self.retry_allowed(net); }
  };

  /**
   * @brief Check name is disallowed retry claim
   *
   * @param net
   * @return true
   * @return false
   */
  bool retry_disallowed(const network &net) const { return !retry_allowed(net); }

  struct guard_retry_disallowed
  {
    static constexpr const char *tag = "retry_disallowed";
    bool operator()(self &self, const network &net) const { return self.retry_disallowed(net); }
  };

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

  struct guard_claiming_priority
  {
    static constexpr const char *tag = "claiming_priority";
    bool operator()(self &self, st_claiming &claiming, const ev_address_claim &address_claim) const
    {
      return self.claiming_priority(claiming, address_claim);
    }
  };

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

  struct guard_claiming_loss
  {
    static constexpr const char *tag = "claiming_loss";
    bool
      operator()(self &self, st_claiming &claiming, const ev_address_claim &address_claim, const network &network) const
    {
      return self.claiming_loss(claiming, address_claim, network);
    }
  };

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

  struct guard_claiming_failure
  {
    static constexpr const char *tag = "claiming_failure";
    bool
      operator()(self &self, st_claiming &claiming, const ev_address_claim &address_claim, const network &network) const
    {
      return self.claiming_failure(claiming, address_claim, network);
    }
  };

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

  struct guard_claimed_priority
  {
    static constexpr const char *tag = "claimed_priority";
    bool operator()(self &self, st_has_address &has_address, const ev_address_claim &address_claim) const
    {
      return self.claimed_priority(has_address, address_claim);
    }
  };

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

  struct guard_claimed_loss
  {
    static constexpr const char *tag = "claimed_loss";
    bool operator()(self &self,
      st_has_address &has_address,
      const ev_address_claim &address_claim,
      const network &network) const
    {
      return self.claimed_loss(has_address, address_claim, network);
    }
  };

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

  struct guard_claimed_failure
  {
    static constexpr const char *tag = "claimed_failure";
    bool operator()(self &self,
      st_has_address &has_address,
      const ev_address_claim &address_claim,
      const network &network) const
    {
      return self.claimed_failure(has_address, address_claim, network);
    }
  };

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

  struct guard_valid_address
  {
    static constexpr const char *tag = "valid_address";
    bool operator()(self &self, st_claiming &claiming, const network &network) const
    {
      return self.valid_address(claiming, network);
    }
  };

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

  struct guard_no_valid_address
  {
    static constexpr const char *tag = "no_valid_address";
    bool operator()(self &self, st_claiming &claiming, const network &network) const
    {
      return self.no_valid_address(claiming, network);
    }
  };

  bool valid_claiming_request(st_claiming &claiming, const ev_address_request &address_request) const
  {
    return address_request.destination_address == claiming.address || is_global_address_req(address_request);
  }

  struct guard_valid_claiming_request
  {
    static constexpr const char *tag = "valid_claiming_request";
    bool operator()(self &self, st_claiming &claiming, const ev_address_request &address_request) const
    {
      return self.valid_claiming_request(claiming, address_request);
    }
  };

  bool is_global_address_req(const ev_address_request &address_request) const
  {
    return address_request.destination_address == J1939_NO_ADDR;
  }

  struct guard_is_global_address_req
  {
    static constexpr const char *tag = "is_global_address_req";
    bool operator()(self &self, const ev_address_request &address_request) const
    {
      return self.is_global_address_req(address_request);
    }
  };

  /**
   * @brief Check if address request is valid
   *
   * @param has_address state
   * @param address_request event
   * @return true if the address request is for the address in has_address state or is a global request
   * @return false if the address request is not for the address in has_address state and is not a global request
   */
  bool valid_address_request(st_has_address &has_address, const ev_address_request &address_request) const
  {
    return address_request.destination_address == has_address.address || is_global_address_req(address_request);
  }

  struct guard_valid_address_request
  {
    static constexpr const char *tag = "valid_address_request";
    bool operator()(self &self, st_has_address &has_address, const ev_address_request &address_request) const
    {
      return self.valid_address_request(has_address, address_request);
    }
  };

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

  struct act_set_pref_address
  {
    static constexpr const char *tag = "act_set_pref_address";
    void operator()(self &self, st_claiming &claiming, const ev_start_claim &ev_st_claim) const
    {
      self.set_pref_address(claiming, ev_st_claim);
    }
  };

  /**
   * @brief Set claimed address from claiming state to has_address state
   * @param src - claiming state
   * @param dst - has_address state
   */
  void set_claimed_address(st_claiming &src, st_has_address &dst) { dst.address = src.address; }

  struct act_set_claimed_address
  {
    static constexpr const char *tag = "set_claimed_address";
    void operator()(self &self, st_claiming &src, st_has_address &dst) const { self.set_claimed_address(src, dst); }
  };

  /**
   * @brief Set claimed address from claiming state to has_address state
   * @param src - claiming state
   * @param dst - has_address state
   */
  void set_claiming_address(st_has_address &src, st_claiming &dst) { dst.address = src.address; }

  struct act_set_claiming_address
  {
    static constexpr const char *tag = "set_claiming_address";
    void operator()(self &self, st_has_address &src, st_claiming &dst) const { self.set_claiming_address(src, dst); }
  };

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
   * @brief Sends an address request to on frame callback
   *
   */
  void send_request() const
  {
    if (callbacks_.on_request) { callbacks_.on_request(); }
  }

  struct act_send_request
  {
    static constexpr const char *tag = "send_request";
    void operator()(self &self) const { self.send_request(); }
  };

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

  struct act_begin_claiming_address
  {
    static constexpr const char *tag = "begin_claiming_address";
    void operator()(self &self, st_claiming &claiming, const network &network) const
    {
      self.begin_claiming_address(claiming, network);
    }
  };

  /**
   * @brief Find an address and send address claim
   * @param claiming state
   * @param network of name address pairs
   */
  void claim_address(st_claiming &claiming, const network &network) const
  {
    claiming.address = network.find_address(name_, claiming.address);
    send_address_claim(claiming.address);
  }

  /**
   * @brief Send address claim with address from claiming state
   * @param claiming state
   */
  void send_claiming(st_claiming &claiming) const { send_address_claim(claiming.address); }

  struct act_send_claiming
  {
    static constexpr const char *tag = "send_claiming";
    void operator()(self &self, st_claiming &claiming) const { self.send_claiming(claiming); }
  };

  /**
   * @brief Send address claim with address from has_address state
   * @param has_address state
   */
  void send_claimed(st_has_address &has_address) const { send_address_claim(has_address.address); }

  struct act_send_claimed
  {
    static constexpr const char *tag = "send_claimed";
    void operator()(self &self, st_has_address &has_address) const { self.send_claimed(has_address); }
  };

  /**
   * @brief send cannot claim address message
   * @note Requires a random 0 - 153 ms delay to prevent bus errors
   */
  void send_cannot_claim() const
  {
    if (callbacks_.on_cannot_claim) { callbacks_.on_cannot_claim(name_); }
  }

  struct act_send_cannot_claim
  {
    static constexpr const char *tag = "send_cannot_claim";
    void operator()(self &self) const { self.send_cannot_claim(); }
  };

  /**
   * @brief Notify though callback which address has been claimed
   * @param has_address state
   */
  void notify_address_gain(st_has_address &has_address)
  {
    if (callbacks_.on_address) { callbacks_.on_address(name_, has_address.address); }
  };

  struct act_notify_address_gain
  {
    static constexpr const char *tag = "notify_address_gain";
    void operator()(self &self, st_has_address &has_address) const { self.notify_address_gain(has_address); }
  };

  /**
   * @brief Notify though callback that address has been lost
   */
  void notify_address_loss()
  {
    if (callbacks_.on_lose_address) { callbacks_.on_lose_address(name_); }
  };

  struct act_notify_address_loss
  {
    static constexpr const char *tag = "notify_address_loss";
    void operator()(self &self) const { self.notify_address_loss(); }
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
    using namespace boost::sml;
    // clang-format off
    return make_transition_table(
      *
      // No Address
      state<st_no_address> + on_entry<_>[guard_address_available{}] / act_send_request{},
      state<st_no_address> + event<ev_address_request>[guard_is_global_address_req{}] / act_send_cannot_claim{},
      state<st_no_address> + event<ev_start_claim>[guard_address_available{}] / act_set_pref_address{} = state<st_claiming>,
      state<st_no_address> + event<ev_start_claim>[guard_no_address_available{}] / act_send_cannot_claim{},

      // Claiming
      state<st_claiming> + on_entry<_> / act_begin_claiming_address{},
      state<st_claiming> + event<ev_address_claim>[guard_claiming_priority{}] / act_send_claiming{},
      state<st_claiming> + event<ev_address_claim>[guard_claiming_loss{}] / act_begin_claiming_address{},
      state<st_claiming> + event<ev_address_claim>[guard_claiming_failure{}] = state<st_address_lost>,
      state<st_claiming> + event<ev_timeout>[guard_valid_address()] / act_set_claimed_address{} = state<st_has_address>,
      state<st_claiming> + event<ev_timeout>[guard_no_valid_address{}] = state<st_no_address>,
      state<st_claiming> + event<ev_address_request>[guard_valid_claiming_request{}] / act_send_claiming{},

      // Has Address
      state<st_has_address> + on_entry<_> / act_notify_address_gain{},
      state<st_has_address> + event<ev_address_request>[guard_valid_address_request{}] / act_send_claimed{},
      state<st_has_address> + event<ev_address_claim>[guard_claimed_priority{}] / act_send_claimed{},
      state<st_has_address> + event<ev_address_claim>[guard_claimed_loss{}] / act_set_claiming_address{} = state<st_claiming>,
      state<st_has_address> + event<ev_address_claim>[guard_claimed_failure{}] = state<st_address_lost>,
      state<st_has_address> + boost::sml::on_exit<_> / act_notify_address_loss{},

      // Address Lost
      state<st_address_lost> + on_entry<_> / act_send_cannot_claim{},
      state<st_address_lost> + event<ev_address_request>[guard_is_global_address_req{}] / act_send_cannot_claim{},
      state<st_address_lost> + event<ev_random_retry>[guard_retry_allowed{}] / act_set_pref_address{} = state<st_claiming>,
      state<st_address_lost> + event<ev_random_retry>[guard_retry_disallowed{}] / act_send_cannot_claim{} = state<st_no_address>
    );
    // clang-format on
  }

private:
  // Global state data

  // Name of the device whom we are claiming an address for
  const name name_{};

  // Callbacks for notifying users of state machine
  callbacks callbacks_;
};

}// namespace jay
