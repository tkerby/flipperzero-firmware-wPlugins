"""Train the composer on melodies written by teacher.py.

usage: python train.py [--per-style N] [--epochs N] [--out composer.pt]
"""
import argparse
import random
import time

import torch
import torch.nn.functional as F

import teacher
from model import BAR, D_BOS, DURATIONS, P_BOS, STYLES, Composer, pitch_index


def encode(melody):
    """Teacher melody -> per-step inputs and targets."""
    prev_p, prev_d, beat, bar, next_p, next_d = [P_BOS], [D_BOS], [], [], [], []
    pos = 0
    for pitch, dur in melody:
        beat.append(pos % BAR)
        bar.append(pos // BAR)
        next_p.append(pitch_index(pitch))
        next_d.append(DURATIONS.index(dur))
        prev_p.append(next_p[-1])
        prev_d.append(next_d[-1])
        pos += dur
    return prev_p[:-1], prev_d[:-1], beat, bar, next_p, next_d


def dataset(per_style, seed):
    rng = random.Random(seed)
    items = []
    for s, style in enumerate(STYLES):
        for _ in range(per_style):
            items.append((s, encode(teacher.melody(style, rng))))
    rng.shuffle(items)
    return items


def batches(items, size, device):
    for i in range(0, len(items), size):
        chunk = items[i:i + size]
        length = max(len(seq[0]) for _, seq in chunk)
        fields = [torch.zeros(len(chunk), length, dtype=torch.long) for _ in range(6)]
        mask = torch.zeros(len(chunk), length, dtype=torch.bool)
        for b, (_, seq) in enumerate(chunk):
            n = len(seq[0])
            for f, values in zip(fields, seq):
                f[b, :n] = torch.tensor(values)
            mask[b, :n] = True
        style = torch.tensor([s for s, _ in chunk])
        yield [f.to(device) for f in fields], mask.to(device), style.to(device)


def loss_on(model, items, device, size=512):
    model.eval()
    total, count = 0.0, 0
    with torch.no_grad():
        for (pp, pd, beat, bar, np_, nd), mask, style in batches(items, size, device):
            lp, ld, _ = model(pp, pd, beat, bar, style, np_)
            l = F.cross_entropy(lp[mask], np_[mask], reduction="sum") + F.cross_entropy(ld[mask], nd[mask], reduction="sum")
            total += l.item()
            count += mask.sum().item()
    model.train()
    return total / count


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--per-style", type=int, default=8000)
    ap.add_argument("--epochs", type=int, default=12)
    ap.add_argument("--out", default="composer.pt")
    a = ap.parse_args()
    torch.manual_seed(0)
    device = "mps" if torch.backends.mps.is_available() else "cpu"
    train_items = dataset(a.per_style, seed=1)
    val_items = dataset(max(200, a.per_style // 20), seed=99)
    model = Composer().to(device)
    params = sum(p.numel() for p in model.parameters())
    print(f"device {device} | params {params} | train melodies {len(train_items)}")
    opt = torch.optim.AdamW(model.parameters(), lr=3e-3, weight_decay=1e-4)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=a.epochs)
    rng = random.Random(5)
    for epoch in range(a.epochs):
        t0 = time.time()
        rng.shuffle(train_items)
        for (pp, pd, beat, bar, np_, nd), mask, style in batches(train_items, 128, device):
            lp, ld, _ = model(pp, pd, beat, bar, style, np_)
            loss = F.cross_entropy(lp[mask], np_[mask]) + F.cross_entropy(ld[mask], nd[mask])
            opt.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            opt.step()
        sched.step()
        print(f"epoch {epoch + 1:2d} | val loss/note {loss_on(model, val_items, device):.3f} | {time.time() - t0:.0f}s", flush=True)
    torch.save(model.cpu().state_dict(), a.out)
    print("saved", a.out)


if __name__ == "__main__":
    main()
