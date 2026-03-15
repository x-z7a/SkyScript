import React from 'react';
import './App.css';

const VERSION = '1.0.6';

function App() {
  return (
    <div className="about">
      <h1>SkyScript</h1>
      <p className="version">Version {VERSION}</p>

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
