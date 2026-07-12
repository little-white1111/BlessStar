// internal/middleware/cors.go
package middleware

import (
	"context"
	"log"
	"strings"

	"douyin-mall-go-template/internal/provider"
	"github.com/gin-gonic/gin"
)

func Cors() gin.HandlerFunc {
	return func(c *gin.Context) {
		// 从 BlessStar 适配器读取跨域配置
		origins, err := douyin_mall.DefaultAdapters.CorsConfig.AllowedOrigins(c.Request.Context())
		if err != nil {
			log.Printf("[BlessStar] CorsConfig.AllowedOrigins failed: %v, using wildcard", err)
			origins = []string{"*"}
		}

		var allowOrigin string
		if len(origins) == 1 && origins[0] == "*" {
			allowOrigin = "*"
		} else {
			origin := c.Request.Header.Get("Origin")
			for _, allowed := range origins {
				if allowed == origin {
					allowOrigin = origin
					break
				}
			}
			if allowOrigin == "" && len(origins) > 0 {
				allowOrigin = strings.Join(origins, ", ")
			}
		}

		c.Header("Access-Control-Allow-Origin", allowOrigin)
		c.Header("Access-Control-Allow-Methods", "GET,POST,PUT,PATCH,DELETE,OPTIONS")
		c.Header("Access-Control-Allow-Headers", "Origin,Content-Type,Authorization")
		c.Header("Access-Control-Expose-Headers", "Content-Length,Access-Control-Allow-Origin,Access-Control-Allow-Headers")

		// CORS max-age from BlessStar config (with fallback)
		_ = context.Background() // placeholder for future CORS configs
		c.Header("Access-Control-Max-Age", "86400")

		if c.Request.Method == "OPTIONS" {
			c.AbortWithStatus(204)
			return
		}

		c.Next()
	}
}
