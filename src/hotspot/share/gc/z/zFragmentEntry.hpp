#ifndef SHARE_GC_Z_ZFRAGMENTENTRY_HPP
#define SHARE_GC_Z_ZFRAGMENTENTRY_HPP

#include "gc/z/zBitField.hpp"

// Fragment Table Entry Layout
// -----------------------
//
//
//  * 63-63 (1-bit) Copied Flag
//  |
//  +-+--+--------------------------------+-----------------------------------+
//  |1|11|11111 11111111 11111111 11111111|11111111 11111111 11111111 11111111|
//  +-+--+--------------------------------+-----------------------------------+
//    |  |                                |
//    |  * 61-62 Overlapping              |
//    |            entries (2-bits)       * 31-0 Live Bit Map (32-bits)
//    |
//    * 60-32 Amount of Live Bytes (29-bits)

typedef size_t ZFragmentObjectCursor;

class ZFragment;

class ZFragmentEntry {
private:

  typedef ZBitField<uint64_t, uint32_t, 0, 32>     field_live_bits;
  typedef ZBitField<uint64_t, uint32_t, 32, 31>    field_live_bytes;
  typedef ZBitField<uint64_t, bool, 63, 1>         field_copied;

  size_t convert_index(size_t index) const;

public:
  uint64_t         _entry;

  ZFragmentEntry() :
    _entry(0) {}

  void clear();

  bool get_liveness(size_t index) const;
  void set_liveness(size_t index);

  int32_t get_next_live_object(ZFragmentObjectCursor* cursor);
  size_t fragment_internal_index(uintptr_t old_page, uintptr_t from_offset);

  uint32_t get_live_bytes() const;
  void set_live_bytes(uint32_t value);

  bool copied() const;
  void set_copied();

  uint32_t count_live_objects(uintptr_t old_page, uintptr_t from_offset, ZFragment* fragment);
  uint32_t calc_fragment_live_bytes(ZFragment* fragment, size_t index);

};

#endif // SHARE_GC_Z_ZFRAGMENTENTRY_HPP
