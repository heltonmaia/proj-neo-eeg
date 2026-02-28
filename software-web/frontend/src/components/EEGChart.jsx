import { LineChart, Line, XAxis, YAxis, CartesianGrid, ResponsiveContainer, ReferenceLine } from 'recharts'

const CHANNEL_COLORS = [
  '#e74c3c', // CH1 - Red
  '#3498db', // CH2 - Blue
  '#2ecc71', // CH3 - Green
  '#f39c12', // CH4 - Orange
  '#9b59b6', // CH5 - Purple
  '#1abc9c', // CH6 - Teal
  '#e91e63', // CH7 - Pink
  '#00bcd4', // CH8 - Cyan
]

function EEGChart({ channelIndex, data, streaming }) {
  const color = CHANNEL_COLORS[channelIndex]

  return (
    <div className="chart-wrapper">
      <div className="chart-header">
        <span
          className="channel-label"
          style={{ backgroundColor: color }}
        >
          CH{channelIndex + 1}
        </span>
        {streaming && <span className="streaming-badge">LIVE</span>}
        {data.length > 0 && (
          <span className="current-value">
            {data[data.length - 1]?.y.toFixed(2)} µV
          </span>
        )}
      </div>

      <ResponsiveContainer width="100%" height={120}>
        <LineChart data={data} margin={{ top: 5, right: 20, bottom: 5, left: 0 }}>
          <CartesianGrid strokeDasharray="3 3" stroke="#333" />
          <XAxis
            dataKey="x"
            tick={false}
            stroke="#666"
          />
          <YAxis
            domain={['auto', 'auto']}
            tickFormatter={(v) => v.toFixed(0)}
            stroke="#666"
            width={60}
            tick={{ fontSize: 10 }}
          />
          <ReferenceLine y={0} stroke="#444" />
          <Line
            type="monotone"
            dataKey="y"
            stroke={color}
            strokeWidth={1.5}
            dot={false}
            isAnimationActive={false}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  )
}

export default EEGChart
