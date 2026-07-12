/**
 * CatchphraseEngine（口头禅引擎）— 模块入口
 *
 * 组装所有子模块，提供统一的外部接口。
 * 架构不变量 D1-D6 在本模块中全面落实。
 */

import { CoreCatchphraseManager } from './core-manager';
import { RelationalCatchphraseManager } from './relational-manager';
import { EmotionTriggerMatcher } from './emotion-trigger';
import { ScenarioTriggerMatcher } from './scenario-trigger';
import { FrequencyGovernor } from './frequency-governor';
import { CatchphraseSelector, type ConsistencyChecker } from './selector';
import { UserConfirmationService } from './user-confirmation';
import type { CatchphraseMatch, CatchphraseSelectionConfig } from './types';

export type { CatchphraseMatch, CatchphraseSelectionConfig } from './types';
export type { ConsistencyChecker } from './selector';
export type { ConfirmationResult } from './user-confirmation';
export type { CoreCatchphraseItem, RelationalCatchphrase } from './types';

export {
  CoreCatchphraseManager,
  RelationalCatchphraseManager,
  EmotionTriggerMatcher,
  ScenarioTriggerMatcher,
  FrequencyGovernor,
  CatchphraseSelector,
  UserConfirmationService,
};

export class CatchphraseEngine {
  readonly coreManager: CoreCatchphraseManager;
  readonly relationalManager: RelationalCatchphraseManager;
  readonly emotionTrigger: EmotionTriggerMatcher;
  readonly scenarioTrigger: ScenarioTriggerMatcher;
  readonly frequencyGovernor: FrequencyGovernor;
  readonly selector: CatchphraseSelector;
  readonly confirmationService: UserConfirmationService;

  constructor(config?: {
    coreManager?: CoreCatchphraseManager;
    relationalManager?: RelationalCatchphraseManager;
    emotionTrigger?: EmotionTriggerMatcher;
    scenarioTrigger?: ScenarioTriggerMatcher;
    frequencyGovernor?: FrequencyGovernor;
    consistencyChecker?: ConsistencyChecker;
    configReader?: import('../../ports/config-reader').ConfigReader;
  }) {
    this.coreManager = config?.coreManager ?? new CoreCatchphraseManager(config?.configReader);
    this.relationalManager = config?.relationalManager ?? new RelationalCatchphraseManager();
    this.emotionTrigger = config?.emotionTrigger ?? new EmotionTriggerMatcher();
    this.scenarioTrigger = config?.scenarioTrigger ?? new ScenarioTriggerMatcher();
    this.frequencyGovernor = config?.frequencyGovernor ?? new FrequencyGovernor();
    this.selector = new CatchphraseSelector(
      this.coreManager,
      this.relationalManager,
      this.emotionTrigger,
      this.scenarioTrigger,
      this.frequencyGovernor,
      config?.consistencyChecker,
    );
    this.confirmationService = new UserConfirmationService(this.relationalManager);
  }

  /**
   * 初始化：加载核心口头禅配置
   */
  async initialize(): Promise<void> {
    await this.coreManager.load();
  }

  /**
   * 选择并获取最佳口头禅
   * 架构不变量 D1: 一致性检查在 selector.select() 内部完成
   * 架构不变量 D3: 频率限制在 selector.select() 内部完成
   * 架构不变量 D6: 语气强度调制在 selector.select() 内部完成
   *
   * @param config 选择配置
   */
  select(config: CatchphraseSelectionConfig): CatchphraseMatch | undefined {
    return this.selector.select(config);
  }
}
