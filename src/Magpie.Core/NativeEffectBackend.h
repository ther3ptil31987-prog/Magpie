#pragma once
#include "FrameGuidanceTypes.h"
#include "ScalingOptions.h"
#include "HdrEffectBoundary.h"
#include "NvidiaVfxMeasurements.h"
#include <utility>

namespace Magpie {

class DeviceResources;
struct NvidiaVfxCudaFrame;

struct NativeEffectDrawContext {
	ID3D11Texture2D* input = nullptr;
	ID3D11Texture2D* output = nullptr;
	HdrFrameMetadata inputMetadata{};
	HdrFrameMetadata outputMetadata{};
	FrameGuidanceFrameId frameId = 0;
	// IDs may skip rejected Reflex GPU attempts. Motion still points to the
	// previous accepted capture, rather than necessarily frameId - 1.
	FrameGuidanceFrameId previousCaptureFrameId = 0;
	// Content version of the actual upstream output, including ordinary new
	// captures and same-frame edits. Include in cache keys, but use the separate
	// history epoch/reset below when deciding whether to discard temporal state.
	uint64_t inputRevision = 0;
	// Separate from content versions so new captures can accumulate history.
	uint64_t inputHistoryRevision = 0;
	bool inputHistoryReset = false;
	bool isNewCaptureFrame = true;
	const FrameGuidanceView& frameGuidance;
	const FrameGuidanceView& zeroFrameGuidance;
};

// Common lifetime and rendering contract for native SDK-backed effects.
// Creation parameters remain in NativeEffectBackendFactory so Renderer does
// not need one parallel container and one name-dispatch branch per SDK.
class NativeEffectBackend {
public:
	virtual ~NativeEffectBackend() = default;

	virtual void SetHdrBoundary(HdrEffectBoundaryContext context) noexcept { _hdrBoundary = std::move(context); }
	const HdrEffectBoundaryContext& GetHdrBoundary() const noexcept { return _hdrBoundary; }

	virtual FrameGuidanceRequirements GetFrameGuidanceRequirements() const noexcept {
		return {};
	}
	virtual bool Drain() noexcept { return true; }
	virtual DlssNrFailure GetDlssNrFailure() const { return {}; }
	virtual bool IsDlssNrProcessing() const noexcept { return false; }
	virtual NvidiaVfxRuntimeMeasurements TakeNvidiaVfxMeasurements() noexcept { return {}; }
	virtual bool SupportsNvidiaVfxCudaInput(ID3D11Texture2D*) const noexcept { return false; }
	virtual bool DrawNvidiaVfxCudaInput(const NativeEffectDrawContext&,
		const NvidiaVfxCudaFrame&) noexcept { return false; }

	virtual EffectParameterApplyMode GetParameterApplyMode(
		std::string_view /*parameterName*/
	) const noexcept {
		return EffectParameterApplyMode::RestartRequired;
	}

	virtual EffectParameterRestartReason GetParameterRestartReason(
		std::string_view /*parameterName*/
	) const noexcept {
		return EffectParameterRestartReason::NativeBackend;
	}

	// Called on the backend thread with a complete candidate option. Implementations
	// must not recreate resources or partially commit settings when validation fails.
	virtual bool ApplyLiveParameters(
		const EffectOption& /*option*/,
		std::span<const std::string> /*parameterNames*/
	) noexcept {
		return false;
	}

	// Backend-thread transaction, serialized with Draw/Resize/destruction.
	// A model-backed implementation may load a candidate private model here;
	// renderer-owned textures and the active model survive a failed load.
	virtual bool ApplyParameters(const EffectOption& option,
		std::span<const std::string> names) noexcept {
		return ApplyLiveParameters(option, names);
	}

	virtual bool Resize(
		DeviceResources& resources,
		ID3D11Texture2D* input,
		ID3D11Texture2D* output
	) noexcept = 0;

	virtual bool Draw(const NativeEffectDrawContext& context) noexcept = 0;

protected:
	HdrEffectBoundaryContext _hdrBoundary{};
};

}
