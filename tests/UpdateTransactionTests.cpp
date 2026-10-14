// Windows-only fixture tests. Build/run in the separate compilation session.
#include "../src/Updater/UpdateTransaction.h"
#include <fstream>
#include <stdexcept>
#include <iostream>

static void Check(bool value) { if (!value) throw std::runtime_error("Update transaction assertion failed"); }
static void Write(const std::filesystem::path& path, const char* value) {
	std::filesystem::create_directories(path.parent_path());
	std::ofstream file(path, std::ios::binary);
	file << value;
	Check(file.good());
}
static std::string Read(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}

int main() {
	const auto fixture = std::filesystem::temp_directory_path() /
		(L"Magpie UpdateTransaction " + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
	try {
		const auto root = fixture / L"success", source = root / L"update";
		Write(root / L"Magpie.exe", "old-exe");
		Write(root / L"nvngx_dlssg.dll", "old-fg");
		Write(root / L"config.json", "user-settings");
		Write(source / L"Magpie.exe", "new-exe");
		Write(source / L"runtimes/community.dll", "new-provider");
		UpdateTransaction success;
		Check(success.Prepare(source, root, { L"nvngx_dlssg.dll", L"missing/old.dll" }));
		Check(success.Install());
		Check(Read(root / L"Magpie.exe") == "new-exe");
		Check(!std::filesystem::exists(root / L"nvngx_dlssg.dll"));
		Check(Read(root / L"config.json") == "user-settings");
		Check(Read(source / L"Magpie.exe") == "new-exe");
		Check(success.Rollback());
		Check(Read(root / L"Magpie.exe") == "old-exe");
		Check(Read(root / L"nvngx_dlssg.dll") == "old-fg");
		Check(!std::filesystem::exists(root / L"runtimes/community.dll"));

		const auto lockedRoot = fixture / L"locked", lockedSource = lockedRoot / L"update";
		Write(lockedRoot / L"Magpie.exe", "old-exe");
		Write(lockedRoot / L"provider.dll", "old-provider");
		Write(lockedSource / L"Magpie.exe", "new-exe");
		Write(lockedSource / L"provider.dll", "new-provider");
		UpdateTransaction failed;
		Check(failed.Prepare(lockedSource, lockedRoot, {}));
		HANDLE lock = CreateFileW((lockedRoot / L"provider.dll").c_str(), GENERIC_READ, FILE_SHARE_READ,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		Check(lock != INVALID_HANDLE_VALUE);
		const bool installed = failed.Install();
		CloseHandle(lock);
		Check(!installed && failed.Rollback());
		Check(Read(lockedRoot / L"Magpie.exe") == "old-exe");
		Check(Read(lockedRoot / L"provider.dll") == "old-provider");
		Check(Read(lockedSource / L"provider.dll") == "new-provider");

		const auto traversal = fixture / L"traversal";
		Write(traversal / L"update/Magpie.exe", "new-exe");
		UpdateTransaction invalid;
		Check(!invalid.Prepare(traversal / L"update", traversal, { L"../outside.txt" }));
		Check(!std::filesystem::exists(traversal / L"Magpie.exe"));
		std::filesystem::remove_all(fixture);
		std::cout << "Update transaction fixtures passed\n";
	} catch (...) {
		std::cerr << "Fixture retained: " << fixture << '\n';
		return 1;
	}
}
