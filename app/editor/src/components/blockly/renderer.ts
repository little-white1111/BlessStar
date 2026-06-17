import * as Blockly from 'blockly/core'

const BLESSSTAR_PRIMARY = '#1a56db'
const BLESSSTAR_ACCENT = '#06b6d4'
const BLESSSTAR_SUCCESS = '#10b981'
const BLESSSTAR_WARNING = '#f59e0b'
const BLESSSTAR_SURFACE = '#1e293b'
const BLESSSTAR_TEXT = '#f8fafc'

class BlessStarRenderer extends Blockly.blockRendering.Renderer {
  constructor(name: string) {
    super(name)
  }

  protected makeConstants_(): Blockly.blockRendering.ConstantProvider {
    return new BlessStarConstantProvider()
  }
}

class BlessStarConstantProvider extends Blockly.blockRendering.ConstantProvider {
  constructor() {
    super()
    this.FIELD_TEXT_FONTSIZE = 12
    this.FIELD_TEXT_FONTWEIGHT = '500'
    this.FIELD_TEXT_FONTFAMILY = "'SF Mono', 'Fira Code', 'Consolas', monospace"
    this.FIELD_BORDER_RECT_COLOUR = BLESSSTAR_SURFACE
  }

  override init(): void {
    super.init()
    this.updateColors()
  }

  private updateColors(): void {
    if (this.FIELD_BORDER_RECT_COLOUR) {
      this.FIELD_BORDER_RECT_COLOUR = BLESSSTAR_SURFACE
    }
  }

  override shapeFor(connection: Blockly.RenderedConnection): Blockly.blockRendering.IPShape | null {
    let shape: Blockly.blockRendering.IPShape | null = null

    switch (connection.type) {
      case Blockly.PREVIOUS_STATEMENT:
      case Blockly.NEXT_STATEMENT:
        shape = new RectangularShape(this, 'statement')
        break
      case Blockly.INPUT_VALUE:
      case Blockly.OUTPUT_VALUE:
        shape = new RectangularShape(this, 'input')
        break
      default:
        shape = super.shapeFor(connection)
    }

    return shape
  }
}

class RectangularShape implements Blockly.blockRendering.IPShape {
  readonly width: number
  readonly height: number
  readonly pathLeft: string
  readonly pathRight: string
  readonly pathDown: string
  readonly pathUp: string

  constructor(constants: Blockly.blockRendering.ConstantProvider, type: 'statement' | 'input') {
    const r = type === 'statement' ? 4 : 6
    const w = type === 'statement' ? 20 : 16
    const h = type === 'statement' ? 12 : 16

    this.width = w
    this.height = h

    const outerR = Math.min(r, w / 2, h / 2)
    const rectPath = [
      `M 0,${outerR}`,
      `A ${outerR},${outerR} 0 0,1 ${outerR},0`,
      `L ${w - outerR},0`,
      `A ${outerR},${outerR} 0 0,1 ${w},${outerR}`,
      `L ${w},${h - outerR}`,
      `A ${outerR},${outerR} 0 0,1 ${w - outerR},${h}`,
      `L ${outerR},${h}`,
      `A ${outerR},${outerR} 0 0,1 0,${h - outerR}`,
      'Z',
    ].join(' ')

    this.pathLeft = rectPath
    this.pathRight = rectPath
    this.pathDown = rectPath
    this.pathUp = rectPath
  }
}

function registerBlessStarRenderer(): void {
  Blockly.blockRendering.register('blessstar_renderer', BlessStarRenderer)
}

export {
  BlessStarRenderer,
  BlessStarConstantProvider,
  registerBlessStarRenderer,
  BLESSSTAR_PRIMARY,
  BLESSSTAR_ACCENT,
  BLESSSTAR_SUCCESS,
  BLESSSTAR_WARNING,
  BLESSSTAR_SURFACE,
  BLESSSTAR_TEXT,
}
