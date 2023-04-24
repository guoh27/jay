//
// Copyright (c) 2022 Bjørn Fuglestad, Jaersense AS (bjorn@jaersense.no)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/bjorn-jaes/jay
//

#include "j1939_connection.hpp"

//Linux
#include <linux/can.h>

//Libraries
#include "canary/socket_options.hpp"
#include "canary/interface_index.hpp"

J1939Connection::J1939Connection(
  boost::asio::io_context& io_context,
  jay::network& network): 
  socket_(boost::asio::make_strand(io_context)),
  network_(network)
{
}

J1939Connection::J1939Connection(
  boost::asio::io_context& io_context,
  jay::network& network, 
  Callbacks && callbacks): 
  socket_(boost::asio::make_strand(io_context)), network_(network),
  callbacks_(std::move(callbacks))
{
}

J1939Connection::J1939Connection(
  boost::asio::io_context& io_context,
  jay::network& network,
  Callbacks && callbacks,
  std::optional<jay::name> local_name, 
  std::optional<jay::name> target_name) : 
  socket_(boost::asio::make_strand(io_context)), network_(network),
  callbacks_(std::move(callbacks)), local_name_(local_name), 
  target_name_(target_name) 
{
}

J1939Connection::~J1939Connection()
{
  if(callbacks_.on_destroy)
  {
    callbacks_.on_destroy(this);
  }
}

bool J1939Connection::Open(const std::vector<canary::filter>& filters)
{
  try
  {
    auto endpoint = canary::raw::endpoint{
      canary::get_interface_index(network_.get_interface_name())};
    socket_.open(endpoint.protocol());
    socket_.bind(endpoint);
    if(filters.size() > 0)
    {
      socket_.set_option(canary::filter_if_any{ filters.data(), filters.size() });
    }
  }
  catch(boost::system::error_code ec)
  {
    callbacks_.on_error("open", ec);
    return false;
  }
  return true;
}

void J1939Connection::Start()
{
  if(callbacks_.on_start)
  {
    callbacks_.on_start(this);
  }
  Read();
}

void J1939Connection::SendRaw(const jay::frame& j1939_frame)
{
  // Post our work to the strand, this ensures
  // that the members of `this` will not be
  // accessed concurrently.
  boost::asio::post(
    socket_.get_executor(),
    [j1939_frame, self = shared_from_this()]()
    {
      // Always add to queue
      self->queue_.push(j1939_frame);

      // Are we already writing?
      if (self->queue_.size() > 1)
      {
        return;
      }

      // We are not currently writing, so send this immediately
      self->Write();
    }
  );
}

void J1939Connection::SendBroadcast(jay::frame& j1939_frame)
{
  if(!j1939_frame.header.is_broadcast())
  {
    throw std::invalid_argument("Not a broadcast frame");
  }

  if(!local_name_.has_value())
  {
    throw std::invalid_argument("Socket has no local name");
  }

  auto source_address = network_.get_address(*local_name_);
  if(source_address == J1939_IDLE_ADDR)
  {
    throw std::invalid_argument("Socket has no source address");
  }

  j1939_frame.header.source_adderess(source_address);
  return SendRaw(j1939_frame);
}

void J1939Connection::Send(jay::frame& j1939_frame)
{
  if(!target_name_.has_value())
  {
    throw std::invalid_argument("Socket has no connection name");
  }

  return SendTo(*target_name_, j1939_frame);
}


void J1939Connection::SendTo(const uint64_t destination, jay::frame& j1939_frame)
{
  if(!local_name_.has_value())
  {
    throw std::invalid_argument("Socket has no local name");
  }

  auto source_address = network_.get_address(*local_name_);
  if(source_address == J1939_IDLE_ADDR)
  {
    throw std::invalid_argument("Socket has no source address");
  }

  auto destination_address = network_.get_address(destination);
  if(source_address == J1939_IDLE_ADDR)
  {
    throw std::invalid_argument("Destination has no address");
  }

  j1939_frame.header.source_adderess(source_address);
  j1939_frame.header.pdu_specific(destination_address);

  return SendRaw(j1939_frame);
}

void J1939Connection::OnError(char const* what, boost::system::error_code ec)
{
  // Don't report on canceled operations
  if(ec == boost::asio::error::operation_aborted)
  {
    return;
  }

  callbacks_.on_error(what, ec);
}

void J1939Connection::Read()
{
  socket_.async_receive(canary::net::buffer(&buffer_, sizeof(buffer_)),
    [self{shared_from_this()}](auto error, auto)
    {
      if(error) { return self->OnError("read", error); }

      //Trigger callback with frame
      self->callbacks_.on_data(self->buffer_);

      //Clear buffer
      self->buffer_.payload.fill(0);

      //Queue another read
      self->Read();
    });
}

void J1939Connection::Write()
{
  auto j1939_frame = queue_.front();

  socket_.async_send(canary::net::buffer(&j1939_frame, sizeof(j1939_frame)), 
    [self{shared_from_this()}](auto error, auto)
    {
      // Handle the error, if any
      if(error) { return self->OnError("write", error); }

      // Remove the string from the queue
      self->queue_.pop();

      // Send the next message if any
      if (!self->queue_.empty())
      {
        self->Write();
      }
    }); 
}

/*
///Add Filters
std::vector<canary::filter> filters{};
filters.reserve(3);

///TODO: Filter address claim
filters.push_back(
  canary::filter{}
          .id_mask(~0xFF)            // Only IDs 0x00-0xFF.
          .remote_transmission(true) // Only remote transmission frames.
          .extended_format(true));   // Only extended format frames.

///TODO: Filter address requests
filters.push_back(
  canary::filter{}
          .id_mask(~0xFF)            // Only IDs 0x00-0xFF.
          .remote_transmission(true) // Only remote transmission frames.
          .extended_format(true));   // Only extended format frames.

/// TODO: More filters?
filters.push_back(
  canary::filter{}
          .id_mask(~0xFF)            // Only IDs 0x00-0xFF.
          .remote_transmission(true) // Only remote transmission frames.
          .extended_format(true));   // Only extended format frames.
*/