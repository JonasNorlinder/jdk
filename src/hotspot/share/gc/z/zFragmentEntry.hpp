#ifndef SHARE_GC_Z_ZFRAGMENTENTRY_HPP
#define SHARE_GC_Z_ZFRAGMENTENTRY_HPP

#include "gc/z/zBitField.hpp"

// Fragment Table Entry Layout
// -----------------------
//
//
//  * 63-63 (1-bit) Copied Flag
//  |
//  +-+----------------------------------+-----------------------------------+
//  |1|1111111 11111111 11111111 11111111|11111111 11111111 11111111 11111111|
//  +-+----------------------------------+-----------------------------------+
//    |                                  |
//    |                                  |
//    |                                  * 31-0 Live Bit Map (32-bits)
//    |
//    * 62-32 Amount of Live Bytes (31-bits)

typedef int32_t ZFragmentObjectCursor;

class ZFragment;

class ZFragmentEntry {
private:
  uint64_t         _entry;

  typedef ZBitField<uint64_t, uint32_t, 0, 32>     field_live_bits;
  typedef ZBitField<uint64_t, uint32_t, 32, 31>    field_live_bytes;
  typedef ZBitField<uint64_t, bool, 63, 1>         field_copied;

  ZFragmentObjectCursor move_cursor(ZFragmentObjectCursor cursor, bool count) const;

public:
  ZFragmentEntry() :
    _entry(0) {}

  void clear();

  void set_size_bit(size_t index, size_t size);

  bool get_liveness(size_t index) const;
  void set_liveness(size_t index);

  int32_t get_next_live_object(ZFragmentObjectCursor cursor, bool count) const;
  size_t fragment_internal_index(uintptr_t old_page, uintptr_t from_offset) const;

  uint32_t live_bytes_before_fragment() const;
  void set_live_bytes_before_fragment(uint32_t value);

  bool copied() const;
  void set_copied();

  uint32_t live_bytes_on_fragment(uintptr_t old_page, uintptr_t from_offset, ZFragment* fragment);

};

#endif // SHARE_GC_Z_ZFRAGMENTENTRY_HPP
