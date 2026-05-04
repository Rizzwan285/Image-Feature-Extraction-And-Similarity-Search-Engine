// ResultCard.jsx
// --------------
// Renders one search result — an image card with its rank badge, similarity
// percentage, an animated score bar, and the raw distance value.
//
// The #1 ranked result (isBest) gets special treatment:
//   - Larger aspect ratio (spans 2 columns and 2 rows on desktop)
//   - Gold color scheme instead of cyan/magenta
//   - A trophy icon and "Best Match" badge
//
// Props:
//   result — one entry from the results array:
//              { rank, filename, distance, original_filename, url, similarity }
//   index  — the position of this card in the list (used to stagger animations)

// useEffect and useState for the animated score bar
import { useEffect, useState } from 'react'

// motion for the card entrance animation and score bar animation
import { motion } from 'framer-motion'

// Trophy icon for the best match badge
import { Trophy } from 'lucide-react'

export default function ResultCard({ result, index }) {
  // The animated score bar starts at 0 and grows to the real similarity value.
  // We store the current animation target value here.
  const [animatedScore, setAnimatedScore] = useState(0)

  // Is this the top result? Used to apply gold styling.
  const isBest = result.rank === 1

  // After a short staggered delay (so cards don't all animate at once),
  // set animatedScore to the real value. Framer Motion's motion.div will
  // then animate the bar width smoothly from 0 to that value.
  useEffect(() => {
    const timeout = setTimeout(() => {
      setAnimatedScore(result.similarity)
    }, 100 + index * 80)    // index * 80ms stagger: card 0 at 100ms, card 1 at 180ms, etc.

    // Cleanup: cancel the timeout if the component unmounts before it fires
    return () => clearTimeout(timeout)
  }, [result.similarity, index])

  // Convert the 0..1 similarity to a percentage for display, e.g. 0.854 → 85%
  const pct = Math.round(result.similarity * 100)

  return (
    // The card itself — animates in from slightly below with a fade
    <motion.div
      initial={{ opacity: 0, y: 16, scale: 0.96 }}
      animate={{ opacity: 1, y: 0, scale: 1 }}
      transition={{
        duration: 0.5,
        delay: index * 0.06,          // stagger: each card starts 60ms after the previous
        ease: [0.22, 1, 0.36, 1],     // custom easing curve for a snappy, springy feel
      }}
      whileHover={{ y: -4 }}          // lift up slightly on hover
      className={`group relative rounded-2xl overflow-hidden glass transition-all
        ${isBest
          // Best match: wider, taller, gold ring and glow
          ? 'md:col-span-2 md:row-span-2 ring-2 ring-amber-300/50 shadow-2xl shadow-amber-400/20'
          // Other results: subtle hover ring
          : 'hover:ring-1 hover:ring-accent-400/40'}
      `}
    >
      {/* ---- Image area ---- */}
      {/* Best match uses a wider aspect ratio to make it visually dominant */}
      <div className={`relative ${isBest ? 'aspect-[16/10]' : 'aspect-[4/3]'} bg-ink-800 overflow-hidden`}>
        <img
          src={result.url}
          alt={result.original_filename}
          // Subtle zoom on hover — the overflow:hidden on the parent clips it cleanly
          className="w-full h-full object-cover transition-transform duration-500 group-hover:scale-105"
        />

        {/* Top-left rank badge (and "Best Match" label for rank 1) */}
        <div className="absolute top-3 left-3 flex items-center gap-2">
          {/* The rank pill — gold gradient for #1, dark glass for others */}
          <div className={`flex items-center gap-1.5 px-2.5 py-1 rounded-full font-mono text-xs font-bold
            ${isBest
              ? 'bg-gradient-to-r from-amber-300 to-amber-500 text-ink-950'
              : 'bg-ink-950/70 backdrop-blur-sm text-white/90'}
          `}>
            {/* Only show the trophy icon on the best match */}
            {isBest && <Trophy size={12} />}
            <span>#{result.rank}</span>
          </div>

          {/* "Best Match" label — only on rank 1 */}
          {isBest && (
            <div className="px-2 py-1 rounded-full text-[10px] font-mono uppercase tracking-wider bg-ink-950/70 backdrop-blur-sm text-amber-200">
              Best Match
            </div>
          )}
        </div>

        {/* Top-right similarity percentage badge */}
        <div className="absolute top-3 right-3 px-2.5 py-1 rounded-full bg-ink-950/70 backdrop-blur-sm font-mono text-xs">
          {/* Gold for best match, cyan for others */}
          <span className={isBest ? 'text-amber-200' : 'text-accent-200'}>
            {pct}%
          </span>
        </div>
      </div>

      {/* ---- Text and score bar area (below the image) ---- */}
      <div className="p-4">
        {/* Filename — truncate if too long */}
        <div className="text-sm font-medium text-white/90 truncate mb-2">
          {result.original_filename}
        </div>

        {/* Animated score bar — grows from 0 to the similarity percentage */}
        <div className="relative h-1.5 rounded-full bg-white/5 overflow-hidden mb-2">
          <motion.div
            initial={{ width: 0 }}                          // start at 0 width
            animate={{ width: `${animatedScore * 100}%` }}  // animate to final width
            transition={{ duration: 0.7, ease: [0.22, 1, 0.36, 1] }}
            className={`h-full rounded-full ${isBest
              ? 'bg-gradient-to-r from-amber-300 to-amber-500'   // gold for best
              : 'bg-gradient-to-r from-accent-400 to-magenta-400'}`}  // cyan→pink for others
          />
        </div>

        {/* Metadata row: raw distance on the left, "similarity" label on the right */}
        <div className="flex items-center justify-between text-[11px] font-mono text-white/40">
          {/* The actual distance value from the C algorithm, shown to 4 decimal places */}
          <span>distance {result.distance.toFixed(4)}</span>
          <span>similarity</span>
        </div>
      </div>
    </motion.div>
  )
}
