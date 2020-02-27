#ifndef SHARE_GC_Z_ZFRAGMENTENTRY_HPP
#define SHARE_GC_Z_ZFRAGMENTENTRY_HPP

#include "gc/z/zBitField.hpp"

// Fragment Table Entry Layout
// -----------------------
//
//
//  * 63-63 (1-bit) Copied Flag
//  |
//  +-+------------------------------------+------------------------------------+
//  |1| 1111111 11111111 11111111 11111111 | 11111111 11111111 11111111 11111111|
//  +-+------------------------------------+------------------------------------+
//    |                                    |
//    |                                    |
//    |                                    * 31-0 Live Bit Map (32-bits)
//    |
//    * 62-32 Amount of Live Bytes (31-bits)

typedef size_t ZFragmentObjectCursor;

class ZFragmentEntry {
private:
  uint64_t         _entry;

  typedef ZBitField<uint64_t, uint32_t, 0, 32>    field_live_bits;
  typedef ZBitField<uint64_t, uint32_t, 32, 31>   field_live_bytes;
  typedef ZBitField<uint64_t, bool, 63, 1>        field_copied;

public:
  ZFragmentEntry() :
    _entry(0) {}

  bool get_liveness(size_t index) const;
  void set_liveness(size_t index);

  uint32_t get_live_bytes() const;
  void set_live_bytes(uint32_t value);

  bool copied() const;
  void set_copied();

  int32_t get_next_live_object(ZFragmentObjectCursor* cursor, bool* is_obj);
  uint32_t calc_fragment_live_bytes();
};

#endif // SHARE_GC_Z_ZFRAGMENTENTRY_HPP
