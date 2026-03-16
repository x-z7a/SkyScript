import React, { useState, useCallback, useRef, useEffect } from 'react';
import './App.css';

interface LogEntry {
  id: number;
  text: string;
  type: 'success' | 'error' | 'info';
}

function App() {
  const [zuluTime, setZuluTime] = useState<string>('--');
  const [latitude, setLatitude] = useState<string>('--');
  const [longitude, setLongitude] = useState<string>('--');
  const [groundspeed, setGroundspeed] = useState<string>('--');
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [customRef, setCustomRef] = useState('sim/time/zulu_time_sec');
  const [polling, setPolling] = useState(false);
  const pollTimer = useRef<ReturnType<typeof setInterval> | null>(null);
  const logId = useRef(0);
  const [bgSeconds, setBgSeconds] = useState(0);

  useEffect(() => {
    const id = setInterval(() => setBgSeconds(s => s + 1), 1000);
    return () => clearInterval(id);
  }, []);

  const addLog = useCallback((text: string, type: LogEntry['type'] = 'info') => {
    setLogs(prev => [{ id: logId.current++, text, type }, ...prev].slice(0, 50));
  }, []);

  const xplm = () => {
    if (!window.skyscript?.xplm) {
      addLog('skyscript.xplm not available', 'error');
      return null;
    }
    return window.skyscript.xplm;
  };

  const readDatarefs = useCallback(async () => {
    const api = xplm();
    if (!api) return;

    try {
      const [zulu, lat, lon, gs] = await Promise.all([
        api.getDataref('sim/time/zulu_time_sec'),
        api.getDataref('sim/flightmodel/position/latitude'),
        api.getDataref('sim/flightmodel/position/longitude'),
        api.getDataref('sim/flightmodel/position/groundspeed'),
      ]);

      setZuluTime(String(Number(zulu).toFixed(1)));
      setLatitude(String(Number(lat).toFixed(6)));
      setLongitude(String(Number(lon).toFixed(6)));
      setGroundspeed(String((Number(gs) * 1.944).toFixed(1)) + ' kts');
      addLog('Read 4 datarefs', 'success');
    } catch (e: any) {
      addLog(`Read error: ${e.message}`, 'error');
    }
  }, [addLog]);

  const togglePolling = useCallback(() => {
    if (polling) {
      if (pollTimer.current) clearInterval(pollTimer.current);
      pollTimer.current = null;
      setPolling(false);
      addLog('Stopped polling', 'info');
    } else {
      readDatarefs();
      pollTimer.current = setInterval(readDatarefs, 1000);
      setPolling(true);
      addLog('Started polling (1s interval)', 'info');
    }
  }, [polling, readDatarefs, addLog]);

  const readCustom = useCallback(async () => {
    const api = xplm();
    if (!api) return;

    try {
      const val = await api.getDataref(customRef);
      addLog(`${customRef} = ${JSON.stringify(val)}`, 'success');
    } catch (e: any) {
      addLog(`${customRef}: ${e.message}`, 'error');
    }
  }, [customRef, addLog]);

  const pauseToggle = useCallback(async () => {
    const api = xplm();
    if (!api) return;

    try {
      await api.executeCommand('sim/operation/pause_toggle');
      addLog('Executed sim/operation/pause_toggle', 'success');
    } catch (e: any) {
      addLog(`Command error: ${e.message}`, 'error');
    }
  }, [addLog]);

  return (
    <div className="hello-world">
      <h1>Hello World</h1>
      <p className="subtitle">SkyScript XPLM Bridge Test</p>
      <p className="subtitle">Background timer: {bgSeconds}s</p>

      <div className="section">
        <h2>Live Datarefs</h2>
        <div className="dataref-row">
          <label>Zulu Time</label>
          <div className="value">{zuluTime}</div>
        </div>
        <div className="dataref-row">
          <label>Latitude</label>
          <div className="value">{latitude}</div>
        </div>
        <div className="dataref-row">
          <label>Longitude</label>
          <div className="value">{longitude}</div>
        </div>
        <div className="dataref-row">
          <label>Groundspeed</label>
          <div className="value">{groundspeed}</div>
        </div>
        <div className="actions">
          <button onClick={readDatarefs}>Read Once</button>
          <button onClick={togglePolling}>
            {polling ? 'Stop Polling' : 'Start Polling'}
          </button>
        </div>
      </div>

      <div className="section">
        <h2>Custom Dataref</h2>
        <div className="input-row">
          <input
            value={customRef}
            onChange={e => setCustomRef(e.target.value)}
            placeholder="sim/time/zulu_time_sec"
            onKeyDown={e => e.key === 'Enter' && readCustom()}
          />
          <button onClick={readCustom}>Read</button>
        </div>
      </div>

      <div className="section">
        <h2>Commands</h2>
        <div className="actions">
          <button onClick={pauseToggle}>Pause Toggle</button>
        </div>
      </div>

      {logs.length > 0 && (
        <div className="section">
          <h2>Log</h2>
          <div className="log">
            {logs.map(entry => (
              <div key={entry.id} className={`log-entry ${entry.type}`}>
                {entry.text}
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}

export default App;
