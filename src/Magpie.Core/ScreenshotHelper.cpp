#include "pch.h"
#include "ScreenshotHelper.h"
#include "ScreenshotNaming.h"
#include "Logger.h"
#include "StrHelper.h"

namespace Magpie {

std::mutex& ScreenshotHelper::SaveMutex() noexcept {
	static std::mutex mutex;
	return mutex;
}

uint32_t ScreenshotHelper::FindUnusedScreenshotNum(const std::filesystem::path& directory) noexcept {
	WIN32_FIND_DATA data{};
	const std::wstring pattern = StrHelper::Concat(directory.native(), L"\\Magpie_*");
	wil::unique_hfind find(FindFirstFileEx(pattern.c_str(), FindExInfoBasic,
		&data, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH));
	if (!find) {
		if (GetLastError() == ERROR_FILE_NOT_FOUND) return 1;
		Logger::Get().Win32Error("Enumerate screenshot directory failed");
		return 0;
	}
	std::vector<uint32_t> numbers;
	do {
		// A directory with a screenshot-shaped name still occupies that path.
		if (const uint32_t number = ParseScreenshotNumber(data.cFileName)) numbers.push_back(number);
	} while (FindNextFile(find.get(), &data));
	if (GetLastError() != ERROR_NO_MORE_FILES) {
		Logger::Get().Win32Error("FindNextFile failed while finding screenshot number");
		return 0;
	}
	return NextScreenshotNumber(std::move(numbers));
}

}
