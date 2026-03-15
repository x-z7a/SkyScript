# Getting Started

This guide walks you through creating your first SkyScript app using the **Hello World** example as a reference. By the end you will have a working app that reads X-Plane datarefs and executes commands from JavaScript.

## Prerequisites

- [Node.js](https://nodejs.org/) 18 or later
- A text editor
- SkyScript plugin installed in X-Plane 12 (see [Installation](/guide/installation))

## Scaffold the Project

Create a new folder inside `apps/` in your SkyScript plugin directory (or in the repo if you're developing from source):

```bash
mkdir apps/my-app && cd apps/my-app
```

### Create the manifest

Every app needs a `manifest.yaml` at its root. Create `apps/my-app/manifest.yaml`:

```yaml
name: My App
hide_addressbar: true
framerate: 20
```

See the [Manifest Reference](/developer/manifest) for all available fields.

### Bootstrap with Create React App

The Hello World example uses React with TypeScript. You can use any framework — or plain HTML — but React is a good starting point:

```bash
npx create-react-app . --template typescript
```

Set the `homepage` field in `package.json` so the built assets use relative paths:

```json
{
  "homepage": "."
}
```

### Project structure

After scaffolding, your app should look like this:

```
apps/my-app/
├── manifest.yaml        # Required — app metadata
├── package.json
├── public/
│   └── index.html
├── src/
│   ├── index.tsx
│   ├── App.tsx
│   └── ...
└── tsconfig.json
```

## Add TypeScript Declarations

Create `src/react-app-env.d.ts` (or add to your existing one) so TypeScript knows about the SkyScript API:

```typescript
/// <reference types="react-scripts" />

interface SkyscriptXplm {
  getDataref(ref: string): Promise<number | string | number[]>;
  setDataref(ref: string, value: number | string, valueType?: string): Promise<void>;
  executeCommand(command: string): Promise<void>;
}

interface Window {
  skyscript: {
    xplm: SkyscriptXplm;
  };
}
```

## Write Your App

Replace `src/App.tsx` with a simple component that reads a dataref:

```tsx
import { useState } from 'react';

function App() {
  const [zuluTime, setZuluTime] = useState('--');

  async function readTime() {
    try {
      const value = await window.skyscript.xplm.getDataref(
        'sim/time/zulu_time_sec'
      );
      setZuluTime(Number(value).toFixed(1));
    } catch (err: any) {
      setZuluTime(`Error: ${err.message}`);
    }
  }

  return (
    <div style={{ padding: 40, textAlign: 'center', color: '#e0e0e0' }}>
      <h1>My App</h1>
      <p>Zulu time: {zuluTime}</p>
      <button onClick={readTime}>Read Dataref</button>
    </div>
  );
}

export default App;
```

## Build

```bash
npm run build
```

This produces a `build/` folder containing `index.html` and static assets. SkyScript serves this folder directly.

## Deploy to X-Plane

If you're developing outside the plugin directory, copy the built output:

```bash
cp -r build/ /path/to/X-Plane/Resources/plugins/SkyScript/apps/my-app/
cp manifest.yaml /path/to/X-Plane/Resources/plugins/SkyScript/apps/my-app/
```

If you're developing inside the repo's `apps/` folder, the build script handles this automatically:

```bash
# From the repo root
./build_platforms.sh mac
```

This compiles the plugin, builds all apps, and copies everything to the X-Plane plugins folder.

## Test

1. In X-Plane, go to **Plugins > SkyScript > Reload configuration**.
2. Open **Plugins > SkyScript > My App**.
3. Click **Read Dataref** — you should see the current Zulu time.

## Next Steps

- Read the full [JavaScript API Reference](/developer/api) to learn about `setDataref` and `executeCommand`.
- Check the [Manifest Reference](/developer/manifest) for options like `user_agent`, `framerate`, and `audio_muted`.
- Look at the [Hello World source](https://github.com/x-z7a/skyscript-cef/tree/main/apps/hello-world) for a more complete example with polling, custom dataref input, and command execution.
