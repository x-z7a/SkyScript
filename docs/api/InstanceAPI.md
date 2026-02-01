# SkyScript Instance API

The Instance API provides access to X-Plane's object instancing system for efficiently placing multiple copies of loaded objects in the world.

## Overview

Instances are lightweight copies of loaded objects that can be independently positioned and animated. They are the recommended way to place multiple copies of the same object in the world.

```typescript
// Basic workflow
const obj = XPlane.scenery.loadObject("path/to/object.obj");
const instance = XPlane.instance.create(obj);
XPlane.instance.setPosition(instance, { x, y, z, heading: 90 });

// Clean up
XPlane.instance.destroy(instance);
XPlane.scenery.unloadObject(obj);
```

## Functions

### create

Create an instance of a loaded object.

```typescript
const instanceId = XPlane.instance.create(
    objectPath: string, 
    datarefs?: string[]
): number | null;
```

**Parameters:**
- `objectPath` - The object path/handle from `XPlane.scenery.loadObject()`
- `datarefs` - (Optional) Array of dataref names for animation control

**Returns:** Instance handle ID, or `null` if failed

**Example - Simple Instance:**
```typescript
// Load and create a basic instance
const obj = XPlane.scenery.loadObject("path/to/object.obj");
const instanceId = XPlane.instance.create(obj);
```

**Example - Animated Instance:**
```typescript
// Create instance with animation datarefs
const instanceId = XPlane.instance.create(obj, [
    "my_plugin/animation/rotation",
    "my_plugin/animation/scale"
]);
```

### destroy

Destroy an instance.

```typescript
const success = XPlane.instance.destroy(instanceId: number): boolean;
```

**Parameters:**
- `instanceId` - The instance handle ID

**Returns:** `true` if successful

**Example:**
```typescript
XPlane.instance.destroy(instanceId);
```

### setPosition

Set instance position, orientation, and animation values.

```typescript
const success = XPlane.instance.setPosition(
    instanceId: number,
    position: InstancePosition,
    data?: number[]
): boolean;
```

**Parameters:**
- `instanceId` - The instance handle ID
- `position` - Position and orientation object
- `data` - (Optional) Array of float values for animation datarefs

**Position Object:**
```typescript
interface InstancePosition {
    x: number;        // Local OpenGL X coordinate
    y: number;        // Local OpenGL Y coordinate (altitude)
    z: number;        // Local OpenGL Z coordinate
    pitch?: number;   // Pitch in degrees (default: 0)
    heading?: number; // Heading in degrees (default: 0)
    roll?: number;    // Roll in degrees (default: 0)
}
```

**Returns:** `true` if successful

**Example - Basic Positioning:**
```typescript
XPlane.instance.setPosition(instanceId, {
    x: 100,
    y: 50,
    z: -200,
    heading: 90,
    pitch: 0,
    roll: 0
});
```

**Example - With Animation:**
```typescript
// Assuming instance was created with datarefs
XPlane.instance.setPosition(instanceId, 
    { x: 100, y: 50, z: -200, heading: 90 },
    [45.0, 1.0]  // Animation values
);
```

---

## Complete Examples

### Ground Vehicle Placement

```typescript
// Place a vehicle on the ground at the current aircraft position
function placeVehicleNearby() {
    // Get aircraft position
    const acfX = XPlane.dataref.getDouble("sim/flightmodel/position/local_x");
    const acfY = XPlane.dataref.getDouble("sim/flightmodel/position/local_y");
    const acfZ = XPlane.dataref.getDouble("sim/flightmodel/position/local_z");
    
    // Load a ground service vehicle
    const vehiclePath = "Resources/default scenery/sim objects/apt_vehicles/catering/catering.obj";
    const obj = XPlane.scenery.loadObject(vehiclePath);
    
    if (!obj) {
        console.error("Failed to load vehicle");
        return null;
    }
    
    // Create instance
    const instanceId = XPlane.instance.create(obj);
    if (!instanceId) {
        console.error("Failed to create instance");
        return null;
    }
    
    // Probe terrain to get ground level
    const probeId = XPlane.scenery.createProbe();
    const terrain = XPlane.scenery.probeTerrain(probeId, acfX + 50, acfY, acfZ);
    XPlane.scenery.destroyProbe(probeId);
    
    // Position on ground, 50m ahead of aircraft
    XPlane.instance.setPosition(instanceId, {
        x: acfX + 50,
        y: terrain.hit ? terrain.y! : acfY,
        z: acfZ,
        heading: 270  // Face the aircraft
    });
    
    return { obj, instanceId };
}
```

### Animated Beacon Light

```typescript
class BeaconLight {
    private obj: string | null = null;
    private instanceId: number | null = null;
    private rotation = 0;
    
    constructor() {
        // Load beacon object (hypothetical path)
        this.obj = XPlane.scenery.loadObject("path/to/beacon.obj");
        
        if (this.obj) {
            // Create with animation dataref for rotation
            this.instanceId = XPlane.instance.create(this.obj, [
                "my_plugin/beacon/rotation"
            ]);
        }
    }
    
    update(x: number, y: number, z: number) {
        if (!this.instanceId) return;
        
        // Rotate beacon
        this.rotation = (this.rotation + 5) % 360;
        
        // Update position and animation
        XPlane.instance.setPosition(this.instanceId, 
            { x, y, z },
            [this.rotation]  // Pass rotation to animation dataref
        );
    }
    
    destroy() {
        if (this.instanceId) {
            XPlane.instance.destroy(this.instanceId);
        }
        if (this.obj) {
            XPlane.scenery.unloadObject(this.obj);
        }
    }
}
```

### Multiple Instances from One Object

```typescript
// Efficiently create many instances from a single loaded object
class ObjectManager {
    private objects = new Map<string, string>();
    private instances = new Map<number, { obj: string, position: InstancePosition }>();
    
    loadObject(path: string): string | null {
        if (this.objects.has(path)) {
            return this.objects.get(path)!;
        }
        
        const obj = XPlane.scenery.loadObject(path);
        if (obj) {
            this.objects.set(path, obj);
        }
        return obj;
    }
    
    createInstance(objPath: string, position: InstancePosition): number | null {
        const obj = this.loadObject(objPath);
        if (!obj) return null;
        
        const instanceId = XPlane.instance.create(obj);
        if (instanceId) {
            this.instances.set(instanceId, { obj, position });
            XPlane.instance.setPosition(instanceId, position);
        }
        return instanceId;
    }
    
    destroyInstance(instanceId: number) {
        if (this.instances.has(instanceId)) {
            XPlane.instance.destroy(instanceId);
            this.instances.delete(instanceId);
        }
    }
    
    destroyAll() {
        // Destroy all instances
        for (const [instanceId] of this.instances) {
            XPlane.instance.destroy(instanceId);
        }
        this.instances.clear();
        
        // Unload all objects
        for (const [, obj] of this.objects) {
            XPlane.scenery.unloadObject(obj);
        }
        this.objects.clear();
    }
}

// Usage
const manager = new ObjectManager();

const cone = "Resources/default scenery/sim objects/apt_vehicles/ground/cone_orange.obj";
for (let i = 0; i < 10; i++) {
    manager.createInstance(cone, {
        x: 100 + i * 5,
        y: 50,
        z: -200,
        heading: 0
    });
}

// Later, clean up everything
manager.destroyAll();
```

---

## Best Practices

1. **Always unload objects when done** - Objects consume memory. Unload them when no longer needed.

2. **Reuse loaded objects** - Load an object once, then create multiple instances from it.

3. **Destroy instances before unloading objects** - Destroy all instances of an object before unloading it.

4. **Use terrain probing for ground placement** - Don't assume Y=0 is ground level.

5. **Update positions in flight loop** - For moving objects, update positions regularly.

## See Also

- [Scenery API](SceneryAPI.md) - Loading objects and terrain probing
- [Graphics API](GraphicsAPI.md) - Coordinate conversions
- [DataRef API](DataRefAPI.md) - Reading simulator data
