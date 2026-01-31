/**
 * SkyScript X-Plane API TypeScript Definitions
 * 
 * This file provides TypeScript type definitions for the X-Plane SDK
 * JavaScript bindings available in SkyScript applications.
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
     * 
     * @example
     * ```typescript
     * if (XPlane.dataref.find("sim/cockpit/radios/nav1_freq_hz")) {
     *     console.log("NAV1 dataref exists");
     * }
     * ```
     */
    find(name: string): boolean | null;

    /**
     * Check if a dataref is writable
     * 
     * Note: Even writable datarefs may be overwritten by X-Plane each frame.
     * Some datarefs require setting an "override" dataref to prevent this.
     * 
     * @param name - The full path of the dataref
     * @returns `true` if writable, `false` if read-only or not found
     * 
     * @example
     * ```typescript
     * if (XPlane.dataref.canWrite("sim/cockpit/autopilot/altitude")) {
     *     XPlane.dataref.setFloat("sim/cockpit/autopilot/altitude", 10000);
     * }
     * ```
     */
    canWrite(name: string): boolean;

    /**
     * Get the supported data types for a dataref
     * 
     * A dataref can support multiple types. Choose the most appropriate
     * accessor method based on the types returned.
     * 
     * @param name - The full path of the dataref
     * @returns Object with boolean flags for each supported type, or `null` if not found
     * 
     * @example
     * ```typescript
     * const types = XPlane.dataref.getTypes("sim/flightmodel/position/elevation");
     * if (types?.float) {
     *     const elevation = XPlane.dataref.getFloat("sim/flightmodel/position/elevation");
     * }
     * ```
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
     * 
     * @example
     * ```typescript
     * // Get COM1 frequency in Hz
     * const com1 = XPlane.dataref.getInt("sim/cockpit/radios/com1_freq_hz");
     * console.log(`COM1: ${com1 / 1000} MHz`);
     * 
     * // Get gear handle position (0 = up, 1 = down)
     * const gearHandle = XPlane.dataref.getInt("sim/cockpit/switches/gear_handle_status");
     * ```
     */
    getInt(name: string): number;

    /**
     * Read a float value from a dataref
     * 
     * @param name - The full path of the dataref
     * @returns The float value, or 0.0 if the dataref is not found
     * 
     * @example
     * ```typescript
     * // Get indicated airspeed in knots
     * const ias = XPlane.dataref.getFloat("sim/flightmodel/position/indicated_airspeed");
     * 
     * // Get heading
     * const heading = XPlane.dataref.getFloat("sim/flightmodel/position/mag_psi");
     * ```
     */
    getFloat(name: string): number;

    /**
     * Read a double-precision value from a dataref
     * 
     * Use this for high-precision values like latitude/longitude.
     * 
     * @param name - The full path of the dataref
     * @returns The double value, or 0.0 if the dataref is not found
     * 
     * @example
     * ```typescript
     * // Get precise latitude/longitude
     * const lat = XPlane.dataref.getDouble("sim/flightmodel/position/latitude");
     * const lon = XPlane.dataref.getDouble("sim/flightmodel/position/longitude");
     * ```
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
     * 
     * @example
     * ```typescript
     * // Get all engine N1 values (up to 8 engines)
     * const n1Values = XPlane.dataref.getIntArray("sim/cockpit2/engine/indicators/N1_percent");
     * if (n1Values) {
     *     console.log(`Engine 1 N1: ${n1Values[0]}%`);
     * }
     * ```
     */
    getIntArray(name: string, offset?: number, count?: number): number[] | null;

    /**
     * Read a float array from a dataref
     * 
     * @param name - The full path of the dataref
     * @param offset - Start index in the array (default: 0)
     * @param count - Number of elements to read (default: all remaining)
     * @returns Array of floats, or `null` if the dataref is not found
     * 
     * @example
     * ```typescript
     * // Get throttle positions for all engines
     * const throttle = XPlane.dataref.getFloatArray("sim/cockpit2/engine/actuators/throttle_ratio");
     * if (throttle) {
     *     throttle.forEach((t, i) => console.log(`Engine ${i + 1} throttle: ${t * 100}%`));
     * }
     * 
     * // Get just the first two engines
     * const firstTwo = XPlane.dataref.getFloatArray("sim/cockpit2/engine/actuators/throttle_ratio", 0, 2);
     * ```
     */
    getFloatArray(name: string, offset?: number, count?: number): number[] | null;

    /**
     * Read byte/string data from a dataref
     * 
     * @param name - The full path of the dataref
     * @param offset - Start byte offset (default: 0)
     * @param maxBytes - Maximum bytes to read (default: all remaining)
     * @returns String value, or empty string if the dataref is not found
     * 
     * @example
     * ```typescript
     * // Get the aircraft ICAO code
     * const icao = XPlane.dataref.getData("sim/aircraft/view/acf_ICAO");
     * console.log(`Aircraft ICAO: ${icao}`);
     * 
     * // Get the current airport ID
     * const airport = XPlane.dataref.getData("sim/flightmodel/position/nearest_airport_id");
     * ```
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
     * 
     * @example
     * ```typescript
     * // Set COM1 frequency to 124.85 MHz (stored as Hz)
     * XPlane.dataref.setInt("sim/cockpit/radios/com1_freq_hz", 12485);
     * 
     * // Lower the gear
     * XPlane.dataref.setInt("sim/cockpit/switches/gear_handle_status", 1);
     * ```
     */
    setInt(name: string, value: number): boolean;

    /**
     * Write a float value to a dataref
     * 
     * @param name - The full path of the dataref
     * @param value - The value to set
     * @returns `true` if successful, `false` if the dataref is not found or not writable
     * 
     * @example
     * ```typescript
     * // Set autopilot altitude to 35,000 feet
     * XPlane.dataref.setFloat("sim/cockpit/autopilot/altitude", 35000);
     * 
     * // Set heading bug
     * XPlane.dataref.setFloat("sim/cockpit/autopilot/heading_mag", 270);
     * ```
     */
    setFloat(name: string, value: number): boolean;

    /**
     * Write a double-precision value to a dataref
     * 
     * @param name - The full path of the dataref
     * @param value - The value to set
     * @returns `true` if successful, `false` if the dataref is not found or not writable
     * 
     * @example
     * ```typescript
     * // Teleport aircraft to specific coordinates (requires override)
     * XPlane.dataref.setDouble("sim/flightmodel/position/latitude", 47.4647);
     * XPlane.dataref.setDouble("sim/flightmodel/position/longitude", -122.3144);
     * ```
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
     * 
     * @example
     * ```typescript
     * // Set transponder digits
     * XPlane.dataref.setIntArray("sim/cockpit/radios/transponder_code", [7, 0, 0, 0]);
     * ```
     */
    setIntArray(name: string, values: number[], offset?: number): boolean;

    /**
     * Write a float array to a dataref
     * 
     * @param name - The full path of the dataref
     * @param values - Array of floats to write
     * @param offset - Start index in the dataref array (default: 0)
     * @returns `true` if successful, `false` if the dataref is not found or not writable
     * 
     * @example
     * ```typescript
     * // Set all engine throttles to 50%
     * XPlane.dataref.setFloatArray("sim/cockpit2/engine/actuators/throttle_ratio", [0.5, 0.5, 0.5, 0.5]);
     * 
     * // Set just engine 1 throttle
     * XPlane.dataref.setFloatArray("sim/cockpit2/engine/actuators/throttle_ratio", [0.75], 0);
     * ```
     */
    setFloatArray(name: string, values: number[], offset?: number): boolean;

    /**
     * Write string/byte data to a dataref
     * 
     * @param name - The full path of the dataref
     * @param value - String value to write
     * @param offset - Start byte offset in the dataref (default: 0)
     * @returns `true` if successful, `false` if the dataref is not found or not writable
     * 
     * @example
     * ```typescript
     * // Set the flight number
     * XPlane.dataref.setData("sim/aircraft/view/acf_tailnum", "N12345");
     * ```
     */
    setData(name: string, value: string, offset?: number): boolean;
}

export {};
