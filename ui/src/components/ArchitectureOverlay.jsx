//displaying architecture overlay component
import { motion, AnimatePresence } from 'framer-motion'

import { X, Layers, Code2, Network, HardDrive, Zap } from 'lucide-react'

//defining application layers
const LAYERS = [
  {
    id: 'ui',
    title: 'UI Layer',
    tech: 'React + Vite + Tailwind + Framer Motion',
    icon: Layers,
    accent: 'text-accent-300',
    border: 'border-accent-500/30',
    description:
      'Single-page interface for uploading a query, browsing the dataset, and viewing animated similarity results. Pure presentation — no algorithm runs here.',
  },
  {
    id: 'comm',
    title: 'Communication Layer',
    tech: 'Python + FastAPI + Pillow',
    icon: Network,
    accent: 'text-magenta-400',
    border: 'border-magenta-500/30',
    description:
      'Bridges the UI and the C binary. Converts uploaded images to PPM with Pillow, invokes imgsearch via subprocess, parses its JSON output, and returns enriched results to the frontend.',
  },
  {
    id: 'algo',
    title: 'Algorithm Layer',
    tech: 'C + pthreads + Makefile',
    icon: Code2,
    accent: 'text-amber-300',
    border: 'border-amber-500/30',
    description:
      'The core. Reads PPM images, extracts a 534-float feature vector per image (RGB histogram + channel stats + 4×4 block grid), and ranks the dataset against the query in parallel using pthreads with a mutex-protected top-K array.',
  },
  {
    id: 'backend',
    title: 'Backend Layer',
    tech: 'Binary feature cache file',
    icon: HardDrive,
    accent: 'text-emerald-400',
    border: 'border-emerald-500/30',
    description:
      'A simple binary cache (features.cache) written by the C program. Skips re-extracting features for unchanged datasets, turning ~30 ms cold searches into ~5 ms warm ones.',
  },
]

export default function ArchitectureOverlay({ open, onClose }) {
  return (
    <AnimatePresence>
      {open && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          transition={{ duration: 0.2 }}
          className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-ink-950/80 backdrop-blur-md"
          onClick={onClose}
        >
          <motion.div
            initial={{ scale: 0.95, opacity: 0, y: 20 }}
            animate={{ scale: 1, opacity: 1, y: 0 }}
            exit={{ scale: 0.95, opacity: 0, y: 20 }}
            transition={{ duration: 0.3, ease: [0.22, 1, 0.36, 1] }}
            onClick={(e) => e.stopPropagation()}
            className="glass-strong w-full max-w-4xl max-h-[90vh] rounded-3xl overflow-hidden flex flex-col"
          >
            <div className="flex items-center justify-between px-6 py-4 border-b border-white/5">
              <div className="flex items-center gap-3">
                <div className="p-2 rounded-lg bg-white/5">
                  <Zap size={18} className="text-accent-300" />
                </div>
                <div>
                  <div className="font-display font-semibold text-lg">Architecture</div>
                  <div className="text-xs text-white/50">Four-layer course-template structure</div>
                </div>
              </div>
              <button
                onClick={onClose}
                className="p-2 rounded-lg text-white/60 hover:text-white hover:bg-white/5 transition-colors"
              >
                <X size={20} />
              </button>
            </div>

            <div className="overflow-y-auto p-6">
              <div className="space-y-3">
                {LAYERS.map((layer, idx) => {
                  const Icon = layer.icon
                  return (
                    <motion.div
                      key={layer.id}
                      initial={{ opacity: 0, x: -16 }}
                      animate={{ opacity: 1, x: 0 }}
                      transition={{ duration: 0.4, delay: idx * 0.08 }}
                      className={`relative p-5 rounded-2xl bg-ink-900/60 border ${layer.border} hover:bg-ink-900/80 transition-colors`}
                    >
                      <div className="flex items-start gap-4">
                        <div className={`p-2.5 rounded-xl bg-white/5 ${layer.accent} flex-shrink-0`}>
                          <Icon size={20} />
                        </div>

                        <div className="min-w-0">
                          <div className="flex items-baseline justify-between gap-3 mb-1 flex-wrap">
                            <div className="font-display font-semibold text-lg">
                              {layer.title}
                            </div>
                            <div className={`text-xs font-mono ${layer.accent}`}>
                              {layer.tech}
                            </div>
                          </div>
                          <div className="text-sm text-white/60 leading-relaxed">
                            {layer.description}
                          </div>
                        </div>
                      </div>

                      {idx < LAYERS.length - 1 && (
                        <div className="absolute -bottom-2 left-9 w-px h-3 bg-gradient-to-b from-white/20 to-transparent" />
                      )}
                    </motion.div>
                  )
                })}
              </div>

              <motion.div
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                transition={{ delay: 0.5 }}
                className="mt-6 p-4 rounded-xl bg-ink-900/40 border border-white/5 text-xs font-mono text-white/50 leading-relaxed"
              >
                <span className="text-white/70">Request flow:</span>{' '}
                browser → <span className="text-accent-300">React</span> → HTTP → <span className="text-magenta-400">FastAPI</span> → subprocess → <span className="text-amber-300">imgsearch (C)</span> → reads <span className="text-emerald-400">cache</span> + dataset → JSON back up the chain.
              </motion.div>
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  )
}
