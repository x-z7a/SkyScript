# HID API

The HID API (`XPlane.hid`) provides low-level access to USB Human Interface Devices such as joysticks, button boxes, custom controllers, LED panels, and other USB peripherals. Built on the cross-platform [HIDAPI](https://github.com/signal11/hidapi) library.

## Overview

| Method | Description |
|--------|-------------|
| `enumerate(vendorId?, productId?)` | List connected HID devices |
| `open(vendorId, productId, serial?)` | Open device by VID/PID |
| `openPath(path)` | Open device by platform path |
| `close(deviceId)` | Close an open device |
| `write(deviceId, data)` | Write an output report |
| `read(deviceId, length, timeoutMs?)` | Read an input report |
| `sendFeatureReport(deviceId, data)` | Send a feature report |
| `getFeatureReport(deviceId, reportId, length)` | Get a feature report |
| `getDeviceInfo(deviceId)` | Get device strings |
| `setNonBlocking(deviceId, nonBlocking)` | Set blocking mode |

## Device Discovery

### `enumerate(vendorId?, productId?)`

List all connected HID devices, optionally filtered by vendor/product ID.

```typescript
// List all HID devices
const allDevices = XPlane.hid.enumerate();

// Filter by vendor ID only
const vendorDevices = XPlane.hid.enumerate(0x1234);

// Filter by vendor and product ID
const specificDevices = XPlane.hid.enumerate(0x1234, 0x5678);
```

**Returns:** `HidDeviceInfo[]` — Array of device info objects:

| Property | Type | Description |
|----------|------|-------------|
| `path` | `string` | Platform-specific device path |
| `vendorId` | `number` | USB Vendor ID |
| `productId` | `number` | USB Product ID |
| `serialNumber` | `string` | Device serial number |
| `releaseNumber` | `number` | Device release number (BCD) |
| `manufacturer` | `string` | Manufacturer name |
| `product` | `string` | Product name |
| `usagePage` | `number` | HID Usage Page (Windows/Mac) |
| `usage` | `number` | HID Usage (Windows/Mac) |
| `interfaceNumber` | `number` | USB interface number |

## Opening and Closing

### `open(vendorId, productId, serialNumber?)`

Open a device by Vendor ID and Product ID.

```typescript
const deviceId = XPlane.hid.open(0x1234, 0x5678);
if (deviceId) {
  console.log('Device opened:', deviceId);
}
```

- **vendorId** — USB Vendor ID
- **productId** — USB Product ID
- **serialNumber** — (optional) To select among multiple identical devices

**Returns:** Device handle ID (`number`) or `null` on failure.

### `openPath(path)`

Open a device by its platform-specific path (from `enumerate()`).

```typescript
const devices = XPlane.hid.enumerate(0x1234, 0x5678);
if (devices.length > 0) {
  const deviceId = XPlane.hid.openPath(devices[0].path);
}
```

**Returns:** Device handle ID (`number`) or `null` on failure.

### `close(deviceId)`

Close an open device and release resources.

```typescript
XPlane.hid.close(deviceId);
```

**Returns:** `true` if successful.

## Reading and Writing

### `write(deviceId, data)`

Write an output report. The first byte must be the Report ID (use `0x00` for devices with a single report).

```typescript
// Send a report: [reportId, byte1, byte2, ...]
const bytesWritten = XPlane.hid.write(deviceId, [0x00, 0x01, 0xFF, 0x00]);
```

**Returns:** Number of bytes written, or `-1` on error.

### `read(deviceId, length, timeoutMs?)`

Read an input report from the device.

```typescript
// Non-blocking read (with timeout)
const data = XPlane.hid.read(deviceId, 64, 100); // 100ms timeout

// Blocking read (no timeout)
const data2 = XPlane.hid.read(deviceId, 64, -1);
```

- **length** — Maximum bytes to read
- **timeoutMs** — (optional) Timeout in ms. `-1` = blocking. Omit to use current mode.

**Returns:** `number[]` of bytes read, empty array if no data, or `null` on error.

## Feature Reports

### `sendFeatureReport(deviceId, data)`

Send a feature report. The first byte must be the Report ID.

```typescript
XPlane.hid.sendFeatureReport(deviceId, [0x01, 0x02, 0x03]);
```

**Returns:** Number of bytes sent, or `-1` on error.

### `getFeatureReport(deviceId, reportId, length)`

Request a feature report from the device.

```typescript
const report = XPlane.hid.getFeatureReport(deviceId, 0x01, 64);
if (report) {
  console.log('Feature report:', report);
}
```

**Returns:** `number[]` of bytes read, or `null` on error.

## Device Information

### `getDeviceInfo(deviceId)`

Get manufacturer, product, and serial number strings from an open device.

```typescript
const info = XPlane.hid.getDeviceInfo(deviceId);
if (info) {
  console.log(`${info.manufacturer} - ${info.product} (S/N: ${info.serialNumber})`);
}
```

**Returns:** `{ manufacturer, product, serialNumber }` or `null` on error.

### `setNonBlocking(deviceId, nonBlocking)`

Set read mode to blocking or non-blocking.

```typescript
// Enable non-blocking reads
XPlane.hid.setNonBlocking(deviceId, true);

// Back to blocking
XPlane.hid.setNonBlocking(deviceId, false);
```

**Returns:** `true` if successful.

## Complete Example

```typescript
// Discover and interact with a custom button box
function pollButtonBox() {
  const VID = 0x1234;
  const PID = 0x5678;
  
  const deviceId = XPlane.hid.open(VID, PID);
  if (!deviceId) {
    console.error('Button box not found');
    return;
  }
  
  // Get device info
  const info = XPlane.hid.getDeviceInfo(deviceId);
  console.log(`Connected: ${info?.product}`);
  
  // Set non-blocking so reads don't stall the sim
  XPlane.hid.setNonBlocking(deviceId, true);
  
  // Poll in a timer
  setInterval(() => {
    const data = XPlane.hid.read(deviceId, 64, 0);
    if (data && data.length > 0) {
      // data[0] = report ID, data[1..] = button states
      const buttons = data[1];
      if (buttons & 0x01) {
        XPlane.dataref.setInt("sim/cockpit/electrical/landing_lights_on", 1);
      }
    }
    
    // Send LED feedback
    XPlane.hid.write(deviceId, [0x00, 0x01, 0xFF]); // Set LED on
  }, 50);
}
```

## Notes

- HIDAPI is initialized automatically on first use.
- Always close devices when done to release USB resources.
- Use non-blocking mode when polling inside `setInterval` to avoid blocking the simulator.
- The first byte in `write()` and `sendFeatureReport()` is always the Report ID.
- On macOS and Windows, `usagePage` and `usage` from `enumerate()` help distinguish multiple HID interfaces on a composite device.
