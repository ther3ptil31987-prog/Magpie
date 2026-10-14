#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Magpie {

struct RtssToolInfo {
	std::filesystem::path executable;
	std::wstring version;
	bool running = false;
};

struct RtssProfileSnapshot {
	// Complete rate expression, including the time denominator not exposed by this SDK.
	std::array<uint32_t, 4> rate{ 0, 1, 0, 1 };
	std::map<std::string, std::string> fields;
	std::string contents;
	std::array<uint64_t, 2> fileIdentity{};
	bool exists = false;
	std::array<bool, 4> rateKnown{};
	uint32_t detectionLevel = 0;
	bool detectionKnown = false;
	bool operator==(const RtssProfileSnapshot&) const = default;
};

enum class RtssFailureStage { None, Read, Setting, Save, Verify, Reload, Restore, Conflict, Cancelled };

struct RtssProfileFailure {
	RtssFailureStage stage = RtssFailureStage::None;
	std::string profile, property;
	std::wstring version;
	std::wstring recoveryFile;
	uint32_t requested = 0, observed = 0;
	DWORD fileError = 0;
	bool observedKnown = false, saveAttempted = false, readbackComplete = false, nativeFault = false;
};

// Worker-thread only. The DLL maintains a current loaded profile, so callers
// must serialize complete transactions, not just individual exported calls.
class RtssProfileAccess {
public:
	~RtssProfileAccess();
	RtssProfileAccess() = default;
	RtssProfileAccess(const RtssProfileAccess&) = delete;
	RtssProfileAccess& operator=(const RtssProfileAccess&) = delete;
	static RtssToolInfo DetectTool(bool afterburner);
	static std::string ProfileName(const std::wstring& name);
	static bool LaunchTool(const RtssToolInfo& tool, std::wstring& error);
	bool Initialize(const RtssToolInfo& tool, std::wstring& error);
	bool Read(const std::string& profile, RtssProfileSnapshot& snapshot, std::wstring& error);
	bool Apply(const std::string& profile, uint32_t rate, const RtssProfileSnapshot& before,
		RtssProfileSnapshot& written, bool& saveAttempted, bool& readbackComplete,
		std::wstring& error, const std::function<bool()>& stillValid);
	bool ReloadProfiles(std::wstring& error, const std::function<bool()>& stillValid);
	bool Restore(const std::string& profile, const RtssProfileSnapshot& original,
		const RtssProfileSnapshot& lastWritten, bool created, bool& conflict, std::wstring& error);
	bool InitializeUnlimited(const std::string& profile, RtssProfileSnapshot& written,
		bool& alreadyUnlimited, std::wstring& error, const std::function<bool()>& stillValid);
	const RtssProfileFailure& Failure() const noexcept { return _failure; }
	bool CanRead() const noexcept { return _module && !_faulted && _load && _get; }
	bool CanApply() const noexcept { return CanRead() && _set && _save && _running && _update; }
	bool CanInitializeUnlimited() const noexcept { return CanRead() && !_selfProfile.empty(); }
	const std::string& SelfProfile() const noexcept { return _selfProfile; }
	const std::filesystem::path& Directory() const noexcept { return _directory; }
	static bool SameInput(const RtssProfileSnapshot& a, const RtssProfileSnapshot& b) noexcept {
		if (a.exists != b.exists || a.contents != b.contents || a.fileIdentity != b.fileIdentity) return false;
		for (size_t i = 0; i < a.rate.size(); ++i)
			if (a.rateKnown[i] && b.rateKnown[i] && a.rate[i] != b.rate[i]) return false;
		return !a.detectionKnown || !b.detectionKnown || a.detectionLevel == b.detectionLevel;
	}
	static bool SameRate(const RtssProfileSnapshot& a, const RtssProfileSnapshot& b) noexcept {
		return a.exists == b.exists && a.rate == b.rate && a.rateKnown == b.rateKnown;
	}

private:
	using ProfileFn = void (*)(LPCSTR);
	using PropertyFn = BOOL (*)(LPCSTR, LPBYTE, DWORD);
	std::filesystem::path _ProfilePath(const std::string& profile) const;
	bool _Exists(const std::string& profile, bool& exists, std::wstring& error);
	bool _ReadFile(const std::string& profile, RtssProfileSnapshot& snapshot, std::wstring& error);
	bool _Read(const std::string& profile, RtssProfileSnapshot& snapshot, std::wstring& error);
	bool _CheckWriteAccess(const std::string& profile, bool exists, std::wstring& error);
	bool _WriteUnlimited(const std::string& profile, const RtssProfileSnapshot& before,
		const std::string& contents, RtssProfileSnapshot& written, std::wstring& error,
		const std::function<bool()>& stillValid);
	bool _RequireInterface(bool available, RtssFailureStage stage, const char* name, std::wstring& error);
	bool _Get(const char* property, uint32_t& value);
	bool _Set(const char* property, uint32_t value);
	bool _SetIfChanged(const char* property, uint32_t value, std::wstring& error, const uint32_t* knownValue = nullptr);
	bool _VerifyProperty(const char* property, uint32_t expected, uint32_t actual, std::wstring& error);
	void _ResetFailure(const std::string& profile);
	bool _Fail(RtssFailureStage stage, const wchar_t* reason, std::wstring& error);
	bool _CallProfile(ProfileFn function, const char* profile);
	bool _Reload();
	void _Unload();
	HMODULE _module = nullptr;
	std::filesystem::path _directory;
	ProfileFn _load = nullptr, _save = nullptr, _delete = nullptr;
	PropertyFn _get = nullptr, _set = nullptr;
	void (*_update)() = nullptr;
	bool _faulted = false;
	std::wstring _version;
	std::string _selfProfile;
	bool _running = true;
	RtssProfileFailure _failure;
};

}
