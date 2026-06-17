import { useRef, useEffect, useState, useCallback } from 'react'
import * as Blockly from 'blockly/core'
import 'blockly/blocks'
import { registerAllBlocks } from './blocks/index'
import { registerBlessStarRenderer } from './renderer'
import { serializeWorkspace } from './serializer'
import { deserializeToWorkspace } from './deserializer'
import type { GateChainDocument } from './serializer'
import { BlessStarBlockRegistry } from './block_registry'

interface BlocklyWorkspaceProps {
  initialJson?: GateChainDocument
  onChange?: (json: GateChainDocument) => void
  className?: string
  maxDepth?: number
}

const TOOLBOX_CONFIG: Blockly.utils.toolbox.ToolboxDefinition = {
  kind: 'categoryToolbox',
  contents: [
    {
      kind: 'category',
      name: 'Gate',
      colour: '#1a56db',
      contents: [
        { kind: 'block', type: 'bs_gate_default' },
        { kind: 'block', type: 'bs_condition' },
        { kind: 'block', type: 'bs_meta_rule' },
        { kind: 'block', type: 'bs_custom_gate' },
      ],
    },
    {
      kind: 'category',
      name: '逻辑',
      colour: '#10b981',
      contents: [
        { kind: 'block', type: 'bs_logic_and' },
        { kind: 'block', type: 'bs_logic_or' },
      ],
    },
    {
      kind: 'category',
      name: '策略',
      colour: '#f59e0b',
      contents: [{ kind: 'block', type: 'bs_policy_attr' }],
    },
  ],
}

function BlocklyWorkspace({
  initialJson,
  onChange,
  className = '',
  maxDepth = 3,
}: BlocklyWorkspaceProps) {
  const containerRef = useRef<HTMLDivElement>(null)
  const workspaceRef = useRef<Blockly.WorkspaceSvg | null>(null)
  const [isReady, setIsReady] = useState(false)

  useEffect(() => {
    registerBlessStarRenderer()
    registerAllBlocks()
  }, [])

  useEffect(() => {
    if (!containerRef.current || workspaceRef.current) return

    const workspace = Blockly.inject(containerRef.current, {
      toolbox: TOOLBOX_CONFIG,
      renderer: 'blessstar_renderer',
      theme: Blockly.Theme.defineTheme('blessstar', {
        base: Blockly.Themes.Classic,
        name: 'blessstar',
        componentStyles: {
          workspaceBackgroundColour: '#0f172a',
          toolboxBackgroundColour: '#1e293b',
          toolboxForegroundColour: '#f8fafc',
          flyoutBackgroundColour: '#1e293b',
          flyoutForegroundColour: '#f8fafc',
          flyoutOpacity: 0.9,
          scrollbarColour: '#334155',
          scrollbarOpacity: 0.5,
        },
      }),
      zoom: {
        controls: true,
        wheel: true,
        startScale: 0.9,
        maxScale: 2,
        minScale: 0.3,
        scaleSpeed: 1.1,
      },
      trashcan: true,
      move: {
        scrollbars: true,
        drag: true,
        wheel: true,
      },
      sounds: false,
    })

    workspaceRef.current = workspace

    const changeListener = () => {
      if (!onChange) return
      const json = serializeWorkspace(workspace)
      onChange(json)
    }

    workspace.addChangeListener(changeListener)

    setIsReady(true)

    return () => {
      workspace.removeChangeListener(changeListener)
      workspace.dispose()
      workspaceRef.current = null
      setIsReady(false)
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  useEffect(() => {
    if (!workspaceRef.current || !isReady) return
    if (!initialJson) return

    const currentJson = serializeWorkspace(workspaceRef.current)
    if (JSON.stringify(currentJson) === JSON.stringify(initialJson)) return

    deserializeToWorkspace(workspaceRef.current, initialJson)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [initialJson, isReady])

  const handleExport = useCallback((): string => {
    if (!workspaceRef.current) return '[]'
    const json = serializeWorkspace(workspaceRef.current)
    return JSON.stringify(json, null, 2)
  }, [])

  const handleImport = useCallback(
    (jsonStr: string): boolean => {
      if (!workspaceRef.current) return false
      try {
        const doc = JSON.parse(jsonStr) as GateChainDocument
        deserializeToWorkspace(workspaceRef.current, doc)
        return true
      } catch {
        return false
      }
    },
    []
  )

  const checkDepth = useCallback((): boolean => {
    if (!workspaceRef.current) return true
    const topBlocks = workspaceRef.current.getTopBlocks(false)
    let maxFound = 0

    function walk(block: Blockly.Block, depth: number): void {
      maxFound = Math.max(maxFound, depth)
      const children = block.getChildren(false)
      for (const child of children) {
        walk(child, depth + 1)
      }
    }

    for (const block of topBlocks) {
      walk(block, 0)
    }

    return maxFound <= maxDepth
  }, [maxDepth])

  const getWorkspace = useCallback((): Blockly.WorkspaceSvg | null => {
    return workspaceRef.current
  }, [])

  return (
    <div className={`blockly-workspace-container ${className}`}>
      <div
        ref={containerRef}
        className="blockly-editor-area"
        style={{
          width: '100%',
          height: '100%',
          minHeight: '400px',
        }}
      />
      <style>{`
        .blockly-workspace-container .blocklyToolboxDiv {
          background-color: #1e293b !important;
          border-right: 1px solid #334155;
        }
        .blockly-workspace-container .blocklyTreeRow {
          height: 36px;
          color: #f8fafc;
          font-family: 'SF Pro', system-ui, sans-serif;
          font-size: 13px;
        }
        .blockly-workspace-container .blocklyTreeRow:hover {
          background-color: #334155;
        }
        .blockly-workspace-container .blocklyTreeSelected {
          background-color: #1a56db !important;
        }
        .blockly-workspace-container .blocklyFlyout {
          background-color: #1e293b;
        }
        .blockly-workspace-container .blocklyFlyoutLabelText {
          fill: #94a3b8 !important;
        }
      `}</style>
    </div>
  )
}

export { BlocklyWorkspace }
export type { BlocklyWorkspaceProps }
export default BlocklyWorkspace
