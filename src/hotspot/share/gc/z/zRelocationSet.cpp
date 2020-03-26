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
  ZHeap* _heap;
  ZFragment* _fragment;
  ZPage *_curr;
  ZAllocationFlags _flags;
  uintptr_t _prev;
public:
  ZLiveMapIterator(ZFragment* fragment, ZAllocationFlags flags) :
    _heap(ZHeap::heap()),
    _fragment(fragment),
    _curr(fragment->get_new_page0()),
    _flags(flags),
    _prev(0)
  {}

  ZPage* curr() {
    return _curr;
  }

  virtual void do_object(oop obj) {
    uintptr_t offset = ZAddress::offset(ZOop::to_address(obj));
    uintptr_t addr = ZAddress::good(offset);

    uintptr_t allocated_obj = (bool) _curr->alloc_object(ZUtils::object_size(addr));

    if (allocated_obj) {
      _prev = addr;
      _curr->inc_attached_old_pages();
      assert(_curr->is_in(allocated_obj), "");
    } else {
      /// Overshooting -- allocation does not fit on current page
      ZPage* prev_curr = _curr;
      _curr = _heap->alloc_page(_fragment->_old_page->type(), _fragment->_old_page->size(), _flags, false);
      allocated_obj = _curr->alloc_object(ZUtils::object_size(addr));
      _curr->inc_attached_old_pages();
      assert(_curr->is_in(allocated_obj), "");

      if (_prev > 0) {
        _fragment->set_last_obj_page0(_prev);
        _fragment->set_new_page1(_curr);
      }
    }

    _heap->global_lock.lock();
    _heap->add_expected(offset, ZAddress::offset(allocated_obj));
    _heap->global_lock.unlock();
  }
};

ZPage* ZRelocationSet::alloc_object_iterator(ZFragment* fragment, ZPage* prev) {
  ZPage* old_page = fragment->old_page();

  ZAllocationFlags flags;
  flags.set_relocation();
  flags.set_non_blocking();
  flags.set_worker_thread();

  ZPage* curr = NULL;
  if (prev == NULL) {
    curr = ZHeap::heap()->alloc_page(old_page->type(), old_page->size(), flags, false);
  } else {
    curr = prev;
  }
  fragment->set_new_page0(curr);

  ZLiveMapIterator cl = ZLiveMapIterator(fragment, flags);
  old_page->_livemap.iterate(&cl, ZAddress::good(old_page->start()), old_page->object_alignment_shift());
  curr = cl.curr();
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
    //prev = alloc_object_iterator(fragment, prev);

    // Simply for now and to simplifed allocation for
    // medium pages
    ZPage* new_page = ZHeap::heap()->alloc_page(old_page->type(), old_page->size(), flags, true /* don't change top */);
    fragment->set_new_page0(new_page);
   new_page->inc_top(new_page->remaining());
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
