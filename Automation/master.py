"""
Embody Automation Master
========================
Central dispatch for all Embody automation scripts.

Usage (from CLI):
    python master.py create_metahuman
    python master.py delete_metahuman MH_Random_abc12345 MH_Random_def67890
    python master.py list_metahumans
    python master.py package_windows
    python master.py package_linux
    python master.py package_all

Can also be imported:
    from master import AutomationMaster
    AutomationMaster.run("create_metahuman")
    AutomationMaster.run("delete_metahuman", ["MH_Random_abc12345"])
    AutomationMaster.run("package_windows")
    AutomationMaster.run("package_all", ["--shipping"])
"""

import sys
import os
import time
import socket
import logging

# UE5 remote execution
sys.path.insert(0, r"C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution

AUTOMATION_DIR = os.path.dirname(os.path.abspath(__file__))
METAHUMAN_DIR = os.path.join(AUTOMATION_DIR, "MetaHumans")
PACKAGING_DIR = os.path.join(AUTOMATION_DIR, "Packaging")


# ---------------------------------------------------------------------------
# Available actions
# ---------------------------------------------------------------------------
class Action:
    def __init__(self, name, description, script_path=None, needs_deferred=False):
        self.name = name
        self.description = description
        self.script_path = script_path
        self.needs_deferred = needs_deferred


ACTIONS = {
    "create_metahuman": Action(
        name="create_metahuman",
        description="Create a fully assembled random MetaHuman (rig, textures, assembly)",
        script_path=os.path.join(METAHUMAN_DIR, "random_metahuman_generator.py"),
        needs_deferred=True,  # Uses blocking cloud ops that crash under remote exec ticker
    ),
    "delete_metahuman": Action(
        name="delete_metahuman",
        description="Delete one or more MetaHumans by name (e.g. MH_Random_abc12345)",
        script_path=os.path.join(METAHUMAN_DIR, "delete_metahuman.py"),
        needs_deferred=False,
    ),
    "list_metahumans": Action(
        name="list_metahumans",
        description="List all MetaHumanCharacter assets in the project",
        script_path=None,
        needs_deferred=False,
    ),
    "package_windows": Action(
        name="package_windows",
        description="Package project for Windows (args: --shipping, --clean)",
        script_path=os.path.join(PACKAGING_DIR, "package_project.py"),
        needs_deferred=False,
    ),
    "package_linux": Action(
        name="package_linux",
        description="Package project for Linux (args: --shipping, --clean)",
        script_path=os.path.join(PACKAGING_DIR, "package_project.py"),
        needs_deferred=False,
    ),
    "package_all": Action(
        name="package_all",
        description="Package project for Windows + Linux (args: --shipping, --clean)",
        script_path=os.path.join(PACKAGING_DIR, "package_project.py"),
        needs_deferred=False,
    ),
}


# ---------------------------------------------------------------------------
# Remote execution helpers
# ---------------------------------------------------------------------------
def connect_to_editor():
    config = remote_execution.RemoteExecutionConfig()
    config.multicast_group_endpoint = ('239.0.0.1', 6766)
    config.multicast_bind_address = '127.0.0.1'
    config.multicast_ttl = 0
    config.command_endpoint = ('127.0.0.1', 6776)

    re = remote_execution.RemoteExecution(config)
    re.start()

    nodes = []
    for _ in range(20):
        time.sleep(0.5)
        nodes = re.remote_nodes
        if nodes:
            break

    if not nodes:
        re.stop()
        raise ConnectionError("No UE5 editor found. Is it running with Python Remote Execution enabled?")

    node_id = nodes[0]["node_id"]
    re.open_command_connection(node_id)
    return re, node_id


def run_direct(re, code):
    """Run code directly via remote execution (for fast, non-blocking ops)."""
    result = re.run_command(code, unattended=True, exec_mode=remote_execution.MODE_EXEC_FILE)
    return result


def run_deferred(re, script_path):
    """Run a script deferred to avoid ticker re-entrancy crashes with blocking cloud ops."""
    script_path_posix = script_path.replace("\\", "/")
    bootstrap = f'''
import unreal

_script_path = r"{script_path_posix}"

def _run_deferred(delta_time):
    unreal.unregister_slate_post_tick_callback(_handle)
    unreal.log("Deferred execution starting...")
    try:
        with open(_script_path, "r", encoding="utf-8") as f:
            exec(f.read(), {{"__name__": "__main__"}})
    except Exception as e:
        unreal.log_error(f"Automation failed: {{e}}")
        import traceback
        unreal.log_error(traceback.format_exc())

_handle = unreal.register_slate_post_tick_callback(_run_deferred)
unreal.log(f"Deferred: {{_script_path}}")
'''
    return run_direct(re, bootstrap)


def disconnect(re):
    try:
        re.close_command_connection()
    except Exception:
        pass
    re.stop()


UE_EDITOR = r"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
PROJECT = r"C:\Users\danek\OneDrive\Documents\Unreal Projects\Embody\Embody.uproject"


def is_editor_running():
    """Check if UnrealEditor.exe is running."""
    import subprocess
    result = subprocess.run(["tasklist"], capture_output=True, text=True)
    return "UnrealEditor.exe" in result.stdout


def relaunch_editor_and_wait(timeout=180):
    """Launch the editor and wait for remote execution to be ready."""
    import subprocess

    if is_editor_running():
        print("Editor is already running.")
        return True

    print("Launching UE5 editor...")
    subprocess.Popen([UE_EDITOR, PROJECT],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    print("Waiting for editor to be ready...")
    remote_execution.set_log_level(logging.WARNING)
    for i in range(timeout // 5):
        time.sleep(5)
        try:
            re = remote_execution.RemoteExecution(remote_execution.RemoteExecutionConfig())
            re.start()
            nodes = []
            for _ in range(6):
                time.sleep(0.5)
                nodes = re.remote_nodes
                if nodes:
                    break
            re.stop()
            if nodes:
                print(f"Editor ready after {(i+1)*5}s")
                return True
        except Exception:
            pass

    print("ERROR: Editor did not respond in time.")
    return False


# ---------------------------------------------------------------------------
# Action implementations
# ---------------------------------------------------------------------------
def do_create_metahuman(re, args):
    script = ACTIONS["create_metahuman"].script_path
    print("Sending create_metahuman to editor (deferred)...")
    result = run_deferred(re, script)
    if result.get("success"):
        print("Command accepted. MetaHuman is being created in the editor.")
        print("Watch the UE5 Output Log for progress (takes several minutes).")
    else:
        print(f"FAILED: {result.get('result', 'Unknown error')}")
    return result.get("success", False)


def do_delete_metahuman(re, args):
    if not args:
        print("ERROR: delete_metahuman requires at least one MetaHuman name.")
        print("Usage: python master.py delete_metahuman MH_Random_abc12345 [...]")
        return False

    names_list = str(args).replace("'", '"')
    code = f'''
import unreal

METAHUMAN_ROOT = "/Game/MetaHumans"
CHARACTER_ROOT = "/Game/Characters/MetaHumans"
names = {names_list}

for name in names:
    deleted = False
    for root in [METAHUMAN_ROOT, CHARACTER_ROOT]:
        path = f"{{root}}/{{name}}"
        if unreal.EditorAssetLibrary.does_directory_exist(path):
            unreal.log(f"  Deleting directory: {{path}}")
            unreal.EditorAssetLibrary.delete_directory(path)
            deleted = True
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log(f"  Deleting asset: {{path}}")
            unreal.EditorAssetLibrary.delete_asset(path)
            deleted = True
    if deleted:
        unreal.log(f"Deleted {{name}}")
    else:
        unreal.log_warning(f"{{name}} not found")

unreal.log(f"=== Done! Processed {{len(names)}} MetaHumans ===")

# Save the MetaHumans directory to flush deletions to disk
# Prevents stale .uasset files from lingering after editor restart
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log("Saved all dirty packages")
'''
    print(f"Deleting {len(args)} MetaHuman(s): {', '.join(args)}")
    result = run_direct(re, code)
    success = result.get("success", False)
    if result.get("output"):
        for e in result["output"]:
            out = e.get("output", "").strip()
            if out:
                print(f"  {out}")
    if not success:
        print(f"FAILED: {result.get('result', 'Unknown error')}")

    # Deletion often crashes the editor — always restart to ensure clean state
    # for subsequent operations (especially create_metahuman)
    print("Restarting editor after deletion for clean state...")
    time.sleep(3)

    # Kill editor if still running (deletion can leave it in a bad state)
    import subprocess
    if is_editor_running():
        subprocess.run(["taskkill", "/F", "/IM", "UnrealEditor.exe"],
                       capture_output=True)
        time.sleep(3)

    relaunch_editor_and_wait()

    return success


def do_list_metahumans(re, args):
    code = '''
import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
f = unreal.ARFilter(
    class_paths=[unreal.MetaHumanCharacter.static_class().get_class_path_name()]
)
assets = ar.get_assets(filter=f)
print(f"Found {len(assets)} MetaHumanCharacter asset(s):")
for a in assets:
    print(f"  {a.package_name}")
'''
    result = run_direct(re, code)
    if result.get("output"):
        for e in result["output"]:
            out = e.get("output", "").strip()
            if out:
                print(out)
    if not result.get("success"):
        print(f"FAILED: {result.get('result', 'Unknown error')}")
    return result.get("success", False)


def do_package(re, args, platform):
    """Package the project. Does NOT require the editor — runs UAT directly."""
    sys.path.insert(0, PACKAGING_DIR)
    from package_project import package

    config = "Development"
    clean = False
    for arg in args:
        if arg == "--shipping":
            config = "Shipping"
        elif arg == "--clean":
            clean = True
        elif arg in ("Development", "Shipping", "DebugGame"):
            config = arg

    results = package(platform_key=platform, config=config, clean=clean)
    return all(r["success"] for r in results.values())


def do_package_windows(re, args):
    return do_package(re, args, "windows")


def do_package_linux(re, args):
    return do_package(re, args, "linux")


def do_package_all(re, args):
    return do_package(re, args, "all")


ACTION_HANDLERS = {
    "create_metahuman": do_create_metahuman,
    "delete_metahuman": do_delete_metahuman,
    "list_metahumans": do_list_metahumans,
    "package_windows": do_package_windows,
    "package_linux": do_package_linux,
    "package_all": do_package_all,
}


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------
class AutomationMaster:
    """Programmatic interface for automation dispatch."""

    @staticmethod
    def available_actions():
        return {k: v.description for k, v in ACTIONS.items()}

    @staticmethod
    def run(action_name, args=None):
        if action_name not in ACTION_HANDLERS:
            raise ValueError(f"Unknown action: {action_name}. Available: {list(ACTIONS.keys())}")

        # Packaging actions don't need the editor
        needs_editor = not action_name.startswith("package_")

        re = None
        if needs_editor:
            remote_execution.set_log_level(logging.WARNING)
            re, node_id = connect_to_editor()
            print(f"Connected to editor: {node_id}")

        try:
            return ACTION_HANDLERS[action_name](re, args or [])
        finally:
            if re:
                disconnect(re)


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------
def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help", "help"):
        print("Embody Automation Master")
        print("=" * 40)
        print()
        print("Usage: python master.py <action> [args...]")
        print()
        print("Actions:")
        for name, action in ACTIONS.items():
            print(f"  {name:25s} {action.description}")
        print()
        print("Examples:")
        print("  python master.py create_metahuman")
        print("  python master.py delete_metahuman MH_Random_abc12345")
        print("  python master.py list_metahumans")
        print("  python master.py package_windows")
        print("  python master.py package_linux --shipping")
        print("  python master.py package_all --clean")
        return

    action_name = sys.argv[1]
    args = sys.argv[2:]

    if action_name not in ACTION_HANDLERS:
        print(f"Unknown action: {action_name}")
        print(f"Available: {', '.join(ACTIONS.keys())}")
        sys.exit(1)

    needs_editor = not action_name.startswith("package_")

    re = None
    if needs_editor:
        remote_execution.set_log_level(logging.WARNING)
        try:
            re, node_id = connect_to_editor()
        except ConnectionError as e:
            print(f"ERROR: {e}")
            sys.exit(1)
        print(f"Connected to editor: {node_id}")

    try:
        success = ACTION_HANDLERS[action_name](re, args)
    finally:
        if re:
            disconnect(re)

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
