import MetricBar from './shared/MetricBar'
import BoxWrapper from './shared/BoxWrapper'

export default function MetricsBox({ meterL, meterR, lufs, peak, ...rest }) {
  const lPct = Math.round(meterL * 100)
  const rPct = Math.round(meterR * 100)

  return (
    <BoxWrapper label="Mix Analysis" color="#FFB000" icon="analytics" {...rest}>
      <div className="grid grid-cols-2 gap-1.5">
        <MetricBar label="L/R Balance"
          val={lPct > rPct ? `${lPct - rPct}% L` : rPct > lPct ? `${rPct - lPct}% R` : 'CENTER'}
          color="#DC143C" pct={Math.abs(lPct - rPct)} />
        <MetricBar label="Side Content" val={`${Math.round(Math.abs(meterL - meterR) * 150)}%`}
          color="#FFB000" pct={Math.round(Math.abs(meterL - meterR) * 150)} />
        <MetricBar label="LUFS" val={lufs > -60 ? `${lufs.toFixed(1)}` : '--'}
          color="#00E5FF" pct={Math.max(0, Math.min(100, (lufs + 60) / 60 * 100))} />
        <MetricBar label="Peak" val={peak > -60 ? `${peak.toFixed(1)} dB` : '--'}
          color="#FF6B35" pct={Math.max(0, Math.min(100, (peak + 60) / 60 * 100))} />
        <MetricBar label="Dynamic Range" val={`${(72 - (lPct + rPct) / 2 * 0.72).toFixed(1)} dB`}
          color="#00FFaa" pct={Math.max(0, 100 - (lPct + rPct) / 2)} />
        <MetricBar label="Low End" val={`${Math.round((meterL * 0.4 + 0.3) * 100)}%`}
          color="#DC143C" pct={Math.round((meterL * 0.4 + 0.3) * 100)} />
      </div>
    </BoxWrapper>
  )
}
