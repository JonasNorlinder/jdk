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
  const size_t            _object_alignment_shift;
  ZPage*                  _old_page;
  const uintptr_t         _ops;
  const ZVirtualMemory    _old_virtual;
  size_t                  _offset0;
  ZPage*                  _new_page0;
  ZPage*                  _new_page1;
  uintptr_t               _last_obj_page0;
  size_t                  _last_entry_index;
  size_t                  _last_internal_index;
  volatile uint32_t       _refcount;
  volatile bool           _pinned;
  bool                    _lei_updated;
  bool                    _lii_updated;
  uint64_t                _conversion_constant;

  bool inc_refcount();
  bool dec_refcount();

  bool is_in_page0(uintptr_t offset) const;

  ZFragment(ZPage* old_page, size_t nentries, size_t n_sizeentries);

public:
  static ZFragment*  create(ZPage* old_page);
  static void        destroy(ZFragment* fragment);

  const uintptr_t old_start();
  const size_t old_size();
  ZPage* old_page() const;
  ZPage* new_page(uintptr_t offset) const;
  uintptr_t new_page_start(uintptr_t offset) const;


  ZPage* get_new_page0();
  ZPage* get_new_page1();
  void set_new_page0(ZPage* p);
  void set_new_page1(ZPage* p);
  void set_offset0(size_t size);
  void set_last_obj_page0(uintptr_t addr);
  uintptr_t get_last_obj_page0();
  void set_last_entry_index(size_t index);
  void set_last_internal_index(size_t index);
  size_t get_last_entry_index();
  size_t get_last_internal_index();

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

  size_t entries_count() const;
  ZFragmentEntry* entries_begin() const;
  ZFragmentEntry* entries_end();

  uintptr_t page_index(uintptr_t offset);
  ZSizeEntry* size_entries_begin() const;

  void calc_fragments_live_bytes();
};

#endif // SHARE_GC_Z_ZFRAGMENT_HPP
