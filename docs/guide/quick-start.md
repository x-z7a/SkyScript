# Quick Start

Get a SkyScript app running in X-Plane in under 5 minutes.

## Prerequisites

- **Node.js 18+** — [Download](https://nodejs.org/)
- **X-Plane 11 or 12** — [Download](https://www.x-plane.com/)
- **SkyScript Plugin** — Download from [Releases](https://github.com/x-z7a/SkyScript/releases)

## Install the Plugin

1. Download the latest SkyScript release.
2. Extract to your X-Plane plugins folder:
   ```
   X-Plane 12/Resources/plugins/SkyScript/
   ```
3. The folder structure should look like:
   ```
   SkyScript/
   ├── lib/
   ├── mac.xpl          # (or win.xpl / lin.xpl)
   ├── resources/
   ├── inspector/
   └── apps/
       └── hello-world/
   ```

## Create a New App

```bash
npx create-react-app my-xplane-app --template typescript
cd my-xplane-app
```

### Add TypeScript Definitions

Copy the X-Plane type definitions into your project:

```bash
mkdir -p src/types/xplane
```

Create `src/types/xplane/index.d.ts` with the SkyScript API types (or copy from the [SkyScript repository](https://github.com/x-z7a/SkyScript/tree/main/docs/api/types/xplane)).

Update your `tsconfig.json` to include the types:

```json
{
  "compilerOptions": {
    "typeRoots": ["./node_modules/@types", "./src/types"]
  }
}
```

### Set the Homepage

Update `package.json` so built assets use relative paths:

```json
{
  "homepage": "./"
}
```

## Build and Deploy

```bash
npm run build
```

Copy the `build/` folder into the SkyScript apps directory:

```bash
# macOS / Linux
cp -r build "/path/to/X-Plane 12/Resources/plugins/SkyScript/apps/my-xplane-app"

# Windows
xcopy /E /I build "C:\X-Plane 12\Resources\plugins\SkyScript\apps\my-xplane-app"
```

Launch X-Plane, load any aircraft, then open **Plugins → SkyScript → my-xplane-app**.

## Next Steps

- [Project Structure](./project-structure) — manifest, window sizing, build output
- [Using the X-Plane API](./using-the-api) — datarefs, scenery, instances, coordinates, HID
- [Debugging](./debugging) — WebKit Inspector, console logging, live editing
