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

class ZLiveMapIteratorPost : public ObjectClosure {
private:
  ZHeap* _heap;
  ZFragment* _fragment;
  ZFragmentEntry *_current_entry;
  size_t _top;

public:
  ZLiveMapIteratorPost(ZFragment* fragment) :
    _heap(ZHeap::heap()),
    _fragment(fragment),
    _current_entry(NULL),
    _top(0) {}

  virtual void do_object(oop obj) {
    const uintptr_t from_offset = ZAddress::offset(ZOop::to_address(obj));
    std::cout << "OBJECT (" << from_offset << ")";
    const size_t obj_size = ZUtils::object_size(ZAddress::good(from_offset));
    std::cout << " size = " << obj_size << std::endl;
    const size_t internal_index = _fragment->offset_to_internal_index(from_offset);
    ZFragmentEntry* entry_for_offset = _fragment->find(from_offset);

    _fragment->_objects++;

    // Copy liveness information
    entry_for_offset->set_liveness(internal_index);

    // Store object size
    entry_for_offset->set_size_bit(internal_index, obj_size);

    // Allocate for object
    if (_current_entry < entry_for_offset) {
      _current_entry = entry_for_offset;
      _current_entry->set_live_bytes_before_fragment(_top);
    }
    _heap->add_expected(from_offset, _top);
    assert(_top == _heap->get_expected(from_offset), "");
    _top += obj_size;
  }
};

class ZLiveMapIterator : public ObjectClosure {
private:
  ZHeap* _heap;
  ZFragment* _fragment;
  ZPage *_current_page;
  ZAllocationFlags _flags;
  ZFragmentEntry *_current_entry;

public:
  ZLiveMapIterator(ZFragment* fragment, ZPage* new_page, ZAllocationFlags flags) :
    _heap(ZHeap::heap()),
    _fragment(fragment),
    _current_page(new_page),
    _flags(flags),
    _current_entry(fragment->entries_begin())
  {
    _current_entry->set_live_bytes_before_fragment(_current_page->top() - _current_page->start());
  }

  ZPage *current_page() const {
    return _current_page;
  }

  virtual void do_object(oop obj) {
    // _heap->add_expected(652214272, 0);
    // assert(0 == _heap->get_expected(652214272), "");
    // assert(false, "") ;
    const uintptr_t from_offset = ZAddress::offset(ZOop::to_address(obj));
    const size_t obj_size = ZUtils::object_size(ZAddress::good(from_offset));
    const size_t internal_index = _fragment->offset_to_internal_index(from_offset);
    ZFragmentEntry* entry_for_offset = _fragment->find(from_offset);

    // Copy liveness information
    entry_for_offset->set_liveness(internal_index);

    // Store object size
    entry_for_offset->set_size_bit(internal_index, obj_size);

    // Allocate for object
    if (_current_entry < entry_for_offset) {
      _current_entry = entry_for_offset;
      _current_entry->set_live_bytes_before_fragment(_current_page->top() - _current_page->start());
    }

    uintptr_t allocated_obj = _current_page->alloc_object(obj_size);

    if (allocated_obj == 0) {
      _current_page = ZHeap::heap()->alloc_page(_current_page->type(), _current_page->size(), _flags);
      allocated_obj = _current_page->alloc_object(obj_size);
      _fragment->add_page_break(_current_page, from_offset);
    }
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
  // ZPage* current_new_page = ngroup0 > 0 ? ZHeap::heap()->alloc_page(group0[0]->type(), group0[0]->size(), flags) : NULL;
  // for (size_t i = 0; i < ngroup0; i++) {
  //   ZPage* old_page = group0[i];
  //   ZFragment* fragment = ZFragment::create(old_page, current_new_page);

  //   ZLiveMapIterator cl = ZLiveMapIterator(fragment, current_new_page);
  //   old_page->_livemap.iterate(&cl, ZAddress::good(old_page->start()), old_page->object_alignment_shift());
  //   current_new_page = cl.current_page();
  //   _fragments[fragment_index++] = fragment;
  // }

  // POST ALLOCATE
  std::cout << "# evicted medium pages = " << ngroup0 << std::endl;
  for (size_t i = 0; i < ngroup0; i++) {
    std::cout << "page" << i << std::endl;
    ZPage* old_page = group0[i];
    ZFragment* fragment = ZFragment::create(old_page, NULL);
    ZLiveMapIteratorPost cl = ZLiveMapIteratorPost(fragment);
    old_page->_livemap.iterate(&cl, ZAddress::good(old_page->start()), old_page->object_alignment_shift());
    _fragments[fragment_index++] = fragment;
  }

  // Populate group 1 (small)
  ZPage* current_new_page = ngroup1 > 0 ? ZHeap::heap()->alloc_page(group1[0]->type(), group1[0]->size(), flags) : NULL;
  for (size_t i = 0; i < ngroup1; i++) {
    ZPage* old_page = group1[i];
    ZFragment* fragment = ZFragment::create(old_page, current_new_page);

    ZLiveMapIterator cl = ZLiveMapIterator(fragment, current_new_page, flags);
    old_page->_livemap.iterate(&cl, ZAddress::good(old_page->start()), old_page->object_alignment_shift());
    current_new_page = cl.current_page();
    _fragments[fragment_index++] = fragment;
  }

  // Post allocate
  // for (size_t i = 0; i < ngroup1; i++) {
  //   ZPage* old_page = group1[i];
  //   ZFragment* fragment = ZFragment::create(old_page, NULL);

  //   ZLiveMapIteratorPost cl = ZLiveMapIteratorPost(fragment, flags);
  //   old_page->_livemap.iterate(&cl, ZAddress::good(old_page->start()), old_page->object_alignment_shift());
  //   _fragments[fragment_index++] = fragment;
  // }
}

void ZRelocationSet::reset() {
  for (size_t i = 0; i < _nfragments; i++) {
    ZFragment::destroy(_fragments[i]);
    _fragments[i] = NULL;
  }
}
