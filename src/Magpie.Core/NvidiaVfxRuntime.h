#pragma once
#include <string_view>
#include <memory>
#include <type_traits>
#include "NvidiaVfxMeasurements.h"

#if defined(MP_ENABLE_RTX_VIDEO_DENOISE) || defined(MP_ENABLE_NVIDIA_VFX)
#include <nvCVImage.h>
#include <nvVideoEffects.h>
#include <nvTransferD3D11.h>

namespace Magpie {
class DeviceResources;
class NvidiaVfxCudaTimings;
struct NvidiaVfxDiagnosticState;

bool NvidiaVfxSucceeded(NvCV_Status status, std::string_view operation) noexcept;
bool LoadNvidiaVfxRuntime(const wchar_t* featureLibrary, bool requireVersion13) noexcept;

// The CUDA primary context belongs to the selected D3D renderer adapter.
// Push/pop is per call; another native SDK's thread-local context is restored.
class NvidiaVfxCudaContext {
public:
	NvidiaVfxCudaContext() = default;
	NvidiaVfxCudaContext(const NvidiaVfxCudaContext&) = delete;
	NvidiaVfxCudaContext& operator=(const NvidiaVfxCudaContext&) = delete;
	~NvidiaVfxCudaContext();
	bool Initialize(DeviceResources& resources) noexcept;
	bool Push() const noexcept;
	static void Pop() noexcept;
	int ComputeMajor() const noexcept { return _major; }
	int ComputeMinor() const noexcept { return _minor; }
	void* Handle() const noexcept { return _context; }
	void Diagnostics(std::shared_ptr<NvidiaVfxDiagnosticState> state) noexcept { _diagnostics = std::move(state); }
private:
	std::shared_ptr<NvidiaVfxDiagnosticState> _diagnostics;
	void* _context = nullptr;
	int _device = -1, _major = 0, _minor = 0;
};
class NvidiaVfxContextScope {
public:
	explicit NvidiaVfxContextScope(const NvidiaVfxCudaContext& context) noexcept : _pushed(context.Push()) {}
	~NvidiaVfxContextScope() { if (_pushed) NvidiaVfxCudaContext::Pop(); }
	explicit operator bool() const noexcept { return _pushed; }
private:
	bool _pushed;
};

// Owns one stream/model and its CUDA/D3D wrappers. All callers enter Context()
// before using Images; release drains before destroying or unregistering them.
// Call LoadNvidiaVfxRuntime before constructing this object: SDK image default
// constructors already invoke proxy functions and cache their module addresses.
class NvidiaVfxSession {
public:
	NvidiaVfxSession();
	NvidiaVfxSession(const NvidiaVfxSession&) = delete;
	NvidiaVfxSession& operator=(const NvidiaVfxSession&) = delete;
	~NvidiaVfxSession();
	bool Initialize(DeviceResources& resources, const wchar_t* featureLibrary) noexcept;
	bool Synchronize() noexcept;
	bool Run(std::string_view operation) noexcept;
	void ParameterRevision(uint64_t revision) noexcept;
	template<class CallFn> auto Call(const char* operation, CallFn&& fn, bool lifecycle = false) noexcept {
		_BeginCall(operation, lifecycle);
		if constexpr (std::is_void_v<std::invoke_result_t<CallFn>>) {
			fn();
			_EndCall(0);
		} else {
			const auto status = fn();
			_EndCall(int(status));
			return status;
		}
	}
	// Establish a GPU dependency without waiting on the CPU. Both sessions must
	// retain the same selected primary context. A borrowed input must also enqueue
	// the reverse dependency before the producer reuses its allocation.
	bool WaitFor(NvidiaVfxSession& producer) noexcept;
	NvidiaVfxRuntimeMeasurements TakeMeasurements() noexcept;
	bool Import(ID3D11Texture2D* texture, NvCVImage& gpuImage) noexcept;
	bool Export(NvCVImage& gpuImage, ID3D11Texture2D* texture) noexcept;
#ifdef MP_ENABLE_NVIDIA_VFX
	// Copy packed HDR10 bits through a CUDA-compatible RGBA8 byte carrier.
	bool ExportPackedHdr10(NvCVImage& gpuImage, ID3D11Texture2D* byteTexture) noexcept;
#endif
	const NvidiaVfxCudaContext& Context() const noexcept { return _context; }
	NvVFX_Handle effect = nullptr;
	CUstream stream = nullptr;
	NvCVImage input, previous, output;
private:
	void _BeginCall(const char* operation, bool lifecycle) noexcept;
	void _EndCall(int status) noexcept;
	std::shared_ptr<NvidiaVfxDiagnosticState> _diagnostics;
	bool _Transfer(ID3D11Texture2D* texture, NvCVImage& gpuImage, bool importing) noexcept;
	NvidiaVfxCudaContext _context;
	std::unique_ptr<NvidiaVfxCudaTimings> _timings;
	NvidiaVfxRuntimeMeasurements _measurements;
	void* _readyEvent = nullptr;
	NvCVImage _inputD3D, _outputD3D, _temporary;
	// Persistent linear buffers for explicit RGBA/BGRA edge conversions.
	NvCVImage _inputBridge, _outputBridge;
	winrt::com_ptr<ID3D11Texture2D> _inputTexture, _outputTexture;
	using SynchronizeFn = NvCV_Status(NvVFX_API*)(CUstream);
	SynchronizeFn _synchronize = nullptr;
};
} // namespace Magpie
#endif
