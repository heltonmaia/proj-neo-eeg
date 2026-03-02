import { useState, useEffect, useRef } from 'react'
import EEGChart from './components/EEGChart'
import RecordingsTab from './components/RecordingsTab'
import CameraPanel from './components/CameraPanel'

const WS_URL = 'ws://localhost:8000/ws'
const API_URL = 'http://localhost:8000'
const MAX_SAMPLES = 500
const UPDATE_INTERVAL = 50 // ms (20 FPS)
const LOG_POLL_INTERVAL = 2000 // ms

function App() {
  const [theme, setTheme] = useState(() => localStorage.getItem('theme') || 'dark')
  const [activeTab, setActiveTab] = useState('live')
  const [wsConnected, setWsConnected] = useState(false)
  const [streaming, setStreaming] = useState(false)
  const [receiving, setReceiving] = useState(false)
  const [recording, setRecording] = useState(false)
  const [recordingTime, setRecordingTime] = useState(0)
  const [config, setConfig] = useState({ sample_rate: 250, channels: 8 })
  const [stats, setStats] = useState({ samples: 0, rate: 0 })
  const [autoZoom, setAutoZoom] = useState(() =>
    Array(8).fill(true)  // Auto-zoom enabled by default
  )
  const [zoomLevel, setZoomLevel] = useState(() =>
    Array(8).fill(1)  // 1 = 100%, 2 = 50%, 0.5 = 200%
  )
  const [channelData, setChannelData] = useState(() =>
    Array(8).fill(null).map(() => [])
  )
  const [selectedChannels, setSelectedChannels] = useState([0, 1, 2, 3, 4, 5, 6, 7])
  const [systemLogs, setSystemLogs] = useState([])
  const [serverStats, setServerStats] = useState(null)
  const [statusExpanded, setStatusExpanded] = useState(false)
  const [cameraActive, setCameraActive] = useState(false)
  const [cameraRecording, setCameraRecording] = useState(false)
  const [recordSignals, setRecordSignals] = useState(true)
  const [recordVideo, setRecordVideo] = useState(true)

  const wsRef = useRef(null)
  const recordingTimerRef = useRef(null)
  const dataBufferRef = useRef(Array(8).fill(null).map(() => []))
  const sampleCountRef = useRef(0)
  const lastSampleCountRef = useRef(0)
  const reconnectTimeoutRef = useRef(null)

  // Apply theme
  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme)
    localStorage.setItem('theme', theme)
  }, [theme])

  const toggleTheme = () => {
    setTheme(prev => prev === 'dark' ? 'light' : 'dark')
  }

  // Connect to WebSocket
  useEffect(() => {
    let mounted = true

    const connect = () => {
      if (!mounted) return
      if (wsRef.current?.readyState === WebSocket.OPEN) return

      console.log('[WS] Connecting...')
      const ws = new WebSocket(WS_URL)

      ws.onopen = () => {
        if (!mounted) return
        console.log('[WS] Connected')
        setWsConnected(true)
      }

      ws.onclose = () => {
        if (!mounted) return
        console.log('[WS] Disconnected')
        setWsConnected(false)
        setStreaming(false)
        setReceiving(false)
        wsRef.current = null

        // Reconnect after 2 seconds
        if (reconnectTimeoutRef.current) {
          clearTimeout(reconnectTimeoutRef.current)
        }
        reconnectTimeoutRef.current = setTimeout(connect, 2000)
      }

      ws.onerror = (error) => {
        console.error('[WS] Error:', error)
      }

      ws.onmessage = (event) => {
        const data = JSON.parse(event.data)

        if (data.type === 'config') {
          setConfig({ sample_rate: data.sample_rate, channels: data.channels })
        } else if (data.type === 'status') {
          setStreaming(data.streaming)
        } else if (data.type === 'ping') {
          // Keepalive - respond with pong
          if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'pong' }))
          }
        } else if (data.type === 'batch' && data.samples) {
          // Batch of EEG samples
          for (const sample of data.samples) {
            sampleCountRef.current++
            for (let i = 0; i < 8; i++) {
              dataBufferRef.current[i].push({
                x: sampleCountRef.current,
                y: sample.c[i]  // 'c' = channels (shortened key)
              })
            }
          }
          // Trim buffers
          for (let i = 0; i < 8; i++) {
            if (dataBufferRef.current[i].length > MAX_SAMPLES * 2) {
              dataBufferRef.current[i] = dataBufferRef.current[i].slice(-MAX_SAMPLES)
            }
          }
        }
      }

      wsRef.current = ws
    }

    connect()

    return () => {
      mounted = false
      if (reconnectTimeoutRef.current) {
        clearTimeout(reconnectTimeoutRef.current)
      }
      if (wsRef.current) {
        wsRef.current.close()
        wsRef.current = null
      }
    }
  }, [])

  // Update UI at fixed interval (not on every sample)
  useEffect(() => {
    const interval = setInterval(() => {
      // Update channel data from buffer
      setChannelData(
        dataBufferRef.current.map(ch => ch.slice(-MAX_SAMPLES))
      )

      // Update stats
      const currentCount = sampleCountRef.current
      const rate = Math.round((currentCount - lastSampleCountRef.current) * (1000 / UPDATE_INTERVAL))
      lastSampleCountRef.current = currentCount

      setStats({
        samples: currentCount,
        rate: rate
      })

      // Update receiving status (connected to ESP32 = receiving data)
      setReceiving(rate > 0)
    }, UPDATE_INTERVAL)

    return () => clearInterval(interval)
  }, [])

  // Poll server stats and logs
  useEffect(() => {
    const fetchStatus = async () => {
      try {
        const [statsRes, logsRes] = await Promise.all([
          fetch(`${API_URL}/stats`),
          fetch(`${API_URL}/logs`)
        ])
        if (statsRes.ok) {
          setServerStats(await statsRes.json())
        }
        if (logsRes.ok) {
          const data = await logsRes.json()
          setSystemLogs(data.logs || [])
        }
      } catch (e) {
        // Server might be down
      }
    }

    fetchStatus()
    const interval = setInterval(fetchStatus, LOG_POLL_INTERVAL)
    return () => clearInterval(interval)
  }, [])

  const handleStart = () => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({ action: 'start' }))
      setStreaming(true)
    }
  }

  const handleStop = () => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({ action: 'stop' }))
      setStreaming(false)
    }
  }

  const handleClear = () => {
    dataBufferRef.current = Array(8).fill(null).map(() => [])
    setChannelData(Array(8).fill(null).map(() => []))
    sampleCountRef.current = 0
    lastSampleCountRef.current = 0
    setStats({ samples: 0, rate: 0 })
  }

  const handleStartRecording = async () => {
    if (!recordSignals && !recordVideo) return
    try {
      const params = new URLSearchParams({
        include_video: recordVideo && cameraActive ? 'true' : 'false',
        channels: selectedChannels.join(',')
      })
      const res = await fetch(`${API_URL}/record/start?${params}`, { method: 'POST' })
      if (res.ok) {
        setRecording(true)
        setRecordingTime(0)
        recordingTimerRef.current = setInterval(() => {
          setRecordingTime(prev => prev + 1)
        }, 1000)
      }
    } catch (e) {
      console.error('Failed to start recording:', e)
    }
  }

  const handleStopRecording = async () => {
    try {
      const res = await fetch(`${API_URL}/record/stop`, { method: 'POST' })
      if (res.ok) {
        setRecording(false)
        if (recordingTimerRef.current) {
          clearInterval(recordingTimerRef.current)
          recordingTimerRef.current = null
        }
      }
    } catch (e) {
      console.error('Failed to stop recording:', e)
    }
  }

  const formatRecordingTime = (seconds) => {
    const mins = Math.floor(seconds / 60)
    const secs = seconds % 60
    return `${mins}:${secs.toString().padStart(2, '0')}`
  }

  const toggleChannel = (ch) => {
    setSelectedChannels(prev =>
      prev.includes(ch)
        ? prev.filter(c => c !== ch)
        : [...prev, ch].sort((a, b) => a - b)
    )
  }

  const toggleAutoZoom = (ch) => {
    setAutoZoom(prev => {
      const newState = [...prev]
      newState[ch] = !newState[ch]
      return newState
    })
  }

  const zoomIn = (ch) => {
    setZoomLevel(prev => {
      const newState = [...prev]
      newState[ch] = Math.min(prev[ch] * 1.5, 10)  // Max 10x zoom
      return newState
    })
    setAutoZoom(prev => {
      const newState = [...prev]
      newState[ch] = false  // Disable auto-zoom when manually zooming
      return newState
    })
  }

  const zoomOut = (ch) => {
    setZoomLevel(prev => {
      const newState = [...prev]
      newState[ch] = Math.max(prev[ch] / 1.5, 0.1)  // Min 0.1x zoom
      return newState
    })
    setAutoZoom(prev => {
      const newState = [...prev]
      newState[ch] = false
      return newState
    })
  }

  const resetZoom = (ch) => {
    setZoomLevel(prev => {
      const newState = [...prev]
      newState[ch] = 1
      return newState
    })
    setAutoZoom(prev => {
      const newState = [...prev]
      newState[ch] = true
      return newState
    })
  }

  const formatLogTime = (isoString) => {
    const date = new Date(isoString)
    return date.toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit', second: '2-digit' })
  }

  const handleCameraStatusChange = (active, recording) => {
    setCameraActive(active)
    setCameraRecording(recording)
  }

  return (
    <div className="app">
      <header className="header">
        <h1>Potyplex EEG <span className="app-variant">(web)</span></h1>
        <div className="status-bar">
          <div className={`connection-status ${receiving ? 'connected' : wsConnected ? 'waiting' : 'disconnected'}`}>
            <span className="indicator" />
            {receiving ? 'ESP32 Connected' : wsConnected ? 'Waiting ESP32' : 'Disconnected'}
          </div>
          {streaming && receiving && <div className="live-badge">LIVE</div>}
          {recording && <div className="rec-badge">REC {formatRecordingTime(recordingTime)}</div>}
          <button className="theme-toggle" onClick={toggleTheme} title="Toggle theme">
            {theme === 'dark' ? '☀️' : '🌙'}
          </button>
        </div>
      </header>

      {/* Tab navigation */}
      <div className="tabs">
        <button
          className={`tab ${activeTab === 'live' ? 'active' : ''}`}
          onClick={() => setActiveTab('live')}
        >
          Live
        </button>
        <button
          className={`tab ${activeTab === 'recordings' ? 'active' : ''}`}
          onClick={() => setActiveTab('recordings')}
        >
          Recordings
        </button>
      </div>

      {activeTab === 'live' ? (
        <>
          <div className="controls">
            <div className="buttons">
              <button
                onClick={handleStart}
                disabled={!wsConnected || streaming}
                className="btn btn-start"
              >
                ▶ Start
              </button>
              <button
                onClick={handleStop}
                disabled={!wsConnected || !streaming}
                className="btn btn-stop"
              >
                ⬛ Stop
              </button>
              <button
                onClick={handleClear}
                className="btn btn-clear"
              >
                ✕ Clear
              </button>
            </div>

            <div className="recording-controls">
              <div className="recording-options">
                <label className={`rec-option ${recording ? 'disabled' : ''}`}>
                  <input
                    type="checkbox"
                    checked={recordSignals}
                    onChange={(e) => setRecordSignals(e.target.checked)}
                    disabled={recording}
                  />
                  <span>Signals</span>
                </label>
                <label className={`rec-option ${recording ? 'disabled' : ''} ${!cameraActive ? 'unavailable' : ''}`}>
                  <input
                    type="checkbox"
                    checked={recordVideo}
                    onChange={(e) => setRecordVideo(e.target.checked)}
                    disabled={recording}
                  />
                  <span>Video {!cameraActive && '(off)'}</span>
                </label>
              </div>
              {!recording ? (
                <button
                  onClick={handleStartRecording}
                  disabled={!receiving || (!recordSignals && !recordVideo)}
                  className="btn btn-rec"
                >
                  ● REC
                </button>
              ) : (
                <button
                  onClick={handleStopRecording}
                  className="btn btn-rec recording"
                >
                  ⬛ STOP ({formatRecordingTime(recordingTime)})
                </button>
              )}
            </div>
          </div>

          {/* Main content area with two columns */}
          <div className="live-content">
            {/* Left column: EEG Charts */}
            <div className="eeg-column">
              <div className="charts-grid">
                {selectedChannels.map(ch => (
                  <EEGChart
                    key={ch}
                    channelIndex={ch}
                    data={channelData[ch]}
                    streaming={streaming}
                    autoZoom={autoZoom[ch]}
                    zoomLevel={zoomLevel[ch]}
                    onToggleAutoZoom={() => toggleAutoZoom(ch)}
                    onZoomIn={() => zoomIn(ch)}
                    onZoomOut={() => zoomOut(ch)}
                    onResetZoom={() => resetZoom(ch)}
                  />
                ))}
              </div>
            </div>

            {/* Right column: Camera + Quick Info */}
            <div className="side-column">
              {/* Camera Panel */}
              <CameraPanel onCameraStatusChange={handleCameraStatusChange} />

              {/* Quick Info Panel (always visible, compact) */}
              <div className="quick-info-panel">
                <div className="quick-stats">
                  <div className="quick-stat">
                    <span className="stat-value">{stats.samples.toLocaleString()}</span>
                    <span className="stat-label">Samples</span>
                  </div>
                  <div className="quick-stat">
                    <span className="stat-value">{stats.rate} Hz</span>
                    <span className="stat-label">Rate</span>
                  </div>
                  <div className="quick-stat">
                    <span className="stat-value">{config.sample_rate} Hz</span>
                    <span className="stat-label">Target</span>
                  </div>
                </div>
                <div className="quick-indicators">
                  <div className={`quick-indicator ${wsConnected ? 'ok' : 'error'}`}>
                    <span className="dot"></span>
                    <span>Backend</span>
                  </div>
                  <div className={`quick-indicator ${receiving ? 'ok' : streaming ? 'warning' : 'off'}`}>
                    <span className="dot"></span>
                    <span>ESP32</span>
                  </div>
                  <div className={`quick-indicator ${cameraActive ? 'ok' : 'off'}`}>
                    <span className="dot"></span>
                    <span>Camera</span>
                  </div>
                </div>
              </div>

              {/* Channel selector in side panel */}
              <div className="side-channel-selector">
                <div className="side-channel-header">Channels</div>
                <div className={`side-channel-grid ${recording ? 'locked' : ''}`}>
                  {Array(8).fill(null).map((_, i) => (
                    <label key={i} className={`side-channel-toggle ${selectedChannels.includes(i) ? 'active' : ''} ${recording ? 'disabled' : ''}`}>
                      <input
                        type="checkbox"
                        checked={selectedChannels.includes(i)}
                        onChange={() => toggleChannel(i)}
                        disabled={recording}
                      />
                      <span>CH{i + 1}</span>
                    </label>
                  ))}
                </div>
                {recording && <span className="locked-indicator">🔒 Locked during recording</span>}
              </div>
            </div>
          </div>

          {/* System Console Panel (expandable, full width, below everything) */}
          <div className="system-console">
            <div
              className="console-header"
              onClick={() => setStatusExpanded(!statusExpanded)}
            >
              <h3>System Console</h3>
              <span className={`toggle-icon ${statusExpanded ? 'expanded' : ''}`}>
                {statusExpanded ? '▲' : '▼'}
              </span>
            </div>
            <div className={`console-content ${statusExpanded ? 'expanded' : ''}`}>
              <div className="console-columns">
                {/* Left column: Connection Status & Details */}
                <div className="console-column">
                  <h4>Connection Status</h4>
                  <div className="console-section">
                    <div className="connection-detail">
                      <span className={`dot ${wsConnected ? 'green' : 'red'}`}></span>
                      <span className="conn-label">Backend Server</span>
                      <span className="conn-value">{wsConnected ? 'ws://localhost:8000' : 'Disconnected'}</span>
                    </div>
                    <div className="connection-detail">
                      <span className={`dot ${receiving ? 'green' : streaming ? 'yellow' : 'gray'}`}></span>
                      <span className="conn-label">ESP32 Device</span>
                      <span className="conn-value">{receiving ? '192.168.4.1:12345' : streaming ? 'Waiting...' : 'Not streaming'}</span>
                    </div>
                    <div className="connection-detail">
                      <span className={`dot ${cameraActive ? 'green' : 'gray'}`}></span>
                      <span className="conn-label">Camera</span>
                      <span className="conn-value">{cameraActive ? (cameraRecording ? 'Recording' : 'Active') : 'Inactive'}</span>
                    </div>
                  </div>

                  <h4>Statistics</h4>
                  <div className="console-section stats-grid">
                    <div className="stat-item">
                      <span className="stat-name">Total Samples</span>
                      <span className="stat-val">{stats.samples.toLocaleString()}</span>
                    </div>
                    <div className="stat-item">
                      <span className="stat-name">Sample Rate</span>
                      <span className="stat-val">{stats.rate} / {config.sample_rate} Hz</span>
                    </div>
                    <div className="stat-item">
                      <span className="stat-name">Active Channels</span>
                      <span className="stat-val">{selectedChannels.length} / 8</span>
                    </div>
                    <div className="stat-item">
                      <span className="stat-name">Recording</span>
                      <span className="stat-val">{recording ? `Active (${formatRecordingTime(recordingTime)})` : 'Stopped'}</span>
                    </div>
                  </div>
                </div>

                {/* Right column: Event Log */}
                <div className="console-column">
                  <h4>Event Log</h4>
                  <div className="console-log">
                    {systemLogs.length === 0 ? (
                      <div className="log-empty">No events recorded</div>
                    ) : (
                      [...systemLogs].reverse().slice(0, 30).map((log, idx) => (
                        <div key={idx} className={`log-entry ${log.level}`}>
                          <span className="log-time">{formatLogTime(log.time)}</span>
                          <span className="log-event">{log.event}</span>
                        </div>
                      ))
                    )}
                  </div>
                </div>
              </div>
            </div>
          </div>
        </>
      ) : (
        <RecordingsTab />
      )}
    </div>
  )
}

export default App
