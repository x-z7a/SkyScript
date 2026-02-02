#include "js_bindings.h"
#include "manager.h"

// Static member definitions
std::unordered_map<std::string, XPLMDataRef> JSBindings::dataref_cache_;
std::mutex JSBindings::cache_mutex_;

// Scenery/Instance static members
std::unordered_map<std::string, XPLMObjectRef> JSBindings::object_cache_;
std::unordered_map<int, XPLMInstanceRef> JSBindings::instance_cache_;
int JSBindings::next_instance_id_ = 1;
std::unordered_map<int, XPLMProbeRef> JSBindings::probe_cache_;
int JSBindings::next_probe_id_ = 1;

XPLMDataRef JSBindings::GetCachedDataRef(const std::string &name)
{
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto it = dataref_cache_.find(name);
    if (it != dataref_cache_.end())
    {
        return it->second;
    }

    XPLMDataRef ref = XPLMFindDataRef(name.c_str());
    if (ref)
    {
        dataref_cache_[name] = ref;
    }
    return ref;
}

void JSBindings::BindToView(RefPtr<View> view)
{
    RefPtr<JSContext> context = view->LockJSContext();
    SetJSContext(context->ctx());
    JSObject global = JSGlobalObject();

    // Create the XPlane namespace object
    JSObject xplane;

    // Create the dataref sub-namespace
    JSObject dataref;

    // Bind all DataRef functions using JSCallbackWithRetval directly (for static functions)
    dataref["find"] = JSCallbackWithRetval(JS_FindDataRef);
    dataref["canWrite"] = JSCallbackWithRetval(JS_CanWriteDataRef);
    dataref["getTypes"] = JSCallbackWithRetval(JS_GetDataRefTypes);

    // Getters
    dataref["getInt"] = JSCallbackWithRetval(JS_GetDatai);
    dataref["getFloat"] = JSCallbackWithRetval(JS_GetDataf);
    dataref["getDouble"] = JSCallbackWithRetval(JS_GetDatad);
    dataref["getIntArray"] = JSCallbackWithRetval(JS_GetDatavi);
    dataref["getFloatArray"] = JSCallbackWithRetval(JS_GetDatavf);
    dataref["getData"] = JSCallbackWithRetval(JS_GetDatab);

    // Setters
    dataref["setInt"] = JSCallbackWithRetval(JS_SetDatai);
    dataref["setFloat"] = JSCallbackWithRetval(JS_SetDataf);
    dataref["setDouble"] = JSCallbackWithRetval(JS_SetDatad);
    dataref["setIntArray"] = JSCallbackWithRetval(JS_SetDatavi);
    dataref["setFloatArray"] = JSCallbackWithRetval(JS_SetDatavf);
    dataref["setData"] = JSCallbackWithRetval(JS_SetDatab);

    // Attach dataref namespace to XPlane
    xplane["dataref"] = JSValue(static_cast<JSObjectRef>(dataref));

    // =========================================================================
    // Create the scenery sub-namespace
    // =========================================================================
    JSObject scenery;

    // Object loading
    scenery["loadObject"] = JSCallbackWithRetval(JS_LoadObject);
    scenery["unloadObject"] = JSCallbackWithRetval(JS_UnloadObject);

    // Terrain probing
    scenery["createProbe"] = JSCallbackWithRetval(JS_CreateProbe);
    scenery["destroyProbe"] = JSCallbackWithRetval(JS_DestroyProbe);
    scenery["probeTerrain"] = JSCallbackWithRetval(JS_ProbeTerrainXYZ);

    // Magnetic variation
    scenery["getMagneticVariation"] = JSCallbackWithRetval(JS_GetMagneticVariation);
    scenery["degTrueToMagnetic"] = JSCallbackWithRetval(JS_DegTrueToDegMagnetic);
    scenery["degMagneticToTrue"] = JSCallbackWithRetval(JS_DegMagneticToDegTrue);

    xplane["scenery"] = JSValue(static_cast<JSObjectRef>(scenery));

    // =========================================================================
    // Create the instance sub-namespace
    // =========================================================================
    JSObject instance;

    instance["create"] = JSCallbackWithRetval(JS_CreateInstance);
    instance["destroy"] = JSCallbackWithRetval(JS_DestroyInstance);
    instance["setPosition"] = JSCallbackWithRetval(JS_InstanceSetPosition);

    xplane["instance"] = JSValue(static_cast<JSObjectRef>(instance));

    // =========================================================================
    // Create the graphics sub-namespace
    // =========================================================================
    JSObject graphics;

    graphics["localToWorld"] = JSCallbackWithRetval(JS_LocalToWorld);
    graphics["worldToLocal"] = JSCallbackWithRetval(JS_WorldToLocal);

    xplane["graphics"] = JSValue(static_cast<JSObjectRef>(graphics));

    // Attach XPlane to global
    global["XPlane"] = JSValue(static_cast<JSObjectRef>(xplane));

    // =========================================================================
    // Create the SkyScript namespace (for app management)
    // =========================================================================
    JSObject skyscript;

    skyscript["listApps"] = JSCallbackWithRetval(JS_ListApps);
    skyscript["reloadApp"] = JSCallbackWithRetval(JS_ReloadApp);
    skyscript["openAppWindow"] = JSCallbackWithRetval(JS_OpenAppWindow);
    skyscript["openAppInspector"] = JSCallbackWithRetval(JS_OpenAppInspector);

    // Attach SkyScript to global
    global["SkyScript"] = JSValue(static_cast<JSObjectRef>(skyscript));

    // =========================================================================
    // WebCrypto Polyfill (for Navigraph and other libraries that need it)
    // Force override because native crypto throws "secure origin" errors
    // =========================================================================
    const char *cryptoPolyfill = R"(
(function() {
    // Simple PRNG based on Math.random (not cryptographically secure, but works for most use cases)
    var getRandomValues = function(array) {
        for (var i = 0; i < array.length; i++) {
            if (array instanceof Uint8Array) {
                array[i] = Math.floor(Math.random() * 256);
            } else if (array instanceof Uint16Array) {
                array[i] = Math.floor(Math.random() * 65536);
            } else if (array instanceof Uint32Array) {
                array[i] = Math.floor(Math.random() * 4294967296);
            } else {
                array[i] = Math.floor(Math.random() * 256);
            }
        }
        return array;
    };
    
    var randomUUID = function() {
        // Generate UUID v4: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
        var bytes = new Uint8Array(16);
        getRandomValues(bytes);
        bytes[6] = (bytes[6] & 0x0f) | 0x40; // Version 4
        bytes[8] = (bytes[8] & 0x3f) | 0x80; // Variant 1
        
        var hex = Array.from(bytes).map(function(b) {
            return b.toString(16).padStart(2, '0');
        }).join('');
        
        return hex.slice(0, 8) + '-' + hex.slice(8, 12) + '-' + hex.slice(12, 16) + '-' + hex.slice(16, 20) + '-' + hex.slice(20);
    };
    
    // =========================================================================
    // SHA-256 Implementation (pure JavaScript)
    // =========================================================================
    var sha256 = function(message) {
        var K = [
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        ];
        
        var H = [0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19];
        
        var rightRotate = function(value, amount) {
            return (value >>> amount) | (value << (32 - amount));
        };
        
        // Convert to bytes if string
        var bytes;
        if (typeof message === 'string') {
            bytes = new Uint8Array(new TextEncoder().encode(message));
        } else if (message instanceof ArrayBuffer) {
            bytes = new Uint8Array(message);
        } else {
            bytes = new Uint8Array(message);
        }
        
        // Pre-processing: adding padding bits
        var msgLen = bytes.length;
        var bitLen = msgLen * 8;
        var padLen = ((msgLen + 8) % 64 < 56) ? (56 - (msgLen + 8) % 64) : (120 - (msgLen + 8) % 64);
        padLen += 8;
        
        var padded = new Uint8Array(msgLen + padLen + 8);
        padded.set(bytes);
        padded[msgLen] = 0x80;
        
        // Append length in bits as 64-bit big-endian
        var lenPos = padded.length - 8;
        for (var i = 0; i < 8; i++) {
            padded[lenPos + i] = (i < 4) ? 0 : ((bitLen >>> ((7 - i) * 8)) & 0xff);
        }
        
        // Process each 64-byte chunk
        var W = new Array(64);
        for (var chunk = 0; chunk < padded.length; chunk += 64) {
            // Create message schedule
            for (var t = 0; t < 16; t++) {
                W[t] = (padded[chunk + t * 4] << 24) | (padded[chunk + t * 4 + 1] << 16) |
                       (padded[chunk + t * 4 + 2] << 8) | padded[chunk + t * 4 + 3];
            }
            for (var t = 16; t < 64; t++) {
                var s0 = rightRotate(W[t - 15], 7) ^ rightRotate(W[t - 15], 18) ^ (W[t - 15] >>> 3);
                var s1 = rightRotate(W[t - 2], 17) ^ rightRotate(W[t - 2], 19) ^ (W[t - 2] >>> 10);
                W[t] = (W[t - 16] + s0 + W[t - 7] + s1) >>> 0;
            }
            
            // Initialize working variables
            var a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
            
            // Main loop
            for (var t = 0; t < 64; t++) {
                var S1 = rightRotate(e, 6) ^ rightRotate(e, 11) ^ rightRotate(e, 25);
                var ch = (e & f) ^ (~e & g);
                var temp1 = (h + S1 + ch + K[t] + W[t]) >>> 0;
                var S0 = rightRotate(a, 2) ^ rightRotate(a, 13) ^ rightRotate(a, 22);
                var maj = (a & b) ^ (a & c) ^ (b & c);
                var temp2 = (S0 + maj) >>> 0;
                
                h = g; g = f; f = e; e = (d + temp1) >>> 0;
                d = c; c = b; b = a; a = (temp1 + temp2) >>> 0;
            }
            
            // Add compressed chunk to hash
            H[0] = (H[0] + a) >>> 0; H[1] = (H[1] + b) >>> 0;
            H[2] = (H[2] + c) >>> 0; H[3] = (H[3] + d) >>> 0;
            H[4] = (H[4] + e) >>> 0; H[5] = (H[5] + f) >>> 0;
            H[6] = (H[6] + g) >>> 0; H[7] = (H[7] + h) >>> 0;
        }
        
        // Produce final hash
        var result = new Uint8Array(32);
        for (var i = 0; i < 8; i++) {
            result[i * 4] = (H[i] >>> 24) & 0xff;
            result[i * 4 + 1] = (H[i] >>> 16) & 0xff;
            result[i * 4 + 2] = (H[i] >>> 8) & 0xff;
            result[i * 4 + 3] = H[i] & 0xff;
        }
        return result.buffer;
    };
    
    // =========================================================================
    // SHA-1 Implementation (for compatibility)
    // =========================================================================
    var sha1 = function(message) {
        var H = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0];
        
        var leftRotate = function(n, s) { return ((n << s) | (n >>> (32 - s))) >>> 0; };
        
        var bytes;
        if (typeof message === 'string') {
            bytes = new Uint8Array(new TextEncoder().encode(message));
        } else if (message instanceof ArrayBuffer) {
            bytes = new Uint8Array(message);
        } else {
            bytes = new Uint8Array(message);
        }
        
        var msgLen = bytes.length;
        var bitLen = msgLen * 8;
        var padLen = ((msgLen % 64) < 56) ? (56 - msgLen % 64) : (120 - msgLen % 64);
        
        var padded = new Uint8Array(msgLen + padLen + 8);
        padded.set(bytes);
        padded[msgLen] = 0x80;
        
        var lenPos = padded.length - 8;
        for (var i = 0; i < 8; i++) {
            padded[lenPos + i] = (i < 4) ? 0 : ((bitLen >>> ((7 - i) * 8)) & 0xff);
        }
        
        var W = new Array(80);
        for (var chunk = 0; chunk < padded.length; chunk += 64) {
            for (var t = 0; t < 16; t++) {
                W[t] = (padded[chunk + t * 4] << 24) | (padded[chunk + t * 4 + 1] << 16) |
                       (padded[chunk + t * 4 + 2] << 8) | padded[chunk + t * 4 + 3];
            }
            for (var t = 16; t < 80; t++) {
                W[t] = leftRotate(W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16], 1);
            }
            
            var a = H[0], b = H[1], c = H[2], d = H[3], e = H[4];
            
            for (var t = 0; t < 80; t++) {
                var f, k;
                if (t < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999; }
                else if (t < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
                else if (t < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
                else { f = b ^ c ^ d; k = 0xCA62C1D6; }
                
                var temp = (leftRotate(a, 5) + f + e + k + W[t]) >>> 0;
                e = d; d = c; c = leftRotate(b, 30); b = a; a = temp;
            }
            
            H[0] = (H[0] + a) >>> 0; H[1] = (H[1] + b) >>> 0;
            H[2] = (H[2] + c) >>> 0; H[3] = (H[3] + d) >>> 0; H[4] = (H[4] + e) >>> 0;
        }
        
        var result = new Uint8Array(20);
        for (var i = 0; i < 5; i++) {
            result[i * 4] = (H[i] >>> 24) & 0xff;
            result[i * 4 + 1] = (H[i] >>> 16) & 0xff;
            result[i * 4 + 2] = (H[i] >>> 8) & 0xff;
            result[i * 4 + 3] = H[i] & 0xff;
        }
        return result.buffer;
    };
    
    // =========================================================================
    // SubtleCrypto Implementation
    // =========================================================================
    var subtleCrypto = {
        digest: function(algorithm, data) {
            return new Promise(function(resolve, reject) {
                try {
                    var algoName = (typeof algorithm === 'string') ? algorithm : algorithm.name;
                    algoName = algoName.toUpperCase().replace('-', '');
                    
                    var bytes;
                    if (data instanceof ArrayBuffer) {
                        bytes = data;
                    } else if (ArrayBuffer.isView(data)) {
                        bytes = data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
                    } else {
                        reject(new Error('Invalid data type for digest'));
                        return;
                    }
                    
                    if (algoName === 'SHA256' || algoName === 'SHA-256') {
                        resolve(sha256(bytes));
                    } else if (algoName === 'SHA1' || algoName === 'SHA-1') {
                        resolve(sha1(bytes));
                    } else {
                        reject(new Error('Unsupported algorithm: ' + algoName));
                    }
                } catch (e) {
                    reject(e);
                }
            });
        },
        
        // Generate random key (simplified)
        generateKey: function(algorithm, extractable, keyUsages) {
            return new Promise(function(resolve, reject) {
                try {
                    var algoName = (typeof algorithm === 'string') ? algorithm : algorithm.name;
                    var keyLength = algorithm.length || 256;
                    var keyBytes = new Uint8Array(keyLength / 8);
                    getRandomValues(keyBytes);
                    
                    resolve({
                        type: 'secret',
                        extractable: extractable,
                        algorithm: { name: algoName, length: keyLength },
                        usages: keyUsages,
                        _keyData: keyBytes
                    });
                } catch (e) {
                    reject(e);
                }
            });
        },
        
        // Import key (simplified)
        importKey: function(format, keyData, algorithm, extractable, keyUsages) {
            return new Promise(function(resolve, reject) {
                try {
                    var algoName = (typeof algorithm === 'string') ? algorithm : algorithm.name;
                    var bytes;
                    if (format === 'raw') {
                        if (keyData instanceof ArrayBuffer) {
                            bytes = new Uint8Array(keyData);
                        } else if (ArrayBuffer.isView(keyData)) {
                            bytes = new Uint8Array(keyData.buffer, keyData.byteOffset, keyData.byteLength);
                        }
                    } else if (format === 'jwk') {
                        // Handle JWK format - extract k value (base64url encoded)
                        if (keyData.k) {
                            var base64 = keyData.k.replace(/-/g, '+').replace(/_/g, '/');
                            var binary = atob(base64);
                            bytes = new Uint8Array(binary.length);
                            for (var i = 0; i < binary.length; i++) {
                                bytes[i] = binary.charCodeAt(i);
                            }
                        }
                    }
                    
                    resolve({
                        type: 'secret',
                        extractable: extractable,
                        algorithm: { name: algoName },
                        usages: keyUsages,
                        _keyData: bytes
                    });
                } catch (e) {
                    reject(e);
                }
            });
        },
        
        // Export key
        exportKey: function(format, key) {
            return new Promise(function(resolve, reject) {
                try {
                    if (format === 'raw' && key._keyData) {
                        resolve(key._keyData.buffer);
                    } else {
                        reject(new Error('Unsupported export format'));
                    }
                } catch (e) {
                    reject(e);
                }
            });
        },
        
        // Sign with HMAC (simplified)
        sign: function(algorithm, key, data) {
            return new Promise(function(resolve, reject) {
                try {
                    // Simple HMAC-SHA256 implementation
                    var algoName = (typeof algorithm === 'string') ? algorithm : algorithm.name;
                    if (algoName !== 'HMAC') {
                        reject(new Error('Only HMAC signing is supported'));
                        return;
                    }
                    
                    var keyBytes = key._keyData;
                    var msgBytes;
                    if (data instanceof ArrayBuffer) {
                        msgBytes = new Uint8Array(data);
                    } else if (ArrayBuffer.isView(data)) {
                        msgBytes = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
                    }
                    
                    // HMAC: H((K XOR opad) || H((K XOR ipad) || message))
                    var blockSize = 64;
                    var opad = new Uint8Array(blockSize);
                    var ipad = new Uint8Array(blockSize);
                    
                    // If key > blockSize, hash it first
                    if (keyBytes.length > blockSize) {
                        keyBytes = new Uint8Array(sha256(keyBytes));
                    }
                    
                    for (var i = 0; i < blockSize; i++) {
                        var k = i < keyBytes.length ? keyBytes[i] : 0;
                        opad[i] = k ^ 0x5c;
                        ipad[i] = k ^ 0x36;
                    }
                    
                    // Inner hash: H(ipad || message)
                    var innerData = new Uint8Array(blockSize + msgBytes.length);
                    innerData.set(ipad);
                    innerData.set(msgBytes, blockSize);
                    var innerHash = new Uint8Array(sha256(innerData));
                    
                    // Outer hash: H(opad || innerHash)
                    var outerData = new Uint8Array(blockSize + 32);
                    outerData.set(opad);
                    outerData.set(innerHash, blockSize);
                    
                    resolve(sha256(outerData));
                } catch (e) {
                    reject(e);
                }
            });
        },
        
        verify: function() { return Promise.reject(new Error('verify not implemented')); },
        encrypt: function() { return Promise.reject(new Error('encrypt not implemented')); },
        decrypt: function() { return Promise.reject(new Error('decrypt not implemented')); },
        deriveBits: function() { return Promise.reject(new Error('deriveBits not implemented')); },
        deriveKey: function() { return Promise.reject(new Error('deriveKey not implemented')); },
        wrapKey: function() { return Promise.reject(new Error('wrapKey not implemented')); },
        unwrapKey: function() { return Promise.reject(new Error('unwrapKey not implemented')); }
    };
    
    // Create a complete crypto polyfill object
    var cryptoPolyfill = {
        getRandomValues: getRandomValues,
        randomUUID: randomUUID,
        subtle: subtleCrypto
    };
    
    // Force override the crypto object to avoid "secure origin" errors
    // We need to delete and recreate because crypto might be non-configurable
    try {
        // Try to override directly
        Object.defineProperty(window, 'crypto', {
            value: cryptoPolyfill,
            writable: true,
            configurable: true,
            enumerable: true
        });
    } catch (e) {
        // If that fails, try to override individual methods
        try {
            window.crypto.getRandomValues = getRandomValues;
            window.crypto.randomUUID = randomUUID;
            window.crypto.subtle = subtleCrypto;
        } catch (e2) {
            // Last resort: create a new global
            window.myCrypto = cryptoPolyfill;
            console.log('WebCrypto polyfill: Could not override crypto, use window.myCrypto instead');
        }
    }
    
    // Also expose as globalThis.crypto for modules
    if (typeof globalThis !== 'undefined') {
        try {
            Object.defineProperty(globalThis, 'crypto', {
                value: cryptoPolyfill,
                writable: true,
                configurable: true,
                enumerable: true
            });
        } catch (e) {}
    }
    
    console.log('WebCrypto polyfill loaded with SHA-256, SHA-1, HMAC support');
})();
)";

    JSEval(cryptoPolyfill);
    LogMsg("JSBindings: WebCrypto polyfill injected");

    LogMsg("JSBindings: Bound XPlane API (dataref, scenery, instance, graphics) and SkyScript API to view");
}

// =========================================================================
// DataRef Lookup Functions
// =========================================================================

JSValue JSBindings::JS_FindDataRef(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: findDataRef requires a string argument");
        return JSValue();
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (ref)
    {
        return JSValue(true);
    }
    return JSValue();
}

JSValue JSBindings::JS_CanWriteDataRef(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: canWriteDataRef requires a string argument");
        return JSValue(false);
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        return JSValue(false);
    }

    return JSValue(XPLMCanWriteDataRef(ref) != 0);
}

JSValue JSBindings::JS_GetDataRefTypes(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: getDataRefTypes requires a string argument");
        return JSValue();
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        return JSValue();
    }

    XPLMDataTypeID types = XPLMGetDataRefTypes(ref);

    JSObject result;
    result["int"] = JSValue((types & xplmType_Int) != 0);
    result["float"] = JSValue((types & xplmType_Float) != 0);
    result["double"] = JSValue((types & xplmType_Double) != 0);
    result["intArray"] = JSValue((types & xplmType_IntArray) != 0);
    result["floatArray"] = JSValue((types & xplmType_FloatArray) != 0);
    result["data"] = JSValue((types & xplmType_Data) != 0);

    return JSValue(static_cast<JSObjectRef>(result));
}

// =========================================================================
// Data Getters
// =========================================================================

JSValue JSBindings::JS_GetDatai(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: getInt requires a string argument");
        return JSValue(0);
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(0);
    }

    return JSValue(XPLMGetDatai(ref));
}

JSValue JSBindings::JS_GetDataf(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: getFloat requires a string argument");
        return JSValue(0.0);
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(0.0);
    }

    return JSValue(static_cast<double>(XPLMGetDataf(ref)));
}

JSValue JSBindings::JS_GetDatad(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: getDouble requires a string argument");
        return JSValue(0.0);
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(0.0);
    }

    return JSValue(XPLMGetDatad(ref));
}

JSValue JSBindings::JS_GetDatavi(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: getIntArray requires a string argument");
        return JSValue();
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue();
    }

    // Get array size
    int size = XPLMGetDatavi(ref, nullptr, 0, 0);
    if (size <= 0)
    {
        return JSValue();
    }

    // Parse optional offset and count
    int offset = 0;
    int count = size;

    if (args.size() > 1 && args[1].IsNumber())
    {
        offset = static_cast<int>(args[1].ToNumber());
    }
    if (args.size() > 2 && args[2].IsNumber())
    {
        count = static_cast<int>(args[2].ToNumber());
    }

    // Clamp values
    if (offset < 0)
        offset = 0;
    if (offset >= size)
        return JSValue();
    if (count > size - offset)
        count = size - offset;

    // Read the data
    std::vector<int> values(count);
    XPLMGetDatavi(ref, values.data(), offset, count);

    // Convert to JS array
    JSArray result;
    for (int i = 0; i < count; i++)
    {
        result.push(JSValue(values[i]));
    }

    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue JSBindings::JS_GetDatavf(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: getFloatArray requires a string argument");
        return JSValue();
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue();
    }

    // Get array size
    int size = XPLMGetDatavf(ref, nullptr, 0, 0);
    if (size <= 0)
    {
        return JSValue();
    }

    // Parse optional offset and count
    int offset = 0;
    int count = size;

    if (args.size() > 1 && args[1].IsNumber())
    {
        offset = static_cast<int>(args[1].ToNumber());
    }
    if (args.size() > 2 && args[2].IsNumber())
    {
        count = static_cast<int>(args[2].ToNumber());
    }

    // Clamp values
    if (offset < 0)
        offset = 0;
    if (offset >= size)
        return JSValue();
    if (count > size - offset)
        count = size - offset;

    // Read the data
    std::vector<float> values(count);
    XPLMGetDatavf(ref, values.data(), offset, count);

    // Convert to JS array
    JSArray result;
    for (int i = 0; i < count; i++)
    {
        result.push(JSValue(static_cast<double>(values[i])));
    }

    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue JSBindings::JS_GetDatab(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: getData requires a string argument");
        return JSValue("");
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue("");
    }

    // Get data size
    int size = XPLMGetDatab(ref, nullptr, 0, 0);
    if (size <= 0)
    {
        return JSValue("");
    }

    // Parse optional offset and maxBytes
    int offset = 0;
    int maxBytes = size;

    if (args.size() > 1 && args[1].IsNumber())
    {
        offset = static_cast<int>(args[1].ToNumber());
    }
    if (args.size() > 2 && args[2].IsNumber())
    {
        maxBytes = static_cast<int>(args[2].ToNumber());
    }

    // Clamp values
    if (offset < 0)
        offset = 0;
    if (offset >= size)
        return JSValue("");
    if (maxBytes > size - offset)
        maxBytes = size - offset;

    // Read the data
    std::vector<char> buffer(maxBytes + 1, 0);
    XPLMGetDatab(ref, buffer.data(), offset, maxBytes);

    return JSValue(buffer.data());
}

// =========================================================================
// Data Setters
// =========================================================================

JSValue JSBindings::JS_SetDatai(const JSObject &thisObject, const JSArgs &args)
{
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsNumber())
    {
        LogMsg("JSBindings: setInt requires (string, number) arguments");
        return JSValue(false);
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    int value = static_cast<int>(args[1].ToNumber());

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(false);
    }

    if (!XPLMCanWriteDataRef(ref))
    {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }

    XPLMSetDatai(ref, value);
    return JSValue(true);
}

JSValue JSBindings::JS_SetDataf(const JSObject &thisObject, const JSArgs &args)
{
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsNumber())
    {
        LogMsg("JSBindings: setFloat requires (string, number) arguments");
        return JSValue(false);
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    float value = static_cast<float>(args[1].ToNumber());

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(false);
    }

    if (!XPLMCanWriteDataRef(ref))
    {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }

    XPLMSetDataf(ref, value);
    return JSValue(true);
}

JSValue JSBindings::JS_SetDatad(const JSObject &thisObject, const JSArgs &args)
{
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsNumber())
    {
        LogMsg("JSBindings: setDouble requires (string, number) arguments");
        return JSValue(false);
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();
    double value = args[1].ToNumber();

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(false);
    }

    if (!XPLMCanWriteDataRef(ref))
    {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }

    XPLMSetDatad(ref, value);
    return JSValue(true);
}

JSValue JSBindings::JS_SetDatavi(const JSObject &thisObject, const JSArgs &args)
{
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsArray())
    {
        LogMsg("JSBindings: setIntArray requires (string, array) arguments");
        return JSValue(false);
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(false);
    }

    if (!XPLMCanWriteDataRef(ref))
    {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }

    // Parse optional offset
    int offset = 0;
    if (args.size() > 2 && args[2].IsNumber())
    {
        offset = static_cast<int>(args[2].ToNumber());
    }

    // Convert JS array to int array
    JSArray arr = args[1].ToArray();
    std::vector<int> values;
    for (unsigned i = 0; i < arr.length(); i++)
    {
        values.push_back(static_cast<int>(arr[i].ToNumber()));
    }

    XPLMSetDatavi(ref, values.data(), offset, static_cast<int>(values.size()));
    return JSValue(true);
}

JSValue JSBindings::JS_SetDatavf(const JSObject &thisObject, const JSArgs &args)
{
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsArray())
    {
        LogMsg("JSBindings: setFloatArray requires (string, array) arguments");
        return JSValue(false);
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(false);
    }

    if (!XPLMCanWriteDataRef(ref))
    {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }

    // Parse optional offset
    int offset = 0;
    if (args.size() > 2 && args[2].IsNumber())
    {
        offset = static_cast<int>(args[2].ToNumber());
    }

    // Convert JS array to float array
    JSArray arr = args[1].ToArray();
    std::vector<float> values;
    for (unsigned i = 0; i < arr.length(); i++)
    {
        values.push_back(static_cast<float>(arr[i].ToNumber()));
    }

    XPLMSetDatavf(ref, values.data(), offset, static_cast<int>(values.size()));
    return JSValue(true);
}

JSValue JSBindings::JS_SetDatab(const JSObject &thisObject, const JSArgs &args)
{
    if (args.size() < 2 || !args[0].IsString() || !args[1].IsString())
    {
        LogMsg("JSBindings: setData requires (string, string) arguments");
        return JSValue(false);
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    String value = args[1].ToString();
    std::string value_str = value.utf8().data();

    XPLMDataRef ref = GetCachedDataRef(name_str);
    if (!ref)
    {
        LogMsg("JSBindings: dataref not found: %s", name_str.c_str());
        return JSValue(false);
    }

    if (!XPLMCanWriteDataRef(ref))
    {
        LogMsg("JSBindings: dataref is read-only: %s", name_str.c_str());
        return JSValue(false);
    }

    // Parse optional offset
    int offset = 0;
    if (args.size() > 2 && args[2].IsNumber())
    {
        offset = static_cast<int>(args[2].ToNumber());
    }

    XPLMSetDatab(ref, const_cast<char *>(value_str.c_str()), offset, static_cast<int>(value_str.length()));
    return JSValue(true);
}

// =========================================================================
// Scenery API - Object Loading
// =========================================================================

JSValue JSBindings::JS_LoadObject(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: loadObject requires a string path argument");
        return JSValue();
    }

    String path = args[0].ToString();
    std::string path_str = path.utf8().data();

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = object_cache_.find(path_str);
        if (it != object_cache_.end())
        {
            // Return the path as the "handle" - we use path-based lookup
            return JSValue(path.utf8().data());
        }
    }

    // Load the object
    XPLMObjectRef obj = XPLMLoadObject(path_str.c_str());
    if (!obj)
    {
        LogMsg("JSBindings: failed to load object: %s", path_str.c_str());
        return JSValue();
    }

    // Cache it
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        object_cache_[path_str] = obj;
    }

    LogMsg("JSBindings: loaded object: %s", path_str.c_str());
    return JSValue(path.utf8().data());
}

JSValue JSBindings::JS_UnloadObject(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: unloadObject requires a string path argument");
        return JSValue(false);
    }

    String path = args[0].ToString();
    std::string path_str = path.utf8().data();

    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = object_cache_.find(path_str);
    if (it == object_cache_.end())
    {
        LogMsg("JSBindings: object not found for unload: %s", path_str.c_str());
        return JSValue(false);
    }

    XPLMUnloadObject(it->second);
    object_cache_.erase(it);

    LogMsg("JSBindings: unloaded object: %s", path_str.c_str());
    return JSValue(true);
}

// =========================================================================
// Scenery API - Terrain Probing
// =========================================================================

JSValue JSBindings::JS_CreateProbe(const JSObject &thisObject, const JSArgs &args)
{
    // Optional probe type argument (default to Y probe)
    int probeType = xplm_ProbeY;
    if (!args.empty() && args[0].IsNumber())
    {
        probeType = static_cast<int>(args[0].ToNumber());
    }

    XPLMProbeRef probe = XPLMCreateProbe(static_cast<XPLMProbeType>(probeType));
    if (!probe)
    {
        LogMsg("JSBindings: failed to create terrain probe");
        return JSValue();
    }

    int id = next_probe_id_++;
    probe_cache_[id] = probe;

    LogMsg("JSBindings: created terrain probe with ID %d", id);
    return JSValue(id);
}

JSValue JSBindings::JS_DestroyProbe(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsNumber())
    {
        LogMsg("JSBindings: destroyProbe requires a probe ID argument");
        return JSValue(false);
    }

    int id = static_cast<int>(args[0].ToNumber());

    auto it = probe_cache_.find(id);
    if (it == probe_cache_.end())
    {
        LogMsg("JSBindings: probe not found: %d", id);
        return JSValue(false);
    }

    XPLMDestroyProbe(it->second);
    probe_cache_.erase(it);

    LogMsg("JSBindings: destroyed probe %d", id);
    return JSValue(true);
}

JSValue JSBindings::JS_ProbeTerrainXYZ(const JSObject &thisObject, const JSArgs &args)
{
    if (args.size() < 4 || !args[0].IsNumber() || !args[1].IsNumber() ||
        !args[2].IsNumber() || !args[3].IsNumber())
    {
        LogMsg("JSBindings: probeTerrain requires (probeId, x, y, z) arguments");
        return JSValue();
    }

    int probeId = static_cast<int>(args[0].ToNumber());
    double x = args[1].ToNumber();
    double y = args[2].ToNumber();
    double z = args[3].ToNumber();

    auto it = probe_cache_.find(probeId);
    if (it == probe_cache_.end())
    {
        LogMsg("JSBindings: probe not found: %d", probeId);
        return JSValue();
    }

    XPLMProbeInfo_t info;
    info.structSize = sizeof(XPLMProbeInfo_t);

    XPLMProbeResult result = XPLMProbeTerrainXYZ(it->second,
                                                 static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), &info);

    if (result != xplm_ProbeHitTerrain)
    {
        // Return result code so caller knows what happened
        JSObject errorResult;
        errorResult["hit"] = JSValue(false);
        errorResult["result"] = JSValue(static_cast<int>(result));
        return JSValue(static_cast<JSObjectRef>(errorResult));
    }

    JSObject jsResult;
    jsResult["hit"] = JSValue(true);
    jsResult["x"] = JSValue(info.locationX);
    jsResult["y"] = JSValue(info.locationY);
    jsResult["z"] = JSValue(info.locationZ);
    jsResult["normalX"] = JSValue(info.normalX);
    jsResult["normalY"] = JSValue(info.normalY);
    jsResult["normalZ"] = JSValue(info.normalZ);
    jsResult["velocityX"] = JSValue(info.velocityX);
    jsResult["velocityY"] = JSValue(info.velocityY);
    jsResult["velocityZ"] = JSValue(info.velocityZ);
    jsResult["isWet"] = JSValue(info.is_wet != 0);

    return JSValue(static_cast<JSObjectRef>(jsResult));
}

// =========================================================================
// Scenery API - Magnetic Variation
// =========================================================================

JSValue JSBindings::JS_GetMagneticVariation(const JSObject &thisObject, const JSArgs &args)
{
    if (args.size() < 2 || !args[0].IsNumber() || !args[1].IsNumber())
    {
        LogMsg("JSBindings: getMagneticVariation requires (latitude, longitude) arguments");
        return JSValue(0.0);
    }

    double latitude = args[0].ToNumber();
    double longitude = args[1].ToNumber();

    float variation = XPLMGetMagneticVariation(latitude, longitude);
    return JSValue(static_cast<double>(variation));
}

JSValue JSBindings::JS_DegTrueToDegMagnetic(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsNumber())
    {
        LogMsg("JSBindings: degTrueToMagnetic requires a heading argument");
        return JSValue(0.0);
    }

    float headingTrue = static_cast<float>(args[0].ToNumber());
    float headingMag = XPLMDegTrueToDegMagnetic(headingTrue);
    return JSValue(static_cast<double>(headingMag));
}

JSValue JSBindings::JS_DegMagneticToDegTrue(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsNumber())
    {
        LogMsg("JSBindings: degMagneticToTrue requires a heading argument");
        return JSValue(0.0);
    }

    float headingMag = static_cast<float>(args[0].ToNumber());
    float headingTrue = XPLMDegMagneticToDegTrue(headingMag);
    return JSValue(static_cast<double>(headingTrue));
}

// =========================================================================
// Instance API - Object Instancing
// =========================================================================

JSValue JSBindings::JS_CreateInstance(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: createInstance requires (objectPath, [datarefs]) arguments");
        return JSValue();
    }

    String path = args[0].ToString();
    std::string path_str = path.utf8().data();

    // Look up the object
    XPLMObjectRef obj = nullptr;
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = object_cache_.find(path_str);
        if (it == object_cache_.end())
        {
            LogMsg("JSBindings: object not loaded: %s", path_str.c_str());
            return JSValue();
        }
        obj = it->second;
    }

    // Parse dataref names array (optional)
    std::vector<std::string> dataref_strs;
    std::vector<const char *> datarefs;

    if (args.size() > 1 && args[1].IsArray())
    {
        JSArray arr = args[1].ToArray();
        for (unsigned i = 0; i < arr.length(); i++)
        {
            if (arr[i].IsString())
            {
                String s = arr[i].ToString();
                dataref_strs.push_back(s.utf8().data());
            }
        }

        // Build C string array (must be null-terminated)
        for (const auto &s : dataref_strs)
        {
            datarefs.push_back(s.c_str());
        }
        datarefs.push_back(nullptr);
    }
    else
    {
        // No datarefs - still need null terminator
        datarefs.push_back(nullptr);
    }

    XPLMInstanceRef instance = XPLMCreateInstance(obj, datarefs.data());
    if (!instance)
    {
        LogMsg("JSBindings: failed to create instance of: %s", path_str.c_str());
        return JSValue();
    }

    int id = next_instance_id_++;
    instance_cache_[id] = instance;

    LogMsg("JSBindings: created instance %d of object: %s", id, path_str.c_str());
    return JSValue(id);
}

JSValue JSBindings::JS_DestroyInstance(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsNumber())
    {
        LogMsg("JSBindings: destroyInstance requires an instance ID argument");
        return JSValue(false);
    }

    int id = static_cast<int>(args[0].ToNumber());

    auto it = instance_cache_.find(id);
    if (it == instance_cache_.end())
    {
        LogMsg("JSBindings: instance not found: %d", id);
        return JSValue(false);
    }

    XPLMDestroyInstance(it->second);
    instance_cache_.erase(it);

    LogMsg("JSBindings: destroyed instance %d", id);
    return JSValue(true);
}

JSValue JSBindings::JS_InstanceSetPosition(const JSObject &thisObject, const JSArgs &args)
{
    if (args.size() < 2 || !args[0].IsNumber() || !args[1].IsObject())
    {
        LogMsg("JSBindings: instanceSetPosition requires (instanceId, position, [data]) arguments");
        return JSValue(false);
    }

    int id = static_cast<int>(args[0].ToNumber());

    auto it = instance_cache_.find(id);
    if (it == instance_cache_.end())
    {
        LogMsg("JSBindings: instance not found: %d", id);
        return JSValue(false);
    }

    // Parse position object
    JSObject pos = args[1].ToObject();
    XPLMDrawInfo_t drawInfo;

    // Required position fields
    drawInfo.structSize = sizeof(XPLMDrawInfo_t);
    drawInfo.x = static_cast<float>(pos["x"].ToNumber());
    drawInfo.y = static_cast<float>(pos["y"].ToNumber());
    drawInfo.z = static_cast<float>(pos["z"].ToNumber());

    // Optional rotation fields (default to 0)
    drawInfo.pitch = pos["pitch"].IsNumber() ? static_cast<float>(pos["pitch"].ToNumber()) : 0.0f;
    drawInfo.heading = pos["heading"].IsNumber() ? static_cast<float>(pos["heading"].ToNumber()) : 0.0f;
    drawInfo.roll = pos["roll"].IsNumber() ? static_cast<float>(pos["roll"].ToNumber()) : 0.0f;

    // Parse data array (optional - for animated datarefs)
    std::vector<float> data;
    if (args.size() > 2 && args[2].IsArray())
    {
        JSArray arr = args[2].ToArray();
        for (unsigned i = 0; i < arr.length(); i++)
        {
            data.push_back(static_cast<float>(arr[i].ToNumber()));
        }
    }

    XPLMInstanceSetPosition(it->second, &drawInfo, data.empty() ? nullptr : data.data());
    return JSValue(true);
}

// =========================================================================
// Graphics API - Coordinate Conversion
// =========================================================================

JSValue JSBindings::JS_LocalToWorld(const JSObject &thisObject, const JSArgs &args)
{
    if (args.size() < 3 || !args[0].IsNumber() || !args[1].IsNumber() || !args[2].IsNumber())
    {
        LogMsg("JSBindings: localToWorld requires (x, y, z) arguments");
        return JSValue();
    }

    double x = args[0].ToNumber();
    double y = args[1].ToNumber();
    double z = args[2].ToNumber();

    double latitude, longitude, altitude;
    XPLMLocalToWorld(x, y, z, &latitude, &longitude, &altitude);

    JSObject result;
    result["latitude"] = JSValue(latitude);
    result["longitude"] = JSValue(longitude);
    result["altitude"] = JSValue(altitude);

    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue JSBindings::JS_WorldToLocal(const JSObject &thisObject, const JSArgs &args)
{
    if (args.size() < 3 || !args[0].IsNumber() || !args[1].IsNumber() || !args[2].IsNumber())
    {
        LogMsg("JSBindings: worldToLocal requires (latitude, longitude, altitude) arguments");
        return JSValue();
    }

    double latitude = args[0].ToNumber();
    double longitude = args[1].ToNumber();
    double altitude = args[2].ToNumber();

    double x, y, z;
    XPLMWorldToLocal(latitude, longitude, altitude, &x, &y, &z);

    JSObject result;
    result["x"] = JSValue(x);
    result["y"] = JSValue(y);
    result["z"] = JSValue(z);

    return JSValue(static_cast<JSObjectRef>(result));
}

// =========================================================================
// SkyScript App Management API
// =========================================================================

JSValue JSBindings::JS_ListApps(const JSObject &thisObject, const JSArgs &args)
{
    auto names = Manager::instance().getAppNames();

    JSArray result;
    for (size_t i = 0; i < names.size(); i++)
    {
        result.push(JSValue(names[i].c_str()));
    }

    return JSValue(static_cast<JSObjectRef>(result));
}

JSValue JSBindings::JS_ReloadApp(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: reloadApp requires a string argument (app name)");
        return JSValue(false);
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    return JSValue(Manager::instance().reloadApp(name_str));
}

JSValue JSBindings::JS_OpenAppWindow(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: openAppWindow requires a string argument (app name)");
        return JSValue(false);
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    return JSValue(Manager::instance().openAppWindow(name_str));
}

JSValue JSBindings::JS_OpenAppInspector(const JSObject &thisObject, const JSArgs &args)
{
    if (args.empty() || !args[0].IsString())
    {
        LogMsg("JSBindings: openAppInspector requires a string argument (app name)");
        return JSValue(false);
    }

    String name = args[0].ToString();
    std::string name_str = name.utf8().data();

    return JSValue(Manager::instance().openAppInspector(name_str));
}
