#pragma once

#include "Inferno/Core/Log.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>

namespace Inferno {
template <typename T> using Scope = std::unique_ptr<T>;
template <typename T> using Ref = std::shared_ptr<T>;

template <typename T, typename... Args> Scope<T> MakeScope(Args &&...args) {
  return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T, typename... Args> Ref<T> MakeRef(Args &&...args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

// Custom Allocators
constexpr size_t KiB(size_t n) { return n * 1024ULL; }
constexpr size_t MiB(size_t n) { return n * 1024ULL * 1024ULL; }
constexpr size_t GiB(size_t n) { return n * 1024ULL * 1024ULL * 1024ULL; }

class ArenaAllocator {
public:
  using MemMarker = size_t;

  explicit ArenaAllocator(size_t capacity) : m_Capacity(capacity) {
    m_Buffer = static_cast<std::byte *>(
        ::operator new(capacity, std::align_val_t{ALIGNMENT}));
  }

  ~ArenaAllocator() { Release(); }

  ArenaAllocator(const ArenaAllocator &) = delete;
  ArenaAllocator &operator=(const ArenaAllocator &) = delete;

  ArenaAllocator(ArenaAllocator &&) = delete;
  ArenaAllocator &operator=(ArenaAllocator &&) = delete;

  template <typename T> T *Allocate() {
    static_assert(
        std::is_trivially_destructible_v<T>,
        "ArenaAllocator::Allocate<T>: T has a non-trivial destructor. "
        "Reset()/RewindTo() will never call it — give T an explicit "
        "Destroy().");
    static_assert(
        std::is_nothrow_default_constructible_v<T>,
        "ArenaAllocator::Allocate<T>: T's default constructor must not throw "
        "(arena allocation has no exception-safety path).");

    // Allocate 1 T and initialize it with Args
    // return T*;
    uintptr_t currentAddress =
        reinterpret_cast<uintptr_t>(m_Buffer + m_Position);
    size_t alignment = alignof(T);
    // size_t padding = (alignment - (currentAddress % alignment)) % alignment;
    size_t padding = (-currentAddress) & (alignment - 1);

    size_t newPosition = m_Position + padding + sizeof(T);
    if (newPosition > m_Capacity) {
      INFERNO_LOG_ERROR("[ArenaAllocator] New Position: {} | Capacity: {}",
                        newPosition, m_Capacity);
      throw std::bad_alloc();
    }

    m_Position += padding;
    void *ptr = m_Buffer + m_Position;
    m_Position += sizeof(T);

    return ::new (ptr) T();
  }

  // Override aligment for SIMD types
  std::byte *AllocateBytes(size_t byteCount,
                           size_t alignment = alignof(std::max_align_t)) {
    assert(alignment != 0);
    assert((alignment & (alignment - 1)) == 0);

    uintptr_t currentAddress =
        reinterpret_cast<uintptr_t>(m_Buffer + m_Position);
    size_t padding = (-currentAddress) & (alignment - 1);

    size_t newPosition = m_Position + padding + byteCount;
    if (newPosition > m_Capacity) {
      INFERNO_LOG_ERROR("[ArenaAllocator] New Position: {} | Capacity: {}",
                        newPosition, m_Capacity);
      throw std::bad_alloc();
    }

    m_Position += padding;
    std::byte *ptr = m_Buffer + m_Position;
    m_Position += byteCount;

    return ptr;
  }

  MemMarker GetMarker() const { return m_Position; }

  void RewindTo(MemMarker marker) { m_Position = marker; }

  void Reset() { m_Position = 0; }

  void Release() {
    if (m_Buffer) {
      ::operator delete(m_Buffer, std::align_val_t{ALIGNMENT});
      m_Buffer = nullptr;
      m_Capacity = 0;
      m_Position = 0;
    }
  }

private:
  static constexpr size_t ALIGNMENT = alignof(std::max_align_t);

  std::byte *m_Buffer = nullptr;
  size_t m_Capacity = 0;
  size_t m_Position = 0;
};

} // namespace Inferno
