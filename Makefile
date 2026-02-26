.PHONY: help build install host upload start web service clean

help: ## Show this help
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-14s\033[0m %s\n", $$1, $$2}'

# ── Flipper ──────────────────────────────────

build: ## Build Flipper FAP
	cd flipper && ufbt build

launch: ## Build + deploy + launch on Flipper
	cd flipper && ufbt launch

upload: ## Upload config.txt to Flipper SD card
	cd host && python -m flipdeck_host.cli upload ../config/config.txt

# ── Host ─────────────────────────────────────

install: ## Install host (pip install)
	cd host && pip install -e .

host: ## Start host daemon (key listener)
	flipdeck start

web: ## Start host daemon + web UI
	flipdeck start --web

service: ## Install as background service (autostart)
	flipdeck install

# ── Dev ──────────────────────────────────────

clean: ## Clean build artifacts
	cd flipper && ufbt clean 2>/dev/null || true
	rm -rf host/*.egg-info host/dist host/build

all: build install ## Build FAP + install host
