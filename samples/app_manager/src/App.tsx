import React, { useState } from 'react';
import './App.css';

interface AppInfo {
  name: string;
  description: string;
  version: string;
  status: 'running' | 'stopped' | 'error';
}

function App() {
  const [apps] = useState<AppInfo[]>([
    { name: 'app_manager', description: 'SkyScript App Manager', version: '1.0.0', status: 'running' },
    { name: 'hello-world', description: 'Hello World Demo App', version: '1.0.0', status: 'running' },
  ]);

  const [selectedApp, setSelectedApp] = useState<string | null>(null);

  const getStatusColor = (status: AppInfo['status']) => {
    switch (status) {
      case 'running': return '#4CAF50';
      case 'stopped': return '#9E9E9E';
      case 'error': return '#F44336';
    }
  };

  return (
    <div className="app-manager">
      <header className="header">
        <h1>✈️ SkyScript App Manager</h1>
        <p className="subtitle">Manage your X-Plane JavaScript apps</p>
      </header>

      <div className="apps-container">
        <div className="apps-list">
          <h2>Installed Apps</h2>
          {apps.map((app) => (
            <div
              key={app.name}
              className={`app-card ${selectedApp === app.name ? 'selected' : ''}`}
              onClick={() => setSelectedApp(app.name)}
            >
              <div className="app-status" style={{ backgroundColor: getStatusColor(app.status) }} />
              <div className="app-info">
                <h3>{app.name}</h3>
                <p>{app.description}</p>
                <span className="version">v{app.version}</span>
              </div>
            </div>
          ))}
        </div>

        <div className="app-details">
          {selectedApp ? (
            <>
              <h2>{selectedApp}</h2>
              <div className="detail-row">
                <label>Status:</label>
                <span className="status-badge running">Running</span>
              </div>
              <div className="actions">
                <button className="btn btn-stop">Stop</button>
                <button className="btn btn-restart">Restart</button>
                <button className="btn btn-open">Open Window</button>
              </div>
            </>
          ) : (
            <div className="no-selection">
              <p>Select an app to view details</p>
            </div>
          )}
        </div>
      </div>

      <footer className="footer">
        <p>SkyScript v1.0.0 • {apps.length} apps installed</p>
      </footer>
    </div>
  );
}

export default App;
