import React, { useState, useEffect } from 'react';
import './App.css';

function App() {
  const [version, setVersion] = useState('--');

  useEffect(() => {
    const check = () => {
      if (window.skyscript?.version) {
        setVersion(window.skyscript.version);
      }
    };
    check();
    const id = setInterval(check, 500);
    return () => clearInterval(id);
  }, []);

  return (
    <div className="about">
      <h1>SkyScript</h1>
      <p className="version">Version {version}</p>

      <div className="description">
        <p>Standalone browser window for X-Plane.</p>
        <p>
          This software is licensed under the{' '}
          <a href="https://www.gnu.org/licenses/gpl-3.0.html">
            GNU General Public License, GPL-3.0
          </a>.
        </p>
      </div>

      <div className="links">
        <a href="https://github.com/x-z7a/skyscript">
          GitHub Repository
        </a>
      </div>

      <div className="info">
        <p>Commands and datarefs use the <code>skyscript/*</code> namespace.</p>
      </div>
    </div>
  );
}

export default App;
