import argparse
import copy
import csv
import json
import logging
import subprocess
import sys
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any
from urllib import parse, request


REQUIRED_MANIFEST_COLUMNS = {
    "family",
    "seed",
    "prompt_file",
    "negative_file",
    "raw_filename",
    "select_filename",
    "final_filename",
}


class RunnerError(RuntimeError):
    pass


@dataclass
class ManifestJob:
    index: int
    family: str
    seed: int
    prompt_file: Path
    negative_file: Path
    raw_filename: str
    select_filename: str
    final_filename: str

    @property
    def slug(self) -> str:
        family_slug = self.family.lower().replace("-", "_").replace(" ", "_")
        return f"{self.index:03d}_{family_slug}_s{self.seed}"

    @property
    def raw_stem(self) -> str:
        return Path(self.raw_filename).stem


class ComfyUiRunner:
    def __init__(self, repo_root: Path, config_path: Path) -> None:
        self.repo_root = repo_root
        self.config_path = config_path
        self.config = self._load_json(config_path)
        self.run_name = self.config["run_name"]
        self.manifest_path = self._resolve_path(self.config["manifest"])
        self.workflow_path = self._resolve_path(self.config["workflow_json"])
        self.prepared_jobs_dir = self._resolve_path(self.config["prepared_jobs_dir"])
        self.logs_dir = self._resolve_path(self.config["logs_dir"])
        self.output_dir = self._resolve_path(self.config["output_dir"])
        self.render_cfg = self.config["render"]
        self.comfy_cfg = self.config["comfyui"]
        self.logs_dir.mkdir(parents=True, exist_ok=True)
        self.prepared_jobs_dir.mkdir(parents=True, exist_ok=True)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.logger = self._build_logger(self.logs_dir / "runner.log")
        self.client_id = str(uuid.uuid4())

    def _build_logger(self, log_path: Path) -> logging.Logger:
        logger = logging.getLogger(f"comfyui_runner.{self.run_name}")
        logger.handlers.clear()
        logger.setLevel(logging.INFO)
        formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")

        file_handler = logging.FileHandler(log_path, encoding="utf-8")
        file_handler.setFormatter(formatter)
        logger.addHandler(file_handler)

        stream_handler = logging.StreamHandler(sys.stdout)
        stream_handler.setFormatter(formatter)
        logger.addHandler(stream_handler)
        return logger

    def _load_json(self, path: Path) -> dict[str, Any]:
        with path.open("r", encoding="utf-8") as handle:
            return json.load(handle)

    def _resolve_path(self, value: str) -> Path:
        path = Path(value)
        return path if path.is_absolute() else (self.repo_root / path)

    def _base_url(self) -> str:
        scheme = self.comfy_cfg.get("scheme", "http")
        host = self.comfy_cfg["host"]
        port = self.comfy_cfg["port"]
        return f"{scheme}://{host}:{port}"

    def _http_json(self, method: str, url: str, payload: dict[str, Any] | None = None) -> Any:
        data = None
        headers = {}
        if payload is not None:
            data = json.dumps(payload).encode("utf-8")
            headers["Content-Type"] = "application/json"
        req = request.Request(url, data=data, headers=headers, method=method)
        with request.urlopen(req, timeout=self.comfy_cfg["connect_timeout_sec"]) as response:
            body = response.read()
        return json.loads(body.decode("utf-8"))

    def _http_bytes(self, url: str) -> bytes:
        req = request.Request(url, method="GET")
        with request.urlopen(req, timeout=self.comfy_cfg["connect_timeout_sec"]) as response:
            return response.read()

    def probe_api(self) -> dict[str, Any]:
        base_url = self._base_url()
        result: dict[str, Any] = {
            "base_url": base_url,
            "reachable": False,
            "system_stats": None,
            "queue": None,
            "errors": [],
        }

        for endpoint in ("system_stats", "queue"):
            url = f"{base_url}/{endpoint}"
            try:
                payload = self._http_json("GET", url)
                result[endpoint] = payload
                result["reachable"] = True
            except Exception as exc:  # noqa: BLE001
                result["errors"].append(f"{endpoint}: {exc}")

        return result

    def maybe_launch_comfyui(self) -> bool:
        launch_command = self.comfy_cfg.get("launch_command", "").strip()
        if not launch_command:
            return False

        launch_cwd_value = self.comfy_cfg.get("launch_cwd", "").strip()
        launch_cwd = self._resolve_path(launch_cwd_value) if launch_cwd_value else None
        self.logger.info("ComfyUI API unavailable, trying configured launch command")
        subprocess.Popen(launch_command, cwd=launch_cwd, shell=True)  # noqa: S602,S603

        deadline = time.time() + self.comfy_cfg["startup_timeout_sec"]
        while time.time() < deadline:
            probe = self.probe_api()
            if probe["reachable"]:
                self.logger.info("ComfyUI API became reachable at %s", probe["base_url"])
                return True
            time.sleep(1.0)

        self.logger.warning("Configured launch command did not make ComfyUI reachable in time")
        return False

    def load_manifest(self) -> list[ManifestJob]:
        if not self.manifest_path.exists():
            raise RunnerError(f"Manifest not found: {self.manifest_path}")

        with self.manifest_path.open("r", encoding="utf-8-sig", newline="") as handle:
            reader = csv.DictReader(handle)
            if reader.fieldnames is None:
                raise RunnerError(f"Manifest is empty: {self.manifest_path}")
            missing_columns = REQUIRED_MANIFEST_COLUMNS - set(reader.fieldnames)
            if missing_columns:
                raise RunnerError(
                    f"Manifest is missing columns: {', '.join(sorted(missing_columns))}"
                )

            jobs: list[ManifestJob] = []
            for index, row in enumerate(reader, start=1):
                jobs.append(
                    ManifestJob(
                        index=index,
                        family=row["family"].strip(),
                        seed=int(row["seed"]),
                        prompt_file=self._resolve_path(row["prompt_file"].strip()),
                        negative_file=self._resolve_path(row["negative_file"].strip()),
                        raw_filename=row["raw_filename"].strip(),
                        select_filename=row["select_filename"].strip(),
                        final_filename=row["final_filename"].strip(),
                    )
                )

        return jobs

    def load_workflow_template(self) -> dict[str, Any] | None:
        if not self.workflow_path.exists():
            self.logger.warning("Workflow JSON not found: %s", self.workflow_path)
            return None

        workflow = self._load_json(self.workflow_path)
        if not self._is_api_prompt_workflow(workflow):
            raise RunnerError(
                f"Workflow JSON is not in ComfyUI API prompt format: {self.workflow_path}"
            )
        return workflow

    def _is_api_prompt_workflow(self, workflow: dict[str, Any]) -> bool:
        if not isinstance(workflow, dict) or not workflow:
            return False
        for node in workflow.values():
            if not isinstance(node, dict):
                return False
            if "class_type" not in node or "inputs" not in node:
                return False
        return True

    def _get_node(self, workflow: dict[str, Any], node_id: str) -> dict[str, Any]:
        if node_id not in workflow:
            raise RunnerError(f"Workflow node {node_id} not found")
        return workflow[node_id]

    def _extract_linked_node_id(self, value: Any, field_name: str) -> str:
        if not isinstance(value, list) or not value:
            raise RunnerError(f"Workflow field {field_name} is not a ComfyUI node link")
        return str(value[0])

    def _find_sampler_node(self, workflow: dict[str, Any]) -> tuple[str, dict[str, Any]]:
        matches = [
            (node_id, node)
            for node_id, node in workflow.items()
            if node.get("class_type") in {"KSampler", "KSamplerAdvanced"}
        ]
        if not matches:
            raise RunnerError("No KSampler node found in workflow JSON")
        return matches[0]

    def _find_first_node(self, workflow: dict[str, Any], class_type: str) -> tuple[str, dict[str, Any]]:
        for node_id, node in workflow.items():
            if node.get("class_type") == class_type:
                return node_id, node
        raise RunnerError(f"No {class_type} node found in workflow JSON")

    def build_job_payload(
        self,
        workflow_template: dict[str, Any],
        job: ManifestJob,
        positive_prompt: str,
        negative_prompt: str,
    ) -> dict[str, Any]:
        workflow = copy.deepcopy(workflow_template)
        sampler_id, sampler_node = self._find_sampler_node(workflow)
        latent_id = self._extract_linked_node_id(sampler_node["inputs"].get("latent_image"), "latent_image")
        positive_id = self._extract_linked_node_id(sampler_node["inputs"].get("positive"), "positive")
        negative_id = self._extract_linked_node_id(sampler_node["inputs"].get("negative"), "negative")
        save_id, save_node = self._find_first_node(workflow, "SaveImage")

        latent_node = self._get_node(workflow, latent_id)
        positive_node = self._get_node(workflow, positive_id)
        negative_node = self._get_node(workflow, negative_id)

        positive_node["inputs"]["text"] = positive_prompt
        negative_node["inputs"]["text"] = negative_prompt
        latent_node["inputs"]["width"] = self.render_cfg["width"]
        latent_node["inputs"]["height"] = self.render_cfg["height"]
        latent_node["inputs"]["batch_size"] = self.render_cfg["batch_size"]
        sampler_node["inputs"]["seed"] = job.seed
        sampler_node["inputs"]["steps"] = self.render_cfg["steps"]
        sampler_node["inputs"]["cfg"] = self.render_cfg["cfg"]
        sampler_node["inputs"]["sampler_name"] = self.render_cfg["sampler_name"]
        sampler_node["inputs"]["scheduler"] = self.render_cfg["scheduler"]
        sampler_node["inputs"]["denoise"] = self.render_cfg["denoise"]

        save_prefix = self.render_cfg["save_prefix_template"].format(
            run_name=self.run_name,
            raw_stem=job.raw_stem,
            family=job.family.lower().replace(" ", "_").replace("-", "_"),
            seed=job.seed,
        )
        save_node["inputs"]["filename_prefix"] = save_prefix.replace("\\", "/")
        self.logger.info("Prepared payload for %s using SaveImage node %s and sampler %s", job.slug, save_id, sampler_id)
        return workflow

    def _write_json(self, path: Path, payload: Any) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8") as handle:
            json.dump(payload, handle, indent=2)

    def _read_text(self, path: Path) -> str:
        if not path.exists():
            raise RunnerError(f"Prompt file not found: {path}")
        return path.read_text(encoding="utf-8").strip()

    def prepare_job_files(
        self,
        job: ManifestJob,
        workflow_template: dict[str, Any] | None,
    ) -> tuple[dict[str, Any], Path]:
        positive_prompt = self._read_text(job.prompt_file)
        negative_prompt = self._read_text(job.negative_file)
        job_spec = {
            "run_name": self.run_name,
            "job_slug": job.slug,
            "family": job.family,
            "seed": job.seed,
            "prompt_file": str(job.prompt_file.relative_to(self.repo_root)),
            "negative_file": str(job.negative_file.relative_to(self.repo_root)),
            "raw_output": str((self.output_dir / job.raw_filename).relative_to(self.repo_root)),
            "select_output": job.select_filename,
            "final_output": job.final_filename,
            "workflow_json": str(self.workflow_path.relative_to(self.repo_root)),
            "render": self.render_cfg,
            "positive_prompt": positive_prompt,
            "negative_prompt": negative_prompt,
        }

        spec_path = self.prepared_jobs_dir / f"{job.slug}.spec.json"
        self._write_json(spec_path, job_spec)

        if workflow_template is None:
            return job_spec, spec_path

        payload = self.build_job_payload(workflow_template, job, positive_prompt, negative_prompt)
        payload_path = self.prepared_jobs_dir / f"{job.slug}.payload.json"
        self._write_json(payload_path, payload)
        return job_spec, payload_path

    def submit_job(self, payload: dict[str, Any]) -> str:
        response = self._http_json(
            "POST",
            f"{self._base_url()}/prompt",
            {"prompt": payload, "client_id": self.client_id},
        )
        prompt_id = response.get("prompt_id")
        if not prompt_id:
            raise RunnerError(f"ComfyUI prompt submission returned no prompt_id: {response}")
        return str(prompt_id)

    def wait_for_job(self, prompt_id: str, save_node_id: str | None = None) -> dict[str, Any]:
        deadline = time.time() + self.comfy_cfg["job_timeout_sec"]
        history_url = f"{self._base_url()}/history/{parse.quote(prompt_id)}"

        while time.time() < deadline:
            try:
                history = self._http_json("GET", history_url)
            except Exception as exc:  # noqa: BLE001
                self.logger.warning("History poll for %s failed: %s", prompt_id, exc)
                time.sleep(self.comfy_cfg["poll_interval_sec"])
                continue

            entry = history.get(prompt_id)
            if entry and entry.get("outputs"):
                return entry

            status = entry.get("status") if entry else None
            if isinstance(status, dict) and status.get("status_str") == "error":
                raise RunnerError(f"ComfyUI job {prompt_id} failed: {status}")

            time.sleep(self.comfy_cfg["poll_interval_sec"])

        raise RunnerError(f"Timed out waiting for ComfyUI job {prompt_id}")

    def _find_image_output(self, history_entry: dict[str, Any], preferred_save_node_id: str | None) -> dict[str, Any]:
        outputs = history_entry.get("outputs", {})
        if preferred_save_node_id and preferred_save_node_id in outputs:
            images = outputs[preferred_save_node_id].get("images", [])
            if images:
                return images[0]

        for node_output in outputs.values():
            images = node_output.get("images", [])
            if images:
                return images[0]

        raise RunnerError("ComfyUI history entry contains no downloadable image outputs")

    def download_image(self, image_info: dict[str, Any], target_path: Path) -> None:
        query = parse.urlencode(
            {
                "filename": image_info.get("filename", ""),
                "subfolder": image_info.get("subfolder", ""),
                "type": image_info.get("type", "output"),
            }
        )
        image_bytes = self._http_bytes(f"{self._base_url()}/view?{query}")
        target_path.parent.mkdir(parents=True, exist_ok=True)
        target_path.write_bytes(image_bytes)

    def run(self, force: bool = False, require_render: bool = False) -> int:
        jobs = self.load_manifest()
        probe = self.probe_api()
        if not probe["reachable"] and self.comfy_cfg.get("launch_command", "").strip():
            self.maybe_launch_comfyui()
            probe = self.probe_api()

        workflow_template = self.load_workflow_template()
        render_enabled = probe["reachable"] and workflow_template is not None
        if require_render and not render_enabled:
            raise RunnerError(
                "Render was required, but ComfyUI API and/or workflow JSON is unavailable"
            )

        summary: dict[str, Any] = {
            "run_name": self.run_name,
            "manifest": str(self.manifest_path.relative_to(self.repo_root)),
            "workflow_json": str(self.workflow_path.relative_to(self.repo_root)),
            "api_probe": probe,
            "render_enabled": render_enabled,
            "jobs": [],
        }

        save_node_id = None
        if workflow_template is not None:
            try:
                save_node_id, _ = self._find_first_node(workflow_template, "SaveImage")
            except RunnerError:
                save_node_id = None

        failures = 0
        for job in jobs:
            raw_target = self.output_dir / job.raw_filename
            record: dict[str, Any] = {
                "job_slug": job.slug,
                "family": job.family,
                "seed": job.seed,
                "raw_target": str(raw_target.relative_to(self.repo_root)),
            }
            summary["jobs"].append(record)

            try:
                _, artifact_path = self.prepare_job_files(job, workflow_template)
                record["prepared_artifact"] = str(artifact_path.relative_to(self.repo_root))

                if raw_target.exists() and not force:
                    record["status"] = "skipped_existing"
                    self.logger.info("Skipping %s because output already exists", job.slug)
                    continue

                if not render_enabled:
                    record["status"] = "prepared_only"
                    self.logger.info("Prepared %s without rendering", job.slug)
                    continue

                payload = self._load_json(artifact_path)
                prompt_id = self.submit_job(payload)
                record["prompt_id"] = prompt_id
                self.logger.info("Submitted %s as ComfyUI prompt %s", job.slug, prompt_id)
                history_entry = self.wait_for_job(prompt_id, save_node_id=save_node_id)
                image_info = self._find_image_output(history_entry, save_node_id)
                self.download_image(image_info, raw_target)
                record["status"] = "rendered"
                record["downloaded_image"] = image_info
                self.logger.info("Downloaded %s to %s", job.slug, raw_target)
            except Exception as exc:  # noqa: BLE001
                failures += 1
                record["status"] = "failed"
                record["error"] = str(exc)
                self.logger.error("Job %s failed: %s", job.slug, exc)

        summary_path = self.logs_dir / "run_summary.json"
        self._write_json(summary_path, summary)
        self.logger.info(
            "Run finished: render_enabled=%s, failures=%d, summary=%s",
            render_enabled,
            failures,
            summary_path,
        )
        return 1 if failures else 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Manifest-driven local ComfyUI runner")
    subparsers = parser.add_subparsers(dest="command", required=True)

    probe_parser = subparsers.add_parser("probe", help="Probe the configured ComfyUI API")
    probe_parser.add_argument("--config", required=True, help="Path to runner config JSON")

    run_parser = subparsers.add_parser("run", help="Prepare jobs and optionally render them")
    run_parser.add_argument("--config", required=True, help="Path to runner config JSON")
    run_parser.add_argument("--force", action="store_true", help="Re-render even if raw output exists")
    run_parser.add_argument(
        "--require-render",
        action="store_true",
        help="Fail if ComfyUI API or workflow JSON is unavailable",
    )
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    repo_root = Path(__file__).resolve().parents[1]
    config_path = Path(args.config)
    if not config_path.is_absolute():
        config_path = repo_root / config_path

    runner = ComfyUiRunner(repo_root, config_path)

    if args.command == "probe":
        probe = runner.probe_api()
        print(json.dumps(probe, indent=2))
        return 0 if probe["reachable"] else 1

    if args.command == "run":
        return runner.run(force=args.force, require_render=args.require_render)

    parser.error(f"Unsupported command: {args.command}")
    return 2


if __name__ == "__main__":
    sys.exit(main())
