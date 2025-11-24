#!/usr/bin/env python3
# BSD 3-Clause License
#
# Copyright (c) 2021-2025
#
# Performance benchmark comparison script for database_system.
# Compares current benchmark results against a baseline and detects regressions.

"""
Benchmark Comparison Tool

Compares Google Benchmark JSON output files and detects performance regressions.
Generates markdown reports suitable for GitHub PR comments.

Usage:
    python compare_benchmarks.py baseline.json current.json [options]

Options:
    --threshold PERCENT    Regression threshold percentage (default: 10)
    --output FILE          Output markdown file path
    --format FORMAT        Output format: markdown, json, console (default: markdown)
"""

import json
import sys
import argparse
from pathlib import Path
from typing import Dict, List, Tuple, Any, Optional
from dataclasses import dataclass


@dataclass
class BenchmarkResult:
    """Single benchmark result."""
    name: str
    real_time: float
    cpu_time: float
    time_unit: str
    iterations: int


@dataclass
class ComparisonResult:
    """Comparison between baseline and current benchmark."""
    name: str
    baseline_time: float
    current_time: float
    change_percent: float
    time_unit: str
    is_regression: bool
    is_improvement: bool


def load_benchmarks(filepath: str) -> Dict[str, BenchmarkResult]:
    """Load benchmark results from JSON file."""
    with open(filepath, 'r') as f:
        data = json.load(f)

    results = {}
    for bench in data.get('benchmarks', []):
        name = bench.get('name', '')
        # Skip aggregate entries (mean, median, stddev)
        if any(suffix in name for suffix in ['_mean', '_median', '_stddev', '_cv']):
            continue

        results[name] = BenchmarkResult(
            name=name,
            real_time=bench.get('real_time', 0),
            cpu_time=bench.get('cpu_time', 0),
            time_unit=bench.get('time_unit', 'ns'),
            iterations=bench.get('iterations', 0)
        )

    return results


def compare_benchmarks(
    baseline: Dict[str, BenchmarkResult],
    current: Dict[str, BenchmarkResult],
    threshold_percent: float
) -> Tuple[List[ComparisonResult], List[ComparisonResult], List[ComparisonResult]]:
    """
    Compare benchmark results.

    Returns:
        Tuple of (all_results, regressions, improvements)
    """
    all_results = []
    regressions = []
    improvements = []

    for name, current_bench in current.items():
        if name not in baseline:
            continue

        baseline_bench = baseline[name]
        baseline_time = baseline_bench.real_time
        current_time = current_bench.real_time

        if baseline_time == 0:
            continue

        change_percent = ((current_time - baseline_time) / baseline_time) * 100

        is_regression = change_percent > threshold_percent
        is_improvement = change_percent < -threshold_percent

        result = ComparisonResult(
            name=name,
            baseline_time=baseline_time,
            current_time=current_time,
            change_percent=change_percent,
            time_unit=current_bench.time_unit,
            is_regression=is_regression,
            is_improvement=is_improvement
        )

        all_results.append(result)

        if is_regression:
            regressions.append(result)
        elif is_improvement:
            improvements.append(result)

    return all_results, regressions, improvements


def format_time(value: float, unit: str) -> str:
    """Format time value with appropriate unit."""
    if unit == 'ns' and value >= 1000:
        return f"{value/1000:.2f} us"
    elif unit == 'us' and value >= 1000:
        return f"{value/1000:.2f} ms"
    elif unit == 'ms' and value >= 1000:
        return f"{value/1000:.2f} s"
    return f"{value:.2f} {unit}"


def generate_markdown(
    all_results: List[ComparisonResult],
    regressions: List[ComparisonResult],
    improvements: List[ComparisonResult],
    threshold: float
) -> str:
    """Generate markdown report."""
    lines = ["## Performance Benchmark Results\n"]

    # Summary
    total = len(all_results)
    regression_count = len(regressions)
    improvement_count = len(improvements)

    if regression_count > 0:
        lines.append(f"**Status**: Performance regression detected ({regression_count} benchmark(s))\n")
    elif improvement_count > 0:
        lines.append(f"**Status**: Performance improved ({improvement_count} benchmark(s))\n")
    else:
        lines.append("**Status**: No significant performance changes detected\n")

    lines.append(f"**Threshold**: {threshold}%\n")
    lines.append(f"**Total benchmarks compared**: {total}\n")

    # Regressions section
    if regressions:
        lines.append("\n### Regressions\n")
        lines.append("| Benchmark | Baseline | Current | Change |")
        lines.append("|-----------|----------|---------|--------|")
        for r in sorted(regressions, key=lambda x: -x.change_percent):
            baseline_str = format_time(r.baseline_time, r.time_unit)
            current_str = format_time(r.current_time, r.time_unit)
            lines.append(
                f"| `{r.name}` | {baseline_str} | {current_str} | "
                f"+{r.change_percent:.1f}% |"
            )

    # Improvements section
    if improvements:
        lines.append("\n### Improvements\n")
        lines.append("| Benchmark | Baseline | Current | Change |")
        lines.append("|-----------|----------|---------|--------|")
        for r in sorted(improvements, key=lambda x: x.change_percent):
            baseline_str = format_time(r.baseline_time, r.time_unit)
            current_str = format_time(r.current_time, r.time_unit)
            lines.append(
                f"| `{r.name}` | {baseline_str} | {current_str} | "
                f"{r.change_percent:.1f}% |"
            )

    # All results (collapsible)
    lines.append("\n### All Results\n")
    lines.append("<details>")
    lines.append("<summary>Click to expand full benchmark results</summary>\n")
    lines.append("| Benchmark | Baseline | Current | Change | Status |")
    lines.append("|-----------|----------|---------|--------|--------|")

    for r in sorted(all_results, key=lambda x: x.name):
        baseline_str = format_time(r.baseline_time, r.time_unit)
        current_str = format_time(r.current_time, r.time_unit)

        if r.is_regression:
            status = "Regression"
            change_str = f"+{r.change_percent:.1f}%"
        elif r.is_improvement:
            status = "Improved"
            change_str = f"{r.change_percent:.1f}%"
        else:
            status = "OK"
            change_str = f"{r.change_percent:+.1f}%"

        lines.append(
            f"| `{r.name}` | {baseline_str} | {current_str} | "
            f"{change_str} | {status} |"
        )

    lines.append("\n</details>")

    return '\n'.join(lines)


def generate_json_output(
    all_results: List[ComparisonResult],
    regressions: List[ComparisonResult],
    improvements: List[ComparisonResult]
) -> str:
    """Generate JSON output."""
    output = {
        "summary": {
            "total": len(all_results),
            "regressions": len(regressions),
            "improvements": len(improvements),
            "has_regression": len(regressions) > 0
        },
        "regressions": [
            {
                "name": r.name,
                "baseline": r.baseline_time,
                "current": r.current_time,
                "change_percent": r.change_percent,
                "unit": r.time_unit
            }
            for r in regressions
        ],
        "improvements": [
            {
                "name": r.name,
                "baseline": r.baseline_time,
                "current": r.current_time,
                "change_percent": r.change_percent,
                "unit": r.time_unit
            }
            for r in improvements
        ],
        "all_results": [
            {
                "name": r.name,
                "baseline": r.baseline_time,
                "current": r.current_time,
                "change_percent": r.change_percent,
                "unit": r.time_unit,
                "status": "regression" if r.is_regression else ("improvement" if r.is_improvement else "ok")
            }
            for r in all_results
        ]
    }
    return json.dumps(output, indent=2)


def main():
    parser = argparse.ArgumentParser(
        description='Compare benchmark results and detect regressions'
    )
    parser.add_argument('baseline', help='Baseline benchmark JSON file')
    parser.add_argument('current', help='Current benchmark JSON file')
    parser.add_argument(
        '--threshold', type=float, default=10.0,
        help='Regression threshold percentage (default: 10)'
    )
    parser.add_argument(
        '--output', help='Output file path'
    )
    parser.add_argument(
        '--format', choices=['markdown', 'json', 'console'],
        default='markdown', help='Output format (default: markdown)'
    )
    args = parser.parse_args()

    # Check if baseline exists
    if not Path(args.baseline).exists():
        print(f"Baseline file not found: {args.baseline}")
        print("Creating initial baseline from current results...")
        Path(args.baseline).write_text(Path(args.current).read_text())
        print("Baseline created. No comparison performed.")
        sys.exit(0)

    # Load benchmark results
    try:
        baseline = load_benchmarks(args.baseline)
        current = load_benchmarks(args.current)
    except (json.JSONDecodeError, FileNotFoundError) as e:
        print(f"Error loading benchmark files: {e}")
        sys.exit(1)

    if not baseline or not current:
        print("No benchmark results found in files")
        sys.exit(1)

    # Compare benchmarks
    all_results, regressions, improvements = compare_benchmarks(
        baseline, current, args.threshold
    )

    # Generate output
    if args.format == 'markdown':
        output = generate_markdown(all_results, regressions, improvements, args.threshold)
    elif args.format == 'json':
        output = generate_json_output(all_results, regressions, improvements)
    else:
        # Console format
        output = generate_markdown(all_results, regressions, improvements, args.threshold)
        # Strip markdown formatting for console
        output = output.replace('##', '').replace('**', '').replace('`', '')

    # Write output
    if args.output:
        Path(args.output).write_text(output)
        print(f"Report written to: {args.output}")
    else:
        print(output)

    # Exit with error code if regressions detected
    if regressions:
        print(f"\nDetected {len(regressions)} performance regression(s)!", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
