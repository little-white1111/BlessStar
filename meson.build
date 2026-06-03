project(
  'blessstar_pure_kernel_skeleton',
  ['c', 'cpp'],
  version: '0.1.0',
  default_options: [
    'cpp_std=c++17',
    'warning_level=2',
    'werror=false',
  ],
)

python = import('python').find_installation(required: true)

run_command(
  python,
  meson.project_source_root() / 'tools' / 'purity' / 'verify_manifest.py',
  'verify',
  '--kernel', meson.project_source_root() / 'factory',
  '--manifest', meson.project_source_root() / 'factory' / 'manifest.sha256',
  check: true,
)

inc_common = include_directories('kernel/common/include')
inc_ir = include_directories('kernel/ir/include')
inc_adapter = include_directories('adapter/include')
inc_pipeline = include_directories('kernel/pipeline/include')
inc_report = include_directories('kernel/report/include')

bs_kernel_common = static_library(
  'bs_kernel_common',
  sources: ['kernel/common/src/bs_safe_format.c'],
  include_directories: [inc_common],
)

bs_kernel_ir = static_library(
  'bs_kernel_ir',
  sources: [
    'kernel/ir/src/ir.cpp',
    'kernel/ir/src/requirements.cpp',
    'kernel/ir/src/resolver.cpp',
    'kernel/ir/src/ir_gate.cpp',
    'kernel/ir/src/ir_plugin.cpp',
  ],
  include_directories: [inc_ir],
)

bs_adapter_requirement = static_library(
  'bs_adapter_requirement',
  sources: ['adapter/src/requirement_filter.cpp'],
  include_directories: [inc_adapter, inc_ir],
  link_with: [bs_kernel_ir],
)

bs_kernel_pipeline = static_library(
  'bs_kernel_pipeline',
  sources: [
    'kernel/pipeline/src/pipeline.cpp',
    'kernel/pipeline/src/stage.c',
  ],
  include_directories: [inc_pipeline, inc_ir, inc_report],
  link_with: [bs_kernel_ir],
)

bs_kernel_report = static_library(
  'bs_kernel_report',
  sources: ['kernel/report/src/report.cpp'],
  include_directories: [inc_common, inc_report, inc_pipeline, inc_ir],
  link_with: [bs_kernel_pipeline, bs_kernel_common],
)

bs_adapter_cli = executable(
  'bs_adapter_cli',
  sources: ['adapter/cli/src/main.cpp'],
  include_directories: [inc_report, inc_pipeline, inc_ir],
  link_with: [bs_kernel_report],
  dependencies: [],
)

bs_test_requirements = executable(
  'bs_test_requirements',
  sources: ['kernel/ir/test/RequirementsTest.cpp'],
  include_directories: [inc_ir],
  link_with: [bs_kernel_ir],
)
test('bs_test_requirements', bs_test_requirements)

bs_test_resolver = executable(
  'bs_test_resolver',
  sources: ['kernel/ir/test/ResolverTest.cpp'],
  include_directories: [inc_ir],
  link_with: [bs_kernel_ir],
)
test('bs_test_resolver', bs_test_resolver)

bs_test_requirement_filter = executable(
  'bs_test_requirement_filter',
  sources: ['adapter/test/RequirementFilterTest.cpp'],
  include_directories: [inc_ir, inc_adapter],
  link_with: [bs_adapter_requirement],
)
test('bs_test_requirement_filter', bs_test_requirement_filter)
