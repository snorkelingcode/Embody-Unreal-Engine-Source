"""
Embody Project Packager
=======================
Packages the Embody project for Windows and/or Linux using RunUAT BuildCookRun.

Usage (from CLI):
    python package_project.py windows
    python package_project.py linux
    python package_project.py all
    python package_project.py windows --shipping
    python package_project.py all --config Development --clean

Can also be imported:
    from package_project import package
    package("windows")
    package("linux", config="Shipping", clean=True)
    package("all")
"""

import subprocess
import sys
import os
import time
import argparse

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
UE_ROOT = r"C:\Program Files\Epic Games\UE_5.7"
RUN_UAT = os.path.join(UE_ROOT, "Engine", "Build", "BatchFiles", "RunUAT.bat")
PROJECT = r"C:\Users\danek\OneDrive\Documents\Unreal Projects\Embody\Embody.uproject"
OUTPUT_BASE = r"C:\Users\danek\OneDrive\Documents\Builds"

# Map to display name for the target platform
MAP_TO_COOK = "/Game/ThirdPerson/Maps/ThirdPersonMap"

PLATFORMS = {
    "windows": {
        "target_platform": "Win64",
        "output_dir": os.path.join(OUTPUT_BASE, "Windows"),
    },
    "linux": {
        "target_platform": "Linux",
        "output_dir": os.path.join(OUTPUT_BASE, "Linux"),
    },
}

VALID_CONFIGS = ["Development", "Shipping", "DebugGame"]


# ---------------------------------------------------------------------------
# Build command
# ---------------------------------------------------------------------------
def build_uat_command(platform_key, config="Development", clean=False, compressed=True,
                     pak=True, prereqs=True, stage=True, archive=True):
    """Build the RunUAT BuildCookRun command for the given platform."""

    if platform_key not in PLATFORMS:
        raise ValueError(f"Unknown platform: {platform_key}. Use: {list(PLATFORMS.keys())}")
    if config not in VALID_CONFIGS:
        raise ValueError(f"Unknown config: {config}. Use: {VALID_CONFIGS}")

    plat = PLATFORMS[platform_key]

    cmd = [
        RUN_UAT,
        "BuildCookRun",
        f"-project={PROJECT}",
        f"-targetplatform={plat['target_platform']}",
        f"-clientconfig={config}",
        f"-serverconfig={config}",
        "-noP4",
        "-utf8output",
        "-nodebuginfo",
        "-build",
        "-cook",
        f"-map={MAP_TO_COOK}",
        "-unversionedcookedcontent",
    ]

    if pak:
        cmd.append("-pak")
    if compressed:
        cmd.append("-compressed")
    if prereqs:
        cmd.append("-prereqs")
    if stage:
        cmd.append("-stage")
    if archive:
        cmd.extend(["-archive", f"-archivedirectory={plat['output_dir']}"])
    if clean:
        cmd.append("-clean")

    return cmd


# ---------------------------------------------------------------------------
# Package execution
# ---------------------------------------------------------------------------
def package(platform_key, config="Development", clean=False, verbose=True):
    """Package the project for the given platform.

    Args:
        platform_key: "windows", "linux", or "all"
        config: "Development", "Shipping", or "DebugGame"
        clean: If True, clean before building
        verbose: If True, stream UAT output to stdout

    Returns:
        dict with platform -> (success: bool, duration: float, output_dir: str)
    """
    if platform_key == "all":
        platforms = list(PLATFORMS.keys())
    else:
        platforms = [platform_key]

    results = {}

    for plat in platforms:
        cmd = build_uat_command(plat, config=config, clean=clean)
        output_dir = PLATFORMS[plat]["output_dir"]

        print("=" * 60)
        print(f"Packaging for {plat.upper()} ({config})")
        print(f"Output: {output_dir}")
        print("=" * 60)

        if verbose:
            print(f"Command: {' '.join(cmd[:5])}...")
            print()

        start = time.time()

        if verbose:
            proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
            )
            for line in proc.stdout:
                line = line.rstrip()
                # Filter to important lines to reduce noise
                if any(kw in line for kw in [
                    "Error", "Warning", "Cook", "Package", "BUILD ",
                    "Stage", "Archive", "success", "FAILURE", "Total ",
                    "LogInit", "Running ", "****",
                ]):
                    print(f"  {line}")
            proc.wait()
            returncode = proc.returncode
        else:
            result = subprocess.run(cmd, capture_output=True, text=True)
            returncode = result.returncode

        duration = time.time() - start

        success = returncode == 0
        results[plat] = {
            "success": success,
            "duration": duration,
            "output_dir": output_dir,
        }

        if success:
            print(f"\n  SUCCESS! Packaged {plat.upper()} in {duration:.0f}s")
            print(f"  Output: {output_dir}")
        else:
            print(f"\n  FAILED! {plat.upper()} packaging failed after {duration:.0f}s")
            print(f"  Exit code: {returncode}")

        print()

    # Summary
    print("=" * 60)
    print("Packaging Summary")
    print("=" * 60)
    for plat, r in results.items():
        status = "OK" if r["success"] else "FAILED"
        print(f"  {plat.upper():10s} {status:8s} {r['duration']:.0f}s  {r['output_dir']}")

    return results


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="Package the Embody project for Windows and/or Linux",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python package_project.py windows
  python package_project.py linux --shipping
  python package_project.py all --config Shipping --clean
  python package_project.py windows --config DebugGame
        """,
    )
    parser.add_argument(
        "platform",
        choices=["windows", "linux", "all"],
        help="Target platform (or 'all' for both)",
    )
    parser.add_argument(
        "--config",
        choices=VALID_CONFIGS,
        default="Development",
        help="Build configuration (default: Development)",
    )
    parser.add_argument(
        "--shipping",
        action="store_const",
        const="Shipping",
        dest="config_shortcut",
        help="Shorthand for --config Shipping",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean before building",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress UAT output (only show summary)",
    )

    args = parser.parse_args()

    config = args.config_shortcut or args.config

    results = package(
        platform_key=args.platform,
        config=config,
        clean=args.clean,
        verbose=not args.quiet,
    )

    all_ok = all(r["success"] for r in results.values())
    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()
