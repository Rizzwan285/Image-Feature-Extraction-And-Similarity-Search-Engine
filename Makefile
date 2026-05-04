# Top-level Makefile
#
# Common workflow:
#   make install   # install Python and Node dependencies
#   make           # build the C binary
#   make run       # start the FastAPI server and the Vite UI
#   make demo      # build, generate sample data, and start everything
#   make clean     # remove build artifacts and caches

.PHONY: all algorithm install run run-api run-ui demo clean clean-cache sample-data ppm

all: algorithm

# --- Build the C algorithm layer ---
algorithm:
	$(MAKE) -C algorithm

# --- Install Python and Node dependencies ---
install:
	@echo "==> installing python deps"
	pip install -r communication/requirements.txt --break-system-packages 2>/dev/null || \
		pip install -r communication/requirements.txt
	@echo "==> installing node deps"
	cd ui && npm install --no-fund --no-audit

# --- Generate the sample dataset (PNGs + PPM conversions) ---
sample-data:
	@echo "==> generating sample images"
	python3 scripts/generate_sample_images.py data/images
	@echo "==> converting to PPM"
	python3 scripts/convert_to_ppm.py data/images data/ppm

ppm:
	python3 scripts/convert_to_ppm.py data/images data/ppm

# --- Start the API and UI together (best for the demo) ---
run: algorithm
	@echo "==> starting FastAPI on :8000 and Vite on :5173"
	@echo "    open http://localhost:5173 in your browser"
	@( cd communication && python3 -m uvicorn server:app --host 0.0.0.0 --port 8000 ) & \
	  ( cd ui && npm run dev -- --port 5173 --host ); \
	  wait

run-api: algorithm
	cd communication && python3 -m uvicorn server:app --host 0.0.0.0 --port 8000

run-ui:
	cd ui && npm run dev -- --port 5173 --host

# --- One-command demo: build + sample data + run ---
demo: algorithm sample-data run

# --- Cleanup ---
clean:
	$(MAKE) -C algorithm clean
	rm -rf ui/node_modules ui/dist
	rm -rf data/ppm data/features.cache data/output.json
	find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
	@echo "cleaned."

clean-cache:
	rm -f data/features.cache data/output.json
	@echo "cache cleared."
