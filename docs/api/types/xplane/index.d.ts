/**
 * SkyScript X-Plane API TypeScript Definitions
 * 
 * This file provides TypeScript type definitions for the X-Plane SDK
 * JavaScript bindings available in SkyScript applications.
 * 
 * Include this in your tsconfig.json:
 * {
 *   "compilerOptions": {
 *     "typeRoots": ["./node_modules/@types", "../../docs/api/types"]
 *   }
 * }
 * 
 * @version 1.1.0
 * @license MIT
 */

declare global {
    /**
     * The XPlane global object provides access to X-Plane SDK functionality
     */
    const XPlane: XPlaneAPI;
}

/**
 * Main X-Plane API interface
 */
interface XPlaneAPI {
    /**
     * DataRef access API for reading and writing simulator data
     */
    dataref: DataRefAPI;

    /**
     * Scenery API for loading objects, terrain probing, and magnetic variation
     */
    scenery: SceneryAPI;

    /**
     * Instance API for creating and managing object instances
     */
    instance: InstanceAPI;

    /**
     * Graphics API for coordinate conversions
     */
    graphics: GraphicsAPI;
}

/**
 * DataRef type information returned by getTypes()
 */
interface DataRefTypes {
    /** DataRef supports integer access */
    int: boolean;
    /** DataRef supports float access */
    float: boolean;
    /** DataRef supports double access */
    double: boolean;
    /** DataRef supports integer array access */
    intArray: boolean;
    /** DataRef supports float array access */
    floatArray: boolean;
    /** DataRef supports byte/string data access */
    data: boolean;
}

/**
 * DataRef API for accessing X-Plane simulator data
 * 
 * DataRefs are the primary way to read and write data in X-Plane.
 * They provide access to cockpit instruments, aircraft position,
 * weather, and thousands of other simulation values.
 * 
 * @example
 * ```typescript
 * // Read the aircraft's altitude
 * const altitude = XPlane.dataref.getFloat("sim/flightmodel/position/elevation");
 * console.log(`Current altitude: ${altitude} meters`);
 * 
 * // Read the NAV1 radio frequency
 * const nav1Freq = XPlane.dataref.getInt("sim/cockpit/radios/nav1_freq_hz");
 * console.log(`NAV1 frequency: ${nav1Freq / 100} MHz`);
 * 
 * // Set the autopilot altitude
 * XPlane.dataref.setFloat("sim/cockpit/autopilot/altitude", 35000);
 * ```
 */
interface DataRefAPI {
    // =========================================================================
    // Lookup Functions
    // =========================================================================

    /**
     * Check if a dataref exists
     * 
     * @param name - The full path of the dataref (e.g., "sim/cockpit/radios/nav1_freq_hz")
     * @returns `true` if the dataref exists, `null` otherwise
     */
    find(name: string): boolean | null;

    /**
     * Check if a dataref is writable
     * 
     * @param name - The full path of the dataref
     * @returns `true` if writable, `false` if read-only or not found
     */
    canWrite(name: string): boolean;

    /**
     * Get the supported data types for a dataref
     * 
     * @param name - The full path of the dataref
     * @returns Object with boolean flags for each supported type, or `null` if not found
     */
    getTypes(name: string): DataRefTypes | null;

    // =========================================================================
    // Scalar Getters
    // =========================================================================

    /**
     * Read an integer value from a dataref
     * 
     * @param name - The full path of the dataref
     * @returns The integer value, or 0 if the dataref is not found
     */
    getInt(name: string): number;

    /**
     * Read a float value from a dataref
     * 
     * @param name - The full path of the dataref
     * @returns The float value, or 0.0 if the dataref is not found
     */
    getFloat(name: string): number;

    /**
     * Read a double-precision value from a dataref
     * 
     * @param name - The full path of the dataref
     * @returns The double value, or 0.0 if the dataref is not found
     */
    getDouble(name: string): number;

    // =========================================================================
    // Array Getters
    // =========================================================================

    /**
     * Read an integer array from a dataref
     * 
     * @param name - The full path of the dataref
     * @param offset - Start index in the array (default: 0)
     * @param count - Number of elements to read (default: all remaining)
     * @returns Array of integers, or `null` if the dataref is not found
     */
    getIntArray(name: string, offset?: number, count?: number): number[] | null;

    /**
     * Read a float array from a dataref
     * 
     * @param name - The full path of the dataref
     * @param offset - Start index in the array (default: 0)
     * @param count - Number of elements to read (default: all remaining)
     * @returns Array of floats, or `null` if the dataref is not found
     */
    getFloatArray(name: string, offset?: number, count?: number): number[] | null;

    /**
     * Read byte/string data from a dataref
     * 
     * @param name - The full path of the dataref
     * @param offset - Start byte offset (default: 0)
     * @param maxBytes - Maximum bytes to read (default: all remaining)
     * @returns String value, or empty string if the dataref is not found
     */
    getData(name: string, offset?: number, maxBytes?: number): string;

    // =========================================================================
    // Scalar Setters
    // =========================================================================

    /**
     * Write an integer value to a dataref
     * 
     * @param name - The full path of the dataref
     * @param value - The value to set
     * @returns `true` if successful, `false` if the dataref is not found or not writable
     */
    setInt(name: string, value: number): boolean;

    /**
     * Write a float value to a dataref
     * 
     * @param name - The full path of the dataref
     * @param value - The value to set
     * @returns `true` if successful, `false` if the dataref is not found or not writable
     */
    setFloat(name: string, value: number): boolean;

    /**
     * Write a double-precision value to a dataref
     * 
     * @param name - The full path of the dataref
     * @param value - The value to set
     * @returns `true` if successful, `false` if the dataref is not found or not writable
     */
    setDouble(name: string, value: number): boolean;

    // =========================================================================
    // Array Setters
    // =========================================================================

    /**
     * Write an integer array to a dataref
     * 
     * @param name - The full path of the dataref
     * @param values - Array of integers to write
     * @param offset - Start index in the dataref array (default: 0)
     * @returns `true` if successful, `false` if the dataref is not found or not writable
     */
    setIntArray(name: string, values: number[], offset?: number): boolean;

    /**
     * Write a float array to a dataref
     * 
     * @param name - The full path of the dataref
     * @param values - Array of floats to write
     * @param offset - Start index in the dataref array (default: 0)
     * @returns `true` if successful, `false` if the dataref is not found or not writable
     */
    setFloatArray(name: string, values: number[], offset?: number): boolean;

    /**
     * Write string/byte data to a dataref
     * 
     * @param name - The full path of the dataref
     * @param value - String value to write
     * @param offset - Start byte offset in the dataref (default: 0)
     * @returns `true` if successful, `false` if the dataref is not found or not writable
     */
    setData(name: string, value: string, offset?: number): boolean;
}

// =============================================================================
// Scenery API Types
// =============================================================================

/**
 * Result from terrain probing
 */
interface TerrainProbeResult {
    /** Whether the probe hit terrain */
    hit: boolean;
    /** Probe result code (only present if hit is false) */
    result?: number;
    /** X coordinate of terrain hit (local OpenGL) */
    x?: number;
    /** Y coordinate of terrain hit (local OpenGL) */
    y?: number;
    /** Z coordinate of terrain hit (local OpenGL) */
    z?: number;
    /** X component of terrain normal */
    normalX?: number;
    /** Y component of terrain normal */
    normalY?: number;
    /** Z component of terrain normal */
    normalZ?: number;
    /** X velocity of terrain (for moving platforms) */
    velocityX?: number;
    /** Y velocity of terrain */
    velocityY?: number;
    /** Z velocity of terrain */
    velocityZ?: number;
    /** Whether the terrain is water */
    isWet?: boolean;
}

/**
 * Scenery API for loading objects, terrain probing, and magnetic variation
 * 
 * @example
 * ```typescript
 * // Load an object
 * const objPath = XPlane.scenery.loadObject("Resources/default scenery/sim objects/apt_vehicles/pushback/Goldhofer_AST1_Tow.obj");
 * 
 * // Probe terrain at a location
 * const probeId = XPlane.scenery.createProbe();
 * const result = XPlane.scenery.probeTerrain(probeId, x, y, z);
 * if (result.hit) {
 *     console.log(`Terrain height: ${result.y}`);
 * }
 * 
 * // Get magnetic variation
 * const variation = XPlane.scenery.getMagneticVariation(47.4, -122.3);
 * ```
 */
interface SceneryAPI {
    // =========================================================================
    // Object Loading
    // =========================================================================

    /**
     * Load an OBJ file from the X-System folder
     * 
     * @param path - Path to the .obj file relative to X-System folder
     * @returns The object path as handle, or `null` if failed
     */
    loadObject(path: string): string | null;

    /**
     * Unload a previously loaded object
     * 
     * @param path - The object path/handle returned from loadObject
     * @returns `true` if successful
     */
    unloadObject(path: string): boolean;

    // =========================================================================
    // Terrain Probing
    // =========================================================================

    /**
     * Create a terrain probe
     * 
     * @param probeType - Type of probe (default: 0 = Y probe)
     * @returns Probe handle ID, or `null` if failed
     */
    createProbe(probeType?: number): number | null;

    /**
     * Destroy a terrain probe
     * 
     * @param probeId - The probe handle ID
     * @returns `true` if successful
     */
    destroyProbe(probeId: number): boolean;

    /**
     * Probe terrain at an XYZ location
     * 
     * @param probeId - The probe handle ID
     * @param x - X coordinate (local OpenGL)
     * @param y - Y coordinate (local OpenGL)
     * @param z - Z coordinate (local OpenGL)
     * @returns Terrain probe result with hit info and terrain properties
     */
    probeTerrain(probeId: number, x: number, y: number, z: number): TerrainProbeResult;

    // =========================================================================
    // Magnetic Variation
    // =========================================================================

    /**
     * Get magnetic variation at a geographic location
     * 
     * @param latitude - Latitude in degrees
     * @param longitude - Longitude in degrees
     * @returns Magnetic variation in degrees (positive = east)
     */
    getMagneticVariation(latitude: number, longitude: number): number;

    /**
     * Convert true heading to magnetic heading at current aircraft location
     * 
     * @param headingTrue - True heading in degrees
     * @returns Magnetic heading in degrees
     */
    degTrueToMagnetic(headingTrue: number): number;

    /**
     * Convert magnetic heading to true heading at current aircraft location
     * 
     * @param headingMagnetic - Magnetic heading in degrees
     * @returns True heading in degrees
     */
    degMagneticToTrue(headingMagnetic: number): number;
}

// =============================================================================
// Instance API Types
// =============================================================================

/**
 * Position and orientation for an instance
 */
interface InstancePosition {
    /** X coordinate (local OpenGL) */
    x: number;
    /** Y coordinate (local OpenGL) */
    y: number;
    /** Z coordinate (local OpenGL) */
    z: number;
    /** Pitch angle in degrees (optional, default: 0) */
    pitch?: number;
    /** Heading angle in degrees (optional, default: 0) */
    heading?: number;
    /** Roll angle in degrees (optional, default: 0) */
    roll?: number;
}

/**
 * Instance API for creating and managing object instances in the world
 * 
 * Instances are lightweight copies of loaded objects that can be placed
 * and animated efficiently. Use instances when you need multiple copies
 * of the same object.
 * 
 * @example
 * ```typescript
 * // Load the object first
 * const objPath = XPlane.scenery.loadObject("path/to/object.obj");
 * 
 * // Create an instance with animation datarefs
 * const instanceId = XPlane.instance.create(objPath, [
 *     "my_plugin/anim/rotation",
 *     "my_plugin/anim/brightness"
 * ]);
 * 
 * // Position the instance
 * XPlane.instance.setPosition(instanceId, {
 *     x: 100, y: 50, z: -200,
 *     heading: 90, pitch: 0, roll: 0
 * }, [45.0, 0.8]); // Animation dataref values
 * 
 * // Clean up when done
 * XPlane.instance.destroy(instanceId);
 * XPlane.scenery.unloadObject(objPath);
 * ```
 */
interface InstanceAPI {
    /**
     * Create an instance of a loaded object
     * 
     * @param objectPath - The object path/handle from loadObject
     * @param datarefs - Array of dataref names for animation (optional)
     * @returns Instance handle ID, or `null` if failed
     */
    create(objectPath: string, datarefs?: string[]): number | null;

    /**
     * Destroy an instance
     * 
     * @param instanceId - The instance handle ID
     * @returns `true` if successful
     */
    destroy(instanceId: number): boolean;

    /**
     * Set instance position and animation values
     * 
     * @param instanceId - The instance handle ID
     * @param position - Position and orientation
     * @param data - Array of float values for animation datarefs (optional)
     * @returns `true` if successful
     */
    setPosition(instanceId: number, position: InstancePosition, data?: number[]): boolean;
}

// =============================================================================
// Graphics API Types
// =============================================================================

/**
 * World coordinates (latitude/longitude/altitude)
 */
interface WorldCoordinates {
    /** Latitude in degrees */
    latitude: number;
    /** Longitude in degrees */
    longitude: number;
    /** Altitude in meters MSL */
    altitude: number;
}

/**
 * Local OpenGL coordinates
 */
interface LocalCoordinates {
    /** X coordinate (local OpenGL, meters) */
    x: number;
    /** Y coordinate (local OpenGL, meters - altitude) */
    y: number;
    /** Z coordinate (local OpenGL, meters) */
    z: number;
}

/**
 * Graphics API for coordinate conversions
 * 
 * X-Plane uses two coordinate systems:
 * - World: Latitude/Longitude/Altitude
 * - Local: OpenGL coordinates centered on the aircraft with Y up
 * 
 * @example
 * ```typescript
 * // Convert world to local coordinates
 * const local = XPlane.graphics.worldToLocal(47.4502, -122.3088, 100);
 * console.log(`Local coords: ${local.x}, ${local.y}, ${local.z}`);
 * 
 * // Convert local back to world
 * const world = XPlane.graphics.localToWorld(local.x, local.y, local.z);
 * console.log(`Lat: ${world.latitude}, Lon: ${world.longitude}`);
 * ```
 */
interface GraphicsAPI {
    /**
     * Convert local OpenGL coordinates to world coordinates
     * 
     * @param x - Local X coordinate
     * @param y - Local Y coordinate
     * @param z - Local Z coordinate
     * @returns World coordinates (lat/lon/alt)
     */
    localToWorld(x: number, y: number, z: number): WorldCoordinates;

    /**
     * Convert world coordinates to local OpenGL coordinates
     * 
     * @param latitude - Latitude in degrees
     * @param longitude - Longitude in degrees
     * @param altitude - Altitude in meters MSL
     * @returns Local OpenGL coordinates
     */
    worldToLocal(latitude: number, longitude: number, altitude: number): LocalCoordinates;
}

export {};
