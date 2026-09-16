#include <cassert>
#include <iostream>
#include <vector>

#include "zoneswitch.h"
using namespace esphome;
using namespace esphome::zoneswitch;
namespace esphome {
uint32_t test_now = 100;
Preferences preferences;
Preferences* global_preferences = &preferences;
}  // namespace esphome
struct Bus : ZoneSwitch {
  std::vector<uint8_t> masks;
  bool transport_ok = true;
  bool transact_v1_(uint8_t m) override {
    masks.push_back(m);
    return transport_ok;
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
    set_protocol(Protocol::V1);
    set_enable_polling(false);
    set_status_timeout(0);
  }
  bool pending() { return waiting_for_response_; }
  uint32_t bad() { return rx_bad_count_; }
  bool frame(const uint8_t* f) { return handle_frame_(f); }
  static uint8_t crc(const uint8_t* f, size_t n) { return crc8_maxim_(f, n); }
};
int main() {
  // Every installed and uninstalled position, ON/OFF, fragmented and prefixed.
  for (unsigned z = 0; z < 6; z++) {
    Bus b;
    b.v1();
    b.feed({0xC0, 0x30, static_cast<uint8_t>(1 << z)});
    b.feed({8});
    for (unsigned i = 0; i < 6; i++) b.rx.push_back(i | (i == z ? 0x80 : 0));
    b.rx.push_back(0x97);
    b.loop();
    assert(b.get_last_mask() == (1 << z));
    assert(b.is_online());
    assert(b.bad() == 0);
    b.feed({8, 0, 1, 2, 3, 4, 5, 0x17});
    assert(b.get_last_mask() == 0);
  }
  Bus b;
  b.v1();
  b.feed({8, 0, 0x81, 2, 3, 0x84, 5, 0x17});
  assert(b.get_last_mask() == 0x12);
  b.feed({8, 0, 1, 2, 3, 4, 5, 0});
  assert(b.get_last_mask() == 0x12);
  assert(b.bad() == 1);
  b.feed({8, 1, 1, 2, 3, 4, 5, 0x18});
  assert(b.get_last_mask() == 0x12);
  // A truncated frame expires and a following valid frame resynchronises.
  b.feed({8, 0, 1});
  b.advance(30);
  b.feed({8, 0, 1, 2, 3, 4, 5, 0x17});
  assert(b.get_last_mask() == 0);
  // Absolute OFF becomes query then ONE toggle, confirmed state is not optimistic.
  b.request_zone_state(2, false);
  b.advance();
  assert(b.masks.back() == 0);
  b.feed({8, 0, 0x81, 2, 3, 0x84, 5, 0x17});
  b.advance();
  assert(b.masks.back() == 2);
  assert(b.get_last_mask() == 0x12);
  b.feed({8, 0, 1, 2, 3, 0x84, 5, 0x97});
  assert(b.get_last_mask() == 0x10);
  size_t n = b.masks.size();
  b.advance(60000);
  assert(b.masks.size() == n);
  // Repeating OFF queries but does not toggle an already-off zone.
  b.request_zone_state(2, false);
  b.advance();
  b.feed({8, 0, 1, 2, 3, 0x84, 5, 0x97});
  b.advance();
  assert(b.masks.size() == n + 1);
  assert(b.masks.back() == 0);
  // Missing write reply abandons intent, even after a later unsolicited status.
  b.request_zone_state(2, true);
  b.advance();
  b.feed({8, 0, 1, 2, 3, 0x84, 5, 0x97});
  b.advance();
  assert(b.masks.back() == 2);
  n = b.masks.size();
  b.advance(300);
  assert(!b.is_online());
  b.feed({8, 0, 1, 2, 3, 0x84, 5, 0x97});
  b.advance(5000);
  assert(b.masks.size() == n);
  // Failed handshake and query timeout also cancel the write.
  b.transport_ok = false;
  b.request_zone_state(2, true);
  b.advance();
  n = b.masks.size();
  b.advance(5000);
  assert(b.masks.size() == n);
  b.transport_ok = true;
  b.request_zone_state(2, true);
  b.advance();
  b.advance(300);
  n = b.masks.size();
  b.feed({8, 0, 1, 2, 3, 0x84, 5, 0x97});
  b.advance();
  assert(b.masks.size() == n);
  // Startup queries once. Idle event-driven buses do not expire by default.
  Bus boot;
  boot.v1();
  boot.setup();
  boot.loop();
  assert(boot.masks.size() == 1);
  boot.feed({8, 0, 1, 2, 3, 4, 5, 0x17});
  boot.advance(60000);
  assert(boot.masks.size() == 1 && boot.is_online());
  // Spill guard prevents closing the known last spill zone.
  boot.set_spill_zone(2);
  boot.request_zone_state(2, false);
  boot.advance();
  boot.feed({8, 0, 0x81, 2, 3, 4, 5, 0x97});
  n = boot.masks.size();
  boot.advance();
  assert(boot.masks.size() == n);
  // V2 remains the default: address confirmation and CRC validation unchanged.
  Bus v2;
  v2.set_enable_polling(false);
  uint8_t f[] = {0xAA, 0x48, 0, 1, 0x81, 1, 0x12, 0, 0x55};
  f[7] = Bus::crc(f + 1, 6);
  assert(v2.frame(f));
  assert(!v2.is_online());
  assert(v2.frame(f));
  assert(v2.frame(f));
  assert(v2.is_online());
  assert(v2.get_last_mask() == 0x12);
  f[7] ^= 1;
  assert(!v2.frame(f));
  assert(v2.get_last_mask() == 0x12);
  v2.request_zone_state(2, false);
  v2.advance();
  assert(v2.tx.size() == 1);
  assert(v2.tx[0][6] == 2);
  assert(v2.tx[0][7] == Bus::crc(v2.tx[0].data() + 1, 6));
  std::cout << "Protocol and transaction regression tests passed\n";
}
