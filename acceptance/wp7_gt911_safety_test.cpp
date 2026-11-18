#include <cstdlib>
#include <iostream>

#include "gt911_sample.h"

namespace {
void check(bool condition, const char* name) {
  if (!condition) {
    std::cerr << "GT911_SAFETY_TEST_FAIL " << name << "\n";
    std::exit(1);
  }
}
}  // namespace

int main() {
  const auto empty = pp::decodeGt911PointInfo(0x00);
  check(empty.valid && !empty.fresh && !empty.touched &&
            empty.reportedTouches == 0,
        "no_new_data_retains_contact");

  const auto released = pp::decodeGt911PointInfo(0x80);
  check(released.valid && released.fresh && !released.touched &&
            released.reportedTouches == 0,
        "fresh_zero_contact_release");

  const auto one = pp::decodeGt911PointInfo(0x81);
  check(one.valid && one.fresh && one.touched && one.reportedTouches == 1,
        "one_point_sample");

  const auto five = pp::decodeGt911PointInfo(0x85);
  check(five.valid && five.touched && five.reportedTouches == 5,
        "five_point_upper_bound");

  const auto six = pp::decodeGt911PointInfo(0x86);
  check(!six.valid && !six.touched && six.reportedTouches == 6,
        "sixth_point_rejected");

  const auto countWithoutReady = pp::decodeGt911PointInfo(0x02);
  check(!countWithoutReady.valid && !countWithoutReady.touched,
        "count_without_ready_rejected");

  std::cout << "GT911_SAFETY_TEST_PASS\n";
  return 0;
}
