import MetricBar from './shared/MetricBar'
import BoxWrapper from './shared/BoxWrapper'

export default function EqBox({ spectrum, ...rest }) {
  const bins = (spectrum && spectrum.length > 0) ? spectrum : []
  const avgBands = (start, end) => {
    if (bins.length === 0) return 0
    const s = Math.floor(bins.length * start)
    const e = Math.floor(bins.length * end)
    if (e <= s) return 0
    let sum = 0
    for (let i = s; i < e; i++) sum += bins[i]
    return sum / (e - s)
  }
  const sub = avgBands(0, 0.05)
  const low = avgBands(0.05, 0.15)
  const lowMid = avgBands(0.15, 0.35)
  const presence = avgBands(0.35, 0.65)
  const high = avgBands(0.65, 0.85)
  const air = avgBands(0.85, 1)

  return (
    <BoxWrapper label="Frequency Analysis" color="#FFB000" icon="graphic_eq" {...rest}>
      <div className="grid grid-cols-3 gap-1.5">
        <MetricBar label="Sub (20-80Hz)" val={`${(sub * 100).toFixed(0)}%`} color="#9B59B6" pct={sub * 100} />
        <MetricBar label="Low (80-300Hz)" val={`${(low * 100).toFixed(0)}%`} color="#DC143C" pct={low * 100} />
        <MetricBar label="Low-Mid (300Hz-1k)" val={`${(lowMid * 100).toFixed(0)}%`} color="#FFB000" pct={lowMid * 100} />
        <MetricBar label="Presence (1k-5k)" val={`${(presence * 100).toFixed(0)}%`} color="#00FFaa" pct={presence * 100} />
        <MetricBar label="High (5k-15k)" val={`${(high * 100).toFixed(0)}%`} color="#00E5FF" pct={high * 100} />
        <MetricBar label="Air (15k+)" val={`${(air * 100).toFixed(0)}%`} color="#E8D5B7" pct={air * 100} />
      </div>
    </BoxWrapper>
  )
}
