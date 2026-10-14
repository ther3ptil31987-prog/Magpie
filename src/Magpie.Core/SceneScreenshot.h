#pragma once
#include "ScalingOptions.h"
#include "ScalingSessionLifetime.h"
#include <atomic>

namespace Magpie {

// Immutable request options; only state changes across threads. One request is
// admitted per Renderer until completion, including encoding and disk writing.
enum class SceneScreenshotState { Pending, Reading, Finished };
struct SceneScreenshotRequest {
	uint64_t id = 0;
	uint32_t runId = 0;
	uint64_t captureSequence = 0;
	uint64_t resourceGeneration = 0;
	ScreenshotContent content = ScreenshotContent::OutputOnly;
	std::atomic<SceneScreenshotState> state = SceneScreenshotState::Pending;
	std::shared_ptr<ScalingSessionLifetime> session;
	winrt::DispatcherQueue cleanupDispatcher{ nullptr };
	std::filesystem::path directory;
	decltype(ScalingOptions::reportErrorDetails) report = nullptr;
	decltype(ScalingOptions::showToast) toast = nullptr;
	HWND target = nullptr;
	std::wstring savedMessage, pairSavedMessage, partialMessage;
};

// A new immutable native-size input candidate exists only while requested.
// The slot lock protects transfer of this pointer with the processed image.
struct SceneScreenshotFrame {
	std::shared_ptr<SceneScreenshotRequest> request;
	winrt::com_ptr<ID3D11Texture2D> input;
	wil::unique_handle inputHandle;
	HdrFrameMetadata metadata;
	ScreenshotFrameIdentity Identity() const noexcept {
		return { metadata.frameId, metadata.captureSequence, metadata.resourceGeneration };
	}
};

}
