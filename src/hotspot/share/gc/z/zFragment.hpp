#ifndef SHARE_GC_Z_ZFRAGMENT_HPP
#define SHARE_GC_Z_ZFRAGMENT_HPP

#include "gc/z/zFragmentEntry.hpp"
#include "gc/z/zAttachedArray.hpp"
#include "gc/z/zVirtualMemory.hpp"

class ZPage;

class ZFragment {
private:
  typedef ZAttachedArray<ZFragment, ZFragmentEntry> AttachedArray;

  const AttachedArray     _entries;
  const size_t            _object_alignment_shift;
  ZPage*                  _old_page;
  const uintptr_t         _ops;
  const uint8_t           _page_type;
  ZFragment*              _previous_fragment;
  const size_t            _page_size;
  const ZVirtualMemory    _old_virtual;
  ZPage*                  _new_page;
  ZPage*                  _snd_page;
  volatile uint32_t       _refcount;
  size_t _first_from_offset_mapped_to_snd_page;
  size_t _page_break_entry_index;

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
  const ZPage* new_page(uintptr_t from_offset);

  ZFragmentEntry* find(const uintptr_t from_addr);
  const uintptr_t to_offset(const uintptr_t from_offset);
  const uintptr_t to_offset(const uintptr_t from_offset, const ZFragmentEntry* entry);
  uintptr_t from_offset(size_t entry_index, size_t internal_index) const;
  size_t offset_to_index(uintptr_t from_offset) const;
  size_t offset_to_internal_index(uintptr_t from_offset) const;

  bool retain_page();
  void release_page();

  ZFragmentEntry* entries_begin() const;
  ZFragmentEntry* entries_end();

  ZPage* last_page();
  bool continues_from_previous_fragment() const;
  void add_page_break(uintptr_t first_on_snd, ZFragment* previous);
};

#endif // SHARE_GC_Z_ZFRAGMENT_HPP
