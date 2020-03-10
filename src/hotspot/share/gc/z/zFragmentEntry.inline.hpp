#ifndef SHARE_GC_Z_ZFRAGMENTTABLEENTRY_INLINE_HPP
#define SHARE_GC_Z_ZFRAGMENTTABLEENTRY_INLINE_HPP

#include "gc/z/zFragment.inline.hpp"
#include "gc/z/zFragmentEntry.hpp"
#include "gc/z/zGlobals.hpp"
#include "utilities/count_leading_zeros.hpp"
#include <iostream>

inline void ZFragmentEntry::clear() {
  _entry = 0;
}

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

  _entry |= 1UL << convert_index(index);
}

inline bool ZFragmentEntry::copied() const {
  return field_copied::decode(_entry);
}

inline void ZFragmentEntry::set_copied() {
  _entry |= 1UL << 63;
}


inline uint32_t ZFragmentEntry::get_live_bytes() const {
  return field_live_bytes::decode(_entry);
}


inline void ZFragmentEntry::set_live_bytes(uint32_t value) {
  assert(!copied(), "Updating not allowed");
  // 0xE0000000FFFFFFFF =
  // 10000000 00000000 00000000 00000000 11111111 11111111 11111111 11111111
  //
  // Thus, using this as a mask, we will reset live bytes to zero and keep
  // the values in the copied flag and the live bit map
  _entry = field_live_bytes::encode(value) | (_entry & 0x80000000FFFFFFFF);
}

inline uint32_t ZFragmentEntry::calc_fragment_live_bytes(ZFragment* fragment, size_t entry_index) {
  assert(!copied(), "Updating not allowed");
  uint32_t live_bytes = 0;

  // Simplest implmentation as possible. NOT EFFICENT
  // TODO: change to count_leading_zeros
  size_t internal_index = 0;
  while (internal_index < 31) {
    if (get_liveness(internal_index)) {
      uintptr_t offset = fragment->from_offset(entry_index, internal_index);
      size_t p_index = fragment->page_index(offset);
      live_bytes += (fragment->size_entries_begin() + p_index)->entry;
    }

    internal_index++;
  }

  return live_bytes;
}

inline int32_t ZFragmentEntry::get_next_live_object(ZFragmentObjectCursor* cursor) {
  ZFragmentObjectCursor local_cursor = *cursor;
  assert(local_cursor < 33, "cursor value too large");
  if (local_cursor == 32) {
    return -1;
  }
  int32_t object = -1;
  uint32_t live_bits = _entry;

  for (bool live = get_liveness(local_cursor);local_cursor<32;local_cursor++) {
    if (live) {
      *cursor = local_cursor + 1;
      return object;
    }
  }

  return object;
}

inline uint32_t ZFragmentEntry::count_live_objects(uintptr_t old_page, uintptr_t from_offset, ZFragment* fragment) {
  size_t index = fragment_internal_index(old_page, from_offset);
  assert(index < 32, "index out of bounds");
  uint32_t cursor = 0;
  uint32_t live_bytes = 0;
  uint32_t live_bits = _entry;
  bool count = false;

  for (bool live = get_liveness(cursor); cursor<index; cursor++) {
    if (live) {
      uintptr_t offset = fragment->from_offset(fragment->offset_to_index(from_offset), cursor);
      size_t p_index = fragment->page_index(offset);
      assert(p_index < 262144, "out of bounds");
      live_bytes += (fragment->size_entries_begin() + p_index)->entry;
    }
  }

  return live_bytes;
}

inline size_t ZFragmentEntry::fragment_internal_index(uintptr_t old_page, uintptr_t from_offset) {
  assert(from_offset >= old_page, "XXX");
  return ((from_offset - old_page) >> 3) % 32;
}

#endif // SHARE_GC_Z_ZFRAGMENTTABLEENTRY_INLINE_HPP
