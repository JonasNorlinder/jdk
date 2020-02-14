#ifndef SHARE_GC_Z_ZFRAGMENT_INLINE_HPP
#define SHARE_GC_Z_ZFRAGMENT_INLINE_HPP

#include "gc/z/zFragment.hpp"
#include "gc/z/zPage.inline.hpp"
#include "gc/z/zAttachedArray.inline.hpp"
#include "gc/z/zFragment.hpp"
#include "gc/z/zFragmentEntry.inline.hpp"
#include "gc/z/zHeap.inline.hpp"
#include "gc/z/zHash.inline.hpp"
#include "runtime/atomic.hpp"

inline const ZPage* ZFragment::new_page() {
  return _new_page;
}

inline const ZPage* ZFragment::old_page() const {
  return _old_page;
}

inline const uintptr_t ZFragment::old_start() {
  return _old_virtual.start();
}

inline const size_t ZFragment::old_size() {
  return _old_virtual.size();
}

inline const size_t ZFragment::entries_count() {
  return _entries.length();
}

inline ZFragmentEntry* ZFragment::entries_begin() {
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

inline ZFragmentEntry* ZFragment::find(uintptr_t from_offset) const {
  // TODO: Add explaination of magical 2 instrucion lookup
  return (ZFragmentEntry*)(from_offset >> 5) - _conversion_constant;
}

inline void ZFragment::calc_fragments_live_bytes() {
  // TODO: add assert here about which ZGC state we are in.

  ZFragmentEntry* p = entries_begin();
  const ZFragmentEntry* end = entries_end();
  uint32_t accumulated_live_bytes = 0;
  for (; p < end; p++) {
    p->set_live_bytes(accumulated_live_bytes);
    accumulated_live_bytes += p->calc_fragment_live_bytes();
  }
}

inline void ZFragment::fill_entires() {
  ZFragmentEntry* p = entries_begin();
  const ZFragmentEntry* end = entries_end();
  size_t index_page_livemap = 0;
  for (; p < end; p++) {
    for(size_t index_fragment_livemap=0;index_fragment_livemap < 32;index_page_livemap++, index_fragment_livemap++) {
      if (old_page()->_livemap.get(index_page_livemap)) {
        p->set_liveness(index_fragment_livemap);
      }
    }
  }

}

#endif // SHARE_GC_Z_ZFRAGMENT_INLINE_HPP
