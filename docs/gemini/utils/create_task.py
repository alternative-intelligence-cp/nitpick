#!/usr/bin/env python3
"""
Utility to create new research tasks following the established pattern.
Usage: python create_task.py <task_number> <title> <priority>
"""

import json
import sys
from pathlib import Path

TASKS_DIR = Path(__file__).parent.parent / "tasks"

TEMPLATE_TXT = """# RESEARCH TASK: {title}

**Task ID:** {task_id}
**Priority:** {priority}
**Status:** PENDING
**Created:** 2025-12-11
**Estimated Time:** {estimated_time}

---

## PROBLEM STATEMENT

[Describe the problem this research addresses]

---

## CONTEXT

[Provide relevant context, existing work, design considerations]

---

## DELIVERABLES

### 1. [Deliverable Name]

**Requirements:**
- [Requirement 1]
- [Requirement 2]

**Include:**
- [Item 1]
- [Item 2]

---

## RELEVANT SPECIFICATIONS

From aria_specs.txt:
```
[Relevant spec excerpts]
```

---

## INSTRUCTIONS FOR GEMINI

**Focus Areas:**
1. [Focus 1]
2. [Focus 2]

**Key Questions:**
- [Question 1]
- [Question 2]

**Deliverable Format:**
- [Format guidance]

---

## SUCCESS CRITERIA

Research complete when we can:
1. [Criterion 1]
2. [Criterion 2]

---

**PRIORITY:** {priority_note}
"""

TEMPLATE_JSON = {
    "task_id": "",
    "title": "",
    "priority": "",
    "status": "PENDING",
    "created": "2025-12-11",
    "estimated_minutes": 30,
    "deliverables": [],
    "context_files": [
        "/home/randy/._____RANDY_____/REPOS/aria/docs/info/aria_specs.txt"
    ],
    "tags": [],
    "blocks": [],
    "related_tasks": [],
}


def create_task(number, title, priority, estimated_time="25-30 minutes"):
    """Create task files."""
    task_id = f"research_{number:03d}_{title.lower().replace(' ', '_')}"

    # Create .txt file
    txt_content = TEMPLATE_TXT.format(
        title=title,
        task_id=task_id,
        priority=priority,
        estimated_time=estimated_time,
        priority_note="[Add priority justification]",
    )

    txt_file = TASKS_DIR / f"{task_id}.txt"
    with open(txt_file, "w") as f:
        f.write(txt_content)

    # Create .json file
    json_data = TEMPLATE_JSON.copy()
    json_data["task_id"] = task_id
    json_data["title"] = title
    json_data["priority"] = priority

    json_file = TASKS_DIR / f"{task_id}.json"
    with open(json_file, "w") as f:
        json.dump(json_data, f, indent=2)

    print(f"✅ Created {task_id}")
    print(f"   TXT: {txt_file}")
    print(f"   JSON: {json_file}")


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python create_task.py <number> <title> <priority>")
        print("Example: python create_task.py 11 'WebAssembly Backend' HIGH")
        sys.exit(1)

    number = int(sys.argv[1])
    title = sys.argv[2]
    priority = sys.argv[3]

    create_task(number, title, priority)
