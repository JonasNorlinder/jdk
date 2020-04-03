#ifndef SHARE_GC_Z_ZFRAGMENTTABLEENTRY_INLINE_HPP
#define SHARE_GC_Z_ZFRAGMENTTABLEENTRY_INLINE_HPP

#include "gc/z/zFragment.inline.hpp"
#include "gc/z/zFragmentEntry.hpp"
#include "gc/z/zGlobals.hpp"
#include "utilities/count_leading_zeros.hpp"
#include <iostream>

inline size_t ZFragmentEntry::convert_index(size_t index) const {
  assert(index < 32, "index too large");
  return 31 - index;
}

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
  return (_entry >> convert_index(index)) & 1UL;
}

inline void ZFragmentEntry::set_liveness(size_t index) {
  assert(!copied(), "Updating liveness not allowed");
  assert(ZGlobalPhase == ZPhaseMarkCompleted, "Updating liveness is not allowed");
  assert(index < 32, "Invalid index");

  _entry |= 1UL << convert_index(index);
}

inline bool ZFragmentEntry::copied() const {
  return field_copied::decode(_entry);
}

inline void ZFragmentEntry::set_copied() {
  _entry |= 1UL << 63;
}


inline uint32_t ZFragmentEntry::live_bytes_before_fragment() const {
  return field_live_bytes::decode(_entry);
}


inline void ZFragmentEntry::set_live_bytes_before_fragment(uint32_t value) {
  assert(!copied(), "Updating not allowed");
  // 0xE0000000FFFFFFFF =
  // 10000000 00000000 00000000 00000000 11111111 11111111 11111111 11111111
  //
  // Thus, using this as a mask, we will reset live bytes to zero and keep
  // the values in the copied flag and the live bit map
  _entry = field_live_bytes::encode(value) | (_entry & 0x80000000FFFFFFFF);
}

inline void ZFragmentEntry::set_size_bit(size_t index, size_t size) {
  assert(!copied(), "Updating not allowed");
  const size_t size_index = index + size / 8 - 1;

  // If size_index is larger than the current entry, that
  // would imply that this is the last living object on this entry.
  //
  // We need to calculate the live bytes of all living objects *before*
  // the current object we are inspecting, therefore this implies that
  // we don't have to store the size bit of the last object on the entry.
  if (size_index > 31) {
    return;
  }

  set_liveness(size_index % 32);
}

inline int32_t ZFragmentEntry::get_next_live_object(ZFragmentObjectCursor* cursor) const {
  if (*cursor > 31) return -1;
  uint32_t live_bits = _entry << convert_index(*cursor);
  if (live_bits == 0) return -1;
  int32_t object = -1;

  object = count_leading_zeros(live_bits) + 1;

  live_bits = live_bits << object;
  if (live_bits == 0) {
    *cursor = 32;
  } else {
    *cursor = object + count_leading_zeros(live_bits) + 1;
  }

  return convert_index(object);
}

inline uint32_t ZFragmentEntry::live_bytes_on_fragment(uintptr_t old_page, uintptr_t from_offset, ZFragment* fragment) {
  size_t index = fragment_internal_index(old_page, from_offset);
  assert(index < 32, "index out of bounds");

  uint32_t cursor = 0;
  uint32_t live_bytes = 0;
  bool count = false;

  if (fragment->is_on_page_break(this) && fragment->is_on_snd_page(from_offset)) {
    cursor = fragment->page_break_entry_internal_index();
  }

  for (; cursor<index; cursor = cursor + 1) {
    bool live = get_liveness(cursor);
    if (live && !count) { // first encounter
      live_bytes++;
      count = true;
    } else if (live && count) { // last encounter
      live_bytes++;
      count = false;
    } else if (count) {
      live_bytes++;
    }
  }

  return live_bytes << 3;
}

inline size_t ZFragmentEntry::fragment_internal_index(uintptr_t old_page, uintptr_t from_offset) const {
  assert(from_offset >= old_page, "XXX");
  return ((from_offset - old_page) >> 3) % 32;
}

#endif // SHARE_GC_Z_ZFRAGMENTTABLEENTRY_INLINE_HPP
