# Debugging Apps

SkyScript includes built-in developer tools powered by the WebKit Inspector, allowing you to debug your apps directly within X-Plane.

## Using the Inspector

The Inspector provides the same developer tools you'd find in Safari, including:

- **Elements** - Inspect and modify the DOM and CSS in real-time
- **Console** - View JavaScript logs and execute commands
- **Sources** - Debug JavaScript with breakpoints
- **Network** - Monitor resource loading
- **Timeline** - Analyze performance

### Opening the Inspector

There are two ways to open the Inspector:

#### From App Manager

1. Open the App Manager: **Plugins → SkyScript → app_manager**
2. Select the app you want to debug
3. Click the **"🔍 Inspect"** button

#### From JavaScript

You can also open the inspector programmatically:

```typescript
// Open inspector for a specific app
SkyScript.openAppInspector('hello-world');

// Or open inspector for the current app
SkyScript.openAppInspector('my-app');
```

## Console Logging

All `console.log()`, `console.warn()`, and `console.error()` calls are captured and can be viewed in:

1. **The Inspector Console** - When the inspector is open
2. **X-Plane's Log.txt** - All console messages are also logged to X-Plane's log file

```typescript
// These all work and appear in the inspector
console.log("Debug info:", someVariable);
console.warn("Warning: something unexpected");
console.error("Error:", error);
```

## Debugging Tips

### Check if API is Available

When developing outside X-Plane (in a browser), the XPlane API won't be available:

```typescript
if (typeof XPlane !== 'undefined') {
  // Running in X-Plane
  const altitude = XPlane.dataref.getFloat("sim/flightmodel/position/elevation");
} else {
  // Running in browser for development
  console.log("XPlane API not available - using mock data");
}
```

### Use the SkyScript API

The `SkyScript` global provides app management functions:

```typescript
// List all installed apps
const apps = SkyScript.listApps();
console.log("Installed apps:", apps);

// Reload an app (useful during development)
SkyScript.reloadApp('my-app');

// Open another app's window
SkyScript.openAppWindow('hello-world');
```

### Live Editing with Inspector

With the Inspector open, you can:

1. **Edit CSS live** - Change styles in the Elements panel and see results immediately
2. **Execute JavaScript** - Run commands in the Console to test API calls
3. **Set breakpoints** - Pause execution and step through code
4. **Inspect variables** - Hover over variables in paused code to see values

### Reload Without Restarting X-Plane

During development, you can reload your app without restarting X-Plane:

```typescript
// From another app or the console
SkyScript.reloadApp('my-app');
```

Or from the App Manager, select your app and click **"Restart"**.

## Common Issues

### "XPlane is not defined"

This error occurs when running your app in a regular browser instead of X-Plane. Add a check:

```typescript
const isInXPlane = typeof XPlane !== 'undefined';
```

### Inspector Not Opening

If the Inspector doesn't open:

1. Make sure the `inspector/` folder exists in your SkyScript plugin directory
2. Check that `inspector/Main.html` is present
3. Look for errors in X-Plane's Log.txt

### Console Messages Not Appearing

Console messages are only visible in:
- The Inspector's Console tab (when inspector is open)
- X-Plane's Log.txt file

They won't appear in X-Plane's developer console.

## Performance Profiling

Use the Inspector's Timeline/Performance tab to identify performance issues:

1. Open Inspector for your app
2. Go to Timeline/Performance tab
3. Start recording
4. Interact with your app
5. Stop recording and analyze

Look for:
- Long-running JavaScript
- Excessive DOM manipulation
- Memory leaks
