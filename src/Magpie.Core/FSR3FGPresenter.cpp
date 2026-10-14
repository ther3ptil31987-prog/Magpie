#include "pch.h"
#include "FSR3FGPresenter.h"
#include "FSR3FGPolicy.h"
#include "FSR3FGMotion.h"
#include "FramePacingWait.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include "Logger.h"
#include "ScalingWindow.h"
#include "Win32Helper.h"
#include <atomic>
#include <array>
#include <mutex>
#include <dcomp.h>

#ifdef MP_ENABLE_FSR3_FRAME_GENERATION
#include <d3d12.h>
#include <ffx_api.h>
#include <dx12/ffx_api_dx12.h>
#include <ffx_framegeneration.h>
#include <dx12/ffx_api_framegeneration_dx12.h>

namespace Magpie {
static constexpr uint32_t BUFFER_COUNT = 3;
static constexpr DXGI_FORMAT COLOR_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
static std::atomic<bool> restartRequired = false;

struct FSR3FGPresenter::Impl {
	~Impl();
	bool Shutdown() noexcept;
	// Serialize Configure/Prepare/Present/resize/destruction. Callback dispatch
	// is inline in the pinned 3.1.7 Present and uses the caller's lock.
	std::mutex apiMutex;
	HWND hwnd = nullptr;
	ID3D11Device5* device11 = nullptr;
	ID3D11DeviceContext4* context11 = nullptr;
	IDXGIFactory7* factory = nullptr;
	winrt::com_ptr<ID3D12Device> device12;
	winrt::com_ptr<ID3D12CommandQueue> queue12;
	std::array<winrt::com_ptr<ID3D12CommandAllocator>, BUFFER_COUNT> allocators12;
	winrt::com_ptr<ID3D12GraphicsCommandList> commandList12;
	winrt::com_ptr<IDXGISwapChain4> swapChain;
	std::array<winrt::com_ptr<ID3D12Resource>, BUFFER_COUNT> backBuffers;
	winrt::com_ptr<ID3D11Texture2D> color11, motion11;
	winrt::com_ptr<ID3D11RenderTargetView> colorRtv11;
	winrt::com_ptr<ID3D11UnorderedAccessView> motionUav11;
	winrt::com_ptr<ID3D12Resource> color12, motion12, zeroMotion12, flatDepth12;
	winrt::com_ptr<ID3D12DescriptorHeap> clearHeap12, clearCpuHeap12;
	winrt::com_ptr<ID3D11ComputeShader> motionShader11;
	winrt::com_ptr<ID3D11Buffer> motionConstants11;
	FSR3FGMotionSource sourceMotion;
	winrt::com_ptr<ID3D11Fence> fence11;
	winrt::com_ptr<ID3D12Fence> fence12;
	std::array<uint64_t, BUFFER_COUNT> allocatorFenceValues{};
	uint64_t fenceValue = 0;
	wil::unique_event_nothrow fenceEvent;
	wil::unique_handle frameLatencyWaitableObject;
	FrameLatencyGate frameLatencyGate;
	std::chrono::steady_clock::time_point capacityWaitStarted{};
	winrt::com_ptr<IDCompositionDesktopDevice> overlayDCompDevice;
	winrt::com_ptr<IDCompositionTarget> overlayDCompTarget;
	winrt::com_ptr<IDCompositionVisual2> overlayDCompVisual;
	winrt::com_ptr<IDCompositionVirtualSurface> overlayDCompSurface;
	bool overlayDrawActive = false;
	HMODULE loaderModule = nullptr, providerModule = nullptr;
	decltype(&ffxCreateContext) createContext = nullptr;
	decltype(&ffxDestroyContext) destroyContext = nullptr;
	decltype(&ffxConfigure) configure = nullptr;
	decltype(&ffxQuery) query = nullptr;
	decltype(&ffxDispatch) dispatch = nullptr;
	ffxContext fgContext = nullptr, swapchainContext = nullptr;
	uint64_t fgVersionId = 0, swapchainVersionId = 0;
	uint32_t width = 0, height = 0;
	RECT generationRect{};
	FSR3FGHistory history;
	FSR3FGMotionAdmission motionAdmission;
	FrameGuidanceAvailability motionAvailability = FrameGuidanceAvailability::Unavailable;
	std::optional<uint32_t> callbackGeneratedCount;
	FSR3FGSourceSample sourceSample;
	FSR3FGHistory::Decision decision{};
	bool externalMotionEnabled = true, externalMotionValid = false, externalMotionReset = true;
	bool idlePresentation = false;
	bool geometryReset = true, capacityBusy = false, fatal = false;
	uint64_t callbackDispatches = 0, interpolations = 0, resetPassThroughs = 0, baseSubmissions = 0;
	uint64_t invalidMotionSamples = 0;
	ffxReturnCode_t callbackResult = FFX_API_RETURN_OK;
	PresenterFailure failure;
	bool Fail(std::string_view operation, HRESULT result = E_FAIL) noexcept {
		const auto detail = fmt::format("FSR3FG {}", operation);
		failure.Record(detail, static_cast<uint32_t>(result));
		Logger::Get().ComError(detail, result);
		fatal = true;
		return false;
	}
	bool Check(ffxReturnCode_t result, std::string_view operation) noexcept {
		if (result == FFX_API_RETURN_OK) return true;
		// AMD return values are not HRESULTs. Keep the AMD code in the context
		// and report E_FAIL through the existing system-error field.
		return Fail(fmt::format("{}: AMD result={}", operation, result));
	}
};

static bool WaitForFence(FSR3FGPresenter::Impl& impl, uint64_t value) noexcept {
	if (!value) return true;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
	bool armed = false;
	for (;;) {
		const uint64_t completed = impl.fence12->GetCompletedValue();
		if (completed == UINT64_MAX) return impl.Fail("device removed during fence wait", impl.device12->GetDeviceRemovedReason());
		if (completed >= value) return true;
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline) return impl.Fail("GPU fence wait timed out");
		if (!armed) {
			const HRESULT hr = impl.fence12->SetEventOnCompletion(value, impl.fenceEvent.get());
			if (FAILED(hr)) return impl.Fail("SetEventOnCompletion", hr);
			armed = true;
		}
		// A prior timed-out registration can signal this same event. A wake is
		// only a notification, never proof that our newer target has completed.
		const DWORD timeout = DWORD(std::chrono::ceil<std::chrono::milliseconds>(deadline - now).count());
		const DWORD result = WaitForSingleObject(impl.fenceEvent.get(), timeout);
		if (result != WAIT_OBJECT_0 && result != WAIT_TIMEOUT) return impl.Fail("GPU fence wait failed");
	}
}
static bool WaitForQueue(FSR3FGPresenter::Impl& impl) noexcept {
	const uint64_t value = ++impl.fenceValue;
	const HRESULT hr = impl.queue12->Signal(impl.fence12.get(), value);
	return SUCCEEDED(hr) ? WaitForFence(impl, value) : impl.Fail("queue Signal", hr);
}

static bool DisableAndDrain(FSR3FGPresenter::Impl& impl) noexcept {
	bool ok = true;
	// SetFrameGuidance can enqueue D3D11 work before BeginFrame obtains
	// capacity. Drain that work too, including partial initialization/resize.
	if (impl.context11 && impl.fence11 && impl.queue12 && impl.fence12) {
		const uint64_t ready = ++impl.fenceValue;
		HRESULT hr = impl.context11->Signal(impl.fence11.get(), ready);
		impl.context11->Flush();
		if (SUCCEEDED(hr)) hr = impl.queue12->Wait(impl.fence12.get(), ready);
		if (FAILED(hr)) ok = impl.Fail("drain frontend work", hr);
	}
	if (impl.fgContext && impl.swapChain) {
		ffxConfigureDescFrameGeneration config{};
		config.header.type = FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;
		config.swapChain = impl.swapChain.get();
		// Null callbacks and HUDLessColor; disabling waits for interpolation/UI.
		ok = impl.Check(impl.configure(&impl.fgContext, &config.header), "disable callbacks and FG") && ok;
	}
	if (impl.swapchainContext) {
		ffxDispatchDescFrameGenerationSwapChainWaitForPresentsDX12 wait{};
		wait.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_WAIT_FOR_PRESENTS_DX12;
		ok = impl.Check(impl.dispatch(&impl.swapchainContext, &wait.header), "drain SDK presents") && ok;
	}
	if (impl.queue12 && impl.fence12 && impl.fenceEvent) ok = WaitForQueue(impl) && ok;
	return ok;
}

bool FSR3FGPresenter::Impl::Shutdown() noexcept {
	std::scoped_lock lock(apiMutex);
	if (!DisableAndDrain(*this)) return false;
	if (overlayDrawActive && overlayDCompSurface) overlayDCompSurface->EndDraw();
	overlayDrawActive = false;
	if (fgContext && destroyContext && !Check(destroyContext(&fgContext, nullptr), "destroy FG context")) return false;
	fgContext = nullptr;
	backBuffers = {};
	frameLatencyWaitableObject.reset();
	if (swapchainContext && destroyContext && !Check(destroyContext(&swapchainContext, nullptr), "destroy swapchain context")) return false;
	swapchainContext = nullptr;
	swapChain = nullptr; // The SDK context and this COM pointer each own a reference.
	return true;
}

void FSR3FGPresenter::ImplDeleter::operator()(Impl* impl) const noexcept {
	if (!impl) return;
	if (impl->Shutdown()) { delete impl; return; }
	// No proof that the SDK/GPU has stopped referencing these objects. Retain
	// the complete owner (including DLL handles) for process lifetime instead
	// of releasing in-flight resources. Block future FG starts to bound this
	// exceptional retention to one failed session; normal shutdown releases all.
	restartRequired.store(true);
	Logger::Get().Error("FSR3FG teardown could not be verified; resources retained until process exit. Restart Magpie before using FSR3FG again.");
}

FSR3FGPresenter::Impl::~Impl() {
	// Release all provider-owned COM interfaces before unloading their code.
	if (loaderModule) FreeLibrary(loaderModule);
	if (providerModule) FreeLibrary(providerModule);
}

static bool CreateSharedColor(FSR3FGPresenter::Impl& impl) noexcept {
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = impl.width;
	desc.Height = impl.height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = COLOR_FORMAT;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

	HRESULT hr = impl.device11->CreateTexture2D(&desc, nullptr, impl.color11.put());
	if (SUCCEEDED(hr)) {
		hr = impl.device11->CreateRenderTargetView(
			impl.color11.get(), nullptr, impl.colorRtv11.put());
	}
	if (FAILED(hr)) {
		Logger::Get().ComError("Create FSR3FG D3D11 output texture failed", hr);
		return false;
	}

	winrt::com_ptr<IDXGIResource1> dxgiResource;
	hr = impl.color11->QueryInterface(IID_PPV_ARGS(dxgiResource.put()));
	if (FAILED(hr)) {
		Logger::Get().ComError("Query FSR3FG shared output resource failed", hr);
		return false;
	}
	HANDLE rawHandle = nullptr;
	hr = dxgiResource->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &rawHandle);
	if (FAILED(hr)) {
		Logger::Get().ComError("Create FSR3FG shared output handle failed", hr);
		return false;
	}
	wil::unique_handle handle(rawHandle);
	hr = impl.device12->OpenSharedHandle(handle.get(), IID_PPV_ARGS(impl.color12.put()));
	if (FAILED(hr)) {
		Logger::Get().ComError("Open FSR3FG shared output in D3D12 failed", hr);
		return false;
	}
	return true;
}

static bool CreateSharedMotion(FSR3FGPresenter::Impl& impl) noexcept {
	if (!impl.externalMotionEnabled) {
		return true;
	}
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = impl.width;
	desc.Height = impl.height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R16G16_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
	HRESULT hr = impl.device11->CreateTexture2D(&desc, nullptr, impl.motion11.put());
	if (SUCCEEDED(hr)) {
		hr = impl.device11->CreateUnorderedAccessView(
			impl.motion11.get(), nullptr, impl.motionUav11.put());
	}
	winrt::com_ptr<IDXGIResource1> dxgiResource;
	if (SUCCEEDED(hr)) {
		hr = impl.motion11->QueryInterface(IID_PPV_ARGS(dxgiResource.put()));
	}
	HANDLE rawHandle = nullptr;
	if (SUCCEEDED(hr)) {
		hr = dxgiResource->CreateSharedHandle(
			nullptr, GENERIC_ALL, nullptr, &rawHandle);
	}
	wil::unique_handle handle(rawHandle);
	if (SUCCEEDED(hr)) {
		hr = impl.device12->OpenSharedHandle(
			handle.get(), IID_PPV_ARGS(impl.motion12.put()));
	}
	if (FAILED(hr)) {
		Logger::Get().ComError("Create FSR3FG D3D11/D3D12 motion interop failed", hr);
		return false;
	}
	static constexpr float ZERO[4]{};
	impl.context11->ClearUnorderedAccessViewFloat(impl.motionUav11.get(), ZERO);
	return true;
}

static bool CreateFlatResource(
	FSR3FGPresenter::Impl& impl,
	DXGI_FORMAT format,
	winrt::com_ptr<ID3D12Resource>& resource,
	D3D12_CPU_DESCRIPTOR_HANDLE cpu,
	D3D12_GPU_DESCRIPTOR_HANDLE gpu,
	const float clearValue[4],
	D3D12_RESOURCE_BARRIER& barrier
) noexcept {
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = impl.width;
	desc.Height = impl.height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	HRESULT hr = impl.device12->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
		IID_PPV_ARGS(resource.put()));
	if (FAILED(hr)) {
		Logger::Get().ComError("Create FSR3FG virtual input failed", hr);
		return false;
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
	uav.Format = format;
	uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	impl.device12->CreateUnorderedAccessView(resource.get(), nullptr, &uav, cpu);
	const UINT stride = impl.device12->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE shaderCpu = impl.clearHeap12->GetCPUDescriptorHandleForHeapStart();
	shaderCpu.ptr += ((gpu.ptr - impl.clearHeap12->GetGPUDescriptorHandleForHeapStart().ptr) / stride) * stride;
	impl.device12->CopyDescriptorsSimple(1, shaderCpu, cpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	impl.commandList12->ClearUnorderedAccessViewFloat(
		gpu, cpu, resource.get(), clearValue, 0, nullptr);

	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition = {
		resource.get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
	};
	return true;
}

static bool CreateSizeDependentResources(FSR3FGPresenter::Impl& impl) noexcept {
	if (!CreateSharedColor(impl) || !CreateSharedMotion(impl)) {
		return false;
	}

	for (uint32_t i = 0; i < BUFFER_COUNT; ++i) {
		HRESULT hr = impl.swapChain->GetBuffer(i, IID_PPV_ARGS(impl.backBuffers[i].put()));
		if (FAILED(hr)) {
			Logger::Get().ComError("Get FSR3FG back buffer failed", hr);
			return false;
		}
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HRESULT hr = impl.device12->CreateDescriptorHeap(
		&heapDesc, IID_PPV_ARGS(impl.clearHeap12.put()));
	if (FAILED(hr)) {
		return false;
	}

	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = impl.device12->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(impl.clearCpuHeap12.put()));
	if (FAILED(hr)) return false;

	if (!WaitForQueue(impl)) {
		return false;
	}
	hr = impl.allocators12[0]->Reset();
	if (SUCCEEDED(hr)) {
		hr = impl.commandList12->Reset(impl.allocators12[0].get(), nullptr);
	}
	if (FAILED(hr)) {
		return false;
	}
	ID3D12DescriptorHeap* heaps[]{ impl.clearHeap12.get() };
	impl.commandList12->SetDescriptorHeaps(1, heaps);
	const UINT stride = impl.device12->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE cpu =
		impl.clearCpuHeap12->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpu =
		impl.clearHeap12->GetGPUDescriptorHandleForHeapStart();
	D3D12_RESOURCE_BARRIER barriers[2]{};
	static constexpr float ZERO[4]{};
	static constexpr float FLAT_DEPTH[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
	if (!CreateFlatResource(impl, DXGI_FORMAT_R16G16_FLOAT,
		impl.zeroMotion12, cpu, gpu, ZERO, barriers[0])) {
		return false;
	}
	cpu.ptr += stride;
	gpu.ptr += stride;
	if (!CreateFlatResource(impl, DXGI_FORMAT_R32_FLOAT,
		impl.flatDepth12, cpu, gpu, FLAT_DEPTH, barriers[1])) {
		return false;
	}
	impl.commandList12->ResourceBarrier(2, barriers);
	hr = impl.commandList12->Close();
	if (FAILED(hr)) {
		return false;
	}
	ID3D12CommandList* lists[]{ impl.commandList12.get() };
	impl.queue12->ExecuteCommandLists(1, lists);
	return WaitForQueue(impl);
}

static void ReleaseSizeDependentResources(FSR3FGPresenter::Impl& impl) noexcept {
	impl.backBuffers = {};
	impl.colorRtv11 = nullptr;
	impl.color11 = nullptr;
	impl.color12 = nullptr;
	impl.motionUav11 = nullptr;
	impl.motion11 = nullptr;
	impl.motion12 = nullptr;
	impl.zeroMotion12 = nullptr;
	impl.flatDepth12 = nullptr;
	impl.clearHeap12 = nullptr;
	impl.clearCpuHeap12 = nullptr;
}

bool FSR3FGPresenter::_ResizeOverlaySurface() noexcept {
	if (!_impl) {
		return false;
	}
	Impl& impl = *_impl;
	HRESULT hr = S_OK;
	if (!impl.overlayDCompDevice) {
		hr = DCompositionCreateDevice3(
			impl.device11, IID_PPV_ARGS(impl.overlayDCompDevice.put()));
		if (SUCCEEDED(hr)) {
			hr = impl.overlayDCompDevice->CreateTargetForHwnd(
				impl.hwnd, TRUE, impl.overlayDCompTarget.put());
		}
		if (SUCCEEDED(hr)) {
			hr = impl.overlayDCompDevice->CreateVisual(impl.overlayDCompVisual.put());
		}
		if (SUCCEEDED(hr)) {
			hr = impl.overlayDCompTarget->SetRoot(impl.overlayDCompVisual.get());
		}
		if (FAILED(hr)) {
			Logger::Get().ComError("Create FSR3FG independent UI visual failed", hr);
			return false;
		}
	}

	if (impl.overlayDCompSurface) {
		hr = impl.overlayDCompSurface->Resize(impl.width, impl.height);
	} else {
		hr = impl.overlayDCompDevice->CreateVirtualSurface(
			impl.width, impl.height, COLOR_FORMAT,
			DXGI_ALPHA_MODE_PREMULTIPLIED, impl.overlayDCompSurface.put());
		if (SUCCEEDED(hr)) {
			hr = impl.overlayDCompVisual->SetContent(impl.overlayDCompSurface.get());
		}
	}
	if (SUCCEEDED(hr)) {
		hr = impl.overlayDCompDevice->Commit();
	}
	if (FAILED(hr)) {
		Logger::Get().ComError("Resize FSR3FG independent UI surface failed", hr);
		return false;
	}
	return true;
}



static bool QueryVersion(FSR3FGPresenter::Impl& impl, bool swapchain, uint64_t& selected) noexcept {
	ffxQueryDescGetVersions desc{};
	desc.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
	desc.createDescType = swapchain ? FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_FOR_HWND_DX12 :
		FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;
	desc.device = impl.device12.get();
	uint64_t count = 0;
	desc.outputCount = &count;
	if (!impl.Check(impl.query(nullptr, &desc.header), "query provider count") || !count || count > 128)
		return impl.Fail("required AMD provider unavailable");
	std::vector<uint64_t> ids(count);
	std::vector<const char*> names(count);
	desc.versionIds = ids.data();
	desc.versionNames = names.data();
	if (!impl.Check(impl.query(nullptr, &desc.header), "query provider versions") || count > ids.size()) return false;
	bool found = false;
	for (size_t i = 0; i < count; ++i) {
		const std::string_view name = names[i] ? names[i] : "";
		Logger::Get().Info(fmt::format("FSR3FG {} provider candidate: {} id={:#x}",
			swapchain ? "swapchain" : "FG", name, ids[i]));
		if (IsFSR3FGProvider(name, swapchain)) { selected = ids[i]; found = true; }
	}
	return found || impl.Fail(swapchain ? "swapchain 3.1.7 provider missing" : "FG 3.1.6 provider missing (ML is not a substitute)");
}

static bool LoadRuntime(FSR3FGPresenter::Impl& impl) noexcept {
	// Pin the packaged DLLs to the application directory. A game working
	// directory must not silently supply a different FidelityFX runtime.
	std::wstring exe(32768, L'\0');
	const DWORD length = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
	if (!length || length >= exe.size()) return impl.Fail("resolve runtime directory");
	exe.resize(length);
	exe.resize(exe.find_last_of(L"\\/") + 1);
	auto load = [&](const wchar_t* name) {
		return LoadLibraryExW((exe + name).c_str(), nullptr,
			LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	};
	impl.providerModule = load(L"amd_fidelityfx_framegeneration_dx12.dll");
	if (!impl.providerModule) return impl.Fail("load packaged AMD FG DLL", HRESULT_FROM_WIN32(GetLastError()));
	impl.loaderModule = load(L"amd_fidelityfx_loader_dx12.dll");
	if (!impl.loaderModule) return impl.Fail("load packaged AMD loader DLL", HRESULT_FROM_WIN32(GetLastError()));
#define LOAD_FFX(member, symbol) impl.member = reinterpret_cast<decltype(impl.member)>(GetProcAddress(impl.loaderModule, symbol))
	LOAD_FFX(createContext, "ffxCreateContext");
	LOAD_FFX(destroyContext, "ffxDestroyContext");
	LOAD_FFX(configure, "ffxConfigure");
	LOAD_FFX(query, "ffxQuery");
	LOAD_FFX(dispatch, "ffxDispatch");
#undef LOAD_FFX
	if (!impl.createContext || !impl.destroyContext || !impl.configure || !impl.query || !impl.dispatch)
		return impl.Fail("AMD loader exports incomplete");
	return QueryVersion(impl, false, impl.fgVersionId) && QueryVersion(impl, true, impl.swapchainVersionId);
}

static bool CreateFGContext(FSR3FGPresenter::Impl& impl) noexcept {
	ffxCreateContextDescFrameGeneration create{};
	create.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;
	create.displaySize = { impl.width, impl.height };
	create.maxRenderSize = create.displaySize;
	create.backBufferFormat = ffxApiGetSurfaceFormatDX12(COLOR_FORMAT);
	// Forward finite depth (1 == far plane), no jitter, no async compute, SDR.
	create.flags = FFX_FRAMEGENERATION_ENABLE_DEBUG_CHECKING;
	ffxCreateBackendDX12Desc backend{};
	backend.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
	backend.device = impl.device12.get();
	ffxCreateContextDescFrameGenerationVersion version{};
	version.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION_VERSION;
	version.version = FFX_FRAMEGENERATION_VERSION;
	ffxOverrideVersion provider{};
	provider.header.type = FFX_API_DESC_TYPE_OVERRIDE_VERSION;
	provider.versionId = impl.fgVersionId;
	create.header.pNext = &backend.header;
	backend.header.pNext = &version.header;
	version.header.pNext = &provider.header;
	return impl.Check(impl.createContext(&impl.fgContext, &create.header, nullptr), "create pinned FG 3.1.6");
}

static bool CreateMotionShader(FSR3FGPresenter::Impl& impl) noexcept {
	if (!impl.externalMotionEnabled) return true;
	winrt::com_ptr<ID3DBlob> blob;
	if (!DirectXHelper::CompileComputeShader(FSR3FG_MOTION_SHADER, "Main", blob.put(), "FSR3FGMotion")) return false;
	HRESULT hr = impl.device11->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, impl.motionShader11.put());
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = 16;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (SUCCEEDED(hr)) hr = impl.device11->CreateBuffer(&desc, nullptr, impl.motionConstants11.put());
	return SUCCEEDED(hr) || impl.Fail("create motion remap shader", hr);
}

static ffxReturnCode_t GenerateFrame(ffxDispatchDescFrameGeneration* desc, void* user) {
	auto& impl = *static_cast<FSR3FGPresenter::Impl*>(user);
	// SDK 3.1.7 calls this inline during Present, under apiMutex. Never add a
	// second lock here, or a UI callback that touches the frontend D3D11 device.
	if (!desc || desc->frameID != impl.decision.sdkFrameId || desc->numGeneratedFrames != 1) {
		if (desc) desc->numGeneratedFrames = 0;
		return impl.callbackResult = FFX_API_RETURN_ERROR_PARAMETER;
	}
	desc->reset = desc->reset || impl.decision.reset != 0;
	const bool reset = desc->reset;
	impl.callbackResult = impl.dispatch(&impl.fgContext, &desc->header);
	++impl.callbackDispatches;
	if (impl.callbackResult == FFX_API_RETURN_OK) {
		// FSR primes history with a pass-through on reset. It is not a valid
		// interpolated frame and must never inflate our interpolation count.
		impl.callbackGeneratedCount = reset ? 0u : desc->numGeneratedFrames;
		if (reset) ++impl.resetPassThroughs;
		else ++impl.interpolations;
	} else desc->numGeneratedFrames = 0;
	return impl.callbackResult;
}

FSR3FGPresenter::FSR3FGPresenter() = default;
FSR3FGPresenter::~FSR3FGPresenter() noexcept = default;

bool FSR3FGPresenter::_Initialize(HWND hwndAttach) noexcept {
	if (restartRequired.load()) {
		Logger::Get().Error("FSR3FG requires a full Magpie restart after incomplete SDK teardown");
		return false;
	}
	if (ScalingWindow::Get().Options().IsHdrCompatibilityEnabled()) {
		Logger::Get().Error("FSR3FG currently supports SDR only; HDR acceptance is pending");
		return false;
	}
	std::unique_ptr<Impl, ImplDeleter> impl(new Impl());
	impl->hwnd = hwndAttach;
	impl->device11 = _deviceResources->GetD3DDevice();
	impl->context11 = _deviceResources->GetD3DDC();
	impl->factory = _deviceResources->GetDXGIFactory();
	const SIZE size = Win32Helper::GetSizeOfRect(ScalingWindow::Get().RendererRect());
	if (size.cx <= 0 || size.cy <= 0) return false;
	impl->width = static_cast<uint32_t>(size.cx);
	impl->height = static_cast<uint32_t>(size.cy);
	impl->generationRect = { 0, 0, size.cx, size.cy };
	impl->externalMotionEnabled = true;
	HRESULT hr = D3D12CreateDevice(_deviceResources->GetGraphicsAdapter(), D3D_FEATURE_LEVEL_12_0,
		IID_PPV_ARGS(impl->device12.put()));
	if (FAILED(hr)) return impl->Fail("create D3D12 device on selected adapter", hr);
	D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{ D3D_SHADER_MODEL_6_2 };
	D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
	if (FAILED(impl->device12->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))) ||
		shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_2 ||
		FAILED(impl->device12->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options))) ||
		!options.TypedUAVLoadAdditionalFormats) return impl->Fail("requires SM 6.2 and typed UAV loads");
	for (DXGI_FORMAT format : { DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R32_FLOAT }) {
		D3D12_FEATURE_DATA_FORMAT_SUPPORT support{ format };
		if (FAILED(impl->device12->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support))) ||
			!(support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) ||
			!(support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE)) return impl->Fail("required UAV format unsupported");
	}
	// D3D11 and D3D12 are explicitly created on the same selected adapter.
	const LUID luid = impl->device12->GetAdapterLuid();
	DXGI_ADAPTER_DESC1 adapter{};
	if (FAILED(_deviceResources->GetGraphicsAdapter()->GetDesc1(&adapter)) ||
		luid.LowPart != adapter.AdapterLuid.LowPart || luid.HighPart != adapter.AdapterLuid.HighPart)
		return impl->Fail("D3D11/D3D12 adapter LUID mismatch");
	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hr = impl->device12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(impl->queue12.put()));
	for (uint32_t i = 0; SUCCEEDED(hr) && i < BUFFER_COUNT; ++i)
		hr = impl->device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(impl->allocators12[i].put()));
	if (SUCCEEDED(hr)) hr = impl->device12->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		impl->allocators12[0].get(), nullptr, IID_PPV_ARGS(impl->commandList12.put()));
	if (SUCCEEDED(hr)) hr = impl->commandList12->Close();
	if (FAILED(hr)) return impl->Fail("create command objects", hr);
	hr = impl->device11->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(impl->fence11.put()));
	HANDLE rawFence = nullptr;
	if (SUCCEEDED(hr)) hr = impl->fence11->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &rawFence);
	wil::unique_handle fenceHandle(rawFence);
	if (SUCCEEDED(hr)) hr = impl->device12->OpenSharedHandle(fenceHandle.get(), IID_PPV_ARGS(impl->fence12.put()));
	if (FAILED(hr)) return impl->Fail("create shared fence", hr);
	if (!impl->fenceEvent.try_create(wil::EventOptions::None, nullptr)) return impl->Fail("create fence event");
	if (!LoadRuntime(*impl)) return false;
	DXGI_SWAP_CHAIN_DESC1 swapDesc{};
	_VRRPathAvailable(true);
	swapDesc.Width = impl->width; swapDesc.Height = impl->height;
	swapDesc.Format = COLOR_FORMAT;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.BufferCount = BUFFER_COUNT;
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | _vrr.CreationFlags();
	ffxCreateContextDescFrameGenerationSwapChainForHwndDX12 create{};
	create.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_FOR_HWND_DX12;
	create.swapchain = impl->swapChain.put();
	create.hwnd = hwndAttach; create.desc = &swapDesc;
	create.dxgiFactory = impl->factory; create.gameQueue = impl->queue12.get();
	ffxCreateContextDescFrameGenerationSwapChainVersionDX12 version{};
	version.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_VERSION_DX12;
	version.version = FFX_FRAMEGENERATION_SWAPCHAIN_DX12_VERSION;
	ffxOverrideVersion provider{};
	provider.header.type = FFX_API_DESC_TYPE_OVERRIDE_VERSION;
	provider.versionId = impl->swapchainVersionId;
	create.header.pNext = &version.header; version.header.pNext = &provider.header;
	if (!impl->Check(impl->createContext(&impl->swapchainContext, &create.header, nullptr), "create pinned swapchain 3.1.7") ||
		!impl->swapChain) return false;
	DXGI_SWAP_CHAIN_DESC1 actual{};
	hr = impl->swapChain->GetDesc1(&actual);
	if (FAILED(hr)) return impl->Fail("query actual swapchain", hr);
	_vrr.Created(actual.Flags);
	_BindVRRSwapChain(impl->swapChain.get());
	hr = impl->swapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
	if (SUCCEEDED(hr)) hr = impl->swapChain->SetMaximumFrameLatency(1);
	if (FAILED(hr)) return impl->Fail("configure SDR swapchain", hr);
	impl->frameLatencyWaitableObject.reset(impl->swapChain->GetFrameLatencyWaitableObject());
	if (!impl->frameLatencyWaitableObject) return impl->Fail("get frame latency event");
	const HRESULT association = impl->factory->MakeWindowAssociation(hwndAttach, DXGI_MWA_NO_ALT_ENTER);
	_vrr.AltEnterDisabled(SUCCEEDED(association));
	if (!CreateSizeDependentResources(*impl) || !CreateMotionShader(*impl) || !CreateFGContext(*impl)) return false;
	Logger::Get().Info(fmt::format("FSR3FG initialized: {}x{} SDR/RGBA8, fixed 2x, FG=3.1.6 id={:#x}, swapchain=3.1.7 id={:#x}, adapter={:x}:{:x} LUID={:x}:{:x}, externalMotion={}, forward flat depth=1, virtual camera, asyncCompute=false; image-only experimental inputs",
		impl->width, impl->height, impl->fgVersionId, impl->swapchainVersionId,
		adapter.VendorId, adapter.DeviceId, uint32_t(luid.HighPart), luid.LowPart, true));
	_impl = std::move(impl);
	return _ResizeOverlaySurface();
}

void FSR3FGPresenter::SetSourceTiming(uint64_t frameId, uint64_t sequence, uint64_t generation, int64_t timestamp100ns) noexcept {
	if (_impl) _impl->sourceSample = { frameId, sequence, generation, timestamp100ns };
}

void FSR3FGPresenter::SetCaptureAdmissionTime(int64_t timestamp100ns) noexcept {
	if (_impl) _impl->sourceSample.captureAdmissionTime100ns = timestamp100ns;
}

void FSR3FGPresenter::SetOutputHistoryRevision(uint64_t revision) noexcept {
	if (_impl) _impl->sourceSample.outputHistoryRevision = revision;
}
void FSR3FGPresenter::SetFrameGuidanceAvailability(FrameGuidanceAvailability availability) noexcept {
	if (_impl) _impl->motionAvailability = availability;
}

void FSR3FGPresenter::SetFrameGuidance(ID3D11Texture2D* motion, FrameGuidanceFrameId frameId,
	bool requiresHistoryReset, const RECT& rect) noexcept {
	if (!_impl || _impl->fatal) return;
	auto& impl = *_impl;
	if (rect.left < 0 || rect.top < 0 || rect.right > LONG(impl.width) || rect.bottom > LONG(impl.height) ||
		rect.right <= rect.left || rect.bottom <= rect.top) {
		impl.Fail("generationRect outside output; cropped output is not supported"); return;
	}
	impl.geometryReset |= !EqualRect(&rect, &impl.generationRect);
	impl.generationRect = rect;
	impl.externalMotionValid = false;
	impl.externalMotionReset = impl.externalMotionEnabled;
	// Provider failure is not permission to reset indefinitely. This status
	// travels in the same publication transaction as the color/motion pair.
	if (!impl.motionAdmission.Accept(impl.motionAvailability, impl.sourceSample)) {
		impl.Fail(fmt::format("required optical flow unavailable: status={} source={}",
			FrameGuidanceAvailabilityName(impl.motionAvailability), impl.sourceSample.frameId)); return;
	}
	// The provider is mandatory; only bootstrap/reset may have no valid motion.
	const HRESULT hr = impl.sourceMotion.Prepare(impl.device11, motion, frameId, impl.sourceSample.frameId);
	if (hr == S_FALSE) {
		++impl.invalidMotionSamples;
		if (impl.motionAvailability != FrameGuidanceAvailability::ResetOnly)
			impl.Fail("FSR3 FG requires valid AMDOF/NVOF motion for new interpolation input");
		return;
	}
	if (FAILED(hr)) {
		const auto& d = impl.sourceMotion.Desc();
		impl.Fail(fmt::format("validate/create source motion SRV: frame={} source={} session={} generation={} "
			"size={}x{} format={} mip={} array={} sample={}/{} bind={:#x} usage={} cpu={:#x} misc={:#x} sameDevice={}",
			frameId, impl.sourceSample.frameId, impl.sourceSample.sequence, impl.sourceSample.generation,
			d.Width, d.Height, uint32_t(d.Format), d.MipLevels, d.ArraySize, d.SampleDesc.Count,
			d.SampleDesc.Quality, d.BindFlags, uint32_t(d.Usage), d.CPUAccessFlags, d.MiscFlags,
			impl.sourceMotion.SameDevice()), hr);
		return;
	}
	RemapFSR3FGMotion(impl.context11, impl.motionShader11.get(), impl.motionConstants11.get(),
		impl.sourceMotion, impl.motionUav11.get(), uint32_t(rect.right - rect.left), uint32_t(rect.bottom - rect.top));
	impl.externalMotionValid = true;
	impl.externalMotionReset = requiresHistoryReset;
}

bool FSR3FGPresenter::BeginFrame(winrt::com_ptr<ID3D11Texture2D>& tex,
	winrt::com_ptr<ID3D11RenderTargetView>& rtv, POINT& offset) noexcept {
	return _BeginFrame(tex, rtv, offset, false);
}

bool FSR3FGPresenter::BeginIdleFrame(winrt::com_ptr<ID3D11Texture2D>& tex,
	winrt::com_ptr<ID3D11RenderTargetView>& rtv, POINT& offset) noexcept {
	return _BeginFrame(tex, rtv, offset, true);
}

bool FSR3FGPresenter::_BeginFrame(winrt::com_ptr<ID3D11Texture2D>& tex,
	winrt::com_ptr<ID3D11RenderTargetView>& rtv, POINT& offset, bool idle) noexcept {
	if (!_impl || _impl->fatal) return false;
	auto& impl = *_impl;
	impl.idlePresentation = idle;
	impl.decision = idle ? impl.history.Replay() :
		impl.history.Next(impl.sourceSample, impl.externalMotionReset, impl.geometryReset);
	if (!impl.decision.submit) return false;
	const DWORD result = impl.frameLatencyGate.TryAcquire(impl.frameLatencyWaitableObject.get());
	impl.capacityBusy = result == WAIT_TIMEOUT;
	if (impl.capacityBusy) {
		const auto now = std::chrono::steady_clock::now();
		if (impl.capacityWaitStarted == std::chrono::steady_clock::time_point{}) impl.capacityWaitStarted = now;
		if (now - impl.capacityWaitStarted >= std::chrono::seconds(3)) return impl.Fail("frame capacity stalled for 3 seconds");
		return false;
	}
	impl.capacityWaitStarted = {};
	if (result != WAIT_OBJECT_0) return impl.Fail("frame capacity wait failed");
	tex = impl.color11; rtv = impl.colorRtv11; offset = {};
	return true;
}
bool FSR3FGPresenter::WaitForFrameCapacity(DWORD timeout) noexcept {
	if (!_impl || _impl->fatal) return false;
	DWORD result = WAIT_TIMEOUT;
	if (!_impl->frameLatencyGate.Wait(_impl->frameLatencyWaitableObject.get(), timeout, result)) return false;
	if (result == WAIT_FAILED) return _impl->Fail("capacity event wait failed");
	return true;
}
bool FSR3FGPresenter::WasFrameCapacityBusy() const noexcept { return _impl && _impl->capacityBusy; }
bool FSR3FGPresenter::HasFatalError() const noexcept { return _impl && _impl->fatal; }
const PresenterFailure* FSR3FGPresenter::GetFailure() const noexcept { return _impl ? &_impl->failure : nullptr; }

bool FSR3FGPresenter::EndFrame(bool waitForGpu) noexcept {
	_lastSubmittedFrameCount.reset();
	if (!_impl || _impl->fatal || !_impl->decision.submit) return false;
	auto& impl = *_impl;
	std::scoped_lock lock(impl.apiMutex);
	impl.frameLatencyGate.Reset();
	impl.capacityBusy = false;
	ffxConfigureDescFrameGeneration config{};
	config.header.type = FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;
	config.swapChain = impl.swapChain.get();
	config.frameGenerationEnabled = !impl.idlePresentation;
	// Keep callbacks stable. The pinned proxy drains on enable/disable changes,
	// not on each replay; source and SDK presentation IDs remain independent.
	config.frameGenerationCallback = GenerateFrame;
	config.frameGenerationCallbackUserContext = &impl;
	config.frameID = impl.decision.sdkFrameId;
	config.generationRect = { impl.generationRect.left, impl.generationRect.top,
		impl.generationRect.right - impl.generationRect.left, impl.generationRect.bottom - impl.generationRect.top };
	// Game HUD is baked in; do not mislabel the captured image as HUDLessColor.
	if (!impl.Check(impl.configure(&impl.fgContext, &config.header), "configure base frame")) return false;
	const uint64_t ready = ++impl.fenceValue;
	HRESULT hr = impl.context11->Signal(impl.fence11.get(), ready);
	impl.context11->Flush();
	if (SUCCEEDED(hr)) hr = impl.queue12->Wait(impl.fence12.get(), ready);
	if (FAILED(hr)) return impl.Fail("D3D11 to D3D12 fence handoff", hr);
	const uint32_t index = impl.swapChain->GetCurrentBackBufferIndex();
	if (index >= BUFFER_COUNT || !WaitForFence(impl, impl.allocatorFenceValues[index])) return impl.Fail("allocator reuse wait");
	hr = impl.allocators12[index]->Reset();
	if (SUCCEEDED(hr)) hr = impl.commandList12->Reset(impl.allocators12[index].get(), nullptr);
	if (FAILED(hr)) return impl.Fail("reset command list", hr);
	D3D12_RESOURCE_BARRIER barriers[2]{};
	barriers[0].Type = barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Transition = { impl.color12.get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE };
	barriers[1].Transition = { impl.backBuffers[index].get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST };
	impl.commandList12->ResourceBarrier(2, barriers);
	impl.commandList12->CopyResource(impl.backBuffers[index].get(), impl.color12.get());
	for (auto& barrier : barriers) std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
	impl.commandList12->ResourceBarrier(2, barriers);
	if (!impl.idlePresentation) {
		D3D12_RESOURCE_BARRIER motionBarrier{};
		motionBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		motionBarrier.Transition = { impl.motion12.get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
		if (impl.externalMotionValid) impl.commandList12->ResourceBarrier(1, &motionBarrier);
		ffxDispatchDescFrameGenerationPrepareV2 prepare{};
		prepare.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_V2;
		prepare.frameID = config.frameID;
		prepare.commandList = impl.commandList12.get();
		prepare.renderSize = { uint32_t(config.generationRect.width), uint32_t(config.generationRect.height) };
		prepare.motionVectorScale = { 1.0f, 1.0f };
		prepare.frameTimeDelta = impl.decision.deltaMs;
		prepare.reset = impl.decision.reset != 0;
		prepare.cameraNear = 0.1f; prepare.cameraFar = 1000.0f;
		prepare.cameraFovAngleVertical = 1.0471975512f;
		prepare.viewSpaceToMetersFactor = 1.0f;
		prepare.cameraUp[1] = 1.0f; prepare.cameraRight[0] = 1.0f; prepare.cameraForward[2] = 1.0f;
		prepare.depth = ffxApiGetResourceDX12(impl.flatDepth12.get());
		prepare.motionVectors = ffxApiGetResourceDX12(impl.externalMotionValid ? impl.motion12.get() : impl.zeroMotion12.get());
		if (!impl.Check(impl.dispatch(&impl.fgContext, &prepare.header), "PrepareV2")) return false;
		if (impl.externalMotionValid) {
			std::swap(motionBarrier.Transition.StateBefore, motionBarrier.Transition.StateAfter);
			impl.commandList12->ResourceBarrier(1, &motionBarrier);
		}
	}
	hr = impl.commandList12->Close();
	if (FAILED(hr)) return impl.Fail("close prepared command list", hr);
	ID3D12CommandList* lists[]{ impl.commandList12.get() };
	impl.queue12->ExecuteCommandLists(1, lists);
	impl.callbackResult = FFX_API_RETURN_OK;
	impl.callbackGeneratedCount.reset();
	const auto start = std::chrono::steady_clock::now();
	hr = _PresentWithVRR(impl.swapChain.get(), false);
	const double presentMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	// Shared D3D11 resources may be overwritten only after Prepare/FG on the
	// game queue. Async compute is disabled; SDK-owned color buffers persist
	// for the SDK's asynchronous present worker independently.
	const uint64_t done = ++impl.fenceValue;
	HRESULT sync = impl.queue12->Signal(impl.fence12.get(), done);
	if (SUCCEEDED(sync)) sync = impl.context11->Wait(impl.fence11.get(), done);
	if (FAILED(sync)) return impl.Fail("D3D12 to D3D11 fence handoff", sync);
	impl.allocatorFenceValues[index] = done;
	if (waitForGpu && !WaitForFence(impl, done)) return false;
	if (FAILED(hr)) return impl.Fail("proxy Present", hr);
	if (!impl.Check(impl.callbackResult, "frame-generation callback")) return false;
	// DXGI/dispatch counts cannot establish how many frames reached a display.
	_lastPresentedFrameCount = std::nullopt;
	_lastSubmissionTime = std::chrono::steady_clock::now();
	if (impl.idlePresentation) {
		impl.history.ReplaySubmitted();
		impl.geometryReset = true; // Re-enable starts with a real-input reset.
		impl.decision.submit = false;
		return true;
	}
	// Submission count, not scanout or proof of novel pixels: GPU scene-cut
	// detection may still replace an accepted interpolation with a copy.
	if (impl.callbackGeneratedCount) _lastSubmittedFrameCount = 1u + *impl.callbackGeneratedCount;
	else if (impl.baseSubmissions % 120 == 0)
		Logger::Get().Warn("FSR3FG Present accepted without a generation callback; content count unknown");
	++impl.baseSubmissions;
	if (impl.decision.reset || impl.baseSubmissions % 120 == 0) Logger::Get().Info(fmt::format(
		"FSR3FG source={} session={} generation={} sdk={} resetMask={} inputMs={:.3f} timingSource={} content={}x{}+{},{} baseSubmits={} callbackDispatches={} interpolationDispatches={} resetPassThroughs={} invalidMotionSamples={} PresentMs={:.3f}; game render time and display events unmeasured",
		impl.sourceSample.frameId, impl.sourceSample.sequence, impl.sourceSample.generation, config.frameID,
		impl.decision.reset, impl.decision.deltaMs, FSR3FGTimingSourceName(impl.decision.timingSource), config.generationRect.width, config.generationRect.height,
		config.generationRect.left, config.generationRect.top, impl.baseSubmissions, impl.callbackDispatches,
		impl.interpolations, impl.resetPassThroughs, impl.invalidMotionSamples, presentMs));
	impl.history.Submitted(impl.sourceSample);
	impl.geometryReset = false;
	impl.decision.submit = false;
	return true;
}

bool FSR3FGPresenter::OnResize() noexcept {
	if (!_impl || _impl->fatal) return false;
	auto& impl = *_impl;
	std::scoped_lock lock(impl.apiMutex);
	const SIZE size = Win32Helper::GetSizeOfRect(ScalingWindow::Get().RendererRect());
	if (size.cx <= 0 || size.cy <= 0) return false;
	if (uint32_t(size.cx) == impl.width && uint32_t(size.cy) == impl.height) return true;
	if (!DisableAndDrain(impl)) return false;
	if (!impl.Check(impl.destroyContext(&impl.fgContext, nullptr), "destroy FG before resize")) return false;
	impl.fgContext = nullptr;
	ReleaseSizeDependentResources(impl);
	impl.sourceMotion = {};
	impl.frameLatencyGate.Reset(); impl.frameLatencyWaitableObject.reset();
	_RefreshVRREnvironment(true);
	HRESULT hr = impl.swapChain->ResizeBuffers(BUFFER_COUNT, size.cx, size.cy, COLOR_FORMAT, _vrr.ResizeFlags());
	if (SUCCEEDED(hr)) hr = impl.swapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
	if (FAILED(hr)) return impl.Fail("resize proxy buffers", hr);
	impl.width = uint32_t(size.cx); impl.height = uint32_t(size.cy);
	impl.generationRect = { 0, 0, size.cx, size.cy };
	impl.history.Reset(); impl.geometryReset = true; impl.externalMotionValid = false;
	impl.motionAdmission = {}; _lastSubmittedFrameCount.reset();
	impl.allocatorFenceValues = {};
	impl.frameLatencyWaitableObject.reset(impl.swapChain->GetFrameLatencyWaitableObject());
	if (!impl.frameLatencyWaitableObject) return impl.Fail("resize capacity event");
	if (!CreateSizeDependentResources(impl) || !CreateFGContext(impl) || !_ResizeOverlaySurface())
		return impl.Fail("recreate resources after resize");
	return true;
}

bool FSR3FGPresenter::HasIndependentOverlay() const noexcept {
	return _impl && _impl->overlayDCompSurface;
}

bool FSR3FGPresenter::BeginOverlayFrame(
	winrt::com_ptr<ID3D11Texture2D>& frameTex,
	winrt::com_ptr<ID3D11RenderTargetView>& frameRtv,
	POINT& drawOffset
) noexcept {
	if (!_impl || !_impl->overlayDCompSurface || _impl->overlayDrawActive) {
		return false;
	}
	Impl& impl = *_impl;
	HRESULT hr = impl.overlayDCompSurface->BeginDraw(
		nullptr, IID_PPV_ARGS(&frameTex), &drawOffset);
	if (FAILED(hr)) {
		Logger::Get().ComError("Begin FSR3FG independent UI draw failed", hr);
		return false;
	}
	impl.overlayDrawActive = true;
	hr = impl.device11->CreateRenderTargetView(
		frameTex.get(), nullptr, frameRtv.put());
	if (FAILED(hr)) {
		impl.overlayDCompSurface->EndDraw();
		impl.overlayDrawActive = false;
		Logger::Get().ComError("Create FSR3FG independent UI RTV failed", hr);
		return false;
	}
	return true;
}

bool FSR3FGPresenter::EndOverlayFrame() noexcept {
	if (!_impl || !_impl->overlayDrawActive) {
		return false;
	}
	Impl& impl = *_impl;
	impl.overlayDrawActive = false;
	HRESULT hr = impl.overlayDCompSurface->EndDraw();
	if (SUCCEEDED(hr)) {
		hr = impl.overlayDCompDevice->Commit();
	}
	if (FAILED(hr)) {
		Logger::Get().ComError("Commit FSR3FG independent UI frame failed", hr);
		return false;
	}
	return true;
}


}
#else
namespace Magpie {
struct FSR3FGPresenter::Impl {};
void FSR3FGPresenter::ImplDeleter::operator()(Impl* impl) const noexcept { delete impl; }
FSR3FGPresenter::FSR3FGPresenter() = default;
FSR3FGPresenter::~FSR3FGPresenter() noexcept = default;
bool FSR3FGPresenter::_Initialize(HWND) noexcept {
	Logger::Get().Error("FSR3FG is unavailable in this build (EnableFSR3FrameGeneration=false or unsupported architecture)");
	return false;
}
bool FSR3FGPresenter::BeginFrame(winrt::com_ptr<ID3D11Texture2D>&, winrt::com_ptr<ID3D11RenderTargetView>&, POINT&) noexcept { return false; }
bool FSR3FGPresenter::BeginIdleFrame(winrt::com_ptr<ID3D11Texture2D>&, winrt::com_ptr<ID3D11RenderTargetView>&, POINT&) noexcept { return false; }
bool FSR3FGPresenter::_BeginFrame(winrt::com_ptr<ID3D11Texture2D>&, winrt::com_ptr<ID3D11RenderTargetView>&, POINT&, bool) noexcept { return false; }
bool FSR3FGPresenter::EndFrame(bool) noexcept { return false; }
bool FSR3FGPresenter::WaitForFrameCapacity(DWORD) noexcept { return false; }
bool FSR3FGPresenter::WasFrameCapacityBusy() const noexcept { return false; }
bool FSR3FGPresenter::HasFatalError() const noexcept { return false; }
const PresenterFailure* FSR3FGPresenter::GetFailure() const noexcept { return nullptr; }
void FSR3FGPresenter::SetSourceTiming(uint64_t, uint64_t, uint64_t, int64_t) noexcept {}
void FSR3FGPresenter::SetCaptureAdmissionTime(int64_t) noexcept {}
void FSR3FGPresenter::SetOutputHistoryRevision(uint64_t) noexcept {}
void FSR3FGPresenter::SetFrameGuidanceAvailability(FrameGuidanceAvailability) noexcept {}
void FSR3FGPresenter::SetFrameGuidance(ID3D11Texture2D*, FrameGuidanceFrameId, bool, const RECT&) noexcept {}
bool FSR3FGPresenter::HasIndependentOverlay() const noexcept { return false; }
bool FSR3FGPresenter::BeginOverlayFrame(winrt::com_ptr<ID3D11Texture2D>&, winrt::com_ptr<ID3D11RenderTargetView>&, POINT&) noexcept { return false; }
bool FSR3FGPresenter::EndOverlayFrame() noexcept { return false; }
bool FSR3FGPresenter::OnResize() noexcept { return false; }
bool FSR3FGPresenter::_ResizeOverlaySurface() noexcept { return false; }
}
#endif
