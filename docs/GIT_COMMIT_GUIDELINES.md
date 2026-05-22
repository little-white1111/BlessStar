# Git Commit Guidelines

## 1. Overview

This document defines the commit message format and guidelines for BlessStar development.

## 2. Commit Message Structure

Each commit message should consist of a subject line, body (optional), and footer (optional).

```
<type>(<scope>): <subject>

<body>

<footer>
```

### 2.1 Type

| Type | Description |
|------|-------------|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation changes |
| `style` | Code style changes |
| `refactor` | Code refactoring |
| `test` | Adding/updating tests |
| `chore` | Build system changes |
| `revert` | Revert a previous commit |

### 2.2 Scope

- `kernel`
- `adapter`
- `factory`
- `tools`
- `docs`
- `ci`

### 2.3 Subject

- Brief summary (50 characters max)
- Written in imperative mood
- Capitalize the first letter
- No period at the end

### 2.4 Body

- Detailed description
- Explain why the change was made
- Wrap lines at 72 characters

### 2.5 Footer

- Issue references: `Fixes #123`
- Breaking changes: `BREAKING CHANGE: ...`

## 3. Examples

### 3.1 Feature Commit

```
feat(kernel): Add IR parsing module

Implement the Input IR parsing functionality with support for:
- YAML configuration format
- Schema validation
- Error reporting
```

### 3.2 Bug Fix Commit

```
fix(adapter): Handle empty configuration gracefully

Previously, an empty config file would cause a crash.
Fixes #42
```

### 3.3 Breaking Change

```
refactor(kernel): Rename PipelineResult to ExecutionResult

BREAKING CHANGE: The `PipelineResult` class has been renamed.
```

## 4. Branch Naming

Use descriptive branch names:
- `feature/IR-parsing`
- `fix/adapter-crash`
- `docs/update-coding-style`

## 5. Pull Request Guidelines

### 5.1 Title
- Follow the same format as commit messages

### 5.2 Checklist
- [ ] Code follows coding style guidelines
- [ ] Tests added/updated
- [ ] Documentation updated
- [ ] CI passes

## 6. Best Practices

1. Small commits: Each commit should address a single logical change
2. Atomic changes: Commits should be self-contained
3. Write meaningful messages
4. Review before pushing

---

*Based on Conventional Commits specification*
