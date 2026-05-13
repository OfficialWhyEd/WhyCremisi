import { createContext, useContext } from 'react'

export const BoxContext = createContext(null)

export function useBoxContext() {
  return useContext(BoxContext)
}

export default BoxContext
