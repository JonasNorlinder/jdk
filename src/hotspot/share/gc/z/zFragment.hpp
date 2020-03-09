#ifndef SHARE_GC_Z_ZFRAGMENT_HPP
#define SHARE_GC_Z_ZFRAGMENT_HPP

#include "gc/z/zSizeEntry.hpp"
#include "gc/z/zFragmentEntry.hpp"
#include "gc/z/zAttachedArray.hpp"
#include "gc/z/zVirtualMemory.hpp"

class ZPage;

class ZFragment {
  friend class ZFragmentEntry;
private:
  typedef ZAttachedArray<ZFragment, ZFragmentEntry> AttachedArray;
  typedef ZAttachedArray<AttachedArray, ZSizeEntry> SizeAttachedArray;

  const AttachedArray     _entries;
  const SizeAttachedArray _size_entries;
  const size_t            _object_alignment_shift;
  const ZPage*            _old_page;
  const ZVirtualMemory    _old_virtual;
  const ZPage*            _new_page;
  volatile uint32_t       _refcount;
  volatile bool           _pinned;
  uint64_t                _conversion_constant;

  bool inc_refcount();
  bool dec_refcount();

  ZFragment(ZPage* old_page, ZPage* new_page, size_t nentries, size_t n_sizeentries);

public:
  static ZFragment*  create(ZPage* old_page);
  static void        destroy(ZFragment* fragment);

  const uintptr_t old_start();
  const size_t old_size();
  const ZPage* old_page() const;
  const ZPage* new_page();
  void fill_entires();

  ZFragmentEntry* find(uintptr_t from_addr) const;
  uintptr_t to_offset(uintptr_t from_offset);
  uintptr_t to_offset(uintptr_t from_offset, ZFragmentEntry* entry);
  uintptr_t from_offset(size_t entry_index, size_t internal_index) const;
  size_t offset_to_index(uintptr_t from_offset) const;
  size_t offset_to_internal_index(uintptr_t from_offset) const;

  bool is_pinned() const;
  void set_pinned();

  bool retain_page();
  void release_page();

  const size_t entries_count();
  ZFragmentEntry* entries_begin() const;
  ZFragmentEntry* entries_end();

  uintptr_t page_index(uintptr_t offset);
  ZSizeEntry* size_entries_begin() const;

  void calc_fragments_live_bytes();
};

#endif // SHARE_GC_Z_ZFRAGMENT_HPP
