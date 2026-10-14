#include "pch.h"
#include "NvidiaVfxRuntime.h"
#include "DeviceResources.h"
#include "Logger.h"
#include "StrHelper.h"
#include "Win32Helper.h"
#include "ScalingWindow.h"
#include <condition_variable>
#include <thread>
#include <mutex>

#if defined(MP_ENABLE_RTX_VIDEO_DENOISE) || defined(MP_ENABLE_NVIDIA_VFX)
// Used by the SDK's proxy loader; the application path is selected explicitly.
char* g_nvVFXSDKPath = nullptr;
namespace Magpie {

struct NvidiaVfxDiagnosticState {
	std::mutex mutex;
	uint64_t session = 0, call = 0, revision = 0, frame = 0;
	uint32_t run = 0, thread = 0;
	std::string feature;
	const char* operation = nullptr;
	ULONGLONG started = 0, lastWarning = 0;
	bool lifecycle = false;
};

// Observe SDK calls from a separate thread. The observer only records a slow
// stage; it never interrupts the SDK or frees resources still in use.
class NvidiaVfxDiagnosticMonitor {
public:
	static NvidiaVfxDiagnosticMonitor& Get() noexcept {
		static NvidiaVfxDiagnosticMonitor value;
		return value;
	}
	std::shared_ptr<NvidiaVfxDiagnosticState> Create() noexcept {
		auto state = std::make_shared<NvidiaVfxDiagnosticState>();
		std::scoped_lock lock(_mutex);
		state->session = ++_nextSession;
		_states.push_back(state);
		return state;
	}
private:
	NvidiaVfxDiagnosticMonitor() : _thread([this](std::stop_token stop) {
		while (!stop.stop_requested()) {
			std::vector<std::shared_ptr<NvidiaVfxDiagnosticState>> states;
			{
				std::unique_lock lock(_mutex);
				_wake.wait_for(lock, std::chrono::milliseconds(500), [&] { return stop.stop_requested(); });
				if (stop.stop_requested()) break;
				std::erase_if(_states, [](const auto& state) { return state.expired(); });
				for (const auto& weak : _states) if (auto state = weak.lock()) states.push_back(std::move(state));
			}
			for (const auto& state : states) {
				std::scoped_lock lock(state->mutex);
				const auto now = GetTickCount64();
				if (!state->operation || now - state->started < 2000 ||
					(state->lastWarning && now - state->lastWarning < 5000)) continue;
				state->lastWarning = now;
				Logger::Get().Warn(fmt::format(
					"Nvidia VFX stage still active: run={} session={} feature={} call={} revision={} frame={} thread={} stage={} elapsedMs={}",
					state->run, state->session, state->feature, state->call, state->revision, state->frame,
					state->thread, state->operation, now - state->started));
				Logger::Get().Flush();
			}
		}
	}) {}
	~NvidiaVfxDiagnosticMonitor() {
		_thread.request_stop(); _wake.notify_all(); _thread.join();
	}
	std::mutex _mutex;
	std::condition_variable _wake;
	std::vector<std::weak_ptr<NvidiaVfxDiagnosticState>> _states;
	uint64_t _nextSession = 0;
	std::jthread _thread;
};

static void BeginVfxCall(const std::shared_ptr<NvidiaVfxDiagnosticState>& state,
	const char* operation, bool lifecycle) noexcept {
	if (!state) return;
	std::scoped_lock lock(state->mutex);
	state->operation = operation; state->started = GetTickCount64(); state->lastWarning = 0;
	state->lifecycle = lifecycle; state->thread = GetCurrentThreadId(); ++state->call;
	if (lifecycle) {
		Logger::Get().Info(fmt::format("Nvidia VFX enter: run={} session={} feature={} call={} revision={} frame={} thread={} stage={}",
			state->run, state->session, state->feature, state->call, state->revision, state->frame, state->thread, operation));
		Logger::Get().Flush();
	}
}
static void EndVfxCall(const std::shared_ptr<NvidiaVfxDiagnosticState>& state, int status) noexcept {
	if (!state) return;
	std::scoped_lock lock(state->mutex);
	const auto elapsed = GetTickCount64() - state->started;
	if (state->lifecycle || status != 0 || elapsed >= 500) {
		Logger::Get().Info(fmt::format("Nvidia VFX exit: run={} session={} feature={} call={} revision={} frame={} stage={} status={} elapsedMs={}",
			state->run, state->session, state->feature, state->call, state->revision, state->frame, state->operation, status, elapsed));
	}
	state->operation = nullptr;
}
template<class Fn> static auto TraceVfxContext(const std::shared_ptr<NvidiaVfxDiagnosticState>& state,
	const char* operation, Fn&& fn) noexcept {
	BeginVfxCall(state, operation, true);
	const auto status = fn();
	EndVfxCall(state, int(status));
	return status;
}

bool NvidiaVfxSucceeded(NvCV_Status status, std::string_view operation) noexcept {
	if (status == NVCV_SUCCESS) return true;
	const char* message = NvCV_GetErrorStringFromCode(status);
	Logger::Get().Error(fmt::format("Nvidia VFX {} failed: {} ({})", operation, message ? message : "unknown", int(status)));
	return false;
}
static HMODULE LoadVfxModule(const std::filesystem::path& directory, const wchar_t* name) noexcept {
	const auto expected = directory / name;
	HMODULE module = GetModuleHandleW(name);
	if (!module) module = LoadLibraryExW(expected.c_str(), nullptr,
		LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (!module) { Logger::Get().Win32Error(fmt::format("Nvidia VFX component missing or cannot load: {}", StrHelper::UTF16ToUTF8(expected.native()))); return nullptr; }
	wchar_t path[32768]{};
	const auto size = GetModuleFileNameW(module, path, ARRAYSIZE(path));
	if (!size || size >= ARRAYSIZE(path) || _wcsicmp(path, expected.c_str()) != 0) {
		Logger::Get().Error(fmt::format("Nvidia VFX DLL path mismatch: {}", StrHelper::UTF16ToUTF8(name)));
		return nullptr;
	}
	// Proxy functions cache addresses for process lifetime. Keep verified module
	// references for the same lifetime rather than unloading under cached pointers.
	return module;
}
bool LoadNvidiaVfxRuntime(const wchar_t* featureLibrary, bool requireVersion13) noexcept {
	const auto directory = Win32Helper::GetExePath().parent_path();
	SetEnvironmentVariableW(L"NV_VIDEO_EFFECTS_PATH", L"USE_APP_PATH");
	if ((!GetModuleHandleW(L"nvcuda.dll") && !LoadLibraryExW(L"nvcuda.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) ||
		!LoadVfxModule(directory, L"nvngxruntime.dll") || !LoadVfxModule(directory, L"NVCVImage.dll") ||
		!LoadVfxModule(directory, L"NVVideoEffects.dll")) return false;
	if (_wcsicmp(featureLibrary, L"nvVFXVideoSuperRes.dll") == 0 && !LoadVfxModule(directory, L"nvngx_vsr.dll")) return false;
	if (_wcsicmp(featureLibrary, L"nvVFXTrueHDR.dll") == 0 && !LoadVfxModule(directory, L"nvngx_truehdr.dll")) return false;
	if (!LoadVfxModule(directory, featureLibrary)) return false;
	unsigned version = 0;
	if (!NvidiaVfxSucceeded(NvVFX_GetVersion(&version), "GetVersion")) return false;
	if (requireVersion13 && version != 0x01030000u) {
		Logger::Get().Error(fmt::format("Nvidia VFX requires matched Core 1.3.0.0; loaded 0x{:08X}", version));
		return false;
	}
	Logger::Get().Info(fmt::format("Nvidia VFX runtime: version=0x{:08X} feature={}", version, StrHelper::UTF16ToUTF8(featureLibrary)));
	return true;
}

template<class Fn> static Fn CudaFunction(const char* name) noexcept {
	return reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"nvcuda.dll"), name));
}
using CudaInitFn = int(WINAPI*)(unsigned);
using CudaD3DDeviceFn = int(WINAPI*)(int*, IDXGIAdapter*);
using CudaRetainFn = int(WINAPI*)(void**, int);
using CudaReleaseFn = int(WINAPI*)(int);
using CudaPushFn = int(WINAPI*)(void*);
using CudaPopFn = int(WINAPI*)(void**);
using CudaAttributeFn = int(WINAPI*)(int*, int, int);
using CudaEventCreateFn = int(WINAPI*)(void**, unsigned);
using CudaEventDestroyFn = int(WINAPI*)(void*);
using CudaEventRecordFn = int(WINAPI*)(void*, void*);
using CudaEventQueryFn = int(WINAPI*)(void*);
using CudaEventElapsedFn = int(WINAPI*)(float*, void*, void*);
using CudaStreamWaitEventFn = int(WINAPI*)(void*, void*, unsigned);

#ifdef MP_ENABLE_NVIDIA_VFX_ASYNC
static constexpr bool VfxAsyncEnabled = true;
#else
static constexpr bool VfxAsyncEnabled = false;
#endif

class NvidiaVfxCudaTimings {
public:
	~NvidiaVfxCudaTimings() {
		if (!_destroy) return;
		for (auto& slot : _slots) {
			if (slot.start) _destroy(slot.start);
			if (slot.end) _destroy(slot.end);
		}
	}
	int Begin(NvidiaVfxStageMeasurements& stage, CUstream stream) noexcept {
		Poll();
		if (!_create || !_destroy || !_record || !_query || !_elapsed) { ++stage.gpuSkipped; return -1; }
		for (size_t i = 0; i < _slots.size(); ++i) {
			auto& slot = _slots[i];
			if (slot.pending || slot.active) continue;
			if (!slot.start && (_create(&slot.start, 0) || _create(&slot.end, 0))) {
				if (slot.start) _destroy(slot.start);
				if (slot.end) _destroy(slot.end);
				slot = {}; ++stage.gpuSkipped; return -1;
			}
			if (_record(slot.start, stream)) { ++stage.gpuSkipped; return -1; }
			slot.stage = &stage; slot.active = true; return int(i);
		}
		++stage.gpuSkipped; return -1;
	}
	void End(int ticket, CUstream stream) noexcept {
		if (ticket < 0) return;
		auto& slot = _slots[size_t(ticket)]; slot.active = false;
		if (_record(slot.end, stream)) { ++slot.stage->gpuSkipped; return; }
		slot.pending = true;
	}
	void Poll() noexcept {
		if (!_query || !_elapsed) return;
		for (auto& slot : _slots) {
			if (!slot.pending) continue;
			const int ready = _query(slot.end);
			if (ready == 600) continue; // CUDA_ERROR_NOT_READY: never wait for timings.
			slot.pending = false; float elapsed = 0;
			if (ready || _elapsed(&elapsed, slot.start, slot.end)) { ++slot.stage->gpuSkipped; continue; }
			++slot.stage->gpuSamples; slot.stage->gpuSpanMs += elapsed;
		}
	}
private:
	struct Slot { void* start = nullptr; void* end = nullptr;
		NvidiaVfxStageMeasurements* stage = nullptr; bool active = false, pending = false; };
	std::array<Slot, 32> _slots;
	CudaEventCreateFn _create = CudaFunction<CudaEventCreateFn>("cuEventCreate");
	CudaEventDestroyFn _destroy = CudaFunction<CudaEventDestroyFn>("cuEventDestroy_v2");
	CudaEventRecordFn _record = CudaFunction<CudaEventRecordFn>("cuEventRecord");
	CudaEventQueryFn _query = CudaFunction<CudaEventQueryFn>("cuEventQuery");
	CudaEventElapsedFn _elapsed = CudaFunction<CudaEventElapsedFn>("cuEventElapsedTime");
};
NvidiaVfxSession::NvidiaVfxSession() : _diagnostics(NvidiaVfxDiagnosticMonitor::Get().Create()) {
	_context.Diagnostics(_diagnostics);
}
void NvidiaVfxSession::_BeginCall(const char* operation, bool lifecycle) noexcept { BeginVfxCall(_diagnostics, operation, lifecycle); }
void NvidiaVfxSession::_EndCall(int status) noexcept { EndVfxCall(_diagnostics, status); }
void NvidiaVfxSession::ParameterRevision(uint64_t revision) noexcept {
	std::scoped_lock lock(_diagnostics->mutex);
	_diagnostics->revision = revision;
}

bool NvidiaVfxCudaContext::Initialize(DeviceResources& resources) noexcept {
	if (_context) return false;
	const auto init = CudaFunction<CudaInitFn>("cuInit");
	const auto device = CudaFunction<CudaD3DDeviceFn>("cuD3D11GetDevice");
	const auto retain = CudaFunction<CudaRetainFn>("cuDevicePrimaryCtxRetain");
	const auto attribute = CudaFunction<CudaAttributeFn>("cuDeviceGetAttribute");
	if (!init || !device || !retain || !attribute ||
		TraceVfxContext(_diagnostics, "cuInit", [&] { return init(0); }) ||
		TraceVfxContext(_diagnostics, "cuD3D11GetDevice", [&] { return device(&_device, resources.GetGraphicsAdapter()); }) ||
		attribute(&_major, 75, _device) || attribute(&_minor, 76, _device) ||
		TraceVfxContext(_diagnostics, "cuDevicePrimaryCtxRetain", [&] { return retain(&_context, _device); })) {
		Logger::Get().Error("Nvidia VFX cannot select the CUDA device matching the D3D11 renderer");
		return false;
	}
	Logger::Get().Info(fmt::format("Nvidia VFX selected CUDA device={} compute={}.{}", _device, _major, _minor));
	return true;
}
NvidiaVfxCudaContext::~NvidiaVfxCudaContext() {
	if (_context) {
		const auto release = CudaFunction<CudaReleaseFn>("cuDevicePrimaryCtxRelease_v2");
		if (!release || TraceVfxContext(_diagnostics, "cuDevicePrimaryCtxRelease", [&] { return release(_device); }))
			Logger::Get().Error("Nvidia VFX CUDA primary context release failed");
	}
}
bool NvidiaVfxCudaContext::Push() const noexcept {
	const auto push = CudaFunction<CudaPushFn>("cuCtxPushCurrent_v2");
	return _context && push && push(_context) == 0;
}
void NvidiaVfxCudaContext::Pop() noexcept {
	void* context = nullptr;
	const auto pop = CudaFunction<CudaPopFn>("cuCtxPopCurrent_v2");
	if (!pop || pop(&context)) Logger::Get().Error("Nvidia VFX CUDA context pop failed");
}
bool NvidiaVfxSession::Initialize(DeviceResources& resources, const wchar_t* library) noexcept {
	{
		std::scoped_lock lock(_diagnostics->mutex);
		_diagnostics->run = ScalingWindow::RunId();
		_diagnostics->feature = StrHelper::UTF16ToUTF8(library);
	}
	if (!LoadNvidiaVfxRuntime(library, true) || !_context.Initialize(resources)) return false;
	NvidiaVfxContextScope scope(_context);
	_synchronize = reinterpret_cast<SynchronizeFn>(GetProcAddress(GetModuleHandleW(L"NVVideoEffects.dll"), "NvVFX_CudaStreamSynchronize"));
	if constexpr (NativeBackendTiming::Enabled) _timings = std::make_unique<NvidiaVfxCudaTimings>();
	return scope && _synchronize && NvidiaVfxSucceeded(Call("CudaStreamCreate", [&] { return NvVFX_CudaStreamCreate(&stream); }, true), "CudaStreamCreate");
}
NvidiaVfxSession::~NvidiaVfxSession() {
	NvidiaVfxContextScope scope(_context);
	if (!scope) return;
	if (stream && !Synchronize()) Logger::Get().Warn("Nvidia VFX teardown synchronization failed; releasing failed session on its owner thread");
	_timings.reset();
	if (_readyEvent) {
		if (auto destroy = CudaFunction<CudaEventDestroyFn>("cuEventDestroy_v2")) destroy(_readyEvent);
		_readyEvent = nullptr;
	}
	if (effect) { Call("DestroyEffect", [&] { NvVFX_DestroyEffect(effect); }, true); effect = nullptr; }
	const std::pair<const char*, NvCVImage*> images[] = {
		{"DeallocInput", &input}, {"DeallocPrevious", &previous}, {"DeallocOutput", &output},
		{"UnregisterInputD3D", &_inputD3D}, {"UnregisterOutputD3D", &_outputD3D}, {"DeallocTemporary", &_temporary},
		{"DeallocInputBridge", &_inputBridge}, {"DeallocOutputBridge", &_outputBridge}
	};
	for (const auto& [name, image] : images) Call(name, [&] { NvCVImage_Dealloc(image); }, true);
	if (stream) { Call("CudaStreamDestroy", [&] { NvVFX_CudaStreamDestroy(stream); }, true); stream = nullptr; }
}
bool NvidiaVfxSession::Synchronize() noexcept {
	const auto started = NativeBackendTiming::Now();
	const bool success = _synchronize && NvidiaVfxSucceeded(Call("CudaStreamSynchronize", [&] { return _synchronize(stream); }), "CudaStreamSynchronize");
	if constexpr (NativeBackendTiming::Enabled) {
		++_measurements.synchronizeCalls;
		_measurements.synchronizeCpuMs += NativeBackendTiming::ElapsedMilliseconds(started);
		if (_timings) _timings->Poll();
	}
	return success;
}
bool NvidiaVfxSession::Run(std::string_view operation) noexcept {
	if constexpr (VfxAsyncEnabled) {
		const auto query = CudaFunction<CudaEventQueryFn>("cuEventQuery");
		if (_readyEvent && query) {
			const int status = query(_readyEvent);
			if (status != 0 && status != 600) {
				Logger::Get().Error(fmt::format("Nvidia VFX asynchronous CUDA work failed: {}", status)); return false;
			}
		}
	}
	const auto started = NativeBackendTiming::Now();
	const int ticket = _timings ? _timings->Begin(_measurements.inference, stream) : -1;
	bool first = false;
	{
		std::scoped_lock lock(_diagnostics->mutex);
		first = ++_diagnostics->frame == 1;
	}
	bool success = NvidiaVfxSucceeded(Call("Run", [&] { return NvVFX_Run(effect, VfxAsyncEnabled ? 1 : 0); }, first), operation);
	if constexpr (VfxAsyncEnabled) {
		const auto create = CudaFunction<CudaEventCreateFn>("cuEventCreate");
		const auto record = CudaFunction<CudaEventRecordFn>("cuEventRecord");
		if (!create || !record || (!_readyEvent && create(&_readyEvent, 2)) || record(_readyEvent, stream)) {
			Logger::Get().Error("Nvidia VFX asynchronous completion event failed"); success = false;
		}
	}
	if (_timings) _timings->End(ticket, stream);
	if constexpr (NativeBackendTiming::Enabled) {
		++_measurements.inference.cpuCalls;
		_measurements.inference.cpuMs += NativeBackendTiming::ElapsedMilliseconds(started);
		if (_timings) _timings->Poll();
	}
	return success;
}
bool NvidiaVfxSession::WaitFor(NvidiaVfxSession& producer) noexcept {
	if (&producer == this) return true;
	if (_context.Handle() != producer._context.Handle()) return false;
	const auto create = CudaFunction<CudaEventCreateFn>("cuEventCreate");
	const auto record = CudaFunction<CudaEventRecordFn>("cuEventRecord");
	const auto wait = CudaFunction<CudaStreamWaitEventFn>("cuStreamWaitEvent");
	if (!create || !record || !wait ||
		(!producer._readyEvent && create(&producer._readyEvent, 2)) ||
		record(producer._readyEvent, producer.stream) || wait(stream, producer._readyEvent, 0)) {
		Logger::Get().Error("Nvidia VFX cross-stream input/output dependency failed"); return false;
	}
	return true;
}
NvidiaVfxRuntimeMeasurements NvidiaVfxSession::TakeMeasurements() noexcept {
	if constexpr (!NativeBackendTiming::Enabled) return {};
	NvidiaVfxContextScope scope(_context);
	if (scope && _timings) _timings->Poll();
	return std::exchange(_measurements, {});
}
bool NvidiaVfxSession::_Transfer(ID3D11Texture2D* texture, NvCVImage& image, bool importing) noexcept {
	if (!texture) return false;
	D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
	const auto encoding = desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ? NVCV_RGBA :
		desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ? NVCV_BGRA : NVCV_FORMAT_UNKNOWN;
	const auto component = NVCV_U8;
	const bool linearSdr = image.pixelFormat == NVCV_RGBA || image.pixelFormat == NVCV_BGRA;
	const bool textureSdr = encoding == NVCV_RGBA || encoding == NVCV_BGRA;
	const bool convert = image.pixelFormat != encoding;
	if (encoding == NVCV_FORMAT_UNKNOWN || desc.SampleDesc.Count != 1 || desc.MipLevels != 1 || desc.ArraySize != 1 ||
		image.width != desc.Width || image.height != desc.Height || image.componentType != component ||
		image.planar != NVCV_INTERLEAVED || image.gpuMem != NVCV_GPU || !image.pixels ||
		image.pixelBytes != 4 || image.pitch < int(uint64_t(desc.Width) * 4) ||
		(convert && !(linearSdr && textureSdr))) {
		Logger::Get().Error(fmt::format("Nvidia VFX {} image contract mismatch: DXGI={} {}x{} NvCV={}/{} layout={} pitch={} {}x{}",
			importing ? "import" : "export", uint32_t(desc.Format), desc.Width, desc.Height,
			int(image.pixelFormat), int(image.componentType), uint32_t(image.planar), image.pitch, image.width, image.height));
		return false;
	}
	const auto started = NativeBackendTiming::Now();
	auto& stage = importing ? _measurements.importImage : _measurements.exportImage;
	const int ticket = _timings ? _timings->Begin(stage, stream) : -1;
	bool timingEnded = false;
	auto timing = wil::scope_exit([&] {
		if (_timings) { if (!timingEnded) _timings->End(ticket, stream); _timings->Poll(); }
		if constexpr (NativeBackendTiming::Enabled) {
			++stage.cpuCalls; stage.cpuMs += NativeBackendTiming::ElapsedMilliseconds(started);
		}
	});
	auto& wrapper = importing ? _inputD3D : _outputD3D;
	auto& retained = importing ? _inputTexture : _outputTexture;
	if (retained.get() != texture) {
		if (!Synchronize()) return false;
		Call("UnregisterReboundD3D", [&] { NvCVImage_Dealloc(&wrapper); }, true);
		retained = nullptr;
		Logger::Get().Info(fmt::format("Nvidia VFX interop binding: {} DXGI={} {}x{} texture={:#x}",
			importing ? "input" : "output", uint32_t(desc.Format), desc.Width, desc.Height, uintptr_t(texture)));
		if (!NvidiaVfxSucceeded(Call("InitFromD3D11Texture", [&] { return NvCVImage_InitFromD3D11Texture(&wrapper, texture); }, true), "InitFromD3D11Texture")) return false;
		retained.copy_from(texture);
	}
	const auto wrapperValid = [&] {
		return wrapper.width == desc.Width && wrapper.height == desc.Height && wrapper.pixelFormat == encoding &&
			wrapper.componentType == component && wrapper.planar == NVCV_INTERLEAVED &&
			wrapper.gpuMem == NVCV_CUDA_ARRAY && wrapper.pixelBytes == 4;
	};
	if (!wrapperValid()) {
		Logger::Get().Error("Nvidia VFX D3D wrapper does not match the texture encoding"); return false;
	}
	auto& bridge = importing ? _inputBridge : _outputBridge;
	if (convert && (bridge.width != desc.Width || bridge.height != desc.Height || bridge.pixelFormat != encoding)) {
		if (!Synchronize()) return false;
		NvCVImage_Dealloc(&bridge);
		if (!NvidiaVfxSucceeded(NvCVImage_Alloc(&bridge, desc.Width, desc.Height, encoding, NVCV_U8,
			NVCV_INTERLEAVED, NVCV_GPU, 32), "Alloc color bridge")) return false;
	}
	// Cross-encoding conversion is confined to linear GPU images. Mapped D3D
	// arrays only copy to/from a matching encoding, in either direction.
	if (convert && !importing && !NvidiaVfxSucceeded(
		Call("ConvertExportEncoding", [&] { return NvCVImage_Transfer(&image, &bridge, 1.f, stream, &_temporary); }), "Convert export encoding")) return false;
	if (!NvidiaVfxSucceeded(Call(importing ? "MapInput" : "MapOutput", [&] { return NvCVImage_MapResource(&wrapper, stream); }), "MapResource")) return false;
	auto& transferImage = convert ? bridge : image;
	const bool mappedValid = wrapperValid();
	if (!mappedValid) Logger::Get().Error("Nvidia VFX mapped wrapper encoding changed unexpectedly");
	const bool transferred = mappedValid && NvidiaVfxSucceeded(Call(importing ? "TransferInput" : "TransferOutput", [&] { return importing
		? NvCVImage_Transfer(&wrapper, &transferImage, 1.f, stream, &_temporary)
		: NvCVImage_Transfer(&transferImage, &wrapper, 1.f, stream, &_temporary); }), "Transfer");
	const bool unmapped = NvidiaVfxSucceeded(Call(importing ? "UnmapInput" : "UnmapOutput", [&] { return NvCVImage_UnmapResource(&wrapper, stream); }), "UnmapResource");
	const bool converted = transferred && unmapped && (!convert || !importing || NvidiaVfxSucceeded(
		Call("ConvertImportEncoding", [&] { return NvCVImage_Transfer(&bridge, &image, 1.f, stream, &_temporary); }), "Convert import encoding"));
	if (_timings) { _timings->End(ticket, stream); timingEnded = true; }
	// Experimental async builds rely on same-stream map/transfer/unmap ordering.
	// Rebinding and destruction still drain. Default builds retain the CPU wait.
	return converted && (VfxAsyncEnabled || Synchronize());
}
bool NvidiaVfxSession::Import(ID3D11Texture2D* texture, NvCVImage& image) noexcept { return _Transfer(texture, image, true); }
bool NvidiaVfxSession::Export(NvCVImage& image, ID3D11Texture2D* texture) noexcept { return _Transfer(texture, image, false); }
#ifdef MP_ENABLE_NVIDIA_VFX
bool NvidiaVfxSession::ExportPackedHdr10(NvCVImage& image, ID3D11Texture2D* texture) noexcept {
	D3D11_TEXTURE2D_DESC desc{};
	if (texture) texture->GetDesc(&desc);
	if (!texture || desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM || image.pixelFormat != NVCV_RGB10A2 ||
		image.componentType != NVCV_P32 || image.planar != NVCV_INTERLEAVED || image.gpuMem != NVCV_GPU ||
		image.pixelBytes != 4 || !image.pixels) {
		Logger::Get().Error("Nvidia VFX packed HDR10 export requires an RGBA8 byte carrier and RGB10A2/P32 GPU image"); return false;
	}
	// NvCV Init creates a non-owning view. Same-format transfer copies all four
	// bytes without unpacking, quantizing, or changing the model's descriptor.
	NvCVImage bytes;
	if (!NvidiaVfxSucceeded(NvCVImage_Init(&bytes, image.width, image.height, image.pitch, image.pixels,
		NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU), "Init packed HDR10 byte view")) return false;
	return _Transfer(texture, bytes, false);
}
#endif

} // namespace Magpie
#endif
