/*
 * Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "precompiled.hpp"
#include "gc/z/zFragment.hpp"
#include "gc/z/zRelocationSet.hpp"
#include "gc/z/zAllocationFlags.hpp"
#include "gc/z/zHeap.inline.hpp"
#include "memory/allocation.hpp"
#include <unordered_map>
#include <vector>

class ZLiveMapIterator : public ObjectClosure {
private:
  ZHeap* _heap;
  ZFragment* _fragment;
  ZPage *_current_page;
  ZAllocationFlags _flags;
  ZFragmentEntry *_current_entry;
  uintptr_t _first_address_in_entry;

  MyArena a;
  MyArena lost_objects_arena;
  using map_allocator = MyAllocator<std::pair<uintptr_t, uintptr_t> >;
  using ptr_to_ptr_t = map<uintptr_t, uintptr_t,
                           std::less<uintptr_t>, map_allocator>;
  ptr_to_ptr_t tmp_object_remaped{std::less<uintptr_t>(), map_allocator{a}};

public:
  ZLiveMapIterator(ZFragment* fragment, ZPage* new_page, ZAllocationFlags flags) :
    _heap(ZHeap::heap()),
    _fragment(fragment),
    _current_page(new_page),
    _flags(flags),
    _current_entry(fragment->entries_begin())
  {
    _current_entry->set_live_bytes(_current_page->top() - _current_page->start());
  }

  ZPage *current_page() const {
    return _current_page;
  }

  /// BEGIN DEBUG
  void install_last_entry() {
    auto h = ZHeap::heap();
    h->global_lock.lock();
    for (auto it : tmp_object_remaped) {
      h->update_expected(it.first, it.second);
    }
    tmp_object_remaped.clear();
    h->global_lock.unlock();
  }

  void move_current_entry() {
    auto h = ZHeap::heap();
    h->global_lock.lock();
    for (auto it : tmp_object_remaped) {
      size_t obj_size = ZUtils::object_size(ZAddress::good(it.first));

      uintptr_t new_address = _current_page->alloc_object(obj_size);

      h->update_expected(it.first, ZAddress::offset(new_address));
    }
    tmp_object_remaped.clear();
    h->global_lock.unlock();
  }
  /// END DEBUG

  virtual void do_object(oop obj) {
    uintptr_t from_offset = ZAddress::offset(ZOop::to_address(obj));
    size_t obj_size = ZUtils::object_size(ZAddress::good(from_offset));

    ZFragmentEntry* entry_for_offset = _fragment->find(from_offset);

    if (_current_entry < entry_for_offset) {
      /// Advance _current_entry_pointer
      _current_entry = entry_for_offset;
      _current_entry->set_live_bytes(_current_page->top() - _current_page->start());

      /// BEGIN DEBUG
      install_last_entry();
      /// END DEBUG

      _first_address_in_entry = from_offset;
    }

    uintptr_t allocated_obj = _current_page->alloc_object(obj_size);

    /// If allocation did not fit on page, move entire current entry to next page
    if (allocated_obj == 0) {
      size_t bytes_allocated_on_previous_page = _current_page->top() - _current_entry->get_live_bytes();

      _current_page = ZHeap::heap()->alloc_page(_current_page->type(), _current_page->size(), _flags);
      _fragment->add_page_break(_current_page, _first_address_in_entry);

      /// Reset live byte counter for current entry
      _current_entry->set_live_bytes(0);

      /// BEGIN DEBUG
      /// NOTE -- when removed, add bytes_allocated_on_previous_page to obj_size
      if (true) {
        move_current_entry();
      } else {
        /// END DEBUG
        obj_size += bytes_allocated_on_previous_page;
      }
      allocated_obj = _current_page->alloc_object(obj_size);
    }

    /// BEGIN DEBUG
    tmp_object_remaped[from_offset] = ZAddress::offset(allocated_obj);
    /// END DEBUG

  }
};

ZRelocationSet::ZRelocationSet() :
  _fragments(NULL),
  _nfragments(0) {}

void ZRelocationSet::populate(ZPage* const* group0, size_t ngroup0,
                              ZPage* const* group1, size_t ngroup1) {
  _nfragments = ngroup0 + ngroup1;
  _fragments = REALLOC_C_HEAP_ARRAY(ZFragment*, _fragments, _nfragments, mtGC);

  size_t fragment_index = 0;

  ZAllocationFlags flags;
  flags.set_relocation();
  flags.set_non_blocking();
  flags.set_worker_thread();

  // Populate group 0 (medium)
  ZPage* current_new_page = ngroup0 > 0 ? ZHeap::heap()->alloc_page(group0[0]->type(), group0[0]->size(), flags) : NULL;
  for (size_t i = 0; i < ngroup0; i++) {
    ZPage* old_page = group0[i];
    ZFragment* fragment = ZFragment::create(old_page, current_new_page);

    ZLiveMapIterator cl = ZLiveMapIterator(fragment, current_new_page, flags);
    old_page->_livemap.iterate(&cl, ZAddress::good(old_page->start()), old_page->object_alignment_shift());
    current_new_page = cl.current_page();
    /// BEGIN DEBUG
    cl.install_last_entry();
    /// END DEBUG

    _fragments[fragment_index++] = fragment;
  }

  // Populate group 1 (small)
  current_new_page = ngroup1 > 0 ? ZHeap::heap()->alloc_page(group1[0]->type(), group1[0]->size(), flags) : NULL;
  for (size_t i = 0; i < ngroup1; i++) {
    ZPage* old_page = group1[i];
    ZFragment* fragment = ZFragment::create(old_page, current_new_page);

    ZLiveMapIterator cl = ZLiveMapIterator(fragment, current_new_page, flags);
    old_page->_livemap.iterate(&cl, ZAddress::good(old_page->start()), old_page->object_alignment_shift());
    current_new_page = cl.current_page();
    /// BEGIN DEBUG
    cl.install_last_entry();
    /// END DEBUG

    _fragments[fragment_index++] = fragment;
  }
}

void ZRelocationSet::reset() {
  for (size_t i = 0; i < _nfragments; i++) {
    ZFragment::destroy(_fragments[i]);
    _fragments[i] = NULL;
  }
}
