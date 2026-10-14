"""Compile the production profile transactions without app discovery or WinUI.

Only tool discovery/launch is omitted. The Win32 file I/O, SEH guards, DLL
initialization and complete Apply/Restore bodies are copied without edits.
Private access is exposed in the generated test header for export injection.
"""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
source = (repo / "src/Magpie/RtssProfileAccess.cpp").read_text(encoding="utf-8-sig")
header = (repo / "src/Magpie/RtssProfileAccess.h").read_text(encoding="utf-8-sig")
source = source.replace('#include "pch.h"\n', "")
source = source.replace('#include "Win32Helper.h"\n', "")
source = source.replace('#include "RtssProfileAccess.h"', '#include "RtssProfileAccess.g.h"')
source = '#ifdef RTSS_TEST_FILE_COMMIT\n#include "RtssFileCommitHooks.g.h"\n#endif\n' + source
start = source.index("bool IsRunning(")
end = source.index("RtssProfileAccess::~RtssProfileAccess()")
name_start = source.index("std::string RtssProfileAccess::ProfileName(")
name_end = source.index("\n}\n", name_start) + 2
name_method = source[name_start:name_end]
source = source[:start] + "}\n\n" + name_method + "\n\n" + source[end:]
source = '#include <windows.h>\n#include <filesystem>\nnamespace Magpie::Win32Helper { std::filesystem::path GetExePath() {\n' + \
    'wchar_t path[32768]{}; GetModuleFileNameW(nullptr, path, 32768); return path; } }\n' + source
service = (repo / "src/Magpie/RtssService.cpp").read_text(encoding="utf-8-sig")
start = service.index("const wchar_t* ProfileFailureText(")
end = service.index("\n}\n", start) + 2
args.output.mkdir(parents=True, exist_ok=True)
(args.output / "RtssProfileAccess.g.h").write_text(header.replace("private:", "public:"), encoding="utf-8")
(args.output / "RtssProfileAccess.g.cpp").write_text(source, encoding="utf-8")
(args.output / "RtssProfileExpression.h").write_text((repo / "src/Magpie/RtssProfileExpression.h").read_text(encoding="utf-8-sig"), encoding="utf-8")
(args.output / "RtssFileCommitHooks.g.h").write_text(
    '#include <windows.h>\nBOOL RtssTestReplaceFileW(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPVOID, LPVOID);\n'
    '#define ReplaceFileW RtssTestReplaceFileW\n', encoding="utf-8")
(args.output / "RtssFailureText.g.h").write_text(service[start:end], encoding="utf-8")
enum_source = (repo / "src/Magpie/RtssService.h").read_text(encoding="utf-8-sig")
enum_start = enum_source.index("enum class RtssState {")
status_end = enum_source.index("\n};", enum_source.index("struct RtssStatus {")) + 3
session_start = service.index("struct RtssService::Session {")
session_end = service.index("\n};", session_start) + 3
methods = []
for name in ("_TryApply", "_Finish", "_InitializeMagpie", "_Prepare", "_RefreshMagpieInitialization", "_PrepareSession", "_PublishSession", "_CheckSession"):
    method_start = service.index(("bool" if name in ("_Prepare", "_PrepareSession") else "void") + " RtssService::" + name + "(")
    method_end = service.index("\n}", method_start) + 2
    methods.append(service[method_start:method_end])
(args.output / "RtssSessionCases.g.h").write_text(enum_source[enum_start:status_end], encoding="utf-8")
(args.output / "RtssSessionTransactions.g.h").write_text(service[session_start:session_end] + "\n" + service[service.index("bool Incompatible("):service.index("\n}\n", service.index("bool Incompatible(")) + 2] + "\n" + service[service.index("void SetAccessStatus("):service.index("\n}\n", service.index("void SetAccessStatus(")) + 2] + "\n" + service[service.index("void SetRateStatus("):service.index("\n}\n", service.index("void SetRateStatus(")) + 2] + "\n" + "\n".join(methods), encoding="utf-8")
print("Extracted complete production RTSS profile transactions, Win32 I/O, SEH and status mapping.")
