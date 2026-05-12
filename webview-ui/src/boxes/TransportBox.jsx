import { motion } from 'framer-motion'
import BoxWrapper from './shared/BoxWrapper'

export default function TransportBox({ transport, onDawCmd, ...rest }) {
  const fields = [
    { label: 'Status', val: transport.isPlaying ? 'PLAY' : 'STOP', color: transport.isPlaying ? '#00FFaa' : '#4d4d4d' },
    { label: 'BPM', val: transport.bpm.toFixed(1), color: '#FFB000' },
    { label: 'Position', val: `${Math.floor(transport.position / 60).toString().padStart(2, '0')}:${(transport.position % 60).toFixed(0).toString().padStart(2, '0')}`, color: '#4d4d4d' },
    { label: 'Recording', val: transport.isRecording ? 'REC' : 'OFF', color: transport.isRecording ? '#DC143C' : '#4d4d4d' },
  ]

  return (
    <BoxWrapper label="Transport Status" color="#00FFaa" icon="play_arrow" {...rest}>
      <div className="grid grid-cols-4 gap-1.5">
        {fields.map(({ label, val, color }) => (
          <div key={label} className="bg-[#111] border border-[#1a1a1a] p-1.5 text-center">
            <div className="text-xs text-[#666] uppercase mb-0.5">{label}</div>
            <div className="text-xs font-bold font-mono" style={{ color }}>{val}</div>
          </div>
        ))}
        <div className="col-span-4 flex gap-1.5 mt-1">
          {[['play', 'PLAY', '#00FFaa'], ['stop', 'STOP', '#4d4d4d'], ['record', 'REC', '#DC143C']].map(([cmd, lbl, clr]) => (
            <motion.button key={cmd} whileHover={{ scale: 1.04 }} whileTap={{ scale: 0.95 }}
              className="flex-1 py-1 text-xs font-bold uppercase border border-[#222] bg-[#111] tracking-widest"
              style={{ color: clr, borderColor: clr + '40' }}
              onClick={() => onDawCmd(cmd)}
            >{lbl}</motion.button>
          ))}
        </div>
      </div>
    </BoxWrapper>
  )
}
