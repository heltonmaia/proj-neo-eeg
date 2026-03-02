import { useState, useEffect, useRef, useCallback } from 'react'

const API_URL = 'http://localhost:8000'
const WS_CAMERA_URL = 'ws://localhost:8000/ws/camera'

function CameraPanel({ onCameraStatusChange }) {
  const [cameras, setCameras] = useState([])
  const [selectedCamera, setSelectedCamera] = useState(0)
  const [selectedResolution, setSelectedResolution] = useState('640x480')
  const [cameraActive, setCameraActive] = useState(false)
  const [cameraRecording, setCameraRecording] = useState(false)
  const [connected, setConnected] = useState(false)
  const [fps, setFps] = useState(0)
  const [error, setError] = useState(null)

  const canvasRef = useRef(null)
  const wsRef = useRef(null)
  const frameCountRef = useRef(0)
  const lastFpsTimeRef = useRef(Date.now())

  // Fetch available cameras
  const fetchCameras = useCallback(async () => {
    try {
      const res = await fetch(`${API_URL}/cameras`)
      if (res.ok) {
        const data = await res.json()
        setCameras(data.cameras || [])
        setCameraActive(data.active)
        setCameraRecording(data.recording)
        if (data.cameras?.length > 0 && data.cameras[0].supported_resolutions?.length > 0) {
          setSelectedResolution(data.cameras[0].current_resolution || '640x480')
        }
      }
    } catch (e) {
      setError('Failed to fetch cameras')
    }
  }, [])

  useEffect(() => {
    fetchCameras()
  }, [fetchCameras])

  // Store callback in ref to avoid reconnection on prop change
  const onCameraStatusChangeRef = useRef(onCameraStatusChange)
  useEffect(() => {
    onCameraStatusChangeRef.current = onCameraStatusChange
  }, [onCameraStatusChange])

  // Connect to camera WebSocket
  useEffect(() => {
    let mounted = true
    let reconnectTimeout = null

    const connect = () => {
      if (!mounted) return

      const ws = new WebSocket(WS_CAMERA_URL)

      ws.onopen = () => {
        if (!mounted) return
        setConnected(true)
        setError(null)
      }

      ws.onclose = () => {
        if (!mounted) return
        setConnected(false)
        wsRef.current = null
        // Reconnect after 2 seconds
        reconnectTimeout = setTimeout(connect, 2000)
      }

      ws.onerror = () => {
        setError('WebSocket connection error')
      }

      ws.onmessage = (event) => {
        const data = JSON.parse(event.data)

        if (data.type === 'camera_status') {
          setCameraActive(data.active)
          setCameraRecording(data.recording)
          onCameraStatusChangeRef.current?.(data.active, data.recording)
        } else if (data.type === 'camera_frame' && canvasRef.current) {
          // Decode and draw frame
          const img = new Image()
          img.onload = () => {
            const canvas = canvasRef.current
            if (canvas) {
              const ctx = canvas.getContext('2d')
              // Resize canvas to match image aspect ratio
              canvas.width = img.width
              canvas.height = img.height
              ctx.drawImage(img, 0, 0)

              // Calculate FPS
              frameCountRef.current++
              const now = Date.now()
              if (now - lastFpsTimeRef.current >= 1000) {
                setFps(frameCountRef.current)
                frameCountRef.current = 0
                lastFpsTimeRef.current = now
              }
            }
          }
          img.src = `data:image/jpeg;base64,${data.frame}`
        }
      }

      wsRef.current = ws
    }

    connect()

    return () => {
      mounted = false
      if (reconnectTimeout) clearTimeout(reconnectTimeout)
      if (wsRef.current) {
        wsRef.current.close()
        wsRef.current = null
      }
    }
  }, []) // Empty deps - connect once on mount

  const startCamera = async () => {
    const [width, height] = selectedResolution.split('x').map(Number)
    try {
      const res = await fetch(`${API_URL}/camera/start?camera_index=${selectedCamera}&width=${width}&height=${height}`, {
        method: 'POST'
      })
      if (res.ok) {
        setCameraActive(true)
        setError(null)
        onCameraStatusChangeRef.current?.(true, false)
      } else {
        setError('Failed to start camera')
      }
    } catch (e) {
      setError('Failed to start camera')
    }
  }

  const stopCamera = async () => {
    try {
      await fetch(`${API_URL}/camera/stop`, { method: 'POST' })
      setCameraActive(false)
      setCameraRecording(false)
      onCameraStatusChangeRef.current?.(false, false)
    } catch (e) {
      setError('Failed to stop camera')
    }
  }

  const currentCamera = cameras.find(c => c.index === selectedCamera)

  return (
    <div className="camera-panel">
      <div className="camera-header">
        <h3>Camera</h3>
        <div className="camera-status">
          <span className={`dot ${connected ? (cameraActive ? 'green' : 'yellow') : 'red'}`}></span>
          <span>{cameraActive ? 'Active' : connected ? 'Ready' : 'Disconnected'}</span>
          {cameraActive && <span className="camera-fps">{fps} FPS</span>}
          {cameraRecording && <span className="rec-indicator">REC</span>}
        </div>
      </div>

      <div className="camera-preview">
        {cameraActive ? (
          <canvas ref={canvasRef} className="camera-canvas" />
        ) : (
          <div className="camera-placeholder">
            <span>No camera feed</span>
            {cameras.length === 0 && <span className="no-cameras">No cameras detected</span>}
          </div>
        )}
      </div>

      <div className="camera-controls">
        <div className="camera-select-group">
          <select
            value={selectedCamera}
            onChange={(e) => setSelectedCamera(Number(e.target.value))}
            disabled={cameraActive}
          >
            {cameras.length === 0 ? (
              <option value={0}>No cameras</option>
            ) : (
              cameras.map(cam => (
                <option key={cam.index} value={cam.index}>
                  {cam.name}
                </option>
              ))
            )}
          </select>

          <select
            value={selectedResolution}
            onChange={(e) => setSelectedResolution(e.target.value)}
            disabled={cameraActive}
          >
            {currentCamera?.supported_resolutions?.map(res => (
              <option key={res} value={res}>{res}</option>
            )) || (
              <>
                <option value="640x480">640x480</option>
                <option value="1280x720">1280x720</option>
              </>
            )}
          </select>
        </div>

        <div className="camera-buttons">
          {!cameraActive ? (
            <button
              onClick={startCamera}
              disabled={cameras.length === 0}
              className="btn btn-camera-start"
            >
              Start Camera
            </button>
          ) : (
            <button
              onClick={stopCamera}
              className="btn btn-camera-stop"
            >
              Stop Camera
            </button>
          )}
          <button
            onClick={fetchCameras}
            className="btn btn-refresh-cameras"
            title="Refresh camera list"
          >
            ↻
          </button>
        </div>
      </div>

      {error && <div className="camera-error">{error}</div>}
    </div>
  )
}

export default CameraPanel
