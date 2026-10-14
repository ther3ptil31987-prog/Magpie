#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace Magpie {

constexpr bool ScreenshotNameEquals(std::wstring_view left, std::wstring_view right) noexcept {
	if (left.size() != right.size()) return false;
	for (size_t i = 0; i < left.size(); ++i) {
		auto lower = [](wchar_t c) { return c >= L'A' && c <= L'Z' ? c + (L'a' - L'A') : c; };
		if (lower(left[i]) != lower(right[i])) return false;
	}
	return true;
}

inline uint32_t ParseScreenshotNumber(std::wstring_view name) noexcept {
	if (name.size() < 7 || !ScreenshotNameEquals(name.substr(0, 7), L"Magpie_")) return 0;
	const size_t dot = name.find_last_of(L'.');
	if (dot == std::wstring_view::npos) return 0;
	const auto extension = name.substr(dot);
	if (!ScreenshotNameEquals(extension, L".png") && !ScreenshotNameEquals(extension, L".dds")) return 0;
	auto digits = name.substr(7, dot - 7);
	for (const auto suffix : { std::wstring_view(L"_input"), std::wstring_view(L"_output") }) {
		if (digits.size() >= suffix.size() && ScreenshotNameEquals(digits.substr(digits.size() - suffix.size()), suffix)) {
			digits.remove_suffix(suffix.size()); break;
		}
	}
	if (digits.empty()) return 0;
	uint32_t value = 0;
	for (wchar_t digit : digits) {
		if (digit < L'0' || digit > L'9') return 0;
		const uint32_t d = static_cast<uint32_t>(digit - L'0');
		if (value > (std::numeric_limits<uint32_t>::max() - d) / 10) return 0;
		value = value * 10 + d;
	}
	return value;
}

inline uint32_t NextScreenshotNumber(std::vector<uint32_t> numbers) noexcept {
	if (numbers.empty()) return 1;
	const uint32_t largest = *std::max_element(numbers.begin(), numbers.end());
	if (largest != std::numeric_limits<uint32_t>::max()) return largest + 1;
	std::sort(numbers.begin(), numbers.end());
	uint32_t next = 1;
	for (uint32_t number : numbers) {
		if (number < next) continue; // A pair (and png/dds) occupies one number.
		if (number > next) return next;
		if (next == std::numeric_limits<uint32_t>::max()) return 0;
		++next;
	}
	return next;
}

}
