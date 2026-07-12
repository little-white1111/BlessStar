package main

import (
	"log"
	"time"

	"douyin-mall-go-template/internal/adapter_http"
	"douyin-mall-go-template/internal/provider"
	"douyin-mall-go-template/internal/routes"
	"douyin-mall-go-template/pkg/db"
	"douyin-mall-go-template/pkg/logger"
	"github.com/gin-gonic/gin"
	"github.com/spf13/viper"
)

func main() {
	// 初始化配置
	if err := initConfig(); err != nil {
		log.Fatalf("init config failed: %v", err)
	}

	// 初始化日志
	if err := logger.Init(); err != nil {
		log.Fatalf("init logger failed: %v", err)
	}

	// 初始化数据库连接
	if err := db.Init(); err != nil {
		log.Fatalf("init database failed: %v", err)
	}

	// 初始化 BlessStar 配置适配器（非阻塞）
	// HTTPReader 通过环境变量 BLESSSTAR_ENDPOINT 获取 Electron 地址
	// 若环境变量未设置，HTTPReader 返回 nil，适配器走 3 阶段降级（Cache→Default）
	reader := adapter_http.New("douyin-mall")
	if reader == nil {
		log.Println("[BlessStar] BLESSSTAR_ENDPOINT not set, using cached/default config values (3-stage fallback)")
		douyin_mall.InitAdapters(nil)
	} else {
		// 用 CachedReader 包装实现秒级缓存热更新
		cachedReader := douyin_mall.NewCachedReader(reader, 30*time.Second)
		douyin_mall.InitAdapters(cachedReader)
		log.Println("[BlessStar] CachedReader enabled (30s refresh interval)")
	}

	// 创建gin引擎
	r := gin.Default()

	// 注册路由
	routes.RegisterRoutes(r)

	// 启动服务器
	port := viper.GetString("server.port")
	if err := r.Run(":" + port); err != nil {
		log.Fatalf("start server failed: %v", err)
	}
}

func initConfig() error {
	viper.SetConfigName("config")
	viper.SetConfigType("yaml")
	viper.AddConfigPath("./configs")
	return viper.ReadInConfig()
}
