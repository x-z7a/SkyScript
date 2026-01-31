import React, { useState, useEffect } from 'react';
import './App.css';

function App() {
  const [count, setCount] = useState(0);
  const [message, setMessage] = useState('');
  const [time, setTime] = useState(new Date().toLocaleTimeString());
  
  // X-Plane data states
  const [altitude, setAltitude] = useState<number | null>(null);
  const [airspeed, setAirspeed] = useState<number | null>(null);
  const [heading, setHeading] = useState<number | null>(null);

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

  const handleClick = () => {
    setCount(count + 1);
    setMessage(`Button clicked ${count + 1} times!`);
  };

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
