#include "pch.h"
#include "ScalingModeEffectItem.h"
#if __has_include("ScalingTypeItem.g.cpp")
#include "ScalingTypeItem.g.cpp"
#endif
#if __has_include("ScalingModeEffectItem.g.cpp")
#include "ScalingModeEffectItem.g.cpp"
#endif
#include "App.h"
#include "AppSettings.h"
#include "CommonSharedConstants.h"
#include "EffectsService.h"
#include "EffectHelper.h"
#include "Logger.h"
#include "MainWindow.h"
#include "ScalingMode.h"
#include "ScalingModesService.h"
#include "StrHelper.h"
#include "NvidiaVfxParameterPolicy.h"
#include "EffectParameterLocalization.h"

using namespace Magpie;

namespace winrt::Magpie::implementation {

ScalingModeEffectItem::ScalingModeEffectItem(uint32_t scalingModeIdx, uint32_t effectIdx) 
	: _scalingModeIdx(scalingModeIdx), _effectIdx(effectIdx)
{
	EffectItem& data = _Data();
	_lastNvidiaVfxSameSize = _IsNvidiaVfxSameSize();
	_parameterChangedRevoker = ScalingModesService::Get().EffectParametersChanged(
		auto_revoke, [this](uint32_t mode, uint32_t effect) {
			if (!_IsRemoved() && mode == _scalingModeIdx && effect == _effectIdx) {
				const bool sameSize = _IsNvidiaVfxSameSize();
				_refreshingScalingTypes = true;
				auto reset = wil::scope_exit([this] { _refreshingScalingTypes = false; });
				if (sameSize != _lastNvidiaVfxSameSize) {
					_lastNvidiaVfxSameSize = sameSize;
					RaisePropertyChanged(L"ScalingTypes");
				}
				RaisePropertyChanged(L"CanScale"); RaisePropertyChanged(L"ScalingType");
				RaisePropertyChanged(L"IsShowScaleFactors");
				RaisePropertyChanged(L"IsShowScalingPixels");
			}
		});

	_effectInfo = data.isRecoveryInvalid ? nullptr : EffectsService::Get().GetEffect(data.name);

	if (_effectInfo) {
		_name = EffectHelper::GetDisplayName(data.name);
		_parametersViewModel = make_self<EffectParametersViewModel>(scalingModeIdx, effectIdx);
	} else {
		ResourceLoader resourceLoader =
			ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID);
		if (data.isRecoveryInvalid) {
			_name = resourceLoader.GetString(L"ScalingModes_RecoveryInvalid_Name");
			return;
		}
		_name = StrHelper::Concat(
			resourceLoader.GetString(L"ScalingModes_Description_UnknownEffect"),
			L" (",
			data.name,
			L")"
		);
	}
}

void ScalingModeEffectItem::ScalingModeIdx(uint32_t value) noexcept {
	if (_IsRemoved()) {
		return;
	}

	_scalingModeIdx = value;

	if (_parametersViewModel) {
		_parametersViewModel->ScalingModeIdx(value);
	}
}

void ScalingModeEffectItem::EffectIdx(uint32_t value) noexcept {
	if (_IsRemoved()) {
		return;
	}

	_effectIdx = value;

	if (_parametersViewModel) {
		_parametersViewModel->EffectIdx(value);
	}
}

bool ScalingModeEffectItem::CanScale() const noexcept {
	if (_IsRemoved()) {
		return false;
	}

	return _effectInfo && _effectInfo->CanScale() && !ScalingModesService::Get().GetScalingMode(_scalingModeIdx).hdrMigrationInvalid;
}

bool ScalingModeEffectItem::HasParameters() const noexcept {
	if (_IsRemoved()) {
		return false;
	}

	return _effectInfo && !_effectInfo->params.empty() && !ScalingModesService::Get().GetScalingMode(_scalingModeIdx).hdrMigrationInvalid;
}

hstring ScalingModeEffectItem::IssueDescription() const noexcept {
	if (_IsRemoved()) return {};
	if (ScalingModesService::Get().GetScalingMode(_scalingModeIdx).hdrMigrationInvalid)
		return ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID).GetString(L"ScalingModes_HdrMigrationInvalid_Description");
	if (_effectInfo) return {};
	return ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID).GetString(
		_Data().isRecoveryInvalid ? L"ScalingModes_RecoveryInvalid_Description" : L"ScalingModes_MissingEffect_Description");
}

IVector<IInspectable> ScalingModeEffectItem::ScalingTypes() noexcept {
	if (_IsNvidiaVfxSameSize()) return single_threaded_vector(std::vector<IInspectable>{make<ScalingTypeItem>(
		to_hstring(EffectParameterLocalization::Resource(L"NvidiaVfx_Vsr_SameSize", "Keep input size")), L"")});
	if (!_IsRemoved() && _Data().name == L"NvidiaVFX\\Upscale") {
		std::vector<IInspectable> items;
		items.push_back(make<ScalingTypeItem>(to_hstring(EffectParameterLocalization::Resource(L"NvidiaVfx_Upscale_Auto", "Auto fit target")),
			to_hstring(EffectParameterLocalization::Resource(L"NvidiaVfx_Upscale_AutoHelp", "Choose the lowest legal ratio, then fit the target with Bicubic."))));
		for (const wchar_t* ratio : {L"4/3×", L"1.5×", L"2×", L"3×", L"4×"}) items.push_back(make<ScalingTypeItem>(ratio,
			to_hstring(EffectParameterLocalization::Resource(L"NvidiaVfx_Upscale_FixedHelp", "Exact native ratio; width and height must be integers."))));
		return single_threaded_vector(std::move(items));
	}
	using Windows::ApplicationModel::Resources::ResourceLoader;
	ResourceLoader resourceLoader =
		ResourceLoader::GetForCurrentView(CommonSharedConstants::APP_RESOURCE_MAP_ID);
	
	return single_threaded_vector(std::vector<IInspectable>{
		make<ScalingTypeItem>(
			resourceLoader.GetString(L"ScalingModes_ScaleFlyout_Type_Factor"),
			resourceLoader.GetString(L"ScalingModes_ScaleFlyout_Type_Factor_Description")
		),
		make<ScalingTypeItem>(
			resourceLoader.GetString(L"ScalingModes_ScaleFlyout_Type_Fit"),
			resourceLoader.GetString(L"ScalingModes_ScaleFlyout_Type_Fit_Description")
		),
		make<ScalingTypeItem>(
			resourceLoader.GetString(L"ScalingModes_ScaleFlyout_Type_Absolute"),
			resourceLoader.GetString(L"ScalingModes_ScaleFlyout_Type_Absolute_Description")
		),
		make<ScalingTypeItem>(
			resourceLoader.GetString(L"ScalingModes_ScaleFlyout_Type_Fill"),
			resourceLoader.GetString(L"ScalingModes_ScaleFlyout_Type_Fill_Description")
		),
	});
}

int ScalingModeEffectItem::ScalingType() const noexcept {
	if (_IsNvidiaVfxSameSize()) return 0;
	if (_IsRemoved()) {
		return (int)ScalingType::Normal;
	}

	if (_Data().name == L"NvidiaVFX\\Upscale") {
		const auto it = _Data().parameters.find(L"upscaleMode");
		return it == _Data().parameters.end() ? 0 : int(it->second);
	}
	return (int)_Data().scalingType;
}

static SIZE GetMonitorSize() noexcept {
	// 使用距离主窗口最近的显示器
	HWND hwndMain = App::Get().MainWindow().Handle();
	HMONITOR hMonitor = MonitorFromWindow(hwndMain, MONITOR_DEFAULTTONEAREST);
	if (!hMonitor) {
		Logger::Get().Win32Error("MonitorFromWindow 失败");
		return { 400,300 };
	}

	MONITORINFO mi{ .cbSize = sizeof(mi) };
	if (!GetMonitorInfo(hMonitor, &mi)) {
		Logger::Get().Win32Error("GetMonitorInfo 失败");
		return { 400,300 };
	}
	
	return {
		mi.rcMonitor.right - mi.rcMonitor.left,
		mi.rcMonitor.bottom - mi.rcMonitor.top
	};
}

void ScalingModeEffectItem::ScalingType(int value) {
	if (_refreshingScalingTypes || _IsNvidiaVfxSameSize()) return;
	if (!CanScale() || value < 0) {
		return;
	}

	EffectItem& data = _Data();
	if (data.name == L"NvidiaVFX\\Upscale") {
		if (value > 5) return;
		data.parameters[L"upscaleMode"] = float(value);
		data.scalingType = ::Magpie::ScalingType::Fit; data.scale = {1.f,1.f};
		RaisePropertyChanged(L"ScalingType"); RaisePropertyChanged(L"IsShowScaleFactors"); RaisePropertyChanged(L"IsShowScalingPixels");
		_SaveScaleChange(); return;
	}
	const ::Magpie::ScalingType scalingType = (::Magpie::ScalingType)value;
	if (data.scalingType == scalingType) {
		return;
	}

	if (data.scalingType == ::Magpie::ScalingType::Absolute) {
		data.scale = { 1.0f,1.0f };
		RaisePropertyChanged(L"ScaleFactorX");
		RaisePropertyChanged(L"ScaleFactorY");
	} else if (scalingType == ::Magpie::ScalingType::Absolute) {
		SIZE monitorSize = GetMonitorSize();
		data.scale = { (float)monitorSize.cx,(float)monitorSize.cy };

		RaisePropertyChanged(L"ScalingPixelsX");
		RaisePropertyChanged(L"ScalingPixelsY");
	}

	data.scalingType = scalingType;
	RaisePropertyChanged(L"ScalingType");
	RaisePropertyChanged(L"IsShowScaleFactors");
	RaisePropertyChanged(L"IsShowScalingPixels");

	_SaveScaleChange();
}

bool ScalingModeEffectItem::IsShowScaleFactors() const noexcept {
	if (_IsRemoved()) {
		return false;
	}

	if (_Data().name == L"NvidiaVFX\\Upscale" || _IsNvidiaVfxSameSize()) return false;
	::Magpie::ScalingType scalingType = _Data().scalingType;
	return scalingType == ::Magpie::ScalingType::Normal || scalingType == ::Magpie::ScalingType::Fit;
}

bool ScalingModeEffectItem::IsShowScalingPixels() const noexcept {
	if (_IsRemoved()) {
		return false;
	}

	return _Data().name != L"NvidiaVFX\\Upscale" && !_IsNvidiaVfxSameSize() && _Data().scalingType == ::Magpie::ScalingType::Absolute;
}

double ScalingModeEffectItem::ScaleFactorX() const noexcept {
	if (_IsRemoved()) {
		return 0.0;
	}

	return _Data().scale.first;
}

void ScalingModeEffectItem::ScaleFactorX(double value) {
	if (!CanScale() || !std::isfinite(value) || value <= 0) {
		return;
	}

	EffectItem& data = _Data();
	const auto previous=data.scale;
	if (data.scalingType != ::Magpie::ScalingType::Normal && data.scalingType != ::Magpie::ScalingType::Fit) {
		return;
	}

	// 用户将 NumberBox 清空时会传入 nan
	if (!std::isnan(value) && value + std::numeric_limits<float>::epsilon() > 1e-4) {
		data.scale.first = (float)value;
	}
	
	RaisePropertyChanged(L"ScaleFactorX");
	if (data.scale!=previous) _SaveScaleChange();
}

double ScalingModeEffectItem::ScaleFactorY() const noexcept {
	if (_IsRemoved()) {
		return 0.0;
	}

	return _Data().scale.second;
}

void ScalingModeEffectItem::ScaleFactorY(double value) {
	if (!CanScale() || !std::isfinite(value) || value <= 0) {
		return;
	}

	EffectItem& data = _Data();
	const auto previous=data.scale;
	if (data.scalingType != ::Magpie::ScalingType::Normal && data.scalingType != ::Magpie::ScalingType::Fit) {
		return;
	}

	if (!std::isnan(value) && value + std::numeric_limits<float>::epsilon() > 1e-4) {
		data.scale.second = (float)value;
	}

	RaisePropertyChanged(L"ScaleFactorY");
	if (data.scale!=previous) _SaveScaleChange();
}

double ScalingModeEffectItem::ScalingPixelsX() const noexcept {
	if (_IsRemoved()) {
		return 0.0;
	}

	return _Data().scale.first;
}

void ScalingModeEffectItem::ScalingPixelsX(double value) {
	if (!CanScale() || !std::isfinite(value) || value <= 0) {
		return;
	}

	EffectItem& data = _Data();
	const auto previous=data.scale;
	if (data.scalingType != ::Magpie::ScalingType::Absolute) {
		return;
	}

	if (!std::isnan(value) && value > 0.5) {
		data.scale.first = (float)std::round(value);
	}

	RaisePropertyChanged(L"ScalingPixelsX");
	if (data.scale!=previous) _SaveScaleChange();
}

double ScalingModeEffectItem::ScalingPixelsY() const noexcept {
	if (_IsRemoved()) {
		return 0.0;
	}

	return _Data().scale.second;
}

void ScalingModeEffectItem::ScalingPixelsY(double value) {
	if (!CanScale() || !std::isfinite(value) || value <= 0) {
		return;
	}

	EffectItem& data = _Data();
	const auto previous=data.scale;
	if (data.scalingType != ::Magpie::ScalingType::Absolute) {
		return;
	}

	if (!std::isnan(value) && value > 0.5) {
		data.scale.second = (float)std::round(value);
	}

	RaisePropertyChanged(L"ScalingPixelsY");
	if (data.scale!=previous) _SaveScaleChange();
}

void ScalingModeEffectItem::Remove() {
	if (_IsRemoved() || ScalingModesService::Get().GetScalingMode(_scalingModeIdx).hdrMigrationInvalid) {
		return;
	}

	auto lifetime=get_strong();
	Removed.Invoke(_effectIdx); // The parent detaches only after a successful transaction.
}

bool ScalingModeEffectItem::_IsNvidiaVfxSameSize() const noexcept {
	if (_IsRemoved() || _Data().name != L"RTXVideo\\RTXVideo_VSR") return false;
	const auto category = _Data().parameters.find(L"category");
	return NvidiaVfxSameSize(NvidiaVfxCategory(category == _Data().parameters.end() ? 0.f : category->second));
}

void ScalingModeEffectItem::_SaveScaleChange() {
	AppSettings::Get().SaveAsync();
}

bool ScalingModeEffectItem::CanDrag() const noexcept {
	if (_IsRemoved()) {
		return false;
	}

	const ScalingMode& mode = ScalingModesService::Get().GetScalingMode(_scalingModeIdx);
	return !mode.hdrMigrationInvalid && mode.effects.size() > 1;
}

void ScalingModeEffectItem::RefreshDragState() {
	if (_IsRemoved()) {
		return;
	}

	RaisePropertyChanged(L"CanDrag");
}

EffectItem& ScalingModeEffectItem::_Data() noexcept {
	return ScalingModesService::Get().GetScalingMode(_scalingModeIdx).effects[_effectIdx];
}

const EffectItem& ScalingModeEffectItem::_Data() const noexcept {
	return ScalingModesService::Get().GetScalingMode(_scalingModeIdx).effects[_effectIdx];
}

// 应确保被删除后依然处于合法的状态，调用任何方法都不会崩溃，见 ScalingModeItem::_IsRemoved
bool ScalingModeEffectItem::_IsRemoved() const noexcept {
	return _scalingModeIdx == std::numeric_limits<uint32_t>::max() ||
		_effectIdx == std::numeric_limits<uint32_t>::max();
}

}
