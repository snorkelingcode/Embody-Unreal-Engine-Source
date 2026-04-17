"""
Delete MetaHuman(s) and all their assembled assets.

Usage: Set METAHUMANS_TO_DELETE list below, then run via remote execution.
Deletes both the assembled folder (BP, meshes, textures) and the
MetaHumanCharacter asset at the root level.
"""

import unreal

METAHUMANS_TO_DELETE = [
    # Add MetaHuman names to delete here, e.g.:
    # "MH_Random_abc12345",
]

METAHUMAN_ROOT = "/Game/MetaHumans"
CHARACTER_ROOT = "/Game/Characters/MetaHumans"


def delete_metahuman(name):
    deleted = False

    # Directories where assembled assets live (BP, meshes, textures, materials)
    dirs_to_delete = [
        f"{METAHUMAN_ROOT}/{name}",
        f"{CHARACTER_ROOT}/{name}",
    ]

    for dir_path in dirs_to_delete:
        if unreal.EditorAssetLibrary.does_directory_exist(dir_path):
            unreal.log(f"  Deleting directory: {dir_path}")
            unreal.EditorAssetLibrary.delete_directory(dir_path)
            deleted = True

    # Root-level MetaHumanCharacter assets (the .uasset that sits alongside the folder)
    assets_to_delete = [
        f"{METAHUMAN_ROOT}/{name}",
        f"{CHARACTER_ROOT}/{name}",
    ]

    for asset_path in assets_to_delete:
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            unreal.log(f"  Deleting asset: {asset_path}")
            unreal.EditorAssetLibrary.delete_asset(asset_path)
            deleted = True

    if deleted:
        unreal.log(f"  Deleted {name}")
    else:
        unreal.log_warning(f"  {name} not found - skipping")

    return deleted


if len(METAHUMANS_TO_DELETE) == 0:
    unreal.log_warning("METAHUMANS_TO_DELETE is empty - nothing to delete")
else:
    count = 0
    for mh_name in METAHUMANS_TO_DELETE:
        unreal.log(f"Deleting {mh_name}...")
        if delete_metahuman(mh_name):
            count += 1
    unreal.log(f"=== Done! Deleted {count}/{len(METAHUMANS_TO_DELETE)} MetaHumans ===")
