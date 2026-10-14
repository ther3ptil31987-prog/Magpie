"""Extract current GPU view-model behavior for a real WinRT collection harness.

Resource loading, notifications and persistence are doubles. The tests use
real boxed IInspectable objects and IVector identity, with no Magpie startup.
"""
from pathlib import Path
import re
import sys

repo = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
source = (repo / 'src/Magpie/ProfileViewModel.cpp').read_text(encoding='utf-8-sig')
names = ('GraphicsCards', '_BuildGraphicsCards', 'GraphicsCard', 'IsGraphicsCardSelectionEnabled',
         'IsNoGraphicsCard', 'ShowGraphicsCardUnavailableWarning', '_RefreshGraphicsCardSettings')
declarations, definitions = [], []
for name in names:
    matches = list(re.finditer(r'^(?:IVector<IInspectable>|IInspectable|void|bool) ProfileViewModel::'
                              + name + r'\([^\n]*\{', source, re.M))
    if not matches:
        raise RuntimeError(f'Missing production method: {name}')
    for match in matches:
        declarations.append(match[0][:-1].strip().replace('ProfileViewModel::', '') + ';')
        end, depth = match.end(), 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        definitions.append(source[match.start():end])
(out / 'RenderingGpuViewModelDeclarations.inc').write_text('\n'.join(declarations), encoding='utf-8')
(out / 'RenderingGpuViewModelProduction.inc').write_text('\n\n'.join(definitions), encoding='utf-8')
print('Extracted current rendering GPU list, selection, missing-state and notification behavior.')
