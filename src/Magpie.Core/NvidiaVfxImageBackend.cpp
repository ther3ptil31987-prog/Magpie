#include "pch.h"
#include "NvidiaVfxImageBackend.h"
#include "NvidiaVfxRuntime.h"
#include "NvidiaVfxFrameGenerator.h"
#include "NvidiaVfxParameters.h"
#include "NvidiaVfxTrueHdrParameters.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include "BackendDescriptorStore.h"
#include "HdrSurfaceAdapter.h"
#include "Logger.h"
#include "EffectCompiler.h"
#include "EffectDrawer.h"
#include "EffectsProfiler.h"
#ifdef MP_ENABLE_NVIDIA_VFX
#include <nvVFXUpscale.h>
#include <nvVFXTrueHDR.h>
#include <nvVFXVideoSuperRes.h>
#endif

namespace Magpie {
#ifdef MP_ENABLE_NVIDIA_VFX

struct NvidiaVfxImageBackend::Impl {
	DeviceResources* resources = nullptr;
	NvidiaVfxSession session;
	BackendDescriptorStore descriptors;
	HdrSurfaceAdapter adapter;
	winrt::com_ptr<ID3D11Texture2D> packedHdr;
	winrt::com_ptr<ID3D11Texture2D> nativeUpscale;
	EffectDrawer adaptation;
	EffectsProfiler adaptationProfiler;
	NvidiaVfxImageKind kind = NvidiaVfxImageKind::Vsr;
	EffectOption settings;
	bool usable = true;
	uint64_t parameterRevision = 0;
	static constexpr const char* hdrSelectors[] = {
		NVVFXTRUEHDR_CONTRAST, NVVFXTRUEHDR_SATURATION, NVVFXTRUEHDR_MIDDLE_GRAY,
		NVVFXTRUEHDR_LUMINANCE, NVVFXTRUEHDR_DEBANDING_OFF
	};
	NvVFX_Handle CreateCandidate(const EffectOption& option) noexcept {
		const auto value = [&](const char* key, float fallback) { const auto it = option.parameters.find(key); return it == option.parameters.end() ? fallback : it->second; };
		NvVFX_Handle effect = nullptr;
		const char* selector = kind == NvidiaVfxImageKind::Vsr ? "VideoSuperRes" :
			kind == NvidiaVfxImageKind::Upscale ? NVVFX_FX_SR_UPSCALE : NVVFX_FX_TRUE_HDR;
		if (!NvidiaVfxSucceeded(session.Call("CreateEffect", [&] { return NvVFX_CreateEffect(selector, &effect); }, true), "CreateEffect")) return nullptr;
		auto cleanup = wil::scope_exit([&] { if (effect) session.Call("DestroyCandidate", [&] { NvVFX_DestroyEffect(effect); }, true); });
		// Select RGB8 before binding I/O, including transactional tier changes.
		if (kind == NvidiaVfxImageKind::Vsr && !NvidiaVfxSucceeded(
			NvVFX_SetU32(effect, NVVFX_IMAGE_ENCODING_MODE, NVVFX_IMAGE_ENCODING_RGB8), "SetVsrImageEncoding")) return nullptr;
		if (!NvidiaVfxSucceeded(NvVFX_SetCudaStream(effect, NVVFX_CUDA_STREAM, session.stream), "SetCudaStream") ||
			!NvidiaVfxSucceeded(NvVFX_SetImage(effect, NVVFX_INPUT_IMAGE, &session.input), "SetImage(input)") ||
			!NvidiaVfxSucceeded(NvVFX_SetImage(effect, NVVFX_OUTPUT_IMAGE, &session.output), "SetImage(output)")) return nullptr;
		if (kind == NvidiaVfxImageKind::Vsr) {
			const float categoryValue = value("category", 0.f);
			const int category = NvidiaVfxCategory(categoryValue);
			const float tier = value("strength", 1.f);
			if (categoryValue != float(category) || !NvidiaVfxTierValid(category, tier) ||
				(category == 2 && session.Context().ComputeMajor() < 8)) return nullptr;
			if (!NvidiaVfxSucceeded(NvVFX_SetU32(effect, "QualityLevel", NvidiaVfxQualityLevel(category, int(tier))), "SetQualityLevel")) return nullptr;
			// The deployed strength key remains a tier. Vendor continuous Strength
			// is independent and intentionally has no third first-level control.
			float enhancement = 0.f;
			if (NvVFX_GetF32(effect, NVVFX_STRENGTH, &enhancement) == NVCV_SUCCESS) {
				if (!std::isfinite(enhancement) || enhancement < 0 || enhancement > 1 ||
					!NvidiaVfxSucceeded(NvVFX_SetF32(effect, NVVFX_STRENGTH, enhancement), "SetVsrStrength")) return nullptr;
				Logger::Get().Info(fmt::format("Nvidia VFX VSR vendor default Strength={}", enhancement));
			}
		} else if (kind == NvidiaVfxImageKind::Upscale) {
			const float strength = value("sharpness", .4f);
			if (!std::isfinite(strength) || strength < 0 || strength > 1 ||
				!NvidiaVfxSucceeded(NvVFX_SetF32(effect, NVVFX_STRENGTH, strength), "SetStrength")) return nullptr;
		} else {
			if (BuildHdrComponentPlan(std::span(&option, 1)).error != HdrComponentError::None) return nullptr;
			const auto tunables = ReadNvidiaVfxTrueHdrTunables(option);
			if (!tunables) return nullptr;
			for (size_t i = 0; i < tunables->values.size(); ++i)
				if (!NvidiaVfxSucceeded(session.Call(hdrSelectors[i], [&] {
					return NvVFX_SetU32(effect, hdrSelectors[i], tunables->values[i]);
				}), "SetTrueHdrTunable")) return nullptr;
		}
		if (!NvidiaVfxSucceeded(session.Call("Load", [&] { return NvVFX_Load(effect); }, true), "Load candidate")) return nullptr;
		return std::exchange(effect, nullptr);
	}
};
#else
struct NvidiaVfxImageBackend::Impl {};
#endif

NvidiaVfxImageBackend::NvidiaVfxImageBackend() = default;
NvidiaVfxImageBackend::~NvidiaVfxImageBackend() = default;
bool NvidiaVfxImageBackend::Initialize(DeviceResources& resources, ID3D11Texture2D* input,
	ID3D11Texture2D* output, const EffectOption& option, NvidiaVfxImageKind kind) noexcept {
#ifdef MP_ENABLE_NVIDIA_VFX
	if (!input || !output) return false;
	D3D11_TEXTURE2D_DESC in{}, out{};
	input->GetDesc(&in); output->GetDesc(&out);
	const auto sdr = [](DXGI_FORMAT format) { return format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM; };
	if (!sdr(in.Format) || in.SampleDesc.Count != 1 || out.SampleDesc.Count != 1 ||
		(kind == NvidiaVfxImageKind::TrueHdr ? out.Format != DXGI_FORMAT_R16G16B16A16_FLOAT : !sdr(out.Format))) return false;
	const auto read = [&](const char* key, float fallback) { const auto it = option.parameters.find(key); return it == option.parameters.end() ? fallback : it->second; };
	if ((kind == NvidiaVfxImageKind::TrueHdr || (kind == NvidiaVfxImageKind::Vsr && NvidiaVfxSameSize(NvidiaVfxCategory(read("category", 0))))) &&
		(in.Width != out.Width || in.Height != out.Height)) return false;
	NvidiaVfxUpscalePlan upscale;
	if (kind == NvidiaVfxImageKind::Upscale) {
		const float mode = read("upscaleMode", 0);
		if (!std::isfinite(mode) || mode < 0 || mode > 5 || std::floor(mode) != mode) return false;
		upscale = PlanNvidiaVfxUpscale({in.Width, in.Height}, {out.Width, out.Height}, int(mode));
		if (!upscale.valid || (mode != 0 && (upscale.native.width != out.Width || upscale.native.height != out.Height))) return false;
	}
	const wchar_t* library = kind == NvidiaVfxImageKind::Vsr ? L"nvVFXVideoSuperRes.dll" :
		kind == NvidiaVfxImageKind::Upscale ? L"nvVFXUpscale.dll" : L"nvVFXTrueHDR.dll";
	// NvCVImage's C++ default constructor already calls the proxy Alloc API.
	// Pin the matched local modules before constructing any image descriptors.
	if (!LoadNvidiaVfxRuntime(library, true)) return false;
	auto candidate = std::make_unique<Impl>();
	candidate->resources = &resources;
	candidate->kind = kind;
	if (!candidate->session.Initialize(resources, library)) return false;
	NvidiaVfxContextScope scope(candidate->session.Context());
	// Every RGB8 model requires RGBA. Import/Export converts BGRA texture
	// boundaries explicitly; TrueHDR alone produces packed RGB10A2.
	constexpr auto inputEncoding = NVCV_RGBA;
	constexpr auto outputEncoding = NVCV_RGBA;
	if (!scope || !NvidiaVfxSucceeded(NvCVImage_Alloc(&candidate->session.input, in.Width, in.Height,
		inputEncoding, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 32), "Alloc(input)") ||
		!NvidiaVfxSucceeded(NvCVImage_Alloc(&candidate->session.output,
			kind == NvidiaVfxImageKind::Upscale ? upscale.native.width : out.Width,
			kind == NvidiaVfxImageKind::Upscale ? upscale.native.height : out.Height,
			kind == NvidiaVfxImageKind::TrueHdr ? NVCV_RGB10A2 : outputEncoding,
			kind == NvidiaVfxImageKind::TrueHdr ? NVCV_P32 : NVCV_U8,
			NVCV_INTERLEAVED, NVCV_GPU, 32), "Alloc(output)")) return false;
	if (kind == NvidiaVfxImageKind::TrueHdr) {
		// CUDA D3D11 registration excludes R10G10B10A2. Carry its raw bytes in
		// RGBA8, then unpack the full 10-bit PQ/Rec.2020 value in the adapter.
		candidate->packedHdr = DirectXHelper::CreateTexture2D(resources.GetD3DDevice(), DXGI_FORMAT_R8G8B8A8_UNORM,
			out.Width, out.Height, D3D11_BIND_SHADER_RESOURCE);
		candidate->descriptors.Initialize(resources.GetD3DDevice());
		if (!candidate->packedHdr || !candidate->adapter.Initialize(resources, candidate->descriptors)) return false;
	}
	if (kind == NvidiaVfxImageKind::Upscale && upscale.adaptation != NvidiaVfxAdaptation::None) {
		candidate->nativeUpscale = DirectXHelper::CreateTexture2D(resources.GetD3DDevice(), DXGI_FORMAT_R8G8B8A8_UNORM,
			upscale.native.width, upscale.native.height, D3D11_BIND_SHADER_RESOURCE);
		candidate->descriptors.Initialize(resources.GetD3DDevice());
		EffectOption resize{ .name = "Bicubic", .parameters{{"paramB", 0.f}, {"paramC", .5f}},
			.scalingType = ScalingType::Absolute, .scale = {float(out.Width), float(out.Height)} };
		EffectDesc desc{ .name = "Bicubic" };
		if (!candidate->nativeUpscale || EffectCompiler::Compile(desc, EffectCompilerFlags::InlineParams, &resize.parameters)) return false;
		ID3D11Texture2D* handoff = candidate->nativeUpscale.get();
		if (!candidate->adaptation.Initialize(desc, resize, resources, candidate->descriptors, &handoff)) return false;
		D3D11_TEXTURE2D_DESC adapted{};
		candidate->adaptation.GetOutputTexture()->GetDesc(&adapted);
		if (adapted.Format != out.Format || adapted.Width != out.Width || adapted.Height != out.Height) {
			Logger::Get().Error("Nvidia VFX Upscale adaptation output is incompatible with its texture boundary");
			return false;
		}
	}
	candidate->session.effect = candidate->CreateCandidate(option);
	if (!candidate->session.effect) return false;
	candidate->settings = option;
	_impl = std::move(candidate); _option = option; _kind = kind;
	return true;
#else
	(void)resources; (void)input; (void)output; (void)option; (void)kind;
	Logger::Get().Error("Nvidia VFX suite requires a build with matched VFX 1.3 development headers and runtime");
	return false;
#endif
}
bool NvidiaVfxImageBackend::Resize(DeviceResources& resources, ID3D11Texture2D* input, ID3D11Texture2D* output) noexcept {
	return Initialize(resources, input, output, _option, _kind);
}
bool NvidiaVfxImageBackend::Drain() noexcept {
#ifdef MP_ENABLE_NVIDIA_VFX
	if (!_impl) return true;
	NvidiaVfxContextScope scope(_impl->session.Context());
	return scope && _impl->session.Synchronize();
#else
	return true;
#endif
}
bool NvidiaVfxImageBackend::Draw(const NativeEffectDrawContext& context) noexcept {
#ifdef MP_ENABLE_NVIDIA_VFX
	if (!_impl || !_impl->usable) return false;
	NvidiaVfxContextScope scope(_impl->session.Context());
	if (!scope || !_impl->session.Import(context.input, _impl->session.input) ||
		!_impl->session.Run("Run image effect")) return false;
	if (_kind != NvidiaVfxImageKind::TrueHdr) {
		if (!_impl->nativeUpscale) return _impl->session.Export(_impl->session.output, context.output);
		if (!_impl->session.Export(_impl->session.output, _impl->nativeUpscale.get()) ||
			!_impl->adaptation.Draw(_impl->adaptationProfiler)) return false;
		_impl->resources->GetD3DDC()->ClearState();
		_impl->resources->GetD3DDC()->CopyResource(context.output, _impl->adaptation.GetOutputTexture());
		return true;
	}
	return _ExportTrueHdr(context);
#else
	(void)context; return false;
#endif
}
bool NvidiaVfxImageBackend::_ExportTrueHdr(const NativeEffectDrawContext& context) noexcept {
#ifdef MP_ENABLE_NVIDIA_VFX
	if (!_impl->session.ExportPackedHdr10(_impl->session.output, _impl->packedHdr.get())) return false;
	HdrTransformParameters parameters;
	const auto peak = _option.parameters.find("peakNits");
	parameters.hdrPeakNits = peak == _option.parameters.end() ? 1000.f : peak->second;
	// Decode PQ and BT.2020 explicitly into the 80-nit canonical scRGB domain.
	return _impl->adapter.ConvertPackedHdr10ToCanonical(_impl->packedHdr.get(), context.output, parameters);
#else
	(void)context; return false;
#endif
}
NvidiaVfxRuntimeMeasurements NvidiaVfxImageBackend::TakeNvidiaVfxMeasurements() noexcept {
#ifdef MP_ENABLE_NVIDIA_VFX
	return _impl ? _impl->session.TakeMeasurements() : NvidiaVfxRuntimeMeasurements{};
#else
	return {};
#endif
}
bool NvidiaVfxImageBackend::SupportsNvidiaVfxCudaInput(ID3D11Texture2D* source) const noexcept {
#if defined(MP_ENABLE_NVIDIA_VFX) && defined(MP_ENABLE_NVIDIA_VFX_CUDA_HANDOFF)
	if (!_impl || _kind != NvidiaVfxImageKind::TrueHdr || !source || _hdrBoundary.plan.requiresBoundedMapping) return false;
	D3D11_TEXTURE2D_DESC desc{}; source->GetDesc(&desc);
	const auto format = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ? NVCV_BGRA : NVCV_RGBA;
	return (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM) &&
		desc.SampleDesc.Count == 1 && desc.Width == _impl->session.input.width &&
		desc.Height == _impl->session.input.height && format == _impl->session.input.pixelFormat;
#else
	(void)source;
	return false;
#endif
}
bool NvidiaVfxImageBackend::DrawNvidiaVfxCudaInput(const NativeEffectDrawContext& context,
	const NvidiaVfxCudaFrame& frame) noexcept {
#if defined(MP_ENABLE_NVIDIA_VFX) && defined(MP_ENABLE_NVIDIA_VFX_CUDA_HANDOFF)
	if (!SupportsNvidiaVfxCudaInput(context.input) || !frame.image || !frame.producer) return false;
	auto& session = _impl->session;
	const auto& image = *frame.image;
	if (image.width != session.input.width || image.height != session.input.height ||
		image.pixelFormat != session.input.pixelFormat ||
		image.componentType != NVCV_U8 || image.gpuMem != NVCV_GPU ||
		(image.pixelFormat != NVCV_RGBA && image.pixelFormat != NVCV_BGRA)) return false;
	NvidiaVfxContextScope scope(session.Context());
	if (!scope || !session.WaitFor(*frame.producer)) return false;
	const bool bound = NvidiaVfxSucceeded(NvVFX_SetImage(session.effect, NVVFX_INPUT_IMAGE, frame.image), "Borrow VFG CUDA image");
	const bool ran = bound && session.Run("Run TrueHDR on VFG CUDA image");
	// Queue the reverse fence even on SDK failure: a partially submitted read
	// must finish before VFG reuses the shared generated/current allocation.
	const bool released = frame.producer->WaitFor(session);
	if (!released) session.Synchronize();
	const bool restored = NvidiaVfxSucceeded(NvVFX_SetImage(session.effect, NVVFX_INPUT_IMAGE, &session.input), "Restore owned TrueHDR input");
	return ran && released && restored && _ExportTrueHdr(context);
#else
	(void)context; (void)frame; return false;
#endif
}
EffectParameterApplyMode NvidiaVfxImageBackend::GetParameterApplyMode(std::string_view name) const noexcept {
	if (_kind == NvidiaVfxImageKind::TrueHdr) return name == "contrast" || name == "saturation" || name == "middleGray" || name == "peakNits" || name == "debanding"
		? EffectParameterApplyMode::Live : EffectParameterApplyMode::Restricted;
	if (_kind == NvidiaVfxImageKind::Upscale) return name == "sharpness" ? EffectParameterApplyMode::Live : EffectParameterApplyMode::RestartRequired;
	return name == "strength" ? EffectParameterApplyMode::Live : EffectParameterApplyMode::RestartRequired;
}
bool NvidiaVfxImageBackend::ApplyParameters(const EffectOption& option, std::span<const std::string> names) noexcept {
#ifdef MP_ENABLE_NVIDIA_VFX
	if (!_impl || !_impl->usable || std::ranges::any_of(names, [&](const auto& name) { return GetParameterApplyMode(name) != EffectParameterApplyMode::Live; })) return false;
	if (_kind == NvidiaVfxImageKind::TrueHdr) {
		const auto previous = ReadNvidiaVfxTrueHdrTunables(_impl->settings);
		const auto candidate = ReadNvidiaVfxTrueHdrTunables(option);
		if (!previous || !candidate) return false;
		// Slider values are normalized before reaching this boundary. A duplicate
		// desired value does not wait on the GPU or call into the SDK again.
		if (*previous == *candidate) return true;
		NvidiaVfxContextScope scope(_impl->session.Context());
		if (!scope || !_impl->session.Synchronize()) return false;
		_impl->session.ParameterRevision(_impl->parameterRevision + 1);
		const auto result = UpdateNvidiaVfxTrueHdrTunables(*previous, *candidate, [&](size_t i, uint32_t value) {
			return NvidiaVfxSucceeded(_impl->session.Call(Impl::hdrSelectors[i], [&] {
				return NvVFX_SetU32(_impl->session.effect, Impl::hdrSelectors[i], value);
			}), "SetTrueHdrTunable");
		});
		if (result == NvidiaVfxTrueHdrUpdate::Invalidated) {
			_impl->usable = false;
			Logger::Get().Error("Nvidia VFX TrueHDR parameter rollback failed; session cannot render further outputs");
			return false;
		}
		if (result != NvidiaVfxTrueHdrUpdate::Applied) {
			_impl->session.ParameterRevision(_impl->parameterRevision);
			return false;
		}
		++_impl->parameterRevision;
		_impl->settings = option; _option = option;
		return true;
	}
	NvidiaVfxContextScope scope(_impl->session.Context());
	if (!scope || !_impl->session.Synchronize()) return false;
	// VSR/Upscale still load a private candidate, preserving the working model
	// when its quality/strength configuration is rejected.
	NvVFX_Handle candidate = _impl->CreateCandidate(option);
	if (!candidate) return false;
	std::swap(candidate, _impl->session.effect);
	_impl->session.Call("DestroyReplacedEffect", [&] { NvVFX_DestroyEffect(candidate); }, true);
	_impl->settings = option; _option = option;
	return true;
#else
	(void)option; (void)names; return false;
#endif
}
} // namespace Magpie
