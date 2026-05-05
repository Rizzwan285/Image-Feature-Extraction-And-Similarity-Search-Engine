//displaying hero section component
import { motion } from 'framer-motion'

import { Sparkles, BarChart3 } from 'lucide-react'

export default function Hero({ onShowArchitecture, onShowEvaluation }) {
  return (
    // The outer header element with padding top/bottom and horizontal padding
    <header className="relative pt-16 pb-10 px-6">
      {/* Constrain content width and center it */}
      <div className="max-w-6xl mx-auto">

        <motion.div
          initial={{ opacity: 0, y: -10 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.6 }}
          className="flex items-center gap-2 mb-6"
        >
          <div className="flex items-center gap-2 glass rounded-full pl-2 pr-3 py-1 text-xs font-mono tracking-wide text-accent-200">
            <span className="relative inline-flex h-2 w-2">
              <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-accent-400 opacity-60"></span>
              <span className="relative inline-flex h-2 w-2 rounded-full bg-accent-400"></span>
            </span>
            <span>react → python → c → threads</span>
          </div>

          <button
            onClick={onShowArchitecture}
            className="glass rounded-full px-3 py-1 text-xs font-mono tracking-wide text-magenta-400 hover:bg-magenta-500/10 hover:border-magenta-400/40 transition-colors flex items-center gap-1.5"
          >
            <Sparkles size={12} />
            <span>view architecture</span>
          </button>

          {onShowEvaluation && (
            <button
              onClick={onShowEvaluation}
              className="glass rounded-full px-3 py-1 text-xs font-mono tracking-wide text-amber-300 hover:bg-amber-300/10 hover:border-amber-300/40 transition-colors flex items-center gap-1.5"
            >
              <BarChart3 size={12} />
              <span>run evaluation</span>
            </button>
          )}
        </motion.div>

        <motion.h1
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.7, delay: 0.1 }}
          className="font-display font-bold tracking-tight text-5xl md:text-7xl leading-[1.05] mb-5"
        >
          Image{' '}
          <span className="gradient-text">Similarity</span>
          <br />
          Search Engine
        </motion.h1>

        <motion.p
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
