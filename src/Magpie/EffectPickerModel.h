#pragma once
#include <algorithm>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <unordered_set>
#include "RTXVideoParameters.h"

namespace Magpie {

// Curated shortcut, independent of recommendation levels and custom filenames.
inline constexpr std::wstring_view ADVANCED_PICKER_EFFECTS[] = {
	L"NvidiaVFX\\Upscale", L"NvidiaVFX\\VFG", L"RTXVideo\\RTXVideo_HDR",
	L"RTXVideo\\RTXVideo_VSR", L"DLSSFG\\DLSS_FrameGeneration",
	L"XeSSFG\\XeSS_FrameGeneration", L"FSR3FG\\FSR3_FrameGeneration",
	L"DLSSNR\\DLSSNR_AI_Filter", L"FrameRate_Filter"
};

struct EffectPickerFamily {
	std::wstring id, name, summary;
};

struct EffectPickerEntry {
	std::wstring id, name, category, subcategory, summary, details, searchText;
	std::vector<std::wstring> purposes;
	std::wstring recommendation;
	bool firstTry = false;
	EffectPickerFamily family, subfamily;
};

struct EffectPickerCategory {
	std::wstring id, name, description;
	std::vector<std::pair<std::wstring, std::wstring>> subcategories;
};

inline std::wstring NormalizeEffectSearch(std::wstring_view text) {
	std::wstring result;
	for (wchar_t c : text) {
		if (c == L'_' || c == L'\\' || c == L'/' || c == L'-' || std::iswspace(c)) continue;
		result += static_cast<wchar_t>(std::towlower(c));
	}
	return result;
}

inline bool MatchesEffectSearch(std::wstring_view normalizedText, std::wstring_view query) {
	// Each whitespace-separated term may match anywhere, including a legacy alias.
	size_t start = 0;
	while (start < query.size()) {
		while (start < query.size() && std::iswspace(query[start])) ++start;
		size_t end = start;
		while (end < query.size() && !std::iswspace(query[end])) ++end;
		if (normalizedText.find(NormalizeEffectSearch(query.substr(start, end - start))) == std::wstring_view::npos) return false;
		start = end;
	}
	return true;
}

inline bool MatchesEffectCategory(const EffectPickerEntry& entry,
	std::wstring_view category, std::wstring_view subcategory) {
	if (category.empty()) return true;
	if (category == L"first_try") return entry.firstTry;
	if (category == L"advanced") return std::ranges::find(ADVANCED_PICKER_EFFECTS, entry.id) != std::end(ADVANCED_PICKER_EFFECTS);
	if (category != entry.category && std::ranges::find(entry.purposes, category) == entry.purposes.end()) return false;
	return subcategory.empty() || (entry.category == category && entry.subcategory == subcategory);
}

// A visible row always carries an effect ID or a family key, never a filtered
// index into the catalog. Only leaves can be added to a scaling mode.
struct EffectPickerVisibleRow {
	std::wstring key, parent, effectId, name, summary;
	size_t count = 1;
	int depth = 0;
	bool expanded = false;
	bool IsFamily() const { return effectId.empty(); }
};

inline int EffectPickerLetter(std::wstring_view name) {
	while (!name.empty() && std::iswspace(name.front())) name.remove_prefix(1);
	if (name.empty()) return 26;
	wchar_t c = name.front();
	if (c >= L'a' && c <= L'z') c -= L'a' - L'A';
	return c >= L'A' && c <= L'Z' ? c - L'A' : 26;
}

inline bool EffectNameLess(std::wstring_view a, std::wstring_view b) {
	while (!a.empty() && std::iswspace(a.front())) a.remove_prefix(1);
	while (!b.empty() && std::iswspace(b.front())) b.remove_prefix(1);
	while (!a.empty() && std::iswspace(a.back())) a.remove_suffix(1);
	while (!b.empty() && std::iswspace(b.back())) b.remove_suffix(1);
	if (EffectPickerLetter(a) != EffectPickerLetter(b)) return EffectPickerLetter(a) < EffectPickerLetter(b);
	size_t i = 0, j = 0;
	while (i < a.size() && j < b.size()) {
		if (a[i] >= L'0' && a[i] <= L'9' && b[j] >= L'0' && b[j] <= L'9') {
			size_t ae = i, be = j;
			while (ae < a.size() && a[ae] >= L'0' && a[ae] <= L'9') ++ae;
			while (be < b.size() && b[be] >= L'0' && b[be] <= L'9') ++be;
			size_t az = i, bz = j;
			while (az < ae && a[az] == L'0') ++az;
			while (bz < be && b[bz] == L'0') ++bz;
			if (ae - az != be - bz) return ae - az < be - bz;
			const int digits = a.substr(az, ae - az).compare(b.substr(bz, be - bz));
			if (digits) return digits < 0;
			i = ae; j = be;
			continue;
		}
		const auto ac = std::towlower(a[i]), bc = std::towlower(b[j]);
		if (ac != bc) return ac < bc;
		++i; ++j;
	}
	if (i != a.size() || j != b.size()) return i == a.size();
	return a < b;
}

struct EffectPickerTree {
	std::vector<EffectPickerVisibleRow> rows;
	size_t effectCount = 0;
};

inline EffectPickerTree BuildEffectPickerTree(const std::vector<EffectPickerEntry>& entries,
	std::wstring_view category, std::wstring_view subcategory, std::wstring_view query,
	const std::unordered_set<std::wstring>& expandedFamilies,
	const std::unordered_set<std::wstring>& collapsedSearchFamilies) {
	struct Node {
		EffectPickerVisibleRow row;
		std::vector<const EffectPickerEntry*> entries;
	};
	EffectPickerTree result;
	const bool searching = !NormalizeEffectSearch(query).empty();
	std::map<std::wstring, Node> roots;
	auto leaf = [](const EffectPickerEntry& entry, std::wstring parent, int depth) {
		return EffectPickerVisibleRow{L"effect:" + entry.id, std::move(parent), entry.id,
			entry.name, entry.summary, 1, depth};
	};
	for (const auto& entry : entries) {
		if (!(searching ? MatchesEffectSearch(entry.searchText, query)
			: MatchesEffectCategory(entry, category, subcategory))) continue;
		++result.effectCount;
		if (entry.family.id.empty()) {
			auto row = leaf(entry, {}, 0);
			roots.emplace(row.key, Node{row, {&entry}});
		} else {
			const auto key = L"family:" + entry.family.id;
			auto& node = roots[key];
			node.row = {key, {}, {}, entry.family.name, entry.family.summary};
			node.entries.push_back(&entry);
		}
	}
	auto order = [](const Node* a, const Node* b) {
		if (a->row.name == b->row.name) return a->row.key < b->row.key;
		return EffectNameLess(a->row.name, b->row.name);
	};
	auto append = [&](auto&& self, Node& node) -> void {
		if (node.entries.size() == 1) {
			result.rows.push_back(leaf(*node.entries[0], node.row.parent, node.row.depth));
			return;
		}
		node.row.count = node.entries.size();
		node.row.expanded = searching ? !collapsedSearchFamilies.contains(node.row.key)
			: expandedFamilies.contains(node.row.key);
		result.rows.push_back(node.row);
		if (!node.row.expanded) return;
		std::map<std::wstring, Node> children;
		for (const auto* entry : node.entries) {
			if (node.row.depth == 0 && !entry->subfamily.id.empty()) {
				const auto key = node.row.key + L"/" + entry->subfamily.id;
				auto& child = children[key];
				child.row = {key, node.row.key, {}, entry->subfamily.name,
					entry->subfamily.summary, 1, 1};
				child.entries.push_back(entry);
			} else {
				auto row = leaf(*entry, node.row.key, node.row.depth + 1);
				children.emplace(row.key, Node{row, {entry}});
			}
		}
		std::vector<Node*> sorted;
		for (auto& [key, child] : children) {
			if (child.entries.size() == 1) child.row.name = child.entries[0]->name;
			sorted.push_back(&child);
		}
		std::ranges::sort(sorted, order);
		for (auto* child : sorted) self(self, *child);
	};
	std::vector<Node*> sorted;
	for (auto& [key, node] : roots) {
		if (node.entries.size() == 1) node.row.name = node.entries[0]->name;
		sorted.push_back(&node);
	}
	std::ranges::sort(sorted, order);
	for (auto* node : sorted) append(append, *node);
	return result;
}

}
