#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace Magpie {

enum class ForegroundCompatibilityMode : uint32_t { Off, Trace, Query, Reference };
enum class ForegroundQueryPhase : uint32_t { Preparing, Device, SwapChain, FirstPresent, Present, Count };
enum class ForegroundPresentationPath : uint32_t { Unknown, OrdinaryDXGI, DLSSFrontendDXGI, XeSSProxy, Composition, FSR3Proxy };

constexpr bool SupportsForegroundQuery(ForegroundPresentationPath path) noexcept {
	return path == ForegroundPresentationPath::OrdinaryDXGI ||
		path == ForegroundPresentationPath::DLSSFrontendDXGI || path == ForegroundPresentationPath::XeSSProxy || path == ForegroundPresentationPath::FSR3Proxy;
}

struct ForegroundWindowIdentity {
	uintptr_t window = 0;
	uint32_t pid = 0, tid = 0;
	uintptr_t atom = 0;
	bool operator==(const ForegroundWindowIdentity&) const = default;
};
struct ForegroundCallerRange {
	uintptr_t begin = 0, size = 0;
	bool operator==(const ForegroundCallerRange&) const = default;
};
struct ForegroundQuerySnapshot {
	uint64_t epoch = 0;
	ForegroundCompatibilityMode mode = ForegroundCompatibilityMode::Off;
	bool armed = false;
	bool outputShown = false;
	bool diagnostics = false;
	ForegroundWindowIdentity source, root, output, outputRoot;
	std::array<ForegroundCallerRange, 5> callers{};
	bool operator==(const ForegroundQuerySnapshot&) const = default;
};
struct ForegroundCompatibilitySnapshot : ForegroundQuerySnapshot {
	ForegroundWindowIdentity mainUI, parameterHost;
	bool operator==(const ForegroundCompatibilitySnapshot&) const = default;
};

// One frontend publisher; arbitrary driver/API readers. Atomic words avoid the
// data race of a conventional seqlock over a plain C++ struct. A reader never
// waits for a suspended publisher and fails closed on a changing generation.
class PublishedForegroundState {
public:
	void Publish(const ForegroundCompatibilitySnapshot& value) noexcept {
		// Only the frontend reads/writes _lastPublished. Equality compares fields,
		// not struct padding. An unchanged probe must not invalidate API readers.
		if (value == _lastPublished) return;
		static_assert(std::is_trivially_copyable_v<ForegroundQuerySnapshot>);
		std::array<uintptr_t, WordCount> words{};
		const ForegroundQuerySnapshot query = value;
		std::memcpy(words.data(), &query, sizeof(query));
		std::array<uintptr_t, DiagnosticWordCount> diagnostic{};
		const std::array identities{ value.mainUI, value.parameterHost };
		std::memcpy(diagnostic.data(), identities.data(), sizeof(identities));
		_version.fetch_add(1);
		_mode.store(value.mode);
		for (size_t i = 0; i < WordCount; ++i) _words[i].store(words[i]);
		for (size_t i = 0; i < DiagnosticWordCount; ++i) _diagnosticWords[i].store(diagnostic[i]);
		_version.fetch_add(1);
		_lastPublished = value;
	}
	bool QueriesEnabled() const noexcept {
		const auto mode = _mode.load();
		return mode == ForegroundCompatibilityMode::Query || mode == ForegroundCompatibilityMode::Trace;
	}
	// Cheap Off/Reference and non-target paths read no window identities. A
	// diagnostic session may observe unknown callers, without admitting them.
	bool ReadQuery(uintptr_t caller, ForegroundQuerySnapshot& value, uint64_t& version,
		unsigned& index) const noexcept {
		if (!QueriesEnabled()) return false;
		version = _version.load();
		if (version & 1) return false;
		std::array<uintptr_t, WordCount> words{};
		for (size_t i = CallerWord; i < WordCount; ++i) words[i] = _words[i].load();
		std::memcpy(value.callers.data(), words.data() + CallerWord, sizeof(value.callers));
		index = unsigned(value.callers.size());
		for (unsigned i = 0; i < value.callers.size(); ++i) {
			const auto& range = value.callers[i];
			if (range.size && caller >= range.begin && caller - range.begin < range.size) { index = i; break; }
		}
		// The small header carries the diagnostic switch. Non-target queries
		// need it, but still skip all four identities in normal mode.
		for (size_t i = 0; i < HeaderWords; ++i) words[i] = _words[i].load();
		ForegroundQuerySnapshot header;
		std::memcpy(&header, words.data(), HeaderWords * sizeof(uintptr_t));
		if (index == value.callers.size() && !header.diagnostics) return false;
		for (size_t i = HeaderWords; i < CallerWord; ++i) words[i] = _words[i].load();
		if (!Current(version)) return false;
		std::memcpy(&value, words.data(), sizeof(value));
		return value.mode == ForegroundCompatibilityMode::Query || value.mode == ForegroundCompatibilityMode::Trace;
	}
	bool Read(ForegroundCompatibilitySnapshot& value, uint64_t& version) const noexcept {
		version = _version.load();
		if (version & 1) return false;
		std::array<uintptr_t, WordCount> words{};
		for (size_t i = 0; i < WordCount; ++i) words[i] = _words[i].load();
		std::array<uintptr_t, DiagnosticWordCount> diagnostic{};
		for (size_t i = 0; i < DiagnosticWordCount; ++i) diagnostic[i] = _diagnosticWords[i].load();
		if (!Current(version)) return false;
		ForegroundQuerySnapshot query;
		std::memcpy(&query, words.data(), sizeof(query));
		static_cast<ForegroundQuerySnapshot&>(value) = query;
		std::array<ForegroundWindowIdentity, 2> identities{};
		std::memcpy(identities.data(), diagnostic.data(), sizeof(identities));
		value.mainUI = identities[0]; value.parameterHost = identities[1];
		return true;
	}
	bool Current(uint64_t version) const noexcept { return !(version & 1) && _version.load() == version; }

private:
	static_assert(std::is_standard_layout_v<ForegroundQuerySnapshot>);
	static_assert(sizeof(ForegroundCallerRange) == 2 * sizeof(uintptr_t));
	static constexpr size_t WordCount = (sizeof(ForegroundQuerySnapshot) + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);
	static constexpr size_t DiagnosticWordCount = 2 * sizeof(ForegroundWindowIdentity) / sizeof(uintptr_t);
	static constexpr size_t HeaderWords = offsetof(ForegroundQuerySnapshot, source) / sizeof(uintptr_t);
	static constexpr size_t CallerWord = offsetof(ForegroundQuerySnapshot, callers) / sizeof(uintptr_t);
	static_assert(offsetof(ForegroundQuerySnapshot, source) % sizeof(uintptr_t) == 0);
	static_assert(offsetof(ForegroundQuerySnapshot, callers) % sizeof(uintptr_t) == 0);
	std::atomic<uint64_t> _version = 0;
	std::atomic<ForegroundCompatibilityMode> _mode = ForegroundCompatibilityMode::Off;
	std::array<std::atomic<uintptr_t>, WordCount> _words{};
	std::array<std::atomic<uintptr_t>, DiagnosticWordCount> _diagnosticWords{};
	ForegroundCompatibilitySnapshot _lastPublished{};
};

}
