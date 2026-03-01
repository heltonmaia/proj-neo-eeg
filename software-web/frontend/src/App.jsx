import { useState, useEffect, useRef } from 'react'
import EEGChart from './components/EEGChart'
import RecordingsTab from './components/RecordingsTab'

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
    try {
      const res = await fetch(`${API_URL}/record/start`, { method: 'POST' })
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

  return (
    <div className="app">
      <header className="header">
        <h1>Potyplex EEG</h1>
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
              <div className="btn-separator" />
              {!recording ? (
                <button
                  onClick={handleStartRecording}
                  disabled={!receiving}
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

            <div className="stats">
              <span className="stat">
                <label>Samples</label>
                <value>{stats.samples.toLocaleString()}</value>
              </span>
              <span className="stat">
                <label>Rate</label>
                <value>{stats.rate} Hz</value>
              </span>
              <span className="stat">
                <label>Target</label>
                <value>{config.sample_rate} Hz</value>
              </span>
            </div>
          </div>

          <div className="channel-selector">
            {Array(8).fill(null).map((_, i) => (
              <label key={i} className={`channel-toggle ${selectedChannels.includes(i) ? 'active' : ''}`}>
                <input
                  type="checkbox"
                  checked={selectedChannels.includes(i)}
                  onChange={() => toggleChannel(i)}
                />
                CH{i + 1}
              </label>
            ))}
          </div>

          {/* System Status Panel */}
          <div className="status-panel">
            <div
              className="status-panel-header"
              onClick={() => setStatusExpanded(!statusExpanded)}
            >
              <h3>
                <span>System Status</span>
              </h3>
              <span className={`toggle-icon ${statusExpanded ? 'expanded' : ''}`}>
                {statusExpanded ? '▲' : '▼'}
              </span>
            </div>
            <div className={`status-panel-content ${statusExpanded ? 'expanded' : ''}`}>
              <div className="status-indicators">
                <div className="status-indicator">
                  <span className={`dot ${wsConnected ? 'green' : 'red'}`}></span>
                  <span className="label">Backend:</span>
                  <span className="value">{wsConnected ? 'Connected' : 'Disconnected'}</span>
                </div>
                <div className="status-indicator">
                  <span className={`dot ${receiving ? 'green' : streaming ? 'yellow' : 'gray'}`}></span>
                  <span className="label">ESP32:</span>
                  <span className="value">{receiving ? 'Receiving' : streaming ? 'Waiting' : 'Stopped'}</span>
                </div>
                {serverStats && (
                  <div className="status-indicator">
                    <span className="label">Samples:</span>
                    <span className="value">{serverStats.samples_received?.toLocaleString()}</span>
                  </div>
                )}
              </div>
              <div className="log-entries">
                {systemLogs.length === 0 ? (
                  <div className="log-empty">No events recorded</div>
                ) : (
                  [...systemLogs].reverse().slice(0, 20).map((log, idx) => (
                    <div key={idx} className={`log-entry ${log.level}`}>
                      <span className="log-time">{formatLogTime(log.time)}</span>
                      <span className="log-event">{log.event}</span>
                    </div>
                  ))
                )}
              </div>
            </div>
          </div>

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
        </>
      ) : (
        <RecordingsTab />
      )}
    </div>
  )
}

export default App
