---
layout: home

hero:
  name: "SkyScript"
  text: "Build X-Plane Plugins with React"
  tagline: Create modern, interactive cockpit displays and tools using web technologies
  actions:
    - theme: brand
      text: Quick Start
      link: /guide/quick-start
    - theme: alt
      text: API Reference
      link: /api/

features:
  - icon: ⚛️
    title: React & TypeScript
    details: Build your plugin UI with React components and full TypeScript support for type-safe X-Plane API access.
  - icon: ✈️
    title: Full X-Plane SDK Access
    details: Read and write DataRefs, load scenery objects, create instances, and convert coordinates - all from JavaScript.
  - icon: 🎨
    title: Modern Web Styling
    details: Use CSS, Tailwind, or any styling framework. Ultralight renders your UI with full CSS3 support.
---

## What is SkyScript?

SkyScript is an X-Plane plugin that embeds the [Ultralight](https://ultralig.ht/) browser engine, allowing you to build plugin UIs using modern web technologies like React, TypeScript, and CSS.

Instead of learning X-Plane's native UI APIs, you can use familiar web development tools and techniques to create beautiful, interactive cockpit displays, EFBs, and utility tools.

```typescript
// Read aircraft data with TypeScript
const altitude = XPlane.dataref.getFloat("sim/flightmodel/position/elevation");
const heading = XPlane.dataref.getFloat("sim/flightmodel/position/mag_psi");

// Place objects in the world
const obj = XPlane.scenery.loadObject("path/to/object.obj");
const instance = XPlane.instance.create(obj);
XPlane.instance.setPosition(instance, { x, y, z, heading: 90 });
```

## Getting Started

Check out the [Quick Start Guide](/guide/quick-start) to create your first SkyScript app in minutes.
