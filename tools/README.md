# ZoneSwitch OCR Tools (uv workflow)

This repo uses **uv** for Python execution and dependency management (no traditional virtual environment).

## 1) Install uv

macOS:

```bash
brew install uv
```

Alternative:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

## 2) Configure credentials

Credentials are loaded from the repo root `.env`:

- `AZURE_DOCUMENT_INTELLIGENCE_ENDPOINT`
- `AZURE_DOCUMENT_INTELLIGENCE_KEY`

## 3) Run OCR / document parsing

From the repo root:

```bash
uv run --no-project python tools/azure_docint_to_markdown.py docs/research/zoneswitch/ZoneSwitchV2_OpInstallationManual2015_12x17.pdf -o docs/research/zoneswitch/ZoneSwitchV2_OpInstallationManual2015_12x17.md
```

For the screenshot/image:

```bash
uv run --no-project python tools/azure_docint_to_markdown.py "docs/research/zoneswitch/Screenshot 2026-03-08 at 7.53.07 PM.png" -o docs/research/zoneswitch/screenshot_ocr.md
```

## Outputs

For each input, the script writes:

- Markdown output at `-o` path
- Raw Document Intelligence response at `<markdown_output>.json`

## Optional flags

- `--model prebuilt-layout` (default)
- `--api-version 2024-11-30` (default)
- `--format markdown|text`
- `--poll-interval 1.0`
- `--timeout 300`
