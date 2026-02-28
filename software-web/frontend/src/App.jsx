import { useState, useEffect, useRef, useCallback } from 'react'
import EEGChart from './components/EEGChart'

const WS_URL = 'ws://localhost:8000/ws'
const API_URL = 'http://localhost:8000'
const MAX_SAMPLES = 500

function App() {
  const [connected, setConnected] = useState(false)
  const [streaming, setStreaming] = useState(false)
  const [config, setConfig] = useState({ sample_rate: 250, channels: 8 })
  const [stats, setStats] = useState({ samples: 0, packetsPerSec: 0 })
  const [channelData, setChannelData] = useState(() =>
    Array(8).fill(null).map(() => [])
  )
  const [selectedChannels, setSelectedChannels] = useState([0, 1, 2, 3, 4, 5, 6, 7])

  const wsRef = useRef(null)
  const samplesCountRef = useRef(0)
  const lastStatsTimeRef = useRef(Date.now())

  const connect = useCallback(() => {
    if (wsRef.current?.readyState === WebSocket.OPEN) return

    const ws = new WebSocket(WS_URL)

    ws.onopen = () => {
      console.log('WebSocket connected')
      setConnected(true)
    }

    ws.onclose = () => {
      console.log('WebSocket disconnected')
      setConnected(false)
      setStreaming(false)
      // Reconnect after 3 seconds
      setTimeout(connect, 3000)
    }

    ws.onerror = (error) => {
      console.error('WebSocket error:', error)
    }

    ws.onmessage = (event) => {
      const data = JSON.parse(event.data)

      if (data.type === 'config') {
        setConfig({ sample_rate: data.sample_rate, channels: data.channels })
      } else if (data.type === 'status') {
        setStreaming(data.streaming)
      } else if (data.type === 'ping') {
        // Keepalive, ignore
      } else if (data.channels) {
        // EEG data
        samplesCountRef.current++

        setChannelData(prev => {
          const newData = prev.map((ch, i) => {
            const newPoint = {
              x: samplesCountRef.current,
              y: data.channels[i]
            }
            const updated = [...ch, newPoint]
            return updated.length > MAX_SAMPLES
              ? updated.slice(-MAX_SAMPLES)
              : updated
          })
          return newData
        })

        // Update stats every second
        const now = Date.now()
        if (now - lastStatsTimeRef.current >= 1000) {
          setStats({
            samples: samplesCountRef.current,
            packetsPerSec: Math.round(
              (samplesCountRef.current - stats.samples) /
              ((now - lastStatsTimeRef.current) / 1000)
            )
          })
          lastStatsTimeRef.current = now
        }
      }
    }

    wsRef.current = ws
  }, [stats.samples])

  useEffect(() => {
    connect()
    return () => {
      if (wsRef.current) {
        wsRef.current.close()
      }
    }
  }, [connect])

  const handleStart = () => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({ action: 'start' }))
    }
  }

  const handleStop = () => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({ action: 'stop' }))
    }
  }

  const handleClear = () => {
    setChannelData(Array(8).fill(null).map(() => []))
    samplesCountRef.current = 0
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
        <div className="status">
          <span className={`indicator ${connected ? 'connected' : 'disconnected'}`} />
          {connected ? 'Connected' : 'Disconnected'}
        </div>
      </header>

      <div className="controls">
        <button
          onClick={handleStart}
          disabled={!connected || streaming}
          className="btn btn-start"
        >
          Start
        </button>
        <button
          onClick={handleStop}
          disabled={!connected || !streaming}
          className="btn btn-stop"
        >
          Stop
        </button>
        <button
          onClick={handleClear}
          className="btn btn-clear"
        >
          Clear
        </button>

        <div className="stats">
          <span>Samples: {stats.samples}</span>
          <span>Rate: {stats.packetsPerSec} Hz</span>
          <span>Sample Rate: {config.sample_rate} Hz</span>
        </div>
      </div>

      <div className="channel-selector">
        {Array(8).fill(null).map((_, i) => (
          <label key={i} className="channel-toggle">
            <input
              type="checkbox"
              checked={selectedChannels.includes(i)}
              onChange={() => toggleChannel(i)}
            />
            CH{i + 1}
          </label>
        ))}
      </div>

      <div className="charts-container">
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
