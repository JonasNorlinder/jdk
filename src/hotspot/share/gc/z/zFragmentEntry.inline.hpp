#ifndef SHARE_GC_Z_ZFRAGMENTTABLEENTRY_INLINE_HPP
#define SHARE_GC_Z_ZFRAGMENTTABLEENTRY_INLINE_HPP

#include "gc/z/zFragment.inline.hpp"
#include "gc/z/zFragmentEntry.hpp"
#include "gc/z/zGlobals.hpp"
#include "utilities/count_trailing_zeros.hpp"
#include <iostream>

inline void ZFragmentEntry::clear() {
  _entry = 0;
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
  return (_entry >> index) & 1UL;
}

inline void ZFragmentEntry::set_liveness(size_t index) {
  assert(!copied(), "Updating liveness not allowed");
  assert(ZGlobalPhase == ZPhaseMarkCompleted, "Updating liveness is not allowed");
  assert(index < 32, "Invalid index");

  _entry |= 1UL << index;
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
  const size_t size_index = index + (size >> 3) - 1;

  // If size_index is larger than the current entry, that
  // would imply that this is the last living object on this entry.
  //
  // We need to calculate the live bytes of all living objects *before*
  // the current object we are inspecting, therefore this implies that
  // we don't have to store the size bit of the last object on the entry.
  if (size_index > 31) {
    return;
  }

  set_liveness(size_index & 31);
}


inline ZFragmentObjectCursor ZFragmentEntry::move_cursor(ZFragmentObjectCursor cursor, bool count) const {
  cursor = !count ? cursor : cursor + 1;
  int32_t mask = cursor < 32 ? (~0U) << (cursor) : 0U;
  uint32_t entry = _entry & mask;
  if (entry == 0) return -1;
  cursor = count_trailing_zeros(entry);
  return cursor;
}

inline int32_t ZFragmentEntry::get_next_live_object(ZFragmentObjectCursor cursor, bool count) const {
  cursor = move_cursor(cursor, count);
  if (cursor == -1) return -1; // No more live objects

  if (!count) { // first encounter
    return cursor;
  }

  return move_cursor(cursor, count);
}

inline size_t ZFragmentEntry::get_size(int32_t cursor) const {
  int32_t mask = cursor < 32 ? (~0U) << (cursor + 1) : 0U;
  uint32_t entry = _entry & mask;
  if (entry == 0) return 0;
  return (count_trailing_zeros(entry) - cursor + 1) << 3;
}

inline uint32_t ZFragmentEntry::live_bytes_on_fragment_n(uintptr_t old_page,
                                                       uintptr_t from_offset,
                                                       ZFragment *fragment) {
  size_t index = fragment_internal_index(old_page, from_offset);
  assert(index < 32, "index out of bounds");

  uint32_t cursor = move_cursor(0, false);
  if (index == 0 || cursor == index) return 0;
  uint32_t live_bytes = get_size(cursor);
  cursor = move_cursor(cursor, true);

  for (cursor = move_cursor(cursor, true); cursor<index; cursor = move_cursor(cursor, true)) {
    live_bytes += get_size(cursor);
    cursor = move_cursor(cursor, true);
  }

  return live_bytes;
}

inline uint32_t ZFragmentEntry::live_bytes_on_fragment(uintptr_t old_page, uintptr_t from_offset, ZFragment* fragment) {
  size_t index = fragment_internal_index(old_page, from_offset);
  assert(index < 32, "index out of bounds");

  uint32_t cursor = 0;
  uint32_t live_bytes = 0;
  bool count = false;

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

  uint32_t result = live_bytes << 3;
  assert(result==live_bytes_on_fragment_n(old_page, from_offset, fragment), "");
  return result;
}

inline size_t ZFragmentEntry::fragment_internal_index(uintptr_t old_page, uintptr_t from_offset) const {
  return ((from_offset - old_page) >> 3) & 31;
}

#endif // SHARE_GC_Z_ZFRAGMENTTABLEENTRY_INLINE_HPP
