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

inline Overlapping ZFragmentEntry::get_overlapping() const {
  return field_overlapping::decode(_entry);
}

inline void ZFragmentEntry::set_overlapping(Overlapping o) {
  assert(!copied(), "Updating not allowed");
  // 0x9FFFFFFFFFFFFFFF =
  // 10011111 11111111 11111111 11111111 11111111 11111111 11111111 11111111
  //
  // Thus, using this as a mask, we will reset overlapping and keep
  // the values in other fields untouched
  _entry = field_overlapping::encode(o) | _entry;
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

  Overlapping o = field_overlapping::decode(_entry);
  _entry |= 1UL << convert_index(index);
  assert(o == field_overlapping::decode(_entry), "");
}

inline void ZFragmentEntry::set_size_bit(size_t index, size_t size) {
  assert(!copied(), "Updating not allowed");
  assert(size > 0, "live object can't have size 0");
  size_t size_index = index + size / 8 - 1;
  if (size_index < 32) {
    Overlapping o = field_overlapping::decode(_entry);
    set_liveness(size_index);
    assert(o == field_overlapping::decode(_entry), "");
  } else {
    size_t entry_offset = size_index / 32;
    for (size_t i = 1; i < entry_offset;i++) {
      (this + i)->set_overlapping(Overlapping::SKIP);
    }
    ZFragmentEntry* next_entry = this + entry_offset;
    next_entry->set_liveness(size_index % 32);
    next_entry->set_overlapping(Overlapping::PREVIOUS);
    set_overlapping(Overlapping::NEXT);
    //if (entry_offset > 1) assert(false, "stop here");
    verify_pair();
    next_entry->verify_pair();
  }
}

inline uint32_t ZFragmentEntry::get_live_bytes() const {
  return field_live_bytes::decode(_entry);
}

inline void ZFragmentEntry::set_live_bytes(uint32_t value) {
  assert(!copied(), "Updating not allowed");
  // 0xE0000000FFFFFFFF =
  // 11100000 00000000 00000000 00000000 11111111 11111111 11111111 11111111
  //
  // Thus, using this as a mask, we will reset live bytes to zero and keep
  // the values in the copied flag and the live bit map
  Overlapping o = field_overlapping::decode(_entry);
  _entry = field_live_bytes::encode(value) | (_entry & 0xE0000000FFFFFFFF);
  assert(o == field_overlapping::decode(_entry), "");
}

inline bool ZFragmentEntry::copied() const {
  return field_copied::decode(_entry);
}

inline void ZFragmentEntry::set_copied() {
  Overlapping o = field_overlapping::decode(_entry);
  _entry |= 1UL << 63;
  assert(o == field_overlapping::decode(_entry), "");
}

inline uint32_t ZFragmentEntry::calc_fragment_live_bytes() {
  assert(!copied(), "Updating not allowed");
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

  if (get_overlapping() == Overlapping::PREVIOUS) {
    // Ignore first 1
    uint32_t zeros = count_leading_zeros(live_bits);
    if (zeros < 31) {
      live_bits = live_bits << count_leading_zeros(live_bits) + 1;
    } else {
      live_bits = 0;
    }
  }
  Overlapping o = get_overlapping();

  while (live_bits != 0) {
    uint32_t zeros = count_leading_zeros(live_bits);
    if (zeros < 31) {
      live_bits = live_bits << count_leading_zeros(live_bits) + 1;
    } else {
      live_bits = 0;
    }

    if (live_bits == 0) {
      return live_bytes;
    }
    uint32_t zero_width = count_leading_zeros(live_bits);
    live_bits = live_bits << (zero_width + 1);

    // Add 2 since we should include 1's as well.
    // Since 1 bit = 1 word, we should therefore multiply with 8
    live_bytes = live_bytes + (zero_width + 2) * 8;
  }

  return live_bytes;
}

inline uint32_t ZFragmentEntry::get_first_mark_size_next_entry() {
  assert(get_overlapping() == Overlapping::NEXT, "should be overlapping");
  ZFragmentEntry* next_entry = (ZFragmentEntry*)this + 1;
  uint32_t count = 0;

  if (Overlapping::NEXT == get_overlapping()) {
    do {
      uint32_t live_bits = next_entry->_entry;
      if (live_bits == 0) {
        count += 32;
      } else {
        count += count_leading_zeros(live_bits) + 1;
        return count;
      }
      next_entry += 1;
    } while (next_entry->get_overlapping() != Overlapping::SKIP);

    uint32_t live_bits = next_entry->_entry;
    count = count_leading_zeros(live_bits);
    return count > 0 ? count + 1 : 0;
  }

  uint32_t live_bits = _entry;
  count = count_leading_zeros(live_bits);
  return count > 0 ? count + 1 : 0;
}

inline int32_t ZFragmentEntry::get_next_live_object(ZFragmentObjectCursor* cursor, size_t* size) {
  ZFragmentObjectCursor local_cursor = *cursor;
  assert(local_cursor < 33, "cursor value too large");
  if (local_cursor == 32) {
    return -1;
  }
  int32_t object = -1;
  uint32_t live_bits = _entry;
  bool count = false;
  size_t local_size = 0;

  for (bool live = get_liveness(local_cursor);local_cursor<32;local_cursor++) {
    if (live && !count) { // first encounter
      object = local_cursor;
      local_cursor++;
      local_size++;
      count = true;
    } else if (live && count) { // last encounter
      *size = (local_size + 1) << 3;
      *cursor = local_cursor + 1;
      return object;
    } else if (count) {
      local_size++;
    }
  }

  if (get_overlapping() == Overlapping::NEXT && count) {
    *size = (local_size + get_first_mark_size_next_entry()) << 3;
    *cursor = local_cursor + 1;
    return object;
  }

  assert(!count, "live bits should always be in pairs");
  return object;
}

inline void ZFragmentEntry::verify_pair() {
  uint32_t live_bits = _entry;
  size_t i = 0;
  while (live_bits != 0) {
    uint32_t zeros = count_leading_zeros(live_bits);
    if (zeros < 31) {
      live_bits = live_bits << count_leading_zeros(live_bits) + 1;
    } else {
      live_bits = 0;
    }
    i++;
  }

  if (i % 2 != 0 && Overlapping::NONE == get_overlapping()) {
    assert(false, "should not happen");
  }
}

inline uint32_t ZFragmentEntry::count_live_objects(uintptr_t old_page, uintptr_t from_offset) {
  size_t index = fragment_internal_index(old_page, from_offset);
  assert(index < 32, "index out of bounds");
  uint32_t cursor = 0;
  uint32_t live_bytes = 0;
  uint32_t live_bits = _entry;
  bool count = false;
  verify_pair();
  for (bool live = get_liveness(cursor);cursor<index;cursor++) {
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

  if (get_overlapping() == Overlapping::NEXT && count) {
    live_bytes += get_first_mark_size_next_entry();
    count = false;
  }

  assert(!count, "live bits should always be in pairs");
  return live_bytes << 3;
}

inline size_t ZFragmentEntry::fragment_internal_index(uintptr_t old_page, uintptr_t from_offset) {
  return ((old_page - from_offset) >> 3) % 32;
}

#endif // SHARE_GC_Z_ZFRAGMENTTABLEENTRY_INLINE_HPP
