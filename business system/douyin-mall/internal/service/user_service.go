package service

import (
	"context"
	"errors"
	"log"

	"douyin-mall-go-template/internal/model"
	"douyin-mall-go-template/internal/model/dto"
	"douyin-mall-go-template/internal/provider"
	"douyin-mall-go-template/pkg/db"
	"douyin-mall-go-template/pkg/utils"
	"golang.org/x/crypto/bcrypt"
)

type UserService struct{}

func NewUserService() *UserService {
	return &UserService{}
}

func (s *UserService) Register(req *dto.RegisterRequest) error {
	// 从 BlessStar 适配器读取注册校验规则（非阻塞，失败走默认值）
	usernameMinLen := 3
	usernameMaxLen := 50
	passwordMinLen := 6
	passwordMaxLen := 50
	emailRequired := true

	if douyin_mall.DefaultAdapters != nil && douyin_mall.DefaultAdapters.UserConfig != nil {
		if v, err := douyin_mall.DefaultAdapters.UserConfig.ValidationUsernameMinLength(context.Background()); err == nil {
			usernameMinLen = int(v)
		} else {
			log.Printf("[BlessStar] UserConfig.ValidationUsernameMinLength: %v, using default %d", err, usernameMinLen)
		}
		if v, err := douyin_mall.DefaultAdapters.UserConfig.ValidationUsernameMaxLength(context.Background()); err == nil {
			usernameMaxLen = int(v)
		} else {
			log.Printf("[BlessStar] UserConfig.ValidationUsernameMaxLength: %v, using default %d", err, usernameMaxLen)
		}
		if v, err := douyin_mall.DefaultAdapters.UserConfig.ValidationPasswordMinLength(context.Background()); err == nil {
			passwordMinLen = int(v)
		} else {
			log.Printf("[BlessStar] UserConfig.ValidationPasswordMinLength: %v, using default %d", err, passwordMinLen)
		}
		if v, err := douyin_mall.DefaultAdapters.UserConfig.ValidationPasswordMaxLength(context.Background()); err == nil {
			passwordMaxLen = int(v)
		} else {
			log.Printf("[BlessStar] UserConfig.ValidationPasswordMaxLength: %v, using default %d", err, passwordMaxLen)
		}
		if v, err := douyin_mall.DefaultAdapters.UserConfig.ValidationEmailRequired(context.Background()); err == nil {
			emailRequired = v
		} else {
			log.Printf("[BlessStar] UserConfig.ValidationEmailRequired: %v, using default %v", err, emailRequired)
		}
	}

	// 校验用户名长度
	if len(req.Username) < usernameMinLen || len(req.Username) > usernameMaxLen {
		return errors.New("invalid username length")
	}

	// 校验密码长度
	if len(req.Password) < passwordMinLen || len(req.Password) > passwordMaxLen {
		return errors.New("invalid password length")
	}

	// 邮箱必填校验
	if emailRequired && req.Email == "" {
		return errors.New("email is required")
	}

	// Check if username exists
	var count int64
	if err := db.DB.Model(&model.User{}).Where("username = ?", req.Username).Count(&count).Error; err != nil {
		return err
	}
	if count > 0 {
		return errors.New("username already exists")
	}

	// Hash password
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		return err
	}

	// 获取注册默认角色和状态
	defaultRole := "user"
	defaultStatus := int32(1)
	if douyin_mall.DefaultAdapters != nil && douyin_mall.DefaultAdapters.UserConfig != nil {
		if v, err := douyin_mall.DefaultAdapters.UserConfig.RegistrationDefaultRole(context.Background()); err == nil {
			defaultRole = v
		} else {
			log.Printf("[BlessStar] UserConfig.RegistrationDefaultRole: %v, using %q", err, defaultRole)
		}
		if v, err := douyin_mall.DefaultAdapters.UserConfig.RegistrationDefaultStatus(context.Background()); err == nil {
			defaultStatus = v
		} else {
			log.Printf("[BlessStar] UserConfig.RegistrationDefaultStatus: %v, using %d", err, defaultStatus)
		}
	}

	// Create user
	user := &model.User{
		Username: req.Username,
		Password: string(hashedPassword),
		Email:    req.Email,
		Phone:    req.Phone,
		Role:     defaultRole,
		Status:   int8(defaultStatus),
	}

	return db.DB.Create(user).Error
}

func (s *UserService) Login(req *dto.LoginRequest) (*dto.LoginResponse, error) {
	var user model.User
	if err := db.DB.Where("username = ?", req.Username).First(&user).Error; err != nil {
		return nil, errors.New("invalid username or password")
	}

	if err := bcrypt.CompareHashAndPassword([]byte(user.Password), []byte(req.Password)); err != nil {
		return nil, errors.New("invalid username or password")
	}

	token, err := utils.GenerateToken(user.ID, user.Username, user.Role)
	if err != nil {
		return nil, err
	}

	return &dto.LoginResponse{
		Token: token,
		User: dto.User{
			ID:        user.ID,
			Username:  user.Username,
			Email:     user.Email,
			Phone:     user.Phone,
			AvatarURL: user.AvatarURL,
			Role:      user.Role,
		},
	}, nil
}
