#pragma once

namespace Inferno {
template <typename A, typename T, size_t D> class FixedArray {
  explicit FixedArray(A *allocator) : m_Allocator(allocator), m_Dimensions(D) {
    for (size_t i = 0; i < D; ++i) {
      m_Data[i] = m_Allocator->Allocate<T>();
    }
  }
  ~FixedArray() {} // TODO: impl

  FixedArray(const FixedArray &) = delete;
  FixedArray &operator=(const FixedArray &) = delete;

  FixedArray(const FixedArray &&) = delete;
  FixedArray &operator=(const FixedArray &&) = delete;

private:
  A *m_Allocator = nullptr;
  T *m_Data = nullptr;
  size_t m_Dimensions;
};
} // namespace Inferno
