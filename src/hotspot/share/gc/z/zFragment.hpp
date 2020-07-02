#ifndef SHARE_GC_Z_ZFRAGMENT_HPP
#define SHARE_GC_Z_ZFRAGMENT_HPP

#include "gc/z/zFragmentEntry.hpp"
#include "gc/z/zAttachedArray.hpp"
#include "gc/z/zVirtualMemory.hpp"

class ZPage;

class ZFragment {
  friend class ZFragmentEntry;
  friend class ZRelocate;

private:
  typedef ZAttachedArray<ZFragment, ZFragmentEntry> AttachedArray;

  const AttachedArray     _entries;
  const size_t            _object_alignment_shift;
  ZPage*                  _old_page;
  const uintptr_t         _ops;
  const uint8_t           _page_type;
  const size_t            _page_size;
  const ZVirtualMemory    _old_virtual;
  ZPage*                  _new_page;
  volatile uint32_t       _refcount;

  bool inc_refcount();
  bool dec_refcount();

  void alloc_page(ZPage** page);

  ZFragment(ZPage* old_page, size_t nentries);

public:
  static ZFragment*  create(ZPage* old_page);
  static void        destroy(ZFragment* fragment);

  const uintptr_t old_start();
  const size_t old_size();
  ZPage* old_page() const;
  ZPage* new_page(uintptr_t from_offset);

  ZFragmentEntry* find(uintptr_t from_addr) const;
  uintptr_t to_offset(uintptr_t from_offset);
  uintptr_t to_offset(uintptr_t from_offset, ZFragmentEntry* entry);
  uintptr_t from_offset(size_t entry_index, size_t internal_index) const;
  size_t offset_to_index(uintptr_t from_offset) const;
  size_t offset_to_internal_index(uintptr_t from_offset) const;

  bool retain_page();
  void release_page();

  size_t entries_count() const;
  ZFragmentEntry* entries_begin() const;
  ZFragmentEntry* entries_end();
};

#endif // SHARE_GC_Z_ZFRAGMENT_HPP
