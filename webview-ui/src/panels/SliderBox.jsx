import { useState, useEffect } from 'react'
import { motion } from 'framer-motion'
import BoxWrapper from './shared/BoxWrapper'

export default function SliderBox({ gainDb, driveVal, onDawCmd, ...rest }) {
  const [localGain, setLocalGain] = useState(gainDb)
  const [localDrive, setLocalDrive] = useState(driveVal)
  useEffect(() => { setLocalGain(gainDb) }, [gainDb])
  useEffect(() => { setLocalDrive(driveVal) }, [driveVal])

  const sliders = [
    { label: 'Master Gain', val: localGain, setVal: setLocalGain, min: -60, max: 12, color: '#DC143C', cmd: 'setGain', key: 'valueDb' },
    { label: 'Drive', val: localDrive, setVal: setLocalDrive, min: 0, max: 100, color: '#FFB000', cmd: 'setDrive', key: 'value' },
  ]

  return (
    <BoxWrapper label="Volume / Gain Control" color="#FFB000" icon="tune" {...rest}>
      <div className="space-y-2">
        {sliders.map(({ label, val, setVal, min, max, color, cmd, key }) => {
          const pct = ((val - min) / (max - min)) * 100
          return (
            <div key={label} className="space-y-1">
              <div className="flex justify-between text-xs uppercase font-bold">
                <span className="text-[#aaa]">{label}</span>
                <span style={{ color }}>{val.toFixed(1)}</span>
              </div>
              <div className="h-2 bg-[#1a1a1a] w-full relative cursor-pointer rounded-sm overflow-hidden"
                onMouseDown={(e) => {
                  const rect = e.currentTarget.getBoundingClientRect()
                  const pct = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width))
                  const newVal = min + pct * (max - min)
                  setVal(newVal)
                  onDawCmd(cmd, { [key]: newVal })
                  const onMove = (ev) => {
                    const p = Math.max(0, Math.min(1, (ev.clientX - rect.left) / rect.width))
                    const v = min + p * (max - min)
                    setVal(v)
                    onDawCmd(cmd, { [key]: v })
                  }
                  const onUp = () => { document.removeEventListener('mousemove', onMove); document.removeEventListener('mouseup', onUp) }
                  document.addEventListener('mousemove', onMove)
                  document.addEventListener('mouseup', onUp)
                }}
              >
                <motion.div className="h-full absolute left-0 top-0"
                  style={{ backgroundColor: color, width: `${pct}%`, boxShadow: `0 0 6px ${color}50` }} />
              </div>
            </div>
          )
        })}
      </div>
    </BoxWrapper>
  )
}
