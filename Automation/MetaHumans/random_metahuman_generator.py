"""
Random MetaHuman Generator
==========================
Creates a fully assembled MetaHumanCharacter with randomised appearance.

Pipeline:
  1. Create MetaHumanCharacter asset
  2. Randomise grooms, body, face, skin, eyes, teeth, eyelashes, makeup
  3. Auto-rig (joints + blendshapes)
  4. Bake hi-res textures (face + body)
  5. Assemble into a full MetaHuman (BP, skeletal meshes, materials)
  6. Save to disk
"""

import unreal
import random
import uuid


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
ASSET_NAME = f"MH_Random_{uuid.uuid4().hex[:8]}"
PACKAGE_PATH = "/Game/MetaHumans"
BUILD_PATH = "/Game/MetaHumans"
COMMON_FOLDER = "/Game/MetaHumans/Common"

# Set True to skip cloud operations (auto-rig, texture bake) that require
# valid Epic/MetaHuman authentication. Assembly will still run but without
# hi-res textures or blendshape rig.
SKIP_CLOUD = False

MAX_FACE_DELTA = 0.08  # Subtle face variation for realistic results


def _rand01():
    return random.random()

def _rand_range(lo, hi):
    return random.uniform(lo, hi)

def _rand_color():
    return unreal.LinearColor(r=_rand01(), g=_rand01(), b=_rand01(), a=1.0)


# ---------------------------------------------------------------------------
# 1. Create the asset
# ---------------------------------------------------------------------------
def create_character():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    character = asset_tools.create_asset(
        asset_name=ASSET_NAME,
        package_path=PACKAGE_PATH,
        asset_class=unreal.MetaHumanCharacter,
        factory=unreal.new_object(type=unreal.MetaHumanCharacterFactoryNew),
    )
    if character is None:
        raise RuntimeError("Failed to create MetaHuman Character asset")
    unreal.log(f"Created MetaHuman Character: {PACKAGE_PATH}/{ASSET_NAME}")
    return character


# ---------------------------------------------------------------------------
# 2. Copy body type from a reference character (Lucy, Sarah, Zach, or Harry)
# ---------------------------------------------------------------------------
BODY_TYPE_REFS = ["Lucy", "Sarah", "Zach", "Harry"]
FEMALE_BODY_TYPES = {"Lucy", "Sarah"}
MALE_BODY_TYPES = {"Zach", "Harry"}

# Maps ref name -> (folder, body mesh asset name)
BODY_MESH_MAP = {
    "Lucy": ("Lucy", "SKM_Lucy_BodyMesh"),
    "Sarah": ("Sarah2", "SKM_Sarah_BodyMesh"),
    "Zach": ("Zach", "SKM_Zach_BodyMesh"),
    "Harry": ("Harry", "SKM_Harry_BodyMesh"),
}

# Per-character transforms for all components that differ between references.
# Verified via C++ BlueprintIntrospectionBFL reading actual BP data.
# FRotator stores (Pitch, Yaw, Roll) but Python unreal.Rotator passes through
# differently — we store raw (Pitch, Yaw, Roll) and construct FRotator in C++.
# For Python set_editor_property("relative_rotation"), use pitch=X, yaw=Z mapping.
BODY_REF_TRANSFORMS = {
    "Lucy": {
        "CharacterMesh0": {"loc": (0, 0, -88), "yaw": 0},
        "Root": {"loc": (0, 0, 0), "yaw": 90},
        "Body": {"loc": (0, 0, 0), "yaw": 0},
        "SpringArm": {"loc": (0, 2, 55), "yaw": 0, "arm_length": 50},
        "Wings": {"loc": (5.72, -6.96, 90.23), "yaw": -71.64},
    },
    "Harry": {
        "CharacterMesh0": {"loc": (0, 0, -90), "yaw": 0},
        "Root": {"loc": (0, 0, 0), "yaw": 90},
        "Body": {"loc": (0, 0, 0), "yaw": 0},
        "SpringArm": {"loc": (0, 2, 58), "yaw": 0, "arm_length": 500},
        "Wings": {"loc": (0, 0, 0), "yaw": 0},
    },
    "Zach": {
        "CharacterMesh0": {"loc": (0, 0, -88), "yaw": -188},
        "Root": {"loc": (0, 0, 0), "yaw": 0},
        "Body": {"loc": (0, 0, 0), "yaw": 90},
        "SpringArm": {"loc": (0, 2, 70), "yaw": 0, "arm_length": 500},
        "Wings": {"loc": (0, 0, 0), "yaw": 0},
    },
    "Sarah": {
        "CharacterMesh0": {"loc": (0, 0, -88), "yaw": 0},
        "Root": {"loc": (0, 0, 0), "yaw": 0},
        "Body": {"loc": (0, 0, 0), "yaw": 0},
        "SpringArm": {"loc": (0, 2, 56), "yaw": 0, "arm_length": 200},
        "Wings": {"loc": (0, 0, 0), "yaw": 0},
    },
}


def choose_body_ref():
    """Pick a random body type reference. Returns the ref name."""
    return random.choice(BODY_TYPE_REFS)


def conform_body_to_ref(subsystem, character, ref_name):
    """Conform the new character's body to match a reference character's body mesh
    using the MetaHuman conform_body API with match_vertices_by_UV."""
    if ref_name not in BODY_MESH_MAP:
        unreal.log_error(f"No body mesh mapping for {ref_name}")
        return

    ref_folder, ref_mesh_name = BODY_MESH_MAP[ref_name]
    body_mesh_path = f"{BUILD_PATH}/{ref_folder}/Body/{ref_mesh_name}.{ref_mesh_name}"
    body_mesh = unreal.load_asset(body_mesh_path)

    if not body_mesh:
        # Try alternate path
        body_mesh_path = f"{BUILD_PATH}/{ref_folder}/Body/SKM_{ref_folder}_BodyMesh.SKM_{ref_folder}_BodyMesh"
        body_mesh = unreal.load_asset(body_mesh_path)

    if not body_mesh:
        unreal.log_error(f"Could not load body mesh for {ref_name}")
        return

    # Get mesh vertices for body conforming (match by UV for accurate shape transfer)
    result, vertices = subsystem.get_mesh_for_body_conforming(
        character, body_mesh, None, match_vertices_by_u_vs=True
    )
    if result != unreal.ImportErrorCode.SUCCESS:
        unreal.log_error(f"Failed to get mesh vertices for body conforming: {result}")
        return

    # Get joint data from the reference body mesh
    result, joint_world_translations, joint_rotations = subsystem.get_joints_for_body_conforming(body_mesh)
    if result != unreal.ImportErrorCode.SUCCESS:
        unreal.log_error(f"Failed to get joints for body conforming: {result}")
        return

    # Conform the body
    success = subsystem.conform_body(
        character, vertices, joint_rotations, repose=True, estimate_joints_from_mesh=False
    )

    if success:
        unreal.log(f"Conformed body to {ref_name} ({ref_mesh_name})")
    else:
        unreal.log_error(f"Failed to conform body to {ref_name}")


# ---------------------------------------------------------------------------
# 3. Randomise face landmarks
# ---------------------------------------------------------------------------
def randomise_face(subsystem, character):
    face_landmarks = subsystem.get_face_landmarks(character=character)
    if len(face_landmarks) == 0:
        unreal.log_warning("No face landmarks - skipping")
        return

    deltas = [
        unreal.Vector(
            x=_rand_range(-MAX_FACE_DELTA, MAX_FACE_DELTA),
            y=_rand_range(-MAX_FACE_DELTA, MAX_FACE_DELTA),
            z=_rand_range(-MAX_FACE_DELTA, MAX_FACE_DELTA),
        )
        for _ in face_landmarks
    ]

    subsystem.translate_face_landmarks(
        character=character,
        landmark_indices=list(range(len(deltas))),
        deltas=deltas,
    )
    subsystem.commit_face_state(character=character)
    unreal.log(f"Randomised {len(face_landmarks)} face landmarks")


# ---------------------------------------------------------------------------
# 4. Randomise skin
# ---------------------------------------------------------------------------
def randomise_skin(subsystem, character):
    skin_props = unreal.MetaHumanCharacterSkinProperties()
    skin_props.u = _rand01()
    skin_props.v = _rand01()
    skin_props.show_top_underwear = random.choice([True, False])
    # Body and face texture indices must match (same character texture set)
    # Valid range is 0-8 (9 character texture sets available)
    tex_index = random.randint(0, 8)
    skin_props.body_texture_index = tex_index
    skin_props.face_texture_index = tex_index
    skin_props.roughness = _rand_range(0.3, 1.0)

    freckles = unreal.MetaHumanCharacterFrecklesProperties()
    freckles.density = _rand_range(0.0, 0.3)
    freckles.strength = _rand_range(0.0, 0.3)
    freckles.saturation = _rand_range(0.3, 0.7)
    freckles.tone_shift = _rand_range(0.3, 0.6)
    freckles.mask = unreal.MetaHumanCharacterFrecklesMask.TYPE2

    def _rand_accent():
        a = unreal.MetaHumanCharacterAccentRegionProperties()
        a.lightness = _rand_range(0.4, 0.6)
        a.redness = _rand_range(0.3, 0.6)
        a.saturation = _rand_range(0.3, 0.6)
        return a

    accents = unreal.MetaHumanCharacterAccentRegions()
    accents.cheeks = _rand_accent()
    accents.chin = _rand_accent()
    accents.ears = _rand_accent()
    accents.forehead = _rand_accent()
    accents.lips = _rand_accent()
    accents.nose = _rand_accent()
    accents.scalp = _rand_accent()
    accents.under_eye = _rand_accent()

    skin_settings = unreal.MetaHumanCharacterSkinSettings()
    skin_settings.skin = skin_props
    skin_settings.freckles = freckles
    skin_settings.accents = accents
    skin_settings.enable_texture_overrides = False

    character.preview_material_type = unreal.MetaHumanCharacterSkinPreviewMaterial.EDITABLE
    subsystem.commit_skin_settings(character, skin_settings)
    unreal.log("Randomised skin settings")


# ---------------------------------------------------------------------------
# 5. Randomise eyes
# ---------------------------------------------------------------------------
IRIS_PATTERNS = [unreal.MetaHumanCharacterEyesIrisPattern.IRIS002]
for _p in ("IRIS001", "IRIS003", "IRIS004"):
    try:
        IRIS_PATTERNS.append(getattr(unreal.MetaHumanCharacterEyesIrisPattern, _p))
    except AttributeError:
        pass

BLEND_METHODS = [unreal.MetaHumanCharacterEyesBlendMethod.RADIAL]


def _rand_realistic_eye():
    """Generate a realistic eye with natural iris colors."""
    iris = unreal.MetaHumanCharacterEyeIrisProperties()
    iris.pattern = random.choice(IRIS_PATTERNS)
    iris.rotation = _rand_range(0.0, 0.2)
    # Natural iris color UV ranges (brown, blue, green, hazel, grey)
    iris.primary_color_u = _rand_range(0.1, 0.9)
    iris.primary_color_v = _rand_range(0.1, 0.9)
    iris.secondary_color_u = iris.primary_color_u + _rand_range(-0.15, 0.15)
    iris.secondary_color_v = iris.primary_color_v + _rand_range(-0.15, 0.15)
    iris.color_blend = _rand_range(0.3, 0.7)
    iris.color_blend_softness = _rand_range(0.3, 0.7)
    iris.blend_method = random.choice(BLEND_METHODS)
    iris.shadow_details = _rand_range(0.3, 0.7)
    iris.limbal_ring_size = _rand_range(0.5, 0.8)
    iris.limbal_ring_softness = _rand_range(0.02, 0.06)
    iris.limbal_ring_color = unreal.LinearColor(r=0.02, g=0.02, b=0.02, a=1.0)
    iris.global_saturation = _rand_range(0.8, 1.5)
    iris.global_tint = unreal.LinearColor(r=1.0, g=1.0, b=1.0, a=1.0)

    pupil = unreal.MetaHumanCharacterEyePupilProperties()
    pupil.dilation = _rand_range(0.4, 0.6)
    pupil.feather = _rand_range(0.4, 0.6)

    sclera = unreal.MetaHumanCharacterEyeScleraProperties()
    sclera.rotation = 0.0
    sclera.use_custom_tint = False
    sclera.transmission_spread = _rand_range(0.2, 0.5)

    eye = unreal.MetaHumanCharacterEyeProperties()
    eye.iris = iris
    eye.pupil = pupil
    eye.sclera = sclera
    return eye


def randomise_eyes(subsystem, character):
    # Both eyes should match for a natural look
    eye = _rand_realistic_eye()
    eyes_settings = unreal.MetaHumanCharacterEyesSettings()
    eyes_settings.eye_left = eye
    eyes_settings.eye_right = eye
    subsystem.commit_eyes_settings(character=character, eyes_settings=eyes_settings)
    unreal.log("Randomised eye settings")


# ---------------------------------------------------------------------------
# 6. Randomise teeth & eyelashes
# ---------------------------------------------------------------------------
def randomise_head_model(subsystem, character):
    head_model_settings = character.head_model_settings

    teeth = head_model_settings.teeth
    teeth.tooth_length = _rand_range(-0.2, 0.2)
    teeth.tooth_spacing = _rand_range(-0.15, 0.15)
    teeth.upper_shift = _rand_range(-0.1, 0.1)
    teeth.lower_shift = _rand_range(-0.1, 0.1)
    teeth.overbite = _rand_range(-0.2, 0.2)
    teeth.overjet = _rand_range(-0.15, 0.15)
    teeth.worn_down = _rand_range(-0.1, 0.2)
    teeth.polycanine = _rand_range(0.0, 0.1)
    teeth.receding_gums = _rand_range(0.0, 0.1)
    teeth.narrowness = _rand_range(-0.2, 0.2)
    teeth.variation = _rand_range(0.0, 0.3)
    # Natural tooth/gum colors
    teeth.teeth_color = unreal.LinearColor(
        r=_rand_range(0.85, 1.0), g=_rand_range(0.82, 0.95), b=_rand_range(0.7, 0.85), a=1.0)
    teeth.gum_color = unreal.LinearColor(
        r=_rand_range(0.55, 0.75), g=_rand_range(0.25, 0.4), b=_rand_range(0.25, 0.4), a=1.0)
    teeth.plaque_color = unreal.LinearColor(
        r=_rand_range(0.8, 0.95), g=_rand_range(0.75, 0.9), b=_rand_range(0.5, 0.7), a=1.0)
    teeth.jaw_open = 0.0
    teeth.plaque_amount = _rand_range(0.0, 0.1)

    eyelashes = head_model_settings.eyelashes
    eyelashes.type = unreal.MetaHumanCharacterEyelashesType.THICK_CURL
    # Natural dark eyelash colors
    eyelashes.dye_color = unreal.LinearColor(
        r=_rand_range(0.02, 0.15), g=_rand_range(0.01, 0.1), b=_rand_range(0.01, 0.08), a=1.0)
    eyelashes.melanin = _rand_range(0.6, 1.0)
    eyelashes.redness = _rand_range(0.0, 0.2)
    eyelashes.roughness = _rand_range(0.4, 0.7)
    eyelashes.salt_and_pepper = _rand_range(0.0, 0.05)
    eyelashes.lightness = _rand_range(0.0, 0.2)
    eyelashes.enable_grooms = True

    subsystem.commit_head_model_settings(
        meta_human_character=character, head_model_settings=head_model_settings
    )
    unreal.log("Randomised teeth & eyelash settings")


# ---------------------------------------------------------------------------
# 7. Randomise makeup
# ---------------------------------------------------------------------------
def randomise_makeup(subsystem, character, body_ref_name):
    """Set makeup based on gender. Male = none. Female = subtle/light."""
    is_female = body_ref_name in FEMALE_BODY_TYPES

    blusher = unreal.MetaHumanCharacterBlushMakeupProperties()
    blusher.type = unreal.MetaHumanCharacterBlushMakeupType.HIGH_CURVE
    blusher.roughness = 0.5

    eyes_makeup = unreal.MetaHumanCharacterEyeMakeupProperties()
    eyes_makeup.type = unreal.MetaHumanCharacterEyeMakeupType.DRAMATIC_SMUDGE
    eyes_makeup.primary_color = unreal.LinearColor(r=0.15, g=0.1, b=0.1, a=1.0)
    eyes_makeup.secondary_color = unreal.LinearColor(r=0.2, g=0.15, b=0.15, a=1.0)

    foundation = unreal.MetaHumanCharacterFoundationMakeupProperties()
    foundation.color = unreal.LinearColor(r=0.8, g=0.65, b=0.55, a=1.0)
    foundation.roughness = 0.5

    lips = unreal.MetaHumanCharacterLipsMakeupProperties()
    lips.type = unreal.MetaHumanCharacterLipsMakeupType.HOLLYWOOD
    lips.roughness = 0.5

    if is_female:
        # Subtle feminine makeup
        blusher.color = unreal.LinearColor(
            r=_rand_range(0.7, 0.9), g=_rand_range(0.35, 0.5), b=_rand_range(0.35, 0.5), a=1.0)
        blusher.intensity = _rand_range(0.0, 0.1)

        eyes_makeup.metalness = 0.0
        eyes_makeup.opacity = _rand_range(0.0, 0.08)

        foundation.apply_foundation = False
        foundation.intensity = 0.0
        foundation.concealer = 0.0

        lips.color = unreal.LinearColor(
            r=_rand_range(0.5, 0.7), g=_rand_range(0.25, 0.4), b=_rand_range(0.25, 0.35), a=1.0)
        lips.metalness = 0.0
        lips.opacity = _rand_range(0.0, 0.1)
    else:
        # No makeup for male
        blusher.color = unreal.LinearColor(r=0.8, g=0.4, b=0.4, a=1.0)
        blusher.intensity = 0.0

        eyes_makeup.metalness = 0.0
        eyes_makeup.opacity = 0.0

        foundation.apply_foundation = False
        foundation.intensity = 0.0
        foundation.concealer = 0.0

        lips.color = unreal.LinearColor(r=0.6, g=0.3, b=0.3, a=1.0)
        lips.metalness = 0.0
        lips.opacity = 0.0

    makeup_settings = unreal.MetaHumanCharacterMakeupSettings()
    makeup_settings.blush = blusher
    makeup_settings.eyes = eyes_makeup
    makeup_settings.foundation = foundation
    makeup_settings.lips = lips

    subsystem.commit_makeup_settings(character=character, makeup_settings=makeup_settings)
    label = "subtle feminine" if is_female else "none (male)"
    unreal.log(f"Set makeup: {label}")


# ---------------------------------------------------------------------------
# 8. Randomise grooms
# ---------------------------------------------------------------------------
GROOM_SLOTS = ["Hair", "Eyebrows", "Beard", "Mustache", "Eyelashes", "Peachfuzz"]
GROOM_PACKAGE_MAP = {
    "Hair": "Hair",
    "Eyebrows": "Eyebrows",
    "Beard": "Beards",
    "Mustache": "Mustaches",
    "Eyelashes": "Eyelashes",
    "Peachfuzz": "Peachfuzz",
}


def randomise_grooms(character, body_ref_name):
    """Select random grooms. Skips Beard/Mustache for female body types."""
    is_female = body_ref_name in FEMALE_BODY_TYPES
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    groom_items = {}

    for slot in GROOM_SLOTS:
        # Skip facial hair for female body types
        if is_female and slot in ("Beard", "Mustache"):
            continue

        package_path = f"/MetaHumanCharacter/Optional/Grooms/Bindings/{GROOM_PACKAGE_MAP[slot]}"
        ar_filter = unreal.ARFilter(
            package_paths=[package_path],
            class_paths=[unreal.MetaHumanWardrobeItem.static_class().get_class_path_name()],
        )
        asset_data_list = asset_registry.get_assets(filter=ar_filter)

        if len(asset_data_list) == 0:
            unreal.log_warning(f"No wardrobe items for groom slot '{slot}' - skipping")
            continue

        asset_data = random.choice(asset_data_list)
        unreal.log(f"  Groom [{slot}]: {asset_data.asset_name}")

        item_key = character.internal_collection.try_add_item_from_wardrobe_item(
            slot_name=slot, wardrobe_item=asset_data.get_asset()
        )
        if item_key is None:
            unreal.log_error(f"Failed to add wardrobe item for slot {slot}")
            continue

        groom_items[slot] = item_key

    for slot, item_key in groom_items.items():
        selection = unreal.MetaHumanPipelineSlotSelection(
            slot_name=slot, selected_item=item_key
        )
        if not character.internal_collection.default_instance.try_add_slot_selection(selection=selection):
            unreal.log_error(f"Failed to select {item_key.to_asset_name_string()} for slot {slot}")

    unreal.log(f"Randomised grooms ({len(groom_items)} slots)")
    return groom_items


# ---------------------------------------------------------------------------
# 9. Auto-rig (cloud operation - requires valid Epic/MetaHuman auth)
# ---------------------------------------------------------------------------
def auto_rig(subsystem, character):
    unreal.log("Starting auto-rigging (joints + blendshapes)...")
    unreal.log("  NOTE: This is a cloud operation requiring valid MetaHuman auth.")
    try:
        params = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        params.blocking = True
        params.report_progress = False
        params.rig_type = unreal.MetaHumanRigType.JOINTS_AND_BLEND_SHAPES
        subsystem.request_auto_rigging(character, params)
        unreal.log("Auto-rigging complete")
        return True
    except Exception as e:
        unreal.log_error(f"Auto-rigging failed (cloud auth may be expired): {e}")
        unreal.log_error("  Fix: sign in via Edit > Plugins > MetaHuman, or restart editor")
        return False


# ---------------------------------------------------------------------------
# 10. Bake hi-res textures (cloud operation)
# ---------------------------------------------------------------------------
def bake_textures(subsystem, character):
    unreal.log("Requesting hi-res texture sources (face + body)...")
    unreal.log("  NOTE: This is a cloud operation requiring valid MetaHuman auth.")
    try:
        params = unreal.MetaHumanCharacterTextureRequestParams()
        params.blocking = True
        params.report_progress = False
        subsystem.request_texture_sources(character, params)

        if not character.has_high_resolution_textures:
            unreal.log_warning("Character does not report high resolution textures after bake")
            return False
        else:
            unreal.log("Hi-res textures baked successfully")
            return True
    except Exception as e:
        unreal.log_error(f"Texture bake failed (cloud auth may be expired): {e}")
        return False


# ---------------------------------------------------------------------------
# 11. Assemble (build the full MetaHuman BP, meshes, materials)
# ---------------------------------------------------------------------------
def assemble(subsystem, character):
    can_build = subsystem.can_build_meta_human(character=character)
    if not can_build:
        unreal.log_warning("can_build_meta_human returned False.")
        unreal.log_warning("  This usually means auto-rig or texture bake failed.")
        unreal.log_warning("  Attempting assembly anyway...")

    unreal.log("Assembling MetaHuman (OPTIMIZED pipeline, MEDIUM quality, no clothing)...")
    try:
        build_params = unreal.MetaHumanCharacterEditorBuildParameters()
        build_params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        build_params.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
        build_params.absolute_build_path = BUILD_PATH
        build_params.common_folder_path = COMMON_FOLDER
        build_params.enable_wardrobe_item_validation = False
        subsystem.build_meta_human(character=character, params=build_params)
        unreal.log("Assembly complete")
        return True
    except Exception as e:
        unreal.log_error(f"Assembly failed: {e}")
        return False


# ---------------------------------------------------------------------------
# 12. Copy reference BP logic into assembly-generated BP
# ---------------------------------------------------------------------------
# Maps MetaHumanCharacter asset name -> (folder name, BP asset name)
BODY_REF_BP_MAP = {
    "Lucy": ("Lucy", "BP_Lucy"),
    "Sarah": ("Sarah2", "BP_Sarah2"),
    "Zach": ("Zach", "BP_Zach"),
    "Harry": ("Harry", "BP_Harry"),
}


def copy_bp_logic(asset_name, ref_name):
    """Copy event graph, variables, and function graphs from a reference BP
    into the assembly-generated BP. Keeps the assembly BP's component hierarchy
    (correct meshes, grooms, materials) and just adds the gameplay logic."""

    if ref_name not in BODY_REF_BP_MAP:
        unreal.log_warning(f"No BP mapping for {ref_name} - keeping assembly BP as-is")
        return False

    ref_folder, ref_bp_name = BODY_REF_BP_MAP[ref_name]
    src_bp_path = f"{BUILD_PATH}/{ref_folder}/{ref_bp_name}"
    new_bp_name = f"BP_{asset_name}"
    dst_bp_path = f"{BUILD_PATH}/{asset_name}/{new_bp_name}"

    # Load both BPs
    src_bp = unreal.load_asset(f"{src_bp_path}.{ref_bp_name}")
    dst_bp = unreal.load_asset(f"{dst_bp_path}.{new_bp_name}")

    if not src_bp:
        unreal.log_error(f"Reference BP not found: {src_bp_path}")
        return False
    if not dst_bp:
        unreal.log_error(f"Assembly BP not found: {dst_bp_path}")
        return False

    bpi = unreal.BlueprintIntrospectionBFL
    bel = unreal.BlueprintEditorLibrary

    # Step 0: Reparent the assembly BP to Character
    # The assembly creates an Actor BP, but we need Character for Pawn functions
    # (Add Movement Input, Get Control Rotation, etc.) and the inherited Mesh component.
    character_class = unreal.Character.static_class()
    bel.reparent_blueprint(dst_bp, character_class)
    unreal.log(f"  Reparented to Character")

    # Step 1: Copy missing SCS components from reference BP
    # (adds SpringArm, CineCamera, Feet, Head, Legs, Torso, Wings, ZenBlink, Root, etc.)
    scs_count = bpi.copy_scs_nodes(src_bp, dst_bp)
    unreal.log(f"  Copied {scs_count} SCS components from {ref_bp_name}")

    # Step 1b: Reparent Root to CharacterMesh0, and Body under Root
    # Assembly creates Body/Face/grooms as top-level, but they need to be
    # under CharacterMesh0 -> Root -> Body (matching reference hierarchy)
    if bpi.attach_scs_node_to_native(dst_bp, "Root", "CharacterMesh0"):
        unreal.log(f"  Attached Root to CharacterMesh0")

    # Make sure Body is under Root (assembly may have it elsewhere)
    bpi.set_scs_node_parent(dst_bp, "Body", "Root")

    # Step 2: Copy variables from reference to assembly BP
    var_count = bpi.copy_variables(src_bp, dst_bp)
    unreal.log(f"  Copied {var_count} variables from {ref_bp_name}")

    # Step 3: Export event graph from reference BP
    event_graph_text = bpi.export_event_graph(src_bp)
    if not event_graph_text:
        unreal.log_warning(f"  No event graph nodes to copy from {ref_bp_name}")
    else:
        # Clear the assembly BP's event graph and import the reference one
        bpi.clear_event_graph(dst_bp)
        if bpi.import_into_event_graph(dst_bp, event_graph_text):
            unreal.log(f"  Copied event graph from {ref_bp_name}")
        else:
            unreal.log_error(f"  Failed to import event graph")

    # Step 4: Copy function graphs (EnableMasterPose, LiveLinkSetup, etc.)
    func_count = bpi.copy_function_graphs(src_bp, dst_bp)
    unreal.log(f"  Copied {func_count} function graphs from {ref_bp_name}")

    # Step 5: Apply per-character transforms (CharacterMesh0, SpringArm, Body)
    transforms = BODY_REF_TRANSFORMS.get(ref_name, BODY_REF_TRANSFORMS["Lucy"])

    # Helper to build FRotator with yaw only (all our transforms are yaw-only)
    def _yaw_rot(yaw):
        return unreal.Rotator(0, yaw, 0)  # FRotator(Pitch, Yaw, Roll)

    # CharacterMesh0 (inherited) — no mesh, set transform from reference
    try:
        gen_class = dst_bp.generated_class()
        cdo = unreal.get_default_object(gen_class)
        mesh_comp = cdo.get_editor_property("mesh")
        if mesh_comp:
            t = transforms["CharacterMesh0"]
            mesh_comp.set_editor_property("skeletal_mesh_asset", None)
            mesh_comp.set_editor_property("relative_location",
                unreal.Vector(*t["loc"]))
            mesh_comp.set_editor_property("relative_rotation", _yaw_rot(t["yaw"]))
            mesh_comp.set_editor_property("relative_scale3d", unreal.Vector(1, 1, 1))
    except Exception as e:
        unreal.log_warning(f"  Could not set CharacterMesh0 transform: {e}")

    # Apply transforms for Root, Body, SpringArm, Wings from reference config
    # Use SetComponentTransformExplicit to bypass Python Rotator ordering ambiguity
    for comp_name in ["Root", "Body", "SpringArm", "Wings"]:
        if comp_name in transforms:
            t = transforms[comp_name]
            bpi.set_component_transform_explicit(dst_bp, comp_name,
                unreal.Vector(*t["loc"]), 0.0, float(t["yaw"]), 0.0, unreal.Vector(1, 1, 1))

    unreal.log(f"  Applied transforms from {ref_name}")

    # Step 6: Refresh all nodes (reconstructs stale pins from imported graph)
    bpi.refresh_all_nodes(dst_bp)
    unreal.log(f"  Refreshed all nodes")

    # Step 7: Compile
    unreal.BlueprintEditorLibrary.compile_blueprint(dst_bp)
    unreal.log(f"  Compiled {new_bp_name}")

    return True


# ---------------------------------------------------------------------------
# 13. Fix materials (post-assembly workaround)
# ---------------------------------------------------------------------------
CORRECT_BODY_PARENT = "/Game/MetaHumans/Common/Materials/MI_BodySynthesized.MI_BodySynthesized"
CORRECT_FACE_PARENT = "/Game/MetaHumans/Common/Lookdev_UHM/Skin/Materials/Baked/MI_Head_Baked_LOD0.MI_Head_Baked_LOD0"


def fix_body_material(asset_name):
    body_mi_path = f"{BUILD_PATH}/{asset_name}/Body/Materials/MI_Body_Baked.MI_Body_Baked"
    body_mi = unreal.load_asset(body_mi_path)
    correct_parent = unreal.load_asset(CORRECT_BODY_PARENT)

    if not body_mi:
        unreal.log_warning(f"Could not load body MI at {body_mi_path} - skipping fix")
        return
    if not correct_parent:
        unreal.log_warning(f"Could not load correct parent at {CORRECT_BODY_PARENT}")
        return

    # Fix parent
    current_parent = body_mi.get_editor_property("parent")
    current_name = current_parent.get_path_name() if current_parent else "None"
    if "BodySynthesized" not in current_name:
        body_mi.set_editor_property("parent", correct_parent)
        unreal.log(f"Fixed body material parent -> MI_BodySynthesized")

    # The assembly creates texture params with names like "Basecolor Baked",
    # "Normal Baked", "SRMF Baked" — but MI_BodySynthesized expects
    # "Color_MAIN", "Normal_MAIN", "Roughness_MAIN", "Color_UNDERWEAR" etc.
    # Read the baked textures and re-apply with correct param names.
    baked_base = f"{BUILD_PATH}/{asset_name}/Body/Baked"
    t_bc = unreal.load_asset(f"{baked_base}/T_Body_BC.T_Body_BC")
    t_n = unreal.load_asset(f"{baked_base}/T_Body_N.T_Body_N")
    t_srmf = unreal.load_asset(f"{baked_base}/T_Body_SRMF.T_Body_SRMF")

    # Keep existing assembly params (Basecolor Baked, Normal Baked, etc.)
    # AND add the _MAIN params that MI_BodySynthesized expects.
    # Both parameter systems are needed.
    def _set_tex_param(mi, param_name, texture):
        if not texture:
            return
        # Remove existing param with same name (avoid duplicates)
        overrides = [ov for ov in mi.get_editor_property("texture_parameter_values")
                     if str(ov.get_editor_property("parameter_info").get_editor_property("name")) != param_name]
        new_ov = unreal.TextureParameterValue()
        info = unreal.MaterialParameterInfo()
        info.set_editor_property("name", param_name)
        new_ov.set_editor_property("parameter_info", info)
        new_ov.set_editor_property("parameter_value", texture)
        overrides.append(new_ov)
        mi.set_editor_property("texture_parameter_values", overrides)

    def _set_scalar_param(mi, param_name, value):
        # Remove existing param with same name (avoid duplicates)
        overrides = [ov for ov in mi.get_editor_property("scalar_parameter_values")
                     if str(ov.get_editor_property("parameter_info").get_editor_property("name")) != param_name]
        new_ov = unreal.ScalarParameterValue()
        info = unreal.MaterialParameterInfo()
        info.set_editor_property("name", param_name)
        new_ov.set_editor_property("parameter_info", info)
        new_ov.set_editor_property("parameter_value", value)
        overrides.append(new_ov)
        mi.set_editor_property("scalar_parameter_values", overrides)

    _set_tex_param(body_mi, "Color_MAIN", t_bc)
    _set_tex_param(body_mi, "Color_UNDERWEAR", t_bc)
    _set_tex_param(body_mi, "Normal_MAIN", t_n)
    _set_tex_param(body_mi, "Roughness_MAIN", t_srmf)
    _set_scalar_param(body_mi, "BaseColor_Brightness", 1.0)
    _set_scalar_param(body_mi, "RefractionDepthBias", 0.0)

    # Force the material to update its shader/cached state
    try:
        unreal.MaterialEditingLibrary.update_material_instance(body_mi)
    except Exception:
        # Fallback: call PostEditChange via modify
        body_mi.modify()

    unreal.log("Fixed body material texture params (Color_MAIN, Normal_MAIN, Roughness_MAIN)")

    # Open and close the body material in the editor to force shader compilation
    asset_editor = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    asset_editor.open_editor_for_assets([body_mi])
    asset_editor.close_all_editors_for_asset(body_mi)
    unreal.log("Forced body material shader compile")

    # Fix face skin materials — same issue as body.
    # Assembly creates params like "Basecolor Baked" but the parent material
    # (MI_Head_Baked_LOD0) expects "Color_MAIN", "Normal_MAIN", "SRMF_MAIN" etc.
    face_baked = f"{BUILD_PATH}/{asset_name}/Face/Baked"

    for lod_suffix in ["LOD3", "LOD5to7"]:
        face_mi_path = f"{BUILD_PATH}/{asset_name}/Face/Materials/MI_Face_Skin_Baked_{lod_suffix}.MI_Face_Skin_Baked_{lod_suffix}"
        face_mi = unreal.load_asset(face_mi_path)
        if not face_mi:
            continue

        # Fix parent if needed
        face_parent = unreal.load_asset(CORRECT_FACE_PARENT)
        current_parent = face_mi.get_editor_property("parent")
        if face_parent and (not current_parent or "Head_Baked" not in current_parent.get_path_name()):
            face_mi.set_editor_property("parent", face_parent)

        # Load baked face textures for this LOD
        t_bc = unreal.load_asset(f"{face_baked}/T_Head_{lod_suffix}_BC.T_Head_{lod_suffix}_BC")
        t_n = unreal.load_asset(f"{face_baked}/T_Head_{lod_suffix}_N.T_Head_{lod_suffix}_N")
        t_srmf = unreal.load_asset(f"{face_baked}/T_Head_{lod_suffix}_SRMF.T_Head_{lod_suffix}_SRMF")
        t_scatter = unreal.load_asset(f"{face_baked}/T_Head_{lod_suffix}_Scatter.T_Head_{lod_suffix}_Scatter")

        # Keep existing assembly params (Basecolor Baked, Normal Baked, etc.)
        # AND add the _MAIN params that the parent material also expects.
        # Both parameter systems drive different parts of the face shader.
        _set_tex_param(face_mi, "Color_MAIN", t_bc)
        _set_tex_param(face_mi, "Normal_MAIN", t_n)
        _set_tex_param(face_mi, "Roughness_MAIN", t_srmf)
        if t_scatter:
            _set_tex_param(face_mi, "Scatter_MAIN", t_scatter)

        # Force update
        try:
            unreal.MaterialEditingLibrary.update_material_instance(face_mi)
        except Exception:
            face_mi.modify()

        unreal.log(f"Fixed face material {lod_suffix} texture params")


# ---------------------------------------------------------------------------
# 13. Save all assembled assets
# ---------------------------------------------------------------------------
def save_all_metahuman_assets(asset_name):
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    ar_filter = unreal.ARFilter(
        package_paths=[f"{BUILD_PATH}/{asset_name}"],
        recursive_paths=True
    )
    all_assets = ar.get_assets(filter=ar_filter)
    for a in all_assets:
        unreal.EditorAssetLibrary.save_asset(str(a.package_name))
    # Also save the character asset itself
    unreal.EditorAssetLibrary.save_asset(f"{PACKAGE_PATH}/{asset_name}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def generate_random_metahuman():
    unreal.log("=" * 60)
    unreal.log("=== Random MetaHuman Generator ===")
    unreal.log("=" * 60)

    # Step 1: Create asset
    character = create_character()

    # Step 2: Choose body type
    subsystem = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
    chosen_body_ref = choose_body_ref()
    unreal.log(f"Chosen body type: {chosen_body_ref}")

    # Step 3: Add grooms (gender-aware — skips beard/mustache for female body types)
    groom_items = randomise_grooms(character, chosen_body_ref)

    # Step 4: Open new character for editing
    if not subsystem.try_add_object_to_edit(character):
        raise RuntimeError("Unable to open character for editing")

    try:
        # Step 5: Conform body to reference mesh and randomise appearance
        conform_body_to_ref(subsystem, character, chosen_body_ref)
        randomise_face(subsystem, character)
        randomise_skin(subsystem, character)
        randomise_eyes(subsystem, character)
        randomise_head_model(subsystem, character)
        randomise_makeup(subsystem, character, chosen_body_ref)

        # Assemble for preview to populate groom instance parameters
        subsystem.assemble_for_preview(character=character)

        # Randomise groom instance parameters
        for slot, item_key in groom_items.items():
            item_path = unreal.MetaHumanPaletteItemPath(item_key=item_key)
            instance_params = character.internal_collection.default_instance.get_instance_parameters(
                item_path=item_path
            )
            for param in instance_params:
                if param.type == unreal.MetaHumanCharacterInstanceParameterType.FLOAT:
                    param.set_float(value=_rand01())
                elif param.type == unreal.MetaHumanCharacterInstanceParameterType.BOOL:
                    param.set_bool(value=random.choice([True, False]))
                elif param.type == unreal.MetaHumanCharacterInstanceParameterType.COLOR:
                    param.set_color(value=_rand_color())

        # Step 5: Auto-rig (cloud) - skip if SKIP_CLOUD is True
        rig_ok = False
        tex_ok = False
        if not SKIP_CLOUD:
            rig_ok = auto_rig(subsystem, character)
            tex_ok = bake_textures(subsystem, character)
        else:
            unreal.log("Skipping cloud operations (SKIP_CLOUD=True)")

        # Step 6: Assemble full MetaHuman
        if rig_ok and tex_ok:
            assemble(subsystem, character)
        elif not SKIP_CLOUD:
            unreal.log_warning("Cloud operations failed - attempting assembly anyway...")
            assemble(subsystem, character)
        else:
            unreal.log("Skipped cloud ops. Assembling without rig/textures...")
            assemble(subsystem, character)

    finally:
        if subsystem.is_object_added_for_editing(character):
            subsystem.remove_object_to_edit(character)

    # Step 7: Fix body material parent (assembly bug assigns MI_Head_Baked_LOD0
    # instead of MI_BodySynthesized)
    fix_body_material(ASSET_NAME)

    # Step 8: Duplicate reference character's BP (copies all blueprint nodes/logic)
    copy_bp_logic(ASSET_NAME, chosen_body_ref)

    # Step 9: Save all assembled assets
    save_all_metahuman_assets(ASSET_NAME)
    unreal.log(f"Saved all assets")

    unreal.log("=" * 60)
    unreal.log(f"=== Done! MetaHuman: {ASSET_NAME} ===")
    unreal.log(f"=== Location: {BUILD_PATH}/{ASSET_NAME}/ ===")
    unreal.log("=" * 60)
    return character


if __name__ == "__main__":
    generate_random_metahuman()
