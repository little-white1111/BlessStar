// Package adapter_blessstar 自动生成于 BlessStar 配置端口-适配器
// 业务系统: 抖音商城 (douyin-mall)
// 领域: 用户管理
// 请勿手动修改 — 由 blessstar-codegen 自动生成

package adapter_blessstar

import (
	"context"
	"sync"

	"douyin-mall-go-template/ports"
)

// UserConfigAdapter 用户管理 域配置的 BlessStar 适配器
// 内置三阶段降级: ConfigReader实时查询 → LastKnownGood缓存 → 硬编码默认值
type UserConfigAdapter struct {
	reader          ports.ConfigReader
	lastKnownCache  sync.Map
	hardcodedDefaults  map[string]interface{}
}

// NewUserConfigAdapter 创建 UserConfigAdapter 适配器实例
// reader 参数是配置读取器，由业务方注入（可为 CachedReader、HTTPReader 等实现）
func NewUserConfigAdapter(reader ports.ConfigReader) ports.UserConfig {
	return &UserConfigAdapter{
		reader: reader,
		hardcodedDefaults: map[string]interface{}{
			"RoleValues": "[\"user\",\"admin\"]",
			"StatusValues": "{\"1\":\"active\",\"0\":\"inactive\",\"-1\":\"deleted\"}",
			"RegistrationDefaultRole": "user",
			"RegistrationDefaultStatus": int32(1),
			"ValidationUsernameMinLength": int32(3),
			"ValidationUsernameMaxLength": int32(50),
			"ValidationPasswordMinLength": int32(6),
			"ValidationPasswordMaxLength": int32(50),
			"ValidationEmailRequired": true,
		},
	}
}

func (a *UserConfigAdapter) RoleValues(ctx context.Context) (string, error) {
	// 第1阶段: ConfigReader 实时查询（SHM / HTTP / 环境变量等）
	val, err := a.reader.Get(ctx, "/config/douyin-mall/user/role/values")
	if err == nil {
		if converted, ok := toString(val); ok {
			a.lastKnownCache.Store("RoleValues", converted)
			return converted, nil
		}
	}

	// 第2阶段: 降级到 Last Known Good 缓存
	if cached, ok := a.lastKnownCache.Load("RoleValues"); ok {
		if converted, ok := toString(cached); ok {
			return converted, nil
		}
	}

	// 第3阶段: 极冷启动 — 返回硬编码默认值
	return a.hardcodedDefaults["RoleValues"].(string), nil
}

func (a *UserConfigAdapter) StatusValues(ctx context.Context) (string, error) {
	// 第1阶段: ConfigReader 实时查询（SHM / HTTP / 环境变量等）
	val, err := a.reader.Get(ctx, "/config/douyin-mall/user/status/values")
	if err == nil {
		if converted, ok := toString(val); ok {
			a.lastKnownCache.Store("StatusValues", converted)
			return converted, nil
		}
	}

	// 第2阶段: 降级到 Last Known Good 缓存
	if cached, ok := a.lastKnownCache.Load("StatusValues"); ok {
		if converted, ok := toString(cached); ok {
			return converted, nil
		}
	}

	// 第3阶段: 极冷启动 — 返回硬编码默认值
	return a.hardcodedDefaults["StatusValues"].(string), nil
}

func (a *UserConfigAdapter) RegistrationDefaultRole(ctx context.Context) (string, error) {
	// 第1阶段: ConfigReader 实时查询（SHM / HTTP / 环境变量等）
	val, err := a.reader.Get(ctx, "/config/douyin-mall/user/registration/default_role")
	if err == nil {
		if converted, ok := toString(val); ok {
			a.lastKnownCache.Store("RegistrationDefaultRole", converted)
			return converted, nil
		}
	}

	// 第2阶段: 降级到 Last Known Good 缓存
	if cached, ok := a.lastKnownCache.Load("RegistrationDefaultRole"); ok {
		if converted, ok := toString(cached); ok {
			return converted, nil
		}
	}

	// 第3阶段: 极冷启动 — 返回硬编码默认值
	return a.hardcodedDefaults["RegistrationDefaultRole"].(string), nil
}

func (a *UserConfigAdapter) RegistrationDefaultStatus(ctx context.Context) (int32, error) {
	// 第1阶段: ConfigReader 实时查询（SHM / HTTP / 环境变量等）
	val, err := a.reader.Get(ctx, "/config/douyin-mall/user/registration/default_status")
	if err == nil {
		if converted, ok := toInt32(val); ok {
			a.lastKnownCache.Store("RegistrationDefaultStatus", converted)
			return converted, nil
		}
	}

	// 第2阶段: 降级到 Last Known Good 缓存
	if cached, ok := a.lastKnownCache.Load("RegistrationDefaultStatus"); ok {
		if converted, ok := toInt32(cached); ok {
			return converted, nil
		}
	}

	// 第3阶段: 极冷启动 — 返回硬编码默认值
	return a.hardcodedDefaults["RegistrationDefaultStatus"].(int32), nil
}

func (a *UserConfigAdapter) ValidationUsernameMinLength(ctx context.Context) (int32, error) {
	// 第1阶段: ConfigReader 实时查询（SHM / HTTP / 环境变量等）
	val, err := a.reader.Get(ctx, "/config/douyin-mall/user/validation/username_min_length")
	if err == nil {
		if converted, ok := toInt32(val); ok {
			a.lastKnownCache.Store("ValidationUsernameMinLength", converted)
			return converted, nil
		}
	}

	// 第2阶段: 降级到 Last Known Good 缓存
	if cached, ok := a.lastKnownCache.Load("ValidationUsernameMinLength"); ok {
		if converted, ok := toInt32(cached); ok {
			return converted, nil
		}
	}

	// 第3阶段: 极冷启动 — 返回硬编码默认值
	return a.hardcodedDefaults["ValidationUsernameMinLength"].(int32), nil
}

func (a *UserConfigAdapter) ValidationUsernameMaxLength(ctx context.Context) (int32, error) {
	// 第1阶段: ConfigReader 实时查询（SHM / HTTP / 环境变量等）
	val, err := a.reader.Get(ctx, "/config/douyin-mall/user/validation/username_max_length")
	if err == nil {
		if converted, ok := toInt32(val); ok {
			a.lastKnownCache.Store("ValidationUsernameMaxLength", converted)
			return converted, nil
		}
	}

	// 第2阶段: 降级到 Last Known Good 缓存
	if cached, ok := a.lastKnownCache.Load("ValidationUsernameMaxLength"); ok {
		if converted, ok := toInt32(cached); ok {
			return converted, nil
		}
	}

	// 第3阶段: 极冷启动 — 返回硬编码默认值
	return a.hardcodedDefaults["ValidationUsernameMaxLength"].(int32), nil
}

func (a *UserConfigAdapter) ValidationPasswordMinLength(ctx context.Context) (int32, error) {
	// 第1阶段: ConfigReader 实时查询（SHM / HTTP / 环境变量等）
	val, err := a.reader.Get(ctx, "/config/douyin-mall/user/validation/password_min_length")
	if err == nil {
		if converted, ok := toInt32(val); ok {
			a.lastKnownCache.Store("ValidationPasswordMinLength", converted)
			return converted, nil
		}
	}

	// 第2阶段: 降级到 Last Known Good 缓存
	if cached, ok := a.lastKnownCache.Load("ValidationPasswordMinLength"); ok {
		if converted, ok := toInt32(cached); ok {
			return converted, nil
		}
	}

	// 第3阶段: 极冷启动 — 返回硬编码默认值
	return a.hardcodedDefaults["ValidationPasswordMinLength"].(int32), nil
}

func (a *UserConfigAdapter) ValidationPasswordMaxLength(ctx context.Context) (int32, error) {
	// 第1阶段: ConfigReader 实时查询（SHM / HTTP / 环境变量等）
	val, err := a.reader.Get(ctx, "/config/douyin-mall/user/validation/password_max_length")
	if err == nil {
		if converted, ok := toInt32(val); ok {
			a.lastKnownCache.Store("ValidationPasswordMaxLength", converted)
			return converted, nil
		}
	}

	// 第2阶段: 降级到 Last Known Good 缓存
	if cached, ok := a.lastKnownCache.Load("ValidationPasswordMaxLength"); ok {
		if converted, ok := toInt32(cached); ok {
			return converted, nil
		}
	}

	// 第3阶段: 极冷启动 — 返回硬编码默认值
	return a.hardcodedDefaults["ValidationPasswordMaxLength"].(int32), nil
}

func (a *UserConfigAdapter) ValidationEmailRequired(ctx context.Context) (bool, error) {
	// 第1阶段: ConfigReader 实时查询（SHM / HTTP / 环境变量等）
	val, err := a.reader.Get(ctx, "/config/douyin-mall/user/validation/email_required")
	if err == nil {
		if converted, ok := toBool(val); ok {
			a.lastKnownCache.Store("ValidationEmailRequired", converted)
			return converted, nil
		}
	}

	// 第2阶段: 降级到 Last Known Good 缓存
	if cached, ok := a.lastKnownCache.Load("ValidationEmailRequired"); ok {
		if converted, ok := toBool(cached); ok {
			return converted, nil
		}
	}

	// 第3阶段: 极冷启动 — 返回硬编码默认值
	return a.hardcodedDefaults["ValidationEmailRequired"].(bool), nil
}
