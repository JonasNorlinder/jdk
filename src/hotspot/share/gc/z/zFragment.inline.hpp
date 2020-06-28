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
    assert(_new_page, "out-of-memory not handled yet");
    return _new_page;
  }

  if (from_offset >= _first_from_offset_mapped_to_snd_page &&
      _first_from_offset_mapped_to_snd_page > 0) {
    if (!_snd_page) {
      alloc_page(&_snd_page);
      assert(_snd_page, "out-of-memory not handled yet");
    }
    return _snd_page;
  } else {
    if (!_new_page && continues_from_previous_fragment()) {
      // Case 1: Overlapping
      _new_page = _previous_fragment->last_page();
      assert(false, "");
    }
    else {
      // Case 0: I am first
      alloc_page(&_new_page);
      assert(_new_page, "out-of-memory not handled yet");
    }
  }
  assert(false, "");
  return _new_page;
}

inline void ZFragment::reset() {
  _previous_fragment = NULL;
}

inline bool ZFragment::continues_from_previous_fragment() const {
  return _previous_fragment != NULL;
}

inline ZPage* ZFragment::last_page() {
  return new_page(_first_from_offset_mapped_to_snd_page);
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

inline size_t ZFragment::entries_count() const {
  return _entries.length();
}

inline ZFragmentEntry* ZFragment::entries_begin() const {
  return _entries(this);
}

inline ZFragmentEntry* ZFragment::entries_end() {
  return entries_begin() + _entries.length();
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

inline uintptr_t ZFragment::to_offset(uintptr_t from_offset) {
  return to_offset(from_offset, find(from_offset));
}

inline size_t ZFragment::page_break_entry_index() const {
  assert(_first_from_offset_mapped_to_snd_page > 0, "");
  return _page_break_entry_index;
}

inline size_t ZFragment::page_break_entry_internal_index() const {
  assert(_first_from_offset_mapped_to_snd_page > 0, "");
  return _page_break_entry_internal_index;
}

inline bool ZFragment::is_on_snd_page(uintptr_t from_offset) const {
  return from_offset >= _first_from_offset_mapped_to_snd_page && _first_from_offset_mapped_to_snd_page;
}

inline bool ZFragment::is_on_page_break(ZFragmentEntry *entry) {
  return (entry == entries_begin() + _page_break_entry_index) && _first_from_offset_mapped_to_snd_page;
}

inline uintptr_t ZFragment::to_offset(uintptr_t from_offset, ZFragmentEntry* entry) {
  size_t live_bytes_before_fragment = is_on_page_break(entry) && is_on_snd_page(from_offset) ?
    0 : entry->live_bytes_before_fragment();

  return
    new_page(from_offset)->start() +
    live_bytes_before_fragment +
    entry->live_bytes_on_fragment(_ops, from_offset, this);
}

inline void ZFragment::add_page_break(uintptr_t first_on_snd) {
  _first_from_offset_mapped_to_snd_page = first_on_snd;
  _page_break_entry_index = offset_to_index(first_on_snd);
  _page_break_entry_internal_index = offset_to_internal_index(first_on_snd);
}

#endif // SHARE_GC_Z_ZFRAGMENT_INLINE_HPP
