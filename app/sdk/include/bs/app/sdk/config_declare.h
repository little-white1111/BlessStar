#ifndef BS_APP_SDK_CONFIG_DECLARE_H
#define BS_APP_SDK_CONFIG_DECLARE_H

/*
 * ── bs_config_declare(): 全局注册 C ABI ─────────────────────────────
 * 业务系统调用此接口一次性声明全部配置字段。声明即索引——
 * 字段信息序列化为 Schema JSON，写入共享内存头部自描述区域。
 *
 * 使用示例（C）：
 *   bs_config_declare(BS_CONFIG_FIELDS(
 *       BS_FIELD("db.host", BS_TYPE_STRING, "localhost", "数据库地址"),
 *       BS_FIELD("db.port", BS_TYPE_INT32,  3306,        "端口号"),
 *       BS_FIELD("db.ssl",  BS_TYPE_BOOL,   false,       "是否启用 SSL"),
 *   ));
 *
 * 幂等性：重复调用会覆盖已声明的同名 key 的定义（匹配更新），
 * 不影响未变动的字段。
 *
 * 线程安全：内部加锁。
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Supported field types ────────────────────────────────────────── */
typedef enum {
    BS_TYPE_INT32  = 0,
    BS_TYPE_INT64  = 1,
    BS_TYPE_STRING = 2,
    BS_TYPE_DOUBLE = 3,
    BS_TYPE_BOOL   = 4,
    BS_TYPE_FILE   = 5,   /* 文件路径/目录/URL, 值应被理解为文件系统路径 */
} bs_field_type_t;

/* ── Field declaration ────────────────────────────────────────────── */
typedef struct {
    const char*      key;             /* 字段全路径 key，如 "db.host"          */
    bs_field_type_t  type;            /* 字段类型                                */
    const char*      default_str;     /* 默认值（统一用字符串表达，由 SDK 转换） */
    const char*      description;     /* 人读语义（ai_hint 初始值）              */
    bool             required;        /* 是否必填                                */
} bs_field_decl_t;

/* ── Convenience macros ──────────────────────────────────────────── */

/* Obtain field count at compile time */
#define BS_CONFIG_FIELD_COUNT(fields) (sizeof(fields) / sizeof(fields[0]))

/* Define default-value helpers */
#define BS_DEF_STR(s)   (s)
#define BS_DEF_INT32(n) _bs_def_str_ ## n  /* use default_str */
#define BS_DEF_BOOL(b)  (b ? "true" : "false")

/* ── one-shot macro: wraps declaration in a static array ─────────── */
#define BS_CONFIG_FIELDS(...)   \
    static const bs_field_decl_t __bs_declared_fields_##__LINE__[] = { __VA_ARGS__ }; \
    bs_config_declare(__bs_declared_fields_##__LINE__, \
                      sizeof(__bs_declared_fields_##__LINE__) / sizeof(bs_field_decl_t))

#define BS_FIELD(key, type, defval_str, desc) \
    { key, type, defval_str, desc, false }

#define BS_FIELD_REQ(key, type, defval_str, desc) \
    { key, type, defval_str, desc, true }

/* ── Core API ────────────────────────────────────────────────────── */

/**
 * bs_config_declare() - 一次性声明全部配置字段
 *
 * @fields:   字段声明数组（bs_field_decl_t 数组）
 * @count:    数组元素个数
 * @return:   0 成功，-1 失败（日志输出错误原因）
 *
 * 约束：
 *   - key 不能为 NULL 或空字符串
 *   - 重复 key 覆盖旧声明（取最后一次）
 *   - 内部通过共享内存 writer 将 Schema JSON 写入头部描述区
 */
int bs_config_declare(const bs_field_decl_t* fields, size_t count);

/**
 * bs_config_declare_get_schema_json() - 获取当前注册的 Schema JSON
 * @out_json: 输出参数，接收 Schema JSON 字符串（调用者须 free）
 * @out_len:  输出参数，JSON 字符串长度
 * @return:   0 成功，-1 尚无声明
 */
int bs_config_declare_get_schema_json(char** out_json, size_t* out_len);

/**
 * bs_config_read() - 按 key 读取运行时配置值
 *
 * @key:      字段全路径 key，如 "livedesign.danmaku.font_size"
 * @return:   字符串值（调用者须 free），NULL 表示 key 不存在
 *
 * 查找顺序：
 *   1. 运行时值存储（bs_config_write 写入的）
 *   2. 注册时的默认值（bs_config_declare 声明的）
 *   3. 不存在返回 NULL
 */
char* bs_config_read(const char* key);

/**
 * bs_config_write() - 按 key 写入运行时配置值
 *
 * @key:      字段全路径 key
 * @value:    字符串值（SDK 内部根据需要转换类型）
 * @return:   0 成功，-1 key 不存在或参数无效
 *
 * 写入后：
 *   - 更新运行时值存储
 *   - 如果 SHM 已初始化，尝试同步写入 SHM
 *   - 数据不持久化到 WAL（仅内存 + SHM）
 */
int bs_config_write(const char* key, const char* value);

/**
 * bs_config_declare_reset() - 清空所有声明（测试用）
 */
void bs_config_declare_reset(void);

/**
 * bs_config_declare_get_dirty_keys() - 获取本次变更的脏字段列表（第 2 层增量序列化用）
 * @out_keys:   输出参数，接收脏字段 key 数组（每个字符串调用者须 free）
 * @out_count:  输出参数，脏字段数量
 * @return:     0 成功，-1 失败
 *
 * 调用 bs_config_declare_clear_dirty() 清空列表。
 */
int bs_config_declare_get_dirty_keys(const char*** out_keys, size_t* out_count);

/**
 * bs_config_declare_clear_dirty() - 清空脏字段列表
 */
void bs_config_declare_clear_dirty(void);

/**
 * bs_config_declare_get_version() - 获取当前版本号（B4 CAS 乐观锁）
 * @return: 当前版本号（单调递增）
 */
uint64_t bs_config_declare_get_version(void);

/**
 * bs_config_commit_check_version() - CAS 版本检测（B4 多窗口并发安全）
 * @expected: 调用方持有的预期版本号
 * @return:   0 = 版本匹配（可安全提交），-1 = 版本冲突（版本已变化）
 *
 * 如果版本匹配，自动将版本号 +1（原子递增）。
 * 如果版本不匹配，返回冲突状态，调用方应提示用户刷新。
 */
int bs_config_commit_check_version(uint64_t expected);

/**
 * bs_config_declare_field_c() — 专题五 A4: 声明单个配置字段（C ABI 便利封装）
 * @key:         字段标识符（如 "livedesign.room.id"）
 * @type:        bs_field_type_t 枚举值（0=INT32, 1=INT64, 2=STRING, 3=DOUBLE, 4=BOOL）
 * @default_str: 默认值字符串
 * @description: 字段描述
 * @required:    非零表示必填
 * @return:      0 成功，-1 失败
 */
int bs_config_declare_field_c(const char* key, int type,
                               const char* default_str,
                               const char* description,
                               int required);

/**
 * bs_config_persist_write_c() — 将 runtime_values 序列化为扁平 JSON 并原子写入文件
 *
 * @param file_path  目标文件路径（configs.json）
 * @return 0 成功，-1 失败
 *
 * 原子写入协议：write(tmp) + flush + rename(tmp → target)
 * 仅写 dirty keys（runtime_values 中的 key/value 对）
 */
int bs_config_persist_write_c(const char* file_path);

/**
 * bs_config_persist_load_c() — 从 JSON 文件读取并加载到 runtime_values
 *
 * @param file_path  源文件路径（configs.json）
 * @return 0 成功（含文件不存在），-1 解析失败
 *
 * 格式同 bs_config_persist_write_c 的输出：扁平 key-value JSON
 * 文件不存在时视为空运行时值，回退到 field.default_str
 */
int bs_config_persist_load_c(const char* file_path);

#ifdef __cplusplus
}
#endif

#endif /* BS_APP_SDK_CONFIG_DECLARE_H */
