/*
 *
 * Copyright 2017 Asylo authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "asylo/platform/arch/include/trusted/hardware_random.h"

#include <immintrin.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

namespace asylo {
namespace {

static void rdrand64(void *out) {
  // Spec recommends retrying for transient failures.
  constexpr int kRandRetries = 10;
  for (int i = 0; i < kRandRetries; ++i) {
    if (_rdrand64_step(static_cast<unsigned long long *>(out))) {
      return;
    }
  }

  abort();
}

static uint64_t rdrand64() {
  uint64_t temp;
  rdrand64(&temp);

  return temp;
}

// Compute alignment properties of given buffer.
static void CalculateAlignment(const void *buf, size_t size, size_t align_size,
                               size_t *unaligned_bytes, size_t *aligned_count,
                               size_t *partial_bytes) {
  // The number of unaligned bytes is the 2s complement, modulo alignment.
  *unaligned_bytes = (~reinterpret_cast<uintptr_t>(buf) + 1) % align_size;
  *unaligned_bytes = std::min(*unaligned_bytes, size);
  *aligned_count = (size - *unaligned_bytes) / align_size;
  *partial_bytes = size - *unaligned_bytes - (*aligned_count * align_size);
}

}  // namespace

// Fills given buffer with random values generated with the rdrand instruction.
extern "C" ssize_t enc_hardware_random(uint8_t *buf, size_t count) {
  size_t unaligned_bytes;
  size_t aligned_count;
  size_t partial_bytes;
  CalculateAlignment(buf, count, sizeof(uint64_t), &unaligned_bytes,
                     &aligned_count, &partial_bytes);

  // Cast to make offsetting easier.
  uint8_t *byte_buf = static_cast<uint8_t *>(buf);

  // Copy random bytes into any unaligned portion at the start.
  if (unaligned_bytes) {
    uint64_t temp = rdrand64();
    memcpy(&byte_buf[0], &temp, unaligned_bytes);
  }

  // Copy random bytes directly into buffer for the aligned portion.
  uint8_t *aligned_end = &byte_buf[count - partial_bytes];
  for (uint8_t *aligned_buf = &byte_buf[unaligned_bytes];
       aligned_buf < aligned_end; aligned_buf += sizeof(uint64_t)) {
    rdrand64(aligned_buf);
  }

  // Copy random bytes into any partial portion at the end.
  if (partial_bytes) {
    uint64_t temp = rdrand64();
    memcpy(aligned_end, &temp, partial_bytes);
  }

  return count;
}

}  // namespace asylo
