#include "pch.h"
#include "RTXVideoHdr.h"
#include "DeviceResources.h"
#include "Logger.h"
#include "NgxRuntimeGuard.h"
#include "Win32Helper.h"

namespace Magpie {
RTXVideoHdr::~RTXVideoHdr() {
	if (_instance && _destroy) {
		RtxHdrCleanupStage stage{};
		uint32_t status = 0;
		const HRESULT result = _destroyEx ? _destroyEx(_instance, &stage, &status) : _destroy(_instance);
		if (FAILED(result)) {
			Logger::Get().Error(fmt::format("RTX Video HDR cleanup failed: stage={} HRESULT={:#x} NGX/exception={:#x}; restart required",
				RtxHdrCleanupStageName(stage), static_cast<uint32_t>(result), status));
			NgxRuntimeGuard::MarkShutdownFailed();
		}
	}
	// A faulted SDK may still own driver callbacks. Retain its module until the
	// process restarts rather than unloading executable code underneath them.
	if (NgxRuntimeGuard::IsFaulted()) (void)_module.release();
}

bool RTXVideoHdr::Initialize(DeviceResources& resources, ID3D11Texture2D* input,
	ID3D11Texture2D* output, const EffectOption& option) noexcept {
	if (NgxRuntimeGuard::IsFaulted()) return false;
	_context = resources.GetD3DDC();
	if (!Resize(resources, input, output)) return false;
	if (!ApplyLiveParameters(option, {})) return false;
	const auto directory = Win32Helper::GetExePath().parent_path();
	_module.reset(LoadLibraryExW((directory / L"Magpie.RtxVideo.dll").c_str(), nullptr,
		LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
	if (!_module) { Logger::Get().Win32Error("Load RTX Video HDR bridge failed"); return false; }
	const auto create = reinterpret_cast<RtxHdrCreateFn>(GetProcAddress(_module.get(), "MagpieRtxHdrCreate"));
	_draw = reinterpret_cast<RtxHdrDrawFn>(GetProcAddress(_module.get(), "MagpieRtxHdrDraw"));
	_destroy = reinterpret_cast<RtxHdrDestroyFn>(GetProcAddress(_module.get(), "MagpieRtxHdrDestroy"));
	_destroyEx = reinterpret_cast<RtxHdrDestroyExFn>(GetProcAddress(_module.get(), "MagpieRtxHdrDestroyEx"));
	if (!create || !_draw || !_destroy) return false;
	uint32_t status = 0;
	const auto result = create(resources.GetD3DDevice(), directory.c_str(), &_instance, &status);
	if (FAILED(result)) {
		Logger::Get().Error(fmt::format("RTX Video HDR initialization failed: HRESULT={:#x} NGX/exception={:#x}",
			static_cast<uint32_t>(result), status));
		if (result == E_UNEXPECTED) NgxRuntimeGuard::MarkShutdownFailed();
		return false;
	}
	Logger::Get().Info("RTX Video HDR: SDR BGRA8/RGBA8 -> linear scRGB FP16; isolated RTX Video SDK 1.1");
	return true;
}

bool RTXVideoHdr::ApplyLiveParameters(const EffectOption& option, std::span<const std::string>) noexcept {
	if (ClassifyHdrComponent(option.name) != HdrComponentKind::RtxVideoHdr) return false;
	const auto plan = BuildHdrComponentPlan(std::span(&option, 1));
	if (plan.error != HdrComponentError::None) return false;
	const auto get = [&](const char* name, uint32_t fallback) {
		const auto it = option.parameters.find(name);
		return it == option.parameters.end() ? fallback : static_cast<uint32_t>(std::lround(it->second));
	};
	_settings = { get("contrast", 100), get("saturation", 100), get("middleGray", 50), get("peakNits", 1000) };
	return true;
}

bool RTXVideoHdr::Resize(DeviceResources&, ID3D11Texture2D* input, ID3D11Texture2D* output) noexcept {
	if (!input || !output) return false;
	D3D11_TEXTURE2D_DESC in{}, out{};
	input->GetDesc(&in); output->GetDesc(&out);
	return MagpieRtxHdrEndpointsSupported(in, out);
}

bool RTXVideoHdr::Draw(const NativeEffectDrawContext& context) noexcept {
	if (!_instance || !_draw || NgxRuntimeGuard::IsFaulted()) return false;
	_context->ClearState();
	uint32_t status = 0;
	const auto result = _draw(_instance, context.input, context.output, &_settings, &status);
	_context->ClearState();
	if (FAILED(result)) {
		Logger::Get().Error(fmt::format("RTX Video HDR evaluation failed: HRESULT={:#x} NGX/exception={:#x}",
			static_cast<uint32_t>(result), status));
		if (result == E_UNEXPECTED) NgxRuntimeGuard::MarkShutdownFailed();
		return false;
	}
	return true;
}
}
