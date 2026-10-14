#pragma once
#include <mutex>

namespace Magpie {

struct ScreenshotHelper {
	// 失败则返回 0
	static uint32_t FindUnusedScreenshotNum(const std::filesystem::path& screenshotsDir) noexcept;
	// Shared by ordinary screenshots and right-click texture exports.
	static std::mutex& SaveMutex() noexcept;
};

}
