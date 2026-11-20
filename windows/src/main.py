import ctypes
import os
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import zipfile
from pathlib import Path, PurePosixPath, PureWindowsPath
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

APP_NAME = "PSP Builder"
MAX_FILES = 10_000
MAX_UNPACKED = 300 * 1024 * 1024
CHUNK = 1024 * 1024
BUILD_TIMEOUT_SECONDS = 30 * 60


def resource_path(name: str) -> Path:
    base = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
    return base / "assets" / name


def short_path(path: Path) -> str:
    """Return an 8.3 path where Windows provides one (helps toolchains with path limits)."""
    if os.name != "nt":
        return str(path)
    buf = ctypes.create_unicode_buffer(32768)
    get_short = ctypes.windll.kernel32.GetShortPathNameW
    result = get_short(str(path), buf, len(buf))
    return buf.value if result else str(path)


def app_data_dir() -> Path:
    base = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local"))
    target = base / "PSPBuilder"
    target.mkdir(parents=True, exist_ok=True)
    return target


def safe_extract_zip(archive: Path, destination: Path, log):
    destination.mkdir(parents=True, exist_ok=True)
    total = 0
    count = 0
    with zipfile.ZipFile(archive, "r") as zf:
        for info in zf.infolist():
            if info.is_dir():
                continue
            count += 1
            if count > MAX_FILES:
                raise RuntimeError(f"El ZIP supera el límite de {MAX_FILES:,} archivos.")
            name = info.filename.replace("\\", "/")
            posix = PurePosixPath(name)
            win = PureWindowsPath(name)
            if posix.is_absolute() or win.is_absolute() or win.drive or any(p == ".." for p in posix.parts):
                raise RuntimeError(f"El ZIP contiene una ruta no segura: {info.filename}")
            # Reject Unix symlinks stored in ZIP metadata.
            mode = (info.external_attr >> 16) & 0xFFFF
            if (mode & 0o170000) == 0o120000:
                raise RuntimeError(f"El ZIP contiene un enlace simbólico no permitido: {info.filename}")
            total += info.file_size
            if total > MAX_UNPACKED:
                raise RuntimeError("El contenido descomprimido supera el límite de 300 MB.")
            target = (destination / Path(*posix.parts)).resolve()
            if not target.is_relative_to(destination.resolve()):
                raise RuntimeError(f"Ruta de extracción fuera del proyecto: {info.filename}")
            target.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(info, "r") as source, target.open("wb") as output:
                shutil.copyfileobj(source, output, CHUNK)
    log(f"ZIP extraído de forma segura: {count} archivos, {total / (1024 * 1024):.1f} MB.")


def locate_makefile(root: Path) -> Path:
    candidates = list(root.rglob("Makefile")) + list(root.rglob("makefile"))
    candidates = [p for p in candidates if p.is_file()]
    if not candidates:
        raise RuntimeError("No se ha encontrado ningún Makefile dentro del ZIP.")
    # Prefer the shallowest Makefile whose project tree contains a C/C++ source file.
    candidates.sort(key=lambda p: (len(p.relative_to(root).parts), str(p).lower()))
    for candidate in candidates:
        folder = candidate.parent
        if any(folder.rglob("*.c")) or any(folder.rglob("*.cpp")) or any(folder.rglob("*.cc")):
            return candidate
    return candidates[0]


class PSPBuilderApp:
    def __init__(self, root):
        self.root = root
        self.root.title("PSP BUILDER")
        self.root.geometry("900x650")
        self.root.minsize(760, 560)
        self.root.configure(bg="#101713")
        self.events = queue.Queue()
        self.zip_path = None
        self.busy = False
        self.output_dir = Path.home() / "Documents" / "PSP Builder" / "EBOOT"
        self._build_ui()
        self.root.after(100, self._poll_events)

    def _build_ui(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TProgressbar", troughcolor="#243129", background="#8fe0a8", bordercolor="#243129")
        header = tk.Frame(self.root, bg="#101713")
        header.pack(fill="x", padx=28, pady=(24, 8))
        tk.Label(header, text="PSP BUILDER", font=("Segoe UI", 27, "bold"),
                 fg="#9ce8b5", bg="#101713").pack(anchor="w")
        tk.Label(header, text="Compila tus proyectos para PlayStation Portable",
                 font=("Segoe UI", 11), fg="#cad7ce", bg="#101713").pack(anchor="w", pady=(3, 0))

        panel = tk.Frame(self.root, bg="#18231c", highlightbackground="#304638", highlightthickness=1)
        panel.pack(fill="x", padx=28, pady=18)
        tk.Label(panel, text="PROYECTO PSP", font=("Segoe UI", 10, "bold"),
                 fg="#8fe0a8", bg="#18231c").pack(anchor="w", padx=18, pady=(16, 6))
        row = tk.Frame(panel, bg="#18231c")
        row.pack(fill="x", padx=18, pady=(0, 14))
        self.file_label = tk.Label(row, text="Ningún ZIP seleccionado", anchor="w",
                                   font=("Segoe UI", 10), fg="#f1f5f2", bg="#18231c")
        self.file_label.pack(side="left", fill="x", expand=True, padx=(0, 12))
        self.choose_btn = tk.Button(row, text="ELEGIR ZIP", command=self.choose_zip,
                                    bg="#304638", fg="#ffffff", activebackground="#42614b",
                                    activeforeground="#ffffff", relief="flat", padx=18, pady=10,
                                    font=("Segoe UI", 9, "bold"), cursor="hand2")
        self.choose_btn.pack(side="right")

        self.compile_btn = tk.Button(self.root, text="COMPILAR EBOOT.PBP", command=self.start_compile,
                                     bg="#9ce8b5", fg="#102016", activebackground="#b2f1c5",
                                     activeforeground="#102016", relief="flat", padx=22, pady=15,
                                     font=("Segoe UI", 12, "bold"), cursor="hand2", state="disabled")
        self.compile_btn.pack(fill="x", padx=28, pady=(0, 14))

        status_row = tk.Frame(self.root, bg="#101713")
        status_row.pack(fill="x", padx=28)
        self.status = tk.Label(status_row, text="Preparando PSPDEV la primera vez puede tardar unos minutos.",
                               anchor="w", font=("Segoe UI", 10), fg="#9ce8b5", bg="#101713")
        self.status.pack(side="left", fill="x", expand=True)
        self.progress = ttk.Progressbar(status_row, mode="indeterminate", length=150)
        self.progress.pack(side="right", padx=(10, 0))

        tk.Label(self.root, text="REGISTRO DE COMPILACIÓN", font=("Segoe UI", 9, "bold"),
                 fg="#8fe0a8", bg="#101713").pack(anchor="w", padx=28, pady=(18, 6))
        log_frame = tk.Frame(self.root, bg="#0b100d")
        log_frame.pack(fill="both", expand=True, padx=28, pady=(0, 12))
        self.log_text = tk.Text(log_frame, bg="#0b100d", fg="#d3dfd6", insertbackground="#ffffff",
                                relief="flat", wrap="word", font=("Consolas", 9), padx=12, pady=10,
                                state="disabled")
        scrollbar = ttk.Scrollbar(log_frame, command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=scrollbar.set)
        self.log_text.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        footer = tk.Frame(self.root, bg="#101713")
        footer.pack(fill="x", padx=28, pady=(0, 20))
        tk.Button(footer, text="ABRIR CARPETA DE EBOOT", command=self.open_output,
                  bg="#24352a", fg="#eaf4ed", activebackground="#304638",
                  relief="flat", padx=14, pady=9, font=("Segoe UI", 9, "bold"),
                  cursor="hand2").pack(side="left")
        tk.Label(footer, text="Windows 10/11 · PSPDEV integrado",
                 fg="#718078", bg="#101713", font=("Segoe UI", 8)).pack(side="right")

    def log(self, message):
        self.events.put(("log", message))

    def set_status(self, message):
        self.events.put(("status", message))

    def _poll_events(self):
        try:
            while True:
                kind, value = self.events.get_nowait()
                if kind == "log":
                    self.log_text.configure(state="normal")
                    self.log_text.insert("end", value + "\n")
                    self.log_text.see("end")
                    self.log_text.configure(state="disabled")
                elif kind == "status":
                    self.status.configure(text=value)
                elif kind == "ready":
                    self.busy = False
                    self.progress.stop()
                    self.choose_btn.configure(state="normal")
                    self.compile_btn.configure(state="normal" if self.zip_path else "disabled")
                elif kind == "done":
                    self.busy = False
                    self.progress.stop()
                    self.choose_btn.configure(state="normal")
                    self.compile_btn.configure(state="normal" if self.zip_path else "disabled")
                    messagebox.showinfo("PSP Builder", value)
                elif kind == "error":
                    self.busy = False
                    self.progress.stop()
                    self.choose_btn.configure(state="normal")
                    self.compile_btn.configure(state="normal" if self.zip_path else "disabled")
                    messagebox.showerror("No se pudo compilar", value)
        except queue.Empty:
            pass
        self.root.after(100, self._poll_events)

    def choose_zip(self):
        selected = filedialog.askopenfilename(
            title="Selecciona el ZIP del proyecto PSP",
            filetypes=[("Archivos ZIP", "*.zip"), ("Todos los archivos", "*.*")]
        )
        if selected:
            self.zip_path = Path(selected)
            self.file_label.configure(text=self.zip_path.name)
            self.compile_btn.configure(state="normal")
            self.log(f"Proyecto seleccionado: {self.zip_path}")

    def start_compile(self):
        if not self.zip_path or not self.zip_path.is_file():
            messagebox.showwarning("PSP Builder", "Selecciona primero un ZIP válido.")
            return
        if self.busy:
            return
        self.busy = True
        self.choose_btn.configure(state="disabled")
        self.compile_btn.configure(state="disabled")
        self.progress.start(12)
        self.set_status("Preparando el compilador...")
        threading.Thread(target=self._compile_worker, daemon=True).start()

    def _ensure_runtime(self):
        base = app_data_dir() / "runtime"
        msys = base / "msys"
        # PSPDEV's psp-config uses the MSYS prefix /pspdev; keep the toolchain
        # inside the private MSYS root so that this prefix resolves correctly.
        pspdev = msys / "pspdev"
        make = msys / "usr" / "bin" / "make.exe"
        compiler = pspdev / "bin" / "psp-gcc.exe"
        if compiler.exists() and make.exists():
            return pspdev, msys

        self.log("Primera ejecución: preparando el compilador integrado...")
        base.mkdir(parents=True, exist_ok=True)
        tool_zip = resource_path("pspdev-win.zip")
        runtime_zip = resource_path("msys-runtime.zip")
        if not tool_zip.is_file() or not runtime_zip.is_file():
            raise RuntimeError("Faltan los componentes integrados de PSPDEV. Descarga una compilación completa de PSP Builder.")
        psp_tmp = base / "pspdev.extracting"
        msys_tmp = base / "msys.extracting"
        shutil.rmtree(psp_tmp, ignore_errors=True)
        shutil.rmtree(msys_tmp, ignore_errors=True)
        psp_tmp.mkdir()
        msys_tmp.mkdir()
        self.set_status("Instalando PSPDEV integrado (solo la primera vez)...")
        with zipfile.ZipFile(tool_zip) as zf:
            zf.extractall(psp_tmp)
        with zipfile.ZipFile(runtime_zip) as zf:
            zf.extractall(msys_tmp)
        # The Windows PSPDEV port resolves its SDK prefix as /pspdev in MSYS.
        (msys_tmp / "pspdev").parent.mkdir(parents=True, exist_ok=True)
        psp_tmp.rename(msys_tmp / "pspdev")
        # Replace only this application's own runtime folder.
        shutil.rmtree(msys, ignore_errors=True)
        msys_tmp.rename(msys)
        if not compiler.exists() or not make.exists():
            raise RuntimeError("El paquete de compilación está incompleto: no se encuentra psp-gcc o make.")
        self.log("PSPDEV y sus herramientas auxiliares están preparados.")
        return pspdev, msys

    def _compile_worker(self):
        temp_root = None
        log_lines = []

        def record(message):
            log_lines.append(str(message))
            self.log(str(message))

        try:
            pspdev, msys = self._ensure_runtime()
            temp_root = Path(tempfile.mkdtemp(prefix="pspbuilder-project-"))
            project_root = temp_root / "source"
            project_root.mkdir()
            self.set_status("Inspeccionando y extrayendo el ZIP...")
            record("Comprobando la estructura del ZIP...")
            safe_extract_zip(self.zip_path, project_root, record)
            makefile = locate_makefile(project_root)
            project_dir = makefile.parent
            record(f"Makefile encontrado: {makefile.relative_to(project_root)}")
            if not (any(project_dir.rglob("*.c")) or any(project_dir.rglob("*.cpp")) or any(project_dir.rglob("*.cc"))):
                record("Aviso: no se detectan fuentes C/C++; se intentará compilar igualmente.")
            msys_bin = msys / "usr" / "bin"
            # The portable MSYS2 bundle does not preserve an empty /tmp directory
            # in ZIP archives. Create it before starting bash/make to avoid MSYS
            # startup warnings and ensure temporary files can be created.
            msys_tmp = msys / "tmp"
            msys_tmp.mkdir(parents=True, exist_ok=True)
            env = os.environ.copy()
            env["PSPDEV"] = "/pspdev"
            # PSPDEV's Windows toolchain documents its bin directory before MSYS2 usr/bin.
            # This precedence is required for the bundled GCC/LTO helper chain.
            env["PATH"] = str(pspdev / "bin") + os.pathsep + str(msys_bin) + os.pathsep + env.get("PATH", "")
            env["MSYSTEM"] = "MSYS"
            env["MSYS2_PATH_TYPE"] = "inherit"
            env["MSYS2_ARG_CONV_EXCL"] = ""
            env["CHERE_INVOKING"] = "1"
            env["HOME"] = str(app_data_dir())
            env["LANG"] = "C.UTF-8"
            env["TMPDIR"] = "/tmp"
            env["TMP"] = "/tmp"
            env["TEMP"] = "/tmp"
            bash_exe = msys_bin / "bash.exe"
            make_command = "exec make -j2"
            self.set_status("Compilando el proyecto PSP...")
            record("Ejecutando make -j2. No cierres PSP Builder.")
            creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
            process = subprocess.Popen(
                [str(bash_exe), "--noprofile", "--norc", "-c", make_command],
                cwd=str(project_dir), env=env,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace",
                bufsize=1, creationflags=creationflags
            )

            # Read compiler output separately so the worker can still detect a
            # process that has stopped producing output or is waiting forever.
            output_queue = queue.Queue()

            def read_output():
                try:
                    for line in process.stdout:
                        output_queue.put(line.rstrip())
                finally:
                    output_queue.put(None)

            reader = threading.Thread(target=read_output, daemon=True)
            reader.start()
            started = time.monotonic()
            last_status = started
            last_log = started
            reader_finished = False

            while True:
                try:
                    output = output_queue.get(timeout=1.0)
                except queue.Empty:
                    output = ""

                if output is None:
                    reader_finished = True
                elif output:
                    record(output)

                code = process.poll()
                now = time.monotonic()
                elapsed = int(now - started)

                if code is not None and reader_finished:
                    break

                if elapsed >= BUILD_TIMEOUT_SECONDS:
                    record(
                        f"ERROR: make sigue activo después de "
                        f"{BUILD_TIMEOUT_SECONDS // 60} minutos. Se va a detener."
                    )
                    if os.name == "nt":
                        try:
                            subprocess.run(
                                ["taskkill", "/PID", str(process.pid), "/T", "/F"],
                                stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL,
                                check=False,
                                timeout=10,
                                creationflags=creationflags
                            )
                        except Exception as kill_error:
                            record(f"No se pudo cerrar el árbol de procesos con taskkill: {kill_error}")
                    else:
                        try:
                            process.terminate()
                        except Exception:
                            pass
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
                    raise RuntimeError(
                        f"La compilación se ha detenido porque superó el límite de "
                        f"{BUILD_TIMEOUT_SECONDS // 60} minutos. Revisa el registro guardado."
                    )

                if now - last_status >= 15:
                    self.set_status(
                        f"Compilando... {elapsed // 60} min {elapsed % 60:02d} s. "
                        "El compilador sigue en ejecución."
                    )
                    last_status = now

                if now - last_log >= 60:
                    record(
                        f"Compilación en curso ({elapsed // 60} min {elapsed % 60:02d} s)."
                    )
                    last_log = now

            code = process.wait()
            if code != 0:
                raise RuntimeError(f"make terminó con código {code}. Revisa el registro para ver el error.")
            eboots = [p for p in project_dir.rglob("EBOOT.PBP") if p.is_file() and p.stat().st_size > 0]
            if not eboots:
                raise RuntimeError("La compilación terminó, pero no se ha encontrado un EBOOT.PBP válido.")
            source_eboot = max(eboots, key=lambda p: p.stat().st_mtime)
            self.output_dir.mkdir(parents=True, exist_ok=True)
            stem = self.zip_path.stem
            stamp = time.strftime("%Y%m%d-%H%M%S")
            final_dir = self.output_dir / f"{stem}-{stamp}"
            final_dir.mkdir(parents=True, exist_ok=False)
            shutil.copy2(source_eboot, final_dir / "EBOOT.PBP")
            record(f"EBOOT.PBP generado correctamente ({(final_dir / 'EBOOT.PBP').stat().st_size:,} bytes).")
            record(f"Guardado en: {final_dir}")
            (final_dir / "compilacion.log").write_text("\n".join(log_lines) + "\n", encoding="utf-8")
            self.set_status("Compilación completada correctamente.")
            self.events.put(("done", f"EBOOT.PBP generado correctamente.\n\nCarpeta:\n{final_dir}"))
        except Exception as exc:
            record(f"ERROR: {exc}")
            try:
                logs_dir = app_data_dir() / "logs"
                logs_dir.mkdir(parents=True, exist_ok=True)
                log_name = f"fallo-{time.strftime('%Y%m%d-%H%M%S')}.log"
                (logs_dir / log_name).write_text("\n".join(log_lines) + "\n", encoding="utf-8")
                record(f"Registro guardado en: {logs_dir / log_name}")
            except Exception:
                pass
            self.set_status("La compilación no se ha completado.")
            self.events.put(("error", str(exc)))
        finally:
            if temp_root:
                shutil.rmtree(temp_root, ignore_errors=True)

    def open_output(self):
        self.output_dir.mkdir(parents=True, exist_ok=True)
        os.startfile(str(self.output_dir))


def main():
    root = tk.Tk()
    PSPBuilderApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
