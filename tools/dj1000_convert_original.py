#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


APP_NAME = "UMAX PhotoRun"
CSC_PATH = Path(r"C:\windows\Microsoft.NET\Framework\v4.0.30319\csc.exe")
EXPORT_MODE_MAP = {"small": 0, "medium": 2, "large": 5}
PROFILE_FIELD_MAP = {
    "green_balance": 0,
    "red_balance": 1,
    "blue_balance": 2,
    "contrast": 3,
    "brightness": 4,
    "vividness": 5,
    "sharpness": 6,
}
DEFAULT_PROFILE = {
    "red_balance": 100,
    "green_balance": 100,
    "blue_balance": 100,
    "contrast": 3,
    "brightness": 3,
    "vividness": 3,
    "sharpness": 3,
}


def project_root() -> Path:
    return Path(__file__).resolve().parents[1]


def resource_root() -> Path:
    if getattr(sys, "frozen", False):
        bundled_root = Path(getattr(sys, "_MEIPASS", Path(sys.executable).resolve().parent))
        app_resources = bundled_root / "app_resources"
        if app_resources.exists():
            return app_resources
        return bundled_root
    return project_root()


def state_root() -> Path:
    override = os.environ.get("DJ1000_STATE_ROOT")
    if override:
        return Path(override).expanduser().resolve()
    if getattr(sys, "frozen", False):
        return (Path.home() / "Library" / "Application Support" / APP_NAME).resolve()
    return project_root() / "tmp_debug"


def env_path(name: str, default: Path) -> Path:
    override = os.environ.get(name)
    if override:
        return Path(override).expanduser().resolve()
    return default


def resolve_wine_binary() -> Path:
    override = os.environ.get("DJ1000_WINE")
    if override:
        return Path(override).expanduser().resolve()
    bundled = ROOT / "tmp_debug" / "wine-stable" / "Wine Stable.app" / "Contents" / "Resources" / "wine" / "bin" / "wine"
    if bundled.exists():
        return bundled
    discovered = shutil.which("wine")
    if discovered:
        return Path(discovered).expanduser().resolve()
    return bundled


ROOT = resource_root()
STATE_ROOT = state_root()
WINE = resolve_wine_binary()
WINEPREFIX_TEMPLATE = env_path("DJ1000_WINEPREFIX_TEMPLATE", ROOT / "tmp_debug" / "wineprefix")
WINEPREFIX = env_path("DJ1000_WINEPREFIX", STATE_ROOT / "wineprefix")
HARNESS_SOURCE = env_path("DJ1000_HARNESS_SOURCE", ROOT / "tools" / "Dj1000DllHarness.cs")
HARNESS_EXE = env_path("DJ1000_HARNESS_EXE", STATE_ROOT / "Dj1000DllHarness.exe")
DLL_PATH = env_path("DJ1000_DLL_PATH", ROOT / "extracted" / "Group1" / "DsGraph.dll")


def ensure_runtime_ready() -> None:
    STATE_ROOT.mkdir(parents=True, exist_ok=True)
    if not WINEPREFIX.exists():
        if WINEPREFIX_TEMPLATE.exists():
            shutil.copytree(WINEPREFIX_TEMPLATE, WINEPREFIX, symlinks=True)
        else:
            WINEPREFIX.mkdir(parents=True, exist_ok=True)

    missing = []
    for required_path in (WINE, HARNESS_SOURCE, DLL_PATH):
        if not required_path.exists():
            missing.append(str(required_path))
    if missing:
        raise FileNotFoundError("Missing converter runtime files:\n" + "\n".join(missing))


def to_wine_path(path: Path) -> str:
    resolved = path.expanduser().resolve()
    return "Z:" + str(resolved).replace("/", "\\")


def compile_harness(force: bool = False) -> None:
    ensure_runtime_ready()
    if HARNESS_EXE.exists() and not force and HARNESS_EXE.stat().st_mtime >= HARNESS_SOURCE.stat().st_mtime:
        return

    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"
    command = [
        str(WINE),
        str(CSC_PATH),
        "/nologo",
        "/target:exe",
        "/platform:x86",
        "/optimize+",
        f"/out:{to_wine_path(HARNESS_EXE)}",
        to_wine_path(HARNESS_SOURCE),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def run_harness(
    input_path: Path,
    output_path: Path,
    export_mode: int,
    converter_mode: int,
    settings: dict[int, int] | None = None,
) -> None:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--input",
        to_wine_path(input_path),
        "--output",
        to_wine_path(output_path),
        "--converter-mode",
        str(converter_mode),
        "--export-mode",
        str(export_mode),
    ]
    if settings:
        for index in sorted(settings):
            command.extend([f"--setting{index}", str(settings[index])])
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def trace_large_callsite(
    input_path: Path,
    output_path: Path,
    export_mode: int = 5,
    converter_mode: int = 0,
    settings: dict[int, int] | None = None,
) -> Path:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--input",
        to_wine_path(input_path),
        "--output",
        to_wine_path(output_path),
        "--trace-large-callsite-params",
        "--converter-mode",
        str(converter_mode),
        "--export-mode",
        str(export_mode),
    ]
    if settings:
        for index in sorted(settings):
            command.extend([f"--setting{index}", str(settings[index])])
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return output_path


def dump_resample_lut(
    width_parameter: int,
    output_path: Path,
    helper_mode_e4: int = 0,
) -> None:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_path),
        "--dump-resample-lut",
        str(width_parameter),
        "--helper-mode-e4",
        str(helper_mode_e4),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def dump_geometry_stage(
    plane0_path: Path,
    plane1_path: Path,
    plane2_path: Path,
    output_prefix: Path,
    preview_flag: int = 0,
    export_mode: int = 5,
    geometry_selector: int = 1,
) -> tuple[Path, Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-geometry-stage",
        "--plane0",
        to_wine_path(plane0_path),
        "--plane1",
        to_wine_path(plane1_path),
        "--plane2",
        to_wine_path(plane2_path),
        "--geometry-preview-flag",
        str(preview_flag),
        "--geometry-export-mode",
        str(export_mode),
        "--geometry-selector",
        str(geometry_selector),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.plane0.bin",
        output_prefix.parent / f"{output_prefix.name}.plane1.bin",
        output_prefix.parent / f"{output_prefix.name}.plane2.bin",
    )


def dump_normalize_stage(
    plane0_path: Path,
    plane1_path: Path,
    plane2_path: Path,
    output_prefix: Path,
    preview_flag: int = 0,
    stride: int | None = None,
    rows: int | None = None,
) -> tuple[Path, Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-normalize-stage",
        "--plane0",
        to_wine_path(plane0_path),
        "--plane1",
        to_wine_path(plane1_path),
        "--plane2",
        to_wine_path(plane2_path),
        "--normalize-preview-flag",
        str(preview_flag),
    ]
    if stride is not None:
        command.extend(["--normalize-stride", str(stride)])
    if rows is not None:
        command.extend(["--normalize-rows", str(rows)])
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.plane0.f32",
        output_prefix.parent / f"{output_prefix.name}.plane1.f32",
        output_prefix.parent / f"{output_prefix.name}.plane2.f32",
    )


def dump_source_stage(
    source_input_path: Path,
    output_prefix: Path,
    preview_flag: int = 0,
    output_stride: int | None = None,
    output_rows: int | None = None,
) -> tuple[Path, Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-source-stage",
        "--source-input",
        to_wine_path(source_input_path),
        "--source-preview-flag",
        str(preview_flag),
    ]
    if output_stride is not None:
        command.extend(["--source-output-stride", str(output_stride)])
    if output_rows is not None:
        command.extend(["--source-output-rows", str(output_rows)])
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.plane0.f32",
        output_prefix.parent / f"{output_prefix.name}.plane1.f32",
        output_prefix.parent / f"{output_prefix.name}.plane2.f32",
    )


def dump_source_prepass_4570(
    source_input_path: Path,
    output_path: Path,
    preview_flag: int = 0,
) -> Path:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_path),
        "--dump-source-prepass-4570",
        "--source-input",
        to_wine_path(source_input_path),
        "--source-preview-flag",
        str(preview_flag),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return output_path


def dump_post_geometry_prepare(
    plane0_path: Path,
    plane1_path: Path,
    plane2_path: Path,
    output_prefix: Path,
    width: int,
    height: int,
) -> tuple[Path, Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-post-geometry-prepare",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane0_path),
        "--plane1",
        to_wine_path(plane1_path),
        "--plane2",
        to_wine_path(plane2_path),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.center.f64",
        output_prefix.parent / f"{output_prefix.name}.delta0.f64",
        output_prefix.parent / f"{output_prefix.name}.delta2.f64",
    )


def dump_post_geometry_filter(
    delta0_path: Path,
    delta2_path: Path,
    output_prefix: Path,
    width: int,
    height: int,
) -> tuple[Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-post-geometry-filter",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--delta0",
        to_wine_path(delta0_path),
        "--delta2",
        to_wine_path(delta2_path),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.delta0.f64",
        output_prefix.parent / f"{output_prefix.name}.delta2.f64",
    )


def dump_post_geometry_center_scale(
    delta0_path: Path,
    center_path: Path,
    delta2_path: Path,
    output_prefix: Path,
    width: int,
    height: int,
) -> tuple[Path, Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-post-geometry-center-scale",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(delta0_path),
        "--plane1",
        to_wine_path(center_path),
        "--plane2",
        to_wine_path(delta2_path),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.delta0.f64",
        output_prefix.parent / f"{output_prefix.name}.center.f64",
        output_prefix.parent / f"{output_prefix.name}.delta2.f64",
    )


def dump_post_geometry_rgb_convert(
    plane0_path: Path,
    plane1_path: Path,
    plane2_path: Path,
    output_prefix: Path,
    width: int,
    height: int,
) -> tuple[Path, Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-post-geometry-rgb-convert",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane0_path),
        "--plane1",
        to_wine_path(plane1_path),
        "--plane2",
        to_wine_path(plane2_path),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.plane0.bin",
        output_prefix.parent / f"{output_prefix.name}.plane1.bin",
        output_prefix.parent / f"{output_prefix.name}.plane2.bin",
    )


def dump_post_geometry_edge_response(
    plane_path: Path,
    output_path: Path,
    width: int,
    height: int,
) -> Path:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_path),
        "--dump-post-geometry-edge-response",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane_path),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return output_path


def dump_post_geometry_dual_scale(
    plane0_path: Path,
    plane1_path: Path,
    output_prefix: Path,
    width: int,
    height: int,
    scalar: float,
) -> tuple[Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-post-geometry-dual-scale",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane0_path),
        "--plane1",
        to_wine_path(plane1_path),
        "--scalar",
        str(scalar),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.plane0.f64",
        output_prefix.parent / f"{output_prefix.name}.plane1.f64",
    )


def dump_post_geometry_stage_2a00(
    plane_path: Path,
    output_path: Path,
    width: int,
    height: int,
) -> Path:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_path),
        "--dump-post-geometry-stage-2a00",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane_path),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return output_path


def dump_post_geometry_stage_2dd0(
    plane0_path: Path,
    plane1_path: Path,
    output_prefix: Path,
    width: int,
    height: int,
) -> tuple[Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-post-geometry-stage-2dd0",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane0_path),
        "--plane1",
        to_wine_path(plane1_path),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.plane0.f64",
        output_prefix.parent / f"{output_prefix.name}.plane1.f64",
    )


def dump_post_geometry_stage_4810(
    plane0_path: Path,
    plane1_path: Path,
    plane2_path: Path,
    output_prefix: Path,
    width: int,
    height: int,
) -> tuple[Path, Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-post-geometry-stage-4810",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane0_path),
        "--plane1",
        to_wine_path(plane1_path),
        "--plane2",
        to_wine_path(plane2_path),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.plane0.f64",
        output_prefix.parent / f"{output_prefix.name}.plane1.f64",
        output_prefix.parent / f"{output_prefix.name}.plane2.f64",
    )


def dump_post_geometry_stage_3600(
    plane_path: Path,
    output_path: Path,
    width: int,
    height: int,
) -> Path:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_path),
        "--dump-post-geometry-stage-3600",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane_path),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return output_path


def dump_post_geometry_stage_3890(
    plane0_path: Path,
    plane1_path: Path,
    output_prefix: Path,
    width: int,
    height: int,
    level: int,
) -> tuple[Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-post-geometry-stage-3890",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane0_path),
        "--plane1",
        to_wine_path(plane1_path),
        "--level",
        str(level),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.plane0.f64",
        output_prefix.parent / f"{output_prefix.name}.plane1.f64",
    )


def dump_post_geometry_stage_3060(
    plane0_path: Path,
    plane1_path: Path,
    output_path: Path,
    width: int,
    height: int,
    stage_param0: int,
    stage_param1: int,
    scalar: float,
    threshold: int,
) -> Path:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_path),
        "--dump-post-geometry-stage-3060",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane0_path),
        "--plane1",
        to_wine_path(plane1_path),
        "--stage-param0",
        str(stage_param0),
        "--stage-param1",
        str(stage_param1),
        "--scalar",
        str(scalar),
        "--threshold",
        str(threshold),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return output_path


def dump_post_rgb_stage_3b00(
    plane0_path: Path,
    plane1_path: Path,
    plane2_path: Path,
    output_prefix: Path,
    width: int,
    height: int,
    level: int,
    scalar: float,
) -> tuple[Path, Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-post-rgb-stage-3b00",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane0_path),
        "--plane1",
        to_wine_path(plane1_path),
        "--plane2",
        to_wine_path(plane2_path),
        "--level",
        str(level),
        "--scalar",
        str(scalar),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.plane0.bin",
        output_prefix.parent / f"{output_prefix.name}.plane1.bin",
        output_prefix.parent / f"{output_prefix.name}.plane2.bin",
    )


def dump_post_rgb_stage_3f40(
    plane0_path: Path,
    plane1_path: Path,
    plane2_path: Path,
    output_prefix: Path,
    width: int,
    height: int,
    level: int = 3,
) -> tuple[Path, Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-post-rgb-stage-3f40",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane0_path),
        "--plane1",
        to_wine_path(plane1_path),
        "--plane2",
        to_wine_path(plane2_path),
        "--level",
        str(level),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.plane0.bin",
        output_prefix.parent / f"{output_prefix.name}.plane1.bin",
        output_prefix.parent / f"{output_prefix.name}.plane2.bin",
    )


def dump_post_rgb_stage_42a0(
    plane0_path: Path,
    plane1_path: Path,
    plane2_path: Path,
    output_prefix: Path,
    width: int,
    height: int,
    scale0: int,
    scale1: int,
    scale2: int,
) -> tuple[Path, Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-post-rgb-stage-42a0",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane0_path),
        "--plane1",
        to_wine_path(plane1_path),
        "--plane2",
        to_wine_path(plane2_path),
        "--stage-param0",
        str(scale0),
        "--stage-param1",
        str(scale1),
        "--stage-param2",
        str(scale2),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.plane0.bin",
        output_prefix.parent / f"{output_prefix.name}.plane1.bin",
        output_prefix.parent / f"{output_prefix.name}.plane2.bin",
    )


def dump_post_rgb_stage_40f0(
    plane0_path: Path,
    plane1_path: Path,
    plane2_path: Path,
    output_prefix: Path,
    width: int,
    height: int,
    level: int,
    selector: int,
) -> tuple[Path, Path, Path]:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_prefix),
        "--dump-post-rgb-stage-40f0",
        "--stage-width",
        str(width),
        "--stage-height",
        str(height),
        "--plane0",
        to_wine_path(plane0_path),
        "--plane1",
        to_wine_path(plane1_path),
        "--plane2",
        to_wine_path(plane2_path),
        "--level",
        str(level),
        "--selector",
        str(selector),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return (
        output_prefix.parent / f"{output_prefix.name}.plane0.bin",
        output_prefix.parent / f"{output_prefix.name}.plane1.bin",
        output_prefix.parent / f"{output_prefix.name}.plane2.bin",
    )


def dump_row_resample(
    row_input_path: Path,
    output_path: Path,
    lut_width_parameter: int,
    width_limit: int,
    output_length: int,
    reset_counter: int = 0x100,
    mode_flag: int = 0,
    mode_e0: int = 1,
    helper_mode_e4: int = 1,
) -> Path:
    ensure_runtime_ready()
    env = os.environ.copy()
    env["WINEPREFIX"] = str(WINEPREFIX)
    env["WINEDEBUG"] = "-all"

    command = [
        str(WINE),
        to_wine_path(HARNESS_EXE),
        "--dll",
        to_wine_path(DLL_PATH),
        "--output",
        to_wine_path(output_path),
        "--dump-row-resample",
        "--row-input",
        to_wine_path(row_input_path),
        "--row-lut-width",
        str(lut_width_parameter),
        "--row-width-limit",
        str(width_limit),
        "--row-output-length",
        str(output_length),
        "--row-reset-counter",
        str(reset_counter),
        "--row-mode-flag",
        str(mode_flag),
        "--row-mode-e0",
        str(mode_e0),
        "--row-helper-mode-e4",
        str(helper_mode_e4),
    ]
    subprocess.run(
        command,
        env=env,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return output_path


def load_profile_settings(profile_path: Path) -> dict[int, int]:
    profile = json.loads(profile_path.read_text())
    settings: dict[int, int] = {}
    for key, index in PROFILE_FIELD_MAP.items():
        value = profile.get(key)
        if value is not None:
            settings[index] = int(value)
    return settings


def apply_named_settings(
    settings: dict[int, int],
    named_values: dict[str, int | None],
) -> dict[int, int]:
    merged = dict(settings)
    for key, index in PROFILE_FIELD_MAP.items():
        value = named_values.get(key)
        if value is not None:
            merged[index] = int(value)
    return merged


def raw_settings_to_named(settings: dict[int, int] | None = None) -> dict[str, int]:
    named = dict(DEFAULT_PROFILE)
    if settings is None:
        return named
    for key, index in PROFILE_FIELD_MAP.items():
        if index in settings:
            named[key] = int(settings[index])
    return named


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run the original DsGraph.dll converter under Wine on macOS."
    )
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--size",
        choices=("small", "medium", "large"),
        default="large",
        help="legacy export mode value: small=0, medium=2, large=5",
    )
    parser.add_argument(
        "--converter-mode",
        type=int,
        default=0,
        help="first DJ1000GraphicConv argument; 0 is export, 1 is preview",
    )
    parser.add_argument(
        "--rebuild",
        action="store_true",
        help="force recompiling the Wine-side harness",
    )
    parser.add_argument(
        "--profile",
        type=Path,
        help="JSON profile with named PhotoRun control values",
    )
    parser.add_argument(
        "--contrast",
        type=int,
        help="override the PhotoRun contrast slider (maps to setting3)",
    )
    parser.add_argument(
        "--brightness",
        type=int,
        help="override the PhotoRun brightness slider (maps to setting4)",
    )
    parser.add_argument(
        "--vividness",
        type=int,
        help="override the PhotoRun vividness slider (maps to setting5)",
    )
    parser.add_argument(
        "--sharpness",
        type=int,
        help="override the PhotoRun sharpness slider (maps to setting6)",
    )
    parser.add_argument(
        "--red-balance",
        type=int,
        help="override the PhotoRun red color-balance control (maps to setting1)",
    )
    parser.add_argument(
        "--green-balance",
        type=int,
        help="override the PhotoRun green color-balance control (maps to setting0)",
    )
    parser.add_argument(
        "--blue-balance",
        type=int,
        help="override the PhotoRun blue color-balance control (maps to setting2)",
    )
    for index in range(7):
        parser.add_argument(
            f"--setting{index}",
            type=int,
            help=f"override EXE-side setting block slot {index}",
        )
    args = parser.parse_args()

    if not args.input.exists():
        print(f"Input file not found: {args.input}", file=sys.stderr)
        return 1

    try:
        ensure_runtime_ready()
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    settings: dict[int, int] = {}

    if args.profile is not None:
        if not args.profile.exists():
            print(f"Profile file not found: {args.profile}", file=sys.stderr)
            return 1
        settings.update(load_profile_settings(args.profile))

    for index in range(7):
        value = getattr(args, f"setting{index}")
        if value is not None:
            settings[index] = value

    settings = apply_named_settings(
        settings,
        {
            "green_balance": args.green_balance,
            "red_balance": args.red_balance,
            "blue_balance": args.blue_balance,
            "contrast": args.contrast,
            "brightness": args.brightness,
            "vividness": args.vividness,
            "sharpness": args.sharpness,
        },
    )

    compile_harness(force=args.rebuild)
    run_harness(
        input_path=args.input,
        output_path=args.output,
        export_mode=EXPORT_MODE_MAP[args.size],
        converter_mode=args.converter_mode,
        settings=settings,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
