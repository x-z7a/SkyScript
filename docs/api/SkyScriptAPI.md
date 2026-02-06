# SkyScript API

The SkyScript API provides app management functionality, allowing apps to interact with and control other SkyScript apps.

## Overview

The `SkyScript` global object is available in all SkyScript apps and provides functions for:

- Listing installed apps
- Opening app windows
- Reloading apps
- Opening the developer inspector

```typescript
// Check if SkyScript API is available
if (typeof SkyScript !== 'undefined') {
  const apps = SkyScript.listApps();
  console.log(`Found ${apps.length} apps`);
}
```

## Functions

### listApps()

Returns a list of all installed SkyScript apps.

```typescript
listApps(): string[]
```

**Returns:** Array of app names

**Example:**
```typescript
const apps = SkyScript.listApps();
// ['hello-world', 'my-efb', 'checklist-app']

apps.forEach(app => {
  console.log(`Found app: ${app}`);
});
```

---

### openAppWindow(name)

Opens or shows an app's window. If the app is not yet initialized, it will be initialized first.

```typescript
openAppWindow(name: string): boolean
```

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `name` | `string` | The app name to open |

**Returns:** `true` if successful, `false` if app not found

**Example:**
```typescript
// Open the hello-world app
if (SkyScript.openAppWindow('hello-world')) {
  console.log('Window opened successfully');
} else {
  console.error('Failed to open window');
}
```

---

### reloadApp(name)

Reloads an app's web view, refreshing its HTML, CSS, and JavaScript. Useful during development.

```typescript
reloadApp(name: string): boolean
```

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `name` | `string` | The app name to reload |

**Returns:** `true` if successful, `false` if app not found or not initialized

**Example:**
```typescript
// Reload after making changes
SkyScript.reloadApp('my-app');
```

---

### openAppInspector(name)

Opens the WebKit Inspector (developer tools) for the specified app. The inspector provides debugging capabilities including DOM inspection, JavaScript console, network monitoring, and performance profiling.

```typescript
openAppInspector(name: string): boolean
```

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `name` | `string` | The app name to inspect |

**Returns:** `true` if successful, `false` if app not found

**Example:**
```typescript
// Open inspector for debugging
SkyScript.openAppInspector('hello-world');
```

::: tip
The inspector is a powerful debugging tool. See the [Debugging Guide](/guide/debugging) for detailed usage instructions.
:::

## App Info

Every SkyScript app has access to its own context information through `SkyScript.app`. These properties are set automatically when the view is initialised.

| Property | Type | Description |
|----------|------|-------------|
| `SkyScript.app.name` | `string` | Internal app name (directory name) |
| `SkyScript.app.displayName` | `string` | Human-readable name from `manifest.json` |
| `SkyScript.app.dir` | `string` | Absolute path to the app's root directory |

**Example:**
```typescript
console.log(`Running: ${SkyScript.app.displayName}`);
console.log(`App directory: ${SkyScript.app.dir}`);
```

## Filesystem API

The `SkyScript.fs` namespace provides sandboxed filesystem access. All paths are resolved relative to the app's root directory. Any attempt to escape the sandbox (e.g. via `..`) is rejected.

### fs.readFile(path)

Read a file as a UTF-8 string.

```typescript
SkyScript.fs.readFile(path: string): string | null
```

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `path` | `string` | Relative path within the app directory |

**Returns:** File contents as a string, or `null` if the file doesn't exist

**Example:**
```typescript
const config = SkyScript.fs.readFile('data/config.json');
if (config) {
  const settings = JSON.parse(config);
}
```

---

### fs.writeFile(path, content)

Write a string to a file. Creates parent directories as needed.

```typescript
SkyScript.fs.writeFile(path: string, content: string): boolean
```

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `path` | `string` | Relative path within the app directory |
| `content` | `string` | The string content to write |

**Returns:** `true` if successful

**Example:**
```typescript
const state = { lastOpened: Date.now() };
SkyScript.fs.writeFile('data/state.json', JSON.stringify(state, null, 2));
```

---

### fs.exists(path)

Check whether a file or directory exists.

```typescript
SkyScript.fs.exists(path: string): boolean
```

**Parameters:**
| Name | Type | Description |
|------|------|-------------|
| `path` | `string` | Relative path within the app directory |

**Returns:** `true` if the path exists

---

## Path Utilities

The `SkyScript.path` namespace provides cross-platform path manipulation. All operations are pure string manipulation – no filesystem access occurs.

### path.join(...parts)

Join path segments using the platform-specific separator.

```typescript
SkyScript.path.join(...parts: string[]): string
```

**Example:**
```typescript
const p = SkyScript.path.join('data', 'users', 'config.json');
// macOS/Linux: "data/users/config.json"
// Windows:     "data\\users\\config.json"
```

---

### path.dirname(path)

Return the directory portion of a path.

```typescript
SkyScript.path.dirname(path: string): string
```

**Example:**
```typescript
SkyScript.path.dirname('/foo/bar/baz.txt'); // "/foo/bar"
```

---

### path.basename(path, ext?)

Return the last segment of a path, optionally stripping an extension.

```typescript
SkyScript.path.basename(path: string, ext?: string): string
```

**Example:**
```typescript
SkyScript.path.basename('/foo/bar.txt');         // "bar.txt"
SkyScript.path.basename('/foo/bar.txt', '.txt'); // "bar"
```

---

### path.extname(path)

Return the extension of a path (including the dot).

```typescript
SkyScript.path.extname(path: string): string
```

**Example:**
```typescript
SkyScript.path.extname('config.json'); // ".json"
```

---

### path.normalize(path)

Normalize a path, resolving `.` and `..` segments.

```typescript
SkyScript.path.normalize(path: string): string
```

**Example:**
```typescript
SkyScript.path.normalize('data/../config/./file.txt'); // "config/file.txt"
```

---

### path.sep

The platform-specific path separator.

```typescript
SkyScript.path.sep: string  // '/' on macOS/Linux, '\\' on Windows
```

## TypeScript Definitions

Add these type definitions to your project for full TypeScript support:

```typescript
declare global {
  interface SkyScriptAPI {
    /** List all installed SkyScript apps */
    listApps(): string[];

    /** Reload an app's view */
    reloadApp(name: string): boolean;

    /** Open/show an app's window */
    openAppWindow(name: string): boolean;

    /** Open the inspector/dev tools for an app */
    openAppInspector(name: string): boolean;

    /** Sandboxed filesystem access */
    fs: {
      readFile(path: string): string | null;
      writeFile(path: string, content: string): boolean;
      exists(path: string): boolean;
    };

    /** Cross-platform path utilities */
    path: {
      join(...parts: string[]): string;
      dirname(path: string): string;
      basename(path: string, ext?: string): string;
      extname(path: string): string;
      normalize(path: string): string;
      sep: string;
    };

    /** Per-app context information */
    app: {
      name: string;
      displayName: string;
      dir: string;
    };
  }

  const SkyScript: SkyScriptAPI | undefined;
}

export {};
```

## Use Cases

### App Launcher

Build a custom launcher that can open any installed app:

```typescript
function AppLauncher() {
  const apps = SkyScript?.listApps() ?? [];
  
  return (
    <div>
      {apps.map(app => (
        <button 
          key={app}
          onClick={() => SkyScript?.openAppWindow(app)}
        >
          Open {app}
        </button>
      ))}
    </div>
  );
}
```

### Development Helper

Create a development toolbar for quick reload and inspection:

```typescript
function DevToolbar({ appName }: { appName: string }) {
  return (
    <div className="dev-toolbar">
      <button onClick={() => SkyScript?.reloadApp(appName)}>
        🔄 Reload
      </button>
      <button onClick={() => SkyScript?.openAppInspector(appName)}>
        🔍 Inspect
      </button>
    </div>
  );
}
```

### Plugin Manager

Build a full-featured plugin manager:

```typescript
function PluginManager() {
  const [apps, setApps] = useState<string[]>([]);
  const [selected, setSelected] = useState<string | null>(null);

  useEffect(() => {
    if (SkyScript) {
      setApps(SkyScript.listApps());
    }
  }, []);

  const handleAction = (action: 'open' | 'reload' | 'inspect') => {
    if (!selected || !SkyScript) return;
    
    switch (action) {
      case 'open':
        SkyScript.openAppWindow(selected);
        break;
      case 'reload':
        SkyScript.reloadApp(selected);
        break;
      case 'inspect':
        SkyScript.openAppInspector(selected);
        break;
    }
  };

  return (
    <div>
      <select onChange={e => setSelected(e.target.value)}>
        <option value="">Select an app...</option>
        {apps.map(app => (
          <option key={app} value={app}>{app}</option>
        ))}
      </select>
      
      <button onClick={() => handleAction('open')}>Open</button>
      <button onClick={() => handleAction('reload')}>Reload</button>
      <button onClick={() => handleAction('inspect')}>Inspect</button>
    </div>
  );
}
```
