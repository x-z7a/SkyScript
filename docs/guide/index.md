# Introduction

SkyScript is an X-Plane plugin that lets you build plugin UIs using modern web technologies. Instead of writing C++ code with X-Plane's native widget system, you can use React, TypeScript, HTML, and CSS.

## Why SkyScript?

**For Web Developers:**
- Use familiar tools: React, TypeScript, npm, and your favorite IDE
- Build beautiful UIs with CSS - no more pixel-pushing in C++
- Hot reload during development
- Full TypeScript support with autocomplete for X-Plane APIs

**For Flight Simmers:**
- Create custom EFBs, checklists, and cockpit displays
- Build tools that integrate directly with the simulator
- Share your creations as simple web apps

## How It Works

SkyScript embeds the [Ultralight](https://ultralig.ht/) browser engine into X-Plane. This lightweight renderer displays your web app as a native X-Plane window, with JavaScript bindings that give you access to:

- **DataRefs** - Read and write simulator data (altitude, heading, switches, etc.)
- **Scenery** - Load 3D objects and probe terrain
- **Instances** - Place multiple copies of objects efficiently
- **Graphics** - Convert between coordinate systems

```
┌─────────────────────────────────────────────────────────┐
│                      X-Plane                             │
│  ┌─────────────────────────────────────────────────┐    │
│  │              Your React App                      │    │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐         │    │
│  │  │Component│  │Component│  │Component│         │    │
│  │  └────┬────┘  └────┬────┘  └────┬────┘         │    │
│  │       │            │            │               │    │
│  │       └────────────┴────────────┘               │    │
│  │                    │                            │    │
│  │              XPlane API                         │    │
│  └──────────────────────────────────────────────────┘    │
│                        │                                 │
│                        ▼                                 │
│  ┌──────────────────────────────────────────────────┐   │
│  │              SkyScript Plugin                     │   │
│  │   (Ultralight + X-Plane SDK Bindings)            │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

## Supported Platforms

- **macOS** - Apple Silicon (arm64) and Intel (x86_64)
- **Windows** - 64-bit
- **Linux** - 64-bit

## Requirements

- X-Plane 11.50+ or X-Plane 12
- Node.js 18+ (for building apps)
- Any modern code editor (VS Code recommended)

## Getting Started

Ready to build your first app? Head to the [Quick Start](/guide/quick-start) guide!
