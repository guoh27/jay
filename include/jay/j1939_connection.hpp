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
#include <functional>
#include <linux/can.h>
#include <queue>
#include <vector>

// Libraries
#include "boost/asio.hpp"

#include "canary/filter.hpp"
#include "canary/interface_index.hpp"
#include "canary/raw.hpp"
#include "canary/socket_options.hpp"

#include "frame.hpp"
#include "network.hpp"
#include "transport_protocol.hpp"

namespace jay {
/**
 * J1939 Connection analog for reading and sending j1939 messages
 *
 * Callbacks are used to signal start on end of connection.
 * Incoming data is also passed along using a callbacks.
 * Outgoing can frames are also queued before being sent.
 * @note The connection manages its own lifetime
 * outgoing buffers such as timed queues and so on
 */
class j1939_connection
{
public:
  using J1939OnSelf = std::function<void(j1939_connection *)>;

  class bus_adapter : public bus
  {
  public:
    explicit bus_adapter(boost::asio::strand<boost::asio::any_io_executor> &strand, BusSend send)
      : bus(strand), send_(send)
    {}

    // jay::j1939::bus API subset
    // block send
    bool send(const jay::frame &fr) override { return send_(fr); }

    std::uint8_t source_address() const noexcept override { return src_addr_; }
    void source_address(std::uint8_t sa) noexcept { src_addr_ = sa; }

  private:
    BusSend send_;
    std::uint8_t src_addr_{ 0xFE };
  };


  /**
   * @brief Construct a new Can Connection object
   *
   * @param io_context for performing async io operation
   * @param network containing address name pairs
   */
  j1939_connection(boost::asio::io_context &io, std::shared_ptr<jay::network> net)
    : socket_(boost::asio::make_strand(io)), network_(std::move(net)), strand_(socket_.get_executor()),
      bus_(std::make_unique<bus_adapter>(strand_, [this](const jay::frame &fr) { return this->send(fr); })),
      tp_(std::make_unique<transport_protocol>(*bus_))
  {}

  /**
   * @brief Construct a new J1939Connection object
   *
   * @param io_context for performing async io operation
   * @param network containing address name pairs
   * @param local_name that this connection is sending messages from
   * @param target_name that this connection is sending messages to
   */
  j1939_connection(boost::asio::io_context &io,
    std::shared_ptr<jay::network> net,
    std::optional<jay::name> local_name,
    std::optional<jay::name> target_name)
    : socket_(boost::asio::make_strand(io)), network_(std::move(net)), strand_(socket_.get_executor()),
      bus_(std::make_unique<bus_adapter>(strand_, [this](const jay::frame &fr) { return this->send(fr); })),
      tp_(std::make_unique<transport_protocol>(*bus_)), local_name_(local_name), target_name_(target_name)
  {}

  /**
   * @brief Destroy the J1939Connection object
   */
  ~j1939_connection()
  {
    if (on_destroy_) { on_destroy_(this); }
  }

  /**
   * @brief Open an j1939 endpoint
   * @param endpoint to open
   * @return true if opened endpoint
   * @return false if failed to open endpoint
   */
  bool open()
  {
    canary::error_code ec;
    auto endpoint = canary::raw::endpoint{ canary::get_interface_index(network_->get_interface_name(), ec) };
    if (ec.failed()) {
      if (on_error_) { on_error_("open " + network_->get_interface_name() + " failed", ec); }
      return false;
    }
    socket_.open(endpoint.protocol(), ec);
    if (ec.failed()) {
      if (on_error_) { on_error_("open " + network_->get_interface_name() + " failed", ec); }
      return false;
    }
    socket_.bind(endpoint, ec);
    if (ec.failed()) {
      if (on_error_) { on_error_("bind " + network_->get_interface_name() + " failed", ec); }
      return false;
    }
    return true;
  }

  /// set filters options, will override old filters
  void set_filter_any(const std::vector<canary::filter> &filters)
  {
    socket_.set_option(canary::filter_if_any{ filters.data(), filters.size() });
  }

  /// set join filters options, will override old filters
  void set_filter_all(const std::vector<canary::filter> &filters)
  {
    socket_.set_option(canary::filter_if_all{ filters.data(), filters.size() });
  }

  /**
   * Listen for incoming j1939 frames
   */
  void start()
  {
    if (on_start_) { on_start_(this); }
    async_read();
  }

  /// ##################### Set/Get ##################### ///

  /**
   * @brief Set the Network object
   * @param network to use for this connection
   */
  void on_start(J1939OnSelf on_start) { on_start_ = std::move(on_start); }

  /**
   * @brief Set the OnDestroy object
   * @param on_destroy callback that is called when this connection is destroyed
   */
  void on_close(J1939OnSelf on_destroy) { on_destroy_ = std::move(on_destroy); }

  /**
   * @brief Set the OnRead object
   * @param on_read callback that is called when a frame is read
   */
  void on_read(J1939OnFrame on_read) { on_read_ = std::move(on_read); }

  /**
   * @brief Set the OnSend object
   * @param on_send callback that is called when a frame is sent
   */
  void on_send(J1939OnFrame on_send) { on_send_ = std::move(on_send); }

  /**
   * @brief Set the OnError object
   * @param on_error callback that is called when an error occurs
   */
  void on_error(J1939OnError on_error) { on_error_ = std::move(on_error); }

  void on_data(J1939OnData cb)
  {
    on_data_ = cb;
    tp_->set_rx_handler(cb);
  }

  /**
   * @brief Set the local j1939 name
   * @param name of the device this connection is sending
   * messages from, used for setting source address in messages
   */
  void set_local_name(jay::name name) { local_name_ = name; }

  /**
   * @brief Set the Target Name object
   * @param name of the device this connection is sending
   * messages to, used for setting destination address in messages
   */
  void set_target_name(jay::name name) { target_name_ = name; }

  /**
   * Get local name on this connection
   * @return optional name, is null_opt if none was set
   */
  std::optional<jay::name> get_local_name() const { return local_name_; }

  /**
   * Get target name on this connection
   * @return optional name, is null_opt if none was set
   */
  std::optional<jay::name> get_target_name() const { return target_name_; }

  /**
   * @brief Get the Network reference
   * @return jay::network&
   */
  std::shared_ptr<jay::network> get_network() const { return network_; }

  /// ##################### WRITE ##################### ///

  /**
   * Send a frame to socket without any checks
   * @param j1939_frame that will be sent
   */
  bool send_raw(const jay::frame &j1939_frame) { return write(j1939_frame); }

  bool send(const data &data)
  {
    if (data.payload.size() <= 8) {
      jay::frame fr{};
      fr.header = data.header;
      for (std::size_t i = 0; i < data.payload.size(); ++i) { fr.payload[i] = data.payload[i]; }
      return send(fr);
    } else {
      auto source_address = network_->get_address(*local_name_);
      if (source_address == J1939_IDLE_ADDR) {
        on_error_(
          "Socket has no source address", boost::system::errc::make_error_code(boost::system::errc::invalid_argument));
        return false;
      }
      bus_->source_address(source_address);
      return tp_->send(data.payload, data.header.pdu_specific(), data.header.pgn());
    }
  }

  /**
   * Send frame to connected controller application
   * @param j1939_frame that will be sent, both source address
   * and PDU specifier is set by the socket
   * @return true if written, false if an error occurred
   * app name has been set
   */
  bool send(const jay::frame &j1939_frame)
  {
    if (j1939_frame.header.is_broadcast()) {
      auto source_address = network_->get_address(*local_name_);
      if (source_address == J1939_IDLE_ADDR) {
        on_error_(
          "Socket has no source address", boost::system::errc::make_error_code(boost::system::errc::invalid_argument));
        return false;
      }

      std::memcpy(&out_buffer_, &j1939_frame, sizeof(j1939_frame));
      out_buffer_.header.source_address(source_address);
      return send_raw(out_buffer_);
    } else {
      if (!target_name_.has_value()) {
        on_error_(
          "Socket has no connection name", boost::system::errc::make_error_code(boost::system::errc::invalid_argument));
        return false;
      }

      return send_to(*target_name_, j1939_frame);
    }
  }

  /**
   * Send frame to specific controller application
   * @param destination - name of the controller application to send to
   * @param j1939_frame that will be sent, both source address
   * and PDU specifier is set by the socket
   * @return true if written, false if an error occurred
   * @throw std::invalid_argument if source and destination addresses
   * are not available
   */
  bool send_to(jay::name name, const jay::frame &j1939_frame)
  {
    if (!local_name_.has_value()) {
      on_error_(
        "Socket has no local name", boost::system::errc::make_error_code(boost::system::errc::invalid_argument));
      return false;
    }

    auto source_address = network_->get_address(*local_name_);
    if (source_address == J1939_IDLE_ADDR) {
      on_error_(
        "Socket has no source address", boost::system::errc::make_error_code(boost::system::errc::invalid_argument));
      return false;
    }

    auto destination_address = network_->get_address(name);
    if (destination_address == J1939_IDLE_ADDR) {
      on_error_(
        "Destination has no address", boost::system::errc::make_error_code(boost::system::errc::invalid_argument));
      return false;
    }

    std::memcpy(&out_buffer_, &j1939_frame, sizeof(j1939_frame));

    out_buffer_.header.source_address(source_address);
    out_buffer_.header.pdu_specific(destination_address);

    return send_raw(out_buffer_);
  }

private:
  /**
   * @brief Called when an event fails
   * @param what failed
   * @param ec for the error
   */
  void on_error(const std::string &what, boost::system::error_code ec)
  {
    // Don't report on canceled operations
    if (ec == boost::asio::error::operation_aborted || !on_error_) { return; }

    on_error_(what, ec);
  }

  /**
   * Read data from socket
   *
   * @todo Are we clearing the buffer
   * correctly it seems the header would remain the same
   */
  void async_read()
  {
    socket_.async_receive(canary::net::buffer(&buffer_, sizeof(buffer_)), [this](auto error, auto) {
      if (error) { return on_error("read", error); }

      on_read_(buffer_);

      // Trigger callback with frame if we are supposed to get the frame
      if (check_address()) {
        tp_->on_can_frame(buffer_);
        on_data_({ buffer_.header, { buffer_.payload.begin(), buffer_.payload.end() } });
      }

      // Clear buffer
      buffer_.payload.fill(0);
      buffer_.header.id(0);

      // Queue another read
      async_read();
    });
  }

  bool write(const frame &j1939_frame)
  {
    boost::system::error_code ec;
    auto n = socket_.send(canary::net::buffer(&j1939_frame, sizeof(j1939_frame)), 0, ec);

    // Handle the error, if any
    if (ec || n != sizeof(j1939_frame)) {
      on_error("write", ec ? ec : boost::asio::error::operation_aborted);
      return false;
    }

    // Callback with data sent
    if (on_send_) { on_send_(j1939_frame); };
    return true;
  }

  /**
   * @note Since we are using raw can the filter cant be sure if the received message is for us.
   * As dynamic addressing could cause a filter to be invalid if the source address was
   * included in the filter. As such we need to check if we are allowed to receive the frame.
   * Since the raw can filters will take care of PGN we only need to check for broadcasts,
   * source if we are using a connection, and if we are the target of the message
   * @note could be able to eliminate this by using j1939 sockets instead!
   */
  bool check_address() const
  {
    /// If we dont have any names then accept any frame
    if (!target_name_ && !local_name_) { return true; }

    // We can accept broadcasts from target is there is one
    if (buffer_.header.is_broadcast()) {
      if (target_name_) { return network_->get_address(*target_name_) == buffer_.header.source_address(); }
      return true;
    }

    // If we have both target and local name then
    // check source and target address, given its not a broadcast
    if (target_name_ && local_name_) {
      return network_->get_address(*target_name_) == buffer_.header.source_address()
             && network_->get_address(*local_name_) == buffer_.header.pdu_specific();
    }

    /// Feel like these two last are more outliers

    // If message is for local name, but we dont care who its from
    if (!target_name_ && local_name_) { return network_->get_address(*local_name_) == buffer_.header.pdu_specific(); }

    // Check that the message is from our intended target but we dont care if its for us
    if (target_name_ && !local_name_) {
      return network_->get_address(*target_name_) == buffer_.header.source_address();
    }

    return false;
  }

private:
  canary::raw::socket socket_; /**< raw CAN-bus socket */
  std::shared_ptr<jay::network> network_; /**< Network reference for querying network for addresses */
  boost::asio::strand<boost::asio::any_io_executor> strand_;

  std::unique_ptr<bus_adapter> bus_;
  std::unique_ptr<transport_protocol> tp_;

  /**
   * @brief Callback for when connection is stated
   * @note is optional
   */
  J1939OnSelf on_start_;

  /**
   * @brief Callback for when connection is destroyed
   * @note is optional
   */
  J1939OnSelf on_destroy_;

  /**
   * @brief Callback for when frame is received
   * @note is required
   */
  J1939OnFrame on_read_;

  /**
   * @brief Callback for when frame is sent
   * @note is optional
   */
  J1939OnFrame on_send_;

  /**
   * @brief Callback for when an error occurs
   *
   * Constains a string indicating where the error happened and
   * an error code detailing the error.
   * @note is required
   */
  J1939OnError on_error_;

  /**
   * @brief Callback for when data is received
   *
   */
  J1939OnData on_data_;

  std::optional<jay::name> local_name_{}; /**< Optional local j1939 name */
  std::optional<jay::name> target_name_{}; /**< Optional targeted j1939 name */

  // Internal

  jay::frame buffer_{};//< Incoming frame buffer
  jay::frame out_buffer_{};// Outcoming frame buffer
};

}// namespace jay