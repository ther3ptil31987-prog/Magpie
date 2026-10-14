#pragma once

#if defined(MP_ENABLE_DLSS_SR) || defined(MP_ENABLE_DLSSNR) || defined(MP_ENABLE_DLSS_FRAME_GENERATION)
#include <nvsdk_ngx.h>
#include <atomic>
#include <array>
#include <cstring>
#include "DlssgSm86Runtime.h"
#include "Logger.h"
#include "StrHelper.h"
#include "Win32Helper.h"
#endif

namespace Magpie {

// Own paths, pointer list and common-info for the whole NGX session. Both D3D11
// SR and the shared D3D12 Core use the same ordering before their first Init.
class NgxFeatureDiscovery {
public:
	NgxFeatureDiscovery() = default;
	NgxFeatureDiscovery(const NgxFeatureDiscovery&) = delete;
	NgxFeatureDiscovery& operator=(const NgxFeatureDiscovery&) = delete;
#if defined(MP_ENABLE_DLSS_SR) || defined(MP_ENABLE_DLSSNR) || defined(MP_ENABLE_DLSS_FRAME_GENERATION)
	bool Initialize(bool communityPrepared) noexcept {
		try {
			_applicationDirectory = Win32Helper::GetExePath().parent_path().wstring();
			std::filesystem::path bundle;
			_communityRegistered = communityPrepared && DlssgSm86Runtime::GetFeatureDirectory(bundle);
			if (communityPrepared && !_communityRegistered) {
				Logger::Get().Error("NGX discovery: prepared FG bundle directory is unavailable");
				return false;
			}
			unsigned int count = 0;
			if (_communityRegistered) _directories[count++] = bundle.wstring();
			_directories[count++] = _applicationDirectory;
			for (unsigned int i = 0; i < count; ++i) {
				_paths[i] = _directories[i].c_str();
				Logger::Get().Info(fmt::format("NGX feature search path[{}]={}", i,
					StrHelper::UTF16ToUTF8(_directories[i])));
			}
			_info.PathListInfo = { _paths.data(), count };
			_info.LoggingInfo = { Log, NVSDK_NGX_LOGGING_LEVEL_ON, false };
			return true;
		} catch (...) {
			Logger::Get().Error("NGX feature search path preparation failed");
			return false;
		}
	}

	const NVSDK_NGX_FeatureCommonInfo* Info() const noexcept { return &_info; }
	const wchar_t* ApplicationDirectory() const noexcept { return _applicationDirectory.c_str(); }
	bool CommunityRegistered() const noexcept { return _communityRegistered; }

private:
	static void NVSDK_CONV Log(const char* message, NVSDK_NGX_Logging_Level level,
		NVSDK_NGX_Feature component) noexcept {
		// SDK callbacks can arrive on arbitrary threads and during shutdown.
		// Do not access a session or reenter NGX. Bound output per process; retain
		// SDK file sinks for any messages omitted here. No string crosses the call.
		static std::atomic_uint32_t count{ 0 };
		const uint32_t index = count.fetch_add(1, std::memory_order_relaxed);
		if (!message || index > 512) return;
		try {
			if (index == 512) {
				Logger::Get().Info("NGX callback log limit reached; further SDK messages remain in NGX sinks");
				return;
			}
			Logger::Get().Info(fmt::format("NGX SDK component={} level={}: {}",
				static_cast<uint32_t>(component), static_cast<uint32_t>(level),
				std::string_view(message, strnlen_s(message, 2048))));
		} catch (...) { /* Never propagate an exception across the SDK ABI. */ }
	}

	std::wstring _applicationDirectory;
	std::array<std::wstring, 2> _directories;
	std::array<const wchar_t*, 2> _paths{};
	NVSDK_NGX_FeatureCommonInfo _info{};
	bool _communityRegistered = false;
#endif
};

}
