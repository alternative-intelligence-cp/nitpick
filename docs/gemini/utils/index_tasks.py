#!/usr/bin/env python3
"""
Quick reference index of all Gemini research tasks for Aria compiler.
Usage: python index_tasks.py
"""

import json
import os
from pathlib import Path

TASKS_DIR = Path(__file__).parent.parent / "tasks"


def load_task_metadata():
    """Load all task JSON files."""
    tasks = []
    for json_file in sorted(TASKS_DIR.glob("research_*.json")):
        with open(json_file) as f:
            task = json.load(f)
            task["file"] = json_file.name
            tasks.append(task)
    return tasks


def print_task_summary():
    """Print formatted task summary."""
    tasks = load_task_metadata()

    print("# ARIA RESEARCH TASKS INDEX\n")
    print(f"Total Tasks: {len(tasks)}\n")

    # Group by priority
    by_priority = {"CRITICAL": [], "HIGH": [], "MEDIUM": []}
    for task in tasks:
        by_priority[task["priority"]].append(task)

    for priority in ["CRITICAL", "HIGH", "MEDIUM"]:
        print(f"\n## {priority} Priority ({len(by_priority[priority])} tasks)\n")
        for task in by_priority[priority]:
            status_icon = {"PENDING": "⏳", "IN_PROGRESS": "🔄", "COMPLETE": "✅"}.get(
                task["status"], "❓"
            )

            print(f"{status_icon} **{task['task_id']}**")
            print(f"   {task['title']}")
            print(
                f"   Time: ~{task['estimated_minutes']} min | Deliverables: {len(task['deliverables'])}"
            )
            print(f"   File: `{task['file'].replace('.json', '.txt')}`")
            if task.get("blocks"):
                print(f"   Blocks: {', '.join(task['blocks'])}")
            print()


if __name__ == "__main__":
    print_task_summary()
