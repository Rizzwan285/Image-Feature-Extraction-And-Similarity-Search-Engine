// LoadingState.jsx
// ----------------
// Shown while the search is running — which includes Python calling the C binary,
// C loading/extracting features across all threads, and Python parsing the output.
//
// Instead of a generic spinner, we cycle through phase labels that describe
// what's actually happening inside the C code. This makes the wait feel meaningful
// and gives the professor a visual hint about what the algorithm is doing.
//
// No props needed — this component is fully self-contained.

// useEffect and useState for the cycling phase animation
import { useEffect, useState } from 'react'

// motion for the smooth fade-in of each phase label
import { motion } from 'framer-motion'

// The text labels we'll cycle through while waiting.
// They loosely describe the real steps the C binary takes.
const PHASES = [
  'Converting query to PPM…',       // Python / Pillow step
  'Extracting features in C…',      // C: extract_features() running
  'Comparing across dataset…',      // C: worker threads computing distances
  'Ranking top matches…',           // C: sorting the top-K array
]

export default function LoadingState() {
  // which phase label is currently shown (index into PHASES)
  const [phase, setPhase] = useState(0)

  // Cycle through the phase labels every 700ms
  useEffect(() => {
    const interval = setInterval(() => {
      // % PHASES.length wraps around: 3 → 0, so it loops forever
      setPhase((p) => (p + 1) % PHASES.length)
    }, 700)
    // Cleanup: clear the interval when this component unmounts (search finished)
    return () => clearInterval(interval)
  }, [])  // empty dependency array = run this effect only once on mount

  return (
    // Fade in when this component appears
    <motion.div
      initial={{ opacity: 0, y: 12 }}
      animate={{ opacity: 1, y: 0 }}
      exit={{ opacity: 0 }}
      transition={{ duration: 0.3 }}
      className="px-6 py-12"
    >
      {/* Center everything horizontally */}
      <div className="max-w-md mx-auto text-center">

        {/* Spinner — a glowing ring that spins */}
        <div className="relative mx-auto w-20 h-20 mb-6">
          {/* Background glow — a blurred gradient that pulses slowly */}
          <div className="absolute inset-0 rounded-full bg-gradient-to-r from-accent-400 to-magenta-500 blur-xl opacity-60 animate-pulse-slow" />
          {/* The actual spinning ring — only the top quarter has the bright color */}
          <div className="relative w-20 h-20 rounded-full border-2 border-accent-400/30 border-t-accent-400 animate-spin" />
        </div>

        {/* Small "Search in progress" label above the phase text */}
        <div className="font-mono text-xs uppercase tracking-widest text-white/40 mb-2">
          Search in progress
        </div>

        {/* Phase label — each new phase fades in from below */}
        <motion.div
          key={phase}    // key changes trigger a remount, which replays the animation
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
