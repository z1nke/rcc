#ifndef RCC_SUPPORT_ALLOCATOR_H
#define RCC_SUPPORT_ALLOCATOR_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

namespace rcc {

template <typename Derived> class AllocatorBase {
public:
  void *allocate(std::size_t Size, std::size_t Align) {
    return static_cast<Derived *>(this)->allocate(Size, Align);
  }

  void deallocate(const void *Ptr, std::size_t Size, std::size_t Align) {
    static_cast<Derived *>(this)->deallocate(Ptr, Size, Align);
  }
};

class MallocAllocator : public AllocatorBase<MallocAllocator> {
public:
  void *allocate(std::size_t Size, std::size_t Align) {
    return ::operator new(Size
#ifdef __cpp_aligned_new
                          ,
                          std::align_val_t(Align)
#endif
    );
  }

  void deallocate(void *Ptr, std::size_t Size, std::size_t Align) {
    ::operator delete(Ptr
#ifdef __cpp_sized_deallocation
                      ,
                      Size
#endif
#ifdef __cpp_aligned_new
                      ,
                      std::align_val_t(Align)
#endif
    );
  }

private:
};

inline std::size_t alignTo(std::size_t Size, std::size_t Align) {
  return (Size + Align - 1) & ~(Align - 1);
}

inline std::size_t alignDown(std::size_t Size, std::size_t Align) {
  return alignTo(Size - Align + 1, Align);
}

inline std::uintptr_t alignAddr(const void *Ptr, std::size_t Align) {
  std::uintptr_t Addr = reinterpret_cast<std::uintptr_t>(Ptr);
  return alignTo(Addr, Align);
}

inline std::size_t offsetToAlignedAddr(const void *Ptr, std::size_t Align) {
  std::uintptr_t Addr = reinterpret_cast<std::uintptr_t>(Ptr);
  return alignTo(Addr, Align) - Addr;
}

template <typename AllocatorT = MallocAllocator, std::size_t SlabSize = 4096,
          std::size_t SizeThreshold = SlabSize, std::size_t GrowthDelay = 128>
class BumpPtrAllocatorImpl
    : public AllocatorBase<BumpPtrAllocatorImpl<AllocatorT, SlabSize,
                                                SizeThreshold, GrowthDelay>>,
      private AllocatorT {
public:
  static_assert(SizeThreshold <= SlabSize,
                "The SizeThreshold must be at most the SlabSize to ensure "
                "that objects larger than a slab go into their own memory "
                "allocation.");
  static_assert(GrowthDelay > 0,
                "GrowthDelay must be at least 1 which already increases the"
                "slab size after each allocated slab.");

  BumpPtrAllocatorImpl() = default;

  template <typename T>
  BumpPtrAllocatorImpl(T &&Allocator)
      : AllocatorT(std::forward<AllocatorT>(Allocator)) {}

  BumpPtrAllocatorImpl(BumpPtrAllocatorImpl &&RHS)
      : AllocatorT(static_cast<AllocatorT &&>(RHS)), CurPtr(RHS.CurPtr),
        End(RHS.End), Slabs(std::move(RHS.Slabs)),
        CustomSizedSlabs(std::move(RHS.CustomSizedSlabs)),
        BytesAllocated(RHS.BytesAllocated) {
    RHS.CurPtr = nullptr;
    RHS.End = nullptr;
    RHS.BytesAllocated = 0;
  }

  BumpPtrAllocatorImpl &operator=(BumpPtrAllocatorImpl &&RHS) {
    if (this == std::addressof(RHS))
      return *this;

    deallocateSlabs();
    deallocateCustomSizedSlabs();
    AllocatorT::operator=(static_cast<AllocatorT &&>(RHS));
    CurPtr = RHS.CurPtr;
    End = RHS.End;
    Slabs = std::move(RHS.Slabs);
    CustomSizedSlabs = std::move(RHS.CustomSizedSlabs);
    BytesAllocated = RHS.BytesAllocated;

    RHS.CurPtr = nullptr;
    RHS.End = nullptr;
    RHS.BytesAllocated = 0;
    return *this;
  }

  ~BumpPtrAllocatorImpl() {
    deallocateSlabs();
    deallocateCustomSizedSlabs();
  }

  void *allocate(std::size_t Size, std::size_t Align) {
    BytesAllocated += Size;

    std::size_t Adjustment = offsetToAlignedAddr(CurPtr, Align);
    if (Size + Adjustment <= static_cast<std::size_t>(End - CurPtr)) {
      char *AlignedPtr = CurPtr + Adjustment;
      CurPtr = AlignedPtr + Size;
      return AlignedPtr;
    }

    std::size_t PaddedSize = Size + Align - 1;
    if (PaddedSize > SizeThreshold) {
      void *NewSlab =
          AllocatorT::allocate(PaddedSize, alignof(std::max_align_t));
      CustomSizedSlabs.push_back(std::make_pair(NewSlab, PaddedSize));
      std::uintptr_t AlignedAddr = alignAddr(NewSlab, Align);
      assert(AlignedAddr + Size <= (std::uintptr_t(NewSlab) + PaddedSize) &&
             "Aligned address + Size must be within the allocated slab");

      return reinterpret_cast<char *>(AlignedAddr);
    }

    startNewSlab();
    std::uintptr_t AlignedAddr = alignAddr(CurPtr, Align);
    assert(AlignedAddr + Size <= reinterpret_cast<std::uintptr_t>(End) &&
           "Unable to allocate memory!");
    char *AlignedPtr = reinterpret_cast<char *>(AlignedAddr);
    CurPtr = AlignedPtr + Size;
    return AlignedPtr;
  }

  void deallocate(void *Ptr, std::size_t Size, std::size_t Align) {
    // Do nothing.
  }

private:
  static constexpr std::size_t computeSlabSize(unsigned SlabIdx) {
    std::size_t Ratio = 1 << std::min<size_t>(30, SlabIdx / GrowthDelay);
    return SlabSize * Ratio;
  }

  void deallocateSlabs() {
    for (std::size_t Idx = 0; Idx < Slabs.size(); ++Idx) {
      std::size_t AllocatedSlabSize = computeSlabSize(Idx);
      AllocatorT::deallocate(Slabs[Idx], AllocatedSlabSize,
                             alignof(std::max_align_t));
    }
  }

  void deallocateCustomSizedSlabs() {
    for (auto [Ptr, Size] : CustomSizedSlabs)
      AllocatorT::deallocate(Ptr, Size, alignof(std::max_align_t));
  }

  void startNewSlab() {
    std::size_t NewSlabSize = computeSlabSize(Slabs.size());
    void *NewSlab =
        AllocatorT::allocate(NewSlabSize, alignof(std::max_align_t));
    Slabs.push_back(NewSlab);
    CurPtr = reinterpret_cast<char *>(NewSlab);
    End = CurPtr + NewSlabSize;
  }

private:
  //                                 Slabs
  //  +-----------+-----------+-----------+------------------------------------
  //  |   slab0   |   slab1   |   slab2   |                  ...
  //  +-----------+-----------+-----------+------------------------------------
  //                          ^           ^
  //                          |           |
  //                         CurPtr      End
  //
  //                              CustomSizedSlabs:
  // +-----------------------------+-----------+-------------------------------
  // |         CSSlab0             |  CSSlab1  |               ...
  // +-----------------------------+-----------+-------------------------------

  char *CurPtr = nullptr;
  char *End = nullptr;
  std::vector<void *> Slabs;
  // Custom sized slabs:
  // To handle an oversized(SizeThreshold) memory allocation request.
  std::vector<std::pair<void *, std::size_t>> CustomSizedSlabs;
  std::size_t BytesAllocated = 0;
};

using BumpPtrAllocator = BumpPtrAllocatorImpl<>;

} // namespace rcc

#endif