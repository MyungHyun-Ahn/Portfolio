"""Build and run the standalone Windows x64 receive-completion fixture."""

import argparse
import os
from pathlib import Path
import re
import shutil
from statistics import median
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
NETWORK = ROOT / "Common" / "NetworkCommon"
SERVER = ROOT / "Server" / "NetworkLib"
MHLIB = ROOT / "Server" / "MHLib" / "includes"


def x64_environment():
    env = os.environ.copy()
    installer = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio" / "Installer"
    location = subprocess.check_output(
        [str(installer / "vswhere.exe"), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath", "-utf8"],
        encoding="utf-8",
        timeout=30,
    ).strip()
    if not location:
        raise RuntimeError("Visual Studio with MSVC x64 tools was not found")
    vcvars = Path(location) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    cmd = env.get("COMSPEC", "cmd.exe")
    # CMD의 바깥쪽 인용부호는 argv 직렬화로 전달하지 않는다.
    raw = f'"{cmd}" /d /v:off /s /c "call "{vcvars}" >nul && set"'
    result = subprocess.check_output(raw, env=env, encoding="mbcs", timeout=60)
    env = {key.upper(): value for key, value in
           (line.split("=", 1) for line in result.splitlines() if "=" in line and not line.startswith("="))}
    if not any((Path(entry) / "WinSock2.h").is_file() for entry in env.get("INCLUDE", "").split(";") if entry):
        add_windows_sdk(env)
    return env


def add_windows_sdk(env):
    import winreg

    # vcvars64가 MSVC만 설정한 경우 설치된 완전한 SDK의 헤더/라이브러리 쌍을 사용한다.
    registry_key = r"SOFTWARE\Microsoft\Windows Kits\Installed Roots"
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, registry_key, 0, winreg.KEY_READ | winreg.KEY_WOW64_32KEY) as key:
        kits = Path(winreg.QueryValueEx(key, "KitsRoot10")[0])
    versions = [path for path in (kits / "Include").glob("10.*") if all(part.isdecimal() for part in path.name.split("."))]
    for include in sorted(versions, key=lambda path: tuple(map(int, path.name.split("."))), reverse=True):
        lib = kits / "Lib" / include.name
        required = (include / "um" / "WinSock2.h", include / "shared" / "sdkddkver.h", include / "ucrt" / "stdio.h",
                    lib / "um" / "x64" / "Ws2_32.Lib", lib / "ucrt" / "x64" / "ucrt.lib")
        if all(path.is_file() for path in required):
            env["INCLUDE"] = ";".join([env.get("INCLUDE", ""), *(str(include / part) for part in ("ucrt", "shared", "um"))])
            env["LIB"] = ";".join([env.get("LIB", ""), str(lib / "ucrt" / "x64"), str(lib / "um" / "x64")])
            print(f"Using Windows SDK fallback {include.name}", flush=True)
            return
    raise RuntimeError(f"No complete Windows x64 SDK found in {kits}")


def benchmark(exe, run, env, producers, groups, samples):
    records = re.compile(r"BENCH path=(ring|view) packets=(\d+) wall_ns=(\d+) cpu_100ns=(\d+)")
    expected = producers * groups * 4 * 12
    for mode in ("Net", "Lan"):
        for layout in ("packed", "split"):
            results = {"ring": [], "view": []}
            for sample in range(samples):
                # 새 프로세스로 TLS 슬롯을 초기화하고 두 경로의 실행 순서를 번갈아 바꾼다.
                view_first = sample % 2
                config = (f"ServerRecvView:\r\n    - Producers = {producers};\r\n    - Groups = {groups};\r\n"
                          f"    - Benchmark = 1;\r\n    - Net = {int(mode == 'Net')};\r\n"
                          f"    - Packed = {int(layout == 'packed')};\r\n    - ViewFirst = {view_first};\r\n")
                (run / "ServerRecvView.ini").write_bytes(config.encode("utf-16"))
                result = subprocess.run([str(exe)], cwd=run, env=env, check=True, timeout=60,
                                        stdout=subprocess.PIPE, encoding="utf-8", errors="replace")
                captured = records.findall(result.stdout)
                order = ["view", "ring"] if view_first else ["ring", "view"]
                if [record[0] for record in captured] != order:
                    raise RuntimeError(f"Invalid benchmark results: {result.stdout!r}")
                for path, packets, wall_ns, cpu_ticks in captured:
                    if int(packets) != expected or min(int(wall_ns), int(cpu_ticks)) <= 0:
                        raise RuntimeError(f"Invalid benchmark counters: {result.stdout!r}")
                    results[path].append((int(cpu_ticks) * 100 / expected, int(wall_ns) / expected))

            ring, view = (tuple(map(median, zip(*results[path]))) for path in ("ring", "view"))
            paired_cpu_pct = [(1 - v[0] / r[0]) * 100 for r, v in zip(results["ring"], results["view"])]
            paired_rate_pct = [(r[1] / v[1] - 1) * 100 for r, v in zip(results["ring"], results["view"])]
            print(f"BENCH producers={producers} mode={mode} layout={layout} packets={expected} samples={samples} "
                  f"cpu_ns_per_packet={ring[0]:.1f}->{view[0]:.1f} cpu_reduction_pct={(1-view[0]/ring[0])*100:+.1f} "
                  f"Mpps={1000/ring[1]:.3f}->{1000/view[1]:.3f} rate_gain_pct={(ring[1]/view[1]-1)*100:+.1f} "
                  f"paired_cpu_range={min(paired_cpu_pct):+.1f}..{max(paired_cpu_pct):+.1f} "
                  f"paired_rate_range={min(paired_rate_pct):+.1f}..{max(paired_rate_pct):+.1f}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--producers", type=int, nargs="+", default=[4], choices=range(1, 33), metavar="1..32")
    parser.add_argument("--groups", type=int, default=256, choices=range(2, 4097), metavar="2..4096")
    parser.add_argument("--benchmark", type=int, choices=range(2, 22), metavar="SAMPLES")
    options = parser.parse_args()
    if sys.platform != "win32":
        parser.error("Windows x64 and MSVC are required")

    build = HERE / ".build"
    build.mkdir(exist_ok=True)
    try:
        env = x64_environment()
        compiler = shutil.which("cl.exe", path=env.get("PATH", ""))
        if not compiler:
            raise RuntimeError("vcvars64 did not put cl.exe on PATH")
        msbuild = Path(env["VSINSTALLDIR"]) / "MSBuild" / "Current" / "Bin" / "MSBuild.exe"
        subprocess.run([str(msbuild), str(SERVER / "ServerNetworkLib.vcxproj"), "/t:Build", "/p:Configuration=Release",
                        "/p:Platform=x64", "/nologo", "/v:minimal"], cwd=ROOT, env=env, check=True, timeout=180)
        lib_root = ROOT / "Build"
        libraries = [lib_root / "Server" / "Release" / "ServerNetworkLib" / "ServerNetworkLib.lib",
                     lib_root / "Common" / "Release" / "NetworkCommonLib" / "NetworkCommonLib.lib"]
        with tempfile.TemporaryDirectory(prefix="run-", dir=build) as run_dir:
            run = Path(run_dir)
            exe = run / "ServerRecvViewMock.exe"
            args = [compiler, "/nologo", "/std:c++20", "/EHsc", "/O2", "/MD", "/W3", "/WX", "/utf-8",
                    "/DUNICODE", "/D_UNICODE", "/DNDEBUG", *(f"/I{path}" for path in (NETWORK, SERVER, MHLIB)),
                    str(HERE / "ServerRecvViewMock.cpp"), *libraries, f"/Fe:{exe}", "/link", "/LTCG"]
            subprocess.run(args, cwd=run, env=env, check=True, timeout=180)
            for producers in options.producers:
                if options.benchmark:
                    benchmark(exe, run, env, producers, options.groups, options.benchmark)
                    continue
                config = f"ServerRecvView:\r\n    - Producers = {producers};\r\n    - Groups = {options.groups};\r\n"
                (run / "ServerRecvView.ini").write_bytes(config.encode("utf-16"))
                subprocess.run([str(exe)], cwd=run, env=env, check=True, timeout=60)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
