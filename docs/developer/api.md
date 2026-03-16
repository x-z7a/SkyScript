# JavaScript API

SkyScript injects the `window.skyscript.xplm` object into every page. All methods return Promises and communicate with X-Plane's SDK on the simulator's main thread.

## Availability

The API is injected after each page load. It is safe to use once `DOMContentLoaded` has fired. You can check for its presence:

```js
if (window.skyscript?.xplm) {
  // API is available
}
```

## API Reference

### `getDataref(ref)`

Read the current value of an X-Plane dataref.

```ts
skyscript.xplm.getDataref(ref: string): Promise<number | string | number[]>
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `ref` | `string` | Full dataref path (e.g. `"sim/time/zulu_time_sec"`) |

**Returns:** A Promise that resolves with the dataref value. The return type depends on the dataref:

| Dataref Type | JS Type | Example |
|-------------|---------|---------|
| `int` | `number` | `1` |
| `float` | `number` | `42345.128906` |
| `double` | `number` | `42345.12890625` |
| `string` / `data` | `string` | `"KSFO"` |
| `float[]` | `number[]` | `[0.5, 1.0, 0.8]` |
| `int[]` | `number[]` | `[1, 0, 1, 1]` |

**Example:**

```js
const zulu = await skyscript.xplm.getDataref('sim/time/zulu_time_sec');
console.log(`Zulu time: ${zulu}`);

const lat = await skyscript.xplm.getDataref('sim/flightmodel/position/latitude');
const lon = await skyscript.xplm.getDataref('sim/flightmodel/position/longitude');
console.log(`Position: ${lat}, ${lon}`);
```

**Errors:** The Promise rejects if the dataref is not found or has an unsupported type.

---

### `setDataref(ref, value, valueType?)`

Write a value to an X-Plane dataref.

```ts
skyscript.xplm.setDataref(
  ref: string,
  value: number | string,
  valueType?: 'int' | 'float' | 'string'
): Promise<void>
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `ref` | `string` | Full dataref path |
| `value` | `number \| string` | The value to set |
| `valueType` | `string` | *(optional)* Force a type. If omitted, inferred from `value`: integers map to `"int"`, other numbers to `"float"`, strings to `"string"`. |

**Example:**

```js
// Set a float dataref
await skyscript.xplm.setDataref('sim/time/zulu_time_sec', 3600);

// Explicitly specify type
await skyscript.xplm.setDataref('sim/cockpit/radios/com1_freq_hz', 12180, 'int');
```

---

### `executeCommand(command)`

Execute an X-Plane command (equivalent to `XPLMCommandOnce`).

```ts
skyscript.xplm.executeCommand(command: string): Promise<void>
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `command` | `string` | Full command path (e.g. `"sim/operation/pause_toggle"`) |

**Example:**

```js
await skyscript.xplm.executeCommand('sim/operation/pause_toggle');
await skyscript.xplm.executeCommand('sim/GPS/g1000n1_hdg_sync');
```

**Errors:** Does not reject if the command is not found — X-Plane silently ignores unknown commands.

---

## Console Logging

All `console.log()`, `console.warn()`, and `console.error()` calls from your app are forwarded to X-Plane's developer console (`Log.txt`). Messages are prefixed with the app name:

```
[SkyScript] [My App] [LOG] Hello from my app (http://localhost/index.js:42)
[SkyScript] [My App] [ERROR] Something went wrong (http://localhost/index.js:55)
```

This makes it easy to debug your app without needing browser DevTools.

---

## How It Works

Understanding the communication flow helps when debugging:

1. Your JavaScript calls a method like `getDataref("sim/time/zulu_time_sec")`.
2. The method creates a Promise and stores its resolve/reject callbacks with a unique ID.
3. A hidden `<iframe>` navigates to `skyscript://xplm/getDataref?ref=sim/time/zulu_time_sec&callbackId=1`.
4. The native plugin intercepts this URL in the CEF resource handler (IO thread) and enqueues the request.
5. On X-Plane's main thread (flight loop), the plugin dequeues the request and executes the XPLM SDK call.
6. The result is sent back by calling `ExecuteJavaScript("window.skyscript._resolve(1, 42345.128906)")`.
7. Your Promise resolves with the value.

All XPLM calls happen on X-Plane's main thread, so there are no threading issues.

## TypeScript Declarations

Add these to a `.d.ts` file in your project for full type safety:

```typescript
interface SkyscriptXplm {
  getDataref(ref: string): Promise<number | string | number[]>;
  setDataref(ref: string, value: number | string, valueType?: string): Promise<void>;
  executeCommand(command: string): Promise<void>;
}

interface Window {
  skyscript: {
    version: string;
    xplaneVersion: string;
    xplm: SkyscriptXplm;
  };
}
```

## Common Datarefs

Here are some frequently used datarefs to get started:

| Dataref | Type | Description |
|---------|------|-------------|
| `sim/time/zulu_time_sec` | float | Current Zulu time in seconds |
| `sim/flightmodel/position/latitude` | float | Aircraft latitude |
| `sim/flightmodel/position/longitude` | float | Aircraft longitude |
| `sim/flightmodel/position/elevation` | float | Altitude MSL (meters) |
| `sim/flightmodel/position/y_agl` | float | Altitude AGL (meters) |
| `sim/flightmodel/position/groundspeed` | float | Ground speed (m/s) |
| `sim/flightmodel/position/indicated_airspeed` | float | IAS (knots) |
| `sim/flightmodel/position/mag_psi` | float | Magnetic heading |
| `sim/weather/wind_direction_degt` | float | Wind direction (true) |
| `sim/weather/wind_speed_kt` | float | Wind speed (knots) |

For a complete list, see the [X-Plane DataRef list](https://developer.x-plane.com/datarefs/).
