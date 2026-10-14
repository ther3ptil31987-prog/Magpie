#pragma once
#include <windows.h>
#include <filesystem>
#include <vector>
#include <string>

// Keeps the downloaded package intact until verification has succeeded. The
// snapshot covers overwritten AND obsolete files; no settings/cache enumeration.
class UpdateTransaction {
public:
	bool Prepare(const std::filesystem::path& source, const std::filesystem::path& root,
		const std::vector<std::filesystem::path>& obsolete) {
		_root = std::filesystem::absolute(root).lexically_normal();
		_source = std::filesystem::absolute(source).lexically_normal();
		_backup = _root / (L"update-rollback-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
		if (!CreateDirectoryW(_backup.c_str(), nullptr)) return false;
		for (const auto& item : std::filesystem::recursive_directory_iterator(_source)) {
			const auto attributes = GetFileAttributesW(item.path().c_str());
			if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
			if (!item.is_regular_file()) continue;
			const auto relative = item.path().lexically_relative(_source);
			if (!Add(relative, true)) return false;
		}
		for (const auto& relative : obsolete) {
			bool incoming = false;
			for (const auto& entry : _entries) {
				if (_wcsicmp(entry.relative.c_str(), relative.c_str()) == 0) { incoming = true; break; }
			}
			if (!incoming && !Add(relative, false)) return false;
		}
		return !_entries.empty();
	}

	bool Install() {
		for (auto& entry : _entries) {
			const auto destination = _root / entry.relative;
			_currentFile = destination;
			// Mark first: a failed copy may already have truncated a destination.
			entry.touched = true;
			if (entry.incoming) {
				std::filesystem::create_directories(destination.parent_path());
				if (!CopyFileW((_source / entry.relative).c_str(), destination.c_str(), FALSE)) {
					_failureCode = GetLastError();
					return false;
				}
			} else if (entry.existed && !DeleteFileW(destination.c_str())) {
				_failureCode = GetLastError();
				return false;
			}
		}
		return true;
	}

	bool Rollback() noexcept {
		bool ok = true;
		try {
			for (auto i = _entries.rbegin(); i != _entries.rend(); ++i) {
				if (!i->touched) continue;
				const auto destination = _root / i->relative;
				if (i->existed) {
					ok = (CopyFileW((_backup / i->relative).c_str(), destination.c_str(), FALSE) != FALSE) && ok;
				} else if (!DeleteFileW(destination.c_str())) {
					const DWORD error = GetLastError();
					if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) ok = false;
				}
			}
		} catch (...) { return false; }
		return ok;
	}

	const std::filesystem::path& Backup() const noexcept { return _backup; }
	const std::filesystem::path& CurrentFile() const noexcept { return _currentFile; }
	DWORD FailureCode() const noexcept { return _failureCode; }

private:
	struct Entry {
		std::filesystem::path relative;
		bool incoming = false, existed = false, touched = false;
	};
	bool Add(const std::filesystem::path& relative, bool incoming) {
		if (relative.empty() || relative.is_absolute() || relative.has_root_name()) return false;
		for (const auto& part : relative) if (part == L".." || part == L".") return false;
		// Do not follow destination junctions out of the install directory.
		auto checked = _root;
		for (const auto& part : relative) {
			checked /= part;
			const auto attributes = GetFileAttributesW(checked.c_str());
			if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
		}
		const auto destination = _root / relative;
		_currentFile = destination;
		const bool existed = std::filesystem::exists(destination);
		if (existed) {
			if (!std::filesystem::is_regular_file(destination)) return false;
			const auto backupFile = _backup / relative;
			std::filesystem::create_directories(backupFile.parent_path());
			if (!CopyFileW(destination.c_str(), backupFile.c_str(), TRUE)) {
				_failureCode = GetLastError();
				return false;
			}
		}
		_entries.push_back({ relative, incoming, existed, false });
		return true;
	}
	std::filesystem::path _source, _root, _backup, _currentFile;
	DWORD _failureCode = 0;
	std::vector<Entry> _entries;
};
