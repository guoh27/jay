#include <gtest/gtest.h>

#include "jay/frame.hpp"
#include "jay/transport_protocol.hpp"

#include <boost/asio/io_context.hpp>

class MockBus : public jay::bus
{
public:
  explicit MockBus(boost::asio::strand<boost::asio::any_io_executor> &strand) : jay::bus(strand) {}

  bool send(const jay::frame &fr) override
  {
    sent_frames.push_back(fr);
    return true;
  }

  std::uint8_t source_address() const override { return src_sa_; }
  void source_address(std::uint8_t sa) { src_sa_ = sa; }

  std::vector<jay::frame> sent_frames;

private:
  std::uint8_t src_sa_{ 0xAA };
};

TEST(Jay_TP_Test, Send_BAM_Multi_Packet_Message)
{
  boost::asio::io_context io;
  boost::asio::strand<boost::asio::any_io_executor> strand{ io.get_executor() };
  MockBus bus{ strand };
  bus.source_address(0x80);

  jay::transport_protocol tp{ bus };

  std::vector<std::uint8_t> data(20);
  for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<std::uint8_t>(i);

  ASSERT_TRUE(tp.send(data, jay::J1939_NO_ADDR, 0x1234));

  ASSERT_GE(bus.sent_frames.size(), 2u);
  ASSERT_EQ(bus.sent_frames.front().header.pgn(), jay::PGN_TP_CM);
  for (std::size_t i = 1; i < bus.sent_frames.size(); ++i) {
    EXPECT_EQ(bus.sent_frames[i].header.pgn(), jay::PGN_TP_DT);
  }
}
