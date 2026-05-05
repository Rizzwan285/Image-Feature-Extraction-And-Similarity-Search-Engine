//managing root application state and orchestrating components
import { useEffect, useState, useCallback } from 'react'

import { AnimatePresence } from 'framer-motion'

import Hero from './components/Hero.jsx'
import QueryPanel from './components/QueryPanel.jsx'
import DatasetModal from './components/DatasetModal.jsx'
import LoadingState from './components/LoadingState.jsx'
import ResultsGrid from './components/ResultsGrid.jsx'
import ComparisonGrid from './components/ComparisonGrid.jsx'
import EvaluationModal from './components/EvaluationModal.jsx'
import ArchitectureOverlay from './components/ArchitectureOverlay.jsx'

export default function App() {
  //initializing ui state variables
  const [dataset, setDataset] = useState([])
  const [datasetOpen, setDatasetOpen] = useState(false)
  const [archOpen, setArchOpen] = useState(false)
  const [query, setQuery] = useState(null)
  const [isSearching, setIsSearching] = useState(false)
  const [results, setResults] = useState(null)
  const [error, setError] = useState(null)
  const [metric, setMetric] = useState('euclidean')
  const [comparison, setComparison] = useState(null)
  const [evalOpen, setEvalOpen] = useState(false)

  //fetching dataset from server
  const refreshDataset = useCallback(async () => {
    try {
      const res = await fetch('/api/dataset')
      if (!res.ok) throw new Error(`HTTP ${res.status}`)
      const data = await res.json()
      setDataset(data.images || [])
    } catch (e) {
      console.error('failed to load dataset:', e)
      setError('Could not reach the backend. Is the FastAPI server running on port 8000?')
    }
  }, [])

  // Fetch the dataset once when the component first mounts (appears on screen)
  useEffect(() => {
    refreshDataset()
  }, [refreshDataset])

  //running similarity search
  const runSearch = useCallback(async () => {
    if (!query || isSearching) return

    setIsSearching(true)
    setError(null)
    setResults(null)
    setComparison(null)

    try {
      const res = await fetch('/api/search', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          query_filename: query.filename,
          top_k: 8,
          num_threads: 4,
          metric: metric,
        }),
      })

      if (!res.ok) {
        const detail = await res.text()
        throw new Error(detail || `HTTP ${res.status}`)
      }

      const data = await res.json()
      setResults(data)

    } catch (e) {
      console.error('search failed:', e)
      setError(`Search failed: ${e.message}`)
    } finally {
      setIsSearching(false)
    }
  }, [query, isSearching, metric])

  //running metrics comparison
  const runCompare = useCallback(async () => {
    if (!query || isSearching) return

    setIsSearching(true)
    setError(null)
    setResults(null)
    setComparison(null)

    try {
      const res = await fetch('/api/compare', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          query_filename: query.filename,
          top_k: 5,
          num_threads: 4,
        }),
      })

      if (!res.ok) {
        const detail = await res.text()
        throw new Error(detail || `HTTP ${res.status}`)
      }

      setComparison(await res.json())
    } catch (e) {
      console.error('compare failed:', e)
      setError(`Compare failed: ${e.message}`)
    } finally {
      setIsSearching(false)
    }
  }, [query, isSearching])

  //handling image upload
  const handleUpload = useCallback(async (file) => {
    setError(null)

    const fd = new FormData()
    fd.append('file', file)

    try {
      const res = await fetch('/api/upload', { method: 'POST', body: fd })
      if (!res.ok) {
        const detail = await res.text()
        throw new Error(detail || `HTTP ${res.status}`)
      }

      const data = await res.json()
      setQuery({ filename: data.filename, url: data.url })
      refreshDataset()
    } catch (e) {
      console.error('upload failed:', e)
      setError(`Upload failed: ${e.message}`)
    }
  }, [refreshDataset])

  const handlePickFromDataset = () => setDatasetOpen(true)

  const handleSelectFromDataset = (img) => {
    setQuery({ filename: img.filename, url: img.url })
    setResults(null)
    setComparison(null)
  }

  const handleClearQuery = () => {
    setQuery(null)
    setResults(null)
    setComparison(null)
  }

  //listening for keyboard shortcuts
  useEffect(() => {
    const onKey = (e) => {
      const tag = (e.target?.tagName || '').toLowerCase()
      if (tag === 'input' || tag === 'textarea' || e.metaKey || e.ctrlKey || e.altKey) return

      if (e.key === 'r' || e.key === 'R') {
        if (query && !isSearching) {
          e.preventDefault()
          runSearch()
        }
      }
    }

    window.addEventListener('keydown', onKey)

    return () => window.removeEventListener('keydown', onKey)
  }, [query, isSearching, runSearch])

  //rendering ui components
  return (
    <div className="min-h-screen">
      <Hero
        onShowArchitecture={() => setArchOpen(true)}
        onShowEvaluation={() => setEvalOpen(true)}
      />

      <QueryPanel
        query={query}
        onPickFromDataset={handlePickFromDataset}
        onUpload={handleUpload}
        onSearch={runSearch}
        onCompare={runCompare}
        onClearQuery={handleClearQuery}
        isSearching={isSearching}
        metric={metric}
        onMetricChange={setMetric}
      />

      <AnimatePresence mode="wait">
        {isSearching && <LoadingState key="loading" />}

        {!isSearching && comparison && (
          <ComparisonGrid key="compare" data={comparison} />
        )}

        {/* Otherwise show standard single-metric results */}
        {!isSearching && !comparison && results && (
          <ResultsGrid
            key="results"
            results={results.results}
            stats={results.stats}
          />
        )}
      </AnimatePresence>

      {error && (
        <div className="px-6 pb-10">
          <div className="max-w-6xl mx-auto p-4 rounded-xl border border-red-500/30 bg-red-500/10 text-sm text-red-200">
            {error}
          </div>
        </div>
      )}

      <DatasetModal
        open={datasetOpen}
        images={dataset}
        onClose={() => setDatasetOpen(false)}
        onSelect={handleSelectFromDataset}
      />

      <ArchitectureOverlay open={archOpen} onClose={() => setArchOpen(false)} />

      <EvaluationModal open={evalOpen} onClose={() => setEvalOpen(false)} />

      <footer className="px-6 py-8 text-center text-xs font-mono text-white/30">
        C Programming course project — image feature extraction & similarity search
      </footer>
    </div>
  )
}
