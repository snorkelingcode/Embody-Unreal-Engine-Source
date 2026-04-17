"""
Launcher: sends a deferred execution command to the running UE5 editor.

The key insight: blocking cloud operations (auto-rig, texture bake) crash when
called from Python Remote Execution because remote exec runs on a ticker callback,
and WaitForCloudRequests re-enters the ticker causing a shared pointer crash.

Fix: use register_slate_post_tick_callback to defer the script execution to run
OUTSIDE the remote execution ticker context. Remote exec just registers the
callback and returns immediately. The heavy work runs on the next frame.
"""

import sys
import time
import socket
import logging

sys.path.insert(0, r"C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution

SCRIPT_PATH = r"C:/Users/danek/OneDrive/Documents/Unreal Projects/Embody/Automation/MetaHumans/random_metahuman_generator.py"

# This small bootstrap runs inside the editor via remote execution.
# It registers a deferred callback that runs our actual script on the next frame,
# safely outside the remote execution ticker context.
BOOTSTRAP_CODE = f'''
import unreal

_script_path = r"{SCRIPT_PATH}"

def _run_deferred_metahuman_generator(delta_time):
    unreal.unregister_slate_post_tick_callback(_deferred_handle)
    unreal.log("Deferred execution starting...")
    try:
        with open(_script_path, "r", encoding="utf-8") as f:
            exec(f.read(), {{"__name__": "__main__"}})
    except Exception as e:
        unreal.log_error(f"MetaHuman generator failed: {{e}}")
        import traceback
        unreal.log_error(traceback.format_exc())

_deferred_handle = unreal.register_slate_post_tick_callback(_run_deferred_metahuman_generator)
unreal.log("MetaHuman generator deferred - will run on next frame")
'''

remote_execution.set_log_level(logging.WARNING)

config = remote_execution.RemoteExecutionConfig()
config.multicast_group_endpoint = ('239.0.0.1', 6766)
config.multicast_bind_address = '127.0.0.1'
config.multicast_ttl = 0
config.command_endpoint = ('127.0.0.1', 6776)

print("Connecting to UE5 editor...")

re = remote_execution.RemoteExecution(config)
re.start()

nodes = []
for i in range(20):
    time.sleep(0.5)
    nodes = re.remote_nodes
    if nodes:
        break

if not nodes:
    print("ERROR: No UE5 editor found!")
    re.stop()
    sys.exit(1)

node_id = nodes[0]["node_id"]
re.open_command_connection(node_id)
print(f"Connected to editor: {node_id}")

print("Sending deferred execution command...")
result = re.run_command(BOOTSTRAP_CODE, unattended=True, exec_mode=remote_execution.MODE_EXEC_FILE)

if result.get("success"):
    print("Command accepted! Script is now running inside the editor.")
    print()
    print("The MetaHuman generator is executing in the editor process.")
    print("Watch the UE5 Output Log for progress (auto-rig, texture bake, assembly).")
    print("This will take several minutes.")
else:
    print(f"FAILED to queue script: {result.get('result', 'Unknown error')}")

re.close_command_connection()
re.stop()
