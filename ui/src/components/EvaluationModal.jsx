//displaying evaluation modal component
import { useEffect, useState } from 'react'
import { motion, AnimatePresence } from 'framer-motion'
import { X, Award, Loader2, BarChart3 } from 'lucide-react'

//defining metric styling
const METRIC_STYLES = {
  euclidean: { label: 'Euclidean (L2)', accent: 'text-accent-300',  bar: 'from-accent-300 to-accent-500' },
  manhattan: { label: 'Manhattan (L1)', accent: 'text-magenta-400', bar: 'from-magenta-400 to-magenta-500' },
  cosine:    { label: 'Cosine',         accent: 'text-amber-300',   bar: 'from-amber-300 to-amber-500' },
}

export default function EvaluationModal({ open, onClose }) {
  const [data, setData] = useState(null)
  const [loading, setLoading] = useState(false)
  const [k, setK] = useState(5)
  const [error, setError] = useState(null)

  //running evaluation
  const runEvaluation = async () => {
    setLoading(true)
    setError(null)
    setData(null)
    try {
      const res = await fetch('/api/evaluate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ top_k: k, num_threads: 4 }),
      })
      if (!res.ok) throw new Error(await res.text() || `HTTP ${res.status}`)
      setData(await res.json())
    } catch (e) {
      console.error('evaluate failed:', e)
      setError(e.message || 'evaluation failed')
    } finally {
      setLoading(false)
    }
  }

  //triggering evaluation automatically
  useEffect(() => {
    if (open && data === null && !loading && !error) {
      runEvaluation()
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [open])

  //determining winning metric
  const winner = (() => {
    if (!data?.metrics) return null
    let best = null
    for (const [name, m] of Object.entries(data.metrics)) {
      if (best === null || m.recall_at_k > best.value) {
        best = { name, value: m.recall_at_k }
      }
    }
    return best
  })()

  return (
    <AnimatePresence>
      {open && (
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          onClick={onClose}
          className="fixed inset-0 z-50 bg-black/70 backdrop-blur-sm flex items-center justify-center p-4"
        >
          <motion.div
            initial={{ opacity: 0, scale: 0.95, y: 8 }}
            animate={{ opacity: 1, scale: 1, y: 0 }}
            exit={{ opacity: 0, scale: 0.97, y: 8 }}
            transition={{ duration: 0.25, ease: [0.22, 1, 0.36, 1] }}
            onClick={(e) => e.stopPropagation()}
            className="glass-strong rounded-3xl p-6 md:p-8 w-full max-w-3xl max-h-[90vh] overflow-y-auto"
          >
            <div className="flex items-start justify-between mb-6">
              <div>
                <div className="flex items-center gap-2 mb-2">
                  <BarChart3 size={18} className="text-accent-300" />
                  <div className="font-mono text-xs uppercase tracking-widest text-white/40">
                    Dataset evaluation
                  </div>
                </div>
                <h2 className="font-display font-bold text-2xl md:text-3xl">
                  Recall@{k} per metric
                </h2>
                <p className="text-sm text-white/50 mt-1">
                  Each image is used as a query; we measure how many of its top-{k} neighbours
                  share the same category (inferred from the filename).
                </p>
              </div>
              <button
                onClick={onClose}
                className="p-2 rounded-full hover:bg-white/10 transition-colors text-white/60 hover:text-white"
                aria-label="close"
              >
                <X size={18} />
              </button>
            </div>

            <div className="flex flex-wrap items-center gap-3 mb-6">
              <label className="flex items-center gap-2 text-sm">
                <span className="text-xs font-mono uppercase tracking-widest text-white/40">K</span>
                <input
                  type="number"
                  min={1}
                  max={20}
                  value={k}
                  onChange={(e) => setK(Math.max(1, Math.min(20, Number(e.target.value) || 1)))}
                  disabled={loading}
                  className="w-16 px-2 py-1.5 rounded-lg bg-white/5 border border-white/10 text-white/90 text-sm focus:outline-none focus:ring-2 focus:ring-accent-400/40 disabled:opacity-50"
                />
              </label>
              <button
                onClick={runEvaluation}
                disabled={loading}
                className="px-4 py-2 rounded-xl font-medium text-ink-950 bg-gradient-to-r from-accent-300 to-magenta-400 disabled:opacity-50 disabled:cursor-not-allowed shadow-lg shadow-accent-500/20 hover:shadow-accent-500/40 transition-shadow flex items-center gap-2 text-sm"
              >
                {loading ? <Loader2 size={14} className="animate-spin" /> : <BarChart3 size={14} />}
                <span>{loading ? 'Evaluating…' : 'Run evaluation'}</span>
              </button>

              {data?.num_queries !== undefined && (
                <div className="ml-auto text-xs font-mono text-white/40">
                  {data.num_queries} queries · {Object.keys(data.categories || {}).length} categories
                </div>
              )}
            </div>

            {error && (
              <div className="mb-4 p-3 rounded-xl border border-red-500/30 bg-red-500/10 text-sm text-red-200">
                {error}
              </div>
            )}

            {loading && (
              <div className="flex flex-col items-center justify-center py-12 text-white/50">
                <Loader2 size={32} className="animate-spin text-accent-300 mb-3" />
                <div className="text-sm">Running {data?.num_queries ?? '?'} queries × 3 metrics…</div>
                <div className="text-xs font-mono mt-1 text-white/30">
                  every comparison still goes through the C binary
                </div>
              </div>
            )}

            {!loading && data?.metrics && (
              <div className="space-y-3">
                {Object.entries(data.metrics).map(([name, m]) => {
                  const style = METRIC_STYLES[name] || METRIC_STYLES.euclidean
                  const pct = Math.round(m.recall_at_k * 100)
                  const isWinner = winner?.name === name
                  return (
                    <motion.div
                      key={name}
                      initial={{ opacity: 0, y: 8 }}
                      animate={{ opacity: 1, y: 0 }}
                      transition={{ duration: 0.3 }}
                      className={`p-4 rounded-2xl glass ${isWinner ? 'ring-2 ring-amber-300/40' : ''}`}
                    >
                      <div className="flex items-center justify-between mb-2">
                        <div className="flex items-center gap-2">
                          <div className={`font-display font-semibold ${style.accent}`}>
                            {style.label}
                          </div>
                          {isWinner && (
                            <div className="flex items-center gap-1 px-2 py-0.5 rounded-full bg-amber-300/15 text-amber-200 text-[10px] font-mono uppercase tracking-wider">
                              <Award size={10} /> best
                            </div>
                          )}
                        </div>
                        <div className="text-right">
                          <div className="font-mono text-xl font-bold text-white">{pct}%</div>
                          <div className="text-[10px] font-mono text-white/40">
                            mean {m.mean_elapsed_ms.toFixed(2)} ms / query
                          </div>
                        </div>
                      </div>
                      <div className="relative h-2 rounded-full bg-white/5 overflow-hidden">
                        <motion.div
                          initial={{ width: 0 }}
                          animate={{ width: `${pct}%` }}
                          transition={{ duration: 0.7, ease: [0.22, 1, 0.36, 1] }}
                          className={`h-full rounded-full bg-gradient-to-r ${style.bar}`}
                        />
                      </div>
                    </motion.div>
                  )
                })}
              </div>
            )}

            {!loading && data?.categories && Object.keys(data.categories).length > 0 && (
              <div className="mt-6 pt-4 border-t border-white/5">
                <div className="text-xs font-mono uppercase tracking-widest text-white/40 mb-2">
                  Categories detected from filenames
                </div>
                <div className="flex flex-wrap gap-2">
                  {Object.entries(data.categories).map(([cat, count]) => (
                    <div
                      key={cat}
                      className="px-2.5 py-1 rounded-full glass text-xs font-mono text-white/70"
                    >
                      {cat} <span className="text-white/40">×{count}</span>
                    </div>
                  ))}
                </div>
              </div>
            )}

          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  )
}
