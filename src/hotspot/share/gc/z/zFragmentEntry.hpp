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

enum Overlapping {
                  SKIP = 3,
                  PREVIOUS = 2,
                  NEXT = 1,
                  NONE = 0
};

class ZFragmentEntry {
private:

  typedef ZBitField<uint64_t, uint32_t, 0, 32>     field_live_bits;
  typedef ZBitField<uint64_t, uint32_t, 32, 29>    field_live_bytes;
  typedef ZBitField<uint64_t, Overlapping, 61, 2>  field_overlapping;
  typedef ZBitField<uint64_t, bool, 63, 1>         field_copied;

  size_t convert_index(size_t index) const;
  uint32_t get_first_mark_size_next_entry();

public:
  uint64_t         _entry;

  ZFragmentEntry() :
    _entry(0) {}

  void clear();
  void verify_pair();
  Overlapping get_overlapping() const;
  void set_overlapping(Overlapping o);

  bool get_liveness(size_t index) const;
  void set_liveness(size_t index);
  void set_size_bit(size_t index, size_t size);

  int32_t get_next_live_object(ZFragmentObjectCursor* cursor, size_t* size);
  size_t fragment_internal_index(uintptr_t old_page, uintptr_t from_offset);

  uint32_t get_live_bytes() const;
  void set_live_bytes(uint32_t value);

  bool copied() const;
  void set_copied();

  uint32_t count_live_objects(uintptr_t old_page, uintptr_t from_offset);
  uint32_t calc_fragment_live_bytes();

};

#endif // SHARE_GC_Z_ZFRAGMENTENTRY_HPP
