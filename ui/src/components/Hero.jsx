// Hero.jsx
// --------
// The top section of the page — the big title, the tagline, and the two
// small pills that show the tech stack and open the architecture overlay.
//
// This component has no state of its own. It receives one prop:
//   onShowArchitecture — function to call when the "view architecture" button is clicked

// motion is Framer Motion's animated version of a div/p/h1/etc.
// Wrapping an element in <motion.div> lets us add initial/animate/transition props.
import { motion } from 'framer-motion'

// A sparkle icon from lucide-react — a collection of clean SVG icons for React
import { Sparkles } from 'lucide-react'

export default function Hero({ onShowArchitecture }) {
  return (
    // The outer header element with padding top/bottom and horizontal padding
    <header className="relative pt-16 pb-10 px-6">
      {/* Constrain content width and center it */}
      <div className="max-w-6xl mx-auto">

        {/* --- Top row: tech stack pill + architecture button --- */}
        <motion.div
          // Start slightly above final position and invisible, then slide down
          initial={{ opacity: 0, y: -10 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.6 }}
          className="flex items-center gap-2 mb-6"
        >
          {/* Tech stack pill — reads-only label showing the request flow */}
          <div className="flex items-center gap-2 glass rounded-full pl-2 pr-3 py-1 text-xs font-mono tracking-wide text-accent-200">
            {/* Pulsing green dot — indicates the system is alive */}
            <span className="relative inline-flex h-2 w-2">
              {/* Outer ring that expands and fades — the "ping" animation */}
              <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-accent-400 opacity-60"></span>
              {/* Inner solid dot that stays put */}
              <span className="relative inline-flex h-2 w-2 rounded-full bg-accent-400"></span>
            </span>
            {/* The text label describing the request flow */}
            <span>react → python → c → threads</span>
          </div>

          {/* Architecture button — clicking it opens the overlay in App.jsx */}
          <button
            onClick={onShowArchitecture}
            className="glass rounded-full px-3 py-1 text-xs font-mono tracking-wide text-magenta-400 hover:bg-magenta-500/10 hover:border-magenta-400/40 transition-colors flex items-center gap-1.5"
          >
            <Sparkles size={12} />
            <span>view architecture</span>
          </button>
        </motion.div>

        {/* --- Main title --- */}
        <motion.h1
          // Fade in and slide up from slightly below, with a short delay
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.7, delay: 0.1 }}
          className="font-display font-bold tracking-tight text-5xl md:text-7xl leading-[1.05] mb-5"
        >
          Image{' '}
          {/* gradient-text is our custom CSS class that makes text cyan→magenta */}
          <span className="gradient-text">Similarity</span>
          <br />
          Search Engine
        </motion.h1>

        {/* --- Subtitle / tagline --- */}
        <motion.p
          // Slightly longer delay than the title so they stagger nicely
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.7, delay: 0.25 }}
          className="text-lg text-white/60 max-w-2xl leading-relaxed"
        >
          Upload an image and find its closest visual matches across the dataset.
          Feature extraction and ranking run in parallel C threads — the UI just
          shows you the result.
        </motion.p>

      </div>
    </header>
  )
}
