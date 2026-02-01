# SkyScript Scenery API

The Scenery API provides access to X-Plane's scenery system including object loading, terrain probing, and magnetic variation.

## Overview

```typescript
// Access through the XPlane global
XPlane.scenery.loadObject(path);
XPlane.scenery.probeTerrain(probeId, x, y, z);
XPlane.scenery.getMagneticVariation(lat, lon);
```

## Object Loading

### loadObject

Load an OBJ file from the X-System folder.

```typescript
const objPath = XPlane.scenery.loadObject(path: string): string | null;
```

**Parameters:**
- `path` - Path to the .obj file relative to X-System folder

**Returns:** The object path as handle, or `null` if loading failed

**Example:**
```typescript
// Load a pushback tug
const tug = XPlane.scenery.loadObject(
    "Resources/default scenery/sim objects/apt_vehicles/pushback/Goldhofer_AST1_Tow.obj"
);

if (tug) {
    console.log("Loaded pushback tug");
}
```

### unloadObject

Unload a previously loaded object.

```typescript
const success = XPlane.scenery.unloadObject(path: string): boolean;
```

**Parameters:**
- `path` - The object path/handle returned from `loadObject`

**Returns:** `true` if successful

**Example:**
```typescript
// Clean up when done
XPlane.scenery.unloadObject(tug);
```

---

## Terrain Probing

Terrain probing allows you to query the height and properties of terrain at any location.

### createProbe

Create a terrain probe.

```typescript
const probeId = XPlane.scenery.createProbe(probeType?: number): number | null;
```

**Parameters:**
- `probeType` - (Optional) Type of probe. Default is `0` (Y probe - finds terrain height)

**Returns:** Probe handle ID, or `null` if failed

**Example:**
```typescript
const probeId = XPlane.scenery.createProbe();
```

### destroyProbe

Destroy a terrain probe when no longer needed.

```typescript
const success = XPlane.scenery.destroyProbe(probeId: number): boolean;
```

**Parameters:**
- `probeId` - The probe handle ID

**Returns:** `true` if successful

### probeTerrain

Probe terrain at an XYZ location in local OpenGL coordinates.

```typescript
const result = XPlane.scenery.probeTerrain(
    probeId: number, 
    x: number, 
    y: number, 
    z: number
): TerrainProbeResult;
```

**Parameters:**
- `probeId` - The probe handle ID
- `x` - X coordinate (local OpenGL)
- `y` - Y coordinate (local OpenGL)
- `z` - Z coordinate (local OpenGL)

**Returns:** A `TerrainProbeResult` object:

```typescript
interface TerrainProbeResult {
    hit: boolean;           // Whether the probe hit terrain
    result?: number;        // Probe result code (only if hit is false)
    x?: number;             // X coordinate of hit
    y?: number;             // Y coordinate of hit (terrain height)
    z?: number;             // Z coordinate of hit
    normalX?: number;       // X component of terrain normal
    normalY?: number;       // Y component of terrain normal
    normalZ?: number;       // Z component of terrain normal
    velocityX?: number;     // X velocity (for moving platforms)
    velocityY?: number;     // Y velocity
    velocityZ?: number;     // Z velocity
    isWet?: boolean;        // Whether the terrain is water
}
```

**Example:**
```typescript
// Get aircraft position
const x = XPlane.dataref.getDouble("sim/flightmodel/position/local_x");
const y = XPlane.dataref.getDouble("sim/flightmodel/position/local_y");
const z = XPlane.dataref.getDouble("sim/flightmodel/position/local_z");

// Probe terrain below aircraft
const result = XPlane.scenery.probeTerrain(probeId, x, y, z);

if (result.hit) {
    const terrainHeight = result.y;
    const agl = y - terrainHeight;
    console.log(`Height AGL: ${agl.toFixed(1)} meters`);
    
    if (result.isWet) {
        console.log("Over water");
    }
}
```

---

## Magnetic Variation

### getMagneticVariation

Get magnetic variation at a geographic location.

```typescript
const variation = XPlane.scenery.getMagneticVariation(
    latitude: number, 
    longitude: number
): number;
```

**Parameters:**
- `latitude` - Latitude in degrees
- `longitude` - Longitude in degrees

**Returns:** Magnetic variation in degrees (positive = east)

**Example:**
```typescript
// Get magnetic variation at Seattle
const variation = XPlane.scenery.getMagneticVariation(47.4502, -122.3088);
console.log(`Magnetic variation: ${variation.toFixed(1)}°`);
```

### degTrueToMagnetic

Convert true heading to magnetic heading at current aircraft location.

```typescript
const magHeading = XPlane.scenery.degTrueToMagnetic(headingTrue: number): number;
```

**Parameters:**
- `headingTrue` - True heading in degrees

**Returns:** Magnetic heading in degrees

**Example:**
```typescript
// Convert runway true heading to magnetic
const trueHeading = 340;
const magHeading = XPlane.scenery.degTrueToMagnetic(trueHeading);
console.log(`Runway ${Math.round(magHeading / 10)}`);
```

### degMagneticToTrue

Convert magnetic heading to true heading at current aircraft location.

```typescript
const trueHeading = XPlane.scenery.degMagneticToTrue(headingMagnetic: number): number;
```

**Parameters:**
- `headingMagnetic` - Magnetic heading in degrees

**Returns:** True heading in degrees

---

## Complete Example

```typescript
// Terrain-aware object placement
async function placeObjectOnTerrain(objPath: string, lat: number, lon: number, heading: number) {
    // Load the object
    const obj = XPlane.scenery.loadObject(objPath);
    if (!obj) {
        console.error("Failed to load object");
        return null;
    }
    
    // Convert world coords to local
    const local = XPlane.graphics.worldToLocal(lat, lon, 0);
    
    // Create a probe and find terrain height
    const probeId = XPlane.scenery.createProbe();
    const terrain = XPlane.scenery.probeTerrain(probeId, local.x, local.y, local.z);
    XPlane.scenery.destroyProbe(probeId);
    
    if (!terrain.hit) {
        console.error("Could not probe terrain");
        return null;
    }
    
    // Create instance at terrain level
    const instanceId = XPlane.instance.create(obj);
    if (!instanceId) {
        console.error("Failed to create instance");
        return null;
    }
    
    // Position on terrain with correct heading
    XPlane.instance.setPosition(instanceId, {
        x: terrain.x!,
        y: terrain.y!,
        z: terrain.z!,
        heading: heading,
        pitch: 0,
        roll: 0
    });
    
    return { obj, instanceId };
}

// Usage
const vehicle = await placeObjectOnTerrain(
    "Resources/default scenery/sim objects/apt_vehicles/pushback/Goldhofer_AST1_Tow.obj",
    47.4502,
    -122.3088,
    90
);

// Clean up later
if (vehicle) {
    XPlane.instance.destroy(vehicle.instanceId);
    XPlane.scenery.unloadObject(vehicle.obj);
}
```

## See Also

- [Instance API](InstanceAPI.md) - Creating and managing object instances
- [Graphics API](GraphicsAPI.md) - Coordinate conversions
- [DataRef API](DataRefAPI.md) - Reading simulator data
