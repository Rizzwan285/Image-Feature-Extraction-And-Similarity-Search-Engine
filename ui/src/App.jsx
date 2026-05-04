// App.jsx
// -------
// This is the root component of the entire React app. Every other component
// lives inside this one. It owns all the "important" state — things like
// which image is selected as the query, whether a search is running, and
// what the results look like — and passes pieces of that state down to
// child components as props.
//
// State that lives here:
//   dataset      — the list of images in data/images/ (fetched on load)
//   datasetOpen  — whether the "pick from dataset" modal is visible
//   archOpen     — whether the architecture overlay is visible
//   query        — the currently selected query image { filename, url }
//   isSearching  — true while the C binary is running (shows loading spinner)
//   results      — the search results from the last successful search
//   error        — any error message to show to the user

// useEffect runs side-effects (like fetching data) after the component renders
// useState stores values that should trigger a re-render when they change
// useCallback memoizes functions so they don't get recreated on every render
import { useEffect, useState, useCallback } from 'react'

// AnimatePresence from Framer Motion allows components to animate when they
// are removed from the React tree (not just when they're added)
import { AnimatePresence } from 'framer-motion'

// Child components — each one handles one section of the page
import Hero from './components/Hero.jsx'
import QueryPanel from './components/QueryPanel.jsx'
import DatasetModal from './components/DatasetModal.jsx'
import LoadingState from './components/LoadingState.jsx'
import ResultsGrid from './components/ResultsGrid.jsx'
import ArchitectureOverlay from './components/ArchitectureOverlay.jsx'

export default function App() {
  // dataset: array of image metadata objects from /api/dataset
  const [dataset, setDataset] = useState([])

  // datasetOpen: controls whether the "pick from dataset" modal is showing
  const [datasetOpen, setDatasetOpen] = useState(false)

  // archOpen: controls whether the architecture overlay is showing
  const [archOpen, setArchOpen] = useState(false)

  // query: the currently selected query image, or null if nothing is selected
  // Shape: { filename: "04_ocean.png", url: "/api/image/04_ocean.png" }
  const [query, setQuery] = useState(null)

  // isSearching: true from the moment we send the search request until we
  // get a response back. Shows the loading spinner and disables the search button.
  const [isSearching, setIsSearching] = useState(false)

  // results: the full JSON response from /api/search, or null before first search
  // Shape: { results: [...], stats: {...}, query_info: {...} }
  const [results, setResults] = useState(null)

  // error: a human-readable error string to show in the red error box, or null
  const [error, setError] = useState(null)

  // metric: which distance metric the C binary should use for the next search.
  // The C layer dispatches to a different function pointer based on this name
  // (see similarity.c → pick_distance_function). Defaults to "euclidean" so
  // behaviour matches what users have always seen on first load.
  const [metric, setMetric] = useState('euclidean')

  // ---------------------------------------------------------------------------
  // refreshDataset — fetches the current dataset from the server
  // ---------------------------------------------------------------------------
  // useCallback with [] means this function is created once and never recreated.
  // We wrap it in useCallback because we pass it to useEffect below, and React
  // would warn about a missing dependency if we didn't memoize it.
  const refreshDataset = useCallback(async () => {
    try {
      // GET /api/dataset — returns { images: [...] }
      const res = await fetch('/api/dataset')
      if (!res.ok) throw new Error(`HTTP ${res.status}`)
      const data = await res.json()
      setDataset(data.images || [])   // update state with the image list
    } catch (e) {
      console.error('failed to load dataset:', e)
      // Show a helpful message — the most common cause is forgetting to start the API
      setError('Could not reach the backend. Is the FastAPI server running on port 8000?')
    }
  }, [])

  // Fetch the dataset once when the component first mounts (appears on screen)
  useEffect(() => {
    refreshDataset()
  }, [refreshDataset])

  // ---------------------------------------------------------------------------
  // runSearch — sends the search request to the server
  // ---------------------------------------------------------------------------
  const runSearch = useCallback(async () => {
    // Do nothing if we don't have a query selected, or if a search is already running
    if (!query || isSearching) return

    setIsSearching(true)    // show the loading spinner
    setError(null)          // clear any previous error
    setResults(null)        // clear previous results while new ones load

    try {
      // POST /api/search with the query filename and parameters
      const res = await fetch('/api/search', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          query_filename: query.filename,
          top_k: 8,           // ask for top 8 matches
          num_threads: 4,     // use 4 worker threads in C
          metric: metric,     // distance metric the C binary should use
        }),
      })

      if (!res.ok) {
        const detail = await res.text()
        throw new Error(detail || `HTTP ${res.status}`)
      }

      const data = await res.json()
      setResults(data)    // store the results — this triggers the ResultsGrid to render

    } catch (e) {
      console.error('search failed:', e)
      setError(`Search failed: ${e.message}`)
    } finally {
      // Always hide the loading spinner, whether we succeeded or failed
      setIsSearching(false)
    }
  }, [query, isSearching, metric])  // re-create this function if query, isSearching, or metric changes

  // ---------------------------------------------------------------------------
  // handleUpload — sends an uploaded file to the server
  // ---------------------------------------------------------------------------
  const handleUpload = useCallback(async (file) => {
    setError(null)

    // FormData is the browser's way of packaging a file upload for a POST request
    const fd = new FormData()
    fd.append('file', file)

    try {
      // POST /api/upload — server saves the file and converts it to PPM
      const res = await fetch('/api/upload', { method: 'POST', body: fd })
      if (!res.ok) {
        const detail = await res.text()
        throw new Error(detail || `HTTP ${res.status}`)
      }

      const data = await res.json()
      // Set the uploaded image as the query automatically
      setQuery({ filename: data.filename, url: data.url })
      // Also refresh the dataset list so the new image appears in the modal
      refreshDataset()
    } catch (e) {
      console.error('upload failed:', e)
      setError(`Upload failed: ${e.message}`)
    }
  }, [refreshDataset])

  // Simple event handlers — these just update state

  // Open the dataset picker modal
  const handlePickFromDataset = () => setDatasetOpen(true)

  // Called when the user clicks an image in the dataset modal
  const handleSelectFromDataset = (img) => {
    setQuery({ filename: img.filename, url: img.url })
    setResults(null)    // clear old results when a new query is picked
  }

  // Clear the query and go back to the upload / pick interface
  const handleClearQuery = () => {
    setQuery(null)
    setResults(null)
  }

  // ---------------------------------------------------------------------------
  // Keyboard shortcut: pressing R re-runs the last search
  // ---------------------------------------------------------------------------
  useEffect(() => {
    const onKey = (e) => {
      // Ignore keypresses when the user is typing in an input or text area
      const tag = (e.target?.tagName || '').toLowerCase()
      if (tag === 'input' || tag === 'textarea' || e.metaKey || e.ctrlKey || e.altKey) return

      if (e.key === 'r' || e.key === 'R') {
        // Only do something if we have a query and aren't already searching
        if (query && !isSearching) {
          e.preventDefault()    // don't let the browser do its default 'R' action
          runSearch()
        }
      }
    }

    // Add the listener to the whole window
    window.addEventListener('keydown', onKey)

    // Cleanup: remove the listener when this component unmounts or when the
    // dependencies change. Without this, old listeners would pile up.
    return () => window.removeEventListener('keydown', onKey)
  }, [query, isSearching, runSearch])  // re-run this effect if any of these change

  // ---------------------------------------------------------------------------
  // Render — put everything on screen
  // ---------------------------------------------------------------------------
  return (
    <div className="min-h-screen">
      {/* Hero: the big title at the top + the architecture button */}
      <Hero onShowArchitecture={() => setArchOpen(true)} />

      {/* QueryPanel: upload zone and "pick from dataset" button, or the
          selected-query card with the search button */}
      <QueryPanel
        query={query}
        onPickFromDataset={handlePickFromDataset}
        onUpload={handleUpload}
        onSearch={runSearch}
        onClearQuery={handleClearQuery}
        isSearching={isSearching}
        metric={metric}
        onMetricChange={setMetric}
      />

      {/* AnimatePresence lets Framer Motion animate components when they
          leave the DOM — so the loading state fades out and results fade in
          rather than just appearing/disappearing abruptly */}
      <AnimatePresence mode="wait">
        {/* Show the loading spinner while search is in progress */}
        {isSearching && <LoadingState key="loading" />}

        {/* Show results once we have them and the search is done */}
        {!isSearching && results && (
          <ResultsGrid
            key="results"
            results={results.results}
            stats={results.stats}
          />
        )}
      </AnimatePresence>

      {/* Error message — only visible when something went wrong */}
      {error && (
        <div className="px-6 pb-10">
          <div className="max-w-6xl mx-auto p-4 rounded-xl border border-red-500/30 bg-red-500/10 text-sm text-red-200">
            {error}
          </div>
        </div>
      )}

      {/* Dataset picker modal — renders on top of everything when open */}
      <DatasetModal
        open={datasetOpen}
        images={dataset}
        onClose={() => setDatasetOpen(false)}
        onSelect={handleSelectFromDataset}
      />

      {/* Architecture overlay — explains the four-layer structure */}
      <ArchitectureOverlay open={archOpen} onClose={() => setArchOpen(false)} />

      {/* Footer */}
      <footer className="px-6 py-8 text-center text-xs font-mono text-white/30">
        C Programming course project — image feature extraction & similarity search
      </footer>
    </div>
  )
}
