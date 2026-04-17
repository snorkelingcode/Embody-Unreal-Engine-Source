import unreal

bpi = unreal.BlueprintIntrospectionBFL

for mh, bp_name in [("Zach", "BP_Zach"), ("MH_Random_e62143b9", "BP_MH_Random_e62143b9")]:
    bp = unreal.load_asset(f"/Game/MetaHumans/{mh}/{bp_name}.{bp_name}")
    if not bp:
        unreal.log_warning(f"{mh}: not found")
        continue

    unreal.log(f"\n=== {bp_name} ===")

    # CharacterMesh0 from CDO
    try:
        gen = bp.generated_class()
        cdo = unreal.get_default_object(gen)
        mesh = cdo.get_editor_property("mesh")
        if mesh:
            loc = mesh.get_editor_property("relative_location")
            rot = mesh.get_editor_property("relative_rotation")
            unreal.log(f"  Mesh(CDO): loc=({loc.x:.1f},{loc.y:.1f},{loc.z:.1f}) rot=(P{rot.pitch:.1f},Y{rot.yaw:.1f},R{rot.roll:.1f})")
    except Exception as e:
        unreal.log(f"  Mesh(CDO): {e}")

    # All SCS components
    comps = bpi.get_scs_components(bp)
    for c in comps:
        loc = c.relative_location
        rot = c.relative_rotation
        line = f"  {c.variable_name}: loc=({loc.x:.1f},{loc.y:.1f},{loc.z:.1f}) rot=(P{rot.pitch:.1f},Y{rot.yaw:.1f},R{rot.roll:.1f})"
        if c.arm_length > 0:
            line += f" arm={c.arm_length:.0f}"
        if c.variable_name in ["Root", "Body", "Face", "SpringArm", "CineCamera", "Wings",
                                 "Feet", "Head", "Legs", "Torso"]:
            unreal.log(line)
