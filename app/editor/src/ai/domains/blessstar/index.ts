/**
 * BlessStar Domain Plugin — entry point.
 *
 * ADR-全链路接通 — implements IDomainPlugin for the BlessStar config domain.
 *
 * Architecture invariances:
 *  - #1 (双向依赖禁止): Engine → IDomainPlugin. This plugin never imports engine internals.
 *  - #3 (Plugin 隔离性): All tools/prompts/concepts are namespaced under 'blessstar'.
 *  - #9 (Plugin 版本契约): version + supportedPipelineVersion are checked on registration.
 */

import type { IDomainPlugin, IPersistenceProvider } from '../../engine/types'
import type { IToolDeclaration, IConceptEntry, ICommandEntry, ITrieDict } from '../../engine/types'
import { BLESSSTAR_TOOLS } from './tools'
import { getBlessStarSystemPrompt, getBlessStarReplyPrompt } from './prompts'
import { BLESSSTAR_CONCEPTS } from './concepts'
import { BLESSSTAR_COMMANDS } from './commands'
import { BlessStarPersistenceProvider } from './persistence'

export class BlessStarDomainPlugin implements IDomainPlugin {
  id = 'blessstar'
  version = '1.0.0'
  supportedPipelineVersion = '1.0.0'

  tools: IToolDeclaration[] = BLESSSTAR_TOOLS
  private _persistenceProvider: IPersistenceProvider = new BlessStarPersistenceProvider()

  private _trieDict: ITrieDict = {
    domainKW: {
      'config': 'blessstar',
      '配置': 'blessstar',
      'workspace': 'blessstar',
      '工作区': 'blessstar',
      'gate': 'blessstar',
      '门控': 'blessstar',
      'schema': 'blessstar',
      'profile': 'blessstar',
    },
    opKW: {
      'read': '读',
      '读取': '读',
      'write': '写',
      '写入': '写',
      '修改': '写',
      '更新': '写',
      '改成': '写',
      '改为': '写',
      'delete': '删',
      '删除': '删',
      'list': '列',
      '列出': '列',
      'validate': '验',
      '校验': '验',
      '验证': '验',
      'build': '建',
      '构建': '建',
      'export': '导',
      '导出': '导',
    },
    opMap: {
      '读': 'read',
      '写': 'write',
      '删': 'delete',
      '列': 'list',
      '验': 'validate',
      '建': 'build',
      '导': 'export',
    },
  }

  getSystemPrompt(): string {
    return getBlessStarSystemPrompt() + '\n\n' + getBlessStarReplyPrompt()
  }

  getConceptKnowledge(): IConceptEntry[] {
    return BLESSSTAR_CONCEPTS
  }

  getTrieDictionary(): ITrieDict {
    return this._trieDict
  }

  getCommandRegistry(): ICommandEntry[] {
    return BLESSSTAR_COMMANDS
  }

  getPersistenceProvider(): IPersistenceProvider {
    return this._persistenceProvider
  }
}

/** Singleton instance for registration. */
export const blessstarPlugin = new BlessStarDomainPlugin()
