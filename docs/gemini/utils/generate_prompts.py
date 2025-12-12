#!/usr/bin/env python3
"""
Generate .txt prompt files from .json task definitions for research_012-031
"""

import json
from pathlib import Path

TASKS_DIR = Path("/home/randy/._____RANDY_____/REPOS/aria/docs/gemini/tasks")

def format_key_questions(questions_dict):
    """Format key questions from nested dict structure."""
    lines = []
    for category, questions in questions_dict.items():
        category_title = category.replace('_', ' ').title()
        lines.append(f"\n**{category_title}:**")
        if isinstance(questions, list):
            for q in questions:
                lines.append(f"- {q}")
        elif isinstance(questions, dict):
            for subcat, subquestions in questions.items():
                lines.append(f"\n  *{subcat.replace('_', ' ').title()}:*")
                for q in subquestions:
                    lines.append(f"  - {q}")
    return "\n".join(lines)

def format_scope(scope_dict):
    """Format scope dictionary."""
    lines = []
    for key, value in scope_dict.items():
        key_title = key.replace('_', ' ').title()
        lines.append(f"- **{key_title}**: {value}")
    return "\n".join(lines)

def generate_prompt_file(task_num):
    """Generate .txt prompt file from .json task definition."""
    json_file = TASKS_DIR / f"research_{task_num:03d}_*.json"
    json_files = list(TASKS_DIR.glob(f"research_{task_num:03d}_*.json"))
    
    if not json_files:
        print(f"❌ No JSON file found for research_{task_num:03d}")
        return
    
    json_file = json_files[0]
    
    # Check if .txt already exists
    txt_file = json_file.with_suffix('.txt')
    if txt_file.exists():
        print(f"⏭️  {txt_file.name} already exists, skipping")
        return
    
    # Load JSON
    with open(json_file, 'r') as f:
        task = json.load(f)
    
    # Generate prompt content
    prompt = f"""# RESEARCH TASK: {task['title']}

**Task ID:** {task['task_id']}
**Priority:** {task['priority'].upper()}
**Category:** {task.get('category', 'unknown').replace('_', ' ').title()}
**Status:** PENDING
**Created:** 2025-12-11
**Estimated Complexity:** {task.get('estimated_complexity', 'medium').title()}

---

## PROBLEM STATEMENT

{task['description']}

---

## SCOPE

{format_scope(task['scope'])}

---

## DEPENDENCIES

"""
    
    # Add dependencies
    if task.get('dependencies'):
        for dep in task['dependencies']:
            prompt += f"- {dep}\n"
    else:
        prompt += "- None\n"
    
    prompt += """
---

## KEY QUESTIONS

"""
    
    # Add key questions
    if isinstance(task.get('key_questions'), dict):
        prompt += format_key_questions(task['key_questions'])
    elif isinstance(task.get('key_questions'), list):
        for q in task['key_questions']:
            prompt += f"- {q}\n"
    
    prompt += """

---

## DELIVERABLES

"""
    
    # Add deliverables
    if task.get('deliverables'):
        for i, deliverable in enumerate(task['deliverables'], 1):
            prompt += f"{i}. {deliverable}\n"
    
    prompt += """
---

## CONTEXT FILES

"""
    
    # Add context files
    if task.get('context_files'):
        for ctx_file in task['context_files']:
            prompt += f"- {ctx_file}\n"
    
    prompt += """
---

## INSTRUCTIONS FOR GEMINI

**Your Task:**
Provide a comprehensive specification document covering all key questions and deliverables listed above.

**Format:**
- Clear section headings
- Code examples where appropriate
- Design rationale explanations
- Platform-specific considerations (Linux, Windows, macOS)
- Integration notes with existing research
- Performance implications
- Security/safety considerations

**Style:**
- Technical and precise
- Include pseudo-code or actual Aria syntax examples
- Reference relevant research papers or existing implementations
- Provide comparison with similar features in other languages

**Length:** Aim for {task.get('target_length', '500-800')} lines of detailed technical content.

---

## SUCCESS CRITERIA

Research complete when we have:
1. Complete specification covering all key questions
2. Design decisions documented with rationale
3. Integration strategy with existing compiler components
4. Code examples demonstrating usage
5. Implementation roadmap or guidance

---

**PRIORITY:** {task['priority'].upper()} - {task.get('description', '')[:100]}...
"""
    
    # Write prompt file
    with open(txt_file, 'w') as f:
        f.write(prompt)
    
    print(f"✅ Created {txt_file.name}")

if __name__ == "__main__":
    # Generate prompts for research_012 through research_031
    for task_num in range(12, 32):
        generate_prompt_file(task_num)
    
    print("\n🎉 All prompt files generated!")
