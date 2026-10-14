#define wmain RtssExistingTestMain
#include "RtssProfileTransactionTests.cpp"
#undef wmain
#include "RtssProfileExpression.h"
#include "AdminStartupSettings.h"

int fileCommitMode = 0;
BOOL RtssTestReplaceFileW(LPCWSTR path, LPCWSTR replacement, LPCWSTR backup, DWORD flags, LPVOID exclude, LPVOID reserved) {
	if (fileCommitMode == 1) {
		if (!MoveFileExW(path, backup, 0)) return FALSE;
		SetLastError(ERROR_UNABLE_TO_MOVE_REPLACEMENT_2); return FALSE;
	}
	const BOOL result = ReplaceFileW(path, replacement, backup, flags, exclude, reserved);
	if (result && fileCommitMode == 2) WriteBytes(path, Profile(88, 1, 0, 1, 3, "external-after-commit"));
	return result;
}

void InitializationTests(const std::filesystem::path& output) {
	for (bool exists : { false, true }) for (uint32_t time : { 0u, 16667u, 1666667u }) {
		Fixture f(output); f.access._version = L"7.3.5.28314";
		WriteBytes(f.Path(""), Profile(97, 1000, time, 100, 0));
		if (exists) WriteBytes(f.Path("Magpie.exe"), Profile(59940, 1000, time, 100, 3, "custom-OSD"));
		const auto before = f.Snapshot("Magpie.exe");
		const auto global = ReadBytes(f.Path(""));
		RtssProfileSnapshot written;
		bool unlimited = true; std::wstring error;
		Require(f.access.InitializeUnlimited("Magpie.exe", written, unlimited, error, [] { return true; }), "Initialize creates or clears self profile");
		Require(!unlimited && written.exists && written.rate == std::array<uint32_t, 4>{0, 1000, 0, 100}, "Both cap forms cleared, denominators kept");
		Require(written.detectionLevel == before.detectionLevel && written.fields.at("other/value") == before.fields.at("other/value"), "Self detection and OSD kept without Low");
		Require(f.saves == 0 && f.setters.empty() && f.reloads == 1 && global == ReadBytes(f.Path("")), "Dedicated clear never saves Global or invokes SDK setters");
		const auto contents = written.contents;
		const auto identity = written.fileIdentity;
		Require(f.access.InitializeUnlimited("Magpie.exe", written, unlimited, error, [] { return true; }), "Repeated initialization succeeds");
		Require(unlimited && written.contents == contents && written.fileIdentity == identity && f.reloads == 1 && f.saves == 0 && f.setters.empty(), "Unlimited profile is a physical no-op");
	}
	for (int mode = 0; mode < 9; ++mode) {
		Fixture f(output); f.access._version = L"7.3.5.28314";
		const bool exists = mode != 5;
		if (exists) WriteBytes(f.Path("Magpie.exe"), Profile(60, 1, 16667));
		const auto original = exists ? ReadBytes(f.Path("Magpie.exe")) : ReadBytes(f.Path(""));
		RtssProfileSnapshot written; bool unlimited = false; std::wstring error;
		int validityCalls = 0;
		if (mode == 0) f.access._update = nullptr;
		if (mode == 3) f.reloadFault = true;
		if (mode == 6) SetFileAttributesW(f.Path("Magpie.exe").c_str(), FILE_ATTRIBUTE_READONLY);
		if (mode == 7) f.access._get = nullptr;
		const HANDLE locked = mode == 8 ? CreateFileW(f.Path("Magpie.exe").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr) : INVALID_HANDLE_VALUE;
		const bool success = f.access.InitializeUnlimited(mode == 4 ? "game.exe" : "Magpie.exe", written, unlimited, error, [&] {
			++validityCalls;
			if (mode == 1) return validityCalls != 2;
			// Edit in the ReplaceFile race window, after the last content comparison.
			if ((mode == 2 || mode == 5) && validityCalls == 2) WriteBytes(f.Path("Magpie.exe"), Profile(42, 1, 0, 1, 3, "external"));
			return true;
		});
		if (locked != INVALID_HANDLE_VALUE) CloseHandle(locked);
		SetFileAttributesW(f.Path("Magpie.exe").c_str(), FILE_ATTRIBUTE_NORMAL);
		Require(!success && f.saves == 0 && f.setters.empty(), "Initialization failures never use a broad fallback");
		if (mode == 2 || mode == 5) Require(ReadBytes(f.Path("Magpie.exe")) == Profile(42, 1, 0, 1, 3, "external"), "Replacement race restores actual external input, new-profile race never overwrites");
		else if (mode != 3 && mode != 4) Require(ReadBytes(f.Path("Magpie.exe")) == original, "Failure before commit preserves original bytes");
		if (mode == 1) Require(error == L"Cancelled" && !f.access.Failure().saveAttempted, "Cancelled before commit");
		if (mode == 3) Require(f.access.Failure().nativeFault && f.access.Failure().stage == RtssFailureStage::Reload && written.rate[2] == 0, "Reload fault is distinct from verified disk write");
		if (mode == 6) Require(f.access.Failure().fileError == ERROR_ACCESS_DENIED, "Actual permission error");
	}
	std::string outputBytes;
	for (int mode : { 1, 2 }) {
		Fixture f(output); f.access._version = L"7.3.5.28314";
		WriteBytes(f.Path("Magpie.exe"), Profile(60));
		const auto original = ReadBytes(f.Path("Magpie.exe"));
		fileCommitMode = mode;
		RtssProfileSnapshot written; bool unlimited = false; std::wstring error;
		unsigned calls = 0;
		const bool result = f.access.InitializeUnlimited("Magpie.exe", written, unlimited, error, [&] {
			if (mode == 2 && ++calls == 2) WriteBytes(f.Path("Magpie.exe"), Profile(42));
			return true;
		});
		fileCommitMode = 0;
		Require(!result && f.access.Failure().saveAttempted && f.access.Failure().readbackComplete, "Partial replacement records actual readback");
		if (mode == 1) Require(ReadBytes(f.Path("Magpie.exe")) == original && f.access.Failure().fileError == ERROR_UNABLE_TO_MOVE_REPLACEMENT_2, "Missing destination recovered after partial ReplaceFile failure");
		else {
			Require(ReadBytes(f.Path("Magpie.exe")) == Profile(88, 1, 0, 1, 3, "external-after-commit"), "Post-commit external edit never rolled back");
			Require(!f.access.Failure().recoveryFile.empty() && ReadBytes(f.access.Failure().recoveryFile) == Profile(42), "Actual replaced external version retained for recovery");
		}
	}
	for (const std::string original : {
		";kept\r\n[Framerate]\r\n  Limit = 59940 \r\nLimitDenominator=1000\r\nLimitTime=1666667\r\nLimitTimeDenominator=100\r\n[OSD]\r\nValue=unchanged\r\n",
		"\xEF\xBB\xBF[Framerate]\nLimit=60\nLimitTime=16667",
		"[Framerate]\nLimitDenominator=1000\n[OSD]\nValue=preserve",
		"[OSD]\r\nValue=preserve" }) {
		Require(ClearRtssRateExpression(original, outputBytes), "Sparse, BOM and line-ending expressions supported");
		Require(outputBytes.find("LimitTime=0") != outputBytes.npos && outputBytes.find("1666667") == outputBytes.npos, "Only cap values replaced or inserted");
		if (original.starts_with("\xEF\xBB\xBF")) Require(outputBytes.starts_with("\xEF\xBB\xBF"), "UTF-8 BOM kept");
		if (original.find("Value=") != original.npos) Require(outputBytes.find(original.substr(original.find("[OSD]"))) != outputBytes.npos, "Non-rate bytes intact");
	}
	Require(!ClearRtssRateExpression("[Framerate]\nLimit=60\nLimit=30\n", outputBytes), "Duplicate rate rejected");
	Require(!ClearRtssRateExpression("[Framerate]\nLimit=60\n[Framerate]\nLimitTime=40\n", outputBytes), "Repeated rate section rejected");
	Require(!ClearRtssRateExpression(std::string("\xFF\xFE") + "[Framerate]", outputBytes), "Unknown UTF-16 rejected");
	for (const char* json : { "{}", "{\"alwaysRunAsAdmin\":false}", "{\"alwaysRunAsElevated\":false}",
		"{\"adminStartupDefaultsVersion\":0,\"alwaysRunAsAdmin\":false}" }) {
		rapidjson::Document root; root.Parse(json); bool enabled = false; uint32_t version = 0;
		Require(ReadAdminStartupSettings(static_cast<const rapidjson::Document&>(root).GetObj(), enabled, version) && enabled && version == 1, "First load and old defaults migrate once");
	}
	for (bool choice : { false, true }) {
		rapidjson::Document root;
		root.Parse(choice ? "{\"adminStartupDefaultsVersion\":1,\"alwaysRunAsAdmin\":true}" : "{\"adminStartupDefaultsVersion\":1,\"alwaysRunAsAdmin\":false}");
		bool enabled = !choice; uint32_t version = 0;
		Require(!ReadAdminStartupSettings(root, enabled, version) && enabled == choice && version == 1, "Later deliberate preference preserved");
	}
}

void NativeInitializationTests(const std::filesystem::path& directory) {
	const auto executable = directory / L"RTSS.exe";
	DWORD ignored = 0;
	std::vector<uint8_t> version(GetFileVersionInfoSizeW(executable.c_str(), &ignored));
	Require(!version.empty() && GetFileVersionInfoW(executable.c_str(), 0, DWORD(version.size()), version.data()), "Read actual copied RTSS version");
	VS_FIXEDFILEINFO* info = nullptr; UINT size = 0;
	Require(VerQueryValueW(version.data(), L"\\", reinterpret_cast<void**>(&info), &size) && size >= sizeof(*info), "Copied RTSS version resource");
	const auto text = std::to_wstring(HIWORD(info->dwFileVersionMS)) + L"." + std::to_wstring(LOWORD(info->dwFileVersionMS)) +
		L"." + std::to_wstring(HIWORD(info->dwFileVersionLS)) + L"." + std::to_wstring(LOWORD(info->dwFileVersionLS));
	Require(text == L"7.3.5.28314", "Only verified native profile format");
	RtssProfileAccess access; std::wstring error;
	Require(access.Initialize({executable, text, false}, error), "Native self-profile access initialize");
	RtssProfileSnapshot global;
	Require(access.Read("", global, error) && global.rate[0] == 97, "Native initialization isolation sentinel");
	const auto originalGlobal = global.contents;
	const auto selfName = access.SelfProfile();
	Require(!selfName.empty() && selfName != "Magpie.exe", "Native initialization uses actual renamed test executable");
	const auto self = directory / "Profiles" / (selfName + ".cfg");
	for (bool exists : { false, true }) for (uint32_t time : { 0u, 16667u, 1666667u }) {
		if (exists) {
			auto bytes = global.contents;
			ReplaceField(bytes, "Limit", "59940"); ReplaceField(bytes, "LimitDenominator", "1000");
			ReplaceField(bytes, "LimitTime", std::to_string(time)); ReplaceField(bytes, "LimitTimeDenominator", time == 1666667 ? "100" : "1");
			WriteBytes(self, bytes);
		} else {
			std::filesystem::remove(self);
			auto bytes = originalGlobal;
			ReplaceField(bytes, "LimitTime", std::to_string(time));
			ReplaceField(bytes, "LimitTimeDenominator", time == 1666667 ? "100" : "1");
			WriteBytes(directory / "Profiles/Global", bytes);
		}
		const auto currentGlobal = ReadBytes(directory / "Profiles/Global");
		RtssProfileSnapshot before, written;
		Require(access.Read(selfName, before, error), "Native self baseline");
		bool unlimited = false;
		if (!access.InitializeUnlimited(selfName, written, unlimited, error, [] { return true; })) {
			std::wcerr << L"Native initialization: " << error << L" stage=" << int(access.Failure().stage) << L"\n";
			throw std::runtime_error("Native self initialization failed");
		}
		Require(!unlimited && written.exists && !written.rate[0] && !written.rate[2] && written.rate[1] == before.rate[1] && written.rate[3] == before.rate[3], "Native SDK and disk agree on unlimited expression");
		std::string expected;
		Require(ClearRtssRateExpression(before.contents, expected) && written.contents == expected && written.detectionLevel == before.detectionLevel, "Native detection and all other bytes preserved");
		const auto identity = written.fileIdentity;
		Require(access.InitializeUnlimited(selfName, written, unlimited, error, [] { return true; }) && unlimited && written.fileIdentity == identity, "Native repeat does not replace the file");
		Require(ReadBytes(directory / "Profiles/Global") == currentGlobal, "Native Global unchanged by self initialization");
	}
	WriteBytes(directory / "Profiles/Global", originalGlobal);
}

int wmain(int argc, wchar_t** argv) {
	try {
		if (argc != 2 && argc != 3) throw std::runtime_error("Expected isolated output directory and optional installation copy");
		InitializationTests(std::filesystem::path(argv[1]) / ("init-run-" + std::to_string(GetTickCount64())));
		std::cout << "RTSS initialization and admin migration: " << checks << " checks passed.\n";
		if (argc == 3) {
			const auto before = checks; NativeInitializationTests(argv[2]);
			std::cout << "RTSS isolated native self initialization: " << checks - before << " checks passed.\n";
		}
		return 0;
	} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
