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

#ifndef SHARE_GC_Z_ZGRANULELOCKMAP_HPP
#define SHARE_GC_Z_ZGRANULELOCKMAP_HPP

#include "memory/allocation.hpp"
#include "utilities/powerOfTwo.hpp"
#include "gc/z/zGlobals.hpp"
#include "runtime/os.hpp"

template<typename T>
class ZGranuleLockMapIterator;

template <typename T>
class ZGranuleLockMap {
  friend class VMStructs;
  friend class ZGranuleLockMapIterator<T>;

private:
  // Granule lock shift/size
  const size_t pw = round_up_power_of_2(os::processor_count() << 2);
  const size_t ZGranuleLockSizeShift = log2(ZAddressOffsetMax / pw);
  const size_t ZGranuleLockSize  = (size_t)1 << ZGranuleLockSizeShift;

  const size_t _size;
  T* const     _map;

  size_t index_for_offset(uintptr_t offset) const;

public:
  ZGranuleLockMap(size_t max_offset);
  ~ZGranuleLockMap();

  T* get(uintptr_t offset) const;
  void put(uintptr_t offset, T value);
  void put(uintptr_t offset, size_t size, T value);
};

template <typename T>
class ZGranuleLockMapIterator : public StackObj {
public:
  const ZGranuleLockMap<T>* const _map;
  size_t                      _next;

public:
  ZGranuleLockMapIterator(const ZGranuleLockMap<T>* map);

  bool next(T* value);
  bool next(T** value);
};

#endif // SHARE_GC_Z_ZGRANULELOCKMAP_HPP
