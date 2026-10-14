// Real WARP + Dear ImGui + production ImGuiBackend and shaders. No app launched.
#include <windows.h>
#include <d3d11_4.h>
#include <winrt/base.h>
#include <imgui.h>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include "ImGuiBackend.h"
#include "shaders/ImGuiImplPS.h"
#include "shaders/ImGuiImplVS.h"

namespace Magpie {
struct Logger {
	static Logger& Get() { static Logger logger; return logger; }
	void ComError(const char* text, HRESULT hr) { std::cerr << text << ": " << std::hex << hr << '\n'; std::abort(); }
	void Error(const char* text) { std::cerr << text << '\n'; std::abort(); }
};
class DeviceResources {
public:
	winrt::com_ptr<ID3D11Device5> device;
	winrt::com_ptr<ID3D11DeviceContext4> context;
	winrt::com_ptr<ID3D11SamplerState> sampler;
	ID3D11Device5* GetD3DDevice() { return device.get(); }
	ID3D11DeviceContext4* GetD3DDC() { return context.get(); }
	ID3D11SamplerState* GetSampler(D3D11_FILTER, D3D11_TEXTURE_ADDRESS_MODE) { return sampler.get(); }
};
struct DirectXHelper {
	static winrt::com_ptr<ID3D11Texture2D> CreateTexture2D(ID3D11Device* device, DXGI_FORMAT format,
		UINT width, UINT height, UINT flags, D3D11_USAGE usage, UINT cpuFlags,
		const D3D11_SUBRESOURCE_DATA* data) {
		const D3D11_TEXTURE2D_DESC desc{width,height,1,1,format,{1,0},usage,flags,cpuFlags,0};
		winrt::com_ptr<ID3D11Texture2D> result;
		winrt::check_hresult(device->CreateTexture2D(&desc,data,result.put()));
		return result;
	}
};
}
#include "OverlayBackendProduction.inc"
using namespace Magpie;

static void Near(float actual, float expected) {
	if (std::abs(actual-expected) > .003f + std::abs(expected)*.001f) {
		std::cerr << "Pixel mismatch: " << actual << " expected " << expected << '\n'; std::abort();
	}
}
static float Decode(float value) {
	return value <= .04045f ? value/12.92f : std::pow((value+.055f)/1.055f,2.4f);
}
static float Half(uint16_t value) {
	const int exp = (value >> 10) & 31;
	const float magnitude = exp ? std::ldexp(float(1024+(value&1023)), exp-25) : std::ldexp(float(value&1023),-24);
	return value & 0x8000 ? -magnitude : magnitude;
}
static void UnbindColor(const ImDrawList*, const ImDrawCmd* command) {
	auto& resources = *static_cast<DeviceResources*>(command->UserCallbackData);
	ID3D11Buffer* empty = nullptr;
	resources.context->PSSetConstantBuffers(0,1,&empty);
}
static std::array<float,4> ReadPixel(DeviceResources& resources, ID3D11Texture2D* target, UINT x=32, UINT y=32) {
	D3D11_TEXTURE2D_DESC desc; target->GetDesc(&desc);
	desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
	winrt::com_ptr<ID3D11Texture2D> staging;
	winrt::check_hresult(resources.device->CreateTexture2D(&desc,nullptr,staging.put()));
	resources.context->CopyResource(staging.get(),target);
	D3D11_MAPPED_SUBRESOURCE mapped;
	winrt::check_hresult(resources.context->Map(staging.get(),0,D3D11_MAP_READ,0,&mapped));
	const auto* row = static_cast<const uint8_t*>(mapped.pData)+y*mapped.RowPitch;
	std::array<float,4> result{};
	for (size_t i=0;i<4;++i) result[i] = desc.Format==DXGI_FORMAT_R16G16B16A16_FLOAT
		? Half(reinterpret_cast<const uint16_t*>(row)[x*4+i]) : row[x*4+i]/255.f;
	resources.context->Unmap(staging.get(),0);
	return result;
}

int main() {
	DeviceResources resources;
	winrt::com_ptr<ID3D11Device> device;
	winrt::com_ptr<ID3D11DeviceContext> context;
	winrt::check_hresult(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
		D3D11_SDK_VERSION,device.put(),nullptr,context.put()));
	resources.device=device.as<ID3D11Device5>(); resources.context=context.as<ID3D11DeviceContext4>();
	D3D11_SAMPLER_DESC sampler{};
	sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
	sampler.MaxLOD=D3D11_FLOAT32_MAX;
	winrt::check_hresult(device->CreateSamplerState(&sampler,resources.sampler.put()));
	ImGui::CreateContext();
	auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.DisplaySize={64,64}; io.DeltaTime=1.f/60;
	ImGui::StyleColorsDark();
	ImGuiBackend backend; assert(backend.Initialize(resources)); assert(backend.BuildFonts());
	unsigned cases=0;
	for (DXGI_FORMAT format : {DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R8G8B8A8_UNORM}) {
		auto target=DirectXHelper::CreateTexture2D(device.get(),format,64,64,D3D11_BIND_RENDER_TARGET,D3D11_USAGE_DEFAULT,0,nullptr);
		winrt::com_ptr<ID3D11RenderTargetView> rtv;
		winrt::check_hresult(device->CreateRenderTargetView(target.get(),nullptr,rtv.put()));
		for (float scale : {0.f,203.f/80,300.f/80}) {
			if (format!=DXGI_FORMAT_R16G16B16A16_FLOAT && scale!=0) continue;
			for (const auto color : {std::array<uint8_t,4>{255,255,255,255},{128,40,8,128},{9,10,11,255}}) {
				for (uint8_t coverage : {uint8_t(255),uint8_t(64)}) {
					const D3D11_SUBRESOURCE_DATA data{&coverage,1,0};
					auto font=DirectXHelper::CreateTexture2D(device.get(),DXGI_FORMAT_R8_UNORM,1,1,D3D11_BIND_SHADER_RESOURCE,D3D11_USAGE_DEFAULT,0,&data);
					winrt::com_ptr<ID3D11ShaderResourceView> srv;
					winrt::check_hresult(device->CreateShaderResourceView(font.get(),nullptr,srv.put()));
					for (float sceneNits : {0.f,1000.f,4000.f}) {
						if (format!=DXGI_FORMAT_R16G16B16A16_FLOAT && sceneNits!=0) continue;
						resources.context->ClearState();
						ID3D11RenderTargetView* view=rtv.get(); resources.context->OMSetRenderTargets(1,&view,nullptr);
						const float background=sceneNits/80;
						const float clear[]{background,background,background,sceneNits==0?0.f:1.f};
						resources.context->ClearRenderTargetView(view,clear);
						ImGui::NewFrame(); auto* list=ImGui::GetBackgroundDrawList();
						list->AddCallback(UnbindColor,&resources);
						list->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
						list->AddImage((ImTextureID)srv.get(),{0,0},{64,64},{0,0},{1,1},IM_COL32(color[0],color[1],color[2],color[3]));
						ImGui::Render(); backend.RenderDrawData(*ImGui::GetDrawData(),{},scale);
						const auto pixel=ReadPixel(resources,target.get());
						const float alpha=(color[3]/255.f)*(coverage/255.f);
						for (size_t i=0;i<3;++i) {
							const float source=scale>0?Decode(color[i]/255.f)*scale:color[i]/255.f;
							Near(pixel[i],source*alpha+background*(1-alpha));
						}
						Near(pixel[3],alpha+clear[3]*(1-alpha)); ++cases;
					}
				}
			}
		}
		if (format!=DXGI_FORMAT_R16G16B16A16_FLOAT) continue;
		// Panels and popups use the real production style scope. Check that HDR
		// blocks a 4000-nit scene, and that leaving the scope restores SDR alpha.
		for (float hdrWhiteScale : {203.f/80,0.f}) {
			for (int repeat=0;repeat<2;++repeat) {
				ID3D11RenderTargetView* view=rtv.get(); resources.context->OMSetRenderTargets(1,&view,nullptr);
				const float clear[]{50,50,50,1}; resources.context->ClearRenderTargetView(view,clear);
				ImGui::NewFrame(); bool _isHdrOverlay=false;
				{
#include "OverlayStyleProduction.inc"
					Near(ImGui::GetStyle().Colors[ImGuiCol_PopupBg].w,hdrWhiteScale>0?1.f:.94f);
					ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({64,64});
					ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
					ImGui::Begin("Panel",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
					ImGui::End(); ImGui::PopStyleVar();
					if (_isHdrOverlay) ImGui::PopStyleColor(2);
					ImGui::Render(); backend.RenderDrawData(*ImGui::GetDrawData(),{},hdrWhiteScale);
				}
				Near(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w,.94f);
				if (repeat==1) {
					const auto pixel=ReadPixel(resources,target.get());
					const float bg=15.f/255;
					const float alpha=240.f/255;
					Near(pixel[0],hdrWhiteScale>0?Decode(bg)*hdrWhiteScale:bg*alpha+50*(1-alpha)); ++cases;
				}
			}
		}
	}
	ImGui::DestroyContext();
	std::cout << "PASS " << cases << " WARP UI cases: production shader/backend, constants/reset callback, sRGB, coverage, alpha, bright scenes, panel style and SDR restoration\n";
}
