#include "RtssProfileAccess.g.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string_view>

using namespace Magpie;
#include "RtssFailureText.g.h"

namespace {
unsigned checks = 0;
void Require(bool condition, const char* message) {
	++checks;
	if (!condition) throw std::runtime_error(message);
}
std::string ReadBytes(const std::filesystem::path& path) {
	std::ifstream stream(path, std::ios::binary);
	return { std::istreambuf_iterator<char>(stream), {} };
}
void WriteBytes(const std::filesystem::path& path, const std::string& bytes) {
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	stream << bytes;
	if (!stream) throw std::runtime_error("Test profile write failed");
}
std::string Profile(uint32_t rate = 0, uint32_t denominator = 1, uint32_t time = 0, uint32_t timeDenominator = 1,
	uint32_t detection = 3, std::string extra = "original") {
	return "[Framerate]\nLimit=" + std::to_string(rate) + "\nLimitDenominator=" + std::to_string(denominator) +
		"\nLimitTime=" + std::to_string(time) + "\nLimitTimeDenominator=" + std::to_string(timeDenominator) +
		"\n[Hooking]\nDetection=" + std::to_string(detection) + "\n[Other]\nValue=" + extra + "\n";
}
void ReplaceField(std::string& bytes, const std::string& key, const std::string& value) {
	const auto start = bytes.find(key + "=");
	Require(start != std::string::npos, "Native rate field");
	const auto end = bytes.find_first_of("\r\n", start);
	bytes.replace(start, end == std::string::npos ? end : end - start, key + "=" + value);
}
struct Fixture;
Fixture* active = nullptr;
enum class SaveMode { Normal, Noop, Partial, BadFile, Fault };
struct Fixture {
	RtssProfileAccess access;
	std::filesystem::path directory;
	std::map<std::string, uint32_t> loaded;
	std::vector<std::string> setters;
	std::string extra = "original", reject, ignore, getFailure;
	std::function<void()> afterSet;
	SaveMode saveMode = SaveMode::Normal;
	bool reloadFault = false, setterFault = false, deleteNoop = false;
	unsigned saves = 0, reloads = 0, deletes = 0;

	explicit Fixture(const std::filesystem::path& output) {
		static unsigned number = 0;
		directory = output / ("mock-" + std::to_string(++number));
		std::filesystem::create_directories(directory / "Profiles");
		WriteBytes(directory / "Profiles/Global", Profile());
		access._directory = directory;
		access._module = reinterpret_cast<HMODULE>(1); // The destructor must never FreeLibrary this fixture.
		access._version = L"mock-7.3.5";
		access._selfProfile = "Magpie.exe";
		access._load = Load; access._save = Save; access._delete = Delete;
		access._get = Get; access._set = Set; access._update = Update;
		active = this;
	}
	~Fixture() { access._module = nullptr; active = nullptr; }
	std::filesystem::path Path(const std::string& name) const {
		return directory / "Profiles" / (name.empty() ? "Global" : name + ".cfg");
	}
	RtssProfileSnapshot Snapshot(const std::string& name) {
		RtssProfileSnapshot result;
		std::wstring error;
		Require(access.Read(name, result, error), "Snapshot read");
		return result;
	}
	static void Load(LPCSTR name) {
		auto path = active->Path(name);
		if (!std::filesystem::exists(path)) path = active->Path("");
		const std::map<std::string, std::string> keys{
			{"Limit", "FramerateLimit"}, {"LimitDenominator", "FramerateLimitDenominator"},
			{"LimitTime", "FrametimeLimit"}, {"LimitTimeDenominator", "TimeDenominator"}, {"Detection", "AppDetectionLevel"}};
		active->loaded = {{"FramerateLimit", 0}, {"FramerateLimitDenominator", 1}, {"FrametimeLimit", 0},
			{"TimeDenominator", 1}, {"AppDetectionLevel", 3}};
		std::istringstream lines(ReadBytes(path));
		for (std::string line; std::getline(lines, line);) {
			const auto split = line.find('=');
			if (split == std::string::npos) continue;
			const auto key = line.substr(0, split), value = line.substr(split + 1);
			if (const auto it = keys.find(key); it != keys.end()) active->loaded[it->second] = uint32_t(std::stoul(value));
			if (key == "Value") active->extra = value;
		}
	}
	static BOOL Get(LPCSTR name, LPBYTE bytes, DWORD size) {
		if (size != sizeof(uint32_t) || active->getFailure == name || !active->loaded.contains(name)) return FALSE;
		*reinterpret_cast<uint32_t*>(bytes) = active->loaded[name];
		return TRUE;
	}
	static BOOL Set(LPCSTR name, LPBYTE bytes, DWORD size) {
		active->setters.emplace_back(name);
		if (active->setterFault) RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
		if (size != sizeof(uint32_t) || active->reject == name) return FALSE;
		const uint32_t value = *reinterpret_cast<uint32_t*>(bytes);
		// Faithful edge semantics: the getter accepts 0 but this setter rejects it.
		if (std::string_view(name) == "FrametimeLimit" && value < 1000 * active->loaded["TimeDenominator"]) return FALSE;
		if (std::string_view(name) == "FramerateLimitDenominator" && !value) return FALSE;
		if (std::string_view(name) == "FramerateLimit" && value > 1000 * active->loaded["FramerateLimitDenominator"]) return FALSE;
		if (active->ignore != name) active->loaded[name] = value;
		if (active->afterSet) active->afterSet();
		return TRUE;
	}
	static void Save(LPCSTR name) {
		++active->saves;
		if (active->saveMode == SaveMode::Fault) RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
		if (active->saveMode == SaveMode::Noop) return;
		if (active->saveMode == SaveMode::BadFile) { WriteBytes(active->Path(name), "[Framerate]\nLimit=bad\n"); return; }
		const auto& v = active->loaded;
		WriteBytes(active->Path(name), Profile(v.at("FramerateLimit"), v.at("FramerateLimitDenominator"),
			v.at("FrametimeLimit"), v.at("TimeDenominator"), active->saveMode == SaveMode::Partial ? 0 : v.at("AppDetectionLevel"), active->extra));
	}
	static void Delete(LPCSTR name) { ++active->deletes; if (!active->deleteNoop) std::filesystem::remove(active->Path(name)); }
	static void Update() { ++active->reloads; if (active->reloadFault) RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr); }
};

void CapabilityTests(const std::filesystem::path& output) {
	for (int missing = 0; missing < 6; ++missing) {
		Fixture f(output);
		if (missing == 0) f.access._delete = nullptr;
		if (missing == 1) f.access._save = nullptr;
		if (missing == 2) f.access._set = nullptr;
		if (missing == 3) f.access._update = nullptr;
		if (missing == 4) f.access._load = nullptr;
		if (missing == 5) f.access._get = nullptr;
		RtssProfileSnapshot before, written;
		std::wstring error; bool attempted = false, complete = false, unlimited = false;
		const bool read = f.access.Read("", before, error);
		Require(read == (missing < 4) && f.access.CanRead() == read, "Only Load/Get exports required for read");
		Require(f.access.CanApply() == (missing == 0), "Apply does not require Delete");
		Require(f.access.CanInitializeUnlimited() == (missing < 4), "Self command does not depend on save, set or delete");
		if (missing < 4) {
			const auto original = ReadBytes(f.Path(""));
			const bool applied = f.access.Apply("new.exe", 60, f.Snapshot("new.exe"), written, attempted, complete, error, [] { return true; });
			Require(applied == (missing == 0), "Missing export affects only dependent writes");
			if (missing) Require(!attempted && f.saves == 0 && f.setters.empty() && error == L"MissingInterface" && original == ReadBytes(f.Path("")), "Required export failure precedes mutation");
			Require(f.access.InitializeUnlimited("Magpie.exe", written, unlimited, error, [] { return true; }) == (missing != 3), "File initialization has independent export requirements");
			if (missing == 3) {
				f.access._running = false;
				Require(f.access.InitializeUnlimited("Magpie.exe", written, unlimited, error, [] { return true; }) && f.reloads == 0, "Offline clear works without UpdateProfiles");
			}
		}
	}
	for (const std::string property : { "AppDetectionLevel", "FrametimeLimit", "FramerateLimitDenominator", "FramerateLimit" }) {
		Fixture f(output); f.getFailure = property;
		RtssProfileSnapshot snapshot, written; std::wstring error; bool attempted = false, complete = false;
		Require(f.access.Read("", snapshot, error) && snapshot.rateKnown[0], "Optional getter does not discard readable fields");
		const bool applied = f.access.Apply("", 60, snapshot, written, attempted, complete, error, [] { return true; });
		Require(applied && attempted && complete, "Missing getter does not block an unchanged known time/denominator/detection value");
		if (!applied) Require(!attempted && f.saves == 0 && error == L"ReadPropertyFailed" && f.access.Failure().property == property, "Required property absence is precise and does not save");
	}
	for (auto property : {"FrametimeLimit", "FramerateLimitDenominator"}) {
		Fixture f(output); f.getFailure = property;
		WriteBytes(f.Path(""), std::string_view(property) == "FrametimeLimit" ? Profile(30, 1, 16667) : Profile(30, 2));
		const auto before = f.Snapshot("");
		RtssProfileSnapshot written; std::wstring error; bool attempted = false, complete = false;
		const bool applied = f.access.Apply("", 60, before, written, attempted, complete, error, [] { return true; });
		if (std::string_view(property) == "FrametimeLimit")
			Require(!applied && !attempted && f.saves == 0 && error == L"TimeModeUnsupported" && f.access.Failure().property == property,
				"Actual time setter refusal is not disguised as missing getter");
		else Require(applied && attempted && complete && written.rate[1] == 1,
			"Changed denominator can verify accepted setter through explicit saved field");
	}
	for (int missing = 0; missing < 4; ++missing) {
		Fixture f(output);
		std::string bytes = Profile();
		const char* fields[] = {"Detection=3\n", "LimitTime=0\n", "LimitDenominator=1\n", "LimitTimeDenominator=1\n"};
		const char* properties[] = {"AppDetectionLevel", "FrametimeLimit", "FramerateLimitDenominator", "unused"};
		bytes.erase(bytes.find(fields[missing]), std::string(fields[missing]).size());
		WriteBytes(f.Path(""), bytes); f.getFailure = properties[missing];
		const auto snapshot = f.Snapshot("");
		Require(missing == 0 ? !snapshot.detectionKnown : !snapshot.rateKnown[missing == 1 ? 2 : missing == 2 ? 1 : 3], "Omitted and unavailable information stays unknown");
		Require(f.saves == 0 && f.setters.empty(), "Read capability never probes by writing");
	}
	for (auto version : { L"7.3.4.0", L"7.3.5.28314", L"7.4.99.999", L"" }) {
		Fixture f(output); f.access._version = version;
		WriteBytes(f.Path("Magpie.exe"), Profile(60, 1000, 16667, 100, 3, "kept"));
		const auto global = ReadBytes(f.Path("")); RtssProfileSnapshot written;
		std::wstring error; bool unlimited = false;
		Require(f.access.InitializeUnlimited("Magpie.exe", written, unlimited, error, [] { return true; }) && !unlimited && written.rate[0] == 0 && written.rate[2] == 0, "Version metadata does not gate identical initialization ability");
		Require(global == ReadBytes(f.Path("")) && written.rate[1] == 1000 && written.rate[3] == 100 && f.access.Failure().version == version, "Version diagnostic and non-cap fields preserved");
		f.access._update = nullptr;
		Require(f.access.InitializeUnlimited("Magpie.exe", written, unlimited, error, [] { return true; }) && unlimited && f.reloads == 1, "Unlimited no-op needs no reload capability");
	}
	for (bool exists : { false, true }) {
		Fixture f(output); f.access._running = false; f.access._save = nullptr; f.access._set = nullptr;
		f.access._delete = nullptr; f.access._update = nullptr;
		if (exists) WriteBytes(f.Path("Magpie.exe"), Profile(30));
		RtssProfileSnapshot written; std::wstring error; bool unlimited = false;
		Require(f.access.InitializeUnlimited("Magpie.exe", written, unlimited, error, [] { return true; }) && written.exists && written.rate[0] == 0 && written.rate[2] == 0, "Offline initialization persists and reads back");
		Require(f.reloads == 0 && f.saves == 0 && f.setters.empty() && !f.access.CanApply(), "Offline operation never invokes reload or live cap");
	}
	{
		Fixture f(output); f.access._selfProfile = "RenamedMagpie.exe";
		RtssProfileSnapshot written; std::wstring error; bool unlimited = false;
		for (const std::string name : {"", "Magpie.exe", "game.exe", "../RenamedMagpie.exe"})
			Require(!f.access.InitializeUnlimited(name, written, unlimited, error, [] { return true; }) && error == L"InvalidSelfProfile", "Self binding cannot be redirected to Global/game/old name/path");
		Require(f.access.InitializeUnlimited("RenamedMagpie.exe", written, unlimited, error, [] { return true; }) && !std::filesystem::exists(f.Path("Magpie.exe")), "Actual renamed self has independent profile");
	}
	{
		Fixture f(output);
		auto bytes = Profile(0, 2); bytes.erase(bytes.find("Limit=0\n"), std::string("Limit=0\n").size());
		WriteBytes(f.Path(""), bytes); f.getFailure = "FramerateLimit";
		const auto original = f.Snapshot("");
		Require(!original.rateKnown[0], "Original omitted FPS is unknown when getter is unavailable");
		f.afterSet = [&f] { f.getFailure.clear(); };
		RtssProfileSnapshot written; std::wstring error; bool attempted = false, complete = false, conflict = false;
		Require(f.access.Apply("", 60, original, written, attempted, complete, error, [] { return true; }), "Authorized apply can succeed when required getter becomes available during transaction");
		const auto setters = f.setters.size();
		Require(!f.access.Restore("", original, written, false, conflict, error) && error == L"ReadPropertyFailed" &&
			f.access.Failure().property == "FramerateLimit" && f.setters.size() == setters && f.saves == 1,
			"Stop does not guess unknown original FPS or impose restore guarantee before start");
	}
	{
		Fixture f(output);
		auto bytes = Profile(60); bytes.erase(bytes.find("Detection=3\n"), std::string("Detection=3\n").size());
		WriteBytes(f.Path("Magpie.exe"), bytes);
		RtssProfileSnapshot written; std::wstring error; bool unlimited = false;
		Require(f.access.InitializeUnlimited("Magpie.exe", written, unlimited, error, [&f] {
			f.getFailure = "AppDetectionLevel"; return true;
		}) && !written.detectionKnown && written.rate[0] == 0 && written.rate[2] == 0,
			"Optional getter becoming unavailable is not a profile conflict or a fabricated detection value");
		Require(written.contents.find("Detection=") == std::string::npos && f.saves == 0, "Optional metadata cannot replace non-rate bytes");
	}
	{
		Fixture f(output); f.getFailure = f.ignore = "FramerateLimit";
		const auto before = f.Snapshot(""); RtssProfileSnapshot written;
		std::wstring error; bool attempted = false, complete = false;
		Require(!f.access.Apply("", 60, before, written, attempted, complete, error, [] { return true; }) && attempted && complete && written.rate[0] == 0 && f.reloads == 0,
			"Accepted-but-ignored setter is rejected by disk readback when getter is absent");
	}
	for (auto name : {L"Magpie.exe", L"Renamed Magpie.exe", L"UPPER.EXE"}) Require(!RtssProfileAccess::ProfileName(name).empty(), "Legal trusted filename conversion");
	for (auto name : {L"", L"Global", L"../Magpie.exe", L"C:\\Magpie.exe", L"bad\"name.exe", L"app.dll"}) Require(RtssProfileAccess::ProfileName(name).empty(), "Invalid profile filenames rejected");
}

void MockTests(const std::filesystem::path& output) {
	for (const std::string name : {"", "existing.exe", "new.exe"}) for (const uint32_t original : {0u, 30u, 60u}) {
		Fixture f(output);
		const bool created = name == "new.exe";
		WriteBytes(f.Path(""), Profile(original, 1, 0, 1, created ? 0 : 3));
		if (!name.empty() && !created) WriteBytes(f.Path(name), Profile(original));
		const auto before = f.Snapshot(name);
		RtssProfileSnapshot written;
		bool attempted = false, complete = false, conflict = false;
		std::wstring error;
		Require(f.access.Apply(name, 60, before, written, attempted, complete, error, [] { return true; }), "Normal FPS apply");
		Require(attempted && complete && written.exists && written.rate == std::array<uint32_t, 4>{60, 1, 0, 1}, "Saved and read back");
		Require(std::count(f.setters.begin(), f.setters.end(), "FrametimeLimit") == 0, "Zero time setter skipped");
		Require(written.detectionLevel == (created ? 1u : 3u), "New Low or existing detection preserved");
		Require(f.access.Failure().stage == RtssFailureStage::None, "Successful stage cleared");
		if (created) Require(f.access.Restore(name, before, written, true, conflict, error) && f.deletes == 1 && !f.Snapshot(name).exists, "New profile inheritance restored");
		else {
			WriteBytes(f.Path(name), Profile(60, 1, 0, 1, 3, "external-non-rate"));
			Require(f.access.Restore(name, before, written, false, conflict, error), "Existing original restored");
			const auto restored = f.Snapshot(name);
			Require(restored.rate == before.rate && restored.fields.at("other/value") == "external-non-rate", "Restore preserves non-rate edit");
		}
		Require(!conflict && std::count(f.setters.begin(), f.setters.end(), "FrametimeLimit") == 0, "Restore also skips zero setter");
	}
	for (const auto denominator : {100u, 1000u}) {
		Fixture f(output);
		WriteBytes(f.Path("fractional.exe"), Profile(5994 * (denominator / 100), denominator));
		const auto before = f.Snapshot("fractional.exe");
		RtssProfileSnapshot written;
		bool attempted = false, complete = false, conflict = false;
		std::wstring error;
		Require(f.access.Apply("fractional.exe", 60, before, written, attempted, complete, error, [] { return true; }), "Fractional original apply");
		Require(f.access.Restore("fractional.exe", before, written, false, conflict, error), "Fractional original restore");
		Require(f.Snapshot("fractional.exe").rate == before.rate, "Exact fractional numerator/denominator restored");
	}
	for (int kind = 0; kind < 13; ++kind) {
		Fixture f(output);
		const std::string name = kind == 4 || kind == 5 ? "new.exe" : "";
		if (kind == 0) WriteBytes(f.Path(""), Profile(0, 1, 16667));
		const auto before = f.Snapshot(name);
		RtssProfileSnapshot written;
		bool attempted = false, complete = false;
		std::wstring error;
		if (kind == 1) f.reject = "FramerateLimit";
		if (kind == 2) f.ignore = "FramerateLimit";
		if (kind == 3) f.getFailure = "FramerateLimit";
		if (kind == 4 || kind == 11) f.saveMode = SaveMode::Noop;
		if (kind == 5) f.saveMode = SaveMode::Partial;
		if (kind == 6) f.saveMode = SaveMode::BadFile;
		if (kind == 7) f.reloadFault = true;
		if (kind == 8) f.setterFault = true;
		if (kind == 9) WriteBytes(f.Path(""), Profile(15));
		if (kind == 10) f.afterSet = [&f] { WriteBytes(f.Path(""), Profile(17)); };
		if (kind == 12) f.saveMode = SaveMode::Fault;
		const bool applied = f.access.Apply(name, 60, before, written, attempted, complete, error, [] { return true; });
		if (kind == 3) {
			Require(applied && attempted && complete && written.rate[0] == 60, "Getter absence alone cannot reject accepted and verified FPS setter");
			continue;
		}
		Require(!applied, "Fault must fail");
		const auto failure = f.access.Failure();
		Require(failure.saveAttempted == attempted && failure.readbackComplete == complete, "Transaction diagnostic flags");
		if (kind <= 2) {
			Require(!attempted && failure.stage == RtssFailureStage::Setting && failure.observedKnown, "Setting failure before save");
			Require(failure.property == (kind == 0 ? "FrametimeLimit" : "FramerateLimit"), "Specific rejected property");
			Require(error == (kind == 0 ? L"TimeModeUnsupported" : kind == 1 ? L"SetPropertyFailed" : L"SetReadbackFailed"), "Specific setter reason");
		}
				if (kind == 4 || kind == 11) Require(attempted && complete && failure.stage == RtssFailureStage::Save, "Void save no-op detected");
		if (kind == 5 || kind == 6) Require(attempted && failure.stage == RtssFailureStage::Verify && f.reloads == 0, "Partial/invalid save verified before reload");
		if (kind == 7 || kind == 8 || kind == 12) Require(failure.nativeFault && error == L"InterfaceFault", "Native faults remain distinct");
		if (kind == 9 || kind == 10) Require(!attempted && error == L"Conflict", "External edits never overwritten");
	}
	{
		Fixture f(output);
		const auto before = f.Snapshot("");
		RtssProfileSnapshot written;
		bool attempted = false, complete = false;
		std::wstring error;
		Require(!f.access.Apply("", 60, before, written, attempted, complete, error, [] { return false; }) &&
			error == L"Cancelled" && !attempted && f.saves == 0, "Cancellation before save");
	}
	for (const bool created : {false, true}) {
		Fixture f(output);
		const std::string name = created ? "new.exe" : "";
		const auto before = f.Snapshot(name);
		RtssProfileSnapshot written;
		bool attempted = false, complete = false, conflict = false;
		std::wstring error;
		Require(f.access.Apply(name, 60, before, written, attempted, complete, error, [] { return true; }), "Conflict baseline write");
		WriteBytes(f.Path(name), Profile(created ? 60 : 59, 1, 0, 1, created ? 1 : 3, "external"));
		Require(!f.access.Restore(name, before, written, created, conflict, error) && conflict && f.deletes == 0 && f.saves == 1, "Restore respects external ownership");
	}
	{
		Fixture f(output);
		const auto before = f.Snapshot("new.exe");
		RtssProfileSnapshot written;
		bool attempted = false, complete = false, conflict = false;
		std::wstring error;
		Require(f.access.Apply("new.exe", 60, before, written, attempted, complete, error, [] { return true; }), "Deletion baseline write");
		f.deleteNoop = true;
		Require(!f.access.Restore("new.exe", before, written, true, conflict, error) && error == L"DeleteFailed", "Void deletion failure verified");
		std::filesystem::remove(f.Path("new.exe"));
		Require(f.access.Restore("new.exe", before, written, true, conflict, error), "Already removed is restored");
	}
	for (const auto stage : {RtssFailureStage::Read, RtssFailureStage::Setting, RtssFailureStage::Save,
		RtssFailureStage::Verify, RtssFailureStage::Reload, RtssFailureStage::Restore}) {
		RtssProfileFailure failure; failure.stage = stage;
		const std::wstring_view key = ProfileFailureText(failure, L"Failure", true);
		Require(key != L"Rtss_Failure_Write" || stage == RtssFailureStage::Save, "Failure stage UI mapping");
		failure.nativeFault = true;
		Require(std::wstring_view(ProfileFailureText(failure, L"Failure", true)) == L"Rtss_Failure_Interface", "Native fault UI mapping");
	}
	for (int kind = 0; kind < 6; ++kind) {
		Fixture f(output);
		const auto before = f.Snapshot("");
		RtssProfileSnapshot written;
		bool attempted = false, complete = false, conflict = false;
		std::wstring error;
		Require(f.access.Apply("", 60, before, written, attempted, complete, error, [] { return true; }), "Restore failure baseline");
		if (kind == 0) f.reject = "FramerateLimit";
		if (kind == 1) f.saveMode = SaveMode::Noop;
		if (kind == 2) f.saveMode = SaveMode::BadFile;
		if (kind == 3) f.reloadFault = true;
		if (kind == 4) f.saveMode = SaveMode::Fault;
		if (kind == 5) f.getFailure = "FramerateLimit";
		const bool restored = f.access.Restore("", before, written, false, conflict, error);
		if (kind == 5) { Require(restored && f.Snapshot("").rate == before.rate, "Restore verifies saved fields when getter is absent"); continue; }
		Require(!restored, "Restore failure reported once");
		Require(!conflict, "Ordinary restore failures are not conflicts");
		const auto failure = f.access.Failure();
		if (kind == 0) Require(failure.stage == RtssFailureStage::Setting && failure.property == "FramerateLimit" && !failure.saveAttempted, "Restore setter failure detail");
		if (kind == 1 || kind == 2) Require(failure.stage == RtssFailureStage::Verify && failure.saveAttempted, "Restore save readback failure detail");
		if (kind == 3 || kind == 4) Require(failure.nativeFault && error == L"InterfaceFault", "Restore native fault quarantined");
			}
	{
		Fixture f(output);
		const auto before = f.Snapshot("");
		Require(SetFileAttributesW(f.Path("").c_str(), FILE_ATTRIBUTE_READONLY) != FALSE, "Read-only profile fixture");
		RtssProfileSnapshot written;
		bool attempted = false, complete = false;
		std::wstring error;
		const bool applied = f.access.Apply("", 60, before, written, attempted, complete, error, [] { return true; });
		SetFileAttributesW(f.Path("").c_str(), FILE_ATTRIBUTE_NORMAL);
		Require(!applied && !attempted && f.saves == 0 && f.setters.empty(), "Write access failure before setter/save");
		Require(f.access.Failure().fileError == ERROR_ACCESS_DENIED &&
			std::wstring_view(ProfileFailureText(f.access.Failure(), error, false)) == L"Rtss_Failure_Permission", "Permission code and UI reason");
	}
	{
		Fixture f(output);
		const auto before = f.Snapshot("");
		const HANDLE locked = CreateFileW(f.Path("").c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		Require(locked != INVALID_HANDLE_VALUE, "File lock fixture");
		RtssProfileSnapshot written;
		bool attempted = false, complete = false;
		std::wstring error;
		const bool applied = f.access.Apply("", 60, before, written, attempted, complete, error, [] { return true; });
		CloseHandle(locked);
		Require(!applied && !attempted && f.access.Failure().fileError == ERROR_SHARING_VIOLATION, "File failure carries Win32 error");
	}
}

// The runner prepares a DLL/config copy with a unique Global value, proving
// directory isolation before any write or reload export is invoked.
void NativeTests(const std::filesystem::path& directory) {
	RtssProfileAccess access;
	std::wstring error;
	Require(access.Initialize({directory / L"RTSS.exe", L"isolated-installed-version", true}, error), "Native initialize");
	RtssProfileSnapshot global;
	Require(access.Read("", global, error) && global.rate[0] == 97, "Native isolation sentinel");
	for (const std::string name : {"", "existing.exe", "new.exe", "fractional.exe"}) {
		RtssProfileSnapshot before, written, after;
		bool attempted = false, complete = false, conflict = false;
		Require(access.Read(name, before, error), "Native read before");
		const bool created = !before.exists;
		if (!access.Apply(name, 60, before, written, attempted, complete, error, [] { return true; })) {
			std::wcerr << L"Native Apply " << directory.c_str() << L": " << error << L"\n";
			throw std::runtime_error("Native Apply failed");
		}
		Require(attempted && complete && written.exists && written.rate[0] == 60 && !written.rate[2], "Native save/API/disk/reload");
		Require(access.Read(name, after, error) && RtssProfileAccess::SameRate(after, written), "Native kept state");
		if (!access.Restore(name, before, written, created, conflict, error)) {
			std::wcerr << L"Native Restore: " << error << L"\n";
			throw std::runtime_error("Native Restore failed");
		}
		Require(!conflict && access.Read(name, after, error) && RtssProfileAccess::SameRate(before, after), "Native exact restore/delete");
		Require(after.detectionLevel == before.detectionLevel, "Native detection preserved/restored inheritance");
	}
	RtssProfileSnapshot time, written;
	bool attempted = false, complete = false;
	Require(access.Read("time.exe", time, error) && time.rate[2] > 0, "Native time-mode fixture");
	Require(!access.Apply("time.exe", 60, time, written, attempted, complete, error, [] { return true; }) &&
		error == L"TimeModeUnsupported" && !attempted && !complete, "Native unsupported time mode never saved");
	Require(access.Failure().property == "FrametimeLimit" && access.Failure().observed == time.rate[2], "Native rejected value diagnostic");
	bool conflict = false;
	// Seed a test-only FPS state to verify the reverse conversion separately.
	// This proves original-time restoration, not forward time-to-FPS support.
	for (const std::string name : {"time.exe", "time-fractional.exe"}) {
		Require(access.Read(name, time, error), "Native original time expression");
		auto bytes = time.contents;
		ReplaceField(bytes, "Limit", "60"); ReplaceField(bytes, "LimitTime", "0");
		WriteBytes(directory / L"Profiles" / (name + ".cfg"), bytes);
		Require(access.Read(name, written, error), "Native time restore baseline");
		Require(access.Restore(name, time, written, false, conflict, error), "Native time expression restore");
		RtssProfileSnapshot restored;
		Require(access.Read(name, restored, error) && restored.rate == time.rate, "Native exact time expression restored");
	}
	for (const bool created : {false, true}) {
		const std::string name = created ? "conflict-new.exe" : "existing.exe";
		RtssProfileSnapshot before, changed;
		Require(access.Read(name, before, error), "Native conflict baseline");
		Require(access.Apply(name, 60, before, written, attempted, complete, error, [] { return true; }), "Native conflict write");
		const auto path = directory / L"Profiles" / (name + ".cfg");
		auto bytes = written.contents;
		const auto offset = bytes.find("Limit=60"); Require(offset != std::string::npos, "Native conflict field");
		bytes.replace(offset, 8, "Limit=59"); WriteBytes(path, bytes);
		Require(!access.Restore(name, before, written, created, conflict, error) && conflict, "Native external edit protected");
		Require(access.Read(name, changed, error) && changed.exists && changed.rate[0] == 59, "Native external profile retained");
	}
	{
		RtssProfileSnapshot before, after;
		Require(access.Read("existing.exe", before, error), "Native non-rate edit baseline");
		Require(access.Apply("existing.exe", 60, before, written, attempted, complete, error, [] { return true; }), "Native non-rate edit apply");
		auto bytes = written.contents;
		const auto offset = bytes.find("ZoomRatio="); Require(offset != std::string::npos, "Native non-rate field");
		const auto end = bytes.find_first_of("\r\n", offset);
		bytes.replace(offset, end - offset, "ZoomRatio=4");
		WriteBytes(directory / L"Profiles/existing.exe.cfg", bytes);
		Require(access.Restore("existing.exe", before, written, false, conflict, error), "Native non-rate edit restore");
		Require(access.Read("existing.exe", after, error) && after.rate == before.rate && after.fields.at("osd/zoomratio") == "4", "Native non-rate edit preserved");
	}
	{
		RtssProfileSnapshot before;
		Require(access.Read("sparse.exe", before, error), "Native sparse expression read");
		Require(access.Apply("sparse.exe", 60, before, written, attempted, complete, error, [] { return true; }), "Native sparse expression apply");
		Require(!access.Restore("sparse.exe", before, written, false, conflict, error) && error == L"RestoreExpressionChanged" &&
			access.Failure().stage == RtssFailureStage::Verify, "SDK added fields are not reported as an identical original expression");
	}
}
}

int wmain(int argc, wchar_t** argv) {
	try {
		if (argc != 2 && argc != 3) throw std::runtime_error("Expected output directory and optional isolated RTSS directory");
		CapabilityTests(std::filesystem::path(argv[1]) / ("capability-run-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64())));
		MockTests(std::filesystem::path(argv[1]) / ("mock-run-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64())));
		std::cout << "RTSS production transaction mock: " << checks << " checks passed.\n";
		if (argc == 3) {
			const unsigned before = checks;
			NativeTests(argv[2]);
			std::cout << "RTSS isolated native transactions: " << checks - before << " checks passed.\n";
		}
		return 0;
	} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
