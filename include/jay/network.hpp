//
// Copyright (c) 2020 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the MIT License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef JAY_J1939_NETWORK_H
#define JAY_J1939_NETWORK_H

#pragma once

//C++
#include <mutex>          //scoped_lock
#include <shared_mutex>
#include <unordered_map>
#include <set>
#include <limits>
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <cstdint>

//Local
#include "header.hpp"

namespace jay
{

/**
 * Storage class for maintaining the relation between
 * controller name and its address.
*/
class network
{
public:

  network() = default;

  ///TODO: Should be able to implement a copy, but deleted in the meantime
  network(const network &) = delete;
  network &operator=(const network &) = delete;

  //Move
  network(network &&) = default;
  network &operator=(network &&) = default;

  /// ##################### Copy Internal ##################### ///

  /**
   * Get a copy of the name address pairings in the internal map
   * @note depricated should not use
  */
  std::unordered_map<uint64_t, uint8_t> cp_controller_map()
  {
    std::shared_lock lock{network_mtx_};
    return ctrl_addr_map_;
  }

  /**
   * 
  */
  std::set<uint64_t> get_controller_set()
  {
    std::shared_lock lock{network_mtx_};
    std::set<uint64_t> set{ctrl_addr_map_.size()};
    for(auto&pair : ctrl_addr_map_)
    {
      set.insert(pair.first);
    }
    return set;
  }

  /// ##################### Map access ##################### ///

  /**
   * Add a controller app to the network, if controller already exists
   * the address will be changed. name is added even if address cant
   * be claimed
   * @param ctrl_name
   * @param address of the controller app, if ADDRESS_NULL then
   * controller name registeded, but if already exist then address is cleared
   * @throw invalid_argument if address is a global address (255)
   * @return true if addre
   * @return false if the address is already claimed by a higher priority controller
  */
  bool insert(uint64_t ctrl_name, uint8_t address)
  {
    if(address > ADDRESS_NULL)
    {
      throw std::invalid_argument("Cant insert with global address");
    }

    //Can insert controllers that dont have an address yet, or use insert to clear address
    if(address == ADDRESS_NULL)
    {
      if(!in_network(ctrl_name))
      {
        ctrl_addr_map_[ctrl_name] = address;
        return true;
      }
      release(ctrl_name);
      return true;
    }

    ///NOTE: Might be some clever way to skip this, but leaving it in for now
    if(auto has_address = get_address(ctrl_name); has_address == address)
    { //Allready exists
      return true;
    }

    std::scoped_lock lock{network_mtx_};
    if(auto it = addr_ctrl_map_.find(address); it != addr_ctrl_map_.end())
    { //Their name is less than ours cant claim address
      if(it->second < ctrl_name)
      { //Register device without an address
        ctrl_addr_map_[ctrl_name] = ADDRESS_NULL;
        return false;
      }
      //Address is larger can claim address, clear existing device address
      ctrl_addr_map_[it->second] = ADDRESS_NULL;
    }

    addr_ctrl_map_[address] = ctrl_name;
    ctrl_addr_map_[ctrl_name] = address;
    return true;
  }

  /**
   * Release the address of the given name
   * @param ctrl_name to release
  */
  void release(const uint64_t ctrl_name)
  {
    std::scoped_lock lock{network_mtx_};
    if(ctrl_addr_map_.find(ctrl_name) == ctrl_addr_map_.end()){return;}
    auto address = ctrl_addr_map_[ctrl_name];
    ctrl_addr_map_[ctrl_name] = ADDRESS_NULL;
    if(addr_ctrl_map_.find(address) == addr_ctrl_map_.end()) {return;}
    addr_ctrl_map_.erase(address);
  }

  /**
   * Remove a name and address
   * @param ctrl_name to remove
  */
  void remove(const uint64_t ctrl_name)
  {
    std::scoped_lock lock{network_mtx_};
    if(ctrl_addr_map_.find(ctrl_name) == ctrl_addr_map_.end()){return;}
    auto address = ctrl_addr_map_[ctrl_name];
    ctrl_addr_map_.erase(ctrl_name);
    if(addr_ctrl_map_.find(address) == addr_ctrl_map_.end()) {return;}
    addr_ctrl_map_.erase(address);
  }

  /**
   * Clear all names and addresses
  */
  void clear() noexcept
  {
    std::scoped_lock lock{network_mtx_};
    ctrl_addr_map_.clear();
    addr_ctrl_map_.clear();
  }

  /**
   * Check if an address is taken
   * @param address to check
   * @return true if no controller has the address,
   * @return false if a controller has the address
   * @ruetrn false if address provided was a global address
  */
  bool available(uint8_t address) const
  {
    if(address >= jay::ADDRESS_NULL) return false;
    std::shared_lock lock{network_mtx_};
    return addr_ctrl_map_.find(address) == addr_ctrl_map_.end();
  }

  /**
   * Check if a Controller application is registered in the network
   * @param ctrl_name to check for
   * @return true if in the network, false if not
  */
  bool in_network(const uint64_t ctrl_name) const
  {
    std::shared_lock lock{network_mtx_};
    return ctrl_addr_map_.find(ctrl_name) != ctrl_addr_map_.end();
  }

  /**
   * Get the amount of addresses used in the network
   * @return address use count
  */
  size_t address_count() const
  {
    std::shared_lock lock{network_mtx_};
    return addr_ctrl_map_.size();
  }

  /**
   * Get the amount of controllers registered in the network
   * @return controller count
  */
  size_t controller_count() const
  {
    std::shared_lock lock{network_mtx_};
    return ctrl_addr_map_.size();
  }

  /**
   * Get the name of the application controller at address
   * @param address to get name at
   * @throw
   * @return name or ADDRESS_NULL if address is not used
  */
  std::optional<uint64_t> get_name(uint8_t address) const
  {
    ///NOTE: Dont need to check global addresses as they cant be inserted.
    std::shared_lock lock{network_mtx_};
    if(auto it = addr_ctrl_map_.find(address); it != addr_ctrl_map_.end()) 
    {
      return it->second;
    }
    return std::nullopt;
  }

  /**
   * Get the address of the application controller
   * @param ctrl_name that we want the address for
   * @return address of the controller
   * @return ADDRESS_NULL if no address is claimed or not in the network
  */
  uint8_t get_address(const uint64_t ctrl_name) const
  {
    std::shared_lock lock{network_mtx_};
    if(auto it = ctrl_addr_map_.find(ctrl_name); it != ctrl_addr_map_.end()) 
    { 
      return it->second; 
    }
    return ADDRESS_NULL;
  }

  /**
   * Check if any addresses are available in the network
   * @return true if address network has reached 255
   * @return false if addresses are still available
  */
  bool full() const
  {
    std::shared_lock lock{network_mtx_};
    ///Is 256 - 2 on end as we dont want to occupy ADDRESS_NULL and ADDRESS_GLOBAL?
    return addr_ctrl_map_.size() >= ADDRESS_NULL;
  }

  /**
   * Search the network empty addresses
   * @param name of the controller appliction looking for address
   * @param preffed_address to start search from, clameped to between 0 - 253
   * @param force the taking of an address from another ecu
   * @return empty address, if no addresses were available ADDRESS_NULL is returned 
  */
  uint8_t find_address(uint64_t name, uint8_t preffed_address = 0, bool force = false) const
  {
    preffed_address = std::clamp(preffed_address, static_cast<uint8_t>(0), static_cast<uint8_t>(jay::ADDRESS_NULL - 1));
    std::shared_lock lock{network_mtx_};
    auto address = search(name, preffed_address, jay::ADDRESS_NULL, force);
    if(address == ADDRESS_NULL) { address = search(name, 0, preffed_address, force); }
    return address;
    //if no address was found above the preffered address, check bellow
  }

  ///TODO: Check the name of other devices and see if they can change their address

private:

  /**
   * Search the network empty addresses
   * @param name of the controller appliction looking for address
   * @param preffed_address to start search from
   * @param limit of the address search
   * @param force the taking of an address from another ecu
   * @return empty address, if no addresses were available ADDRESS_NULL is returned 
  */
  uint8_t search(uint64_t name, uint8_t start_address, uint8_t end_address, bool force) const 
  {
    for(uint8_t address = start_address; address < end_address; address++)
    {
      auto pair = addr_ctrl_map_.find(address);
      if(pair == addr_ctrl_map_.end()){ return address; }
      if(pair->second > name && force) { return address;}  //Claim address is we have smaller name
    }
    return jay::ADDRESS_NULL;
  }

private:

  ///TODO: Implement map as a bidirectional map, would need special container for null address
  ///TODO: Have map use name class instead of uint64_t?

  std::unordered_map<uint64_t, uint8_t> ctrl_addr_map_{};
  std::unordered_map<uint8_t, uint64_t> addr_ctrl_map_{};
  
  mutable std::shared_mutex network_mtx_{};
};

} //Namespace jay

#endif