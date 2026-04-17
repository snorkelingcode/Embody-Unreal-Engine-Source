# Lighting

Control scene lighting through directional lights and spawned volumetric lights.

## Directional Light (no spawn needed)

The directional light (sun/environment) is always available. No object spawn required.

```
DL_Int_{value}       -- Intensity (0-10)
DL_Color_{r}_{g}_{b} -- Color (r/g/b floats 0-1)
```

## Volumetric Lights (requires spawn)

To use `LIGHTC_` and `LIGHTI_` commands, you must first spawn a `LIGHT.Volume` object. These commands target a specific light by its tag.

### Step 1: Spawn a LIGHT.Volume
```
SPWN_{tag}_LIGHT.Volume_{x}_{y}_{z}_{rotX}_{rotY}_{rotZ}_{scaleX}_{scaleY}_{scaleZ}
```

Example:
```
SPWN_MyLight_LIGHT.Volume_0_100_200_0_-90_90_1_1_1
```

### Step 2: Set color and intensity
```
LIGHTC_MyLight_{r}_{g}_{b}
LIGHTI_MyLight_{value}
```

Example:
```
LIGHTC_MyLight_1.00_0.70_0.40
LIGHTI_MyLight_5.00
```

### Step 3 (optional): Reposition the light
```
SPWNMOVE_MyLight_{x}_{y}_{z}_{rotX}_{rotY}_{rotZ}_{scaleX}_{scaleY}_{scaleZ}
```

## Presets and Animated Effects

Static presets and animated multi-light effects are also available. See the Commands/ directory for the full list.
