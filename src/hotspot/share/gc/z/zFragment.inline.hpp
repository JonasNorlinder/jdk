#ifndef SHARE_GC_Z_ZFRAGMENT_INLINE_HPP
#define SHARE_GC_Z_ZFRAGMENT_INLINE_HPP

#include "gc/z/zFragment.hpp"
#include "gc/z/zPage.inline.hpp"
#include "gc/z/zAttachedArray.inline.hpp"
#include "gc/z/zFragment.hpp"
#include "gc/z/zFragmentEntry.inline.hpp"
#include "gc/z/zHeap.hpp"
#include "gc/z/zHash.inline.hpp"
#include "runtime/atomic.hpp"
#include "gc/z/zThread.inline.hpp"
#include <iostream>

inline ZPage* ZFragment::new_page(uintptr_t from_offset) {
  if (!_new_page) {
    alloc_page(&_new_page);
  }
  return _new_page;
}

inline void ZFragment::alloc_page(ZPage** page) {
  ZHeap* heap = ZHeap::heap();
  ZAllocationFlags flags;
  flags.set_relocation();
  flags.set_non_blocking();

  if (ZThread::is_worker()) {
    flags.set_worker_thread();
  }

  ZPage* p = heap->alloc_page(_page_type, _page_size, flags);
  assert(p != NULL, "out-of-memory handling not supported yet");
  p->set_top(_page_size);
  // Get NULL => success
  ZPage* page_prev = Atomic::cmpxchg(page, (ZPage*)NULL, p);
  if (page_prev) {
    heap->undo_alloc_page(p);
  }
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

inline size_t ZFragment::offset_to_index(uintptr_t from_offset) const {
  return (from_offset - _ops) >> 8;
}

inline size_t ZFragment::offset_to_internal_index(uintptr_t from_offset) const {
  return ((from_offset - _ops) >> 3) & 31;
}

inline uintptr_t ZFragment::from_offset(size_t entry_index, size_t internal_index) const {
  return _ops + (entry_index << 8) + (internal_index << 3);
}

inline ZFragmentEntry* ZFragment::find(uintptr_t from_offset) const {
  return entries_begin() + offset_to_index(from_offset);
}

inline uintptr_t ZFragment::to_offset(uintptr_t from_offset) {
  return to_offset(from_offset, find(from_offset));
}

inline uintptr_t ZFragment::to_offset(uintptr_t from_offset, ZFragmentEntry* entry) {
  size_t live_bytes_before_fragment = entry->live_bytes_before_fragment();

  return
    new_page(from_offset)->start() +
    live_bytes_before_fragment +
    entry->live_bytes_on_fragment(_ops, from_offset, this);
}

#endif // SHARE_GC_Z_ZFRAGMENT_INLINE_HPP
