# Scenes API

Controls world building, object spawning, environment presets, lighting, and backgrounds via Pixel Streaming DataChannel (WebRTC). Be cautious using these.
The preset worlds can be heavy in terms of cost on performance.

## Directory Structure

### Build-Your-Own/
Spawn, move, and delete 3D objects in the scene. Supports 35+ object categories with full 9-axis transform control (position XYZ, rotation XYZ, scale XYZ). Includes Procedural Rock generation and LIGHT.Volume spawnable lights.

### Presets/
Pre-built environment presets loaded from Enviornments.txt. Each preset sends a batch of SPWN commands to construct a full scene (Office, Classroom, Clothing Store, Cave, Sci-Fi Burn Area, VTuber Room).

### Lighting/
Directional light controls (DL_Int_, DL_Color_) and per-object volumetric light controls (LIGHTC_, LIGHTI_) with static presets and animated multi-light effects.

### Background-Image/
Set a background image from a URL on a Live.Plane object.

### Background-Live-Video/
Stream live .m3u8 video to a Live.Plane object or UI overlay.
