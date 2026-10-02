"""Minimal Hugging Face datasets-server client. Stdlib only, cached on disk."""

from __future__ import annotations

import json
import time
import urllib.parse
import urllib.request
from pathlib import Path

CACHE = Path(__file__).resolve().parent.parent.parent / "build" / "hf_cache"
API = "https://datasets-server.huggingface.co/rows"
PAGE = 100


def rows(dataset, config="default", split="train", limit=100000, sleep=0.05):
    """Yields dataset rows, paging through the datasets-server and caching each page."""
    CACHE.mkdir(parents=True, exist_ok=True)
    tag = dataset.replace("/", "__")
    offset = 0
    while offset < limit:
        cache_file = CACHE / f"{tag}.{config}.{split}.{offset}.json"
        if cache_file.exists():
            payload = json.loads(cache_file.read_text())
        else:
            url = API + "?" + urllib.parse.urlencode(
                {"dataset": dataset, "config": config, "split": split,
                 "offset": offset, "length": PAGE})
            with urllib.request.urlopen(url, timeout=60) as resp:
                payload = json.loads(resp.read().decode())
            cache_file.write_text(json.dumps(payload))
            time.sleep(sleep)
        batch = payload.get("rows", [])
        if not batch:
            return
        for item in batch:
            yield item["row"]
        offset += len(batch)
