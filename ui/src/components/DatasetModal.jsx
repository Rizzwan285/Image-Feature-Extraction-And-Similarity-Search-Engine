// DatasetModal.jsx
// ----------------
// A full-screen overlay that shows all the images in the dataset as a grid.
// The user clicks one to select it as the query image. Clicking outside the
// modal (the dark backdrop) or the X button closes it without selecting anything.
//
// Props:
//   open     — whether the modal is visible
//   images   — array of image metadata objects from /api/dataset
//   onClose  — called when the user dismisses the modal
//   onSelect — called with the chosen image object when the user picks one

// motion and AnimatePresence for entrance/exit animations
import { motion, AnimatePresence } from 'framer-motion'

// X icon for the close button
import { X } from 'lucide-react'

export default function DatasetModal({ open, images, onClose, onSelect }) {
  return (
    // AnimatePresence watches when `open` changes and animates the modal in/out
    <AnimatePresence>
      {open && (
        // The full-screen backdrop — clicking it closes the modal
        <motion.div
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          transition={{ duration: 0.2 }}
          className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-ink-950/70 backdrop-blur-md"
          onClick={onClose}    // clicking the dark area behind the modal closes it
        >
          {/* The actual modal panel — clicking inside it doesn't close the modal */}
          <motion.div
            // Scale up slightly and fade in for a "pop" entrance effect
            initial={{ scale: 0.96, opacity: 0, y: 12 }}
            animate={{ scale: 1, opacity: 1, y: 0 }}
            exit={{ scale: 0.96, opacity: 0, y: 12 }}
            transition={{ duration: 0.25, ease: [0.22, 1, 0.36, 1] }}
            // stopPropagation prevents clicks inside the modal from bubbling up
            // to the backdrop (which would close the modal)
            onClick={(e) => e.stopPropagation()}
            className="glass-strong w-full max-w-5xl max-h-[85vh] rounded-3xl overflow-hidden flex flex-col"
          >
            {/* Modal header: title + close button */}
            <div className="flex items-center justify-between px-6 py-4 border-b border-white/5">
              <div>
                <div className="font-display font-semibold text-xl">Choose a query image</div>
                {/* Show how many images are available */}
                <div className="text-xs text-white/50 mt-0.5">{images.length} images in dataset</div>
              </div>
              {/* Close button in the top-right corner */}
              <button
                onClick={onClose}
                className="p-2 rounded-lg text-white/60 hover:text-white hover:bg-white/5 transition-colors"
              >
                <X size={20} />
              </button>
            </div>

            {/* Scrollable grid of images */}
            <div className="overflow-y-auto p-6">
              {/* Responsive grid: 2 columns on phones, up to 5 on wide screens */}
              <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 gap-4">
                {images.map((img, idx) => (
                  <motion.button
                    key={img.filename}
                    // Stagger the entrance: each image appears slightly later than the previous
                    initial={{ opacity: 0, y: 10 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.3, delay: idx * 0.025 }}
                    // Lift slightly on hover for a tactile feel
                    whileHover={{ y: -4 }}
                    // Selecting an image calls both onSelect (to set the query) and
                    // onClose (to dismiss the modal)
                    onClick={() => { onSelect(img); onClose() }}
                    className="group relative rounded-xl overflow-hidden aspect-square bg-ink-800 ring-1 ring-white/5 hover:ring-accent-400/60 transition-all"
                  >
                    {/* The image thumbnail */}
                    <img
                      src={img.url}
                      alt={img.filename}
                      className="w-full h-full object-cover transition-transform group-hover:scale-105"
                    />
                    {/* Filename label that slides up on hover */}
                    <div className="absolute inset-x-0 bottom-0 p-2 bg-gradient-to-t from-black/80 to-transparent opacity-0 group-hover:opacity-100 transition-opacity">
                      <div className="text-xs font-mono text-white/90 truncate">
                        {img.stem}  {/* show name without extension for cleanliness */}
                      </div>
                    </div>
                  </motion.button>
                ))}
              </div>
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  )
}
