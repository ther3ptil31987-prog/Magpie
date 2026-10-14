"""Exercise production manual list and scale methods with synchronous row callbacks."""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1])

def block(text, marker):
    start = text.index(marker)
    brace = text.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

group = (repo / 'src/Magpie/ScalingModeItem.cpp').read_text(encoding='utf-8-sig')
effect = (repo / 'src/Magpie/ScalingModeEffectItem.cpp').read_text(encoding='utf-8-sig')
options = (repo / 'src/Magpie.Core/include/ScalingOptions.h').read_text(encoding='utf-8-sig')
fg = options[options.index('enum class FrameGenerationEffectKind'):options.index('enum class EffectParameterApplyMode')]
(out / 'manual_fg.inc').write_text('namespace Magpie {\n' + fg + '\n}\n', encoding='utf-8')
parts = [block(group, marker) for marker in [
    'void ScalingModeItem::_Effects_VectorChanged(',
    'void ScalingModeItem::_ScalingModeEffectItem_Removed(',
    'void ScalingModeItem::_RefreshEffectDragState(',
    'void ScalingModeItem::AddEffect(',
    'bool ScalingModeItem::CanAddEffect(',
    'bool ScalingModeItem::CanReorderEffects()']]
parts += [block(effect, marker) for marker in [
    'bool ScalingModeEffectItem::CanScale()',
    'bool ScalingModeEffectItem::HasParameters()',
    'bool ScalingModeEffectItem::CanDrag()',
    'bool ScalingModeEffectItem::_IsRemoved()',
    'bool ScalingModeEffectItem::_IsNvidiaVfxSameSize()',
    'void ScalingModeEffectItem::ScalingType(int value)',
    'void ScalingModeEffectItem::ScaleFactorX(double value)',
    'void ScalingModeEffectItem::ScaleFactorY(double value)',
    'void ScalingModeEffectItem::ScalingPixelsX(double value)',
    'void ScalingModeEffectItem::ScalingPixelsY(double value)',
    'void ScalingModeEffectItem::_SaveScaleChange()',
    'void ScalingModeEffectItem::Remove()']]

prefix = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "manual_fg.inc"
#include "NvidiaVfxParameters.h"
using hstring=std::wstring;
struct SIZE { long cx,cy; };
static SIZE GetMonitorSize() { return {1920,1080}; }
namespace Magpie {
enum class ScalingType : uint32_t { Normal,Fit,Absolute,Fill };
struct EffectItem {
 std::wstring name; std::map<std::wstring,float> parameters;
 ScalingType scalingType=ScalingType::Normal; std::pair<float,float> scale{1,1};
 bool operator==(const EffectItem&) const = default;
};
struct ScalingMode { std::vector<EffectItem> effects; bool hdrMigrationInvalid=false; };
struct AppSettings {
 int saves=0; static AppSettings& Get() { static AppSettings s; return s; }
 void SaveAsync() { ++saves; }
};
struct ScalingModesService {
 std::vector<ScalingMode> modes;
 static ScalingModesService& Get() { static ScalingModesService s; return s; }
 auto& GetScalingMode(uint32_t index) { return modes.at(index); }
};
struct EffectInfo {
 bool scaling=false; std::vector<int> params{1,2}; bool CanScale() const { return scaling; }
};
struct EffectsService {
 std::map<std::wstring,EffectInfo> effects;
 static EffectsService& Get() { static EffectsService s; return s; }
 const EffectInfo* GetEffect(const hstring& name) {
  const auto it=effects.find(name); return it==effects.end()?nullptr:&it->second;
 }
};
static void NormalizeDLSSNRDetailParameters(auto&) {} // Ordinary-effect initialization is outside this test.
}
using namespace Magpie;
struct RemovedEvent {
 std::function<void(uint32_t)> handler;
 void Invoke(uint32_t i) { if(handler) handler(i); }
};
struct ScalingModeEffectItem : std::enable_shared_from_this<ScalingModeEffectItem> {
 uint32_t _scalingModeIdx=0,_effectIdx=0; const EffectInfo* _effectInfo=nullptr; RemovedEvent Removed;
 bool _refreshingScalingTypes=false;
 auto get_strong() { return shared_from_this(); }
 auto& _Data() { return ScalingModesService::Get().GetScalingMode(_scalingModeIdx).effects.at(_effectIdx); }
 const auto& _Data() const { return ScalingModesService::Get().GetScalingMode(_scalingModeIdx).effects.at(_effectIdx); }
 void RaisePropertyChanged(const wchar_t*) {}
 void EffectIdx(uint32_t i) { _effectIdx=i; }
 bool CanScale() const noexcept; bool HasParameters() const noexcept; bool CanDrag() const noexcept;
 bool _IsRemoved() const noexcept; bool _IsNvidiaVfxSameSize() const noexcept;
 void ScalingType(int); void ScaleFactorX(double); void ScaleFactorY(double);
 void ScalingPixelsX(double); void ScalingPixelsY(double); void _SaveScaleChange(); void Remove();
 void RefreshDragState() {}
};
using IInspectable=std::shared_ptr<ScalingModeEffectItem>;
enum class CollectionChange { ItemRemoved,ItemInserted };
struct IVectorChangedEventArgs {
 CollectionChange kind; uint32_t index;
 auto CollectionChange() const -> ::CollectionChange { return kind; } uint32_t Index() const { return index; }
};
template<class T> struct IObservableVector {};
struct Rows {
 std::vector<IInspectable> values;
 std::function<void(IVectorChangedEventArgs)> changed;
 auto begin() const { return values.begin(); } auto end() const { return values.end(); }
 uint32_t Size() const { return static_cast<uint32_t>(values.size()); }
 const auto& GetAt(uint32_t i) const { return values.at(i); }
 void Append(ScalingModeEffectItem& row) {
  values.push_back(row.shared_from_this()); if(changed) changed({CollectionChange::ItemInserted,Size()-1});
 }
 void RemoveAt(uint32_t i) {
  values.erase(values.begin()+i); if(changed) changed({CollectionChange::ItemRemoved,i});
 }
 void Move(uint32_t from,uint32_t to) {
  auto row=values.at(from); RemoveAt(from); values.insert(values.begin()+to,row);
  if(changed) changed({CollectionChange::ItemInserted,to});
 }
};
static auto& GetEffectItemImpl(const IInspectable& row) { return *row; }
struct ScalingModeItem {
 uint32_t _index=0,_movingFromIdx=UINT32_MAX; bool _isMovingEffects=true; Rows _effects;
 ScalingModeItem() { _effects.changed=[this](auto args) { _Effects_VectorChanged({},args); }; }
 auto& _Data() const { return ScalingModesService::Get().GetScalingMode(_index); }
 bool _IsRemoved() const noexcept { return _index==UINT32_MAX; }
 void RaisePropertyChanged(const wchar_t*) {}
 IInspectable _CreateScalingModeEffectItem(uint32_t mode,uint32_t index) {
  auto row=std::make_shared<ScalingModeEffectItem>(); row->_scalingModeIdx=mode; row->_effectIdx=index;
  row->_effectInfo=EffectsService::Get().GetEffect(_Data().effects.at(index).name);
  row->Removed.handler=[this](uint32_t i) { _ScalingModeEffectItem_Removed(i); }; return row;
 }
 void _Effects_VectorChanged(IObservableVector<IInspectable> const&,IVectorChangedEventArgs const&);
 void _ScalingModeEffectItem_Removed(uint32_t); void _RefreshEffectDragState();
 void AddEffect(const hstring&); bool CanAddEffect(const hstring&) const noexcept; bool CanReorderEffects() const noexcept;
};
'''

tests = r'''
int main() {
 auto& service=ScalingModesService::Get(); service.modes.resize(1);
 auto& catalog=EffectsService::Get().effects;
 for (const auto name:{L"Color\\HDR_to_SDR",L"Color\\SDR_to_HDR",L"RTXVideo\\RTXVideo_HDR",L"CAS\\CAS",L"XeSSFG\\XeSS_FrameGeneration"})
  catalog.emplace(name,EffectInfo{});
 catalog.emplace(L"DLSS\\DLSS_SR",EffectInfo{true});
 ScalingModeItem group;
 group.AddEffect(L"Color\\HDR_to_SDR"); group.AddEffect(L"CAS\\CAS"); group.AddEffect(L"Color\\SDR_to_HDR");
 auto& effects=service.modes[0].effects;
 effects[0].parameters={{L"mode",0},{L"whiteNits",257.3f}};
 effects[2].parameters={{L"mode",2}};
 for (const auto& row:group._effects) assert(row->HasParameters() && row->CanDrag());
 auto before=effects; group.AddEffect(L"DLSS\\DLSS_SR");
 assert(effects.size()==4 && std::equal(before.begin(),before.end(),effects.begin()));
 auto scaled=group._effects.GetAt(3); before=effects; const auto saves=AppSettings::Get().saves;
 scaled->ScaleFactorX(1.2345); assert(effects.size()==before.size());
 assert(std::equal(before.begin(),before.begin()+3,effects.begin()));
 assert(effects[3].scale.first==1.2345f && AppSettings::Get().saves==saves+1);
 scaled->ScaleFactorX(1.2345); scaled->ScaleFactorX(std::numeric_limits<double>::quiet_NaN());
 assert(AppSettings::Get().saves==saves+1);
 scaled->ScalingType(int(Magpie::ScalingType::Absolute)); scaled->ScalingPixelsY(1440);
 assert(effects[3].scale==std::pair(1920.f,1440.f) && effects.size()==4);
 // Move callbacks update only order and row identity, including actual converter rows.
 before=effects; group._effects.Move(2,0);
 assert(effects[0]==before[2] && effects[1]==before[0] && effects[2]==before[1] && effects[3]==before[3]);
 for (uint32_t i=0;i<group._effects.Size();++i) assert(group._effects.GetAt(i)->_effectIdx==i);
 auto removed=group._effects.GetAt(1); before=effects; removed->Remove();
 assert(effects.size()==3 && effects[0]==before[0] && effects[1]==before[2] && effects[2]==before[3]);
 assert(removed->_IsRemoved() && !removed->HasParameters() && !removed->CanDrag());
 removed->Remove(); assert(effects.size()==3); // late UI callbacks cannot remove a neighboring node
 group.AddEffect(L"RTXVideo\\RTXVideo_HDR"); assert(effects.size()==4);
 auto rtx=group._effects.GetAt(3); assert(rtx->HasParameters() && rtx->CanDrag()); rtx->Remove(); assert(effects.size()==3);
 group.AddEffect(L"XeSSFG\\XeSS_FrameGeneration"); assert(!group.CanAddEffect(L"XeSSFG\\XeSS_FrameGeneration"));
 group.AddEffect(L"XeSSFG\\XeSS_FrameGeneration"); assert(effects.size()==4);
 service.modes[0].hdrMigrationInvalid=true; before=effects;
 assert(!group.CanAddEffect(L"CAS\\CAS") && !group.CanReorderEffects());
 for (const auto& row:group._effects) { assert(!row->HasParameters() && !row->CanDrag() && !row->CanScale()); row->Remove(); }
 assert(effects==before);
 std::cout<<"PASS: production manual add/remove/drag/scale, converter editing access, exact neighbors/order/parameters, row detachment, duplicate FG and recovery guards; no automatic converter management\n";
}
'''
(out / 'HdrManualEditingTests.cpp').write_text(prefix + '\n'.join(parts) + tests, encoding='utf-8')
