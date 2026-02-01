# SkyScript Graphics API

The Graphics API provides coordinate conversion functions between X-Plane's local OpenGL coordinate system and world coordinates (latitude/longitude/altitude).

## Overview

X-Plane uses two coordinate systems:

- **World Coordinates**: Latitude, longitude, and altitude (meters MSL)
- **Local Coordinates**: OpenGL coordinates in meters, centered on the aircraft reference point, with Y pointing up

```typescript
// Convert between coordinate systems
const local = XPlane.graphics.worldToLocal(lat, lon, alt);
const world = XPlane.graphics.localToWorld(x, y, z);
```

## Functions

### worldToLocal

Convert world coordinates (lat/lon/alt) to local OpenGL coordinates.

```typescript
const local = XPlane.graphics.worldToLocal(
    latitude: number,
    longitude: number,
    altitude: number
): LocalCoordinates;
```

**Parameters:**
- `latitude` - Latitude in degrees
- `longitude` - Longitude in degrees
- `altitude` - Altitude in meters MSL

**Returns:**
```typescript
interface LocalCoordinates {
    x: number;  // Local OpenGL X (meters)
    y: number;  // Local OpenGL Y (meters, altitude)
    z: number;  // Local OpenGL Z (meters)
}
```

**Example:**
```typescript
// Convert Seattle Tacoma Airport coordinates to local
const seatac = XPlane.graphics.worldToLocal(47.4502, -122.3088, 130);
console.log(`SeaTac local: x=${seatac.x}, y=${seatac.y}, z=${seatac.z}`);
```

### localToWorld

Convert local OpenGL coordinates to world coordinates.

```typescript
const world = XPlane.graphics.localToWorld(
    x: number,
    y: number,
    z: number
): WorldCoordinates;
```

**Parameters:**
- `x` - Local OpenGL X coordinate
- `y` - Local OpenGL Y coordinate
- `z` - Local OpenGL Z coordinate

**Returns:**
```typescript
interface WorldCoordinates {
    latitude: number;   // Latitude in degrees
    longitude: number;  // Longitude in degrees
    altitude: number;   // Altitude in meters MSL
}
```

**Example:**
```typescript
// Get aircraft position in world coordinates
const x = XPlane.dataref.getDouble("sim/flightmodel/position/local_x");
const y = XPlane.dataref.getDouble("sim/flightmodel/position/local_y");
const z = XPlane.dataref.getDouble("sim/flightmodel/position/local_z");

const world = XPlane.graphics.localToWorld(x, y, z);
console.log(`Aircraft at: ${world.latitude.toFixed(6)}°, ${world.longitude.toFixed(6)}°`);
console.log(`Altitude: ${world.altitude.toFixed(1)} meters MSL`);
```

---

## Understanding X-Plane Coordinates

### Local OpenGL Coordinates

The local coordinate system is:
- Centered at the scenery reference point (not the aircraft)
- **X** - Positive east
- **Y** - Positive up
- **Z** - Positive south

All distances are in meters.

### World Coordinates

- **Latitude** - Degrees, positive north, negative south
- **Longitude** - Degrees, positive east, negative west
- **Altitude** - Meters above mean sea level (MSL)

---

## Complete Examples

### Display Current Position

```typescript
function displayAircraftPosition() {
    // Get local coordinates from datarefs
    const x = XPlane.dataref.getDouble("sim/flightmodel/position/local_x");
    const y = XPlane.dataref.getDouble("sim/flightmodel/position/local_y");
    const z = XPlane.dataref.getDouble("sim/flightmodel/position/local_z");
    
    // Convert to world coordinates
    const world = XPlane.graphics.localToWorld(x, y, z);
    
    // Format latitude
    const latDir = world.latitude >= 0 ? "N" : "S";
    const latDeg = Math.abs(world.latitude);
    const latMin = (latDeg % 1) * 60;
    
    // Format longitude
    const lonDir = world.longitude >= 0 ? "E" : "W";
    const lonDeg = Math.abs(world.longitude);
    const lonMin = (lonDeg % 1) * 60;
    
    console.log(`Position: ${Math.floor(latDeg)}°${latMin.toFixed(2)}'${latDir} ` +
                `${Math.floor(lonDeg)}°${lonMin.toFixed(2)}'${lonDir}`);
    console.log(`Altitude: ${(world.altitude * 3.28084).toFixed(0)} ft MSL`);
}
```

### Calculate Distance to Waypoint

```typescript
function distanceToWaypoint(targetLat: number, targetLon: number): number {
    // Get current aircraft position
    const x = XPlane.dataref.getDouble("sim/flightmodel/position/local_x");
    const y = XPlane.dataref.getDouble("sim/flightmodel/position/local_y");
    const z = XPlane.dataref.getDouble("sim/flightmodel/position/local_z");
    
    // Convert target to local coordinates
    const target = XPlane.graphics.worldToLocal(targetLat, targetLon, 0);
    
    // Calculate horizontal distance (ignore altitude)
    const dx = target.x - x;
    const dz = target.z - z;
    const distance = Math.sqrt(dx * dx + dz * dz);
    
    return distance; // meters
}

// Usage
const distanceToSeaTac = distanceToWaypoint(47.4502, -122.3088);
console.log(`Distance to SeaTac: ${(distanceToSeaTac / 1852).toFixed(1)} NM`);
```

### Place Object at GPS Coordinates

```typescript
function placeObjectAtCoordinates(
    objPath: string,
    lat: number,
    lon: number,
    altMSL: number,
    heading: number
): { obj: string, instanceId: number } | null {
    
    // Load the object
    const obj = XPlane.scenery.loadObject(objPath);
    if (!obj) {
        console.error("Failed to load object");
        return null;
    }
    
    // Convert GPS coordinates to local
    const local = XPlane.graphics.worldToLocal(lat, lon, altMSL);
    
    // Create and position instance
    const instanceId = XPlane.instance.create(obj);
    if (!instanceId) {
        console.error("Failed to create instance");
        return null;
    }
    
    XPlane.instance.setPosition(instanceId, {
        x: local.x,
        y: local.y,
        z: local.z,
        heading: heading
    });
    
    return { obj, instanceId };
}

// Place a marker at the Golden Gate Bridge
const marker = placeObjectAtCoordinates(
    "path/to/marker.obj",
    37.8199,   // Latitude
    -122.4783, // Longitude
    70,        // Altitude MSL
    0          // Heading
);
```

### Create Flight Path Visualization

```typescript
interface Waypoint {
    lat: number;
    lon: number;
    alt: number;
}

class FlightPathMarkers {
    private instances: number[] = [];
    private obj: string | null = null;
    
    constructor(markerObjPath: string) {
        this.obj = XPlane.scenery.loadObject(markerObjPath);
    }
    
    showPath(waypoints: Waypoint[]) {
        if (!this.obj) return;
        
        // Clear existing markers
        this.clearPath();
        
        // Create marker at each waypoint
        for (const wp of waypoints) {
            const local = XPlane.graphics.worldToLocal(wp.lat, wp.lon, wp.alt);
            
            const instanceId = XPlane.instance.create(this.obj);
            if (instanceId) {
                XPlane.instance.setPosition(instanceId, {
                    x: local.x,
                    y: local.y,
                    z: local.z
                });
                this.instances.push(instanceId);
            }
        }
    }
    
    clearPath() {
        for (const instanceId of this.instances) {
            XPlane.instance.destroy(instanceId);
        }
        this.instances = [];
    }
    
    destroy() {
        this.clearPath();
        if (this.obj) {
            XPlane.scenery.unloadObject(this.obj);
        }
    }
}

// Usage
const path = new FlightPathMarkers("path/to/waypoint_marker.obj");
path.showPath([
    { lat: 47.4502, lon: -122.3088, alt: 3000 },  // SeaTac
    { lat: 47.6062, lon: -122.3321, alt: 3500 },  // Downtown Seattle
    { lat: 47.9790, lon: -122.2021, alt: 4000 },  // Everett
]);
```

---

## Notes

- Coordinate conversion accuracy depends on distance from the scenery reference point
- For very long distances (hundreds of kilometers), consider the Earth's curvature
- The local coordinate system shifts when loading new scenery

## See Also

- [Scenery API](SceneryAPI.md) - Terrain probing and object loading
- [Instance API](InstanceAPI.md) - Object instancing
- [DataRef API](DataRefAPI.md) - Reading aircraft position
