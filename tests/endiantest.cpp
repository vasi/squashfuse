#include <gtest/gtest.h>

extern "C" {
#include "swap.h"
#include <endian.h>
}

TEST(endiantest, u16) {
  uint16_t val = htole16(0xbabe);
  sqfs_swapin16(&val);
  EXPECT_EQ(val, 0xbabe);
}

TEST(endiantest, u32) {
  uint32_t val = htole32(0xc0ffee01);
  sqfs_swapin32(&val);
  EXPECT_EQ(val, 0xc0ffee01);
}

TEST(endiantest, u64) {
  uint64_t val = htole64(0xf00ddeadbeeff00dUL);
  sqfs_swapin64(&val);
  EXPECT_EQ(val, 0xf00ddeadbeeff00dUL);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
