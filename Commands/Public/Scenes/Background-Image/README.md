# Background Image

Set a static image as a background in the 3D scene.

## Prerequisites

You must first spawn a `Live.Plane` object before sending the `BACK_` command. The image is rendered onto the plane in 3D space.

### Step 1: Spawn a Live.Plane
```
SPWN_{tag}_Live.Plane_{x}_{y}_{z}_{rotX}_{rotY}_{rotZ}_{scaleX}_{scaleY}_{scaleZ}
```

Example:
```
SPWN_MyBackground_Live.Plane_100_0_100_0_0_180_0.8_0.8_0.8
```

### Step 2: Set the background image
```
BACK_{url}
```

Example:
```
BACK_https://drive.google.com/your-image-link
```

### Step 3 (optional): Reposition the plane
```
SPWNMOVE_MyBackground_{x}_{y}_{z}_{rotX}_{rotY}_{rotZ}_{scaleX}_{scaleY}_{scaleZ}
```
