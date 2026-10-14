"""Extract the production scene copy. Generation does not execute C++/GPU code."""
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
source = (repo / "src/Magpie.Core/Renderer.cpp").read_text(encoding="utf-8-sig")
start = source.index("void Renderer::_CopySceneToTarget(")
end = source.index("{", start) + 1
depth = 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
template = (repo / "tests/VRRPerformanceTests.cpp").read_text(encoding="utf-8-sig")
assert template.count("/* PRODUCTION_SCENE_COPY */") == 1
(output / "VRRPerformanceTests.cpp").write_text(
    template.replace("/* PRODUCTION_SCENE_COPY */", source[start:end]), encoding="utf-8")
print("Generated production VRR performance/copy fixture (not compiled or executed).")
