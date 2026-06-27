# IntrospectorExtensions.cmake — 多语言扫描模块编译开关
#
# 每个语言插件是一个独立源文件，由 lang_plugin_register() 在 init 时注册。
# 编译开关（可选）：引入新的语言扫描器时只需添加源文件到 SDK。
#
# 用法：在根 CMakeLists.txt 中：
#   include(cmake/IntrospectorExtensions.cmake)
#
# 或直接 target_sources(bs_app_sdk PRIVATE ...) 添加需要的文件。

# 当前支持的语言列表
set(BIZ_INTROSPECTOR_LANG_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/app/sdk/src/biz_introspector/lang_plugin.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/sdk/src/biz_introspector/lang_c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/sdk/src/biz_introspector/lang_go.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/sdk/src/biz_introspector/lang_py.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/sdk/src/biz_introspector/lang_java.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/sdk/src/biz_introspector/lang_yaml.c
)

# 可选扩展：加入 SDK 编译
# target_sources(bs_app_sdk PRIVATE ${BIZ_INTROSPECTOR_LANG_SRCS})
