"""Extract actual WrapPanel row packing and the existing restart/elevation gate.

The fixtures replace UI measurement and ShellExecute; no product or UAC starts.
"""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1])
header = (repo / 'src/Magpie/WrapPanel.h').read_text(encoding='utf-8-sig')
source = (repo / 'src/Magpie/WrapPanel.cpp').read_text(encoding='utf-8-sig')
models = header[header.index('struct UvMeasure {'):header.index('struct WrapPanel :')]
start = source.index('Size WrapPanel::_UpdateRows(')
end = source.index('\n}', start) + 2
(out / 'RtssWrapLayout.g.h').write_text(models, encoding='utf-8')
(out / 'RtssWrapRows.g.h').write_text(source[start:end], encoding='utf-8')
app = (repo / 'src/Magpie/App.cpp').read_text(encoding='utf-8-sig')
start = app.index('void App::Restart(bool asElevated,')
end = app.index('\n}', start) + 2
(out / 'RtssRestart.g.h').write_text(app[start:end], encoding='utf-8')
start = app.index('\tif (settings.IsAlwaysRunAsAdmin() && !Win32Helper::IsProcessElevated())')
end = app.index('\n\tsettings.UpdateAdminAutoStart();', start)
gate = app[start:end]
(out / 'RtssStartupGate.g.h').write_text('bool App::StartupGate(const wchar_t* arguments) {\n' + gate + '\nreturn true;\n}', encoding='utf-8')
settings = (repo / 'src/Magpie/AppSettings.cpp').read_text(encoding='utf-8-sig')
start = settings.index('void AppSettings::UpdateAdminAutoStart() noexcept {')
end = settings.index('\n}', start) + 2
(out / 'RtssAdminAutoStart.g.h').write_text(settings[start:end], encoding='utf-8')
print('Extracted unchanged production row packing, elevation gate and restart.')
