//displaying results grid component
import { motion } from 'framer-motion'

import { Clock, Cpu, Database } from 'lucide-react'

import ResultCard from './ResultCard.jsx'

//displaying single stat pill component
function StatPill({ icon: Icon, label, value, accent }) {
  return (
    <div className="flex items-center gap-2 px-3 py-1.5 rounded-full glass text-xs">
      <Icon size={13} className={accent} />
      <span className="text-white/50 font-mono">{label}</span>
      <span className="font-mono text-white/90">{value}</span>
    </div>
  )
}

export default function ResultsGrid({ results, stats }) {
  if (!results || results.length === 0) return null

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
              Top {results.length} matches
            </div>
            <h2 className="font-display font-bold text-3xl">
              Closest images
            </h2>
          </div>

          <div className="flex flex-wrap gap-2">
            <StatPill
              icon={Clock}
              label="elapsed"
              value={`${stats.elapsed_ms.toFixed(1)} ms`}
              accent="text-accent-300"
            />
            <StatPill
              icon={Cpu}
              label="threads"
              value={stats.threads_used}
              accent="text-magenta-400"
            />
            <StatPill
              icon={Database}
              label="dataset"
              value={stats.total_dataset_images}
              accent="text-accent-300"
            />
            <StatPill
              icon={Database}
              label="cache"
              value={stats.cache_hit ? 'HIT' : 'MISS'}
              accent={stats.cache_hit ? 'text-emerald-400' : 'text-amber-400'}
            />
          </div>
        </div>

        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4 auto-rows-fr">
          {results.map((r, i) => (
            <ResultCard key={`${r.filename}-${i}`} result={r} index={i} />
          ))}
        </div>

      </div>
    </motion.section>
  )
}
