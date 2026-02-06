# X-Plane Utilities API

The Utilities API exposes X-Plane command management and other miscellaneous utilities from `XPLMUtilities.h`.

## Overview

The `XPlane.utilities` namespace provides programmatic access to commands (look up, create, execute) and the ability to register JavaScript handlers for simulator commands.

## Functions

### findCommand

Lookup a command by its persistent name. Returns a numeric command reference (opaque) or `0`/`null` if not found.

```typescript
const cmdRef = XPlane.utilities.findCommand("sim/commands/flight_controls/parking_brake");
```

### createCommand

Create a new command with a name and description. Returns a numeric command reference.

```typescript
const cmdRef = XPlane.utilities.createCommand("myplugin/toggle_light", "Toggle my plugin light");
```

### commandBegin / commandEnd / commandOnce

Control execution of a command by reference. `commandBegin` must be balanced with `commandEnd`.

```typescript
XPlane.utilities.commandBegin(cmdRef);
// ...
XPlane.utilities.commandEnd(cmdRef);

// Execute once
XPlane.utilities.commandOnce(cmdRef);
```

### registerCommandHandler

Register a JavaScript callback to be invoked when a command executes. The callback receives
three arguments: `(commandRef: number, phase: number, before: boolean)` and should return `1` to allow
further processing or `0` to suppress X-Plane handling (when registered `before`).

```typescript
function handler(cmdRef: number, phase: number, before: boolean): number {
  console.log('Command', cmdRef, 'phase', phase, 'before', before);
  return 1; // let X-Plane continue
}

XPlane.utilities.registerCommandHandler(cmdRef, handler, true);
```

Passing `before = true` registers the handler to run before X-Plane; returning `0` will prevent the sim from handling the command.


## Notes

- Command references are opaque numeric handles in the JavaScript environment; treat them as tokens returned from `findCommand`/`createCommand`.
- Registered callbacks are kept alive for the life of the plugin; remember to unregister if necessary (future enhancement).
