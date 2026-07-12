// Package adapter_mock 自动生成于 BlessStar 配置 Mock
// 业务系统: 抖音商城 (douyin-mall)
// 领域: 用户管理
// 专为单元测试设计 — 固定返回值

package adapter_mock

import (
	"context"
	"douyin-mall-go-template/ports"
)

// UserConfigMock 用户管理 域配置的 Mock 实现（单元测试用）
type UserConfigMock struct {
	RoleValuesFunc func(ctx context.Context) (string, error)
	StatusValuesFunc func(ctx context.Context) (string, error)
	RegistrationDefaultRoleFunc func(ctx context.Context) (string, error)
	RegistrationDefaultStatusFunc func(ctx context.Context) (int32, error)
	ValidationUsernameMinLengthFunc func(ctx context.Context) (int32, error)
	ValidationUsernameMaxLengthFunc func(ctx context.Context) (int32, error)
	ValidationPasswordMinLengthFunc func(ctx context.Context) (int32, error)
	ValidationPasswordMaxLengthFunc func(ctx context.Context) (int32, error)
	ValidationEmailRequiredFunc func(ctx context.Context) (bool, error)
}

// NewUserConfigMock 创建默认 Mock（返回值按 manifest 默认值设定）
func NewUserConfigMock() ports.UserConfig {
	return &UserConfigMock{
		RoleValuesFunc: func(ctx context.Context) (string, error) {
			return "[\"user\",\"admin\"]", nil
		},
		StatusValuesFunc: func(ctx context.Context) (string, error) {
			return "{\"1\":\"active\",\"0\":\"inactive\",\"-1\":\"deleted\"}", nil
		},
		RegistrationDefaultRoleFunc: func(ctx context.Context) (string, error) {
			return "user", nil
		},
		RegistrationDefaultStatusFunc: func(ctx context.Context) (int32, error) {
			return 1, nil
		},
		ValidationUsernameMinLengthFunc: func(ctx context.Context) (int32, error) {
			return 3, nil
		},
		ValidationUsernameMaxLengthFunc: func(ctx context.Context) (int32, error) {
			return 50, nil
		},
		ValidationPasswordMinLengthFunc: func(ctx context.Context) (int32, error) {
			return 6, nil
		},
		ValidationPasswordMaxLengthFunc: func(ctx context.Context) (int32, error) {
			return 50, nil
		},
		ValidationEmailRequiredFunc: func(ctx context.Context) (bool, error) {
			return true, nil
		},
	}
}

func (m *UserConfigMock) RoleValues(ctx context.Context) (string, error) {
	return m.RoleValuesFunc(ctx)
}

func (m *UserConfigMock) StatusValues(ctx context.Context) (string, error) {
	return m.StatusValuesFunc(ctx)
}

func (m *UserConfigMock) RegistrationDefaultRole(ctx context.Context) (string, error) {
	return m.RegistrationDefaultRoleFunc(ctx)
}

func (m *UserConfigMock) RegistrationDefaultStatus(ctx context.Context) (int32, error) {
	return m.RegistrationDefaultStatusFunc(ctx)
}

func (m *UserConfigMock) ValidationUsernameMinLength(ctx context.Context) (int32, error) {
	return m.ValidationUsernameMinLengthFunc(ctx)
}

func (m *UserConfigMock) ValidationUsernameMaxLength(ctx context.Context) (int32, error) {
	return m.ValidationUsernameMaxLengthFunc(ctx)
}

func (m *UserConfigMock) ValidationPasswordMinLength(ctx context.Context) (int32, error) {
	return m.ValidationPasswordMinLengthFunc(ctx)
}

func (m *UserConfigMock) ValidationPasswordMaxLength(ctx context.Context) (int32, error) {
	return m.ValidationPasswordMaxLengthFunc(ctx)
}

func (m *UserConfigMock) ValidationEmailRequired(ctx context.Context) (bool, error) {
	return m.ValidationEmailRequiredFunc(ctx)
}

