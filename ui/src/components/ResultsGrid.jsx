// ResultsGrid.jsx
// ---------------
// Renders the search results section: a stats bar at the top
// (elapsed time, thread count, dataset size, cache hit/miss) followed by
// a responsive grid of ResultCard components.
//
// Props:
//   results — array of result objects from the API response
//   stats   — { elapsed_ms, threads_used, total_dataset_images, cache_hit }

// motion for the section fade-in
import { motion } from 'framer-motion'

// Icons for the stats pills
import { Clock, Cpu, Database } from 'lucide-react'

// The individual result card component
import ResultCard from './ResultCard.jsx'

// StatPill — a small inline helper component for the stats row.
// We define it in this file because it's only used here and is very small.
// Props: icon (component), label (string), value (string), accent (CSS class)
function StatPill({ icon: Icon, label, value, accent }) {
  return (
    // Glass pill with an icon, a muted label, and a bright value
    <div className="flex items-center gap-2 px-3 py-1.5 rounded-full glass text-xs">
      <Icon size={13} className={accent} />
      <span className="text-white/50 font-mono">{label}</span>
      <span className="font-mono text-white/90">{value}</span>
    </div>
  )
}

export default function ResultsGrid({ results, stats }) {
  // Don't render anything if there are no results yet
  if (!results || results.length === 0) return null

  return (
    // Fade in the whole section when results arrive
    <motion.section
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      transition={{ duration: 0.4 }}
      className="px-6 pb-16"
    >
      <div className="max-w-6xl mx-auto">

        {/* ---- Header row: title + stats pills ---- */}
        <div className="flex flex-wrap items-end justify-between gap-4 mb-6">

          {/* Left side: result count heading */}
          <div>
            <div className="font-mono text-xs uppercase tracking-widest text-white/40 mb-1">
              Top {results.length} matches
            </div>
            <h2 className="font-display font-bold text-3xl">
              Closest images
            </h2>
          </div>

          {/* Right side: stats pills — one for each diagnostic value */}
          <div className="flex flex-wrap gap-2">
            {/* How long the search took — pulled from the C binary's timing code */}
            <StatPill
              icon={Clock}
              label="elapsed"
              value={`${stats.elapsed_ms.toFixed(1)} ms`}
              accent="text-accent-300"
            />
            {/* How many worker threads the C binary used */}
            <StatPill
              icon={Cpu}
              label="threads"
              value={stats.threads_used}
              accent="text-magenta-400"
            />
            {/* How many images are in the dataset that was searched */}
            <StatPill
              icon={Database}
              label="dataset"
              value={stats.total_dataset_images}
              accent="text-accent-300"
            />
            {/* Whether the feature cache was used — HIT means features weren't
                re-extracted, MISS means they were computed from scratch */}
            <StatPill
              icon={Database}
              label="cache"
              value={stats.cache_hit ? 'HIT' : 'MISS'}
              // Green for cache hit (fast), amber for cache miss (slower)
              accent={stats.cache_hit ? 'text-emerald-400' : 'text-amber-400'}
            />
          </div>
        </div>

        {/* ---- Results grid ---- */}
        {/* auto-rows-fr makes all rows in a CSS grid equal height, which keeps
            cards aligned even when the first one spans multiple rows */}
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-4 auto-rows-fr">
          {results.map((r, i) => (
            // Key must be unique — we combine filename and index just in case
            // two results somehow have the same filename
            <ResultCard key={`${r.filename}-${i}`} result={r} index={i} />
          ))}
        </div>

      </div>
    </motion.section>
  )
}
