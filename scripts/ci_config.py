"""Compile the tracked placeholder fixture without touching developer configuration."""
from pathlib import Path

Import("env")

project = Path(env.subst("$PROJECT_DIR"))
generated = Path(env.subst("$BUILD_DIR")) / "placeholder"
generated.mkdir(parents=True, exist_ok=True)
fixture = generated / "env.cpp"
fixture.write_text((project / "src/infrastructure/env.cpp.example").read_text(encoding="utf-8"), encoding="utf-8")
env.Append(CPPPATH=[str(project / "include")])
library = env.BuildLibrary(str(generated / "objects"), str(generated))
env.Append(LIBS=[library])
