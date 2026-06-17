#!/usr/bin/env python3
"""
BlessStar RL Data Pipeline — Configuration-change history → GRPO training samples.

AGF-06: Training data ONLY from BlessStar config change history + validation logs.
          No synthetic data from model self-generation.

Pipeline steps:
  1. extract_entity_groups — parse domain_knowledge.json → entity groups (SAKE Turn 1)
  2. extract_gate_triplets — parse constraint_knowledge.json → (field, op, value) triplets (SAKE Turn 2)
  3. build_training_samples — read audit log → {query, expected_tool_call, schema_change} samples
  4. save_to_format — serialize to JSONL consumable by GRPO trainer

This module is a **skeleton / parallel research pipeline**. Actual training
requires 30+ days of accumulated BlessStar configuration change history.
"""

from __future__ import annotations

import json
import logging
import os
from dataclasses import dataclass, field, asdict
from datetime import datetime
from typing import Any, Optional

logger = logging.getLogger(__name__)

# ── Data structures ──────────────────────────────────────────────────────


@dataclass
class EntityGroup:
    """SAKE Turn 1: extracted entity group from domain_knowledge.json."""
    domain: str
    fields: list[str]
    field_count: int


@dataclass
class GateTriplet:
    """SAKE Turn 2: (field, op, value) triplet from Gate chain."""
    gate_id: str
    field_key: str
    op: str
    value: str
    scenario: str
    layer: int
    sub_category: str
    stable_key: str


@dataclass
class TrainingSample:
    """One RL training sample derived from config change history."""
    query: str
    expected_tool_call: dict[str, Any]
    schema_change: Optional[dict[str, Any]] = None
    validation_log: Optional[str] = None
    timestamp: Optional[str] = None


@dataclass
class DomainKnowledge:
    """Mirror of agent_indexer domain_knowledge.json."""
    version: str
    generated_at: str
    business_name: str
    domains: list[dict[str, Any]]


@dataclass
class ConstraintKnowledge:
    """Mirror of agent_indexer constraint_knowledge.json."""
    version: str
    generated_at: str
    business_name: str
    gates: list[dict[str, Any]]


# ── Pipeline ─────────────────────────────────────────────────────────────


class BlessStarDataPipeline:
    """
    Extract RL training samples from BlessStar configuration audit history.

    Usage:
        pipeline = BlessStarDataPipeline()
        samples = pipeline.build_training_samples('/data/audit_logs/')
        pipeline.save_to_format(samples, '/output/training_data/')
    """

    # ── SAKE Turn 1: Entity Extraction ─────────────────────────────────

    @staticmethod
    def extract_entity_groups(domain_knowledge: dict[str, Any]) -> list[EntityGroup]:
        """
        Parse domain_knowledge.json → entity groups.

        Each domain in the index becomes one entity group containing
        its field names. This mirrors SAKE's Turn1 Entity Extraction.
        """
        dk = DomainKnowledge(**domain_knowledge)
        groups: list[EntityGroup] = []
        for d in dk.domains:
            groups.append(
                EntityGroup(
                    domain=d["name"],
                    fields=list(d.get("fields", [])),
                    field_count=d.get("field_count", 0),
                )
            )
        logger.info("extract_entity_groups: %d groups from domain=%s", len(groups), dk.business_name)
        return groups

    # ── SAKE Turn 2: Group Filtering / Gate Triplets ───────────────────

    @staticmethod
    def extract_gate_triplets(constraint_knowledge: dict[str, Any]) -> list[GateTriplet]:
        """
        Parse constraint_knowledge.json → (field, op, value) triplets.

        Each Gate rule becomes one triplet. This mirrors SAKE's Turn2
        Group Filtering step.
        """
        ck = ConstraintKnowledge(**constraint_knowledge)
        triplets: list[GateTriplet] = []
        for g in ck.gates:
            triplets.append(
                GateTriplet(
                    gate_id=g.get("gate_id", ""),
                    field_key=g.get("field_key", ""),
                    op=g.get("op", ""),
                    value=g.get("value", ""),
                    scenario=g.get("scenario", ""),
                    layer=g.get("layer", 0),
                    sub_category=g.get("sub_category", ""),
                    stable_key=g.get("stable_key", ""),
                )
            )
        logger.info("extract_gate_triplets: %d triplets from %s", len(triplets), ck.business_name)
        return triplets

    # ── Build training samples from audit logs ─────────────────────────

    @staticmethod
    def build_training_samples(audit_log_path: str) -> list[TrainingSample]:
        """
        Walk audit_log_path for BlessStar configuration-change log files.

        Each log entry (JSONL) describes a config mutation. We transform
        it into one or more TrainingSample entries usable by GRPO.

        Log format expected (one JSON object per line):
            {"timestamp":"...", "action":"create_field|update_gate|delete_field",
             "user":"...", "field_key":"...", "widget":"...",
             "label":"...", "gate_id":"...", "validation":"pass|fail"}

        Returns empty list if no audit logs exist yet (pre-training phase).
        """
        if not os.path.isdir(audit_log_path):
            logger.warning("build_training_samples: audit_log_path %s not found; returning []", audit_log_path)
            return []

        samples: list[TrainingSample] = []
        for root, _dirs, files in os.walk(audit_log_path):
            for fname in sorted(files):
                if not fname.endswith(".jsonl"):
                    continue
                fpath = os.path.join(root, fname)
                try:
                    with open(fpath, "r", encoding="utf-8") as fh:
                        for line in fh:
                            line = line.strip()
                            if not line:
                                continue
                            sample = BlessStarDataPipeline._parse_log_line(line)
                            if sample:
                                samples.append(sample)
                except OSError as exc:
                    logger.error("Failed to read %s: %s", fpath, exc)

        logger.info("build_training_samples: %d samples from %s", len(samples), audit_log_path)
        return samples

    @staticmethod
    def _parse_log_line(line: str) -> Optional[TrainingSample]:
        """Parse one JSONL line → TrainingSample."""
        try:
            entry = json.loads(line)
        except json.JSONDecodeError:
            logger.warning("Skipping invalid JSONL line: %.80s", line)
            return None

        action = entry.get("action", "")
        field_key = entry.get("field_key", "")
        ts = entry.get("timestamp") or datetime.now().isoformat()

        if action == "create_field":
            return TrainingSample(
                query=f"Create field '{field_key}' in schema",
                expected_tool_call={
                    "name": "create_schema_field",
                    "arguments": {
                        "key": field_key,
                        "widget": entry.get("widget", "input"),
                        "label": entry.get("label", field_key),
                    },
                },
                schema_change={"action": "create_field", "field_key": field_key},
                validation_log=entry.get("validation"),
                timestamp=ts,
            )
        elif action == "update_gate":
            gate_id = entry.get("gate_id", "")
            return TrainingSample(
                query=f"Update gate rule '{gate_id}' for field '{field_key}'",
                expected_tool_call={
                    "name": "update_gate_rule",
                    "arguments": {
                        "gate_id": gate_id,
                        "field": field_key,
                        "operator": entry.get("operator", "eq"),
                        "value": entry.get("value", ""),
                    },
                },
                schema_change={"action": "update_gate", "gate_id": gate_id, "field_key": field_key},
                validation_log=entry.get("validation"),
                timestamp=ts,
            )
        elif action == "delete_field":
            return TrainingSample(
                query=f"Delete field '{field_key}' from schema",
                expected_tool_call={
                    "name": "update_gate_rule",
                    "arguments": {
                        "gate_id": "cleanup",
                        "field": field_key,
                        "action": "remove_rule",
                    },
                },
                schema_change={"action": "delete_field", "field_key": field_key},
                validation_log=entry.get("validation"),
                timestamp=ts,
            )
        return None

    # ── Serialize ──────────────────────────────────────────────────────

    @staticmethod
    def save_to_format(samples: list[TrainingSample], output_dir: str) -> str:
        """
        Save training samples as JSONL, consumable by GRPO trainer.

        Returns path to the output file.
        """
        os.makedirs(output_dir, exist_ok=True)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        out_path = os.path.join(output_dir, f"training_samples_{timestamp}.jsonl")

        with open(out_path, "w", encoding="utf-8") as fh:
            for s in samples:
                fh.write(json.dumps(asdict(s), ensure_ascii=False) + "\n")

        logger.info("save_to_format: %d samples → %s", len(samples), out_path)
        return out_path


# ── CLI entry point ──────────────────────────────────────────────────────


def main() -> None:
    """Minimal CLI: parse domain/constraint JSON → extract groups/triplets."""
    import argparse

    parser = argparse.ArgumentParser(description="BlessStar RL Data Pipeline")
    parser.add_argument("--domain-knowledge", required=True, help="Path to domain_knowledge.json")
    parser.add_argument("--constraint-knowledge", required=True, help="Path to constraint_knowledge.json")
    parser.add_argument("--audit-log-dir", default="", help="Path to audit log directory (optional)")
    parser.add_argument("--output-dir", default="./training_data", help="Output directory for JSONL")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format="%(levelname)s | %(message)s")

    pipeline = BlessStarDataPipeline()

    # Load inputs
    with open(args.domain_knowledge, "r", encoding="utf-8") as f:
        dk = json.load(f)
    with open(args.constraint_knowledge, "r", encoding="utf-8") as f:
        ck = json.load(f)

    groups = pipeline.extract_entity_groups(dk)
    triplets = pipeline.extract_gate_triplets(ck)

    print(f"Entity groups: {len(groups)}")
    print(f"Gate triplets: {len(triplets)}")

    # Build samples from audit logs (if directory exists)
    if args.audit_log_dir:
        samples = pipeline.build_training_samples(args.audit_log_dir)
        out = pipeline.save_to_format(samples, args.output_dir)
        print(f"Training samples: {len(samples)} → {out}")
    else:
        print("No audit log dir; skipping training sample extraction.")


if __name__ == "__main__":
    main()
