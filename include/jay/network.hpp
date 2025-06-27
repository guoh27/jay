//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no), 2025 Hong.Guo (hong.guo@advantech.com.cn)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/guoh27/jay
//
#pragma once

// C++
#include <algorithm>//std::clamp
#include <mutex>//std::scoped_lock
#include <optional>//std::optional
#include <set>//std::set
#include <shared_mutex>//std::shared_mutex, std::shared_lock
#include <string>//std::string
#include <unordered_map>//std::unordered_map

// Local
#include "name.hpp"// name, jay globals, and jay::address_t

namespace jay {

/**
 * @mainpage
 * @brief Storage class for maintaining the relation between
 * controller name and its address.
 * @note Is thread safe, needs to be passed by reference or pointer
 * as moving or copying is not allowed.
 */
class network
{
public:
  using OnNewName = std::function<void(jay::name, jay::address_t)>;
  /**
   * @brief Construct a new network object
   *
   * @param interface_name that the network is associated with
   */
  explicit network(std::string interface_name) : interface_name_(interface_name) {}

  network(const std::string &interface_name, const OnNewName &on_new_name) noexcept
    : interface_name_(interface_name), on_new_name_(on_new_name)
  {}

  network(const std::string &interface_name, OnNewName &&on_new_name) noexcept
    : interface_name_(interface_name), on_new_name_(std::move(on_new_name))
  {}

  network(const network &) = delete;
  network &operator=(const network &) = delete;

  // Move
  network(network &&) = delete;
  network &operator=(network &&) = delete;

  /// ##################### Callback ##################### ///

  /**
   * @brief Set the on new name callback using a copy
   *
   * @param on_new_name callback for when a new name is added to the network
   * @note When the callback is triggered the network is locked. So the callback should be fast
   * and cant call any network functions. Its recommended to copy the name and
   * queue it for later processing in an io_context.
   */
  void on_new_name_callback(const OnNewName &on_new_name) noexcept
  {
    std::scoped_lock lock{ network_mtx_ };
    on_new_name_ = on_new_name;
  }

  /**
   * @brief Set the on new name callback using a move
   *
   * @param on_new_name callback for when a new name is added to the network
   * @note When the callback is triggered the network is locked. So the callback should be fast
   * and cant call any network functions. Its recommended to copy the name and
   * queue it for later processing in an io_context.
   */
  void on_new_name_callback(OnNewName &&on_new_name) noexcept
  {
    std::scoped_lock lock{ network_mtx_ };
    on_new_name_ = std::move(on_new_name);
  }

  /// ##################### Copy Internal ##################### ///

  /**
   * @brief Get a set containing all names
   * @return set of names
   */
  std::set<jay::name> get_name_set()
  {
    std::shared_lock lock{ network_mtx_ };
    std::set<jay::name> set{};
    for (auto &pair : name_addr_map_) { set.insert(pair.first); }
    return set;
  }

  /// ##################### Map access ##################### ///

  /**
   * Add a name to the network, if name already exists
   * the address will be changed. name is added even if address cant
   * be claimed.
   * @param name
   * @param address of the controller app, if J1939_NO_ADDR then
   * controller name registered, but if already exist then address is cleared
   * @return true if name was inserted
   * @return true if existing name address was changed
   * @return false if no name was inserted
   * @return false if name and address was the same
   */
  bool insert(jay::name name, jay::address_t address)
  {
    std::scoped_lock lk{ network_mtx_ };

    // 1. If the given address is not a valid unicast address,
    //    just keep the device in IDLE state.
    if (address > J1939_MAX_UNICAST_ADDR) {
      auto [it, _] = name_addr_map_.try_emplace(name, J1939_IDLE_ADDR);
      // Remove the old reverse mapping (if any).
      addr_name_map_.erase(it->second);
      it->second = J1939_IDLE_ADDR;
      return true;
    }

    // Helper lambda: move a device to IDLE state.
    auto set_idle = [&](jay::name n) {
      addr_name_map_.erase(name_addr_map_[n]);
      name_addr_map_[n] = J1939_IDLE_ADDR;
    };

    // Helper lambda: let a device claim @address and trigger the callback.
    auto claim = [&](jay::name n) {
      addr_name_map_.erase(name_addr_map_[n]);// remove old reverse entry
      name_addr_map_[n] = address;// forward map
      addr_name_map_[address] = n;// reverse map
      if (on_new_name_) on_new_name_(n, address);
    };

    // 2. Check whether the target address is already occupied.
    auto addr_it = addr_name_map_.find(address);
    bool occupied = addr_it != addr_name_map_.end();
    jay::name conflict = occupied ? addr_it->second : jay::name{};

    // 3. If the address is occupied and we have *lower* priority,
    //    we must stay idle and report failure.
    if (occupied && name > conflict) {// lower priority (larger value)
      name_addr_map_.try_emplace(name, J1939_IDLE_ADDR);
      return false;// notify caller: claim failed
    }

    // 4. We are allowed to claim this address.
    name_addr_map_.try_emplace(name, J1939_IDLE_ADDR);// ensure entry exists
    claim(name);

    // 5. If someone else was using the address, put them into IDLE state.
    if (occupied && conflict != name) set_idle(conflict);

    return true;
  }

  /**
   * @brief Release the address of the given name
   * @param name to release
   */
  void release(const jay::name name)
  {
    std::scoped_lock lock{ network_mtx_ };

    if (name_addr_map_.find(name) == name_addr_map_.end()) { return; }
    auto address = name_addr_map_[name];
    name_addr_map_[name] = J1939_IDLE_ADDR;

    if (addr_name_map_.find(address) == addr_name_map_.end()) { return; }
    addr_name_map_.erase(address);
  }

  /**
   * @brief Remove a name and address
   * @param name to remove
   */
  void remove(const jay::name name)
  {
    std::scoped_lock lock{ network_mtx_ };

    if (name_addr_map_.find(name) == name_addr_map_.end()) { return; }
    auto address = name_addr_map_[name];
    name_addr_map_.erase(name);

    if (addr_name_map_.find(address) == addr_name_map_.end()) { return; }
    addr_name_map_.erase(address);
  }

  /**
   * @brief Clear all names and addresses
   */
  void clear() noexcept
  {
    std::scoped_lock lock{ network_mtx_ };
    name_addr_map_.clear();
    addr_name_map_.clear();
  }

  /**
   * @brief Check if an address is taken
   * @param address to check
   * @return true if no controller has the address,
   * @return false if a controller has the address
   * @return false if address provided was a global address
   */
  bool available(jay::address_t address) const
  {
    if (address > J1939_MAX_UNICAST_ADDR) return false;
    std::shared_lock lock{ network_mtx_ };
    return addr_name_map_.find(address) == addr_name_map_.end();
  }

  /**
   * @brief Check if an address is claimable by a name
   * @param address to check
   * @param name that
   * @return true if address is open
   * @return true if name has higher priority than existing name
   * @return false if global address
   * @return false if name on address has higher priority
   * @todo should global address be an assert?
   */
  bool claimable(jay::address_t address, jay::name name) const
  {
    if (address > J1939_MAX_UNICAST_ADDR) return false;
    std::shared_lock lock{ network_mtx_ };
    if (auto it = addr_name_map_.find(address); it != addr_name_map_.end()) { return it->second > name; }
    return true;
  }

  /**
   * @brief Check if a Controller application is registered in the network
   * @param name to check for
   * @return true if in the network, false if not
   */
  bool in_network(const jay::name name) const
  {
    std::shared_lock lock{ network_mtx_ };
    return name_addr_map_.find(name) != name_addr_map_.end();
  }

  /**
   * @brief Check if ctrl - name pairing is matched in the network
   * @param name
   * @param address
   * @return true if address and controller are paired
   * @return false if address or ctrl are not paired
   */
  bool match(jay::name name, jay::address_t address)
  {
    /// TODO: Should it also check the other way, that address map has controller?
    std::shared_lock lock{ network_mtx_ };
    if (auto it = name_addr_map_.find(name); it != name_addr_map_.end()) { return it->second == address; }
    return false;
  }

  /**
   * @brief Get the amount of addresses used in the network
   * @return address use count
   */
  size_t address_count() const
  {
    std::shared_lock lock{ network_mtx_ };
    return addr_name_map_.size();
  }

  /**
   * @brief Get the amount of names registered in the network
   * @return name count
   */
  size_t name_count() const
  {
    std::shared_lock lock{ network_mtx_ };
    return name_addr_map_.size();
  }

  /**
   * @brief Get the name at address
   * @param address to get name at
   * @throw
   * @return name or nullopt if address is not used
   */
  std::optional<jay::name> get_name(jay::address_t address) const
  {
    /// NOTE: Dont need to check global addresses as they cant be inserted.
    std::shared_lock lock{ network_mtx_ };
    if (auto it = addr_name_map_.find(address); it != addr_name_map_.end()) { return it->second; }
    return std::nullopt;
  }

  /**
   * @brief Get the address associated with given name
   * @param name that we want the address for
   * @return address of the controller
   * @return J1939_NO_ADDR if not in the network
   */
  jay::address_t get_address(const jay::name name) const
  {
    std::shared_lock lock{ network_mtx_ };
    if (auto it = name_addr_map_.find(name); it != name_addr_map_.end()) { return it->second; }
    return J1939_NO_ADDR;
  }

  /**
   * @brief Check if any addresses are available in the network
   * @return true if address network has reached 255
   * @return false if addresses are still available
   */
  bool full() const
  {
    std::shared_lock lock{ network_mtx_ };
    /// Is 256 - 2 on end as we dont want to occupy J1939_IDLE_ADDR and J1939_NO_ADDR
    return addr_name_map_.size() > J1939_MAX_UNICAST_ADDR;
  }

  /**
   * Search the network empty addresses
   * @param name of the controller application looking for address
   * @param preferred_address to start search from, clamped to between 0 - 253
   * @return empty address, if no addresses were available J1939_NO_ADDR is returned
   */
  jay::address_t find_address(jay::name name, jay::address_t preferred_address = 0) const
  {
    preferred_address = std::clamp(preferred_address, static_cast<jay::address_t>(0), J1939_MAX_UNICAST_ADDR);
    std::shared_lock lock{ network_mtx_ };
    if (!name.self_config_address()) {
      auto pair = addr_name_map_.find(preferred_address);
      if (pair == addr_name_map_.end()) { return preferred_address; }
      if (name <= pair->second) { return preferred_address; }// we have smaller(higher priority) name
      return J1939_NO_ADDR;
    }

    auto address = search(name, preferred_address, J1939_IDLE_ADDR);
    if (address == J1939_NO_ADDR) { address = search(name, 0, preferred_address); }
    return address;
    // if no address was found above the preferred address, check bellow
  }

  /// TODO: Check the name of other devices and see if they can change their address

  /**
   * @brief Get the name of the interface that this network is associated with
   * @return const std::string&
   */
  const std::string &get_interface_name() const { return interface_name_; }

private:
  /**
   * Search the network empty addresses
   * @param name of the controller application looking for address
   * @param preferred_address to start search from
   * @param end_address limit of the address search
   * @return empty address, if no addresses were available J1939_NO_ADDR is returned
   */
  jay::address_t search(jay::name name, jay::address_t start_address, jay::address_t end_address) const
  {
    for (jay::address_t address = start_address; address < end_address; address++) {
      auto pair = addr_name_map_.find(address);
      if (pair == addr_name_map_.end()) { return address; }
      if (pair->second == name) { return address; }// reuse old address
    }
    return J1939_NO_ADDR;
  }

private:
  const std::string interface_name_{ "can0" };
  OnNewName on_new_name_;

  /// NOTE: Could implement maps as a bidirectional map,
  /// but would need somewhere for names with null address
  /// so overall it might not be worth it

  std::unordered_map<jay::name, jay::address_t, jay::name::hash> name_addr_map_{};
  std::unordered_map<jay::address_t, jay::name> addr_name_map_{};

  mutable std::shared_mutex network_mtx_{};
};

}// Namespace jay
