#!/usr/bin/env python3
"""
Analyze HSV metrics for each frame in a replay images folder.

For every frame:
  - Detects the 3DS screen region via Otsu threshold + contour bounding box
  - Splits the detected region into top-screen (upper) and bottom-screen (lower) halves
  - Computes a rich set of HSV statistics on the full frame, top half, and bottom half

Output: CSV file + printed summary for a configurable frame range.

Usage:
    python tools/analyze_frames.py <images_dir> [output.csv] [--range 490 620]

Example:
    python tools/analyze_frames.py video_replays/fennekin_normal/images/ metrics.csv --range 490 620
"""

import cv2
import numpy as np
import csv
import sys
import argparse
from pathlib import Path
from typing import Optional


# ---------------------------------------------------------------------------
# Screen detection
# ---------------------------------------------------------------------------


def find_screen_bbox(gray: np.ndarray) -> Optional[tuple[int, int, int, int]]:
    """
    Detect the bounding box of the largest bright region (the 3DS screen area).
    Returns (x, y, w, h) or None if nothing suitable is found.
    """
    _, thresh = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (7, 7))
    thresh = cv2.morphologyEx(thresh, cv2.MORPH_CLOSE, kernel)
    thresh = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, kernel)

    contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours:
        return None

    frame_area = gray.shape[0] * gray.shape[1]
    largest = max(contours, key=cv2.contourArea)
    if cv2.contourArea(largest) < 0.05 * frame_area:
        return None

    return cv2.boundingRect(largest)


# ---------------------------------------------------------------------------
# Metric extraction
# ---------------------------------------------------------------------------


def hsv_metrics(bgr: np.ndarray, prefix: str) -> dict:
    """Compute HSV statistics for a BGR region and return them with a key prefix."""
    if bgr is None or bgr.size == 0:
        return {}

    hsv = cv2.cvtColor(bgr, cv2.COLOR_BGR2HSV)
    s = hsv[:, :, 1].astype(np.float32)
    v = hsv[:, :, 2].astype(np.float32)
    total = float(v.size)

    def ratio(mask) -> float:
        return float(np.count_nonzero(mask)) / total

    return {
        f"{prefix}_avg_v": float(np.mean(v) / 255.0),
        f"{prefix}_std_v": float(np.std(v) / 255.0),
        f"{prefix}_avg_s": float(np.mean(s) / 255.0),
        # Dark pixel thresholds (V out of 255)
        f"{prefix}_v_lt_20": ratio(v < 20),
        f"{prefix}_v_lt_30": ratio(v < 30),
        f"{prefix}_v_lt_50": ratio(v < 50),
        # Bright pixel thresholds
        f"{prefix}_v_gt_150": ratio(v > 150),
        f"{prefix}_v_gt_180": ratio(v > 180),
        f"{prefix}_v_gt_200": ratio(v > 200),
        f"{prefix}_v_gt_220": ratio(v > 220),
        # White-ish: bright AND low saturation
        f"{prefix}_white_s40": ratio((s < 40) & (v > 180)),
        f"{prefix}_white_s60": ratio((s < 60) & (v > 180)),
        f"{prefix}_white_s80": ratio((s < 80) & (v > 160)),
        # Colorful: high saturation
        f"{prefix}_colorful": ratio(s > 80),
    }


def process_frame(path: Path) -> dict:
    """Load one frame and return all metrics."""
    img = cv2.imread(str(path))
    if img is None:
        return {}

    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    bbox = find_screen_bbox(gray)

    if bbox is not None:
        x, y, w, h = bbox
        screen = img[y : y + h, x : x + w]
    else:
        screen = img  # fallback: use whole frame

    mid = screen.shape[0] // 2
    top_region = screen[:mid, :]
    bot_region = screen[mid:, :]

    result: dict = {"has_bbox": int(bbox is not None)}
    result.update(hsv_metrics(screen, "full"))
    result.update(hsv_metrics(top_region, "top"))
    result.update(hsv_metrics(bot_region, "bot"))
    return result


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("images_dir", help="Path to folder with frame images")
    parser.add_argument(
        "output_csv",
        nargs="?",
        help="Output CSV path (default: <images_dir>/../metrics.csv)",
    )
    parser.add_argument(
        "--range",
        nargs=2,
        type=int,
        metavar=("FROM", "TO"),
        help="Print summary only for this 0-based frame range (e.g. --range 490 620)",
    )
    args = parser.parse_args()

    images_dir = Path(args.images_dir)
    output_csv = (
        Path(args.output_csv) if args.output_csv else images_dir.parent / "metrics.csv"
    )

    if not images_dir.exists():
        print(f"Error: {images_dir} does not exist", file=sys.stderr)
        sys.exit(1)

    exts = {".jpg", ".jpeg", ".png", ".bmp"}
    img_files = sorted(
        [f for f in images_dir.iterdir() if f.suffix.lower() in exts],
        key=lambda p: p.stem,
    )

    if not img_files:
        print(f"No images found in {images_dir}", file=sys.stderr)
        sys.exit(1)

    print(f"Processing {len(img_files)} frames from {images_dir} ...")

    rows = []
    for idx, path in enumerate(img_files):
        metrics = process_frame(path)
        metrics["frame_idx"] = idx
        metrics["filename"] = path.name
        rows.append(metrics)
        if idx % 100 == 0:
            print(f"  {idx}/{len(img_files)}")

    if not rows:
        print("No frames processed.", file=sys.stderr)
        sys.exit(1)

    # Write CSV
    fixed_cols = ["frame_idx", "filename", "has_bbox"]
    metric_cols = sorted(k for k in rows[0] if k not in fixed_cols)
    fieldnames = fixed_cols + metric_cols

    with open(output_csv, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow({k: row.get(k, "") for k in fieldnames})

    print(f"\nCSV written to: {output_csv}")

    # Printed summary for the interesting range
    lo, hi = (args.range[0], args.range[1]) if args.range else (0, len(rows))

    print()
    header = (
        f"{'fr':>5}  "
        f"{'top_avgV':>9}  {'top_v<30':>9}  {'top_v>180':>9}  "
        f"{'bot_avgV':>9}  {'bot_v>180':>9}  {'bot_wht_s60':>11}  {'bot_v>220':>9}"
    )
    print(header)
    print("-" * len(header))

    for row in rows:
        idx = row["frame_idx"]
        if lo <= idx <= hi:
            print(
                f"{idx:>5}  "
                f"{row.get('top_avg_v', 0):>9.3f}  "
                f"{row.get('top_v_lt_30', 0):>9.3f}  "
                f"{row.get('top_v_gt_180', 0):>9.3f}  "
                f"{row.get('bot_avg_v', 0):>9.3f}  "
                f"{row.get('bot_v_gt_180', 0):>9.3f}  "
                f"{row.get('bot_white_s60', 0):>11.3f}  "
                f"{row.get('bot_v_gt_220', 0):>9.3f}"
            )


if __name__ == "__main__":
    main()
