#ifndef SHARE_GC_Z_ZFRAGMENT_INLINE_HPP
#define SHARE_GC_Z_ZFRAGMENT_INLINE_HPP

#include "gc/z/zFragment.hpp"
#include "gc/z/zPage.inline.hpp"
#include "gc/z/zAttachedArray.inline.hpp"
#include "gc/z/zFragment.hpp"
#include "gc/z/zFragmentEntry.inline.hpp"
#include "gc/z/zHeap.hpp"
#include "gc/z/zHash.inline.hpp"
#include "gc/z/zAllocationFlags.hpp"
#include "runtime/atomic.hpp"
#include <iostream>

inline ZPage* ZFragment::new_page(uintptr_t offset) const {
  return offset > _last_obj_page0 && _last_obj_page0 > 0 ? _new_page1 : _new_page0;
}

inline ZPage* ZFragment::old_page() const {
  return _old_page;
}

inline const uintptr_t ZFragment::old_start() {
  return _old_virtual.start();
}

inline const size_t ZFragment::old_size() {
  return _old_virtual.size();
}

inline ZSizeEntry* ZFragment::size_entries_begin() const {
  return (ZSizeEntry*) entries_begin() + entries_count();
}

inline size_t ZFragment::entries_count() const {
  return _entries.length();
}

inline ZFragmentEntry* ZFragment::entries_begin() const {
  return _entries(this);
}

inline ZFragmentEntry* ZFragment::entries_end() {
  return entries_begin() + _entries.length();
}

inline bool ZFragment::inc_refcount() {
  uint32_t refcount = Atomic::load(&_refcount);

  while (refcount > 0) {
    const uint32_t old_refcount = refcount;
    const uint32_t new_refcount = old_refcount + 1;
    const uint32_t prev_refcount = Atomic::cmpxchg(&_refcount, old_refcount, new_refcount);
    if (prev_refcount == old_refcount) {
      return true;
    }

    refcount = prev_refcount;
  }

  return false;
}

inline bool ZFragment::dec_refcount() {
  assert(_refcount > 0, "Invalid state");
  return Atomic::sub(&_refcount, 1u) == 0u;
}

inline bool ZFragment::retain_page() {
  return inc_refcount();
}

inline void ZFragment::release_page() {
  if (dec_refcount()) {
    ZHeap::heap()->free_page(const_cast<ZPage*>(_old_page), true /* reclaimed */);
    _old_page = NULL;
  }
}

inline bool ZFragment::is_pinned() const {
  return Atomic::load(&_pinned);
}

inline void ZFragment::set_pinned() {
  Atomic::store(&_pinned, true);
}

inline size_t ZFragment::offset_to_index(uintptr_t from_offset) const {
  return (from_offset - _ops) / 256;
}

inline size_t ZFragment::offset_to_internal_index(uintptr_t from_offset) const {
  return (from_offset - _ops) / 8 % 32;
}

inline uintptr_t ZFragment::from_offset(size_t entry_index, size_t internal_index) const {
  return _ops + (entry_index * 256) + (internal_index * 8);
}

inline ZFragmentEntry* ZFragment::find(uintptr_t from_offset) const {
  // TODO: Add explaination of magical 2 instrucion lookup
  //return (ZFragmentEntry*)(from_offset >> 5) - _conversion_constant + (uintptr_t)this->entries_begin();
  return entries_begin() + offset_to_index(from_offset);
}

inline uintptr_t ZFragment::page_index(uintptr_t from_offset) {
  return (from_offset - _ops) / 8;
}

inline void ZFragment::calc_fragments_live_bytes() {
  // TODO: add assert here about which ZGC state we are in.
  ZFragmentEntry* p = entries_begin();
  const ZFragmentEntry* end = entries_end();
  uint32_t accumulated_live_bytes = 0;
  for (size_t i = 0; p < end; p++, i++) {
    p->set_live_bytes(accumulated_live_bytes);
    accumulated_live_bytes += p->calc_fragment_live_bytes(this, i);
  }
}

inline void ZFragment::fill_entires() {
  size_t nsegments = old_page()->_livemap.nsegments;

  for (BitMap::idx_t segment = old_page()->_livemap.first_live_segment(); segment < nsegments; segment = old_page()->_livemap.next_live_segment(segment)) {
    // For each live segment
    const BitMap::idx_t start_index = old_page()->_livemap.segment_start(segment);
    const BitMap::idx_t end_index   = old_page()->_livemap.segment_end(segment);
    BitMap::idx_t index = old_page()->_livemap._bitmap.get_next_one_offset(start_index, end_index);

    while (index < end_index) {
      // Calculate object address
      const uintptr_t offset = _ops + ((index / 2) << old_page()->object_alignment_shift());
      const uintptr_t addr = ZAddress::good(offset);

      // Apply closure
      ZFragmentEntry* entry = find(offset);
      size_t internal_index = offset_to_internal_index(offset);
      entry->set_liveness(internal_index);

      const size_t size = ZUtils::object_size(addr);
      size_t p_index = page_index(offset);
      assert(p_index < old_page()->size()/8, "");
      ZSizeEntry* size_entry = size_entries_begin() + p_index;
      size_entry->entry = size;

      // Find next bit after this object
      const uintptr_t next_addr = align_up(addr + size, 1 << old_page()->object_alignment_shift());
      const BitMap::idx_t next_index = ((next_addr - ZAddress::good(_ops)) >> old_page()->object_alignment_shift()) * 2;
      if (next_index >= end_index) {
        // End of live map
        break;
      }

      index = old_page()->_livemap._bitmap.get_next_one_offset(next_index, end_index);
    }
  }
}

inline uintptr_t ZFragment::to_offset(uintptr_t from_offset) {
  return to_offset(from_offset, find(from_offset));
}

inline uintptr_t ZFragment::new_page_start(uintptr_t offset) const {
  if (offset > _last_obj_page0 && _last_obj_page0 > 0) {
    return _new_page1->start();
  } else {
    return _new_page0->start() + _offset0;
  }
}

inline uintptr_t ZFragment::to_offset(uintptr_t from_offset, ZFragmentEntry* entry) {
  uintptr_t page_base = _ops; // FIXME old page start. can be removed?
  return
    new_page_start(from_offset) +
    entry->get_live_bytes() +
    entry->count_live_objects(_ops, from_offset, this);
}

inline void ZFragment::set_offset0(size_t size) {
  _offset0 = size;
}

inline void ZFragment::set_last_obj_page0(uintptr_t addr) {
  _last_obj_page0 = ZAddress::offset(addr);
}

inline uintptr_t ZFragment::get_last_obj_page0() {
  return _last_obj_page0;
}

#endif // SHARE_GC_Z_ZFRAGMENT_INLINE_HPP
