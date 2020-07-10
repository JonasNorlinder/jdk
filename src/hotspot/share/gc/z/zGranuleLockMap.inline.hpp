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

#ifndef SHARE_GC_Z_ZGRANULELOCKMAP_INLINE_HPP
#define SHARE_GC_Z_ZGRANULELOCKMAP_INLINE_HPP

#include "gc/z/zGlobals.hpp"
#include "gc/z/zGranuleLockMap.hpp"
#include "memory/allocation.inline.hpp"
#include "utilities/align.hpp"
#include "utilities/debug.hpp"
#include <iostream>

template <typename T>
inline ZGranuleLockMap<T>::ZGranuleLockMap(size_t max_offset) :
     _size(max_offset >> ZGranuleSizeShift),
    _map(MmapArrayAllocator<T>::allocate(_size, mtGC)) {
  assert(is_aligned(max_offset, ZGranuleLockSize), "Misaligned");
}

template <typename T>
inline ZGranuleLockMap<T>::~ZGranuleLockMap() {
  MmapArrayAllocator<T>::free(_map, _size);
}

template <typename T>
inline size_t ZGranuleLockMap<T>::index_for_offset(uintptr_t offset) const {
  const size_t index = offset >> ZGranuleSizeShift;
  assert(index < _size, "Invalid index");

  return index;
}

template <typename T>
inline T* ZGranuleLockMap<T>::get(uintptr_t offset) const {
  const size_t index = index_for_offset(offset);
  return &_map[index];
}

template <typename T>
inline void ZGranuleLockMap<T>::put(uintptr_t offset, T value) {
  const size_t index = index_for_offset(offset);
  _map[index] = value;
}

template <typename T>
inline void ZGranuleLockMap<T>::put(uintptr_t offset, size_t size, T value) {
  assert(is_aligned(size, ZGranuleSize), "Misaligned");

  const size_t start_index = index_for_offset(offset);
  const size_t end_index = start_index + (size >> ZGranuleSizeShift);
  for (size_t index = start_index; index < end_index; index++) {
    _map[index] = value;
  }
}

template <typename T>
inline ZGranuleLockMapIterator<T>::ZGranuleLockMapIterator(const ZGranuleLockMap<T>* map) :
    _map(map),
    _next(0) {}

template <typename T>
inline bool ZGranuleLockMapIterator<T>::next(T* value) {
  if (_next < _map->_size) {
    *value = _map->_map[_next++];
    return true;
  }

  // End of map
  return false;
}

template <typename T>
inline bool ZGranuleLockMapIterator<T>::next(T** value) {
  if (_next < _map->_size) {
    *value = _map->_map + _next++;
    return true;
  }

  // End of map
  return false;
}

#endif // SHARE_GC_Z_ZGRANULELOCKMAP_INLINE_HPP
