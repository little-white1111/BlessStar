package utils

import (
	"context"
	"log"
	"time"

	"douyin-mall-go-template/internal/provider"
	"github.com/golang-jwt/jwt"
)

var jwtSecret = []byte("your-secret-key") // In production, use environment variable

type Claims struct {
	UserID   int64  `json:"user_id"`
	Username string `json:"username"`
	Role     string `json:"role"`
	jwt.StandardClaims
}

func GenerateToken(userID int64, username, role string) (string, error) {
	// 从 BlessStar 适配器读取 JWT 过期时间（秒），非阻塞
	expiryDuration := 24 * time.Hour // 默认 24 小时
	if douyin_mall.DefaultAdapters != nil && douyin_mall.DefaultAdapters.AuthConfig != nil {
		val, err := douyin_mall.DefaultAdapters.AuthConfig.JwtTokenExpirySeconds(context.Background())
		if err != nil {
			log.Printf("[BlessStar] AuthConfig.JwtTokenExpirySeconds failed: %v, using default 24h", err)
		} else {
			expiryDuration = val
		}
	}

	nowTime := time.Now()
	expireTime := nowTime.Add(expiryDuration)

	claims := Claims{
		UserID:   userID,
		Username: username,
		Role:     role,
		StandardClaims: jwt.StandardClaims{
			ExpiresAt: expireTime.Unix(),
			Issuer:    "douyin-mall",
		},
	}

	tokenClaims := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	token, err := tokenClaims.SignedString(jwtSecret)

	return token, err
}

func ParseToken(token string) (*Claims, error) {
	tokenClaims, err := jwt.ParseWithClaims(token, &Claims{}, func(token *jwt.Token) (interface{}, error) {
		return jwtSecret, nil
	})

	if tokenClaims != nil {
		if claims, ok := tokenClaims.Claims.(*Claims); ok && tokenClaims.Valid {
			return claims, nil
		}
	}

	return nil, err
}
