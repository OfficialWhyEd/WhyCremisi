import React, { useState, useEffect } from 'react'
import { whycremisi } from '../whycremisi-bridge'

function Toolbox({ lastMessage }) {
  const [widgets, setWidgets] = useState([])
  const [proposals, setProposals] = useState([])
  const [logs, setLogs] = useState([])

  useEffect(() => {
    if (!lastMessage) return
    addLog(`Ricevuto: ${lastMessage.type}`)

    if (lastMessage.type === 'daw.transport') {
      addLog(`Transport: play=${lastMessage.payload?.isPlaying} rec=${lastMessage.payload?.isRecording} bpm=${lastMessage.payload?.bpm}`)
    } else if (lastMessage.type === 'osc.message' || lastMessage.type === 'daw.param_updated') {
      const proposal = generateProposal(lastMessage)
      if (proposal) {
        setProposals(prev => [...prev, proposal])
        addLog(`Proposta: ${proposal.widget_config.type}`)
      }
    }
  }, [lastMessage])

  const generateProposal = (message) => {
    const payload = message.payload || message
    const id = `proposal-${Date.now()}`

    if (message.type === 'osc.message') {
      const addr = payload.address || ''
      const value = payload.value ?? 0.5
      if (addr.includes('volume') || addr.includes('gain')) {
        return { id, message, widget_config: { type: 'slider', id: `widget-${Date.now()}`, label: addr, min: 0, max: 1, default: value, address: addr } }
      }
      if (addr.includes('pan')) {
        return { id, message, widget_config: { type: 'knob', id: `widget-${Date.now()}`, label: addr, min: -1, max: 1, default: value, address: addr } }
      }
      if (addr.includes('play') || addr.includes('stop') || addr.includes('record')) {
        return { id, message, widget_config: { type: 'button', id: `widget-${Date.now()}`, label: addr, address: addr } }
      }
    }

    if (payload.event) {
      const { event } = payload
      if (event.type === 'parameter_change' && event.category === 'volume') {
        return { id, message, widget_config: { type: 'slider', id: `widget-${Date.now()}`, label: `Volume ${event.target?.track_name || event.target?.track_id || ''}`, min: 0, max: 1, default: event.value?.raw || 0.5, param: event.target?.param_name, track_id: event.target?.track_id } }
      }
      if (event.type === 'parameter_change' && event.category === 'pan') {
        return { id, message, widget_config: { type: 'knob', id: `widget-${Date.now()}`, label: `Pan ${event.target?.track_name || event.target?.track_id || ''}`, min: -1, max: 1, default: event.value?.raw || 0, param: event.target?.param_name, track_id: event.target?.track_id } }
      }
      if (event.type === 'transport') {
        return { id, message, widget_config: { type: 'button', id: `widget-${Date.now()}`, label: 'Transport', action: event.target?.param_name || 'play' } }
      }
    }

    return null
  }

  const acceptProposal = (proposal) => {
    setWidgets(prev => [...prev, proposal.widget_config])
    setProposals(prev => prev.filter(p => p.id !== proposal.id))
    addLog(`Widget aggiunto: ${proposal.widget_config.type}`)
  }

  const handleWidgetChange = (widgetId, value) => {
    const widget = widgets.find(w => w.id === widgetId)
    if (!widget) return

    if (whycremisi.isConnected()) {
      if (widget.address) {
        whycremisi.send({ type: 'osc.send', address: widget.address, value: value })
      } else if (widget.param) {
        whycremisi.sendDAWCommand('setVolume', { trackId: widget.track_id, param: widget.param, value })
      }
      addLog(`Inviato: ${widget.label} = ${value.toFixed(2)}`)
    }
  }

  const handleTransportCommand = (action) => {
    if (whycremisi.isConnected()) {
      whycremisi.sendDAWCommand(action)
      addLog(`Transport: ${action}`)
    }
  }

  const removeWidget = (widgetId) => {
    setWidgets(prev => prev.filter(w => w.id !== widgetId))
    addLog(`Widget rimosso: ${widgetId}`)
  }

  const addTestWidget = (type) => {
    const configs = {
      slider: { type: 'slider', id: `widget-${Date.now()}`, label: 'Volume Test', min: 0, max: 1, default: 0.5, address: '/track/1/volume' },
      knob: { type: 'knob', id: `widget-${Date.now()}`, label: 'Pan Test', min: -1, max: 1, default: 0, address: '/track/1/pan' },
      button: { type: 'button', id: `widget-${Date.now()}`, label: 'Play/Stop', address: '/play' }
    }
    setWidgets(prev => [...prev, configs[type]])
    addLog(`Widget test aggiunto: ${type}`)
  }

  const addLog = (msg) => {
    setLogs(prev => [{ time: new Date().toLocaleTimeString(), msg }, ...prev].slice(0, 10))
  }

  const renderWidget = (widget) => {
    const onChange = (v) => handleWidgetChange(widget.id, v)
    const onAction = () => handleTransportCommand(widget.action || 'play')
    const onRemove = () => removeWidget(widget.id)

    switch (widget.type) {
      case 'slider':
        return <WidgetSlider key={widget.id} {...widget} onChange={onChange} onRemove={onRemove} />
      case 'knob':
        return <WidgetKnob key={widget.id} {...widget} onChange={onChange} onRemove={onRemove} />
      case 'button':
        return <WidgetButton key={widget.id} {...widget} onAction={onAction} onRemove={onRemove} />
      default:
        return null
    }
  }

  return (
    <div className="p-4 space-y-4 font-['Space_Grotesk']">
      {/* Transport Controls */}
      <div className="flex gap-2">
        <button className="bg-[#1a1a1a] border border-[#222222] text-[#FFB000] px-3 py-1.5 text-[10px] uppercase font-bold tracking-widest hover:border-[#FFB000] hover:shadow-[0_0_15px_rgba(255,176,0,0.2)] transition-all" onClick={() => handleTransportCommand('play')}>Play</button>
        <button className="bg-[#1a1a1a] border border-[#222222] text-[#4d4d4d] px-3 py-1.5 text-[10px] uppercase font-bold tracking-widest hover:border-[#FFB000] hover:text-[#FFB000] transition-all" onClick={() => handleTransportCommand('stop')}>Stop</button>
        <button className="bg-[#1a1a1a] border border-[#222222] text-[#4d4d4d] px-3 py-1.5 text-[10px] uppercase font-bold tracking-widest hover:border-[#DC143C] hover:text-[#DC143C] transition-all" onClick={() => handleTransportCommand('record')}>Rec</button>
      </div>

      {/* Test Widget Buttons */}
      <div className="flex gap-2 flex-wrap">
        <button className="bg-[#0e0e0e] border border-[#222222] text-[#4d4d4d] px-2 py-1 text-[9px] uppercase font-bold hover:border-[#FFB000] hover:text-[#FFB000] transition-all" onClick={() => addTestWidget('slider')}>+ Slider</button>
        <button className="bg-[#0e0e0e] border border-[#222222] text-[#4d4d4d] px-2 py-1 text-[9px] uppercase font-bold hover:border-[#FFB000] hover:text-[#FFB000] transition-all" onClick={() => addTestWidget('knob')}>+ Knob</button>
        <button className="bg-[#0e0e0e] border border-[#222222] text-[#4d4d4d] px-2 py-1 text-[9px] uppercase font-bold hover:border-[#FFB000] hover:text-[#FFB000] transition-all" onClick={() => addTestWidget('button')}>+ Button</button>
      </div>

      {/* Proposals */}
      <div>
        <span className="text-[10px] font-bold text-[#4d4d4d] uppercase tracking-widest">Proposte Widget</span>
        {proposals.length === 0 ? (
          <p className="text-[9px] text-[#4d4d4d] mt-1 font-mono">Nessuna proposta.</p>
        ) : (
          proposals.map(p => (
            <div key={p.id} className="flex items-center justify-between bg-[#1a1a1a] border border-[#222222] px-2 py-1 mt-1">
              <span className="text-[9px] text-[#e5e2e1] font-mono">{p.widget_config.label} <span className="text-[#4d4d4d]">({p.widget_config.type})</span></span>
              <div className="flex gap-1">
                <button className="text-[9px] text-[#00FFaa] font-bold uppercase" onClick={() => acceptProposal(p)}>Add</button>
                <button className="text-[9px] text-[#4d4d4d] font-bold uppercase" onClick={() => setProposals(prev => prev.filter(x => x.id !== p.id))}>X</button>
              </div>
            </div>
          ))
        )}
      </div>

      {/* Active Widgets */}
      <div>
        <span className="text-[10px] font-bold text-[#4d4d4d] uppercase tracking-widest">Widget Attivi ({widgets.length})</span>
        <div className="space-y-2 mt-2">
          {widgets.map(renderWidget)}
        </div>
      </div>

      {/* Logs */}
      <div>
        <span className="text-[10px] font-bold text-[#4d4d4d] uppercase tracking-widest">Log</span>
        <div className="mt-1 space-y-0.5 max-h-32 overflow-y-auto custom-scrollbar">
          {logs.map((log, i) => (
            <div key={i} className="text-[8px] font-mono text-[#4d4d4d]">
              <span className="text-[#FFB000]">{log.time}</span> {log.msg}
            </div>
          ))}
        </div>
      </div>
    </div>
  )
}

function WidgetSlider({ id, label, min, max, default: defaultValue, onChange, onRemove }) {
  const [value, setValue] = useState(defaultValue || 0.5)

  const handleChange = (e) => {
    const newValue = parseFloat(e.target.value)
    setValue(newValue)
    if (onChange) onChange(newValue)
  }

  return (
    <div className="bg-[#0e0e0e] border border-[#222222] p-2">
      <div className="flex justify-between items-center mb-1">
        <span className="text-[9px] text-[#FFB000] uppercase font-bold tracking-widest">{label}</span>
        <div className="flex items-center gap-2">
          <span className="text-[10px] text-[#e5e2e1] font-mono font-bold">{value.toFixed(2)}</span>
          <button className="text-[8px] text-[#4d4d4d] hover:text-[#DC143C]" onClick={onRemove}>X</button>
        </div>
      </div>
      <input type="range" min={min || 0} max={max || 1} step={0.01} value={value} onChange={handleChange}
        className="w-full h-1 appearance-none bg-[#222222] rounded outline-none [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:w-3 [&::-webkit-slider-thumb]:h-3 [&::-webkit-slider-thumb]:bg-[#DC143C] [&::-webkit-slider-thumb]:rounded-full [&::-webkit-slider-thumb]:cursor-pointer" />
    </div>
  )
}

function WidgetKnob({ id, label, min, max, default: defaultValue, onChange, onRemove }) {
  const [value, setValue] = useState(defaultValue || 0)
  const percentage = ((value - (min || -1)) / ((max || 1) - (min || -1))) * 100

  const handleChange = (e) => {
    const newValue = parseFloat(e.target.value)
    setValue(newValue)
    if (onChange) onChange(newValue)
  }

  return (
    <div className="bg-[#0e0e0e] border border-[#222222] p-2">
      <div className="flex justify-between items-center mb-1">
        <span className="text-[9px] text-[#FFB000] uppercase font-bold tracking-widest">{label}</span>
        <div className="flex items-center gap-2">
          <span className="text-[10px] text-[#e5e2e1] font-mono font-bold">{value.toFixed(2)}</span>
          <button className="text-[8px] text-[#4d4d4d] hover:text-[#DC143C]" onClick={onRemove}>X</button>
        </div>
      </div>
      <div className="flex items-center gap-2">
        <div className="flex-1">
          <div className="h-1.5 bg-[#222222] relative">
            <div className="absolute top-0 left-1/2 h-full w-[1px] bg-[#4d4d4d]"></div>
            <div className="h-full bg-[#FFB000] smooth-transition" style={{ width: `${percentage}%`, marginLeft: percentage > 50 ? '50%' : `${percentage}%` }}></div>
          </div>
          <input type="range" min={min || -1} max={max || 1} step={0.01} value={value} onChange={handleChange}
            className="w-full h-0 opacity-0 absolute" />
        </div>
      </div>
    </div>
  )
}

function WidgetButton({ id, label, action, onAction, onRemove }) {
  const [isActive, setIsActive] = useState(false)

  const handleClick = () => {
    setIsActive(!isActive)
    if (onAction) onAction()
  }

  return (
    <div className="bg-[#0e0e0e] border border-[#222222] p-2 flex justify-between items-center">
      <span className="text-[9px] text-[#FFB000] uppercase font-bold tracking-widest">{label}</span>
      <div className="flex items-center gap-2">
        <button
          className={`px-3 py-1 text-[9px] uppercase font-bold tracking-widest ${isActive ? 'bg-[#DC143C] text-white' : 'bg-[#1a1a1a] border border-[#222222] text-[#4d4d4d] hover:border-[#FFB000] hover:text-[#FFB000]'} transition-all`}
          onClick={handleClick}
        >
          {isActive ? 'ON' : 'OFF'}
        </button>
        <button className="text-[8px] text-[#4d4d4d] hover:text-[#DC143C]" onClick={onRemove}>X</button>
      </div>
    </div>
  )
}

export default Toolbox