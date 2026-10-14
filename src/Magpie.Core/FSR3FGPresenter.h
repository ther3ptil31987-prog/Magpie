#pragma once
#include "PresenterBase.h"
#include "FSR3FGPolicy.h"
#include <memory>

namespace Magpie {

// Experimental image-only FSR 3.1.6: fixed 2x, SDR, synchronous FG compute.
// All public calls belong to the frontend thread. The SDK generation callback
// runs synchronously inside Present; its own presentation workers own no UI.
class FSR3FGPresenter final : public PresenterBase {
public:
	struct Impl;
	struct ImplDeleter { void operator()(Impl* impl) const noexcept; };
	FSR3FGPresenter();
	~FSR3FGPresenter() noexcept override;
	bool BeginFrame(winrt::com_ptr<ID3D11Texture2D>& frameTex,
		winrt::com_ptr<ID3D11RenderTargetView>& frameRtv, POINT& drawOffset) noexcept override;
	bool BeginIdleFrame(winrt::com_ptr<ID3D11Texture2D>& frameTex,
		winrt::com_ptr<ID3D11RenderTargetView>& frameRtv, POINT& drawOffset) noexcept override;
	bool EndFrame(bool waitForGpu = false) noexcept override;
	bool WaitForFrameCapacity(DWORD timeout) noexcept override;
	bool WasFrameCapacityBusy() const noexcept override;
	bool HasFatalError() const noexcept override;
	const PresenterFailure* GetFailure() const noexcept override;
	void SetSourceTiming(uint64_t frameId, uint64_t sequence, uint64_t generation,
		int64_t timestamp100ns) noexcept override;
	void SetCaptureAdmissionTime(int64_t timestamp100ns) noexcept override;
	void SetOutputHistoryRevision(uint64_t revision) noexcept override;
	void SetFrameGuidanceAvailability(FrameGuidanceAvailability availability) noexcept override;
	ContentFrameCount LastContentFrameCount() const noexcept override {
		return { _lastSubmittedFrameCount, ContentFrameCountSource::Submitted };
	}
	void SetFrameGuidance(ID3D11Texture2D* motion, FrameGuidanceFrameId frameId,
		bool requiresHistoryReset, const RECT& destinationRect) noexcept override;
	bool HasIndependentOverlay() const noexcept override;
	bool BeginOverlayFrame(winrt::com_ptr<ID3D11Texture2D>& frameTex,
		winrt::com_ptr<ID3D11RenderTargetView>& frameRtv, POINT& drawOffset) noexcept override;
	bool EndOverlayFrame() noexcept override;
	bool UsesFrameLatencyWaitableObject() const noexcept override { return true; }
	bool OnResize() noexcept override;
protected:
	bool _Initialize(HWND hwndAttach) noexcept override;
	UINT _PresentSyncInterval(UINT flags) const noexcept override { return FSR3FGSyncInterval(flags != 0); }
private:
	bool _BeginFrame(winrt::com_ptr<ID3D11Texture2D>& frameTex,
		winrt::com_ptr<ID3D11RenderTargetView>& frameRtv, POINT& drawOffset, bool idle) noexcept;
	bool _ResizeOverlaySurface() noexcept;
	std::optional<uint32_t> _lastSubmittedFrameCount;
	[[maybe_unused]] std::unique_ptr<Impl, ImplDeleter> _impl;
};
}
