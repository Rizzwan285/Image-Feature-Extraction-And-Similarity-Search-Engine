//displaying loading state component
import { useEffect, useState } from 'react'

import { motion } from 'framer-motion'

const PHASES = [
  'Converting query to PPM…',
  'Extracting features in C…',
  'Comparing across dataset…',
  'Ranking top matches…',
]

export default function LoadingState() {
  const [phase, setPhase] = useState(0)

  useEffect(() => {
    const interval = setInterval(() => {
      setPhase((p) => (p + 1) % PHASES.length)
    }, 700)
    return () => clearInterval(interval)
  }, [])

  return (
    <motion.div
      initial={{ opacity: 0, y: 12 }}
      animate={{ opacity: 1, y: 0 }}
      exit={{ opacity: 0 }}
      transition={{ duration: 0.3 }}
      className="px-6 py-12"
    >
      <div className="max-w-md mx-auto text-center">

        <div className="relative mx-auto w-20 h-20 mb-6">
          <div className="absolute inset-0 rounded-full bg-gradient-to-r from-accent-400 to-magenta-500 blur-xl opacity-60 animate-pulse-slow" />
          <div className="relative w-20 h-20 rounded-full border-2 border-accent-400/30 border-t-accent-400 animate-spin" />
        </div>

        <div className="font-mono text-xs uppercase tracking-widest text-white/40 mb-2">
          Search in progress
        </div>

        <motion.div
          key={phase}
          initial={{ opacity: 0, y: 6 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.3 }}
          className="font-display font-medium text-lg text-white/85"
        >
          {PHASES[phase]}
        </motion.div>

      </div>
    </motion.div>
  )
}
