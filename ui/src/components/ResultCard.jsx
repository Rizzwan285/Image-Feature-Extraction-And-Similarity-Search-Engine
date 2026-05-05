//displaying single result card component
import { useEffect, useState } from 'react'

import { motion } from 'framer-motion'

import { Trophy } from 'lucide-react'

export default function ResultCard({ result, index }) {
  const [animatedScore, setAnimatedScore] = useState(0)

  const isBest = result.rank === 1

  useEffect(() => {
    const timeout = setTimeout(() => {
      setAnimatedScore(result.similarity)
    }, 100 + index * 80)

    return () => clearTimeout(timeout)
  }, [result.similarity, index])

  const pct = Math.round(result.similarity * 100)

  return (
    <motion.div
      initial={{ opacity: 0, y: 16, scale: 0.96 }}
      animate={{ opacity: 1, y: 0, scale: 1 }}
      transition={{
        duration: 0.5,
        delay: index * 0.06,
        ease: [0.22, 1, 0.36, 1],
      }}
      whileHover={{ y: -4 }}
      className={`group relative rounded-2xl overflow-hidden glass transition-all
        ${isBest
          ? 'md:col-span-2 md:row-span-2 ring-2 ring-amber-300/50 shadow-2xl shadow-amber-400/20'
          : 'hover:ring-1 hover:ring-accent-400/40'}
      `}
    >
      <div className={`relative ${isBest ? 'aspect-[16/10]' : 'aspect-[4/3]'} bg-ink-800 overflow-hidden`}>
        <img
          src={result.url}
          alt={result.original_filename}
          className="w-full h-full object-cover transition-transform duration-500 group-hover:scale-105"
        />

        <div className="absolute top-3 left-3 flex items-center gap-2">
          <div className={`flex items-center gap-1.5 px-2.5 py-1 rounded-full font-mono text-xs font-bold
            ${isBest
              ? 'bg-gradient-to-r from-amber-300 to-amber-500 text-ink-950'
              : 'bg-ink-950/70 backdrop-blur-sm text-white/90'}
          `}>
            {isBest && <Trophy size={12} />}
            <span>#{result.rank}</span>
          </div>

          {isBest && (
            <div className="px-2 py-1 rounded-full text-[10px] font-mono uppercase tracking-wider bg-ink-950/70 backdrop-blur-sm text-amber-200">
              Best Match
            </div>
          )}
        </div>

        <div className="absolute top-3 right-3 px-2.5 py-1 rounded-full bg-ink-950/70 backdrop-blur-sm font-mono text-xs">
          <span className={isBest ? 'text-amber-200' : 'text-accent-200'}>
            {pct}%
          </span>
        </div>
      </div>

      <div className="p-4">
        <div className="text-sm font-medium text-white/90 truncate mb-2">
          {result.original_filename}
        </div>

        <div className="relative h-1.5 rounded-full bg-white/5 overflow-hidden mb-2">
          <motion.div
            initial={{ width: 0 }}
            animate={{ width: `${animatedScore * 100}%` }}
            transition={{ duration: 0.7, ease: [0.22, 1, 0.36, 1] }}
            className={`h-full rounded-full ${isBest
              ? 'bg-gradient-to-r from-amber-300 to-amber-500'
              : 'bg-gradient-to-r from-accent-400 to-magenta-400'}`}
          />
        </div>

        <div className="flex items-center justify-between text-[11px] font-mono text-white/40">
          <span>distance {result.distance.toFixed(4)}</span>
          <span>similarity</span>
        </div>
      </div>
    </motion.div>
  )
}
