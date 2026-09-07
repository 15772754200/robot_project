#pragma once
#include <type_traits>

template <typename T> struct basic_bit_pattern
{
  using value_type = T;

  value_type mask;
  value_type expected;

  constexpr basic_bit_pattern(value_type mask, value_type expected)
      : mask(mask), expected(expected)
  {
  }

  constexpr bool match(value_type v) const { return (v & mask) == expected; }

  constexpr void set(volatile value_type &v) const
  {
    v &= ~mask;
    v |= expected;
  }

};

template <typename T>
inline constexpr bool operator==(const basic_bit_pattern<T> &x, const basic_bit_pattern<T> &y)
{
  return x.mask == y.mask && x.expected == y.expected;
}

template <typename T>
inline constexpr bool operator!=(const basic_bit_pattern<T> &x, const basic_bit_pattern<T> &y)
{
  return !(x == y);
}

template <typename T>
inline constexpr bool operator==(const basic_bit_pattern<T> &pattern, const volatile T &word)
{
  return pattern.match(word);
}

template <typename T>
inline void operator<<(volatile T &word, const basic_bit_pattern<std::remove_cv_t<T>> &pattern)
{
  pattern.set(word);
}

template <typename T>
inline T operator<<(T &&word, const basic_bit_pattern<std::remove_cv_t<T>> &pattern)
{
  T tmp = word;
  pattern.set(tmp);
  return tmp;
}

