import React, { useState, useEffect } from 'react';
import './App.css';

function App() {
  const [count, setCount] = useState(0);
  const [message, setMessage] = useState('');
  const [time, setTime] = useState(new Date().toLocaleTimeString());

  useEffect(() => {
    const timer = setInterval(() => {
      setTime(new Date().toLocaleTimeString());
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
