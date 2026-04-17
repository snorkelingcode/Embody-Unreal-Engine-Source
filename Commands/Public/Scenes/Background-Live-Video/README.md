# Background Live Video

Stream live .m3u8 (HLS) video into the scene or as a UI overlay.

## Prerequisites

To stream video onto a 3D plane, you must first spawn a `Live.Plane` object. The `UI` target mode does not require a spawned object.

### Plane Mode (3D scene)

#### Step 1: Spawn a Live.Plane
```
SPWN_{tag}_Live.Plane_{x}_{y}_{z}_{rotX}_{rotY}_{rotZ}_{scaleX}_{scaleY}_{scaleZ}
```

Example:
```
SPWN_VideoWall_Live.Plane_100_0_100_0_0_180_0.8_0.8_0.8
```

#### Step 2: Send the video stream
```
m3u8_Plane_{url}
```

Example:
```
m3u8_Plane_https://example.com/stream.m3u8
```

### UI Mode (2D screen overlay)

No spawned object needed. The video renders as a UI overlay on screen.

```
m3u8_UI_{url}
```
