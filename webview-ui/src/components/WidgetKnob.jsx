import React, { useState } from 'react'
import { whycremisi } from '../../whycremisi-bridge'
import './WidgetKnob.css'

function WidgetKnob({ id, label, min, max, default: defaultValue, onRemove }) {
  const [value, setValue] = useState(defaultValue || 0)

  const handleChange = (e) => {
    const newValue = parseFloat(e.target.value)
    setValue(newValue)
    
    whycremisi.sendDAWCommand('setParameter', { widgetId: id, value: newValue })
  }

  // Visualizzazione grafica semplice del knob
  const percentage = ((value - (min || -1)) / ((max || 1) - (min || -1))) * 100

  return (
    <div className="widget knob-widget">
      <div className="widget-header">
        <label>{label}</label>
        <button className="remove-btn" onClick={onRemove} title="Rimuovi">×</button>
      </div>
      <div className="widget-body">
        <div className="knob-visual">
          <div className="knob-track">
            <div 
              className="knob-fill" 
              style={{ width: `${percentage}%` }}
            />
          </div>
          <span className="knob-value">{value.toFixed(2)}</span>
        </div>
        <input
          type="range"
          min={min || -1}
          max={max || 1}
          step={0.01}
          value={value}
          onChange={handleChange}
          className="knob-input"
        />
      </div>
    </div>
  )
}

export default WidgetKnob
