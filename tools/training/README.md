# BlessStar RL 训练环境说明

## 概述

本目录包含 BlessStar 专属 SAKE 式 RL（强化学习）微调管道，用于提升 Agent 在 BlessStar 配置编辑场景中的准确性。

**AGF-06**: 训练数据仅来源于 BlessStar 配置变更历史 + 校验日志，禁止引入模型自身生成的合成数据。

## 文件结构

| 文件 | 用途 |
|------|------|
| `data_pipeline.py` | 配置变更历史 → RL 训练样本管道（JSONL） |
| `sake_trainer.py` | SAKE 式 GRPO 训练脚本骨架（LoRA + Qwen2.5-7B） |
| `README.md` | 本文档 |

`tools/inference/sake_infer.py` — 推理入口骨架。

## 依赖（训练环境）

### 生产环境（GPU）

```bash
pip install torch>=2.1.0 transformers>=4.36.0 datasets>=2.16.0
pip install peft>=0.7.0 trl>=0.7.0 vllm>=0.3.0  # GRPO 训练
pip install accelerate>=0.25.0 deepspeed>=0.13.0  # 分布式
```

推荐配置：1× A100-80GB（全参数微调 Qwen2.5-7B）或 1× RTX 4090（LoRA）。

### 骨架验证（无 GPU）

```bash
# 数据管道验证
python tools/training/data_pipeline.py \
    --domain-knowledge .cursor/agents/biz-default/agent_index/domain_knowledge.json \
    --constraint-knowledge .cursor/agents/biz-default/agent_index/constraint_knowledge.json

# 训练骨架验证
python tools/training/sake_trainer.py --dry-run

# 推理骨架验证
python tools/inference/sake_infer.py --dry-run
```

## 工作流

1. **数据积累阶段**（~30 天）：BlessStar 配置变更历史 → audit logs
2. **数据提取**：`data_pipeline.py --audit-log-dir <path>` → JSONL
3. **训练**：`sake_trainer.py --data-path ./training_data/*.jsonl` → LoRA adapter
4. **推理**：`sake_infer.py --model-path <path> --prompt "..."`

## 关键设计点

- **GRPO**（Group Relative Policy Optimization）：SAKE 论文 Algorithm 1，三回合 rollout + 课程奖励
- **课程奖励**（Curriculum Reward）：前三回合逐步加入格式 → 正确性 → 安全性奖励
- **LoRA**：秩 16，alpha 32，仅训练 adapter 节省显存
- **AGF-05 三层结构**：Planner（规划）→ Executor（执行）→ Reflection（反思）
