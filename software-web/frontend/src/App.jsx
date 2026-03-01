import { useState, useEffect, useRef } from 'react'
import EEGChart from './components/EEGChart'

const WS_URL = 'ws://localhost:8000/ws'
const MAX_SAMPLES = 500
const UPDATE_INTERVAL = 50 // ms (20 FPS)

function App() {
  const [connected, setConnected] = useState(false)
  const [streaming, setStreaming] = useState(false)
  const [config, setConfig] = useState({ sample_rate: 250, channels: 8 })
  const [stats, setStats] = useState({ samples: 0, rate: 0 })
  const [channelData, setChannelData] = useState(() =>
    Array(8).fill(null).map(() => [])
  )
  const [selectedChannels, setSelectedChannels] = useState([0, 1, 2, 3, 4, 5, 6, 7])

  const wsRef = useRef(null)
  const dataBufferRef = useRef(Array(8).fill(null).map(() => []))
  const sampleCountRef = useRef(0)
  const lastSampleCountRef = useRef(0)
  const reconnectTimeoutRef = useRef(null)

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
        setConnected(true)
      }

      ws.onclose = () => {
        if (!mounted) return
        console.log('[WS] Disconnected')
        setConnected(false)
        setStreaming(false)
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
    }, UPDATE_INTERVAL)

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

  const toggleChannel = (ch) => {
    setSelectedChannels(prev =>
      prev.includes(ch)
        ? prev.filter(c => c !== ch)
        : [...prev, ch].sort((a, b) => a - b)
    )
  }

  return (
    <div className="app">
      <header className="header">
        <h1>Potyplex EEG</h1>
        <div className="status-bar">
          <div className={`connection-status ${connected ? 'connected' : 'disconnected'}`}>
            <span className="indicator" />
            {connected ? 'Connected' : 'Disconnected'}
          </div>
          {streaming && <div className="live-badge">LIVE</div>}
        </div>
      </header>

      <div className="controls">
        <div className="buttons">
          <button
            onClick={handleStart}
            disabled={!connected || streaming}
            className="btn btn-start"
          >
            ▶ Start
          </button>
          <button
            onClick={handleStop}
            disabled={!connected || !streaming}
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

      <div className="charts-grid">
        {selectedChannels.map(ch => (
          <EEGChart
            key={ch}
            channelIndex={ch}
            data={channelData[ch]}
            streaming={streaming}
          />
        ))}
      </div>
    </div>
  )
}

export default App
