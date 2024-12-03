//
// Copyright (c) 2024 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef JAY_J1939_NETWORK_H
#define JAY_J1939_NETWORK_H

#pragma once

// C++
#include <algorithm>//std::clamp
#include <mutex>//std::scoped_lock
#include <optional>//std::optional
#include <shared_mutex>//std::shared_mutex, std::shared_lock
#include <string>//std::string
#include <unordered_map>//std::unordered_map
#include <vector>//std::vector

// Local
#include "name.hpp"// name, jay globals, and std::uint8_t

namespace jay {

/**
 * @mainpage
 * @brief Storage class for maintaining the relation between
 * j1939 name and its address.
 * @note Is thread safe, needs to be passed by reference or pointer
 * as moving or copying is not allowed.
 */
class network
{
public:
  /**
   * @brief Construct a new network object
   *
   * @param interface_name that the network is assosiated with
   */
  network(const std::string &interface_name) noexcept : interface_name_(interface_name) {}

  network(const std::string &interface_name, const std::function<void(const jay::name)> &on_new_name) noexcept
    : interface_name_(interface_name), on_new_name_(on_new_name)
  {}

  network(const std::string &interface_name, std::function<void(const jay::name)> &&on_new_name) noexcept
    : interface_name_(interface_name), on_new_name_(std::move(on_new_name))
  {}


  /// TODO: Should be able to implement a copy, but deleted in the meantime
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
  inline void on_new_name_callback(const std::function<void(const jay::name)> &on_new_name) noexcept
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
  inline void on_new_name_callback(std::function<void(const jay::name)> &&on_new_name) noexcept
  {
    std::scoped_lock lock{ network_mtx_ };
    on_new_name_ = std::move(on_new_name);
  }

  /// ##################### Copy Internal ##################### ///

  /**
   * @brief Get a vector containing all names in the network
   * @return vector of names
   */
  [[nodiscard]] inline std::vector<jay::name> names_vector() const noexcept
  {
    std::shared_lock lock{ network_mtx_ };
    std::vector<jay::name> names{};
    names.reserve(name_address_map_.size());
    for (auto &pair : name_address_map_) { names.emplace_back(pair.first); }
    return names;
  }

  /**
   * @brief Get a vector containing all used addresses in the network
   * @return vector of addresses
   */
  [[nodiscard]] inline std::vector<std::uint8_t> address_vector() const noexcept
  {
    std::shared_lock lock{ network_mtx_ };
    std::vector<std::uint8_t> addresses{};
    addresses.reserve(address_name_map_.size());
    for (auto &pair : address_name_map_) { addresses.emplace_back(pair.first); }
    return addresses;
  }

  /// ##################### Map access ##################### ///

  /**
   * @brief Try to emplace a name in the network
   *
   * Same as try_emplace for std::unordered_map, but with multithread safety
   *
   * @param name to emplace
   * @return true if name could be empalced
   * @return false if name could not be empalced or already exists
   * @throw std::memory_resource::bad_alloc if memory allocation fails
   */
  bool try_emplace(const jay::name name)
  {
    std::scoped_lock lock{ network_mtx_ };
    if (name_address_map_.find(name) != name_address_map_.end()) { return false; }
    auto inserted = name_address_map_.try_emplace(name, J1939_IDLE_ADDR).second;
    if (on_new_name_ && inserted) { on_new_name_(name); }
    return inserted;
  }

  /**
   * @brief Try to claim an address for the name in the network
   *
   * @note If name does not exist it will be created.
   * @note If the name already exists it will try to claim the new address
   * and discard the old one.
   *
   * @param name
   * @param address of the name app, if J1939_NO_ADDR then
   * name name registeded, but if already exist then address is cleared
   * @return true if name is in the network and address was claimed
   * @return false if we failed claim the address
   * @throw std::memory_resource::bad_alloc if memory allocation fails
   * @todo Will it cause issues if we trigger on_new before we check if address can be claimed?
   * @todo should we trigger the callback if we fail to claim the address?
   * @todo Should we trigger the callback outside the lock instead?
   */
  bool try_address_claim(const jay::name name, const std::uint8_t address)
  {
    bool is_new_name = false;
    std::scoped_lock lock{ network_mtx_ };
    if (name_address_map_.find(name) == name_address_map_.end()) {
      auto inserted = name_address_map_.try_emplace(name, J1939_IDLE_ADDR).second;
      if (on_new_name_ && inserted) { on_new_name_(name); }
    } else {
      /// If new address is not the same as the old address then remove the old address
      auto old_address = name_address_map_.at(name);
      if (address != old_address) {
        address_name_map_.erase(old_address);
      } else {
        return true;
      }
    }

    /// Claiming an idle address
    if (address > J1939_MAX_UNICAST_ADDR) {
      name_address_map_.at(name) = J1939_IDLE_ADDR;
      return true;
    }

    /// Check if address is available
    auto it = address_name_map_.find(address);
    if (it == address_name_map_.end()) {
      address_name_map_.try_emplace(address, name);
      name_address_map_.at(name) = address;
      return true;
    }

    /// Address is already taken by another name, check if we can claim it
    if (name.has_priority_over(it->second)) {
      name_address_map_.at(it->second) = J1939_IDLE_ADDR;
      address_name_map_.at(address) = name;
      name_address_map_.at(name) = address;
      return true;
    }

    return false;
  }

  /**
   * @brief Release the address of the given name
   * @param name to release
   */
  inline void release(const jay::name name) noexcept
  {
    std::scoped_lock lock{ network_mtx_ };

    if (name_address_map_.find(name) == name_address_map_.end()) { return; }
    auto address = name_address_map_.at(name);
    name_address_map_.at(name) = J1939_IDLE_ADDR;

    if (address_name_map_.find(address) == address_name_map_.end()) { return; }
    address_name_map_.erase(address);
  }

  /**
   * @brief erase the address and set the name to idle address
   * @param address to release
   */
  inline void erase(const std::uint8_t address) noexcept
  {
    std::scoped_lock lock{ network_mtx_ };

    if (address_name_map_.find(address) == address_name_map_.end()) { return; }
    auto name = address_name_map_.at(address);
    address_name_map_.erase(address);

    if (name_address_map_.find(name) == name_address_map_.end()) { return; }
    name_address_map_.at(name) = J1939_IDLE_ADDR;
  }

  /**
   * @brief Remove a name and address
   * @param name to remove
   */
  inline void erase(const jay::name name) noexcept
  {
    std::scoped_lock lock{ network_mtx_ };

    if (name_address_map_.find(name) == name_address_map_.end()) { return; }
    auto address = name_address_map_.at(name);
    name_address_map_.erase(name);

    if (address_name_map_.find(address) == address_name_map_.end()) { return; }
    address_name_map_.erase(address);
  }

  /**
   * @brief Clear all names and addresses
   */
  inline void clear() noexcept
  {
    std::scoped_lock lock{ network_mtx_ };
    name_address_map_.clear();
    address_name_map_.clear();
  }

  /**
   * @brief Check if an address is taken
   * @param address to check
   * @return true if no name has claimed the address
   * @return false if a name has the address
   * @return false if address provided was a global address
   */
  [[nodiscard]] inline bool available(const std::uint8_t address) const
  {
    if (address > J1939_MAX_UNICAST_ADDR) { return false; }
    std::shared_lock lock{ network_mtx_ };
    return address_name_map_.find(address) == address_name_map_.end();
  }

  /**
   * @brief Check if an address is claimable by the name
   * @param address that want name want to claim
   * @param name that wants to claim the address
   * @return true if no name has claimed the address
   * @return true if claiming name has higher priority than existing name
   * @return false if global address
   * @return false if existing name has higher priority or equal priority to claiming name
   */
  [[nodiscard]] inline bool claimable(const std::uint8_t address, const jay::name name) const noexcept
  {
    if (address > J1939_MAX_UNICAST_ADDR) { return false; }
    std::shared_lock lock{ network_mtx_ };
    if (auto it = address_name_map_.find(address); it != address_name_map_.end()) {
      return name.has_priority_over(it->second);
    }
    return true;
  }

  /**
   * @brief Check if a name application is registered in the network
   * @param name to check for
   * @return true if in the network, false if not
   */
  [[nodiscard]] inline bool in_network(const jay::name name) const noexcept
  {
    std::shared_lock lock{ network_mtx_ };
    return name_address_map_.find(name) != name_address_map_.end();
  }

  /**
   * @brief Check if address - name pairing is matched in the network
   * @param name
   * @param address
   * @return true if address and name are paired
   * @return false if address and name are not paired
   * @todo Should it also check the other way, that address map has name?
   */
  [[nodiscard]] inline bool match(const jay::name name, const std::uint8_t address) const noexcept
  {
    std::shared_lock lock{ network_mtx_ };
    if (auto it = name_address_map_.find(name); it != name_address_map_.end()) { return it->second == address; }
    return false;
  }

  /**
   * @brief Get the amount of addresses used in the network
   * @return address use count
   */
  [[nodiscard]] inline size_t address_size() const noexcept
  {
    std::shared_lock lock{ network_mtx_ };
    return address_name_map_.size();
  }

  /**
   * @brief Get the amount of names registered in the network
   * @return name count
   */
  [[nodiscard]] inline size_t name_size() const noexcept
  {
    std::shared_lock lock{ network_mtx_ };
    return name_address_map_.size();
  }

  /**
   * @brief Get the name at address
   * @param address to find name for
   * @return name
   * @return nullopt if address is not used
   */
  [[nodiscard]] std::optional<jay::name> inline find_name(const std::uint8_t address) const noexcept
  {
    /// NOTE: Dont need to check global addresses as they cant be inserted.
    std::shared_lock lock{ network_mtx_ };
    if (auto it = address_name_map_.find(address); it != address_name_map_.end()) { return it->second; }
    return std::nullopt;
  }

  /**
   * @brief Get the address associated with given name
   * @param name that we want the address for
   * @return address of the name, can be J1939_IDLE_ADDR,
   * @return nullopt if not in the network
   */
  [[nodiscard]] std::optional<std::uint8_t> inline find_address(const jay::name name) const noexcept
  {
    std::shared_lock lock{ network_mtx_ };
    if (auto it = name_address_map_.find(name); it != name_address_map_.end()) { return it->second; }
    return std::nullopt;
  }

  /**
   * @brief Check if any addresses are available in the network
   * @return true if address network has reached 255
   * @return false if addresses are still available
   */
  [[nodiscard]] inline bool is_full() const noexcept
  {
    std::shared_lock lock{ network_mtx_ };
    /// Is 256 - 2 on end as we dont want to occupy J1939_IDLE_ADDR and J1939_NO_ADDR
    return address_name_map_.size() > J1939_MAX_UNICAST_ADDR;
  }

  /**
   * Search the network empty addresses
   * @param name of the name appliction looking for address
   * @param preffed_address to start search from, clameped to between 0 - 253
   * @param force the taking of an address from another ecu
   * @return available address, if no addresses were available J1939_IDLE_ADDR is returned
   * @note The function will search for available addresses above the prefferend address,
   * if none are found the search will continue from 0 to preffered address
   * @todo should this return optional?
   */
  [[nodiscard]] std::uint8_t find_available_address(const jay::name name,
    const std::uint8_t preffed_address = 0,
    const bool force = false) const noexcept
  {
    auto scoped_preffed_address = std::clamp(preffed_address, static_cast<std::uint8_t>(0), J1939_MAX_UNICAST_ADDR);
    std::shared_lock lock{ network_mtx_ };
    auto address = search(name, scoped_preffed_address, J1939_IDLE_ADDR, force);
    if (address == J1939_IDLE_ADDR) { address = search(name, 0, scoped_preffed_address, force); }
    return address;
  }

  /// TODO: Check the name of other devices and see if they can change their address

  /**
   * @brief Get the name of the interface that this network is assosiated with
   * @return const std::string&
   */
  [[nodiscard]] inline const std::string &interface_name() const noexcept { return interface_name_; }

private:
  /**
   * Search the network empty addresses
   * @param name of the name appliction looking for address
   * @param preffed_address to start search from
   * @param limit of the address search
   * @param force the taking of an address from another ecu
   * @return empty address, if no addresses were available J1939_NO_ADDR is returned
   * @todo should this return optional?
   */
  [[nodiscard]] std::uint8_t search(const jay::name name,
    const std::uint8_t start_address,
    const std::uint8_t end_address,
    const bool force) const noexcept
  {
    for (std::uint8_t address = start_address; address < end_address; address++) {
      auto pair = address_name_map_.find(address);
      if (pair == address_name_map_.end()) { return address; }
      if (name.has_priority_over(pair->second) && force) { return address; }// Claim address is we have smaller name
    }
    return J1939_NO_ADDR;
  }

private:
  const std::string interface_name_{ "can0" };
  std::function<void(const jay::name)> on_new_name_;

  /// NOTE: Could implement maps as a bidirectional map,
  /// but would need somewhere for names with null address
  /// so overal it migth not be worth it

  std::unordered_map<jay::name, std::uint8_t, jay::name::hash> name_address_map_{};
  std::unordered_map<std::uint8_t, jay::name> address_name_map_{};

  mutable std::shared_mutex network_mtx_{};
};

}// Namespace jay

#endif