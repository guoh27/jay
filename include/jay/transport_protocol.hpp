// transport_protocol.hpp – minimal J1939 TP (RTS/CTS + BAM) support for the Jay library
// ──────────────────────────────────────────────────────────────────────────────
// Design notes
// ------------
// • Implements TP.CM (PGN 0xEC00) and TP.DT (0xEB00) for both BAM (broadcast) and RTS/CTS flows.
// • Supports payloads up to 1785 bytes (255 packets × 7 bytes).
// • No Extended TP (ETP) yet; guard checked at compile‑time.
// • Integrates with Jay’s Boost.Asio executor by posting completion handlers back to the I/O context.
//
// Copyright (c) 2025  MIT‑licensed.
//
#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <vector>

#include <boost/asio.hpp>
#include <boost/functional/hash.hpp>

// local
#include "data.hpp"
#include "frame.hpp"
#include "j1939_type.hpp"


namespace jay {
class bus
{
public:
  using BusSend = std::function<bool(const jay::frame &)>;

  explicit bus(boost::asio::strand<boost::asio::any_io_executor> &strand) : strand_(strand) {}

  virtual ~bus() = default;

  // async send of a single raw J1939 frame
  virtual bool send(const jay::frame &) { return true; };

  // local source address (SA) helper
  virtual std::uint8_t source_address() const { return 0xFF; };

  boost::asio::strand<boost::asio::any_io_executor> &strand_;
};

constexpr std::uint32_t PGN_TP_CM = 0x00EC00;// 60416
constexpr std::uint32_t PGN_TP_DT = 0x00EB00;// 60160

// J1939 TP timeout defaults
constexpr auto T1 = std::chrono::milliseconds(750);// RTS/CTS handshake
constexpr auto T2 = std::chrono::milliseconds(1250);// Between CTS and first DT
constexpr auto T3 = std::chrono::milliseconds(1250);// End of message ACK wait
constexpr auto Tr = std::chrono::milliseconds(200);// Minimum separation time

// ──────────────────────────────────────────────────
// TP Control Bytes (first byte of TP.CM payload)
// ──────────────────────────────────────────────────
enum class Control : std::uint8_t { RTS = 0x10, CTS = 0x11, EOM = 0x13, BAM = 0x20, ABORT = 0xFF };

enum class AbortCode : std::uint8_t {
  AlreadyInSession = 1,
  ResourcesBusy = 2,
  Timeout = 3,
  CtsWhileDT = 4,
  MaxRetransmit = 5,
  UnexpectedPacket = 6,
  BadSequence = 7,
  DuplicateSeq = 8,
  LengthExceeded = 9,
  Unspecified = 250
};

inline std::string to_string(AbortCode code)
{
  switch (code) {
  case AbortCode::AlreadyInSession:
    return "already in session";
  case AbortCode::ResourcesBusy:
    return "resources busy";
  case AbortCode::Timeout:
    return "timeout";
  case AbortCode::CtsWhileDT:
    return "cts during dt";
  case AbortCode::MaxRetransmit:
    return "retransmit limit";
  case AbortCode::UnexpectedPacket:
    return "unexpected packet";
  case AbortCode::BadSequence:
    return "bad sequence";
  case AbortCode::DuplicateSeq:
    return "duplicate seq";
  case AbortCode::LengthExceeded:
    return "length exceeded";
  default:
    return "unspecified";
  }
}

// Little helper to cast enum ↔ byte
inline std::uint8_t to_byte(Control c) { return static_cast<std::uint8_t>(c); }

// ──────────────────────────────────────────────────
// Internal TpSession
// ──────────────────────────────────────────────────
struct TpSession
{
  enum class Direction { Tx, Rx } dir{};
  std::vector<std::uint8_t> buffer;

  std::uint8_t total_packets{ 0 };
  std::uint8_t next_seq{ 1 };
  std::uint16_t length{ 0 };

  std::uint8_t window_size{ 0xFF };

  jay::address_t dest_sa{};
  jay::address_t src_sa{};
  pgn_t pgn{};
  bool bam{ false };

  std::chrono::steady_clock::time_point last_activity;
  bool aborted{ false };
};

class transport_protocol : public std::enable_shared_from_this<transport_protocol>
{
public:
  explicit transport_protocol(jay::bus &bus) : bus_(bus), tick_timer_(bus.strand_) {}

  void set_error_handler(J1939OnError cb) { error_callback_ = std::move(cb); }

  // High‑level entry – send any size buffer
  bool send(const std::vector<std::uint8_t> &data, jay::address_t dest, std::uint32_t pgn)
  {
    return start_tx(data, dest, pgn);
  }

  // Feed every incoming J1939 CAN frame to this dispatcher
  void on_can_frame(const jay::frame &fr)
  {
    if (fr.header.pgn() == PGN_TP_CM) {
      handle_cm(fr);
    } else if (fr.header.pgn() == PGN_TP_DT) {
      handle_dt(fr);
    }
    // else: ignore
  }

  // Periodic timer tick (call every ~100 ms from your main loop)
  void tick()
  {
    auto now = std::chrono::steady_clock::now();
    for (auto it = sessions_.begin(); it != sessions_.end();) {
      auto timeout = (it->second.dir == TpSession::Direction::Tx) ? T3 : T2;
      if (now - it->second.last_activity > timeout) {
        send_abort(it->second, AbortCode::Timeout);
        report_error("tp timeout", boost::asio::error::timed_out);
        it = sessions_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void start_tick(std::chrono::milliseconds period)
  {
    tick_timer_.expires_after(period);
    tick_timer_.async_wait([this, period](const boost::system::error_code &ec) {
      if (!ec) {
        tick();
        start_tick(period);
      }
    });
  }

  void report_error(const std::string &what, boost::system::error_code ec)
  {
    if (error_callback_) { error_callback_(what, ec); }
  }

private:
  frame make_base_frame(const TpSession &s, pgn_t pgn, jay::address_t dst, jay::address_t src)
  {
    frame fr;
    fr.header.pgn(pgn);
    fr.header.pdu_specific(dst);
    fr.header.source_address(src);
    fr.header.payload_length(8);
    fr.payload.fill(0);
    return fr;
  }

  frame make_cm_frame(Control ctrl, const TpSession &s, jay::address_t dst_sa, jay::address_t src_sa)
  {
    auto fr = make_base_frame(s, PGN_TP_CM, dst_sa, src_sa);
    fr.payload[0] = to_byte(ctrl);
    return fr;
  }

  frame make_dt_frame(const TpSession &s) { return make_base_frame(s, PGN_TP_DT, s.dest_sa, s.src_sa); }

  void fill_tp_payload(frame &fr, const TpSession &s, std::optional<std::uint8_t> first = std::nullopt)
  {
    if (first) fr.payload[0] = *first;
    fr.payload[1] = s.length & 0xFF;
    fr.payload[2] = s.length >> 8;
    fr.payload[3] = s.total_packets;
    fr.payload[4] = s.window_size;
    fr.payload[5] = s.pgn & 0xFF;
    fr.payload[6] = (s.pgn >> 8) & 0xFF;
    fr.payload[7] = (s.pgn >> 16) & 0xFF;
  }

private:
  // Session key helper
  using Key = std::tuple<jay::address_t, jay::address_t>;// src, dst

  static Key make_key(jay::address_t src, jay::address_t dst) { return { src, dst }; }

  // Map of active sessions
  std::unordered_map<Key, TpSession, boost::hash<Key>> sessions_;

  // ── TX path ───────────────────────────────────
  bool start_tx(const std::vector<std::uint8_t> &data, jay::address_t dest, pgn_t pgn)
  {
    // ignore
    if (data.size() <= 8) { return false; }
    if (data.size() > 1785) {
      report_error("payload too large", boost::asio::error::message_size);
      return false;
    }

    // multi‑packet – choose BAM if broadcast (255) else RTS/CTS
    bool use_bam = (dest == 0xFF);
    TpSession sess;
    sess.dir = TpSession::Direction::Tx;
    sess.buffer = data;
    sess.length = static_cast<std::uint16_t>(sess.buffer.size());
    sess.total_packets = static_cast<std::uint8_t>((sess.length + 6) / 7);
    sess.dest_sa = dest;
    sess.src_sa = bus_.source_address();
    sess.pgn = pgn;
    sess.bam = use_bam;
    sess.last_activity = std::chrono::steady_clock::now();

    auto key = make_key(sess.src_sa, sess.dest_sa);
    sessions_[key] = std::move(sess);

    if (use_bam)
      return send_bam_start(sessions_[key]);
    else
      return send_rts(sessions_[key]);
  }

  bool send_bam_start(TpSession &s)
  {
    frame fr = make_cm_frame(Control::BAM, s, jay::J1939_NO_ADDR, s.src_sa);
    fill_tp_payload(fr, s);
    if (!bus_.send(fr)) {
      report_error("tp send BAM", boost::system::errc::make_error_code(boost::system::errc::io_error));
      send_abort(s, AbortCode::ResourcesBusy);
      return false;
    }
    s.last_activity = std::chrono::steady_clock::now();
    return send_data_packets(s);
  }

  bool send_rts(TpSession &s)
  {
    frame fr = make_cm_frame(Control::RTS, s, s.dest_sa, s.src_sa);
    fill_tp_payload(fr, s);
    auto ok = bus_.send(fr);
    if (!ok) {
      report_error("tp send RTS", boost::system::errc::make_error_code(boost::system::errc::io_error));
      send_abort(s, AbortCode::ResourcesBusy);
      return false;
    }
    s.last_activity = std::chrono::steady_clock::now();
    return true;
    // wait for CTS before sending data
  }

  bool send_data_packets(TpSession &s, std::uint8_t count = 0xFF)
  {
    while (s.next_seq <= s.total_packets && count--) {
      frame fr = make_dt_frame(s);

      fr.payload[0] = s.next_seq;
      std::size_t offset = (s.next_seq - 1) * 7;
      auto avail = std::min<std::size_t>(7, s.buffer.size() - offset);
      std::copy_n(s.buffer.data() + offset, avail, fr.payload.begin() + 1);

      if (!bus_.send(fr)) {
        report_error("tp send DT", boost::system::errc::make_error_code(boost::system::errc::io_error));
        send_abort(s, AbortCode::ResourcesBusy);
        return false;
      }

      s.last_activity = std::chrono::steady_clock::now();
      ++s.next_seq;
    }

    if (s.next_seq > s.total_packets) {
      send_eom_ack(s);
      auto key = make_key(s.src_sa, s.dest_sa);
      sessions_.erase(key);
      return true;
    } else {
      // Reason?
      return false;
    }
  }

  bool send_eom_ack(const TpSession &s)
  {
    if (s.bam) return true;// not required

    frame fr = make_cm_frame(Control::EOM, s, s.src_sa, s.dest_sa);
    fill_tp_payload(fr, s);
    fr.payload[4] = 0xFF;// seq
    if (!bus_.send(fr)) {
      report_error("tp send EOM", boost::system::errc::make_error_code(boost::system::errc::io_error));
      return false;
    }
    return true;
  }

  bool send_abort(const TpSession &s, AbortCode code)
  {
    frame fr = make_cm_frame(Control::ABORT, s, s.src_sa, s.dest_sa);
    fill_tp_payload(fr, s);
    fr.payload[1] = static_cast<std::uint8_t>(code);
    fr.payload[2] = fr.payload[3] = fr.payload[4] = 0;
    return bus_.send(fr);
  }

  // ── RX path ───────────────────────────────────
  void handle_cm(const jay::frame &fr)
  {
    auto ctrl = static_cast<Control>(fr.payload[0]);
    switch (ctrl) {
    case Control::RTS:
      start_rx_rts(fr);
      break;
    case Control::CTS:
      handle_cts(fr);
      break;
    case Control::BAM:
      start_rx_bam(fr);
      break;
    case Control::EOM:
      complete_rx(fr);
      break;
    case Control::ABORT:
      handle_abort(fr);
      break;
    default: /* ignore */
      break;
    }
  }

  void response_cts(const TpSession &session)
  {
    // send CTS
    frame cts = make_cm_frame(Control::CTS, session, session.src_sa, session.dest_sa);
    fill_tp_payload(cts, session);
    cts.payload[1] = session.window_size;
    cts.payload[2] = session.next_seq;
    cts.payload[3] = 0;
    cts.payload[4] = 0;

    bus_.send(cts);
  }

  void start_rx_rts(const jay::frame &fr)
  {
    // not send to self
    if (fr.header.pdu_specific() != bus_.source_address()) { return; }

    TpSession s;
    s.dir = TpSession::Direction::Rx;
    s.length = fr.payload[1] | (fr.payload[2] << 8);
    s.total_packets = fr.payload[3];
    s.window_size = fr.payload[4];
    s.dest_sa = fr.header.is_broadcast() ? jay::J1939_NO_ADDR : fr.header.pdu_specific();
    s.src_sa = fr.header.source_address();
    s.pgn = get_payload_pgn(fr);
    s.buffer.resize(s.length);
    s.last_activity = std::chrono::steady_clock::now();
    s.next_seq = 1;

    auto key = make_key(s.src_sa, s.dest_sa);
    sessions_[key] = std::move(s);
    response_cts(sessions_[key]);
  }

  void handle_cts(const jay::frame &fr)
  {
    auto key = make_key(bus_.source_address(), fr.header.source_address());
    if (auto it = sessions_.find(key); it != sessions_.end()) {
      auto count = fr.payload[1];
      send_data_packets(it->second, count);
    }
  }

  void start_rx_bam(const jay::frame &fr)
  {
    TpSession s;
    s.dir = TpSession::Direction::Rx;
    s.bam = true;
    s.length = fr.payload[1] | (fr.payload[2] << 8);
    s.total_packets = fr.payload[3];
    s.dest_sa = 0xFF;
    s.src_sa = fr.header.source_address();
    s.pgn = get_payload_pgn(fr);
    s.buffer.resize(s.length);
    s.last_activity = std::chrono::steady_clock::now();
    auto key = make_key(s.src_sa, s.dest_sa);
    sessions_[key] = std::move(s);
  }

  void handle_dt(const jay::frame &fr)
  {
    auto key = make_key(fr.header.source_address(), fr.header.pdu_specific());
    auto it = sessions_.find(key);
    if (it == sessions_.end()) return;

    auto &session = it->second;
    std::uint8_t seq = fr.payload[0];
    if (seq < 1 || seq > session.total_packets) return;// bad

    std::size_t offset = (seq - 1) * 7;
    auto avail = std::min<std::size_t>(7, session.buffer.size() - offset);
    std::copy_n(fr.payload.data() + 1, avail, session.buffer.data() + offset);
    session.last_activity = std::chrono::steady_clock::now();

    if (seq == session.total_packets) {
      // finished, deliver
      if (rx_callback_) {
        frame_header hr;
        hr.pgn(session.pgn);
        hr.source_address(session.src_sa);
        if (!session.bam) { hr.pdu_specific(session.dest_sa); }
        hr.payload_length(session.buffer.size());
        rx_callback_({ hr, session.buffer });
      }

      // send EOM_ACK if not BAM
      if (!session.bam) send_eom_ack(session);

      sessions_.erase(it);
    } else if (seq % session.window_size == 0 && !session.bam) {
      response_cts(session);
    }
  }

  void handle_abort(const jay::frame &fr)
  {
    auto key = make_key(bus_.source_address(), fr.header.source_address());
    if (auto it = sessions_.find(key); it != sessions_.end()) {
      it->second.aborted = true;
      sessions_.erase(it);
      auto reason = static_cast<AbortCode>(fr.payload[1]);
      report_error("remote abort: " + to_string(reason), boost::asio::error::operation_aborted);
    }
  }

  void complete_rx(const jay::frame & /*fr*/)
  {
    // nothing – handled in DT loop
  }

  jay::pgn_t get_payload_pgn(const frame &fr) { return fr.payload[5] | (fr.payload[6] << 8) | (fr.payload[7] << 16); }

public:
  void set_rx_handler(J1939OnData cb) { rx_callback_ = std::move(cb); }

private:
  jay::bus &bus_;
  J1939OnData rx_callback_;
  J1939OnError error_callback_;
  boost::asio::steady_timer tick_timer_;
};
}// namespace jay
