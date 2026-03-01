import { memo } from 'react'
import { LineChart, Line, XAxis, YAxis, CartesianGrid, ResponsiveContainer, ReferenceLine } from 'recharts'

const CHANNEL_COLORS = [
  '#f85149', // CH1 - Red
  '#58a6ff', // CH2 - Blue
  '#3fb950', // CH3 - Green
  '#d29922', // CH4 - Orange
  '#a371f7', // CH5 - Purple
  '#39d353', // CH6 - Teal
  '#db61a2', // CH7 - Pink
  '#2dba4e', // CH8 - Cyan
]

const EEGChart = memo(function EEGChart({ channelIndex, data }) {
  const color = CHANNEL_COLORS[channelIndex]
  const lastValue = data.length > 0 ? data[data.length - 1]?.y : null

  return (
    <div className="chart-wrapper">
      <div className="chart-header">
        <span className="channel-label" style={{ backgroundColor: color }}>
          CH{channelIndex + 1}
        </span>
        {lastValue !== null && (
          <span className="current-value">
            {lastValue.toFixed(2)} µV
          </span>
        )}
      </div>

      <ResponsiveContainer width="100%" height={100}>
        <LineChart data={data} margin={{ top: 5, right: 10, bottom: 5, left: 0 }}>
          <CartesianGrid strokeDasharray="3 3" stroke="#30363d" />
          <XAxis dataKey="x" hide />
          <YAxis
            domain={['auto', 'auto']}
            tickFormatter={(v) => v.toFixed(0)}
            stroke="#484f58"
            width={50}
            tick={{ fontSize: 9, fill: '#8b949e' }}
          />
          <ReferenceLine y={0} stroke="#30363d" />
          <Line
            type="monotone"
            dataKey="y"
            stroke={color}
            strokeWidth={1.2}
            dot={false}
            isAnimationActive={false}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  )
})

export default EEGChart
