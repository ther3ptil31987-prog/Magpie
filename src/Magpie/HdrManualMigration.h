#pragma once
#include "HdrComponents.h"
#include <map>
#include <optional>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <type_traits>

namespace Magpie {

// This schema is read only, and is used solely to validate and unfold trial
// configurations. It cannot generate, adopt, reorder or synchronize effects.
namespace HdrManualMigration {
struct LegacySettings {
	bool managed = false;
	unsigned input = 0;
	unsigned output = 0;
	bool nativeLegacy = false;
	float whiteNits = 203, peakNits = 1000, exposure = 1, shoulder = 1;
	float nativeWhiteNits = 203, legacyWhiteNits = 203, legacyExposure = 1, legacyPeakNits = 1000;
	float rtxPeakNits = 1000, rtxContrast = 100, rtxSaturation = 100, rtxMiddleGray = 50;
	std::optional<bool> CaptureIntent() const {
		return input == 0 ? std::nullopt : std::optional<bool>(input == 2);
	}
};

inline constexpr std::pair<const char*, float LegacySettings::*> FIELDS[] = {
	{"whiteNits", &LegacySettings::whiteNits}, {"peakNits", &LegacySettings::peakNits},
	{"exposure", &LegacySettings::exposure}, {"shoulder", &LegacySettings::shoulder},
	{"nativeWhiteNits", &LegacySettings::nativeWhiteNits}, {"legacyWhiteNits", &LegacySettings::legacyWhiteNits},
	{"legacyExposure", &LegacySettings::legacyExposure}, {"legacyPeakNits", &LegacySettings::legacyPeakNits},
	{"rtxPeakNits", &LegacySettings::rtxPeakNits}, {"rtxContrast", &LegacySettings::rtxContrast},
	{"rtxSaturation", &LegacySettings::rtxSaturation}, {"rtxMiddleGray", &LegacySettings::rtxMiddleGray}
};

inline bool ReadLegacySettings(const rapidjson::Value& value, LegacySettings& settings) {
	if (!value.IsObject() || !value.HasMember("version") || !value["version"].IsUint() || value["version"].GetUint() != 1)
		return false;
	for (const char* key : {"input", "output"}) if (!value.HasMember(key) || !value[key].IsUint()) return false;
	for (const char* key : {"managed", "nativeLegacy"}) if (!value.HasMember(key) || !value[key].IsBool()) return false;
	settings.input = value["input"].GetUint(); settings.output = value["output"].GetUint();
	settings.managed = value["managed"].GetBool(); settings.nativeLegacy = value["nativeLegacy"].GetBool();
	if (settings.input > 2 || settings.output > 4 || (settings.managed && settings.input == 0)) return false;
	for (const auto& [key, member] : FIELDS) {
		if (!value.HasMember(key) || !value[key].IsNumber()) return false;
		settings.*member = value[key].GetFloat();
	}
	auto range = [](float v, float lo, float hi) { return std::isfinite(v) && v >= lo && v <= hi; };
	auto integer = [&](float v, float lo, float hi) { return range(v, lo, hi) && std::floor(v) == v; };
	return range(settings.whiteNits, 80, 400) && range(settings.peakNits, 400, 4000) &&
		range(settings.exposure, .1f, 4) && range(settings.shoulder, .1f, 4) && range(settings.nativeWhiteNits, 8, 4000) &&
		range(settings.legacyWhiteNits, 80, 400) && range(settings.legacyExposure, .1f, 4) && range(settings.legacyPeakNits, 400, 4000) &&
		integer(settings.rtxPeakNits, 400, 2000) && integer(settings.rtxContrast, 0, 200) &&
		integer(settings.rtxSaturation, 0, 200) && integer(settings.rtxMiddleGray, 10, 100);
}

template<class String> std::string Ascii(const String& text) {
	std::string result;
	for (auto ch : text) {
		if (ch < 0 || ch > 127) return {}; // Only the converter identifiers need ASCII classification.
		result.push_back(static_cast<char>(ch));
	}
	return result;
}

template<class Effect> auto Key(std::string_view key) {
	using String = typename std::remove_cvref_t<decltype(std::declval<Effect>().parameters)>::key_type;
	return String(key.begin(), key.end());
}

template<class Effect> float Value(const Effect& effect, const char* key, float fallback) {
	const auto it = effect.parameters.find(Key<Effect>(key));
	return it == effect.parameters.end() ? fallback : it->second;
}

template<class Effects> HdrComponentPlan Plan(const Effects& effects, std::optional<bool> intent) {
	struct ColorEffect { std::string name; std::map<std::string, float> parameters; };
	std::vector<ColorEffect> colors;
	for (const auto& effect : effects) {
		ColorEffect color{Ascii(effect.name)};
		for (const auto& [key, value] : effect.parameters) color.parameters.emplace(Ascii(key), value);
		colors.push_back(std::move(color));
	}
	return BuildHdrComponentPlan(colors, intent);
}

// Compare the full execution contract, including pair sources and metadata.
// The list and parameters are never modified by migration.
inline bool SamePlan(const HdrComponentPlan& a, const HdrComponentPlan& b) {
	if (a.error != HdrComponentError::None || b.error != HdrComponentError::None || a.enabled != b.enabled ||
		a.captureHdr != b.captureHdr || a.outputHdr != b.outputHdr || a.stages.size() != b.stages.size()) return false;
	for (size_t i = 0; i < a.stages.size(); ++i) {
		const auto& x = a.stages[i]; const auto& y = b.stages[i];
		if (x.kind != y.kind || x.inputHdr != y.inputHdr || x.outputHdr != y.outputHdr || x.pairIndex != y.pairIndex ||
			x.mode != y.mode || x.whiteNits != y.whiteNits || x.peakNits != y.peakNits ||
			x.exposure != y.exposure || x.shoulder != y.shoulder) return false;
	}
	return true;
}

template<class Effects> bool ValidateLegacyRoles(const Effects& effects, const rapidjson::Value& array,
	const LegacySettings& settings, const HdrComponentPlan& plan) {
	if (!array.IsArray() || array.Size() != effects.size()) return false;
	for (size_t i = 0; i < effects.size(); ++i) {
		const auto& effect = effects[i]; const auto& node = array[static_cast<rapidjson::SizeType>(i)];
		if (!node.IsObject() || effect.isRecoveryInvalid) return false;
		unsigned role = 0;
		if (node.HasMember("hdrManagedRole")) {
			if (!node["hdrManagedRole"].IsUint()) return false;
			role = node["hdrManagedRole"].GetUint();
		}
		const auto name = Ascii(effect.name);
		const auto kind = ClassifyHdrComponent(name);
		if (!settings.managed) { if (role != 0) return false; continue; }
		if (role == 0) { if (kind != HdrComponentKind::None) return false; continue; }
		if (effect.HasScale()) return false;
		if (role == 1) {
			if (kind != HdrComponentKind::HdrToSdr || Value(effect, "mode", 0) != (settings.output == 4 ? 1.f : 0.f) ||
				Value(effect, "whiteNits", 203) != settings.whiteNits || Value(effect, "peakNits", 1000) != settings.peakNits ||
				Value(effect, "exposure", 1) != settings.exposure || Value(effect, "shoulder", 1) != settings.shoulder) return false;
		} else if (role == 2) {
			if (kind != HdrComponentKind::SdrToHdr || Value(effect, "mode", 0) != 2 ||
				plan.stages[i].pairIndex == size_t(-1)) return false;
		} else if (role == 3) {
			if (settings.output == 3) {
				if (kind != HdrComponentKind::RtxVideoHdr || Value(effect, "peakNits", 1000) != settings.rtxPeakNits ||
					Value(effect, "contrast", 100) != settings.rtxContrast || Value(effect, "saturation", 100) != settings.rtxSaturation ||
					Value(effect, "middleGray", 50) != settings.rtxMiddleGray) return false;
			} else {
				if (kind != HdrComponentKind::SdrToHdr || !(settings.output == 2 || (settings.output == 0 && settings.input == 1)) ||
					Value(effect, "mode", 0) != (settings.output == 0 ? 0.f : 1.f)) return false;
				const bool merged = effect.parameters.contains(Key<std::remove_cvref_t<decltype(effect)>>("nativeWhiteNits"));
				if (merged == settings.nativeLegacy) return false;
				if (merged) { if (Value(effect, "nativeWhiteNits", 203) != settings.nativeWhiteNits) return false; }
				else if (Value(effect, "whiteNits", 203) != settings.legacyWhiteNits ||
					Value(effect, "exposure", 1) != settings.legacyExposure || Value(effect, "peakNits", 1000) != settings.legacyPeakNits) return false;
			}
		} else if (role == 4) {
			if (name != "Bicubic" || !effect.parameters.empty()) return false;
		} else return false;
	}
	if (settings.managed && settings.output == 1 && std::ranges::none_of(plan.stages,
		[](const auto& stage) { return stage.pairIndex != size_t(-1); })) return false;
	return !settings.managed || plan.outputHdr == (settings.output != 4);
}

inline std::string Snapshot(const rapidjson::GenericObject<true, rapidjson::Value>& value) {
	rapidjson::StringBuffer raw;
	rapidjson::Writer<rapidjson::StringBuffer> writer(raw);
	writer.StartObject();
	for (const auto& member : value) {
		writer.Key(member.name.GetString(), member.name.GetStringLength());
		member.value.Accept(writer);
	}
	writer.EndObject();
	return {raw.GetString(), raw.GetSize()};
}

template<class Mode> bool ReadOrMigrate(const rapidjson::GenericObject<true, rapidjson::Value>& object, Mode& mode) {
	const auto hdr = object.FindMember("hdr");
	const auto migrated = object.FindMember("hdrManualMigration");
	const auto effects = object.FindMember("effects");
	bool roles = false;
	if (effects != object.MemberEnd() && effects->value.IsArray())
		for (const auto& node : effects->value.GetArray()) roles |= node.IsObject() && node.HasMember("hdrManagedRole");
	auto fail = [&] {
		mode.hdrMigrationInvalid = true;
		mode.hdrMigrationRecovery = Snapshot(object);
		return false;
	};
	if (mode.hdrMigrationInvalid) return fail();
	if (hdr == object.MemberEnd() && migrated == object.MemberEnd() && !roles) return true;
	if (migrated != object.MemberEnd()) {
		// A saved manual list is authoritative. The backup never re-applies its
		// old settings, so editing and reloading cannot regenerate nodes.
		if (hdr != object.MemberEnd() || roles) return fail();
		const auto& value = migrated->value;
		if (!value.IsObject() || !value.HasMember("version") || !value["version"].IsUint() || value["version"].GetUint() != 1 ||
			!value.HasMember("original") || !value["original"].IsString()) return fail();
		const auto& original = value["original"];
		rapidjson::Document backup;
		backup.Parse(original.GetString(), original.GetStringLength());
		if (backup.HasParseError() || !backup.IsObject() || backup.HasMember("hdrManualMigration")) return fail();
		LegacySettings archived;
		if (!backup.HasMember("hdr") || !ReadLegacySettings(backup["hdr"], archived)) return fail();
		mode.hdrMigrationBackup.assign(original.GetString(), original.GetStringLength());
		if (value.HasMember("captureHdr")) {
			if (!value["captureHdr"].IsBool() || archived.CaptureIntent() != std::optional<bool>(value["captureHdr"].GetBool())) return fail();
			mode.legacyHdrCaptureIntent = value["captureHdr"].GetBool();
		}
		return true;
	}
	if (hdr == object.MemberEnd() || effects == object.MemberEnd()) return fail();
	LegacySettings settings;
	if (!ReadLegacySettings(hdr->value, settings)) return fail();
	const auto before = Plan(mode.effects, settings.CaptureIntent());
	if (before.error != HdrComponentError::None || mode.effects.empty() ||
		!ValidateLegacyRoles(mode.effects, effects->value, settings, before)) return fail();
	// Drop capture intent when explicit nodes already give exactly the same
	// contract. Direct HDR/SDR trial chains retain it without adding converters.
	const auto inferred = Plan(mode.effects, std::nullopt);
	mode.legacyHdrCaptureIntent = SamePlan(before, inferred) ? std::nullopt : settings.CaptureIntent();
	if (!SamePlan(before, Plan(mode.effects, mode.legacyHdrCaptureIntent))) return fail();
	mode.hdrMigrationBackup = Snapshot(object);
	return true;
}
} // namespace HdrManualMigration
} // namespace Magpie
