#include "pch.h"
#include "NvidiaVfxFrameGenerator.h"
#include "NvidiaVfxRuntime.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include "Logger.h"
#ifdef MP_ENABLE_NVIDIA_VFX
#include <nvVFXVideoFrameGeneration.h>
#endif

namespace Magpie {
#ifdef MP_ENABLE_NVIDIA_VFX
struct NvidiaVfxFrameGenerator::Impl {
	NvidiaVfxSession session;
	DeviceResources* resources = nullptr;
	winrt::com_ptr<ID3D11Texture2D> generated;
	winrt::com_ptr<ID3D11ComputeShader> compareShader;
	winrt::com_ptr<ID3D11ShaderResourceView> generatedView, currentView;
	winrt::com_ptr<ID3D11Texture2D> currentViewTexture;
	winrt::com_ptr<ID3D11Buffer> difference, differenceReadback;
	winrt::com_ptr<ID3D11UnorderedAccessView> differenceView;
	NvidiaVfxFrameIdentity previousIdentity{};
	bool hasPrevious = false;
	bool currentUsesInput = true;
	uint32_t consecutiveFailures = 0;
	NvidiaVfxFrameMeasurements measurements;
	NvidiaVfxD3dTimings comparisonTimings;

	// The vendor's automatic cut detector copies B on a discontinuity. Do not
	// publish that same real image M times. Read back only a four-byte GPU flag,
	// never video pixels. This also handles a static pair without false motion.
	bool IsCurrentFrame(ID3D11Texture2D* current, bool& equal) noexcept {
		const auto started = NativeBackendTiming::Now();
		auto measure = wil::scope_exit([&] {
			if constexpr (NativeBackendTiming::Enabled) {
				++measurements.comparisons;
				measurements.comparisonCpuMs += NativeBackendTiming::ElapsedMilliseconds(started);
			}
		});
		D3D11_TEXTURE2D_DESC textureDesc{}; current->GetDesc(&textureDesc);
		if (currentViewTexture.get() != current) {
			winrt::com_ptr<ID3D11ShaderResourceView> candidate;
			if (FAILED(resources->GetD3DDevice()->CreateShaderResourceView(current, nullptr, candidate.put()))) return false;
			currentView = std::move(candidate);
			currentViewTexture.copy_from(current);
			if constexpr (NativeBackendTiming::Enabled) ++measurements.srvCreations;
		}
		auto* dc = resources->GetD3DDC();
		const int gpuTicket = comparisonTimings.Begin(resources->GetD3DDevice(), dc);
		dc->ClearState();
		const UINT zeros[4]{}; dc->ClearUnorderedAccessViewUint(differenceView.get(), zeros);
		ID3D11ShaderResourceView* views[]{ generatedView.get(), currentView.get() };
		ID3D11UnorderedAccessView* uav = differenceView.get();
		dc->CSSetShader(compareShader.get(), nullptr, 0);
		dc->CSSetShaderResources(0, 2, views); dc->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
		dc->Dispatch((textureDesc.Width + 7) / 8, (textureDesc.Height + 7) / 8, 1);
		comparisonTimings.End(dc, gpuTicket);
		dc->ClearState(); dc->CopyResource(differenceReadback.get(), difference.get());
		D3D11_MAPPED_SUBRESOURCE mapped{};
		const auto readbackStarted = NativeBackendTiming::Now();
		const HRESULT mappedStatus = dc->Map(differenceReadback.get(), 0, D3D11_MAP_READ, 0, &mapped);
		if constexpr (NativeBackendTiming::Enabled) measurements.readbackWaitCpuMs += NativeBackendTiming::ElapsedMilliseconds(readbackStarted);
		if (FAILED(mappedStatus)) return false;
		equal = *static_cast<const uint32_t*>(mapped.pData) == 0;
		dc->Unmap(differenceReadback.get(), 0);
		return true;
	}
};
#else
struct NvidiaVfxFrameGenerator::Impl {};
#endif

NvidiaVfxFrameGenerator::NvidiaVfxFrameGenerator() = default;
NvidiaVfxFrameGenerator::~NvidiaVfxFrameGenerator() = default;
bool NvidiaVfxFrameGenerator::Initialize(DeviceResources& resources, ID3D11Texture2D* input,
	const NvidiaVfxFrameGenerationSettings& settings) noexcept {
#ifdef MP_ENABLE_NVIDIA_VFX
	if (!input || !NvidiaVfxFrameGenerationSettingsValid(settings)) return false;
	D3D11_TEXTURE2D_DESC td{}; input->GetDesc(&td);
	if ((td.Format != DXGI_FORMAT_R8G8B8A8_UNORM && td.Format != DXGI_FORMAT_B8G8R8A8_UNORM) || td.SampleDesc.Count != 1) {
		Logger::Get().Error("Nvidia VFX VFG requires an SDR 8-bit stage; add HDR -> SDR before it"); return false;
	}
	// NvCVImage constructors call their proxy, so establish app-local loading first.
	if (!LoadNvidiaVfxRuntime(L"nvVFXVideoFrameGeneration.dll", true)) return false;
	auto candidate = std::make_unique<Impl>();
	if (!candidate->session.Initialize(resources, L"nvVFXVideoFrameGeneration.dll")) return false;
	const auto& cuda = candidate->session.Context();
	if (!((cuda.ComputeMajor() == 8 && cuda.ComputeMinor() == 9) || cuda.ComputeMajor() == 10 || cuda.ComputeMajor() == 12)) {
		Logger::Get().Error("Nvidia VFX VFG Windows x64 requires Ada or Blackwell on the selected rendering GPU"); return false;
	}
	NvidiaVfxContextScope scope(cuda);
	if (!scope) return false;
	const auto encoding = td.Format == DXGI_FORMAT_B8G8R8A8_UNORM ? NVCV_BGRA : NVCV_RGBA;
	for (auto* image : { &candidate->session.previous, &candidate->session.input, &candidate->session.output })
		if (!NvidiaVfxSucceeded(NvCVImage_Alloc(image, td.Width, td.Height, encoding, NVCV_U8,
			NVCV_INTERLEAVED, NVCV_GPU, 32), "VFG Alloc")) return false;
	NvVFX_Handle& effect = candidate->session.effect;
	if (!NvidiaVfxSucceeded(NvVFX_CreateEffect(NVVFX_FX_VIDEO_FRAME_GENERATION, &effect), "VFG CreateEffect") ||
		!NvidiaVfxSucceeded(NvVFX_SetCudaStream(effect, NVVFX_CUDA_STREAM, candidate->session.stream), "VFG SetCudaStream")) return false;
	const std::pair<const char*, uint32_t> parameters[]{
		{NVVFX_INPUT_WIDTH, td.Width}, {NVVFX_INPUT_HEIGHT, td.Height},
		{NVVFXVIDEOFRAMEGENERATION_MODE, settings.model},
		{NVVFXVIDEOFRAMEGENERATION_FRAME_MULTIPLIER, settings.multiplier},
		{NVVFXVIDEOFRAMEGENERATION_SHOT_CHANGE, 0},
		{NVVFXVIDEOFRAMEGENERATION_AUTOMATIC_SHOT_CHANGE_DETECTION_ENABLED, settings.automaticShotDetection ? 1u : 0u}
	};
	for (const auto& [key, value] : parameters)
		if (!NvidiaVfxSucceeded(NvVFX_SetU32(effect, key, value), "VFG SetParameter")) return false;
	if (!NvidiaVfxSucceeded(NvVFX_SetImage(effect, NVVFX_INPUT_IMAGE_0, &candidate->session.previous), "VFG previous") ||
		!NvidiaVfxSucceeded(NvVFX_SetImage(effect, NVVFX_INPUT_IMAGE_1, &candidate->session.input), "VFG current") ||
		!NvidiaVfxSucceeded(NvVFX_SetImage(effect, NVVFX_OUTPUT_IMAGE, &candidate->session.output), "VFG output") ||
		!NvidiaVfxSucceeded(NvVFX_Load(effect), "VFG Load")) return false;
	candidate->resources = &resources;
	candidate->generated = DirectXHelper::CreateTexture2D(resources.GetD3DDevice(), td.Format, td.Width, td.Height,
		D3D11_BIND_SHADER_RESOURCE);
	if (!candidate->generated) return false;
	constexpr char compare[] = R"(
Texture2D<float4> generated : register(t0);
Texture2D<float4> current : register(t1);
RWByteAddressBuffer difference : register(u0);
groupshared uint groupDifferent;
[numthreads(8,8,1)] void Main(uint3 id : SV_DispatchThreadID, uint localIndex : SV_GroupIndex) {
    if (localIndex == 0) groupDifferent = 0;
    GroupMemoryBarrierWithGroupSync();
    uint w,h; current.GetDimensions(w,h);
    // Edge threads must reach both barriers even when outside the texture.
    if (id.x < w && id.y < h &&
        any(generated.Load(int3(id.xy,0)) != current.Load(int3(id.xy,0)))) {
        InterlockedOr(groupDifferent, 1u);
    }
    GroupMemoryBarrierWithGroupSync();
    if (localIndex == 0 && groupDifferent != 0) {
        uint old; difference.InterlockedOr(0,1,old);
    }
})";
	winrt::com_ptr<ID3DBlob> blob;
	if (!DirectXHelper::CompileComputeShader(compare, "Main", blob.put(), "NvidiaVfxFrameCompare", nullptr, {}, true) ||
		FAILED(resources.GetD3DDevice()->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, candidate->compareShader.put())) ||
		FAILED(resources.GetD3DDevice()->CreateShaderResourceView(candidate->generated.get(), nullptr, candidate->generatedView.put()))) return false;
	D3D11_BUFFER_DESC bd{}; bd.ByteWidth = 4; bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS; bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	if (FAILED(resources.GetD3DDevice()->CreateBuffer(&bd, nullptr, candidate->difference.put()))) return false;
	D3D11_UNORDERED_ACCESS_VIEW_DESC ud{}; ud.Format = DXGI_FORMAT_R32_TYPELESS;
	ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements = 1; ud.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
	if (FAILED(resources.GetD3DDevice()->CreateUnorderedAccessView(candidate->difference.get(), &ud, candidate->differenceView.put()))) return false;
	bd.Usage = D3D11_USAGE_STAGING; bd.BindFlags = 0; bd.MiscFlags = 0; bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	if (FAILED(resources.GetD3DDevice()->CreateBuffer(&bd, nullptr, candidate->differenceReadback.put()))) return false;
	_impl = std::move(candidate); _settings = settings;
	return true;
#else
	(void)resources; (void)input; (void)settings;
	Logger::Get().Error("Nvidia VFX VFG needs a build with VFX 1.3.0.0"); return false;
#endif
}
bool NvidiaVfxFrameGenerator::Draw(ID3D11Texture2D* input, NvidiaVfxFrameIdentity identity,
	bool newCapture, const Publish& publish, const PublishCuda& publishCuda) noexcept {
#ifdef MP_ENABLE_NVIDIA_VFX
	if (!_impl || !input) return false;
	if (!newCapture) return true;
	auto& s = _impl->session;
	NvidiaVfxContextScope scope(s.Context());
	// Only the roles move. Owning NvCVImage descriptors stay at stable addresses.
	auto& current = _impl->currentUsesInput ? s.input : s.previous;
	auto& previous = _impl->currentUsesInput ? s.previous : s.input;
	if (!scope || !s.Import(input, current)) { _impl->hasPrevious = false; return false; }
	const auto publishOutput = [&](ID3D11Texture2D* texture, NvCVImage& image, uint32_t index) {
		return publishCuda ? publishCuda({&image, &s}, index, _settings.multiplier) :
			publish(texture, index, _settings.multiplier);
	};
	const auto old = _impl->previousIdentity;
	const bool pair = _impl->hasPrevious && identity.frameId > old.frameId &&
		identity.captureSequence == old.captureSequence && identity.resourceGeneration == old.resourceGeneration &&
		identity.historyRevision == old.historyRevision;
	bool interpolationSucceeded = true;
	if (pair && _impl->consecutiveFailures < 3) {
		interpolationSucceeded = NvidiaVfxSucceeded(NvVFX_SetImage(s.effect, NVVFX_INPUT_IMAGE_0, &previous), "VFG bind A") &&
			NvidiaVfxSucceeded(NvVFX_SetImage(s.effect, NVVFX_INPUT_IMAGE_1, &current), "VFG bind B") &&
			NvidiaVfxSucceeded(NvVFX_SetU32(s.effect, NVVFXVIDEOFRAMEGENERATION_SHOT_CHANGE, 0), "VFG clear ShotChange");
		for (uint32_t index = 1; interpolationSucceeded && index < _settings.multiplier; ++index) {
			interpolationSucceeded = NvidiaVfxSucceeded(NvVFX_SetU32(s.effect, NVVFXVIDEOFRAMEGENERATION_FRAME_INDEX, index), "VFG FrameIndex") &&
				s.Run("VFG Run");
			// The exact first-output comparison still needs its D3D texture.
			// Otherwise an adjacent CUDA consumer can read the SDK output directly.
			if (interpolationSucceeded && (!publishCuda || (index == 1 && _settings.automaticShotDetection)))
				interpolationSucceeded = s.Export(s.output, _impl->generated.get());
			if (!interpolationSucceeded) break;
			if (index == 1 && _settings.automaticShotDetection) {
				bool equal = false;
				if (!_impl->IsCurrentFrame(input, equal)) { interpolationSucceeded = false; break; }
				if (equal) {
					if constexpr (NativeBackendTiming::Enabled) ++_impl->measurements.bypassedPairs;
					break;
				}
			}
			if (!publishOutput(_impl->generated.get(), s.output, index)) { _impl->hasPrevious = false; return false; }
			if constexpr (NativeBackendTiming::Enabled) ++_impl->measurements.generatedOutputs;
		}
	}
	if (!interpolationSucceeded) {
		++_impl->consecutiveFailures;
		Logger::Get().Warn("Nvidia VFX VFG failed; publishing the real frame through the same post-processing stage");
	} else if (pair && _impl->consecutiveFailures < 3) _impl->consecutiveFailures = 0;
	if (!publishOutput(input, current, _settings.multiplier)) { _impl->hasPrevious = false; return false; }
	if constexpr (NativeBackendTiming::Enabled) ++_impl->measurements.realOutputs;
	// B becomes the next A without a full-image copy. The other allocation is
	// reused only after this group's outputs have been consumed by publication.
	_impl->currentUsesInput = !_impl->currentUsesInput;
	_impl->previousIdentity = identity; _impl->hasPrevious = interpolationSucceeded;
	return true;
#else
	(void)input; (void)identity; (void)newCapture; (void)publish; (void)publishCuda; return false;
#endif
}
NvidiaVfxFrameMeasurements NvidiaVfxFrameGenerator::TakeMeasurements() noexcept {
#ifdef MP_ENABLE_NVIDIA_VFX
	if (!_impl || !NativeBackendTiming::Enabled) return {};
	auto result = std::exchange(_impl->measurements, {});
	result.runtime = _impl->session.TakeMeasurements();
	result.comparisonGpu = _impl->comparisonTimings.Take(_impl->resources->GetD3DDC());
	return result;
#else
	return {};
#endif
}
bool NvidiaVfxFrameGenerator::ApplyModel(DeviceResources& resources, ID3D11Texture2D* input, uint32_t model) noexcept {
	if (!NvidiaVfxVfgModelValid(float(model))) return false;
	auto settings = _settings; settings.model = model;
	return Initialize(resources, input, settings);
}
void NvidiaVfxFrameGenerator::RequestHistoryReset() noexcept {
#ifdef MP_ENABLE_NVIDIA_VFX
	if (_impl) { _impl->hasPrevious = false; _impl->consecutiveFailures = 0; }
#endif
}
bool NvidiaVfxFrameGenerator::Drain() noexcept {
#ifdef MP_ENABLE_NVIDIA_VFX
	if (!_impl) return true;
	NvidiaVfxContextScope scope(_impl->session.Context()); return scope && _impl->session.Synchronize();
#else
	return true;
#endif
}
}
