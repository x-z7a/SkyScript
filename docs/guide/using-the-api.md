# Using the X-Plane API

SkyScript injects global objects — `XPlane`, `SkyScript`, and `Hid` — into every app's JavaScript context. This page covers common usage patterns. For the full reference, see the [API docs](../api/).

## Reading & Writing DataRefs

DataRefs are X-Plane's key-value system for simulator state.

```typescript
// Read values
const alt = XPlane.dataref.getFloat("sim/flightmodel/position/elevation");
const hdg = XPlane.dataref.getFloat("sim/flightmodel/position/psi");
const gs  = XPlane.dataref.getFloat("sim/flightmodel/position/groundspeed");

// Write values
XPlane.dataref.setFloat("sim/flightmodel/engine/ENGN_thro", 0.85);
XPlane.dataref.setInt("sim/cockpit/electrical/landing_lights_on", 1);
```

Use `getFloat`, `getInt`, `getFloatArray`, etc. depending on the dataref type. The full list of X-Plane datarefs is available in the [X-Plane DataRef search tool](https://developer.x-plane.com/datarefs/).

### Polling for Updates

SkyScript doesn't push dataref changes — your app should poll on an interval:

```typescript
useEffect(() => {
  const timer = setInterval(() => {
    if (typeof XPlane !== 'undefined') {
      setAltitude(XPlane.dataref.getFloat("sim/flightmodel/position/elevation"));
      setHeading(XPlane.dataref.getFloat("sim/flightmodel/position/psi"));
    }
  }, 100); // 10 Hz
  return () => clearInterval(timer);
}, []);
```

Choose an interval that balances responsiveness with performance. For cockpit gauges, 100 ms (10 Hz) is usually sufficient. For things like map position updates, 500 ms–1 s is fine.

## Placing Scenery Objects

You can place `.obj` files into the X-Plane world:

```typescript
// Load the object (returns an object ref)
const objRef = XPlane.scenery.loadObject("Resources/default scenery/sim objects/apt_vehicles/pushback/pushback.obj");

// Create an instance at a specific world position
const lat = 47.4505;
const lon = -122.3085;
const alt = 130.0;

const instance = XPlane.instance.createInstance(objRef, lat, lon, alt, 0, 45, 0);
```

The rotation parameters are **pitch**, **heading**, and **roll** in degrees.

### Coordinate Conversion

X-Plane uses two coordinate systems. **Local coordinates** (OpenGL, in metres) are for rendering. **World coordinates** (lat/lon/alt) are geographic.

```typescript
// World → Local (for positioning relative to the scenery)
const local = XPlane.graphics.worldToLocal(lat, lon, alt);
// local.x, local.y, local.z

// Local → World
const world = XPlane.graphics.localToWorld(local.x, local.y, local.z);
// world.lat, world.lon, world.alt
```

## HID (USB Devices)

The `Hid` API lets you communicate with USB HID devices — flight controllers, button boxes, custom hardware panels.

```typescript
// Enumerate all HID devices
const devices = Hid.enumerate(0, 0); // vendorId=0, productId=0 → all devices

// Open a specific device
const handle = Hid.open(0x1234, 0x5678, null);

// Read data
const data = Hid.read(handle, 64);

// Clean up
Hid.close(handle);
```

See the [HID API reference](../api/HidAPI) for the complete list of functions.

## Graphics API

Screen-space drawing helpers:

```typescript
// Get the current screen size
const screen = XPlane.graphics.getScreenSize();
// screen.width, screen.height

// Get the current world rendering bounds
const bounds = XPlane.graphics.getScreenBoundsGlobal();
```

## Guarding API Calls

When developing in a browser (`npm start`), the global objects won't exist. Always guard API access:

```typescript
function getSimAltitude(): number {
  if (typeof XPlane !== 'undefined') {
    return XPlane.dataref.getFloat("sim/flightmodel/position/elevation");
  }
  return 35000; // mock value for browser preview
}
```

This lets you iterate on layout and styling in the browser, then test real data inside X-Plane.
