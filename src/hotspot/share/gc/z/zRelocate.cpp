/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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
#include "gc/z/zAddress.inline.hpp"
#include "gc/z/zBarrier.inline.hpp"
#include "gc/z/zFragment.inline.hpp"
#include "gc/z/zFragmentEntry.inline.hpp"
#include "gc/z/zHeap.hpp"
#include "gc/z/zOopClosures.inline.hpp"
#include "gc/z/zPage.hpp"
#include "gc/z/zRelocate.hpp"
#include "gc/z/zRelocationSet.inline.hpp"
#include "gc/z/zRootsIterator.hpp"
#include "gc/z/zStat.hpp"
#include "gc/z/zTask.hpp"
#include "gc/z/zThread.inline.hpp"
#include "gc/z/zThreadLocalAllocBuffer.hpp"
#include "gc/z/zWorkers.hpp"
#include "logging/log.hpp"
#include <iostream>

static const ZStatCounter ZCounterRelocationContention("Contention", "Relocation Contention", ZStatUnitOpsPerSecond);

ZRelocate::ZRelocate(ZWorkers* workers) :
    _workers(workers) {}

class ZRelocateRootsIteratorClosure : public ZRootsIteratorClosure {
public:
  virtual void do_thread(Thread* thread) {
    // Update thread local address bad mask
    ZThreadLocalData::set_address_bad_mask(thread, ZAddressBadMask);

    // Relocate invisible root
    ZThreadLocalData::do_invisible_root(thread, ZBarrier::relocate_barrier_on_root_oop_field);

    // Remap TLAB
    ZThreadLocalAllocBuffer::remap(thread);
  }

  virtual bool should_disarm_nmethods() const {
    return true;
  }

  virtual void do_oop(oop* p) {
    ZBarrier::relocate_barrier_on_root_oop_field(p);
  }

  virtual void do_oop(narrowOop* p) {
    ShouldNotReachHere();
  }
};

class ZRelocateRootsTask : public ZTask {
private:
  ZRootsIterator                _roots;
  ZRelocateRootsIteratorClosure _cl;

public:
  ZRelocateRootsTask() :
      ZTask("ZRelocateRootsTask"),
      _roots(true /* visit_jvmti_weak_export */) {}

  virtual void work() {
    // During relocation we need to visit the JVMTI
    // export weak roots to rehash the JVMTI tag map
    _roots.oops_do(&_cl);
  }
};

void ZRelocate::start() {
  ZRelocateRootsTask task;
  _workers->run_parallel(&task);
}

uintptr_t last = 0;
uintptr_t ZRelocate::relocate_object_inner(ZFragment* fragment, uintptr_t from_offset) const {

  // Lookup fragment entry
  ZFragmentEntry* entry = fragment->find(from_offset);
  assert(entry != NULL, "");
  const uintptr_t to_offset = fragment->to_offset(from_offset, entry);

  //ssert(to_offset != last, "arrived");
  last = to_offset;

  // std::cerr << "\t" << std::hex << from_offset << " --> " << std::hex << to_offset << "\n";


  if (entry->copied()) {
    // Already relocated, return new address
    return to_offset;
  }
  assert(ZHeap::heap()->is_object_live(ZAddress::good(from_offset)), "Should be live");

  ZHeap* heap = ZHeap::heap();
  heap->global_lock.lock();
  if (entry->copied()) {
    // Another thread beat us to it
    // return new adress
    heap->global_lock.unlock();
    return to_offset;
  }

  uintptr_t prev = 0;
  uintptr_t x = fragment->new_page()->top();

  // Reallocate all live objects within fragment
  ZFragmentObjectCursor cursor = 0;
  int32_t live_index=-1;
  do {
    live_index = entry->get_next_live_object(&cursor);
    uintptr_t to_offset = fragment->to_offset(from_offset, entry);
    size_t p_index = fragment->page_index(from_offset);
    size_t size = (fragment->size_entries_begin() + p_index)->entry;

    assert(prev != to_offset, "BOOM!");
    prev = to_offset;

    assert(size > 0, "Size was zero");

    uintptr_t to_good = fragment->new_page()->alloc_object(size);
    assert(to_good != 0, "couldn't allocate space for object");

    assert(ZAddress::offset(to_good) == x, "BLAM");
    x += size;

    ZUtils::object_copy(ZAddress::good(from_offset),
                        to_good,
                        size);
  } while (live_index != -1);
  entry->set_copied();

  heap->global_lock.unlock();
  return to_offset;
}



uintptr_t ZRelocate::relocate_object(ZFragment* fragment, uintptr_t from_addr) const {
  const uintptr_t from_offset = ZAddress::offset(from_addr);
  const uintptr_t to_offset = relocate_object_inner(fragment, from_offset);

  ZFragmentEntry *e = fragment->find(from_offset);
  if (e->copied()) {
    return ZAddress::good(fragment->to_offset(from_offset, e));
  }

  if (from_offset == to_offset) {
    // In-place forwarding, pin page

    fragment->set_pinned();
  }

  return ZAddress::good(to_offset);
}

uintptr_t ZRelocate::forward_object(ZFragment* fragment, uintptr_t from_addr) const {
  return ZAddress::good(fragment->to_offset(from_addr));
}

class ZRelocateObjectClosure : public ObjectClosure {
private:
  ZRelocate* const   _relocate;
  ZFragment* const   _fragment;

public:
  ZRelocateObjectClosure(ZRelocate* relocate, ZFragment* fragment) :
      _relocate(relocate),
      _fragment(fragment) {}

  virtual void do_object(oop o) {
    _relocate->relocate_object(_fragment, ZOop::to_address(o));
  }
};

bool ZRelocate::work(ZRelocationSetParallelIterator* iter) {
  bool success = true;

  // Relocate pages in the relocation set
  for (ZFragment* fragment; iter->next(&fragment);) {
    // Relocate objects in page
    ZRelocateObjectClosure cl(this, fragment);
    fragment->old_page()->object_iterate(&cl);

    if (fragment->is_pinned()) {
      // Relocation failed, page is now pinned
      success = false;
    } else {
      // Relocation succeeded, release page
      std::cerr << "Released!";
      fragment->release_page();
    }
  }

  return success;
}

class ZRelocateTask : public ZTask {
private:
  ZRelocate* const               _relocate;
  ZRelocationSetParallelIterator _iter;
  bool                           _failed;

public:
  ZRelocateTask(ZRelocate* relocate, ZRelocationSet* relocation_set) :
      ZTask("ZRelocateTask"),
      _relocate(relocate),
      _iter(relocation_set),
      _failed(false) {}

  virtual void work() {
    if (!_relocate->work(&_iter)) {
      _failed = true;
    }
  }

  bool failed() const {
    return _failed;
  }
};

bool ZRelocate::relocate(ZRelocationSet* relocation_set) {
  ZRelocateTask task(this, relocation_set);
  _workers->run_concurrent(&task);
  return !task.failed();
}
