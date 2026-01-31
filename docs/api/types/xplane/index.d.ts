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
 * @version 1.0.0
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

export {};
