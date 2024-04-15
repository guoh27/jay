#include "j1939_connection.hpp"

// Linux
#include <linux/can.h>

// Libraries
#include "canary/interface_index.hpp"
#include "canary/socket_options.hpp"

J1939Connection::J1939Connection(boost::asio::io_context &io_context, const jay::network &network)
  : socket_(boost::asio::make_strand(io_context)), network_(network)
{}

J1939Connection::J1939Connection(boost::asio::io_context &io_context,
  const jay::network &network,
  Callbacks &&callbacks)
  : socket_(boost::asio::make_strand(io_context)), network_(network), callbacks_(std::move(callbacks))
{}

J1939Connection::J1939Connection(boost::asio::io_context &io_context,
  const jay::network &network,
  Callbacks &&callbacks,
  std::optional<jay::name> local_name,
  std::optional<jay::name> target_name)
  : socket_(boost::asio::make_strand(io_context)), network_(network), callbacks_(std::move(callbacks)),
    local_name_(local_name), target_name_(target_name)
{}

J1939Connection::~J1939Connection()
{
  if (callbacks_.on_destroy) { callbacks_.on_destroy(this); }
}

bool J1939Connection::Open(const std::vector<canary::filter> &filters)
{
  try {
    auto endpoint = canary::raw::endpoint{ canary::get_interface_index(network_.get_interface_name()) };
    socket_.open(endpoint.protocol());
    socket_.bind(endpoint);
    if (filters.size() > 0) { socket_.set_option(canary::filter_if_any{ filters.data(), filters.size() }); }
  } catch (boost::system::error_code ec) {
    callbacks_.on_error("open", ec);
    return false;
  }
  return true;
}

void J1939Connection::Start()
{
  if (callbacks_.on_start) { callbacks_.on_start(this); }
  assert(callbacks_.on_error);
  assert(callbacks_.on_error);
  Read();
}

void J1939Connection::SendRaw(const jay::frame &j1939_frame)
{
  /// TODO: Post our work to the strand, this ensures
  // that the members of `this` will not be
  // accessed concurrently.
  boost::asio::post(socket_.get_executor(), [j1939_frame, self = shared_from_this()]() {
    // Always add to queue
    self->queue_.push(j1939_frame);

    // Are we already writing?
    if (self->queue_.size() > 1) { return; }

    // We are not currently writing, so send this immediately
    self->Write();
  });
}

void J1939Connection::SendBroadcast(jay::frame &j1939_frame)
{
  if (!j1939_frame.header.is_broadcast()) { throw std::invalid_argument("Not a broadcast frame"); }

  if (!local_name_.has_value()) { throw std::invalid_argument("Socket has no local name"); }

  auto source_address = network_.get_address(*local_name_);
  if (source_address == J1939_IDLE_ADDR) { throw std::invalid_argument("Socket has no source address"); }

  j1939_frame.header.source_adderess(source_address);
  return SendRaw(j1939_frame);
}

void J1939Connection::Send(jay::frame &j1939_frame)
{
  if (!target_name_.has_value()) { throw std::invalid_argument("Socket has no connection name"); }

  return SendTo(*target_name_, j1939_frame);
}


void J1939Connection::SendTo(const uint64_t destination, jay::frame &j1939_frame)
{
  if (!local_name_.has_value()) { throw std::invalid_argument("Socket has no local name"); }

  auto source_address = network_.get_address(*local_name_);
  if (source_address == J1939_IDLE_ADDR) { throw std::invalid_argument("Socket has no source address"); }

  auto destination_address = network_.get_address(destination);
  if (source_address == J1939_IDLE_ADDR) { throw std::invalid_argument("Destination has no address"); }

  j1939_frame.header.source_adderess(source_address);
  j1939_frame.header.pdu_specific(destination_address);

  return SendRaw(j1939_frame);
}

void J1939Connection::OnError(char const *what, boost::system::error_code ec)
{
  // Don't report on canceled operations
  if (ec == boost::asio::error::operation_aborted) { return; }

  callbacks_.on_error(what, ec);
}

/**
 * @todo Are we clearing the buffer
 * correctly it seems the header would remain the same
 */
void J1939Connection::Read()
{
  socket_.async_receive(canary::net::buffer(&buffer_, sizeof(buffer_)), [self{ shared_from_this() }](auto error, auto) {
    if (error) { return self->OnError("read", error); }

    // Trigger callback with frame if we are supposed to get the frame
    if (self->CheckAddress()) { self->callbacks_.on_read(self->buffer_); }

    // Clear buffer
    self->buffer_.payload.fill(0);
    self->buffer_.header.id(0);

    // Queue another read
    self->Read();
  });
}

void J1939Connection::Write()
{
  auto j1939_frame = queue_.front();

  /// TODO: Migh want to use async write as send might not send all the information
  /// though will have to see

  socket_.async_send(canary::net::buffer(&j1939_frame, sizeof(j1939_frame)),
    [j1939_frame, self{ shared_from_this() }](auto error, auto) {
      // Handle the error, if any
      if (error) { return self->OnError("write", error); }

      // Callback with data sent
      if (self->callbacks_.on_send) { self->callbacks_.on_send(j1939_frame); };

      // Remove the string from the queue
      self->queue_.pop();

      // Send the next message if any
      if (!self->queue_.empty()) { self->Write(); }
    });
}

/**
 * @note Since we are using raw can the filter cant be sure if the recieved message is for us.
 * As dynamic addressing could cause a filter to be invalid if the source address was
 * included in the filter. As such we need to check if we are allowed to recieve the frame.
 * Since the raw can filters will take care of PGN we only need to check for broadcasts,
 * source if we are using a connection, and if we are the target of the message
 * @note sould be able to eliminate this by using j1939 sockets instead!
 */
bool J1939Connection::CheckAddress() const
{
  /// If we dont have any names then accept any frame
  if (!target_name_ && !local_name_) { return true; }

  // We can accept broadcasts from target is there is one
  if (buffer_.header.is_broadcast()) {
    if (target_name_) { return network_.get_address(*target_name_) == buffer_.header.source_adderess(); }
    return true;
  }

  // If we have both target and local name then
  // check source and target address, given its not a broadcast
  if (target_name_ && local_name_) {
    return network_.get_address(*target_name_) == buffer_.header.source_adderess()
           && network_.get_address(*local_name_) == buffer_.header.pdu_specific();
  }

  /// Feel like these two last are more outliers

  // If message is for local name, but we dont care who its from
  if (!target_name_ && local_name_) { return network_.get_address(*local_name_) == buffer_.header.pdu_specific(); }

  // Check that the message is from our intended target but we dont care if its for us
  if (target_name_ && !local_name_) { return network_.get_address(*target_name_) == buffer_.header.source_adderess(); }

  return false;
}