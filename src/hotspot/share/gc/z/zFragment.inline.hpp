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
#include <iostream>

inline void ZFragment::set_new_page(ZPage* page) {
  _new_page = page;
}

inline ZPage* ZFragment::new_page(uintptr_t from_offset) const {
  if (_snd_page && from_offset >= _first_from_offset_mapped_to_snd_page) {
    return _snd_page;
  } else {
    return _new_page;
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

inline uintptr_t ZFragment::to_offset(uintptr_t from_offset) {
  return to_offset(from_offset, find(from_offset));
}

inline uintptr_t ZFragment::to_offset(uintptr_t from_offset, ZFragmentEntry* entry) {


  uintptr_t r = new_page(from_offset)->start() + entry->get_live_bytes() + entry->count_live_objects(_ops, from_offset, this);

  ZHeap *h = ZHeap::heap();
  if (h->get_expected(from_offset) != r) {
    h->global_lock.lock();

    std::cerr
      << std::hex
      << " (a) "
      << new_page(from_offset)->start()
      << " (b) "
      << entry->get_live_bytes()
      << " (c) "
      << entry->count_live_objects(_ops, from_offset, this)
      << " (d) "
      << _new_page->start()
      << " (e) "
      << (_snd_page ? _snd_page->start() : -1)
      << " (f) "
      << r
      << " (g) "
      << h->get_expected(from_offset)
      << " (h) "
      << entry - entries_begin()
      << " (i) "
      << from_offset
      << " (j) "
      << entry->fragment_internal_index(_ops, from_offset)
      << "\n";
    h->global_lock.unlock();
    assert(false, "boom");
  }


  return
    new_page(from_offset)->start() +
    entry->get_live_bytes() +
    entry->count_live_objects(_ops, from_offset, this);
}

inline void ZFragment::add_page_break(ZPage *snd_page, uintptr_t first_on_snd) {
  _snd_page = snd_page;
  _first_from_offset_mapped_to_snd_page = first_on_snd;
}


#endif // SHARE_GC_Z_ZFRAGMENT_INLINE_HPP
