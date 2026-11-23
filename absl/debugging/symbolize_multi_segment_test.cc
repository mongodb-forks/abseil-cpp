// Copyright 2026 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Tests symbolizing a function that lives in a second executable PT_LOAD
// segment, whose addresses are offset from its file position differently than
// the main text segment's. See symbolize_multi_segment_test.ld.

#include <link.h>

#include <cstddef>
#include <cstdint>

#include "gtest/gtest.h"
#include "absl/base/attributes.h"
#include "absl/debugging/symbolize.h"

extern "C" {
ABSL_ATTRIBUTE_NOINLINE void MainSegmentFunction() {
  // Prevent the compiler from folding this function with others.
  volatile int x = __LINE__;
  static_cast<void>(x);
}

ABSL_ATTRIBUTE_NOINLINE __attribute__((section(".text_far_segment"))) void
FarSegmentFunction() {
  volatile int x = __LINE__;
  static_cast<void>(x);
}
}  // extern "C"

namespace {

struct LoadSegment {
  bool found = false;
  bool executable = false;
  uintptr_t start = 0;
  // `p_vaddr - p_offset`: how far the segment's addresses are from its file
  // offsets.
  uintptr_t vaddr_minus_offset = 0;
};

// Describes the PT_LOAD segment of the main executable that contains `addr`.
LoadSegment FindLoadSegmentInMainExecutable(uintptr_t addr) {
  struct Query {
    uintptr_t addr;
    LoadSegment result;
  } query{addr, {}};
  dl_iterate_phdr(
      [](dl_phdr_info *info, size_t, void *data) {
        auto &q = *static_cast<Query *>(data);
        for (size_t i = 0; i < info->dlpi_phnum; ++i) {
          const ElfW(Phdr) &ph = info->dlpi_phdr[i];
          if (ph.p_type != PT_LOAD) continue;
          uintptr_t start = info->dlpi_addr + ph.p_vaddr;
          if (q.addr >= start && q.addr < start + ph.p_memsz) {
            q.result = {true, (ph.p_flags & PF_X) != 0, start,
                        static_cast<uintptr_t>(ph.p_vaddr - ph.p_offset)};
          }
        }
        // The main executable is always the first object visited.
        return 1;
      },
      &query);
  return query.result;
}

// Checks that the linker really produced the two-segment layout, so that the
// tests below cannot silently stop testing anything.
TEST(SymbolizeMultiSegment, FunctionsAreInDifferentSegments) {
  LoadSegment far_segment = FindLoadSegmentInMainExecutable(
      reinterpret_cast<uintptr_t>(&FarSegmentFunction));
  LoadSegment main_segment = FindLoadSegmentInMainExecutable(
      reinterpret_cast<uintptr_t>(&MainSegmentFunction));
  ASSERT_TRUE(far_segment.found);
  ASSERT_TRUE(main_segment.found);
  EXPECT_TRUE(far_segment.executable);
  EXPECT_TRUE(main_segment.executable);
  EXPECT_NE(far_segment.start, main_segment.start);
  EXPECT_NE(far_segment.vaddr_minus_offset, main_segment.vaddr_minus_offset);
}

TEST(SymbolizeMultiSegment, MainSegment) {
  char symbol[1024];
  ASSERT_TRUE(absl::Symbolize(reinterpret_cast<void *>(&MainSegmentFunction),
                              symbol, sizeof(symbol)));
  EXPECT_STREQ("MainSegmentFunction", symbol);
}

TEST(SymbolizeMultiSegment, FarSegment) {
  char symbol[1024];
  ASSERT_TRUE(absl::Symbolize(reinterpret_cast<void *>(&FarSegmentFunction),
                              symbol, sizeof(symbol)));
  EXPECT_STREQ("FarSegmentFunction", symbol);
}

}  // namespace

int main(int argc, char **argv) {
  absl::InitializeSymbolizer(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
