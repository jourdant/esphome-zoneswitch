#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "protocol_v1.h"
#include "protocol_v2.h"
using namespace esphome;
using namespace esphome::zoneswitch;
namespace esphome {
uint32_t test_now = 100;
Preferences preferences;
Preferences* global_preferences = &preferences;
}  // namespace esphome
struct Bus : V1ZoneSwitch {
  std::vector<uint8_t> masks;
  bool transport_ok = true;
  V1TransportResult transact_v1_(uint8_t m) override {
    masks.push_back(m);
    return transport_ok ? V1TransportResult::SENT : V1TransportResult::ACK_TIMEOUT;
  }
  void feed(std::initializer_list<uint8_t> b) {
    rx.insert(rx.end(), b);
    loop();
  }
  void advance(uint32_t n = 1100) {
    test_now += n;
    loop();
  }
  void v1() {
    set_enable_polling(false);
    set_status_timeout(0);
  }
  bool pending() { return waiting_for_response_; }
  uint32_t bad() { return rx_bad_count_; }
  uint32_t ok() { return rx_ok_count_; }
};
struct V2Bus : V2ZoneSwitch {
  bool frame(const uint8_t* f) { return handle_frame_(f); }
  static uint8_t crc(const uint8_t* f, size_t n) { return crc8_maxim_(f, n); }
  void advance(uint32_t n = 1100) {
    test_now += n;
    loop();
  }
  bool pending() { return waiting_for_response_; }
  bool locked() { return node_locked_; }
  uint32_t ok() { return rx_ok_count_; }
  uint32_t bad() { return rx_bad_count_; }
};
static const char* current_test = "";
#define CHECK(expr)                                                                  \
  do {                                                                               \
    if (!(expr)) {                                                                   \
      std::cerr << current_test << ": " << #expr << " at line " << __LINE__ << "\n"; \
      std::exit(1);                                                                  \
    }                                                                                \
  } while (0)
void test_v1_regression() {
  // Every installed and uninstalled position, ON/OFF, fragmented and prefixed.
  for (unsigned z = 0; z < 6; z++) {
    Bus b;
    b.v1();
    b.feed({0xC0, 0x30, static_cast<uint8_t>(1 << z)});
    b.feed({8});
    for (unsigned i = 0; i < 6; i++) b.rx.push_back(i | (i == z ? 0x80 : 0));
    b.rx.push_back(0x97);
    b.loop();
    CHECK(b.get_last_mask() == (1 << z));
    CHECK(b.is_online());
    CHECK(b.bad() == 0);
    b.feed({8, 0, 1, 2, 3, 4, 5, 0x17});
    CHECK(b.get_last_mask() == 0);
  }
  Bus b;
  b.v1();
  b.feed({8, 0, 0x81, 2, 3, 0x84, 5, 0x17});
  CHECK(b.get_last_mask() == 0x12);
  b.feed({8, 0, 1, 2, 3, 4, 5, 0});
  CHECK(b.get_last_mask() == 0x12);
  CHECK(b.bad() == 1);
  b.feed({8, 1, 1, 2, 3, 4, 5, 0x18});
  CHECK(b.get_last_mask() == 0x12);
  // A truncated frame expires and a following valid frame resynchronises.
  b.feed({8, 0, 1});
  b.advance(30);
  b.feed({8, 0, 1, 2, 3, 4, 5, 0x17});
  CHECK(b.get_last_mask() == 0);
  // Absolute OFF becomes query then ONE toggle, confirmed state is not optimistic.
  b.request_zone_state(2, false);
  b.advance();
  CHECK(b.masks.back() == 0);
  b.feed({8, 0, 0x81, 2, 3, 0x84, 5, 0x17});
  b.advance();
  CHECK(b.masks.back() == 2);
  CHECK(b.get_last_mask() == 0x12);
  b.feed({8, 0, 1, 2, 3, 0x84, 5, 0x97});
  CHECK(b.get_last_mask() == 0x10);
  size_t n = b.masks.size();
  b.advance(60000);
  CHECK(b.masks.size() == n);
  // Repeating OFF queries but does not toggle an already-off zone.
  b.request_zone_state(2, false);
  b.advance();
  b.feed({8, 0, 1, 2, 3, 0x84, 5, 0x97});
  b.advance();
  CHECK(b.masks.size() == n + 1);
  CHECK(b.masks.back() == 0);
  // Missing write reply abandons intent, even after a later unsolicited status.
  b.request_zone_state(2, true);
  b.advance();
  b.feed({8, 0, 1, 2, 3, 0x84, 5, 0x97});
  b.advance();
  CHECK(b.masks.back() == 2);
  n = b.masks.size();
  b.advance(300);
  CHECK(!b.is_online());
  b.feed({8, 0, 1, 2, 3, 0x84, 5, 0x97});
  b.advance(5000);
  CHECK(b.masks.size() == n);
  // Failed handshake and query timeout also cancel the write.
  b.transport_ok = false;
  b.request_zone_state(2, true);
  b.advance();
  n = b.masks.size();
  b.advance(5000);
  CHECK(b.masks.size() == n);
  b.transport_ok = true;
  b.request_zone_state(2, true);
  b.advance();
  b.advance(300);
  n = b.masks.size();
  b.feed({8, 0, 1, 2, 3, 0x84, 5, 0x97});
  b.advance();
  CHECK(b.masks.size() == n);
  // Startup queries once. Idle event-driven buses do not expire by default.
  Bus boot;
  boot.v1();
  boot.setup();
  boot.loop();
  CHECK(boot.masks.size() == 1);
  boot.feed({8, 0, 1, 2, 3, 4, 5, 0x17});
  boot.advance(60000);
  CHECK(boot.masks.size() == 1 && boot.is_online());
  // Spill guard prevents closing the known last spill zone.
  boot.set_spill_zone(2);
  boot.request_zone_state(2, false);
  boot.advance();
  boot.feed({8, 0, 0x81, 2, 3, 4, 5, 0x97});
  n = boot.masks.size();
  boot.advance();
  CHECK(boot.masks.size() == n);
}
void test_v2_regression() {
  // V2 remains the default: address confirmation and CRC validation unchanged.
  V2Bus v2;
  v2.set_enable_polling(false);
  uint8_t f[] = {0xAA, 0x48, 0, 1, 0x81, 1, 0x12, 0, 0x55};
  f[7] = V2Bus::crc(f + 1, 6);
  CHECK(v2.frame(f));
  CHECK(!v2.is_online());
  CHECK(v2.frame(f));
  CHECK(v2.frame(f));
  CHECK(v2.is_online());
  CHECK(v2.get_last_mask() == 0x12);
  f[7] ^= 1;
  CHECK(!v2.frame(f));
  CHECK(v2.get_last_mask() == 0x12);
  v2.request_zone_state(2, false);
  v2.advance();
  CHECK(v2.tx.size() == 1);
  CHECK(v2.tx[0][6] == 2);
  CHECK(v2.tx[0][7] == V2Bus::crc(v2.tx[0].data() + 1, 6));
}

void test_listen_only() {
  Bus v1;
  GPIOPin v1_de;
  v1.set_flow_control_pin(&v1_de);
  v1.v1();
  v1.set_listen_only(true);
  v1.set_enable_polling(true);  // Defence in depth even if Python validation is bypassed.
  v1.setup();
  v1.advance();
  v1.request_refresh();
  v1.request_zone_state(2, true);
  v1.advance(60000);
  v1.feed({0xC0, 0x30, 2, 8, 0, 0x81, 2, 3, 4, 5, 0x97});
  CHECK(v1.get_last_mask() == 2 && v1.is_online());
  CHECK(v1.masks.empty() && v1.get_tx_count() == 0);
  CHECK(!v1_de.level && v1_de.high_writes == 0);
  V2Bus v2;
  GPIOPin v2_de;
  v2.set_flow_control_pin(&v2_de);
  v2.set_listen_only(true);
  v2.setup();
  v2.set_node_confirmations(1);
  uint8_t f[] = {0xAA, 0x48, 0, 1, 0x81, 1, 2, 0, 0x55};
  f[7] = V2Bus::crc(f + 1, 6);
  v2.frame(f);
  v2.request_zone_state(2, false);
  v2.request_refresh();
  v2.advance(60000);
  CHECK(v2.tx.empty() && v2.get_tx_count() == 0);
  CHECK(!v2_de.level && v2_de.high_writes == 0);
  CHECK(v2.get_last_mask() == 2);
}
void test_v1_busy_and_diagnostics() {
  Bus b;
  b.v1();
  CHECK(std::isnan(b.get_status_age()));
  b.request_zone_state(2, true);
  b.request_zone_state(3, true);
  CHECK(b.get_rejected_busy() == 1);
  b.transport_ok = false;
  b.advance();
  CHECK(b.get_ack_timeouts() == 1 && b.get_tx_count() == 1);
  CHECK(std::string(b.get_transaction_result()) == "ack_timeout");
  b.transport_ok = true;
  b.request_refresh();
  b.advance();
  b.advance(300);
  CHECK(b.get_response_timeouts() == 1);
  CHECK(std::string(b.get_transaction_result()) == "response_timeout");
  b.feed({8, 0, 1, 2, 3, 4, 5, 0x17});
  b.advance(2000);
  CHECK(b.get_status_age() == 2.0f);
}
void test_v2_sequence_and_node_change() {
  V2Bus b;
  b.set_enable_polling(false);
  b.set_node_confirmations(1);
  auto status = [&](uint8_t node, uint8_t seq, uint8_t mask) {
    uint8_t f[] = {0xAA, node, 0, seq, 0x81, 1, mask, 0, 0x55};
    f[7] = V2Bus::crc(f + 1, 6);
    b.frame(f);
  };
  status(0x48, 99, 2);
  b.request_zone_state(2, false);
  b.advance();
  CHECK(b.pending());
  status(0x48, 99, 0);  // Valid passive state but not our write acknowledgement.
  CHECK(b.pending() && b.get_last_mask() == 0);
  b.advance(5000);
  CHECK(!b.pending() && b.get_response_timeouts() == 1);
  CHECK(b.tx.size() == 1);  // No blind toggle retry.
  b.set_node_mismatch_threshold(2);
  status(0xB4, 20, 4);
  CHECK(b.locked());
  status(0xB4, 21, 4);
  CHECK(!b.locked() && !b.is_online());
  status(0xB4, 22, 4);
  CHECK(b.locked() && b.get_node_addr() == 0xB4);
  CHECK(b.get_last_mask() == 4);
}
void test_timeout_wrap_and_interruption() {
  test_now = 0xFFFFFF00;
  Bus b;
  b.v1();
  b.feed({8, 0, 1, 2, 3, 4, 5, 0x17});
  b.request_zone_state(2, true);
  b.advance(25);
  CHECK(b.pending());
  b.advance(300);  // Cross uint32 clock wrap while waiting for a response.
  CHECK(!b.pending() && b.get_response_timeouts() == 1);
  CHECK(b.get_status_age() > 0.3f);
  // A new boot has no previous pending write. It only queries once.
  Bus reboot;
  reboot.v1();
  reboot.setup();
  reboot.advance();
  CHECK(reboot.masks.size() == 1 && reboot.masks[0] == 0);
  reboot.feed({8, 0, 1, 2, 3, 4, 5, 0x17});
  reboot.advance(5000);
  CHECK(reboot.masks.size() == 1);
  reboot.set_status_timeout(100);
  reboot.request_zone_state(2, true);
  reboot.advance(150);
  CHECK(!reboot.is_online());
  CHECK(std::string(reboot.get_transaction_result()) == "status_timeout");
  CHECK(reboot.masks.size() == 1);  // Expired state cancels the pending intent.
}
void test_capture_fixtures() {
  std::ifstream input("tests/fixtures/captures.tsv");
  CHECK(input.good());
  std::string line;
  std::getline(input, line);  // Header.
  size_t count = 0;
  while (std::getline(input, line)) {
    std::istringstream row(line);
    std::string id, protocol, bytes, mask, source, timestamp;
    std::getline(row, id, '\t');
    std::getline(row, protocol, '\t');
    std::getline(row, bytes, '\t');
    std::getline(row, mask, '\t');
    std::getline(row, source, '\t');
    std::getline(row, timestamp, '\t');
    std::vector<uint8_t> data;
    std::istringstream hex(bytes);
    unsigned value;
    while (hex >> std::hex >> value) data.push_back(value);
    current_test = id.c_str();
    if (protocol == "v1") {
      Bus b;
      b.v1();
      b.set_listen_only(true);
      for (auto byte : data) b.feed({byte});  // Also exercises byte-wise fragmentation.
      CHECK(b.is_online() && b.get_last_mask() == std::stoul(mask, nullptr, 16));
      CHECK(b.bad() == 0 && b.ok() == 1);
    } else {
      V2Bus b;
      b.set_listen_only(true);
      b.set_node_confirmations(1);
      b.rx.insert(b.rx.end(), data.begin(), data.end());
      b.loop();
      CHECK(b.is_online() && b.get_last_mask() == std::stoul(mask, nullptr, 16));
      CHECK(b.bad() == 0 && b.ok() == 2);
    }
    count++;
  }
  CHECK(count >= 15);
}
int main() {
  struct Test {
    const char* name;
    void (*run)();
  };
  for (auto test :
       {Test{"v1_states_and_transactions", test_v1_regression}, Test{"v2_crc_and_control", test_v2_regression},
        Test{"listen_only", test_listen_only}, Test{"v1_busy_and_diagnostics", test_v1_busy_and_diagnostics},
        Test{"v2_sequence_and_node_change", test_v2_sequence_and_node_change},
        Test{"timeout_wrap_and_interruption", test_timeout_wrap_and_interruption},
        Test{"capture_fixtures", test_capture_fixtures}}) {
    current_test = test.name;
    test_now = 100;
    test.run();
    std::cout << "PASS " << test.name << "\n";
  }
}
