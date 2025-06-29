#include <gtest/gtest.h>

#include "jay/frame.hpp"
#include "jay/transport_protocol.hpp"

#include <boost/asio/io_context.hpp>
#include <thread>

class MockBus : public jay::bus
{
public:
  explicit MockBus(boost::asio::strand<boost::asio::any_io_executor> &strand) : jay::bus(strand) {}

  bool send(const jay::frame &fr) override
  {
    sent_frames.push_back(fr);
    return true;
  }

  jay::address_t source_address() const override { return src_sa_; }
  void source_address(jay::address_t sa) { src_sa_ = sa; }

  std::vector<jay::frame> sent_frames;

private:
  jay::address_t src_sa_{ 0xAA };
};

class FailAfterFirstBus : public MockBus
{
public:
  using MockBus::MockBus;
  bool send(const jay::frame &fr) override
  {
    ++count_;
    if (count_ > 1) return false;
    return MockBus::send(fr);
  }

private:
  int count_{ 0 };
};

TEST(Jay_TP_Test, Send_BAM_Multi_Packet_Message)
{
  boost::asio::io_context io;
  boost::asio::strand<boost::asio::any_io_executor> strand{ io.get_executor() };
  MockBus bus{ strand };
  bus.source_address(0x80);

  jay::transport_protocol tp{ bus };
  bool err = false;
  tp.set_error_handler([&](auto, auto) { err = true; });


  std::vector<std::uint8_t> data(20);
  for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<std::uint8_t>(i);

  ASSERT_TRUE(tp.send(data, jay::J1939_NO_ADDR, 0x1234));

  ASSERT_GE(bus.sent_frames.size(), 2u);
  ASSERT_EQ(bus.sent_frames.front().header.pgn(), jay::PGN_TP_CM);
  for (std::size_t i = 1; i < bus.sent_frames.size(); ++i) {
    EXPECT_EQ(bus.sent_frames[i].header.pgn(), jay::PGN_TP_DT);
  }
}

TEST(Jay_TP_Test, Rx_Timeout_Sends_Abort)
{
  boost::asio::io_context io;
  boost::asio::strand<boost::asio::any_io_executor> strand{ io.get_executor() };
  MockBus bus{ strand };

  jay::transport_protocol tp{ bus };
  bool err = false;
  tp.set_error_handler([&](auto, auto) { err = true; });
  bus.source_address(0x01);

  // incoming RTS addressed to us
  jay::frame rts{};
  rts.header.pgn(jay::PGN_TP_CM);
  rts.header.pdu_specific(bus.source_address());
  rts.header.source_address(0x90);
  rts.header.payload_length(8);
  rts.payload = { jay::to_byte(jay::Control::RTS), 8, 0, 2, 1, 0x34, 0x12, 0x00 };

  tp.on_can_frame(rts);

  ASSERT_EQ(bus.sent_frames.size(), 1u);// CTS response

  std::this_thread::sleep_for(jay::T2 + std::chrono::milliseconds(50));
  tp.tick();

  ASSERT_EQ(bus.sent_frames.size(), 2u);// abort sent
  EXPECT_EQ(static_cast<jay::Control>(bus.sent_frames.back().payload[0]), jay::Control::ABORT);
  EXPECT_EQ(bus.sent_frames.back().payload[1], static_cast<std::uint8_t>(jay::AbortCode::Timeout));
  EXPECT_TRUE(err);
}

TEST(Jay_TP_Test, Tx_CM_Remote_Abort_Stops_Send)
{
  boost::asio::io_context io;
  boost::asio::strand<boost::asio::any_io_executor> strand{ io.get_executor() };
  MockBus bus{ strand };
  bus.source_address(0x01);

  jay::transport_protocol tp{ bus };
  bool err = false;
  tp.set_error_handler([&](auto, auto) { err = true; });

  std::vector<std::uint8_t> data(20, 0x55);
  ASSERT_TRUE(tp.send(data, 0x90, 0x1234));
  ASSERT_EQ(bus.sent_frames.size(), 1u);// RTS

  jay::frame abort{};
  abort.header.pgn(jay::PGN_TP_CM);
  abort.header.pdu_specific(bus.source_address());
  abort.header.source_address(0x90);
  abort.header.payload_length(8);
  abort.payload = { jay::to_byte(jay::Control::ABORT), 0, 0, 0, 0, 0x34, 0x12, 0x00 };

  tp.on_can_frame(abort);
  tp.tick();

  EXPECT_EQ(bus.sent_frames.size(), 1u);// no data sent
  EXPECT_TRUE(err);
}

TEST(Jay_TP_Test, Tx_CM_Timeout_No_CTS)
{
  boost::asio::io_context io;
  boost::asio::strand<boost::asio::any_io_executor> strand{ io.get_executor() };
  MockBus bus{ strand };
  bus.source_address(0x01);

  jay::transport_protocol tp{ bus };
  bool err = false;
  tp.set_error_handler([&](auto, auto) { err = true; });

  std::vector<std::uint8_t> data(20, 0x55);
  ASSERT_TRUE(tp.send(data, 0x90, 0x1234));
  ASSERT_EQ(bus.sent_frames.size(), 1u);// RTS

  std::this_thread::sleep_for(jay::T3 + std::chrono::milliseconds(50));
  tp.tick();

  EXPECT_EQ(bus.sent_frames.size(), 2u);// abort sent on timeout
  EXPECT_EQ(static_cast<jay::Control>(bus.sent_frames.back().payload[0]), jay::Control::ABORT);
  EXPECT_EQ(bus.sent_frames.back().payload[1], static_cast<std::uint8_t>(jay::AbortCode::Timeout));
  EXPECT_TRUE(err);
}

TEST(Jay_TP_Test, Tx_BAM_Send_Error_Returns_False)
{
  boost::asio::io_context io;
  boost::asio::strand<boost::asio::any_io_executor> strand{ io.get_executor() };
  FailAfterFirstBus bus{ strand };
  bus.source_address(0x01);

  jay::transport_protocol tp{ bus };
  bool err = false;
  tp.set_error_handler([&](auto, auto) { err = true; });

  std::vector<std::uint8_t> data(20, 0x77);
  EXPECT_FALSE(tp.send(data, jay::J1939_NO_ADDR, 0x1234));
  EXPECT_TRUE(err);
}
