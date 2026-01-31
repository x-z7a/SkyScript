# SkyScript X-Plane DataRef API

The DataRef API provides JavaScript access to X-Plane's data system, allowing you to read and write thousands of simulation values including cockpit instruments, aircraft position, weather, and more.

## Quick Start

```typescript
// Read current altitude
const altitude = XPlane.dataref.getFloat("sim/flightmodel/position/elevation");
console.log(`Altitude: ${altitude} meters`);

// Set autopilot altitude
XPlane.dataref.setFloat("sim/cockpit/autopilot/altitude", 35000);

// Read NAV1 frequency
const nav1 = XPlane.dataref.getInt("sim/cockpit/radios/nav1_freq_hz");
console.log(`NAV1: ${nav1 / 100} MHz`);
```

## API Reference

### Namespace: `XPlane.dataref`

All dataref functions are accessed through the `XPlane.dataref` namespace.

---

## Lookup Functions

### `find(name: string): boolean | null`

Check if a dataref exists.

**Parameters:**
- `name` - The full path of the dataref (e.g., `"sim/cockpit/radios/nav1_freq_hz"`)

**Returns:** `true` if the dataref exists, `null` otherwise

**Example:**
```typescript
if (XPlane.dataref.find("sim/cockpit/radios/nav1_freq_hz")) {
    console.log("NAV1 dataref exists");
}
```

---

### `canWrite(name: string): boolean`

Check if a dataref is writable.

> **Note:** Even writable datarefs may be overwritten by X-Plane each frame. Some datarefs require setting an "override" dataref to prevent this.

**Parameters:**
- `name` - The full path of the dataref

**Returns:** `true` if writable, `false` if read-only or not found

**Example:**
```typescript
if (XPlane.dataref.canWrite("sim/cockpit/autopilot/altitude")) {
    XPlane.dataref.setFloat("sim/cockpit/autopilot/altitude", 10000);
}
```

---

### `getTypes(name: string): DataRefTypes | null`

Get the supported data types for a dataref.

A dataref can support multiple types. Choose the most appropriate accessor method based on the types returned.

**Parameters:**
- `name` - The full path of the dataref

**Returns:** Object with boolean flags for each supported type:
```typescript
interface DataRefTypes {
    int: boolean;        // Supports getInt/setInt
    float: boolean;      // Supports getFloat/setFloat
    double: boolean;     // Supports getDouble/setDouble
    intArray: boolean;   // Supports getIntArray/setIntArray
    floatArray: boolean; // Supports getFloatArray/setFloatArray
    data: boolean;       // Supports getData/setData
}
```

**Example:**
```typescript
const types = XPlane.dataref.getTypes("sim/flightmodel/position/elevation");
if (types?.float) {
    const elevation = XPlane.dataref.getFloat("sim/flightmodel/position/elevation");
}
```

---

## Scalar Getters

### `getInt(name: string): number`

Read an integer value from a dataref.

**Parameters:**
- `name` - The full path of the dataref

**Returns:** The integer value, or `0` if the dataref is not found

**Example:**
```typescript
// Get COM1 frequency in Hz
const com1 = XPlane.dataref.getInt("sim/cockpit/radios/com1_freq_hz");
console.log(`COM1: ${com1 / 1000} MHz`);

// Get gear handle position (0 = up, 1 = down)
const gearHandle = XPlane.dataref.getInt("sim/cockpit/switches/gear_handle_status");
```

---

### `getFloat(name: string): number`

Read a float value from a dataref.

**Parameters:**
- `name` - The full path of the dataref

**Returns:** The float value, or `0.0` if the dataref is not found

**Example:**
```typescript
// Get indicated airspeed in knots
const ias = XPlane.dataref.getFloat("sim/flightmodel/position/indicated_airspeed");

// Get magnetic heading
const heading = XPlane.dataref.getFloat("sim/flightmodel/position/mag_psi");
```

---

### `getDouble(name: string): number`

Read a double-precision value from a dataref.

Use this for high-precision values like latitude/longitude.

**Parameters:**
- `name` - The full path of the dataref

**Returns:** The double value, or `0.0` if the dataref is not found

**Example:**
```typescript
// Get precise latitude/longitude
const lat = XPlane.dataref.getDouble("sim/flightmodel/position/latitude");
const lon = XPlane.dataref.getDouble("sim/flightmodel/position/longitude");
console.log(`Position: ${lat}, ${lon}`);
```

---

## Array Getters

### `getIntArray(name: string, offset?: number, count?: number): number[] | null`

Read an integer array from a dataref.

**Parameters:**
- `name` - The full path of the dataref
- `offset` - Start index in the array (default: `0`)
- `count` - Number of elements to read (default: all remaining)

**Returns:** Array of integers, or `null` if the dataref is not found

**Example:**
```typescript
// Get all engine N1 values (up to 8 engines)
const n1Values = XPlane.dataref.getIntArray("sim/cockpit2/engine/indicators/N1_percent");
if (n1Values) {
    console.log(`Engine 1 N1: ${n1Values[0]}%`);
}
```

---

### `getFloatArray(name: string, offset?: number, count?: number): number[] | null`

Read a float array from a dataref.

**Parameters:**
- `name` - The full path of the dataref
- `offset` - Start index in the array (default: `0`)
- `count` - Number of elements to read (default: all remaining)

**Returns:** Array of floats, or `null` if the dataref is not found

**Example:**
```typescript
// Get throttle positions for all engines
const throttle = XPlane.dataref.getFloatArray("sim/cockpit2/engine/actuators/throttle_ratio");
if (throttle) {
    throttle.forEach((t, i) => console.log(`Engine ${i + 1}: ${(t * 100).toFixed(1)}%`));
}

// Get just the first two engines
const firstTwo = XPlane.dataref.getFloatArray(
    "sim/cockpit2/engine/actuators/throttle_ratio", 0, 2
);
```

---

### `getData(name: string, offset?: number, maxBytes?: number): string`

Read byte/string data from a dataref.

**Parameters:**
- `name` - The full path of the dataref
- `offset` - Start byte offset (default: `0`)
- `maxBytes` - Maximum bytes to read (default: all remaining)

**Returns:** String value, or empty string if the dataref is not found

**Example:**
```typescript
// Get the aircraft ICAO code
const icao = XPlane.dataref.getData("sim/aircraft/view/acf_ICAO");
console.log(`Aircraft: ${icao}`);

// Get the nearest airport
const airport = XPlane.dataref.getData("sim/flightmodel/position/nearest_airport_id");
```

---

## Scalar Setters

### `setInt(name: string, value: number): boolean`

Write an integer value to a dataref.

**Parameters:**
- `name` - The full path of the dataref
- `value` - The value to set

**Returns:** `true` if successful, `false` if the dataref is not found or not writable

**Example:**
```typescript
// Set COM1 frequency to 124.85 MHz (stored as 12485 Hz * 10)
XPlane.dataref.setInt("sim/cockpit/radios/com1_freq_hz", 12485);

// Lower the landing gear
XPlane.dataref.setInt("sim/cockpit/switches/gear_handle_status", 1);
```

---

### `setFloat(name: string, value: number): boolean`

Write a float value to a dataref.

**Parameters:**
- `name` - The full path of the dataref
- `value` - The value to set

**Returns:** `true` if successful, `false` if the dataref is not found or not writable

**Example:**
```typescript
// Set autopilot altitude to 35,000 feet
XPlane.dataref.setFloat("sim/cockpit/autopilot/altitude", 35000);

// Set heading bug to 270°
XPlane.dataref.setFloat("sim/cockpit/autopilot/heading_mag", 270);
```

---

### `setDouble(name: string, value: number): boolean`

Write a double-precision value to a dataref.

**Parameters:**
- `name` - The full path of the dataref
- `value` - The value to set

**Returns:** `true` if successful, `false` if the dataref is not found or not writable

**Example:**
```typescript
// Teleport aircraft to Seattle (requires position override)
XPlane.dataref.setInt("sim/operation/override/override_planepath", 1);
XPlane.dataref.setDouble("sim/flightmodel/position/latitude", 47.4647);
XPlane.dataref.setDouble("sim/flightmodel/position/longitude", -122.3144);
XPlane.dataref.setInt("sim/operation/override/override_planepath", 0);
```

---

## Array Setters

### `setIntArray(name: string, values: number[], offset?: number): boolean`

Write an integer array to a dataref.

**Parameters:**
- `name` - The full path of the dataref
- `values` - Array of integers to write
- `offset` - Start index in the dataref array (default: `0`)

**Returns:** `true` if successful, `false` if the dataref is not found or not writable

**Example:**
```typescript
// Set transponder code to 7000
XPlane.dataref.setIntArray("sim/cockpit/radios/transponder_code", [7, 0, 0, 0]);
```

---

### `setFloatArray(name: string, values: number[], offset?: number): boolean`

Write a float array to a dataref.

**Parameters:**
- `name` - The full path of the dataref
- `values` - Array of floats to write
- `offset` - Start index in the dataref array (default: `0`)

**Returns:** `true` if successful, `false` if the dataref is not found or not writable

**Example:**
```typescript
// Set all engine throttles to 50%
XPlane.dataref.setFloatArray("sim/cockpit2/engine/actuators/throttle_ratio", 
    [0.5, 0.5, 0.5, 0.5]);

// Set just engine 1 throttle to 75%
XPlane.dataref.setFloatArray("sim/cockpit2/engine/actuators/throttle_ratio", 
    [0.75], 0);
```

---

### `setData(name: string, value: string, offset?: number): boolean`

Write string/byte data to a dataref.

**Parameters:**
- `name` - The full path of the dataref
- `value` - String value to write
- `offset` - Start byte offset in the dataref (default: `0`)

**Returns:** `true` if successful, `false` if the dataref is not found or not writable

**Example:**
```typescript
// Set the aircraft tail number
XPlane.dataref.setData("sim/aircraft/view/acf_tailnum", "N12345");
```

---

## Common Datarefs

Here are some commonly used datarefs:

### Position & Attitude
| Dataref | Type | Description |
|---------|------|-------------|
| `sim/flightmodel/position/latitude` | double | Aircraft latitude |
| `sim/flightmodel/position/longitude` | double | Aircraft longitude |
| `sim/flightmodel/position/elevation` | float | Altitude MSL (meters) |
| `sim/flightmodel/position/y_agl` | float | Altitude AGL (meters) |
| `sim/flightmodel/position/indicated_airspeed` | float | IAS (knots) |
| `sim/flightmodel/position/groundspeed` | float | Ground speed (m/s) |
| `sim/flightmodel/position/mag_psi` | float | Magnetic heading |
| `sim/flightmodel/position/true_psi` | float | True heading |
| `sim/flightmodel/position/phi` | float | Roll (degrees) |
| `sim/flightmodel/position/theta` | float | Pitch (degrees) |

### Radios
| Dataref | Type | Description |
|---------|------|-------------|
| `sim/cockpit/radios/com1_freq_hz` | int | COM1 frequency (Hz × 10) |
| `sim/cockpit/radios/com2_freq_hz` | int | COM2 frequency (Hz × 10) |
| `sim/cockpit/radios/nav1_freq_hz` | int | NAV1 frequency (Hz × 100) |
| `sim/cockpit/radios/nav2_freq_hz` | int | NAV2 frequency (Hz × 100) |
| `sim/cockpit/radios/adf1_freq_hz` | int | ADF1 frequency (Hz) |
| `sim/cockpit/radios/transponder_code` | int | Transponder code |

### Autopilot
| Dataref | Type | Description |
|---------|------|-------------|
| `sim/cockpit/autopilot/autopilot_mode` | int | AP master (0=off, 1=FD, 2=on) |
| `sim/cockpit/autopilot/heading_mag` | float | Heading bug (degrees) |
| `sim/cockpit/autopilot/altitude` | float | Target altitude (feet) |
| `sim/cockpit/autopilot/vertical_velocity` | float | Target VS (fpm) |
| `sim/cockpit/autopilot/airspeed` | float | Target IAS (knots) |

### Engine
| Dataref | Type | Description |
|---------|------|-------------|
| `sim/cockpit2/engine/actuators/throttle_ratio` | float[] | Throttle 0-1 per engine |
| `sim/cockpit2/engine/indicators/N1_percent` | float[] | N1 per engine |
| `sim/cockpit2/engine/indicators/N2_percent` | float[] | N2 per engine |
| `sim/cockpit2/engine/indicators/EGT_deg_C` | float[] | EGT per engine |
| `sim/cockpit2/engine/indicators/fuel_flow_kg_sec` | float[] | Fuel flow per engine |

### Controls
| Dataref | Type | Description |
|---------|------|-------------|
| `sim/cockpit2/controls/yoke_pitch_ratio` | float | Elevator -1 to 1 |
| `sim/cockpit2/controls/yoke_roll_ratio` | float | Aileron -1 to 1 |
| `sim/cockpit2/controls/yoke_heading_ratio` | float | Rudder -1 to 1 |
| `sim/cockpit/switches/gear_handle_status` | int | Gear handle (0=up, 1=down) |
| `sim/cockpit2/controls/flap_ratio` | float | Flaps 0-1 |
| `sim/cockpit2/controls/speedbrake_ratio` | float | Speedbrake 0-1 |

---

## Tips & Best Practices

1. **Cache dataref lookups**: The API caches dataref handles internally, but you should still avoid calling `find()` repeatedly in tight loops.

2. **Check writability**: Always verify a dataref is writable before attempting to set it, especially for custom aircraft.

3. **Override datarefs**: Some datarefs (like position) are constantly updated by X-Plane. You may need to set an override dataref first:
   ```typescript
   XPlane.dataref.setInt("sim/operation/override/override_planepath", 1);
   // Now you can set position
   XPlane.dataref.setInt("sim/operation/override/override_planepath", 0);
   ```

4. **Units matter**: Pay attention to units - altitude may be in meters or feet, speeds in m/s or knots, frequencies in Hz with various multipliers.

5. **Array indices**: Engine arrays are 0-indexed, so engine 1 is at index 0.

---

## Finding Datarefs

X-Plane provides a complete list of datarefs in:
- `Resources/plugins/DataRefs.txt` in your X-Plane installation
- The [X-Plane SDK Documentation](https://developer.x-plane.com/datarefs/)
- Third-party tools like DataRefTool
