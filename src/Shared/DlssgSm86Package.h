#pragma once
#include "PinnedFile.h"

namespace Magpie::DlssgSm86Package {

inline constexpr wchar_t Directory[] = L"runtimes/dlssg_sm86";
inline constexpr char Revision[] = "9621db573e07ed54f50c15bbb585ed9a7bdfac28";
inline constexpr char Version[] = "0.3.5";
inline constexpr char RuntimeVersion[] = "310.9.1.0";

struct File {
	const wchar_t* name;
	uint64_t size;
	const char* sha256;
};

// Source of truth for restore, packaging, updater preflight and runtime load.
inline constexpr File Files[]{
	{ L"version.dll", 30021920, "c3934a09399f022504227c72df0bf8c0de55f9a08880dddde898c5262cefa838" },
	{ L"native.ini", 463, "38b42e4325b43da3f451539a24c711f7b1c8bfac590c8548ec92d543eab02923" },
	{ L"forward.ini", 452, "acd899417ffbf3a9d73725e3526932465d21835d92b52192e2e9a4e22ce23a6d" },
	{ L"THIRD_PARTY_NOTICES.txt", 3349, "ac3b44ab30a4235edd18feca1ab4f802d57c8d3d0ee4878dc77b81a6b127155f" }
};

// Read-only PE resource inspection of the pinned proxy, resources 30001/30002.
inline constexpr File Runtime{ L"nvngx_dlssg.dll", 7460976,
	"ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82" };
inline constexpr File Backend{ L"sm86_backend.dll", 22065664,
	"1aebf8f306fbaede18c98b372da9c70a1bbdb1bd35327f7f85c2969085a619a1" };

inline bool Verify(const std::filesystem::path& root) {
	for (const auto& file : Files) {
		if (!PinnedFile::Verify(root / Directory / file.name, file.size, file.sha256)) return false;
	}
	return true;
}

}
