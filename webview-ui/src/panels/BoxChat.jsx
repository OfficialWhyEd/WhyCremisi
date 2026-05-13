import { useState, useCallback } from 'react'
import VectorscopeBox from './VectorscopeBox'
import StereoscopeBox from './StereoscopeBox'
import LoudnessBox from './LoudnessBox'
import ClippingBox from './ClippingBox'
import EqBox from './EqBox'
import SpectralBox from './SpectralBox'
import SliderBox from './SliderBox'
import KnobBox from './KnobBox'
import TransportBox from './TransportBox'
import CompressorBox from './CompressorBox'
import AdvisoryBox from './AdvisoryBox'
import MetricsBox from './MetricsBox'

const boxComponents = {
  vectorscope: VectorscopeBox,
  stereo: StereoscopeBox,
  loudness: LoudnessBox,
  clipping: ClippingBox,
  eq: EqBox,
  spectral: SpectralBox,
  slider: SliderBox,
  knob: KnobBox,
  transport: TransportBox,
  compressor: CompressorBox,
  advisory: AdvisoryBox,
  metrics: MetricsBox,
}

const interactiveTypes = new Set(['slider', 'knob', 'compressor', 'clipping', 'vectorscope'])

export default function BoxChat({ boxType, meterL, meterR, lufs, peak, transport, pluginStats, gainDb, driveVal, correlation, spectrum, onDawCmd, personality, onAnalyzeFurther, suggestion, layout, forceVisible, clippingCount, cpuUsage }) {
  const types = Array.isArray(boxType) ? boxType : [boxType || 'metrics']
  const [hidden, setHidden] = useState(new Set())
  const [minimized, setMinimized] = useState(new Set())
  const [interacting, setInteracting] = useState(new Set())

  const onMinimize = useCallback((type) => {
    setMinimized(prev => { const n = new Set(prev); if (n.has(type)) n.delete(type); else n.add(type); return n })
  }, [])

  const onClose = useCallback((type) => {
    setHidden(prev => { const n = new Set(prev); n.add(type); return n })
  }, [])

  const onInteract = useCallback((type, active) => {
    setInteracting(prev => { const n = new Set(prev); if (active) n.add(type); else n.delete(type); return n })
  }, [])

  const shouldShow = (type) => !hidden.has(type)
  const isMinimized = (type) => minimized.has(type)
  const isInteracting = (type) => interacting.has(type)

  const visible = types.filter(shouldShow)
  if (visible.length === 0) return null

  const containerClass = layout === 'grid' ? 'grid grid-cols-2 gap-2 mt-2' :
    layout === 'popover' ? 'fixed bottom-20 right-4 w-80 z-50 shadow-2xl bg-[#0d0d0d]/90 border border-[#222] max-h-96 overflow-y-auto custom-scrollbar' :
    ''

  const renderBox = (type) => {
    const passMinimize = { onMinimize: () => onMinimize(type), minimized: isMinimized(type) }
    const passClose = { onClose: () => onClose(type) }
    const wrapperProps = { ...passMinimize, ...passClose }

    if (type === 'advisory') return <AdvisoryBox key="advisory" suggestion={suggestion} personality={personality} onDawCmd={onDawCmd} onAnalyzeFurther={onAnalyzeFurther} />
    if (type === 'vectorscope') return <VectorscopeBox key="vec" correlation={correlation} onDawCmd={onDawCmd} {...wrapperProps} />
    if (type === 'stereo') return <StereoscopeBox key="stereo" meterL={meterL} meterR={meterR} onDawCmd={onDawCmd} {...wrapperProps} />
    if (type === 'loudness') return <LoudnessBox key="loud" lufs={lufs} peak={peak} meterL={meterL} meterR={meterR} onDawCmd={onDawCmd} {...wrapperProps} />
    if (type === 'clipping') return <ClippingBox key="clip" peak={peak} onDawCmd={onDawCmd} clippingCount={clippingCount} {...wrapperProps} />
    if (type === 'eq') return <EqBox key="eq" spectrum={spectrum} {...wrapperProps} />
    if (type === 'spectral') return <SpectralBox key="spec" spectrum={spectrum} onDawCmd={onDawCmd} {...wrapperProps} />
    if (type === 'slider') return <SliderBox key="slider" gainDb={gainDb} driveVal={driveVal} onDawCmd={onDawCmd} {...wrapperProps} />
    if (type === 'knob') return <KnobBox key="knob" meterL={meterL} meterR={meterR} {...wrapperProps} />
    if (type === 'transport') return <TransportBox key="trans" transport={transport} onDawCmd={onDawCmd} {...wrapperProps} />
    if (type === 'compressor') return <CompressorBox key="comp" correlation={correlation} onDawCmd={onDawCmd} {...wrapperProps} />
    return <MetricsBox key="metrics" meterL={meterL} meterR={meterR} lufs={lufs} peak={peak} {...wrapperProps} />
  }

  return (
    <div className={containerClass}>
      {visible.map((type, i) => {
        const interactive = interactiveTypes.has(type)
        const keepVisible = forceVisible || isInteracting(type) || interactive
        const delay = layout === 'popover' ? 0 : i * 0.05
        if (!keepVisible && i > 0 && !isInteracting(visible[i - 1])) return null
        return <div key={type} style={{ animationDelay: `${delay}s` }}>{renderBox(type)}</div>
      })}
    </div>
  )
}
