// QueryPanel.jsx
// --------------
// This component handles everything related to choosing a query image.
// It has two visual states:
//
//   1. No query selected: shows a drag-and-drop upload zone on the left
//      and a "pick from dataset" button on the right.
//
//   2. Query selected: shows the chosen image in a card with its name,
//      and a "Search Similar Images" button.
//
// Props:
//   query             — the selected query { filename, url }, or null
//   onPickFromDataset — called when the user clicks "pick from dataset"
//   onUpload          — called with a File object when the user drops/selects a file
//   onSearch          — called when the user clicks "Search Similar Images"
//   onClearQuery      — called when the user clicks "choose different image"
//   isSearching       — disables buttons while a search is running
//   metric            — current distance metric ("euclidean"|"manhattan"|"cosine")
//   onMetricChange    — called with the new metric name when the dropdown changes

// useState tracks local UI state (isDragging) that doesn't need to live in App.jsx
// useRef gives us a reference to the hidden file input element
import { useState, useRef } from 'react'

// motion for animations, AnimatePresence so the two states can cross-fade
import { motion, AnimatePresence } from 'framer-motion'

// Icons
import { Upload, Image as ImageIcon, Search, X } from 'lucide-react'

export default function QueryPanel({
  query,
  onPickFromDataset,
  onUpload,
  onSearch,
  onClearQuery,
  isSearching,
  metric,
  onMetricChange,
}) {
  // isDragging: true while the user is hovering a file over the drop zone.
  // We use this to highlight the drop zone with a brighter border.
  const [isDragging, setIsDragging] = useState(false)

  // A reference to the hidden <input type="file"> element.
  // We use this to trigger the browser's file picker when the user clicks the zone.
  const fileInputRef = useRef(null)

  // Called when the user drops a file onto the drop zone
  const handleDrop = (e) => {
    e.preventDefault()         // prevent the browser from navigating to the file
    setIsDragging(false)       // remove the drag highlight
    const file = e.dataTransfer.files?.[0]   // grab the first dropped file
    if (file) onUpload(file)   // pass it up to App.jsx
  }

  // Called when the user selects a file through the native file picker dialog
  const handleFileSelect = (e) => {
    const file = e.target.files?.[0]
    if (file) onUpload(file)
    // Reset the input so the user can pick the same file again later if they want
    e.target.value = ''
  }

  return (
    <section className="px-6 pb-10">
      <div className="max-w-6xl mx-auto">

        {/* AnimatePresence allows the two states (query selected / not selected)
            to animate when switching. mode="wait" means the exiting component
            fully leaves before the entering one appears. */}
        <AnimatePresence mode="wait">

          {/* ---- STATE 1: Query has been selected ---- */}
          {query ? (
            <motion.div
              key="query-selected"
              initial={{ opacity: 0, y: 12 }}
              animate={{ opacity: 1, y: 0 }}
              exit={{ opacity: 0, y: -12 }}
              transition={{ duration: 0.35, ease: [0.22, 1, 0.36, 1] }}
              className="glass-strong rounded-3xl p-6 md:p-8"
            >
              {/* Flex row on desktop, column on mobile */}
              <div className="flex flex-col md:flex-row items-start md:items-center gap-6">

                {/* Thumbnail of the selected query image */}
                <motion.div
                  // layoutId connects this thumbnail to the one in the dataset modal,
                  // so Framer Motion can animate it smoothly between the two positions
                  layoutId={`thumb-${query.filename}`}
                  className="relative w-40 h-40 md:w-48 md:h-48 rounded-2xl overflow-hidden flex-shrink-0 ring-2 ring-accent-400/40 shadow-lg shadow-accent-500/20"
                >
                  <img
                    src={query.url}
                    alt={query.filename}
                    className="w-full h-full object-cover"
                  />
                  {/* Small "Query" label overlaid on the image */}
                  <div className="absolute top-2 left-2 bg-black/60 backdrop-blur-sm text-[10px] font-mono uppercase tracking-wider text-accent-200 px-2 py-1 rounded">
                    Query
                  </div>
                </motion.div>

                {/* Text info and action buttons */}
                <div className="flex-1 min-w-0">
                  <div className="text-xs font-mono uppercase tracking-widest text-white/40 mb-2">
                    Selected query image
                  </div>
                  {/* truncate prevents long filenames from breaking the layout */}
                  <div className="font-display font-semibold text-2xl mb-1 truncate">
                    {query.filename}
                  </div>
                  <div className="text-sm text-white/50 mb-6">
                    Ready to search across the dataset using parallel feature extraction.
                  </div>

                  {/* Action buttons */}
                  <div className="flex flex-wrap items-center gap-3">

                    {/* Primary search button — gradient background, subtle glow */}
                    <motion.button
                      whileHover={{ scale: isSearching ? 1 : 1.02 }}
                      whileTap={{ scale: isSearching ? 1 : 0.98 }}
                      onClick={onSearch}
                      disabled={isSearching}   // can't click while a search is running
                      className="group relative px-6 py-3 rounded-xl font-medium text-ink-950 bg-gradient-to-r from-accent-300 to-magenta-400 disabled:opacity-50 disabled:cursor-not-allowed shadow-lg shadow-accent-500/30 hover:shadow-accent-500/50 transition-shadow flex items-center gap-2"
                    >
                      <Search size={18} />
                      {/* Button label changes while searching */}
                      <span>{isSearching ? 'Searching…' : 'Search Similar Images'}</span>
                    </motion.button>

                    {/* Distance-metric selector — picks which C function pointer
                        the algorithm layer dispatches to for this search */}
                    <label className="flex items-center gap-2 text-sm">
                      <span className="text-xs font-mono uppercase tracking-widest text-white/40">
                        Metric
                      </span>
                      <select
                        value={metric}
                        onChange={(e) => onMetricChange?.(e.target.value)}
                        disabled={isSearching}
                        className="px-3 py-2 rounded-xl bg-white/5 border border-white/10 text-white/90 text-sm hover:bg-white/10 focus:outline-none focus:ring-2 focus:ring-accent-400/40 disabled:opacity-50 transition-colors"
                      >
                        <option value="euclidean" className="bg-ink-950">Euclidean (L2)</option>
                        <option value="manhattan" className="bg-ink-950">Manhattan (L1)</option>
                        <option value="cosine" className="bg-ink-950">Cosine</option>
                      </select>
                    </label>

                    {/* Secondary button to go back and pick a different image */}
                    <button
                      onClick={onClearQuery}
                      disabled={isSearching}
                      className="px-4 py-3 rounded-xl text-white/70 hover:text-white hover:bg-white/5 disabled:opacity-50 transition-colors flex items-center gap-2 text-sm"
                    >
                      <X size={16} />
                      <span>Choose different image</span>
                    </button>

                    {/* Keyboard shortcut hint — only shows on large screens */}
                    <div className="ml-auto text-xs font-mono text-white/40 hidden lg:block">
                      Press <kbd className="px-1.5 py-0.5 rounded bg-white/10 text-white/70">R</kbd> to repeat last search
                    </div>

                  </div>
                </div>
              </div>
            </motion.div>

          ) : (
            /* ---- STATE 2: No query selected yet ---- */
            <motion.div
              key="query-pick"
              initial={{ opacity: 0, y: 12 }}
              animate={{ opacity: 1, y: 0 }}
              exit={{ opacity: 0, y: -12 }}
              transition={{ duration: 0.35, ease: [0.22, 1, 0.36, 1] }}
              className="grid md:grid-cols-2 gap-4"
            >

              {/* Left card: drag-and-drop upload zone */}
              <div
                // Drag events: highlight zone on enter, remove on leave, handle drop
                onDragOver={(e) => { e.preventDefault(); setIsDragging(true) }}
                onDragLeave={() => setIsDragging(false)}
                onDrop={handleDrop}
                // Clicking anywhere in this zone triggers the file input
                onClick={() => fileInputRef.current?.click()}
                className={`relative cursor-pointer rounded-3xl p-8 md:p-10 transition-all duration-300 group
                  ${isDragging
                    ? 'glass-strong border-accent-400/60 ring-2 ring-accent-400/40'
                    : 'glass hover:border-white/15'}
                `}
              >
                {/* Hidden file input — only visible to the OS file picker */}
                <input
                  ref={fileInputRef}
                  type="file"
                  accept="image/png,image/jpeg,image/jpg,image/bmp,image/webp"
                  className="hidden"
                  onChange={handleFileSelect}
                />

                {/* Centered icon and text */}
                <div className="flex flex-col items-center justify-center text-center min-h-[180px]">
                  {/* Icon box — scales up slightly when dragging */}
                  <div className={`p-4 rounded-2xl mb-4 transition-transform ${isDragging ? 'scale-110 bg-accent-500/20' : 'bg-white/5 group-hover:scale-105'}`}>
                    <Upload size={28} className="text-accent-300" />
                  </div>
                  <div className="font-display font-semibold text-lg mb-1">
                    {isDragging ? 'Drop to upload' : 'Drop an image here'}
                  </div>
                  <div className="text-sm text-white/50">
                    or click to browse — PNG, JPG, WebP
                  </div>
                </div>
              </div>

              {/* Right card: pick from dataset button */}
              <button
                onClick={onPickFromDataset}
                className="relative cursor-pointer rounded-3xl p-8 md:p-10 transition-all duration-300 group glass hover:border-white/15 text-left"
              >
                <div className="flex flex-col items-center justify-center text-center min-h-[180px]">
                  <div className="p-4 rounded-2xl mb-4 bg-white/5 group-hover:scale-105 transition-transform">
                    <ImageIcon size={28} className="text-magenta-400" />
                  </div>
                  <div className="font-display font-semibold text-lg mb-1">
                    Pick from dataset
                  </div>
                  <div className="text-sm text-white/50">
                    browse the loaded image collection
                  </div>
                </div>
              </button>

            </motion.div>
          )}
        </AnimatePresence>

      </div>
    </section>
  )
}
