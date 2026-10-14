"""Exercise production group naming/deletion code with synchronous binding callbacks, without UI/GPU startup."""
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
repo=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]); out.mkdir(parents=True,exist_ok=True)
baseline=sys.argv[2] if len(sys.argv)>2 else None
def read(path, old=False):
 if old and baseline:
  return subprocess.check_output(['git','-C',str(repo),'show',baseline+':'+path]).decode('utf-8-sig')
 return (repo/path).read_text(encoding='utf-8-sig')
def block(text, marker):
 start=text.index(marker);brace=text.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
service=read('src/Magpie/ScalingModesService.cpp')
item=read('src/Magpie/ScalingModeItem.cpp')
view=read('src/Magpie/ScalingModesViewModel.cpp')
effect=read('src/Magpie/ScalingModeEffectItem.cpp')
event=read('src/Magpie.Core/include/Event.h')
(out/'event.inc').write_text(event[:event.index('// 对 Event 的每个操作加锁')]+'\n}\n',encoding='utf-8')
parts=[]
for marker in ['ScalingMode& ScalingModesService::GetScalingMode(', 'uint32_t ScalingModesService::GetScalingModeCount(',
 'void ScalingModesService::AddScalingMode(', 'bool ScalingModesService::CanUseName(',
 'bool ScalingModesService::HasDuplicateNames(', 'bool ScalingModesService::HasNameConflict(',
 'bool ScalingModesService::RenameScalingMode(', 'static void UpdateProfileAfterRemove(']:
 parts.append(block(service,marker))
# Use checked access in the storage fixture so a stale index produces a diagnostic instead of undefined behavior.
parts[0]=parts[0].replace('ScalingModes()[idx]', 'ScalingModes().at(idx)')
parts.append(block(read('src/Magpie/ScalingModesService.cpp',old=True),'void ScalingModesService::RemoveScalingMode('))
for marker in ['void ScalingModeItem::_Index(', 'bool ScalingModeItem::_IsRemoved()', 'hstring ScalingModeItem::Name() const',
 'void ScalingModeItem::Detach()', 'void ScalingModeItem::PrepareForRemoval(']:
 parts.append(block(item,marker))
parts.append(block(read('src/Magpie/ScalingModeItem.cpp',old=True),'void ScalingModeItem::Remove()'))
for marker in ['void ScalingModeEffectItem::ScalingModeIdx(', 'bool ScalingModeEffectItem::_IsRemoved()']:
 parts.append(block(effect,marker))
for marker in ['void ScalingModesViewModel::_ScalingModesService_Removing(', 'void ScalingModesViewModel::_ScalingModesService_Removed(']:
 parts.append(block(view,marker))
start=service.index('\tstd::vector<ScalingMode>& settings =',service.index('bool ScalingModesService::Import('))
end=service.index('\n\tsettings.insert(',start)
parts.append('void ImportGroups(std::vector<ScalingMode> scalingModes, bool loadingSettings, uint32_t* renamedCount) {\n'
 '*renamedCount=0;\n'+service[start:end]+'\nsettings.insert(settings.end(), scalingModes.begin(), scalingModes.end());\n}')
publish=block(read('src/Magpie/AppSettings.cpp'),'void AppSettings::PublishStartupNotice()')
parts.append(publish)
code=r'''
#include <windows.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "ScalingModeNames.h"
#include "FrameGenerationParameters.h"
namespace winrt { struct auto_revoke_t{}; inline constexpr auto_revoke_t auto_revoke{}; }
#include "event.inc"
using hstring=std::wstring;
static void Require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
namespace Magpie {
struct FixtureEffect {
 int value;
 std::wstring name;
 std::map<std::wstring, float> parameters;
 bool isRecoveryInvalid = false;
 std::shared_ptr<const uint8_t> runtimeIdentity = std::make_shared<const uint8_t>(uint8_t{0});
 FixtureEffect(int v):value(v){}
 operator int() const { return value; }
 bool operator==(const FixtureEffect& other) const { return value == other.value; }
};
struct ScalingMode { std::wstring name; std::vector<FixtureEffect> effects;
 std::shared_ptr<const uint8_t> runtimeIdentity = std::make_shared<const uint8_t>(uint8_t{0}); };
struct Profile { int scalingMode=-1; };
enum class ScalingError { NoError,DuplicateScalingModeNames };
enum class EffectAddedWay { Add, Duplicate, Import };
struct ErrorService {
 static ErrorService& Get() { static ErrorService x; return x; }
 unsigned duplicateNotices=0;
 void Report(ScalingError error,std::string={}) { if(error==ScalingError::DuplicateScalingModeNames) ++duplicateNotices; }
};
struct StrHelper { static std::string UTF16ToUTF8(std::wstring_view) { return {}; } };
struct AppSettings {
 std::vector<ScalingMode> modes;
 Profile defaults;
 std::vector<Profile> profiles;
 unsigned saves=0;
 std::filesystem::path _recoveredConfigPath;
 std::string _recoveryDetails;
 ScalingError _recoveryNotice=ScalingError::NoError;
 static AppSettings& Get() { static AppSettings x; return x; }
 auto& ScalingModes() { return modes; }
 auto& DefaultProfile() { return defaults; }
 auto& Profiles() { return profiles; }
 void SaveAsync() { ++saves; }
 void PublishStartupNotice() noexcept;
};
struct ScalingModesService {
 static ScalingModesService& Get() { static ScalingModesService x; return x; }
 ScalingMode& GetScalingMode(uint32_t);
 uint32_t GetScalingModeCount();
 void AddScalingMode(std::wstring_view,int);
 bool CanUseName(std::wstring_view,uint32_t) const noexcept;
 bool HasDuplicateNames() const noexcept;
 bool HasNameConflict(uint32_t) const noexcept;
 bool RenameScalingMode(uint32_t,std::wstring_view);
 void RemoveScalingMode(uint32_t);
 Event<EffectAddedWay> ScalingModeAdded;
 Event<uint32_t,uint32_t> ScalingModeMoved;
 Event<uint32_t> ScalingModeRemoving, ScalingModeRemoved;
 Event<> ScalingModeNamesChanged;
};
}
using namespace Magpie;
struct ScalingModeItem;
namespace winrt::Magpie { using ScalingModeItem=std::shared_ptr<::ScalingModeItem>; }
struct IInspectable {
 std::shared_ptr<void> value;
 template<class T> IInspectable(std::shared_ptr<T> x):value(std::move(x)){}
 template<class T> T as() const { return std::static_pointer_cast<typename T::element_type>(value); }
};
template<class T> T* get_self(const std::shared_ptr<T>& p) { return p.get(); }
struct Collection {
 std::vector<IInspectable> values;
 std::function<void(uint32_t,bool)> changed;
 auto begin() const { return values.begin(); } auto end() const { return values.end(); }
 uint32_t Size() const { return static_cast<uint32_t>(values.size()); }
 void RemoveAt(uint32_t index) {
  if(changed) changed(index,false);
  values.erase(values.begin()+index);
  if(changed) changed(index,true);
 }
};
struct Parameters { uint32_t mode=0; void ScalingModeIdx(uint32_t x) { mode=x; } };
struct ScalingModeEffectItem {
 uint32_t _scalingModeIdx=0,_effectIdx=0;
 std::shared_ptr<Parameters> _parametersViewModel=std::make_shared<Parameters>();
 void ScalingModeIdx(uint32_t) noexcept;
 bool _IsRemoved() const noexcept;
 int Read() const {
  if(_IsRemoved()) return -1;
  return AppSettings::Get().modes.at(_scalingModeIdx).effects.at(_effectIdx);
 }
};
static ScalingModeEffectItem& GetEffectItemImpl(const IInspectable& item) {
 return *item.as<std::shared_ptr<ScalingModeEffectItem>>();
}
struct DummyRevoker { void revoke() {} };
struct ScalingModeItem : std::enable_shared_from_this<ScalingModeItem> {
 uint32_t _index;
 Collection _effects;
 std::vector<FixtureEffect> expected;
 DummyRevoker _effectsChangedRevoker;
 Event<EffectAddedWay>::EventRevoker _scalingModeAddedRevoker;
 Event<uint32_t,uint32_t>::EventRevoker _scalingModeMovedRevoker;
 Event<uint32_t>::EventRevoker _scalingModeRemovedRevoker; // retained for the optional baseline reproduction
 Event<>::EventRevoker _scalingModeNamesChangedRevoker;
 explicit ScalingModeItem(uint32_t index):_index(index),expected(AppSettings::Get().modes.at(index).effects) {
  auto& service=ScalingModesService::Get();
  _scalingModeAddedRevoker=service.ScalingModeAdded(winrt::auto_revoke,[](auto){});
  _scalingModeMovedRevoker=service.ScalingModeMoved(winrt::auto_revoke,[](auto,auto){});
  _scalingModeNamesChangedRevoker=service.ScalingModeNamesChanged(winrt::auto_revoke,[]{});
  for(uint32_t i=0;i<expected.size();++i) {
   auto effect=std::make_shared<ScalingModeEffectItem>(); effect->_scalingModeIdx=index;effect->_effectIdx=i;effect->_parametersViewModel->mode=index;
   _effects.values.emplace_back(effect);
  }
 }
 auto get_strong() { return shared_from_this(); }
 auto& _Data() const { return ScalingModesService::Get().GetScalingMode(_index); }
 void _Index(uint32_t) noexcept;
 bool _IsRemoved() const noexcept;
 hstring Name() const noexcept;
 void Detach() noexcept;
 void PrepareForRemoval(uint32_t) noexcept;
 void Remove();
 void VerifyRendered() const {
  if(_IsRemoved()) {
   Require(Name().empty(),"removed row still exposes a name");
   for(const auto& item:_effects) {
    auto& effect=GetEffectItemImpl(item);
    Require(effect.Read()==-1,"removed child still accesses group data");
    Require(effect._parametersViewModel->mode==UINT32_MAX,"removed parameter editor still has a live group index");
   }
  } else {
   Require(_Data().effects==expected,"surviving row points to a different effect chain");
   for(size_t i=0;i<expected.size();++i) Require(GetEffectItemImpl(_effects.values[i]).Read()==int(expected[i]),"child effect index changed identity");
  }
 }
 void RefreshAfterRemoval() { VerifyRendered(); }
};
struct ScalingModesViewModel {
 Collection _scalingModes;
 uint32_t _collectionGeneration=0,_movingFromIdx=UINT32_MAX;
 bool _updatingScalingModes=false;
 Event<uint32_t>::EventRevoker before,after;
 explicit ScalingModesViewModel(uint32_t loaded) {
  auto& service=ScalingModesService::Get();
  // Subscribe before creating late-loaded rows to exercise the former ordering hazard.
  before=service.ScalingModeRemoving(winrt::auto_revoke,[this](uint32_t i){_ScalingModesService_Removing(i);});
  after=service.ScalingModeRemoved(winrt::auto_revoke,[this](uint32_t i){_ScalingModesService_Removed(i);});
  for(uint32_t i=0;i<loaded;++i) _scalingModes.values.emplace_back(std::make_shared<ScalingModeItem>(i));
 }
 void _ScalingModesService_Removing(uint32_t);
 void _ScalingModesService_Removed(uint32_t);
 void RaisePropertyChanged(const wchar_t*) {}
 void VerifyRendered() { for(auto& x:_scalingModes) x.as<winrt::Magpie::ScalingModeItem>()->VerifyRendered(); }
};
PRODUCTION
static void DeleteCase(uint32_t removed,bool throughItem,uint32_t loaded=3) {
 auto& settings=AppSettings::Get();
 settings.modes={{L"Same",{10,11,12}},{L"Same",{20}},{L"Last",{30,31}}};
 settings.defaults.scalingMode=1;settings.profiles={{0},{1},{2}};
 ScalingModesViewModel view(loaded);
 auto& rows=view._scalingModes;
 std::weak_ptr<ScalingModeItem> weak;
 if(removed<loaded) weak=rows.values[removed].as<winrt::Magpie::ScalingModeItem>();
 rows.changed=[&](uint32_t index,bool after) {
  if(!after) {
   auto target=rows.values[index].as<winrt::Magpie::ScalingModeItem>();
   Require(target->_IsRemoved(),"binding callback queried removed group before its index was invalidated");
   target->VerifyRendered();
  } else {
   view.VerifyRendered();
   if(throughItem) Require(!weak.expired(),"deleting row lost its last strong reference inside Remove");
  }
 };
 if(throughItem) {
  auto* target=rows.values.at(removed).as<winrt::Magpie::ScalingModeItem>().get();
  target->Remove();
  Require(weak.expired(),"removed row was retained after synchronous deletion completed");
 } else ScalingModesService::Get().RemoveScalingMode(removed);
 Require(settings.modes.size()==2,"delete did not remove exactly one group");
 Require(settings.modes[0].effects==(removed==0?std::vector<FixtureEffect>{20}:std::vector<FixtureEffect>{10,11,12}),"wrong group removed for duplicate names");
 for(int i=0;i<3;++i) Require(settings.profiles[i].scalingMode==(i==static_cast<int>(removed)?-1:i>static_cast<int>(removed)?i-1:i),"profile index not remapped");
 view.VerifyRendered();
 ScalingModesService::Get().RemoveScalingMode(999);
 Require(settings.modes.size()==2,"invalid delete changed data");
}
int main() {
 try {
  DeleteCase(0,true);
  DeleteCase(1,true);
  DeleteCase(2,true);
  DeleteCase(0,false);
  DeleteCase(2,false,1);
  auto& settings=AppSettings::Get();auto& service=ScalingModesService::Get();
  settings.modes={{L"Same",{10,11}},{L"Same",{20}},{L"Same",{30,31,32}}};
  {
   ScalingModesViewModel view(3);
   view._scalingModes.changed=[&](uint32_t index,bool after) {
    if (!after) {
     auto target=view._scalingModes.values[index].as<winrt::Magpie::ScalingModeItem>();
     Require(target->_IsRemoved(),"sequential deletion exposed stale binding");
     target->VerifyRendered();
    } else view.VerifyRendered();
   };
   while(!settings.modes.empty()) {
    auto stale=view._scalingModes.values.front().as<winrt::Magpie::ScalingModeItem>();
    auto oldCount=settings.modes.size();
    stale->Remove();stale->Remove();
    Require(settings.modes.size()+1==oldCount,"stale delete removed another group");
    stale->VerifyRendered();
   }
   Require(view._scalingModes.Size()==0,"last group removal left a UI row");
  }
  settings.modes={{L"Same",{1}},{L"same",{2}},{L"Same (2)",{3}}};
  Require(service.HasDuplicateNames() && service.HasNameConflict(0),"legacy duplicates not detected");
  settings.PublishStartupNotice();
  Require(ErrorService::Get().duplicateNotices==1,"legacy upgrade notice not published");
  Require(!service.RenameScalingMode(0,L" SAME "),"duplicate rename was accepted");
  Require(!service.RenameScalingMode(0,L" \t"),"empty rename was accepted");
  Require(service.RenameScalingMode(0,L"Renamed"),"unique rename failed");
  Require(!service.HasDuplicateNames(),"resolved duplicate warning persists");
  Require(service.RenameScalingMode(0,L" renamed "),"self-only case change failed");
  Require(settings.modes[0].name==L"renamed","rename whitespace not normalized");
  service.AddScalingMode(L"SAME",1);
  Require(settings.modes.back().name==L"SAME (3)" && settings.modes.back().effects==std::vector<FixtureEffect>{2},"copy did not retain effects with a unique name");
  Require(settings.modes.back().runtimeIdentity != settings.modes[1].runtimeIdentity, "duplicate reused group runtime identity");
  Require(settings.modes.back().effects[0].runtimeIdentity != settings.modes[1].effects[0].runtimeIdentity, "duplicate reused effect runtime identity");
  size_t count=settings.modes.size();service.AddScalingMode(L" ",-1);service.AddScalingMode(L"Invalid",999);
  Require(settings.modes.size()==count,"invalid creation changed data");
  uint32_t renamed=0;
  ImportGroups({{L"Same",{4,5}},{L" same ",{6}}},false,&renamed);
  Require(renamed==2 && settings.modes[count].effects==std::vector<FixtureEffect>({4,5}) && settings.modes[count+1].effects==std::vector<FixtureEffect>{6},"explicit import lost groups or did not report renaming");
  Require(!service.HasDuplicateNames(),"explicit import created duplicate names");
  settings.modes.clear();
  ImportGroups({{L"Legacy",{7}},{L"Legacy",{8,9}}},true,&renamed);
  Require(renamed==0 && service.HasDuplicateNames() && settings.modes[0].effects==std::vector<FixtureEffect>{7} && settings.modes[1].effects==std::vector<FixtureEffect>({8,9}),"upgrade altered legacy group identity");
  Require(ScalingModeNames::Equal(L" 测试 ",L"测试"),"Chinese whitespace comparison failed");
  std::cout<<"Group identity: duplicate-name deletion, synchronous bindings/lifetime, late-loaded rows, profile remapping, rename/create/copy/import rules and upgrade notices passed.\n";
  return 0;
 } catch(const std::exception& e) { std::cerr<<e.what()<<"\n";return 1; }
}
'''
code=code.replace('PRODUCTION','\n\n'.join(parts))
(out/'group_identity.cpp').write_text(code,encoding='utf-8')
if not baseline:
 page=read('src/Magpie/ScalingModesPage.xaml');ET.fromstring(page)
 for text in ['ViewModel.HasDuplicateNames','HasNameConflict, Mode=OneWay','RenameProblem, Mode=OneWay']:
  assert text in page
 for language in ('en-US','zh-Hans','zh-Hant'):
  entries=ET.fromstring(read(f'src/Magpie/Resources.language-{language}.resw')).findall('data')
  keys=[entry.attrib['name'] for entry in entries]; assert len(keys)==len(set(keys))
  assert 'Message_DuplicateScalingModeNames' in keys and 'ScalingModes_NameAlreadyExists' in keys
 assert 'ScalingModeRemoving.Invoke(index)' in service
 assert 'ScalingModeRemoved(' not in item
 assert 'RenameScalingMode(_index' in block(item,'void ScalingModeItem::Name(const hstring&')
 assert 'RenameScalingMode(_index' in block(item,'void ScalingModeItem::RenameButton_Click(')
