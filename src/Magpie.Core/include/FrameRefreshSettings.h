#pragma once
#include "FramePacingOptions.h"
#include "CursorRefreshSettings.h"
#include <optional>
#include <type_traits>

namespace Magpie {

// Auto is retained only as a legacy serialized value; do not renumber Custom.
enum class ContentFrameRateMode : uint32_t { Source, Auto, Custom };
enum class CursorRefreshMode : uint32_t { Responsive, OriginalOnly, Supplement };
enum class CursorSupplementMode : uint32_t { Auto, Custom };

// Saved per profile. Runtime limits are projections of this single model;
// clamping a runtime value must never overwrite the user's saved target.
struct FrameRefreshSettings {
	static constexpr int MinimumEditedRate = 15;
	static constexpr int MaximumEditedRate = 360;
	ContentFrameRateMode contentMode = ContentFrameRateMode::Custom;
	float contentRate = 60.0f;
	FrameSyncMode pacing = FrameSyncMode::FrontEdge;
	CursorRefreshMode cursorMode = CursorRefreshMode::Supplement;
	CursorSupplementMode cursorSupplement = CursorSupplementMode::Custom;
	float cursorRate = 60.0f;
	bool idleEnabled = true;
	float idleRate = 30.0f;
	// Explicit per-profile opt-in. Do not migrate the dormant global vrr field.
	bool vrrEnabled = false;
	// Migration-only constraints. Auto must remain dynamic when an old cap
	// coexists with it. Explicit edits to content/cursor mode release these.
	float legacyContentLimit = 0.0f;
	// -1 means absent; 0 remains Auto for an old filter following disabled sync.
	float legacySourceTarget = -1.0f;
	bool legacyLimiterOnly = false;
	bool legacyResponsiveMinimum = false;
	bool operator==(const FrameRefreshSettings&) const = default;

	static float ValidateRate(double value, float fallback = 60.0f) noexcept {
		return std::isfinite(value) && value >= 1 && value <= 1000 ? float(value) : fallback;
	}
	// Quantize explicit user edits only. Preserve legacy saved values and the
	// fractional targets calculated from display refresh / FG multipliers.
	static float ValidateEditedRate(double value, float fallback = 60.0f) noexcept {
		return std::round(std::clamp(ValidateRate(value, fallback),
			float(MinimumEditedRate), float(MaximumEditedRate)));
	}
	bool IsValid() const noexcept {
		return uint32_t(contentMode) <= uint32_t(ContentFrameRateMode::Custom) &&
			uint32_t(cursorMode) <= uint32_t(CursorRefreshMode::Supplement) &&
			uint32_t(cursorSupplement) <= uint32_t(CursorSupplementMode::Custom) &&
			IsValidFrameSyncMode(pacing) && contentRate == ValidateRate(contentRate, -1) &&
			cursorRate == ValidateRate(cursorRate, -1) && idleRate == ValidateRate(idleRate, -1) &&
			(legacyContentLimit == 0 || legacyContentLimit == ValidateRate(legacyContentLimit, -1)) &&
			(legacySourceTarget == -1 || legacySourceTarget == 0 || legacySourceTarget == ValidateRate(legacySourceTarget, -2)) &&
			(!legacyLimiterOnly || contentMode == ContentFrameRateMode::Custom);
	}
	void ContentEdited() noexcept { legacyContentLimit = 0; legacySourceTarget = -1; legacyLimiterOnly = false; }
	void CursorEdited() noexcept { legacyResponsiveMinimum = false; }
	FrameSyncSettings ContentSync() const noexcept {
		const float rate = legacySourceTarget >= 0 && (contentMode == ContentFrameRateMode::Source || legacyLimiterOnly)
			? legacySourceTarget : contentMode == ContentFrameRateMode::Auto ? 0.0f : contentRate;
		return { contentMode != ContentFrameRateMode::Source && !legacyLimiterOnly, rate, pacing };
	}
	std::optional<float> ContentLimit() const noexcept {
		if (legacyLimiterOnly) return contentRate;
		return legacyContentLimit > 0 ? std::optional<float>(legacyContentLimit) : std::nullopt;
	}
	CursorRefreshSettings Cursor() const noexcept {
		return { cursorMode != CursorRefreshMode::Responsive,
			cursorMode == CursorRefreshMode::Supplement ||
				(cursorMode == CursorRefreshMode::Responsive && legacyResponsiveMinimum),
			cursorRate, cursorSupplement == CursorSupplementMode::Auto };
	}
};

inline FrameRefreshSettings NormalizeContentFrameRate(FrameRefreshSettings settings) noexcept {
	if (settings.contentMode == ContentFrameRateMode::Auto) {
		settings.contentMode = ContentFrameRateMode::Source;
		settings.ContentEdited();
	}
	return settings;
}

// Saved fields remain the advanced preferences. Simple mode only projects them
// into runtime values; content, idle and VRR remain shared.
inline FrameRefreshSettings EffectiveFrameRefreshSettings(
	FrameRefreshSettings settings, bool simple) noexcept {
	settings = NormalizeContentFrameRate(settings);
	if (simple) {
		settings.pacing = FrameSyncMode::FrontEdge;
		settings.cursorMode = CursorRefreshMode::Supplement;
		settings.cursorSupplement = CursorSupplementMode::Custom;
		settings.cursorRate = settings.contentMode == ContentFrameRateMode::Custom ? settings.contentRate : 60.0f;
		settings.ContentEdited();
		settings.CursorEdited();
	}
	return settings;
}

// Transactional field merge for stale runtime panels. Independent edits are
// retained; any conflicting edit rejects the entire refresh transaction.
inline bool MergeFrameRefreshSettings(FrameRefreshSettings& current,
	const FrameRefreshSettings& before, const FrameRefreshSettings& after) noexcept {
	if (!after.IsValid()) return false;
	auto merged = current;
	auto merge = [](auto& result, const auto& oldValue, const auto& newValue) {
		if (newValue == oldValue) return true;
		if (result != oldValue && result != newValue) return false;
		result = newValue;
		return true;
	};
#define MERGE_REFRESH(field) if (!merge(merged.field, before.field, after.field)) return false
	MERGE_REFRESH(contentMode); MERGE_REFRESH(contentRate); MERGE_REFRESH(pacing);
	MERGE_REFRESH(cursorMode); MERGE_REFRESH(cursorSupplement); MERGE_REFRESH(cursorRate);
	MERGE_REFRESH(idleEnabled); MERGE_REFRESH(idleRate);
	MERGE_REFRESH(vrrEnabled);
	MERGE_REFRESH(legacyContentLimit); MERGE_REFRESH(legacyLimiterOnly); MERGE_REFRESH(legacyResponsiveMinimum);
	MERGE_REFRESH(legacySourceTarget);
#undef MERGE_REFRESH
	// Compatibility constraints belong to the content/cursor choice as a group.
	// Do not accept a stale removal while a concurrent edit changed that choice.
	if ((after.legacyContentLimit != before.legacyContentLimit || after.legacyLimiterOnly != before.legacyLimiterOnly ||
		after.legacySourceTarget != before.legacySourceTarget) &&
		((current.contentMode != before.contentMode && current.contentMode != after.contentMode) ||
		 (current.contentRate != before.contentRate && current.contentRate != after.contentRate) ||
		 (current.pacing != before.pacing && current.pacing != after.pacing))) return false;
	if (after.legacyResponsiveMinimum != before.legacyResponsiveMinimum &&
		current.cursorMode != before.cursorMode && current.cursorMode != after.cursorMode) return false;
	if (!merged.IsValid()) return false;
	current = merged;
	return true;
}

template<class Options>
void ApplyFrameRefreshSettings(Options& options, const FrameRefreshSettings& settings, bool simple = false) noexcept {
	options.frameRefresh = settings;
	const auto effective = EffectiveFrameRefreshSettings(settings, simple);
	options.isVRREnabled = effective.vrrEnabled;
	const auto sync = effective.ContentSync();
	options.isFrontEdgeSyncEnabled = sync.enabled;
	options.frontEdgeSyncFrameRate = sync.frameRate;
	options.frameSyncMode = sync.mode;
	options.maxFrameRate = effective.ContentLimit();
	options.minFrameRate = effective.idleEnabled ? effective.idleRate : 0;
	options.cursorRefresh = effective.Cursor();
}

}
