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
#include "name.hpp"// name, jay globals, and std::uint8_t

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
  /**
   * @brief Construct a new network object
   *
   * @param interface_name that the network is associated with
   */
  explicit network(std::string interface_name) : interface_name_(interface_name) {}

  /// TODO: Should be able to implement a copy, but deleted in the meantime
  network(const network &) = delete;
  network &operator=(const network &) = delete;

  // Move
  network(network &&) = delete;
  network &operator=(network &&) = delete;

  /// ##################### Copy Internal ##################### ///

  /**
   * @brief Get a set containing all names
   * @return set of names
   */
  std::set<jay::name> get_name_set()
  {
    std::shared_lock lock{ network_mtx_ };
    std::set<jay::name> set{ name_addr_map_.size() };
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
  bool insert(jay::name name, std::uint8_t address)
  {
    // Insert new controllers that dont have an address yet
    if (address > J1939_MAX_UNICAST_ADDR) {
      if (!in_network(name)) {
        std::scoped_lock lock{ network_mtx_ };
        name_addr_map_[name] = J1939_IDLE_ADDR;
        return true;
      }
      return false;
    }

    // Already exists
    if (match(name, address)) { return false; }

    std::scoped_lock lock{ network_mtx_ };
    if (auto it = addr_name_map_.find(address);
        it != addr_name_map_.end()) {// Their name is less than ours cant claim address
      if (it->second < name) {// Register device without an address
        name_addr_map_[name] = J1939_IDLE_ADDR;
        return true;
      }
      // Address is larger can claim address, clear existing device address
      name_addr_map_[it->second] = J1939_IDLE_ADDR;
    }

    addr_name_map_[address] = name;
    name_addr_map_[name] = address;
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
  bool available(std::uint8_t address) const
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
  bool claimable(std::uint8_t address, jay::name name) const
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
  bool match(jay::name name, std::uint8_t address)
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
  std::optional<jay::name> get_name(std::uint8_t address) const
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
  std::uint8_t get_address(const jay::name name) const
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
   * @param force the taking of an address from another ecu
   * @return empty address, if no addresses were available J1939_NO_ADDR is returned
   */
  std::uint8_t find_address(jay::name name, std::uint8_t preferred_address = 0, bool force = false) const
  {
    preferred_address = std::clamp(preferred_address, static_cast<std::uint8_t>(0), J1939_MAX_UNICAST_ADDR);
    std::shared_lock lock{ network_mtx_ };
    auto address = search(name, preferred_address, J1939_IDLE_ADDR, force);
    if (address == J1939_NO_ADDR) { address = search(name, 0, preferred_address, force); }
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
   * @param limit of the address search
   * @param force the taking of an address from another ecu
   * @return empty address, if no addresses were available J1939_NO_ADDR is returned
   */
  std::uint8_t search(jay::name name, std::uint8_t start_address, std::uint8_t end_address, bool force) const
  {
    for (std::uint8_t address = start_address; address < end_address; address++) {
      auto pair = addr_name_map_.find(address);
      if (pair == addr_name_map_.end()) { return address; }
      if (pair->second > name && force) { return address; }// Claim address is we have smaller name
    }
    return J1939_NO_ADDR;
  }

private:
  const std::string interface_name_{ "can0" };

  /// NOTE: Could implement maps as a bidirectional map,
  /// but would need somewhere for names with null address
  /// so overall it might not be worth it

  std::unordered_map<jay::name, std::uint8_t, jay::name::hash> name_addr_map_{};
  std::unordered_map<std::uint8_t, jay::name> addr_name_map_{};

  mutable std::shared_mutex network_mtx_{};
};

}// Namespace jay
