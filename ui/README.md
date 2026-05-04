# UI Layer (React + Vite)

A single-page React app for uploading queries and viewing similarity results.

## Stack

- **Vite** — dev server and build tool
- **React 18**
- **Tailwind CSS** — utility-first styling, dark theme + glassmorphism
- **Framer Motion** — entrance animations and layout transitions
- **lucide-react** — icons

## Run it

```
cd ui
npm install
npm run dev
```

The dev server starts on port 5173 and proxies `/api/*` requests to the
FastAPI server on port 8000 (configured in `vite.config.js`).

For a production build:

```
npm run build
npm run preview
```

## Component tour

- **Hero** — gradient title, an architecture pill, and the small status
  indicator showing the request flow.
- **QueryPanel** — drag-and-drop upload + "pick from dataset" button. When
  a query is selected it transitions into a focused card with a search
  button.
- **DatasetModal** — overlay grid showing every image in the dataset for
  quick selection.
- **LoadingState** — animated spinner that cycles through phase labels
  ("Extracting features…", "Comparing across dataset…", etc.) while the
  search runs.
- **ResultsGrid** — responsive grid of `ResultCard`s plus a stats row
  showing elapsed milliseconds, threads used, dataset size, and cache
  hit/miss.
- **ResultCard** — single result with rank badge, similarity percentage,
  and an animated horizontal score bar. The #1 match gets a larger card
  with a gold "Best Match" badge.
- **ArchitectureOverlay** — full-screen overlay describing the four
  architectural layers and the request flow. Useful during the
  architecture-explanation segment of the presentation.

## Keyboard shortcut

Pressing `R` re-runs the last search without clicking the button.
