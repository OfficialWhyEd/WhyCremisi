import React, { useState } from 'react'
import { whycremisi } from '../../whycremisi-bridge'
import './WidgetButton.css'

function WidgetButton({ id, label, action, onRemove }) {
  const [isActive, setIsActive] = useState(false)

  const handleClick = () => {
    setIsActive(!isActive)
    
    whycremisi.sendDAWCommand(action || 'play', { widgetId: id, active: !isActive })
  }

  return (
    <div className="widget button-widget">
      <div className="widget-header">
        <label>{label}</label>
        <button className="remove-btn" onClick={onRemove} title="Rimuovi">×</button>
      </div>
      <div className="widget-body">
        <button 
          className={`action-btn ${isActive ? 'active' : ''}`}
          onClick={handleClick}
        >
          {isActive ? '⏹ Stop' : '▶ Play'}
        </button>
      </div>
    </div>
  )
}

export default WidgetButton
