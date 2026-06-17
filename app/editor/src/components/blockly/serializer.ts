import type { Block } from 'blockly'
import * as Blockly from 'blockly/core'
import { BlessStarBlockRegistry } from './block_registry'

interface GateChainNode {
  type: string
  [key: string]: any
}

interface GateChainDocument {
  version: '1.0'
  gates: GateChainNode[]
}

function extractAllBlocks(rootBlock: Block | null): Block[] {
  const blocks: Block[] = []
  function walk(block: Block | null): void {
    if (!block) return
    blocks.push(block)
    const children = block.getChildren(false)
    for (const child of children) {
      walk(child)
    }
  }
  walk(rootBlock)
  return blocks
}

function serializeBlockToJson(block: Block, generator: Blockly.Generator): GateChainNode | null {
  const func = generator.forBlock[block.type]
  if (!func) {
    console.warn(`[Serializer] No generator for block type: ${block.type}`)
    return null
  }

  const code = func(block, generator)
  try {
    return JSON.parse(code) as GateChainNode
  } catch {
    console.error(`[Serializer] Failed to parse generator output for block ${block.type}:`, code)
    return null
  }
}

function serializeWorkspace(
  workspace: Blockly.Workspace | Blockly.WorkspaceSvg
): GateChainDocument {
  const generator = new Blockly.Generator('GateChainJSON')
  generator.forBlock = getGeneratorForBlocks()

  const topBlocks = workspace.getTopBlocks(false)
  const gates: GateChainNode[] = []

  for (const topBlock of topBlocks) {
    const node = serializeBlockToJson(topBlock, generator)
    if (node) {
      gates.push(node)
    }
  }

  return {
    version: '1.0',
    gates,
  }
}

function getGeneratorForBlocks(): Record<string, (block: Block, generator: Blockly.Generator) => string> {
  const defs = BlessStarBlockRegistry.getAllBlockDefs()
  const result: Record<string, (block: Block, generator: Blockly.Generator) => string> = {}

  for (const def of defs) {
    result[def.type] = def.generator
  }

  return result
}

export { serializeWorkspace, serializeBlockToJson, extractAllBlocks }
export type { GateChainNode, GateChainDocument }
