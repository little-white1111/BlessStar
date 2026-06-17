import type { Block, Generator } from 'blockly'

export interface RegisterBlockDef {
  type: string
  category: string
  colour: number
  jsonDef: any
  generator: (block: Block, generator: Generator) => string
  deserializer?: (json: any) => Record<string, any>
}

class BlessStarBlockRegistryImpl {
  private blocks = new Map<string, RegisterBlockDef>()
  private categories = new Map<string, RegisterBlockDef[]>()

  registerBlockType(type: string, def: RegisterBlockDef): void {
    if (this.blocks.has(type)) {
      console.warn(`[BlessStarBlockRegistry] Block type "${type}" already registered, overwriting.`)
    }
    this.blocks.set(type, def)

    const cat = def.category
    if (!this.categories.has(cat)) {
      this.categories.set(cat, [])
    }
    this.categories.get(cat)!.push(def)
  }

  getBlockDef(type: string): RegisterBlockDef | undefined {
    return this.blocks.get(type)
  }

  getAllBlockDefs(): RegisterBlockDef[] {
    return Array.from(this.blocks.values())
  }

  getCategories(): [string, RegisterBlockDef[]][] {
    return Array.from(this.categories.entries())
  }

  hasBlockType(type: string): boolean {
    return this.blocks.has(type)
  }

  getCategoryNames(): string[] {
    return Array.from(this.categories.keys())
  }

  clear(): void {
    this.blocks.clear()
    this.categories.clear()
  }
}

export const BlessStarBlockRegistry = new BlessStarBlockRegistryImpl()
