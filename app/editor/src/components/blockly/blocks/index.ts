import registerGateDefault from './gate_default'
import registerCondition from './condition'
import registerMetaRule from './meta_rule'
import registerLogicOperator from './logic_operator'
import registerPolicyAttr from './policy_attr'
import registerCustomGate from './custom_gate'

export function registerAllBlocks(): void {
  registerGateDefault()
  registerCondition()
  registerMetaRule()
  registerLogicOperator()
  registerPolicyAttr()
  registerCustomGate()
}

export {
  registerGateDefault,
  registerCondition,
  registerMetaRule,
  registerLogicOperator,
  registerPolicyAttr,
  registerCustomGate,
}
