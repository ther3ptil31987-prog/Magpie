#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string_view>
#pragma comment(lib, "bcrypt.lib")

namespace Magpie::PinnedFile {

// No executable mapping: verify bytes before allowing a third-party load.
// Callers can retain this handle to deny changes while the file is in use.
inline HANDLE Open(const std::filesystem::path& path) noexcept {
	return CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

inline bool Verify(HANDLE file, uint64_t expectedSize, std::string_view expectedHash) noexcept {
	if (file == INVALID_HANDLE_VALUE || expectedHash.size() != 64) return false;
	LARGE_INTEGER size{};
	if (!GetFileSizeEx(file, &size) || static_cast<uint64_t>(size.QuadPart) != expectedSize ||
		!SetFilePointerEx(file, {}, nullptr, FILE_BEGIN)) return false;
	BCRYPT_ALG_HANDLE algorithm = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
	if (ok) ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
	std::array<UCHAR, 65536> buffer{};
	DWORD bytes = 0;
	while (ok) {
		if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes, nullptr)) {
			ok = false;
			break;
		}
		if (!bytes) break;
		ok = BCryptHashData(hash, buffer.data(), bytes, 0) >= 0;
	}
	std::array<UCHAR, 32> digest{};
	if (ok) ok = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
	if (hash) BCryptDestroyHash(hash);
	if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
	constexpr char hex[] = "0123456789abcdef";
	for (size_t i = 0; ok && i < digest.size(); ++i) {
		ok = hex[digest[i] >> 4] == expectedHash[i * 2] &&
			hex[digest[i] & 15] == expectedHash[i * 2 + 1];
	}
	return ok;
}

inline bool Verify(const std::filesystem::path& path, uint64_t size, std::string_view hash) noexcept {
	HANDLE file = Open(path);
	const bool ok = Verify(file, size, hash);
	if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
	return ok;
}

}
