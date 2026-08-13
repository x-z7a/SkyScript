# JavaScript API

SkyScript injects the `window.skyscript` object into every page. The `xplm` namespace provides X-Plane SDK access, `postMessage`/`onMessage` enable bidirectional plugin communication, `notification` displays native SkyScript toasts, and the `fs` namespace offers scoped file system access. All async methods return Promises and communicate with the native plugin on the simulator's main thread.

## Availability

The API is injected after each page load. It is safe to use once `DOMContentLoaded` has fired. You can check for its presence:

```js
if (window.skyscript?.xplm) {
  // API is available
}
```

## API Reference

### X-Plane SDK (`skyscript.xplm`)

#### `getDataref(ref)`

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

#### `setDataref(ref, value, valueType?)`

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

#### `executeCommand(command)`

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

### Message Passing (`skyscript.postMessage` / `skyscript.onMessage`)

SkyScript provides a bidirectional message channel between JavaScript and the native plugin (Go/C++). This enables plugins to expose custom functions and push data to the JS frontend without SkyScript needing to know the plugin's domain.

#### `postMessage(channel, payload)`

Send a message from JavaScript to the native plugin and wait for a response.

```ts
skyscript.postMessage(channel: string, payload: any): Promise<any>
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `channel` | `string` | Channel name identifying the handler (e.g. `"getProfile"`) |
| `payload` | `any` | Data to send. Will be JSON-serialized automatically. |

**Returns:** A Promise that resolves with the handler's JSON response, or rejects if the handler returns an error or no handler is registered for the channel.

**Example:**

```js
// Request data from the plugin
const profile = await skyscript.postMessage('getProfile', { name: 'default' });
console.log(profile);

// Send data to the plugin
await skyscript.postMessage('saveProfile', { name: 'custom', buttons: [1, 2, 3] });
```

---

#### `onMessage(channel, callback)`

Register a listener for messages pushed from the native plugin to JavaScript.

```ts
skyscript.onMessage(channel: string, callback: (payload: any) => void): void
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `channel` | `string` | Channel name to listen on |
| `callback` | `function` | Called with the parsed JSON payload when the plugin sends a message |

**Example:**

```js
// Listen for updates from the plugin
skyscript.onMessage('profileUpdated', (data) => {
  console.log('Profile updated:', data);
  updateUI(data);
});

skyscript.onMessage('validationResult', (result) => {
  if (result.errors.length > 0) {
    showErrors(result.errors);
  }
});
```

Multiple listeners can be registered on the same channel. They are called in registration order.

---

### Notifications (`skyscript.notification`)

SkyScript can show a native toast inside the app window. Notifications slide in from a configurable corner, use a translucent panel, and can auto-dismiss after a timeout.

#### `notification.show(options)`

```ts
skyscript.notification.show(options: {
  title?: string;
  body?: string;
  message?: string;
  corner?: 'top-left' | 'top-right' | 'bottom-left' | 'bottom-right';
  timeoutSeconds?: number;
  opacity?: number;
  slideSeconds?: number;
  dismissible?: boolean;
  playSound?: boolean;
}): Promise<void>
```

| Option | Default | Description |
|--------|---------|-------------|
| `title` | `""` | Heading text. |
| `body` / `message` | `""` | Body text. `message` is an alias. |
| `corner` | manifest/default | Corner to slide from. |
| `timeoutSeconds` | `5` | Seconds before auto-dismiss. Use `0` to stay visible until dismissed. |
| `opacity` | `0.78` | Toast background opacity, clamped between `0.15` and `0.95`. |
| `slideSeconds` | `0.25` | Slide-in and slide-out duration. |
| `dismissible` | `true` | Show a close affordance. |
| `playSound` | `true` | Play the bundled notification sound. |

```js
await skyscript.notification.show({
  title: 'Route imported',
  body: 'KSFO to KSEA is ready.',
  corner: 'bottom-right',
  timeoutSeconds: 4
});
```

There is also a convenience wrapper:

```js
await skyscript.notify('Route imported', 'KSFO to KSEA is ready.', {
  corner: 'bottom-right'
});
```

#### `notification.dismiss()`

```ts
skyscript.notification.dismiss(): Promise<void>
```

Dismiss the current notification with the configured slide-out animation.

---

### File System (`skyscript.fs`)

The `fs` namespace provides scoped file system access. All paths are restricted to the plugin directory for security — attempts to access paths outside the plugin directory will be rejected.

#### `readFile(path)`

Read the contents of a file as a string.

```ts
skyscript.fs.readFile(path: string): Promise<string>
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `path` | `string` | Absolute path to the file (must be within the plugin directory) |

**Example:**

```js
const content = await skyscript.fs.readFile('/path/to/plugin/config.yaml');
console.log(content);
```

**Errors:** Rejects if the file is not found, cannot be read, or the path is outside the plugin directory.

---

#### `writeFile(path, content)`

Write a string to a file. Creates the file and any parent directories if they don't exist. Overwrites existing files.

```ts
skyscript.fs.writeFile(path: string, content: string): Promise<void>
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `path` | `string` | Absolute path to the file (must be within the plugin directory) |
| `content` | `string` | Content to write |

**Example:**

```js
await skyscript.fs.writeFile('/path/to/plugin/config.yaml', yamlContent);
```

**Errors:** Rejects if the file cannot be written or the path is outside the plugin directory.

---

#### `listDir(path)`

List the names of entries in a directory.

```ts
skyscript.fs.listDir(path: string): Promise<string[]>
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `path` | `string` | Absolute path to the directory (must be within the plugin directory) |

**Returns:** A Promise that resolves with an array of file and directory names (not full paths).

**Example:**

```js
const entries = await skyscript.fs.listDir('/path/to/plugin/profiles');
console.log(entries); // ["default.yaml", "custom.yaml"]
```

**Errors:** Rejects if the directory is not found or the path is outside the plugin directory.

---

#### `exists(path)`

Check whether a file or directory exists.

```ts
skyscript.fs.exists(path: string): Promise<boolean>
```

**Parameters:**

| Name | Type | Description |
|------|------|-------------|
| `path` | `string` | Absolute path to check (must be within the plugin directory) |

**Returns:** A Promise that resolves with `true` if the path exists, `false` otherwise.

**Example:**

```js
if (await skyscript.fs.exists('/path/to/plugin/profiles/custom.yaml')) {
  const content = await skyscript.fs.readFile('/path/to/plugin/profiles/custom.yaml');
}
```

**Errors:** Rejects if the path is outside the plugin directory.

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

interface SkyscriptFs {
  readFile(path: string): Promise<string>;
  writeFile(path: string, content: string): Promise<void>;
  listDir(path: string): Promise<string[]>;
  exists(path: string): Promise<boolean>;
}

interface Window {
  skyscript: {
    version: string;
    xplaneVersion: string;
    xplm: SkyscriptXplm;
    postMessage(channel: string, payload: any): Promise<any>;
    onMessage(channel: string, callback: (payload: any) => void): void;
    fs: SkyscriptFs;
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
