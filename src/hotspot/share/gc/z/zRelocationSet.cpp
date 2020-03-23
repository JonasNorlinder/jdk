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
#include "gc/z/zFragment.inline.hpp"
#include "gc/z/zRelocationSet.hpp"
#include "gc/z/zAllocationFlags.hpp"
#include "gc/z/zHeap.inline.hpp"
#include "memory/allocation.hpp"
#include <iostream>

class ZLiveMapIterator : public ObjectClosure {
private:
  ZFragment* _fragment;
  ZPage *_curr;
  ZPage *_old_page;
  ZAllocationFlags _flags;
public:
  ZLiveMapIterator(ZFragment* fragment, ZPage *curr, ZPage *old_page, ZAllocationFlags flags) :
    _fragment(fragment), _curr(curr), _old_page(old_page), _flags(flags) {}

  void do_object(oop obj) override {
    uintptr_t offset = ZOop::to_address(obj);
    uintptr_t addr = ZAddress::good(offset);
      
    const size_t size = ZUtils::object_size(addr);
    if (_curr->remaining() < size) {
      _curr = ZHeap::heap()->alloc_page(_old_page->type(), _old_page->size(), _flags, true /* don't change top */);

      if (_fragment->get_new_page0() == NULL) {
        _fragment->set_new_page0(_curr);
      } else {
        _fragment->set_last_obj_page0(addr);
        _fragment->set_new_page1(_curr);
      }
    }

    assert(_fragment->get_new_page0() != NULL, "");
    _fragment->new_page(offset)->inc_top(size);
  }
};

ZPage* ZRelocationSet::alloc_object_iterator(ZFragment* fragment, ZPage* prev) {
  ZPage* old_page = fragment->old_page();
  size_t nsegments = old_page->_livemap.nsegments;

  ZAllocationFlags flags;
  flags.set_relocation();
  flags.set_non_blocking();
  flags.set_worker_thread();

  ZPage* curr = NULL;
  if (prev == NULL) {
    curr = ZHeap::heap()->alloc_page(old_page->type(), old_page->size(), flags, true /* don't change top */);
    fragment->set_new_page0(curr);
    fragment->set_offset0(curr->top() - curr->start());
  } else {
    curr = prev;
    fragment->set_new_page0(curr);
    fragment->set_offset0(curr->top() - curr->start());
  }

  ZLiveMapIterator cl = ZLiveMapIterator(fragment, curr, old_page, flags);
  curr->_livemap.iterate(&cl, curr->start(), curr->object_alignment_shift());
  
  for (BitMap::idx_t segment = old_page->_livemap.first_live_segment(); segment < nsegments; segment = old_page->_livemap.next_live_segment(segment)) {
    // For each live segment
    const BitMap::idx_t start_index = old_page->_livemap.segment_start(segment);
    const BitMap::idx_t end_index   = old_page->_livemap.segment_end(segment);

    BitMap::idx_t index = old_page->_livemap._bitmap.get_next_one_offset(start_index, end_index);
    
    while (index < end_index) {
      // Calculate object address
      const uintptr_t offset = old_page->start() + ((index / 2) << old_page->object_alignment_shift());
      const uintptr_t addr = ZAddress::good(offset);

      // Apply closure
      uintptr_t prev_obj = addr;
      const size_t size = ZUtils::object_size(addr);
      if (curr->remaining() < size) {
        curr = ZHeap::heap()->alloc_page(old_page->type(), old_page->size(), flags, true /* don't change top */);

        if (fragment->get_new_page0() == NULL) {
          fragment->set_new_page0(curr);
        } else {
          fragment->set_last_obj_page0(prev_obj);
          fragment->set_new_page1(curr);
        }
      }

      assert(fragment->get_new_page0() != NULL, "");
      fragment->new_page(offset)->inc_top(size);

      // Find next bit after this object
      const uintptr_t next_addr = align_up(addr + size, 1 << old_page->object_alignment_shift());
      const BitMap::idx_t next_index = ((next_addr - ZAddress::good(old_page->start())) >> old_page->object_alignment_shift()) * 2;
      if (next_index >= end_index) {
        // End of live map
        break;
      }

      index = old_page->_livemap._bitmap.get_next_one_offset(next_index, end_index);
    }
  }
  return curr;
}

ZRelocationSet::ZRelocationSet() :
    _fragments(NULL),
    _nfragments(0) {}

void ZRelocationSet::populate(ZPage* const* group0, size_t ngroup0,
                              ZPage* const* group1, size_t ngroup1) {
  _nfragments = ngroup0 + ngroup1;
  _fragments = REALLOC_C_HEAP_ARRAY(ZFragment*, _fragments, _nfragments, mtGC);

  size_t j = 0;

  ZHeap* heap = ZHeap::heap();
  ZAllocationFlags flags;
  flags.set_relocation();
  flags.set_non_blocking();
  flags.set_worker_thread();

  // Populate group 0 (medium)
  ZPage* prev = NULL;
  for (size_t i = 0; i < ngroup0; i++) {
    ZPage* old_page = group0[i];
    ZFragment* fragment= ZFragment::create(old_page);
    prev = alloc_object_iterator(fragment, prev);
    _fragments[j++] = fragment;
  }

  // Populate group 1 (small)
  prev = NULL;
  for (size_t i = 0; i < ngroup1; i++) {
    ZPage* old_page = group1[i];
    ZFragment* fragment= ZFragment::create(old_page);
    prev = alloc_object_iterator(fragment, prev);
    _fragments[j++] = fragment;
  }
}

void ZRelocationSet::reset() {
  for (size_t i = 0; i < _nfragments; i++) {
    ZFragment::destroy(_fragments[i]);
    _fragments[i] = NULL;
  }
}

