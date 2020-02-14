#ifndef SHARE_GC_Z_ZFRAGMENTTABLE_HPP
#define SHARE_GC_Z_ZFRAGMENTTABLE_HPP

#include "gc/z/zFragmentEntry.hpp"
#include "gc/z/zAttachedArray.hpp"
#include "gc/z/zVirtualMemory.hpp"
#include "gc/z/zPage.inline.hpp"

class ZFragment {
private:
  typedef ZAttachedArray<ZFragment, ZFragmentEntry> AttachedArray;

  const AttachedArray   _entries;
  const size_t          _object_alignment_shift;
  const ZPage*          _old_page;
  const ZVirtualMemory  _old_virtual;
  const ZPage*          _new_page;
  volatile uint32_t     _refcount;
  volatile bool         _pinned;
  uint64_t              _conversion_constant;

  bool inc_refcount();
  bool dec_refcount();

  ZFragment(ZPage* old_page, ZPage* new_page, size_t nentries);

public:
  static ZFragment*  create(ZPage* old_page);
  static void        destroy(ZFragment* fragment);

  const uintptr_t old_start();
  const size_t old_size();
  const ZPage* old_page() const;
  const ZPage* new_page();
  void fill_entires();

  ZFragmentEntry* find(uintptr_t from_addr) const;

  bool is_pinned() const;
  void set_pinned();

  bool retain_page();
  void release_page();

  const size_t entries_count();
  ZFragmentEntry* entries_begin();
  ZFragmentEntry* entries_end();

  void calc_fragments_live_bytes();
};

#endif // SHARE_GC_Z_ZFRAGMENTTABLE_HPP
