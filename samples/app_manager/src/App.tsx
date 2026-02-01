import React, { useState, useEffect } from 'react';
import './App.css';

// Declare global SkyScript API
declare global {
  interface SkyScriptAPI {
    listApps(): string[];
    reloadApp(name: string): boolean;
    openAppWindow(name: string): boolean;
  }
  const SkyScript: SkyScriptAPI | undefined;
}

interface AppInfo {
  name: string;
  description: string;
  version: string;
}

function App() {
  const [apps, setApps] = useState<AppInfo[]>([]);
  const [selectedApp, setSelectedApp] = useState<string | null>(null);

  // Load apps from SkyScript API on mount
  useEffect(() => {
    refreshAppList();
  }, []);

  const refreshAppList = () => {
    if (typeof SkyScript !== 'undefined' && SkyScript.listApps) {
      const appNames = SkyScript.listApps();
      // Filter out app_manager - it shouldn't manage itself
      const filteredApps = appNames.filter(name => name !== 'app_manager');
      const appList: AppInfo[] = filteredApps.map(name => ({
        name,
        description: `${name} App`,
        version: '1.0.0',
      }));
      setApps(appList);
    } else {
      // Fallback for development/preview (exclude app_manager)
      setApps([
        { name: 'hello-world', description: 'Hello World Demo App', version: '1.0.0' },
      ]);
    }
  };

  const handleRestart = (appName: string) => {
    if (typeof SkyScript !== 'undefined' && SkyScript.reloadApp) {
      SkyScript.reloadApp(appName);
      console.log(`Restarted app: ${appName}`);
    }
  };

  const handleOpenWindow = (appName: string) => {
    if (typeof SkyScript !== 'undefined' && SkyScript.openAppWindow) {
      SkyScript.openAppWindow(appName);
      console.log(`Opened window for app: ${appName}`);
    }
  };

  const selectedAppInfo = apps.find(app => app.name === selectedApp);

  return (
    <div className="app-manager">
      <header className="header">
        <h1>✈️ SkyScript App Manager</h1>
        <p className="subtitle">Manage your X-Plane JavaScript apps</p>
      </header>

      <div className="apps-container">
        <div className="apps-list">
          <h2>Installed Apps</h2>
          {apps.length === 0 ? (
            <div className="no-apps">
              <p>No apps installed</p>
            </div>
          ) : (
            apps.map((app) => (
              <div
                key={app.name}
                className={`app-card ${selectedApp === app.name ? 'selected' : ''}`}
                onClick={() => setSelectedApp(app.name)}
              >
                <div className="app-info">
                  <h3>{app.name}</h3>
                  <p>{app.description}</p>
                  <span className="version">v{app.version}</span>
                </div>
              </div>
            ))
          )}
        </div>

        <div className="app-details">
          {selectedApp && selectedAppInfo ? (
            <>
              <h2>{selectedApp}</h2>
              <div className="detail-row">
                <label>Version:</label>
                <span>v{selectedAppInfo.version}</span>
              </div>
              <div className="actions">
                <button 
                  className="btn btn-open"
                  onClick={() => handleOpenWindow(selectedApp)}
                >
                  Open Window
                </button>
                <button 
                  className="btn btn-restart"
                  onClick={() => handleRestart(selectedApp)}
                >
                  Restart
                </button>
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
