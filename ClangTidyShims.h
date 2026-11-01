#pragma once
// Force-included into clang-tidy runs (Directory.Build.props) and clang-cl builds (Directory.Build.targets);
// never part of the MSVC build.
// Fills in MSVC intrinsics that third-party headers use but clang doesn't provide.

#if defined(__clang__) && !__has_builtin(_udiv128)
// libobs util/util_uint64.h
static inline unsigned __int64 _udiv128(unsigned __int64 high, unsigned __int64 low, unsigned __int64 divisor, unsigned __int64* remainder)
{
	const unsigned __int128 dividend = (static_cast<unsigned __int128>(high) << 64u) | low;
	*remainder = static_cast<unsigned __int64>(dividend % divisor);
	return static_cast<unsigned __int64>(dividend / divisor);
}
#endif
