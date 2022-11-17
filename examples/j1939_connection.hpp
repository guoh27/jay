//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#ifndef J1939_CONNECTION_H
#define J1939_CONNECTION_H

#pragma once

//C++
#include <queue>
#include <vector>

//Libraries
#include "boost/asio.hpp"

#include "canary/raw.hpp"
#include "canary/filter.hpp"

#include "../include/jay/frame.hpp"
#include "../include/jay/network.hpp"

///TODO: There is probably some way to combine the CAN and j1939 connection
/// into a single templated class
/// then should only need to specify callbacks and buffer types
/// to create a raw and J1939 object
/// though keeping it seperate for now until i find a good solution

/**
 * J1939 Connection for reading and sending j1939 messages
*/
class J1939Connection : public std::enable_shared_from_this<J1939Connection>
{
public:

  struct Callbacks
  {
    std::function<void(J1939Connection*)> on_start;
    std::function<void(J1939Connection*)> on_destroy;
    std::function<void(jay::frame)> on_data;
    std::function<void(const std::string, const boost::system::error_code)> on_fail;
  };

  /**
   * @brief Construct a new Can Connection object
   * 
   * @param io_context for this connection
   * @param callbacks triggered interaly
   */
  J1939Connection(
    boost::asio::io_context& io_context,
    std::shared_ptr<jay::network> network);

  J1939Connection(
    boost::asio::io_context& io_context,
    std::shared_ptr<jay::network> network, 
    Callbacks && callbacks);

  J1939Connection(
    boost::asio::io_context& io_context,
    std::shared_ptr<jay::network> network,
    Callbacks && callbacks,
    jay::name local_name);

  J1939Connection(
    boost::asio::io_context& io_context,
    std::shared_ptr<jay::network> network,
    Callbacks && callbacks,
    jay::name local_name, 
    jay::name target_name);

  ~J1939Connection();

  bool Open(canary::raw::endpoint endpoint, 
    std::vector<canary::filter> filters);

  /**
   * Listen for incomming j1939 frames
   * @param
  */
  void Start();

  /// ##################### Set/Get ##################### ///

  void SetCallbacks(Callbacks && callbacks)
  {
    callbacks_ = std::move(callbacks);
  }

  void SetLocalName(jay::name name)
  {
    local_name_ = name;
  }

  void SetTargetName(jay::name name)
  {
    target_name_ = name;
  }

  /**
   * Get bound name of the controller application
   * @return bound name, is 0 if no name is bound
  */
  std::optional<jay::name> GetLocalName() const
  {
    return local_name_;
  } 

  /**
   * @todo lock mutex?
   * Get name of the remote controller application this socket is connected to
   * @return connected name, is 0 if no name is bound
  */
  std::optional<jay::name> GetTargeName() const
  {
    return target_name_;
  }

  std::shared_ptr<jay::network> GetNetwork() const
  {
    return network_;
  }


  /// ##################### WRITE ##################### ///

  /**
   * Send a frame to socket without any checks
   * @param j1939_frame that will be sent
  */
  void SendRaw(const jay::frame& j1939_frame);

  /**
   * Send a broadcast frame to the socket
   * @param j1939_frame that will be broadcast, the source address
   * is set by the socket
   * @throw std::invalid_argument if frame does not contain a
   * broadcast PDU_S or there is if the socket does not have
   * and address
   * are not available
  */
  void SendBroadcast(jay::frame& j1939_frame);

  /**
   * Send frame to connected controller application
   * @param j1939_frame that will be sent, both source address
   * and PDU specifier is set by the socket
   * @return true if written, false if an error occured
   * @throw std::invalid_argument if no connected controller
   * app name has been set
  */ 
  void Send(jay::frame& j1939_frame);

  /**
   * Send frame to specific controller application
   * @param destination - name of the controller application to send to
   * @param j1939_frame that will be sent, both source address
   * and PDU specifier is set by the socket
   * @return true if written, false if an error occured
   * @throw std::invalid_argument if source and destination addresses
   * are not available
  */
  void SendTo(const uint64_t destination, jay::frame& j1939_frame);

  /// ##################### GETTERS ##################### ///


private:

  void OnFail(char const* what, boost::system::error_code ec);

  /**
   * 
  */
  void Read();

  /**
   * 
  */
  void Write();

private:

  //Injected

  canary::raw::socket socket_;
  std::shared_ptr<jay::network> network_{};
  Callbacks callbacks_;

  std::optional<jay::name> local_name_{};
  std::optional<jay::name> target_name_{};

  //Internal

  jay::frame buffer_{};
  std::queue<jay::frame> queue_{};


};

#endif