#pragma once
#include "NativeEffectBackend.h"

namespace Magpie {
enum class NvidiaVfxImageKind { Upscale, Vsr, TrueHdr };
class NvidiaVfxImageBackend final : public NativeEffectBackend {
public:
	NvidiaVfxImageBackend();
	~NvidiaVfxImageBackend() override;
	bool Initialize(DeviceResources& resources, ID3D11Texture2D* input,
		ID3D11Texture2D* output, const EffectOption& option, NvidiaVfxImageKind kind) noexcept;
	bool Resize(DeviceResources& resources, ID3D11Texture2D* input, ID3D11Texture2D* output) noexcept override;
	bool Draw(const NativeEffectDrawContext& context) noexcept override;
	bool Drain() noexcept override;
	NvidiaVfxRuntimeMeasurements TakeNvidiaVfxMeasurements() noexcept override;
	bool SupportsNvidiaVfxCudaInput(ID3D11Texture2D* source) const noexcept override;
	bool DrawNvidiaVfxCudaInput(const NativeEffectDrawContext& context,
		const NvidiaVfxCudaFrame& frame) noexcept override;
	EffectParameterApplyMode GetParameterApplyMode(std::string_view name) const noexcept override;
	bool ApplyParameters(const EffectOption& option, std::span<const std::string> names) noexcept override;
private:
	bool _ExportTrueHdr(const NativeEffectDrawContext& context) noexcept;
	struct Impl;
	[[maybe_unused]] std::unique_ptr<Impl> _impl;
	EffectOption _option;
	NvidiaVfxImageKind _kind = NvidiaVfxImageKind::Vsr;
};
}
