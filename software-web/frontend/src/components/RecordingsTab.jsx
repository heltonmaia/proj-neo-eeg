import { useState, useEffect, useMemo } from 'react'
import { LineChart, Line, XAxis, YAxis, CartesianGrid, ResponsiveContainer, ReferenceLine } from 'recharts'

const API_URL = 'http://localhost:8000'

const CHANNEL_COLORS = [
  '#f85149', '#58a6ff', '#3fb950', '#d29922',
  '#a371f7', '#39d353', '#db61a2', '#2dba4e'
]

function RecordingsTab() {
  const [recordings, setRecordings] = useState([])
  const [selectedSession, setSelectedSession] = useState(null)
  const [sessionData, setSessionData] = useState(null)
  const [loading, setLoading] = useState(false)
  const [selectedChannels, setSelectedChannels] = useState([0, 1, 2, 3, 4, 5, 6, 7])
  const [timeWindow, setTimeWindow] = useState({ start: 0, end: 10 }) // seconds
  const [windowSize, setWindowSize] = useState(10) // seconds

  // Fetch recordings list
  useEffect(() => {
    fetchRecordings()
  }, [])

  const fetchRecordings = async () => {
    try {
      const res = await fetch(`${API_URL}/recordings`)
      if (res.ok) {
        const data = await res.json()
        setRecordings(data.recordings || [])
      }
    } catch (e) {
      console.error('Failed to fetch recordings:', e)
    }
  }

  const loadSession = async (sessionId) => {
    setLoading(true)
    setSelectedSession(sessionId)
    try {
      const res = await fetch(`${API_URL}/recordings/${sessionId}`)
      if (res.ok) {
        const data = await res.json()
        setSessionData(data)
        // Reset time window to start
        const duration = data.metadata?.duration_s ||
          (data.data?.length ? data.data[data.data.length - 1].time : 10)
        setTimeWindow({ start: 0, end: Math.min(windowSize, duration) })
      }
    } catch (e) {
      console.error('Failed to load session:', e)
    }
    setLoading(false)
  }

  const deleteSession = async (sessionId) => {
    if (!confirm(`Delete recording ${sessionId}?`)) return
    try {
      const res = await fetch(`${API_URL}/recordings/${sessionId}`, { method: 'DELETE' })
      if (res.ok) {
        fetchRecordings()
        if (selectedSession === sessionId) {
          setSelectedSession(null)
          setSessionData(null)
        }
      }
    } catch (e) {
      console.error('Failed to delete session:', e)
    }
  }

  const downloadSession = (sessionId) => {
    window.open(`${API_URL}/recordings/${sessionId}/download`, '_blank')
  }

  // Filter data for current time window
  const windowedData = useMemo(() => {
    if (!sessionData?.data) return []
    return sessionData.data.filter(
      d => d.time >= timeWindow.start && d.time <= timeWindow.end
    )
  }, [sessionData, timeWindow])

  // Prepare chart data for each channel
  const chartData = useMemo(() => {
    return selectedChannels.map(ch =>
      windowedData.map(d => ({
        x: d.time,
        y: d.channels[ch]
      }))
    )
  }, [windowedData, selectedChannels])

  const totalDuration = sessionData?.metadata?.duration_s ||
    (sessionData?.data?.length ? sessionData.data[sessionData.data.length - 1].time : 0)

  const handleScroll = (direction) => {
    const step = windowSize / 2
    setTimeWindow(prev => {
      let newStart = direction === 'left' ? prev.start - step : prev.start + step
      newStart = Math.max(0, Math.min(newStart, totalDuration - windowSize))
      return { start: newStart, end: newStart + windowSize }
    })
  }

  const handleWindowSizeChange = (size) => {
    setWindowSize(size)
    setTimeWindow(prev => ({
      start: prev.start,
      end: Math.min(prev.start + size, totalDuration)
    }))
  }

  const formatDuration = (seconds) => {
    if (!seconds) return '--'
    const mins = Math.floor(seconds / 60)
    const secs = Math.floor(seconds % 60)
    return `${mins}:${secs.toString().padStart(2, '0')}`
  }

  const formatDate = (isoString) => {
    if (!isoString) return '--'
    const date = new Date(isoString)
    return date.toLocaleString('pt-BR', {
      day: '2-digit', month: '2-digit', year: 'numeric',
      hour: '2-digit', minute: '2-digit'
    })
  }

  const toggleChannel = (ch) => {
    setSelectedChannels(prev =>
      prev.includes(ch)
        ? prev.filter(c => c !== ch)
        : [...prev, ch].sort((a, b) => a - b)
    )
  }

  return (
    <div className="recordings-tab">
      <div className="recordings-layout">
        {/* Sidebar - List of recordings */}
        <div className="recordings-sidebar">
          <div className="sidebar-header">
            <h3>Recordings</h3>
            <button className="btn-refresh" onClick={fetchRecordings}>↻</button>
          </div>
          <div className="recordings-list">
            {recordings.length === 0 ? (
              <div className="no-recordings">No recordings found</div>
            ) : (
              recordings.map(rec => (
                <div
                  key={rec.session_id}
                  className={`recording-item ${selectedSession === rec.session_id ? 'selected' : ''}`}
                  onClick={() => loadSession(rec.session_id)}
                >
                  <div className="recording-info">
                    <span className="recording-date">{formatDate(rec.start_time)}</span>
                    <span className="recording-duration">{formatDuration(rec.duration_s)}</span>
                  </div>
                  <div className="recording-meta">
                    <span className="recording-samples">{rec.total_samples?.toLocaleString() || '--'} samples</span>
                    <span className="recording-size">{rec.size_kb} KB</span>
                  </div>
                  {rec.subject && <div className="recording-subject">{rec.subject}</div>}
                  <div className="recording-actions">
                    <button onClick={(e) => { e.stopPropagation(); downloadSession(rec.session_id) }}>⬇</button>
                    <button onClick={(e) => { e.stopPropagation(); deleteSession(rec.session_id) }}>🗑</button>
                  </div>
                </div>
              ))
            )}
          </div>
        </div>

        {/* Main content - Viewer */}
        <div className="recordings-viewer">
          {!selectedSession ? (
            <div className="viewer-empty">
              <p>Select a recording to view</p>
            </div>
          ) : loading ? (
            <div className="viewer-loading">Loading...</div>
          ) : sessionData ? (
            <>
              {/* Metadata */}
              <div className="session-info">
                <div className="session-meta">
                  <span><strong>Date:</strong> {formatDate(sessionData.metadata?.start_time)}</span>
                  <span><strong>Duration:</strong> {formatDuration(sessionData.metadata?.duration_s)}</span>
                  <span><strong>Samples:</strong> {sessionData.metadata?.total_samples?.toLocaleString()}</span>
                  <span><strong>Rate:</strong> {sessionData.metadata?.sample_rate} Hz</span>
                </div>
                {sessionData.metadata?.subject && (
                  <div className="session-subject">
                    <strong>Subject:</strong> {sessionData.metadata.subject}
                  </div>
                )}
                {sessionData.metadata?.notes && (
                  <div className="session-notes">
                    <strong>Notes:</strong> {sessionData.metadata.notes}
                  </div>
                )}
              </div>

              {/* Channel selector */}
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

              {/* Time navigation */}
              <div className="time-navigation">
                <button onClick={() => handleScroll('left')} disabled={timeWindow.start <= 0}>◀</button>
                <div className="time-info">
                  <span>{timeWindow.start.toFixed(1)}s - {timeWindow.end.toFixed(1)}s</span>
                  <span className="time-total">/ {totalDuration.toFixed(1)}s</span>
                </div>
                <button onClick={() => handleScroll('right')} disabled={timeWindow.end >= totalDuration}>▶</button>
                <select value={windowSize} onChange={(e) => handleWindowSizeChange(Number(e.target.value))}>
                  <option value={5}>5s</option>
                  <option value={10}>10s</option>
                  <option value={30}>30s</option>
                  <option value={60}>60s</option>
                </select>
              </div>

              {/* Charts */}
              <div className="charts-grid">
                {selectedChannels.map((ch, idx) => (
                  <div key={ch} className="chart-wrapper">
                    <div className="chart-header">
                      <span className="channel-label" style={{ backgroundColor: CHANNEL_COLORS[ch] }}>
                        CH{ch + 1}
                      </span>
                    </div>
                    <ResponsiveContainer width="100%" height={100}>
                      <LineChart data={chartData[idx]} margin={{ top: 5, right: 10, bottom: 5, left: 0 }}>
                        <CartesianGrid strokeDasharray="3 3" stroke="#30363d" />
                        <XAxis
                          dataKey="x"
                          tickFormatter={(v) => v.toFixed(1)}
                          stroke="#484f58"
                          tick={{ fontSize: 9, fill: '#8b949e' }}
                        />
                        <YAxis
                          tickFormatter={(v) => v.toFixed(0)}
                          stroke="#484f58"
                          width={50}
                          tick={{ fontSize: 9, fill: '#8b949e' }}
                        />
                        <ReferenceLine y={0} stroke="#30363d" />
                        <Line
                          type="monotone"
                          dataKey="y"
                          stroke={CHANNEL_COLORS[ch]}
                          strokeWidth={1.2}
                          dot={false}
                          isAnimationActive={false}
                        />
                      </LineChart>
                    </ResponsiveContainer>
                  </div>
                ))}
              </div>

              {/* Markers */}
              {sessionData.metadata?.markers?.length > 0 && (
                <div className="markers-section">
                  <h4>Markers</h4>
                  <div className="markers-list">
                    {sessionData.metadata.markers.map((marker, idx) => (
                      <div key={idx} className="marker-item">
                        <span className="marker-time">{marker.time_s.toFixed(2)}s</span>
                        <span className="marker-label">{marker.label}</span>
                      </div>
                    ))}
                  </div>
                </div>
              )}
            </>
          ) : null}
        </div>
      </div>
    </div>
  )
}

export default RecordingsTab
