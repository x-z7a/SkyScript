import React, { useState, useEffect, useCallback } from 'react';
import './App.css';

// Object placement state
interface PlacedObject {
  obj: string;
  instanceId: number;
}

// HID device info from enumerate()
interface HidDeviceInfo {
  path: string;
  vendorId: number;
  productId: number;
  serialNumber: string;
  manufacturer: string;
  product: string;
  usagePage: number;
  usage: number;
  interfaceNumber: number;
  releaseNumber: number;
}

function App() {
  const [count, setCount] = useState(0);
  const [message, setMessage] = useState('');
  const [time, setTime] = useState(new Date().toLocaleTimeString());

  // HID device state
  const [hidDevices, setHidDevices] = useState<HidDeviceInfo[]>([]);
  const [hidScanning, setHidScanning] = useState(false);
  const [hidError, setHidError] = useState<string | null>(null);

  // X-Plane data states
  const [altitude, setAltitude] = useState<number | null>(null);
  const [airspeed, setAirspeed] = useState<number | null>(null);
  const [heading, setHeading] = useState<number | null>(null);

  // Object placement state
  const [placedObject, setPlacedObject] = useState<PlacedObject | null>(null);
  const [objectStatus, setObjectStatus] = useState<string>('');

  useEffect(() => {
    const timer = setInterval(() => {
      setTime(new Date().toLocaleTimeString());

      // Read X-Plane data if available
      if (typeof XPlane !== 'undefined') {
        try {
          setAltitude(XPlane.dataref.getFloat("sim/flightmodel/position/elevation") * 3.28084); // meters to feet
          setAirspeed(XPlane.dataref.getFloat("sim/flightmodel/position/indicated_airspeed"));
          setHeading(XPlane.dataref.getFloat("sim/flightmodel/position/mag_psi"));
        } catch (e) {
          console.log("Error reading datarefs:", e);
        }
      }
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Clean up object on unmount
  useEffect(() => {
    return () => {
      if (placedObject && typeof XPlane !== 'undefined') {
        XPlane.instance.destroy(placedObject.instanceId);
        XPlane.scenery.unloadObject(placedObject.obj);
      }
    };
  }, [placedObject]);

  const handleClick = () => {
    setCount(count + 1);
    setMessage(`Button clicked ${count + 1} times!`);
  };

  const toggleObject = useCallback(() => {
    if (typeof XPlane === 'undefined') {
      setObjectStatus('XPlane API not available');
      return;
    }

    if (placedObject) {
      // Remove existing object
      try {
        XPlane.instance.destroy(placedObject.instanceId);
        XPlane.scenery.unloadObject(placedObject.obj);
        setPlacedObject(null);
        setObjectStatus('Object removed');
      } catch (e) {
        setObjectStatus(`Error removing: ${e}`);
      }
    } else {
      // Place new object in front of aircraft
      try {
        // Get aircraft position and heading
        const acfX = XPlane.dataref.getDouble("sim/flightmodel/position/local_x");
        const acfY = XPlane.dataref.getDouble("sim/flightmodel/position/local_y");
        const acfZ = XPlane.dataref.getDouble("sim/flightmodel/position/local_z");
        const acfHeading = XPlane.dataref.getFloat("sim/flightmodel/position/psi"); // true heading

        // Calculate position 50m ahead of aircraft
        const distance = 50; // meters
        const headingRad = (acfHeading * Math.PI) / 180;
        const offsetX = Math.sin(headingRad) * distance;
        const offsetZ = -Math.cos(headingRad) * distance; // negative because Z is south

        // Try several object paths that might exist in X-Plane
        const objectPaths = [
          "Resources/default scenery/airport scenery/Dynamic_Vehicles/catering_truck.obj",
          "Resources/default scenery/airport scenery/Ramp_Equipment/Cone.obj",
          "Resources/default scenery/airport scenery/Ramp_Equipment/Fire_Extinguisher.obj",
          "Resources/default scenery/sim objects/dynamic/windsock.obj",
        ];

        let obj: string | null = null;
        for (const path of objectPaths) {
          obj = XPlane.scenery.loadObject(path);
          if (obj) {
            break;
          }
        }

        if (!obj) {
          setObjectStatus('No valid object found - check X-Plane paths');
          return;
        }

        // Create instance
        const instanceId = XPlane.instance.create(obj);
        if (!instanceId) {
          XPlane.scenery.unloadObject(obj);
          setObjectStatus('Failed to create instance');
          return;
        }

        // Probe terrain to get ground level
        const probeId = XPlane.scenery.createProbe();
        if (probeId) {
          const terrain = XPlane.scenery.probeTerrain(probeId, acfX + offsetX, acfY, acfZ + offsetZ);
          XPlane.scenery.destroyProbe(probeId);

          // Position on ground
          const groundY = (terrain && terrain.hit && terrain.y !== undefined) ? terrain.y : acfY;
          XPlane.instance.setPosition(instanceId, {
            x: acfX + offsetX,
            y: groundY,
            z: acfZ + offsetZ,
            heading: acfHeading,
            pitch: 0,
            roll: 0
          });
        } else {
          // No probe, just place at aircraft altitude
          XPlane.instance.setPosition(instanceId, {
            x: acfX + offsetX,
            y: acfY,
            z: acfZ + offsetZ,
            heading: acfHeading,
            pitch: 0,
            roll: 0
          });
        }

        setPlacedObject({ obj, instanceId });
        setObjectStatus(`Object placed 50m ahead!`);
      } catch (e) {
        setObjectStatus(`Error: ${e}`);
      }
    }
  }, [placedObject]);

  const scanHidDevices = useCallback(() => {
    if (typeof XPlane === 'undefined') {
      setHidError('XPlane API not available');
      return;
    }
    setHidScanning(true);
    setHidError(null);
    try {
      const devices: HidDeviceInfo[] = XPlane.hid.enumerate();
      setHidDevices(devices);
    } catch (e) {
      setHidError(`Error scanning: ${e}`);
      setHidDevices([]);
    } finally {
      setHidScanning(false);
    }
  }, []);

  return (
    <div className="hello-world">
      <div className="container">
        <h1>👋 Hello, X-Plane!</h1>
        <p className="subtitle">Welcome to SkyScript</p>

        <div className="clock">
          <span className="clock-icon">🕐</span>
          <span className="time">{time}</span>
        </div>

        {/* X-Plane Flight Data */}
        {altitude !== null && (
          <div className="flight-data">
            <div className="data-item">
              <span className="data-icon">📏</span>
              <span className="data-label">Altitude</span>
              <span className="data-value">{altitude.toFixed(0)} ft</span>
            </div>
            <div className="data-item">
              <span className="data-icon">💨</span>
              <span className="data-label">Airspeed</span>
              <span className="data-value">{airspeed?.toFixed(0)} kts</span>
            </div>
            <div className="data-item">
              <span className="data-icon">🧭</span>
              <span className="data-label">Heading</span>
              <span className="data-value">{heading?.toFixed(0)}°</span>
            </div>
          </div>
        )}

        {/* Object Placement Toggle */}
        <div className="object-section">
          <button
            className={`object-btn ${placedObject ? 'active' : ''}`}
            onClick={toggleObject}
          >
            {placedObject ? '🔴 Remove Truck' : '🟠 Place Truck'}
          </button>
          {objectStatus && <p className="object-status">{objectStatus}</p>}
        </div>

        {/* USB HID Devices */}
        <div className="hid-section">
          <h3 className="hid-title">🔌 USB HID Devices</h3>
          <button
            className={`hid-scan-btn ${hidScanning ? 'scanning' : ''}`}
            onClick={scanHidDevices}
            disabled={hidScanning}
          >
            {hidScanning ? '⏳ Scanning…' : '🔍 Scan Devices'}
          </button>
          {hidError && <p className="hid-error">{hidError}</p>}
          {hidDevices.length > 0 ? (
            <div className="hid-device-list">
              <p className="hid-count">{hidDevices.length} device(s) found</p>
              {hidDevices.map((dev, i) => (
                <div key={i} className="hid-device-card">
                  <span className="hid-device-name">
                    {dev.product || 'Unknown Device'}
                  </span>
                  <span className="hid-device-mfr">
                    {dev.manufacturer || '—'}
                  </span>
                  <span className="hid-device-ids">
                    VID: 0x{dev.vendorId.toString(16).toUpperCase().padStart(4, '0')}{' · '}
                    PID: 0x{dev.productId.toString(16).toUpperCase().padStart(4, '0')}
                  </span>
                </div>
              ))}
            </div>
          ) : (
            !hidError && <p className="hid-empty">No devices found — click Scan</p>
          )}
        </div>

        <div className="counter-section">
          <button className="counter-btn" onClick={handleClick}>
            Click Me! 🚀
          </button>
          <div className="counter-display">
            <span className="count">{count}</span>
            <span className="label">clicks</span>
          </div>
        </div>

        {message && <p className="message">{message}</p>}

        <div className="input-section">
          <input
            type="text"
            placeholder="Type something..."
            className="text-input"
            onChange={(e) => setMessage(e.target.value ? `You typed: ${e.target.value}` : '')}
          />
        </div>

        {/* App Info */}
        {typeof SkyScript !== 'undefined' && SkyScript.app && (
          <div className="app-info">
            <h3 className="app-info-title">📋 App Info</h3>
            <div className="app-info-grid">
              <span className="app-info-label">Name</span>
              <span className="app-info-value">{SkyScript.app.name}</span>
              <span className="app-info-label">Display Name</span>
              <span className="app-info-value">{SkyScript.app.displayName}</span>
              <span className="app-info-label">Directory</span>
              <span className="app-info-value app-info-path">{SkyScript.app.dir}</span>
            </div>
          </div>
        )}

        <div className="info-box">
          <p>✈️ This app is running inside X-Plane via SkyScript!</p>
          <p>📦 Built with React + TypeScript</p>
          <p>🎨 Rendered by Ultralight</p>
        </div>
      </div>
    </div>
  );
}

export default App;
