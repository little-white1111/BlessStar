import * as Blockly from 'blockly/core'
import { BlessStarBlockRegistry } from './block_registry'

interface GateChainNode {
  type: string
  [key: string]: any
}

interface GateChainDocument {
  version: string
  gates: GateChainNode[]
}

function deserializeSingleNode(
  workspace: Blockly.WorkspaceSvg,
  node: GateChainNode,
  parentBlock?: Blockly.Block,
  parentInputName?: string
): Blockly.Block | null {
  const def = BlessStarBlockRegistry.getBlockDef(findBlockType(node))
  if (!def) {
    console.warn(`[Deserializer] Unknown block type for node type: ${node.type}`)
    return null
  }

  const blockType = def.type
  const block = workspace.newBlock(blockType)

  if (def.deserializer) {
    const fieldValues = def.deserializer(node)
    for (const [key, value] of Object.entries(fieldValues)) {
      try {
        block.setFieldValue(value as string, key)
      } catch {
        // field may not exist on this block
      }
    }
  }

  (block as any).setEnabled(true)

  if (parentBlock && parentInputName) {
    const input = parentBlock.getInput(parentInputName)
    if (input) {
      const connection = input.connection
      if (connection) {
        // Connect statement or value blocks
        const blockConnection =
          block.previousConnection || block.outputConnection
        if (blockConnection && connection.targetConnection === null) {
          connection.connect(blockConnection)
        }
      }
    }
  }

  return block
}

function findBlockType(node: GateChainNode): string {
  const typeMap: Record<string, string> = {
    gate_default: 'bs_gate_default',
    condition: 'bs_condition',
    meta_rule: 'bs_meta_rule',
    logic_AND: 'bs_logic_and',
    logic_OR: 'bs_logic_or',
    policy_attr: 'bs_policy_attr',
    custom_gate: 'bs_custom_gate',
  }

  return typeMap[node.type] || `bs_${node.type}`
}

function deserializeToWorkspace(
  workspace: Blockly.WorkspaceSvg,
  jsonDoc: GateChainDocument
): void {
  workspace.clear()

  for (const gateNode of jsonDoc.gates) {
    const topBlock = deserializeSingleNode(workspace, gateNode)
    if (!topBlock) continue

    if (gateNode.do && Array.isArray(gateNode.do)) {
      deserializeChildren(workspace, topBlock, gateNode.do, 'DO')
    }

    if (gateNode.then && Array.isArray(gateNode.then)) {
      deserializeChildren(workspace, topBlock, gateNode.then, 'THEN')
    }

    if (gateNode.else && Array.isArray(gateNode.else)) {
      deserializeChildren(workspace, topBlock, gateNode.else, 'ELSE')
    }

    if (gateNode.if) {
      const ifBlock = deserializeSingleNode(workspace, gateNode.if, topBlock, 'IF')
      if (ifBlock) {
        (ifBlock as any).setEnabled(true)

        if (gateNode.if.A) {
          deserializeSingleNode(workspace, gateNode.if.A, ifBlock, 'A')
        }
        if (gateNode.if.B) {
          deserializeSingleNode(workspace, gateNode.if.B, ifBlock, 'B')
        }
      }
    }

    if (gateNode.A) {
      deserializeSingleNode(workspace, gateNode.A, topBlock, 'A')
    }
    if (gateNode.B) {
      deserializeSingleNode(workspace, gateNode.B, topBlock, 'B')
    }

    (topBlock as any).initSvg()
    (topBlock as any).render()
  }
}

function deserializeChildren(
  workspace: Blockly.WorkspaceSvg,
  parentBlock: Blockly.Block,
  children: GateChainNode[],
  inputName: string
): void {
  let prevChild: Blockly.Block | null = null

  for (const childNode of children) {
    const childBlock = deserializeSingleNode(workspace, childNode)
    if (!childBlock) continue

    if (prevChild) {
      const nextConn = prevChild.nextConnection
      const prevConn = childBlock.previousConnection
      if (nextConn && prevConn && nextConn.targetConnection === null) {
        nextConn.connect(prevConn)
      }
    } else {
      const input = parentBlock.getInput(inputName)
      if (input && input.connection) {
        const blockConn = childBlock.previousConnection
        if (blockConn && input.connection.targetConnection === null) {
          input.connection.connect(blockConn)
        }
      }
    }

    (childBlock as any).initSvg()
    (childBlock as any).render()

    prevChild = childBlock
  }
}

export { deserializeToWorkspace, deserializeSingleNode, findBlockType }
export type { GateChainNode, GateChainDocument }
