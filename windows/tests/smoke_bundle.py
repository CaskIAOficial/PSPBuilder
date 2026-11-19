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
        env["CHERE_INVOKING"] = "1"
        env["TMPDIR"] = "/tmp"
        env["TMP"] = "/tmp"
        env["TEMP"] = "/tmp"
        # Set COLLECT_GCC in the native Windows parent environment, using the
        # absolute Windows path. This avoids relying on MSYS shell export or
        # POSIX path translation for the LTO helper process.
        env["COLLECT_GCC"] = str(compiler)
        (msys / "tmp").mkdir(parents=True, exist_ok=True)
        bash = msys_bin / "bash.exe"
        if not bash.is_file():
            raise SystemExit("El bundle no contiene bash.exe.")

        subprocess.run([str(compiler), "--version"], env=env, check=True)
        diagnostic = r"""
printf 'BASH_COLLECT_GCC=<%s>\\n' "${COLLECT_GCC-UNSET}"
printf 'BASH_COLLECT_GCC_OPTIONS=<%s>\\n' "${COLLECT_GCC_OPTIONS-UNSET}"
printf 'BASH_PATH=<%s>\\n' "$PATH"
printf 'BASH_COMPILER=<%s>\\n' "$(command -v psp-gcc)"
printf 'COLLECT2_PATH=<%s>\\n' "$(psp-gcc -print-prog-name=collect2)"
printf 'LTO_WRAPPER_PATH=<%s>\\n' "$(psp-gcc -print-prog-name=lto-wrapper)"
printf 'LTO_PLUGIN_PATH=<%s>\\n' "$(psp-gcc -print-file-name=liblto_plugin-0.dll)"
"""
        subprocess.run(
            [str(bash), "--noprofile", "--norc", "-c", diagnostic],
            cwd=ROOT, env=env, check=True
        )
        for project in PROJECTS:
            subprocess.run(
                [str(bash), "--noprofile", "--norc", "-c", "exec make clean"],
                cwd=project, env=env, check=False
            )
            subprocess.run(
                [str(bash), "--noprofile", "--norc", "-c", "exec make -j2"],
                cwd=project, env=env, check=True
            )
            eboot = project / "EBOOT.PBP"
            if not eboot.is_file() or eboot.stat().st_size <= 0:
                raise SystemExit(f"El bundle no ha generado un EBOOT.PBP válido para {project.name}.")
            print(f"BUNDLE_SMOKE_TEST_OK [{project.name}]: {eboot.stat().st_size} bytes")


if __name__ == "__main__":
    main()
