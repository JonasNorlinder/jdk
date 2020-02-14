#ifndef SHARE_GC_Z_ZFRAGMENTTABLEENTRY_INLINE_HPP
#define SHARE_GC_Z_ZFRAGMENTTABLEENTRY_INLINE_HPP

#include "gc/z/zFragmentEntry.hpp"
#include "gc/z/zGlobals.hpp"
#include "utilities/count_leading_zeros.hpp"

inline bool ZFragmentEntry::get_liveness(size_t index) const {
  // Right shift live bits in order of size index,
  // then perform bitwise AND using 1
  //
  // Example:     Index: 14
  //          Live bits: 0000 0000 0010 0100 0100 0000 0000 0000
  // [Right shift live bits 14 times] =>
  //          Live bits: 0000 0000 0000 0000 0000 0000 1001 0001
  // [Bitwise AND with 1] =>
  //
  //   0000 0000 0000 0000 0000 0000 1001 0001
  //                                       AND
  //   0000 0000 0000 0000 0000 0000 0000 0001
  // = 0000 0000 0000 0000 0000 0000 0000 0001 = True
  return (_entry >> index) & 1L;
}

inline void ZFragmentEntry::set_liveness(size_t index) {
  assert(!copied(), "Updating liveness not allowed");
  assert(ZGlobalPhase == ZPhaseMark, "Updating liveness is not allowed");
  assert(index < 32, "index out of bounds");
  _entry |= 1 << index;
}

inline uint32_t ZFragmentEntry::get_live_bytes() const {
  return field_live_bytes::decode(_entry);
}

inline void ZFragmentEntry::set_live_bytes(uint32_t value) {
  assert(!copied(), "Updating live bytes not allowed");
  // 0x80000000FFFFFFFF =
  // 10000000 00000000 00000000 00000000 11111111 11111111 11111111 11111111
  //
  // Thus, using this as a mask, we will reset live bytes to zero and keep
  // the values in the copied flag and the live bit map
  _entry = field_live_bytes::encode(value) | (_entry & 0x80000000FFFFFFFFL);
}

inline bool ZFragmentEntry::copied() const {
  return field_copied::decode(_entry);
}

inline void ZFragmentEntry::set_copied() {
  _entry |= 1L << 63;
}

inline uint32_t ZFragmentEntry::calc_fragment_live_bytes() {
  uint32_t live_bytes = 0;
  uint32_t live_bits = _entry;

  // Example
  //
  // Note that 1001 should have 0-width = 2 and including 1's
  // this means that the fragment has size of 32 bytes
  //
  // Assume:
  //   live_bits = 1001 0000 0000 0000 0000 0000 0000 1001
  //
  // while-loop (1st iteration):
  //   live_bits = 0010 0000 0000 0000 0000 0000 0001 0010
  //   width = 2
  //   live_bits = 0000 0000 0000 0000 0000 0000 1001 0000
  // while-loop (2nd iteration):
  //   live_bits = 0010 0000 0000 0000 0000 0000 0000 0000
  //   width = 4
  //   live_bits = 0000 0000 0000 0000 0000 0000 0000 0000
  //
  //   [terminate on condition]

  while (live_bits != 0) {
    live_bits = live_bits << count_leading_zeros(live_bits) + 1;
    uint32_t zero_width = count_leading_zeros(live_bits);
    live_bits = live_bits << zero_width + 1;

    // Add 2 since we should include 1's as well.
    // Since 1 bit = 1 word, we should therefore multiply with 8
    live_bytes = live_bytes + (zero_width + 2) * 8;
  }

  return live_bytes;
}

inline int32_t ZFragmentEntry::get_next_live_object(ZFragmentObjectCursor* cursor, bool* is_obj) {
  assert(*cursor < 32, "cursor value too large");
  int32_t object = -1;
  uint32_t live_bits = _entry;
  bool obj = *is_obj;

  for (bool live = get_liveness(*cursor);*cursor<32;*cursor = *cursor + 1) {
    if (live && obj) {
      object = *cursor;
      *cursor = *cursor + 1;
      *is_obj = false;
      return object;
    } else if (live && !obj) {
      *is_obj = true;
      return object;
    }
  }

  return object;
}


#endif // SHARE_GC_Z_ZFRAGMENTTABLEENTRY_INLINE_HPP
