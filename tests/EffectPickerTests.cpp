#include "../src/Magpie/EffectPickerModel.h"
#include "../src/Magpie/EffectHelper.h"
#include <cassert>
#include <iostream>

int main() {
	using namespace Magpie;
	const auto search = NormalizeEffectSearch(L"DLSS\\DLSS_SR 抗锯齿 时序边缘平滑");
	assert(MatchesEffectSearch(search, L"dlss sr"));
	assert(MatchesEffectSearch(search, L"抗锯齿 DLSS"));
	assert(MatchesEffectSearch(search, L" DLSS_SR\t"));
	assert(!MatchesEffectSearch(search, L"DLSS 降噪"));
	assert(MatchesEffectSearch(search, L"   "));
	EffectPickerEntry aa;
	aa.category = L"upscale";
	aa.subcategory = L"动画线条";
	aa.purposes = { L"upscale", L"antialiasing" };
	assert(MatchesEffectCategory(aa, L"", L""));
	assert(MatchesEffectCategory(aa, L"antialiasing", L""));
	assert(!MatchesEffectCategory(aa, L"antialiasing", L"时序"));
	assert(MatchesEffectCategory(aa, L"upscale", L"动画线条"));
	assert(!MatchesEffectCategory(aa, L"first_try", L""));
	aa.firstTry = true;
	assert(MatchesEffectCategory(aa, L"first_try", L""));
	std::vector<EffectPickerEntry> advancedEntries;
	// Keep new frame generators reachable independently of their level label.
	EffectPickerEntry fsr3;
	fsr3.id = L"FSR3FG\\FSR3_FrameGeneration";
	fsr3.category = L"frame"; fsr3.subcategory = L"补帧";
	assert(MatchesEffectCategory(fsr3, L"advanced", L""));
	assert(MatchesEffectCategory(fsr3, L"frame", L"补帧"));
	assert(!MatchesEffectCategory(fsr3, L"frame", L"帧率调度"));
	for (const auto id : ADVANCED_PICKER_EFFECTS) {
		EffectPickerEntry entry;
		entry.id = id; entry.name = id; entry.category = L"style";
		entry.searchText = NormalizeEffectSearch(id);
		assert(MatchesEffectCategory(entry, L"advanced", L""));
		assert(MatchesEffectCategory(entry, L"style", L""));
		advancedEntries.push_back(entry);
	}
	for (const auto id : {L"RTXVideo\\RTXVideo_Denoise", L"custom\\DLSSNR_AI_Filter", L"DLSS\\DLSS_SR"}) {
		EffectPickerEntry entry;
		entry.id = id; entry.name = id; entry.recommendation = L"进阶／实验";
		assert(!MatchesEffectCategory(entry, L"advanced", L""));
		advancedEntries.push_back(entry);
	}
	const auto advancedTree = BuildEffectPickerTree(advancedEntries, L"advanced", L"", L"", {}, {});
	assert(advancedTree.effectCount == std::size(ADVANCED_PICKER_EFFECTS) &&
		advancedTree.rows.size() == std::size(ADVANCED_PICKER_EFFECTS));
	for (int family = 0; family < 2; ++family) {
		const auto canonical = RTXVideoCanonicalId<wchar_t>(family);
		assert(RTXVideoFamily(canonical) == family);
		for (int tier = 0; tier < 4; ++tier) {
			const auto oldId = RTX_VIDEO_IDS[family][tier];
			assert(RTXVideoFamily(oldId) == family);
			assert(RTXVideoStrength(oldId) == tier);
			assert(EffectHelper::GetDisplayName(canonical) == EffectHelper::GetDisplayName(oldId));
			assert(RTXVideoQualityLevel(family, tier) == unsigned((family == 0 ? 8 : 1) + tier));
		}
	}
	assert(RTXVideoFamily(L"custom\\RTXVideo_VSR_High") == -1);
	assert(RTXVideoFamily(L"RTXVideo\\RTXVideo_VSR_High_Custom") == -1);
	for (const auto& [id, name] : std::vector<std::pair<std::wstring, std::wstring>>{
		{L"DLSSFG\\DLSS_FrameGeneration", L"DLSS FrameGeneration"},
		{L"XeSSFG\\XeSS_FrameGeneration", L"XeSS FrameGeneration"},
		{L"FSR3FG\\FSR3_FrameGeneration", L"FSR3 FrameGeneration"}}) {
		assert(EffectHelper::GetDisplayName(id) == name);
		const auto fgSearch = NormalizeEffectSearch(id + L" " + name);
		assert(MatchesEffectSearch(fgSearch, id));
		assert(MatchesEffectSearch(fgSearch, name));
	}
	assert(EffectHelper::GetDisplayName(L"custom\\FSR3_FrameGeneration") == L"FSR3_FrameGeneration");
	assert(EffectHelper::GetDisplayName(L"NvidiaVFX\\VFG") == L"Nvidia VFX VFG");
	std::vector<EffectPickerEntry> entries;
	for (const auto& [id, name] : std::vector<std::pair<std::wstring, std::wstring>>{
		{L"old/16", L"CuNNy-16"}, {L"old/2", L"CuNNy-2"}, {L"v2/4", L"CuNNy2-4"},
		{L"v2/8", L"CuNNy2-8"}}) {
		EffectPickerEntry entry;
		entry.id = id; entry.name = name; entry.category = L"upscale";
		entry.family = {L"cunny", L"CuNNy", L"Family description"};
		entry.subfamily = id.starts_with(L"old") ? EffectPickerFamily{L"old", L"CuNNy 原版", L"Original"}
			: EffectPickerFamily{L"v2", L"CuNNy2", L"Second generation"};
		entry.searchText = NormalizeEffectSearch(name + L" CuNNy");
		entries.push_back(entry);
	}
	EffectPickerEntry custom;
	custom.id = L"custom/CuNNy"; custom.name = L"CuNNy custom"; custom.category = L"custom";
	custom.searchText = NormalizeEffectSearch(custom.name);
	entries.push_back(custom);
	custom.id = L"#custom"; custom.name = L" 中文"; custom.searchText = L"custom";
	entries.push_back(custom);
	const std::unordered_set<std::wstring> expanded{L"family:cunny", L"family:cunny/old", L"family:cunny/v2"};
	auto tree = BuildEffectPickerTree(entries, L"", L"", L"", {}, {});
	assert(tree.effectCount == 6 && tree.rows.size() == 3);
	assert(tree.rows[0].IsFamily() && tree.rows[0].count == 4 && tree.rows[0].key == L"family:cunny");
	assert(tree.rows[1].effectId == L"custom/CuNNy" && tree.rows[2].effectId == L"#custom");
	tree = BuildEffectPickerTree(entries, L"upscale", L"", L"", expanded, {});
	assert(tree.effectCount == 4 && tree.rows.size() == 7);
	std::vector<std::wstring> leaves;
	for (const auto& row : tree.rows) if (!row.IsFamily()) leaves.push_back(row.effectId);
	assert((leaves == std::vector<std::wstring>{L"old/2", L"old/16", L"v2/4", L"v2/8"}));
	// Search ignores the purpose filter, expands matches temporarily and retains IDs.
	tree = BuildEffectPickerTree(entries, L"custom", L"", L"CuNNy-16", {}, {});
	assert(tree.effectCount == 1 && tree.rows.size() == 1 && tree.rows[0].effectId == L"old/16");
	tree = BuildEffectPickerTree(entries, L"custom", L"", L"CuNNy", {}, {});
	assert(tree.effectCount == 5 && tree.rows.size() == 8);
	tree = BuildEffectPickerTree(entries, L"custom", L"", L"CuNNy", {}, {L"family:cunny"});
	assert(tree.effectCount == 5 && tree.rows.size() == 2 && !tree.rows[0].expanded);
	tree = BuildEffectPickerTree(entries, L"upscale", L"", L"", {}, {});
	assert(tree.rows.size() == 1 && !tree.rows[0].expanded);
	tree = BuildEffectPickerTree(entries, L"", L"", L"missing", expanded, {});
	assert(tree.rows.empty() && tree.effectCount == 0);
	entries[0].purposes = {L"cleanup"};
	tree = BuildEffectPickerTree(entries, L"cleanup", L"", L"", expanded, {});
	assert(tree.rows.size() == 1 && tree.rows[0].effectId == L"old/16");
	assert(EffectNameLess(L" NNEDI3_nns8", L"NNEDI3_nns16"));
	assert(EffectNameLess(L"xBRZ_2x", L"xBRZ_10x"));
	assert(EffectNameLess(L"a99999999999999999999", L"a100000000000000000000"));
	assert(EffectNameLess(L"Z", L"2x"));
	assert(EffectPickerLetter(L"  cunny") == 2 && EffectPickerLetter(L"nnedi3") == 13);
	assert(EffectPickerLetter(L" 中文 ") == 26 && EffectPickerLetter(L"2x") == 26);
	assert(EffectPickerLetter(L"#") == 26 && EffectPickerLetter(L"\t") == 26);
	std::cout << "Family collapse/search restoration, exact leaf IDs, secondary purposes, natural sorting and A-Z/# indexing passed.\n";
	std::cout << "Effect picker search, purpose filters, canonical names and all eight vendor quality levels passed.\n";
}
