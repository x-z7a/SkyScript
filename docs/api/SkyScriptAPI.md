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

## TypeScript Definitions

Add these type definitions to your project for full TypeScript support:

```typescript
declare global {
  interface SkyScriptAPI {
    /**
     * List all installed SkyScript apps
     * @returns Array of app names
     */
    listApps(): string[];

    /**
     * Reload an app's view (refresh HTML/JS)
     * @param name - The app name to reload
     * @returns true if successful
     */
    reloadApp(name: string): boolean;

    /**
     * Open/show an app's window
     * @param name - The app name to open
     * @returns true if successful
     */
    openAppWindow(name: string): boolean;

    /**
     * Open the inspector/dev tools for an app
     * @param name - The app name to inspect
     * @returns true if successful
     */
    openAppInspector(name: string): boolean;
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
