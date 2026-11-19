import os
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "windows" / "assets"
TOOLCHAIN = ASSETS / "pspdev-win.zip"
RUNTIME = ASSETS / "msys-runtime.zip"
PROJECTS = [
    ROOT / "windows" / "tests" / "hello",
    ROOT / "windows" / "tests" / "el_sagrado_api",
]


def main():
    if not TOOLCHAIN.is_file() or not RUNTIME.is_file():
        raise SystemExit("Faltan los ZIP integrados de PSPDEV/MSYS2.")
    with tempfile.TemporaryDirectory(prefix="pspbuilder-bundle-test-") as temp:
        base = Path(temp)
        msys = base / "msys"
        psp_tmp = base / "pspdev.extracting"
        msys.mkdir()
        psp_tmp.mkdir()
        with zipfile.ZipFile(RUNTIME) as archive:
            archive.extractall(msys)
        with zipfile.ZipFile(TOOLCHAIN) as archive:
            archive.extractall(psp_tmp)
        psp_tmp.rename(msys / "pspdev")

        msys_bin = msys / "usr" / "bin"
        make = msys_bin / "make.exe"
        compiler = msys / "pspdev" / "bin" / "psp-gcc.exe"
        if not make.is_file() or not compiler.is_file():
            raise SystemExit("El bundle no contiene make.exe o psp-gcc.exe.")
        if not (msys_bin / "msys-2.0.dll").is_file():
            raise SystemExit("El bundle no contiene msys-2.0.dll.")

        env = os.environ.copy()
        env["PSPDEV"] = "/pspdev"
        env["MSYSTEM"] = "MSYS"
        env["MSYS2_PATH_TYPE"] = "inherit"
        env["MSYS2_ARG_CONV_EXCL"] = ""
        env["PATH"] = os.pathsep.join(
            [str(msys_bin), str(msys / "pspdev" / "bin"), env.get("PATH", "")]
        )
        env["MSYS2_PATH_TYPE"] = "inherit"
        env["MSYS2_ARG_CONV_EXCL"] = ""
        env["CHERE_INVOKING"] = "1"

        subprocess.run([str(compiler), "--version"], env=env, check=True)
        for project in PROJECTS:
            subprocess.run([str(make), "clean"], cwd=project, env=env, check=False)
            subprocess.run([str(make), "-j2"], cwd=project, env=env, check=True)
            eboot = project / "EBOOT.PBP"
            if not eboot.is_file() or eboot.stat().st_size <= 0:
                raise SystemExit(f"El bundle no ha generado un EBOOT.PBP válido para {project.name}.")
            print(f"BUNDLE_SMOKE_TEST_OK [{project.name}]: {eboot.stat().st_size} bytes")


if __name__ == "__main__":
    main()
