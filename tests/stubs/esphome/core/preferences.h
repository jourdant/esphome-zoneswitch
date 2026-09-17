#pragma once
#include <cstdint>
namespace esphome {
struct ESPPreferenceObject {
  template <class T>
  bool load(T*) {
    return false;
  }
  template <class T>
  bool save(T*) {
    return true;
  }
};
struct Preferences {
  template <class T>
  ESPPreferenceObject make_preference(uint32_t, bool) {
    return {};
  }
  void sync() {}
};
extern Preferences* global_preferences;
}  // namespace esphome
