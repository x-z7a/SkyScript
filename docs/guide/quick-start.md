# Quick Start

This guide walks you through creating your first SkyScript app using React and TypeScript. By the end, you'll have a working X-Plane plugin that displays live flight data and can place objects in the simulator.

## Prerequisites

- **Node.js 18+** - [Download](https://nodejs.org/)
- **X-Plane 11 or 12** - [Download](https://www.x-plane.com/)
- **SkyScript Plugin** - Download from [Releases](https://github.com/your-username/SkyScript/releases)

## Installation

1. Download the latest SkyScript release
2. Extract to your X-Plane plugins folder:
   ```
   X-Plane 12/Resources/plugins/SkyScript/
   ```
3. The folder structure should look like:
   ```
   SkyScript/
   ├── mac_x64/
   │   ├── SkyScript.xpl
   │   └── lib/
   ├── win_x64/
   │   ├── SkyScript.xpl
   │   └── lib/
   ├── resources/
   └── apps/
       └── hello-world/
   ```

## Create a New App

### 1. Create React App

```bash
npx create-react-app my-xplane-app --template typescript
cd my-xplane-app
```

### 2. Add TypeScript Definitions

Copy the X-Plane type definitions to your project:

```bash
mkdir -p src/types/xplane
```

Create `src/types/xplane/index.d.ts` with the SkyScript API types (or copy from the [SkyScript repository](https://github.com/your-username/SkyScript/tree/main/docs/api/types/xplane)).

Update your `tsconfig.json` to include the types:

```json
{
  "compilerOptions": {
    "typeRoots": ["./node_modules/@types", "./src/types"]
  }
}
```

### 3. Configure for X-Plane

Update `package.json` to set the homepage for relative paths:

```json
{
  "homepage": "./"
}
```

## Your First X-Plane App

Replace `src/App.tsx` with this simple flight data display:

```tsx
import React, { useState, useEffect } from 'react';
import './App.css';

function App() {
  // Flight data state
  const [altitude, setAltitude] = useState<number | null>(null);
  const [airspeed, setAirspeed] = useState<number | null>(null);
  const [heading, setHeading] = useState<number | null>(null);

  useEffect(() => {
    // Update flight data every second
    const timer = setInterval(() => {
      // Check if XPlane API is available
      if (typeof XPlane !== 'undefined') {
        try {
          // Read datarefs - elevation is in meters, convert to feet
          setAltitude(XPlane.dataref.getFloat("sim/flightmodel/position/elevation") * 3.28084);
          setAirspeed(XPlane.dataref.getFloat("sim/flightmodel/position/indicated_airspeed"));
          setHeading(XPlane.dataref.getFloat("sim/flightmodel/position/mag_psi"));
        } catch (e) {
          console.error("Error reading datarefs:", e);
        }
      }
    }, 1000);

    return () => clearInterval(timer);
  }, []);

  return (
    <div className="app">
      <h1>My X-Plane App</h1>
      
      {altitude !== null ? (
        <div className="flight-data">
          <div className="data-item">
            <span className="label">Altitude</span>
            <span className="value">{altitude.toFixed(0)} ft</span>
          </div>
          <div className="data-item">
            <span className="label">Airspeed</span>
            <span className="value">{airspeed?.toFixed(0)} kts</span>
          </div>
          <div className="data-item">
            <span className="label">Heading</span>
            <span className="value">{heading?.toFixed(0)}°</span>
          </div>
        </div>
      ) : (
        <p>Waiting for X-Plane connection...</p>
      )}
    </div>
  );
}

export default App;
```

Add some basic styling in `src/App.css`:

```css
.app {
  padding: 20px;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
  color: white;
  min-height: 100vh;
}

.flight-data {
  display: flex;
  gap: 20px;
  margin-top: 20px;
}

.data-item {
  background: rgba(255, 255, 255, 0.1);
  padding: 15px 25px;
  border-radius: 10px;
  text-align: center;
}

.label {
  display: block;
  font-size: 12px;
  text-transform: uppercase;
  opacity: 0.7;
}

.value {
  display: block;
  font-size: 24px;
  font-weight: bold;
  font-family: 'Courier New', monospace;
}
```

## Build and Deploy

### 1. Build the App

```bash
npm run build
```

This creates a `build/` folder with your compiled app.

### 2. Deploy to X-Plane

Copy the build folder to your SkyScript apps directory:

```bash
# macOS/Linux
cp -r build "/path/to/X-Plane 12/Resources/plugins/SkyScript/apps/my-xplane-app"

# Windows
xcopy /E /I build "C:\X-Plane 12\Resources\plugins\SkyScript\apps\my-xplane-app"
```

### 3. Launch X-Plane

1. Start X-Plane
2. Load any aircraft
3. Go to **Plugins → SkyScript → my-xplane-app**

Your React app will appear as a floating window in X-Plane, displaying live flight data!

## Working with the X-Plane API

### Reading DataRefs

DataRefs are X-Plane's way of exposing simulator data. Read them with:

```typescript
// Integer values
const gear = XPlane.dataref.getInt("sim/cockpit/switches/gear_handle_status");

// Float values
const altitude = XPlane.dataref.getFloat("sim/flightmodel/position/elevation");

// Double values (high precision)
const latitude = XPlane.dataref.getDouble("sim/flightmodel/position/latitude");

// Arrays
const engines = XPlane.dataref.getFloatArray("sim/flightmodel/engine/ENGN_thro", 0, 4);
```

### Writing DataRefs

Some DataRefs are writable:

```typescript
// Check if writable
if (XPlane.dataref.canWrite("sim/cockpit/autopilot/heading_mag")) {
  // Set autopilot heading
  XPlane.dataref.setFloat("sim/cockpit/autopilot/heading_mag", 270);
}
```

### Placing Objects

You can load and place 3D objects in the simulator:

```typescript
// Load an object
const obj = XPlane.scenery.loadObject("path/to/object.obj");

// Create an instance
const instanceId = XPlane.instance.create(obj);

// Position it (local OpenGL coordinates)
XPlane.instance.setPosition(instanceId, {
  x: 1000,
  y: 50,
  z: -500,
  heading: 90,
  pitch: 0,
  roll: 0
});

// Clean up when done
XPlane.instance.destroy(instanceId);
XPlane.scenery.unloadObject(obj);
```

### Coordinate Conversion

Convert between world coordinates (lat/lon) and local OpenGL coordinates:

```typescript
// World to local
const local = XPlane.graphics.worldToLocal(47.45, -122.31, 100);
console.log(`Local: ${local.x}, ${local.y}, ${local.z}`);

// Local to world
const world = XPlane.graphics.localToWorld(local.x, local.y, local.z);
console.log(`World: ${world.latitude}, ${world.longitude}, ${world.altitude}`);
```

## Development Workflow

### Local Development

During development, run your app in the browser:

```bash
npm start
```

The X-Plane API won't be available, but you can add mock data:

```typescript
// Check if running in X-Plane
if (typeof XPlane !== 'undefined') {
  // Real X-Plane data
  setAltitude(XPlane.dataref.getFloat("sim/flightmodel/position/elevation"));
} else {
  // Mock data for browser testing
  setAltitude(35000);
}
```

## Next Steps

- Explore the [API Reference](/api/) for all available functions
- Check out the [hello-world sample](https://github.com/your-username/SkyScript/tree/main/samples/hello-world) for a complete example
- Learn about [terrain probing](/api/SceneryAPI#terrain-probing) to place objects at ground level
- Use [coordinate conversion](/api/GraphicsAPI) to work with GPS coordinates

## Troubleshooting

### "XPlane is not defined"

This means your app is running before the X-Plane API is injected. Always check:

```typescript
if (typeof XPlane !== 'undefined') {
  // Safe to use XPlane API
}
```

### DataRef returns 0 or null

- Check the DataRef path is correct (use [DataRefTool](https://developer.x-plane.com/tools/datarefeditor/) to browse available DataRefs)
- Some DataRefs only have values in certain conditions (e.g., engine running)

### App not appearing in X-Plane

- Ensure the app folder is in `plugins/SkyScript/apps/`
- Check that `index.html` is in the app folder root
- Look at X-Plane's `Log.txt` for error messages
