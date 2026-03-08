#!/usr/bin/env python3
import argparse
import json
import mimetypes
import os
import sys
import time
from pathlib import Path
from urllib import error, parse, request


def load_dotenv(dotenv_path: Path) -> None:
    if not dotenv_path.exists():
        return

    for raw_line in dotenv_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip().strip('"').strip("'")
        if key and key not in os.environ:
            os.environ[key] = value


def detect_content_type(file_path: Path) -> str:
    guessed, _ = mimetypes.guess_type(str(file_path))
    if guessed:
        return guessed

    extension_map = {
        ".pdf": "application/pdf",
        ".png": "image/png",
        ".jpg": "image/jpeg",
        ".jpeg": "image/jpeg",
        ".bmp": "image/bmp",
        ".tif": "image/tiff",
        ".tiff": "image/tiff",
        ".webp": "image/webp",
        ".heic": "image/heic",
    }
    return extension_map.get(file_path.suffix.lower(), "application/octet-stream")


def http_json(url: str, method: str, headers: dict, body: bytes | None = None) -> tuple[int, dict, dict]:
    req = request.Request(url=url, data=body, headers=headers, method=method)
    try:
        with request.urlopen(req) as resp:
            status = resp.getcode()
            response_headers = {k.lower(): v for k, v in resp.headers.items()}
            payload = resp.read().decode("utf-8")
            data = json.loads(payload) if payload else {}
            return status, response_headers, data
    except error.HTTPError as exc:
        payload = exc.read().decode("utf-8", errors="replace")
        try:
            parsed = json.loads(payload)
        except json.JSONDecodeError:
            parsed = {"raw": payload}
        raise RuntimeError(f"HTTP {exc.code} calling {url}: {json.dumps(parsed, ensure_ascii=False)}") from exc


def build_analyze_url(endpoint: str, model: str, api_version: str, output_format: str) -> str:
    endpoint = endpoint.rstrip("/")
    query = parse.urlencode({"api-version": api_version, "outputContentFormat": output_format})
    return f"{endpoint}/documentintelligence/documentModels/{model}:analyze?{query}"


def poll_operation(operation_url: str, api_key: str, interval_sec: float, timeout_sec: int) -> dict:
    deadline = time.time() + timeout_sec
    headers = {
        "Ocp-Apim-Subscription-Key": api_key,
    }

    while time.time() < deadline:
        status_code, _, payload = http_json(operation_url, "GET", headers)
        if status_code not in (200, 202):
            raise RuntimeError(f"Unexpected status while polling: {status_code}")

        status = str(payload.get("status", "")).lower()
        if status == "succeeded":
            return payload
        if status in {"failed", "canceled"}:
            raise RuntimeError(f"Analyze operation did not succeed: {json.dumps(payload, ensure_ascii=False)}")

        time.sleep(interval_sec)

    raise TimeoutError(f"Timed out waiting for operation to complete after {timeout_sec} seconds")


def extract_markdown(result_payload: dict) -> str:
    analyze_result = result_payload.get("analyzeResult", {})

    content = analyze_result.get("content")
    if isinstance(content, str) and content.strip():
        return content

    paragraphs = analyze_result.get("paragraphs") or []
    lines = []
    for paragraph in paragraphs:
        text = paragraph.get("content")
        if text:
            lines.append(text)
    if lines:
        return "\n\n".join(lines)

    return ""


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Analyze a PDF/image with Azure Document Intelligence and export Markdown + raw JSON."
    )
    parser.add_argument("input", help="Path to input document (PDF or image)")
    parser.add_argument("-o", "--output", help="Output markdown path. Defaults to <input_stem>.md")
    parser.add_argument("--json-output", help="Output JSON path. Defaults to <output>.json")
    parser.add_argument("--model", default="prebuilt-layout", help="Document Intelligence model (default: prebuilt-layout)")
    parser.add_argument("--api-version", default="2024-11-30", help="API version (default: 2024-11-30)")
    parser.add_argument("--format", default="markdown", choices=["markdown", "text"], help="Requested content format")
    parser.add_argument("--poll-interval", type=float, default=1.0, help="Polling interval in seconds")
    parser.add_argument("--timeout", type=int, default=300, help="Polling timeout in seconds")
    args = parser.parse_args()

    script_path = Path(__file__).resolve()
    repo_root = script_path.parent.parent
    load_dotenv(repo_root / ".env")

    endpoint = os.getenv("AZURE_DOCUMENT_INTELLIGENCE_ENDPOINT")
    api_key = os.getenv("AZURE_DOCUMENT_INTELLIGENCE_KEY")

    if not endpoint or not api_key:
        print(
            "Missing AZURE_DOCUMENT_INTELLIGENCE_ENDPOINT or AZURE_DOCUMENT_INTELLIGENCE_KEY. "
            "Set them in environment or .env",
            file=sys.stderr,
        )
        return 2

    input_path = Path(args.input).expanduser().resolve()
    if not input_path.exists() or not input_path.is_file():
        print(f"Input file not found: {input_path}", file=sys.stderr)
        return 2

    output_md = Path(args.output).expanduser().resolve() if args.output else input_path.with_suffix(".md")
    output_json = Path(args.json_output).expanduser().resolve() if args.json_output else Path(str(output_md) + ".json")

    content_type = detect_content_type(input_path)
    analyze_url = build_analyze_url(endpoint, args.model, args.api_version, args.format)

    body = input_path.read_bytes()
    headers = {
        "Ocp-Apim-Subscription-Key": api_key,
        "Content-Type": content_type,
    }

    status_code, response_headers, response_data = http_json(analyze_url, "POST", headers, body)

    operation_url = response_headers.get("operation-location")
    if status_code not in (200, 202) or not operation_url:
        raise RuntimeError(
            f"Analyze request failed or missing operation-location (status={status_code}): "
            f"{json.dumps(response_data, ensure_ascii=False)}"
        )

    result_payload = poll_operation(operation_url, api_key, args.poll_interval, args.timeout)
    markdown = extract_markdown(result_payload)

    output_md.parent.mkdir(parents=True, exist_ok=True)
    output_json.parent.mkdir(parents=True, exist_ok=True)

    output_md.write_text(markdown, encoding="utf-8")
    output_json.write_text(json.dumps(result_payload, ensure_ascii=False, indent=2), encoding="utf-8")

    print(f"Input:      {input_path}")
    print(f"Markdown:   {output_md}")
    print(f"Raw JSON:   {output_json}")
    print(f"Chars out:  {len(markdown)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
