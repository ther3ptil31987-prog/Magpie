"""Source/translation contracts; does not claim native UI or GPU acceptance."""
from pathlib import Path
import xml.etree.ElementTree as ET

repo = Path(__file__).resolve().parents[1]
def read(path):
    return (repo / path).read_text(encoding='utf-8-sig')

x = '{http://schemas.microsoft.com/winfx/2006/xaml}'
local = '{using:Magpie}'
root = ET.fromstring(read('src/Magpie/ProfilePage.xaml'))
general = next(n for n in root.iter() if n.get(x + 'Uid') == 'Profile_General')
performance = next(n for n in root.iter() if n.get(x + 'Uid') == 'Profile_Performance')
children = list(performance)
card = next(n for n in children if n.get(x + 'Uid') == 'Profile_General_GraphicsCard')
assert children[0] == card
assert not any(n.get(x + 'Uid') == 'Profile_General_GraphicsCard' for n in general.iter())
groups = [n.get(x + 'Uid') for n in root.iter(local + 'SettingsGroup')]
assert groups.index('Profile_WindowedScaling') + 1 == groups.index('Profile_Performance')
parent = next(n for n in root.iter() if performance in list(n))
assert list(parent)[list(parent).index(performance)+1].get(x+'Name') == 'FrameRefreshGroup'
assert {n.get(x + 'Uid') for n in children[1:]} == {'Profile_General_NoGraphicsCard', 'Profile_General_GraphicsCardUnavailable'}
assert card.get(x + 'Load') is None and card.get('Visibility') is None
assert card.get('IsWrapEnabled') == 'True'
combo = next(n for n in card if n.tag.endswith('ComboBox'))
assert combo.get('SelectedItem') == '{x:Bind ViewModel.GraphicsCard, Mode=TwoWay}'
assert combo.get('SelectedIndex') is None
assert combo.get('IsEnabled') == '{x:Bind ViewModel.IsGraphicsCardSelectionEnabled, Mode=OneWay}'
vm = read('src/Magpie/ProfileViewModel.cpp')
assert '_BuildMonitorOptions();\n\t_BuildGraphicsCards();' in vm
assert 'IsShowGraphicsCardSettingsCard' not in vm
idl = read('src/Magpie/ProfileViewModel.idl')
assert 'IInspectable GraphicsCard;' in idl
keys = ('Profile_General_GraphicsCard.Header',
        'Profile_General_GraphicsCard_Input.[using:Windows.UI.Xaml.Automation]AutomationProperties.Name',
        'Profile_General_GraphicsCard_Auto', 'Profile_General_GraphicsCard_None',
        'Profile_General_GraphicsCard_Unavailable', 'Profile_General_NoGraphicsCard.Title',
        'Profile_General_NoGraphicsCard.Message', 'Profile_General_GraphicsCardUnavailable.Title',
        'Profile_General_GraphicsCardUnavailable.Message')
files = list((repo / 'src/Magpie').glob('Resources.language-*.resw'))
for path in files:
    data = ET.parse(path).getroot().findall('data')
    resources = {n.get('name'): n.findtext('value') for n in data}
    assert len(resources) == len(data), path.name
    for key in keys:
        assert resources[key], (path.name, key)
    assert 'Performance → Graphics card' not in path.read_text(encoding='utf-8-sig')
    assert '配置 → 性能 → 显示卡' not in path.read_text(encoding='utf-8-sig')
    assert 'General → Rendering GPU' not in path.read_text(encoding='utf-8-sig')
    assert '配置 → 通用 → 渲染显卡' not in path.read_text(encoding='utf-8-sig')
print(f'Rendering GPU card placement, identity binding and 9 resources in {len(files)} languages passed.')
