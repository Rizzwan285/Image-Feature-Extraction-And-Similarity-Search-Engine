//displaying metrics comparison grid
import { motion } from 'framer-motion'
import { Clock, Cpu } from 'lucide-react'

const METRIC_ORDER = [
  { key: 'euclidean', title: 'Euclidean (L2)',  accent: 'from-accent-300 to-accent-500' },
  { key: 'manhattan', title: 'Manhattan (L1)',  accent: 'from-magenta-400 to-magenta-500' },
  { key: 'cosine',    title: 'Cosine',          accent: 'from-amber-300 to-amber-500' },
]

//displaying single metric column
function MetricColumn({ title, accent, payload, index }) {
  const stats = payload?.stats || {}
  const results = payload?.results || []

  return (
    <motion.div
      initial={{ opacity: 0, y: 16 }}
      animate={{ opacity: 1, y: 0 }}
      transition={{ duration: 0.4, delay: index * 0.08 }}
      className="glass rounded-2xl p-4 flex flex-col gap-3"
    >
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <div className={`h-2 w-8 rounded-full bg-gradient-to-r ${accent}`} />
          <div className="font-display font-semibold text-base">{title}</div>
        </div>
        <div className="flex items-center gap-1 text-[11px] font-mono text-white/40">
          <Clock size={11} />
          <span>{(stats.elapsed_ms ?? 0).toFixed(1)} ms</span>
        </div>
      </div>

      <div className="flex flex-col gap-2">
        {results.map((r, i) => (
          <motion.div
            key={`${r.filename}-${i}`}
            initial={{ opacity: 0, x: -8 }}
            animate={{ opacity: 1, x: 0 }}
            transition={{ duration: 0.3, delay: index * 0.08 + i * 0.04 }}
            className="flex items-center gap-3 p-2 rounded-xl bg-white/5 hover:bg-white/10 transition-colors"
          >
            <div className="flex-shrink-0 w-7 h-7 rounded-md bg-ink-950/70 flex items-center justify-center font-mono text-xs">
              #{r.rank}
            </div>
            <div className="flex-shrink-0 w-12 h-12 rounded-md overflow-hidden bg-ink-800">
              <img src={r.url} alt={r.original_filename} className="w-full h-full object-cover" />
            </div>
            <div className="flex-1 min-w-0">
              <div className="text-xs text-white/90 truncate">{r.original_filename}</div>
              <div className="text-[10px] font-mono text-white/40">d = {r.distance.toFixed(4)}</div>
            </div>
          </motion.div>
        ))}
        {results.length === 0 && (
          <div className="text-xs text-white/40 italic p-2">no results</div>
        )}
      </div>
    </motion.div>
  )
}

export default function ComparisonGrid({ data }) {
  if (!data || !data.metrics) return null

  const totalElapsed = METRIC_ORDER.reduce((sum, m) => {
    return sum + (data.metrics[m.key]?.stats?.elapsed_ms || 0)
  }, 0)

  return (
    <motion.section
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      transition={{ duration: 0.4 }}
      className="px-6 pb-16"
    >
      <div className="max-w-6xl mx-auto">

        <div className="flex flex-wrap items-end justify-between gap-4 mb-6">
          <div>
            <div className="font-mono text-xs uppercase tracking-widest text-white/40 mb-1">
              Compare metrics
            </div>
            <h2 className="font-display font-bold text-3xl">
              Same query, three function pointers
            </h2>
            <p className="text-sm text-white/50 mt-1">
              Each column ran the C binary with a different
              {' '}<code className="font-mono text-accent-200">DistanceFunction</code>.
              The threading code didn’t change — only the function pointer did.
            </p>
          </div>

          <div className="flex items-center gap-2 px-3 py-1.5 rounded-full glass text-xs">
            <Cpu size={13} className="text-magenta-400" />
            <span className="text-white/50 font-mono">total</span>
            <span className="font-mono text-white/90">{totalElapsed.toFixed(1)} ms</span>
          </div>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
          {METRIC_ORDER.map((m, i) => (
            <MetricColumn
              key={m.key}
              title={m.title}
              accent={m.accent}
              payload={data.metrics[m.key]}
              index={i}
            />
          ))}
        </div>

      </div>
    </motion.section>
  )
}
