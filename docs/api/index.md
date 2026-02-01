# API Reference

The SkyScript API provides JavaScript/TypeScript access to X-Plane SDK functionality. All APIs are accessed through the global `XPlane` object.

## API Overview

| Namespace | Description |
|-----------|-------------|
| [`XPlane.dataref`](./DataRefAPI) | Read and write simulator data |
| [`XPlane.scenery`](./SceneryAPI) | Load objects, probe terrain, magnetic variation |
| [`XPlane.instance`](./InstanceAPI) | Create and manage object instances |
| [`XPlane.graphics`](./GraphicsAPI) | Coordinate system conversions |

## Quick Examples

### Reading Flight Data

```typescript
// Current altitude in feet
const altitudeFt = XPlane.dataref.getFloat("sim/flightmodel/position/elevation") * 3.28084;

// Aircraft heading
const heading = XPlane.dataref.getFloat("sim/flightmodel/position/mag_psi");

// GPS coordinates
const lat = XPlane.dataref.getDouble("sim/flightmodel/position/latitude");
const lon = XPlane.dataref.getDouble("sim/flightmodel/position/longitude");
```

### Writing Data

```typescript
// Set autopilot altitude
if (XPlane.dataref.canWrite("sim/cockpit/autopilot/altitude")) {
  XPlane.dataref.setFloat("sim/cockpit/autopilot/altitude", 35000);
}

// Toggle landing lights
XPlane.dataref.setInt("sim/cockpit/electrical/landing_lights_on", 1);
```

### Placing Objects

```typescript
// Load and place an object
const obj = XPlane.scenery.loadObject("path/to/object.obj");
const instance = XPlane.instance.create(obj);

// Get ground position
const probe = XPlane.scenery.createProbe();
const terrain = XPlane.scenery.probeTerrain(probe, x, y, z);

// Position on ground
XPlane.instance.setPosition(instance, {
  x: terrain.x,
  y: terrain.y,
  z: terrain.z,
  heading: 90
});

// Cleanup
XPlane.scenery.destroyProbe(probe);
```

### Coordinate Conversion

```typescript
// Convert GPS to local coordinates
const local = XPlane.graphics.worldToLocal(47.45, -122.31, 100);

// Convert local back to GPS
const world = XPlane.graphics.localToWorld(local.x, local.y, local.z);
```

## TypeScript Support

SkyScript provides full TypeScript definitions. Add them to your project:

```json
// tsconfig.json
{
  "compilerOptions": {
    "typeRoots": ["./node_modules/@types", "./src/types"]
  }
}
```

Then copy the type definitions from the [SkyScript repository](https://github.com/your-username/SkyScript/tree/main/docs/api/types/xplane).

## Checking API Availability

Always check if the API is available before using it:

```typescript
if (typeof XPlane !== 'undefined') {
  // Safe to use XPlane API
  const altitude = XPlane.dataref.getFloat("sim/flightmodel/position/elevation");
} else {
  // Running outside X-Plane (e.g., in browser during development)
  console.log("XPlane API not available");
}
```

## Error Handling

Most API functions return `null` or `false` on failure:

```typescript
const obj = XPlane.scenery.loadObject("invalid/path.obj");
if (!obj) {
  console.error("Failed to load object");
  return;
}

const success = XPlane.dataref.setFloat("sim/readonly/dataref", 100);
if (!success) {
  console.error("Failed to write dataref (may be read-only)");
}
```

## Finding DataRefs

X-Plane has thousands of DataRefs. Use these resources to find the ones you need:

- [DataRefTool](https://developer.x-plane.com/tools/datarefeditor/) - In-sim DataRef browser
- [X-Plane DataRefs](https://developer.x-plane.com/datarefs/) - Official documentation
- [SimInnovations DataRef Search](https://www.siminnovations.com/xplane/dataref/index.php) - Searchable database
