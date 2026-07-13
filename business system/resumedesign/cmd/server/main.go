package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"syscall"
	"time"

	"resumedesign/internal/adapter/browser"
	"resumedesign/internal/adapter/cache"
	"resumedesign/internal/adapter/config"
	"resumedesign/internal/adapter/exporter"
	"resumedesign/internal/adapter/fetcher"
	"resumedesign/internal/adapter/filestorage"
	"resumedesign/internal/adapter/keyword"
	"resumedesign/internal/adapter/llm"
	"resumedesign/internal/adapter/parser"
	"resumedesign/internal/adapter/scorer"
	"resumedesign/internal/adapter/search"
	"resumedesign/internal/domain"
	"resumedesign/internal/port"
	"resumedesign/internal/repository"
	"resumedesign/internal/scheduler"
	"resumedesign/internal/service"

	"github.com/gin-gonic/gin"
)

func main() {
	// 初始化数据库
	if err := repository.InitDB(""); err != nil {
		log.Fatalf("failed to init database: %v", err)
	}

	// 初始化依赖
	cacheStore := cache.NewSQLiteCache()
	httpFetcher := fetcher.NewHTTPFetcher()
	openAIClient := llm.NewOpenAIClient(os.Getenv("OPENAI_API_KEY"))

	// Repositories
	db := repository.GetDB()
	resumeRepo := repository.NewResumeRepo(db)
	sectionRepo := repository.NewSectionRepo(db)
	jdRepo := repository.NewJDRepo(db)
	sessionRepo := repository.NewSessionRepo(db)
	diffRepo := repository.NewDiffRepo(db)

	// Services
	resumeSvc := service.NewResumeService(resumeRepo, sectionRepo, serverParser())
	jdSvc := service.NewJDService(jdRepo, httpFetcher)
	rewriteSvc := service.NewRewriteService(sessionRepo, diffRepo, resumeRepo, sectionRepo, jdRepo, openAIClient, cacheStore)

	// 增强模块 - Repositories
	scoringRepo := repository.NewScoringRepo(db)
	deliveryRepo := repository.NewDeliveryRepo(db)

	// 增强模块 - Adapters
	scorerImpl := scorer.NewHeuristicScorer("")
	expanderImpl := keyword.NewExpander(keyword.DefaultDict())
	searchEng := search.NewWebPageSearchEngine()
	fileStorage := filestorage.NewLocalFileStorage()
	browserOpener := browser.NewDefaultOpener()

	// 增强模块 - Services
	screeningSvc := service.NewScreeningService(scorerImpl, expanderImpl, searchEng, httpFetcher, config.NewLocalConfigReader(), scoringRepo)
	deliverySvc := service.NewDeliveryService(deliveryRepo, resumeRepo, scoringRepo, nil, fileStorage, browserOpener, config.NewLocalConfigReader())

	// 启动定时清理协程
	cleanupTask := scheduler.NewCleanupTask(deliveryRepo, fileStorage, 1*time.Hour)
	go cleanupTask.Start(context.Background())

	// 路由
	r := gin.Default()

	// CORS
	r.Use(func(c *gin.Context) {
		c.Header("Access-Control-Allow-Origin", "*")
		c.Header("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS")
		c.Header("Access-Control-Allow-Headers", "Content-Type,Authorization")
		if c.Request.Method == "OPTIONS" {
			c.AbortWithStatus(204)
			return
		}
		c.Next()
	})

	api := r.Group("/api/v1")
	{
		// 配置
		api.GET("/config", func(c *gin.Context) {
			result := make(map[string]interface{})
			for _, k := range config.Keys() {
				result[k] = config.FormatValue(k, config.DefaultConfig())
			}
			c.JSON(200, result)
		})

		// 简历
		api.POST("/resumes/upload", func(c *gin.Context) {
			file, err := c.FormFile("file")
			if err != nil {
				c.JSON(400, gin.H{"error": "file required"})
				return
			}

			format := c.PostForm("format")
			if format == "" {
				format = "txt"
			}

			tmpFile := filepath.Join(os.TempDir(), "resumedesign-upload-"+file.Filename)
			if err := c.SaveUploadedFile(file, tmpFile); err != nil {
				c.JSON(500, gin.H{"error": "save file failed"})
				return
			}
			defer os.Remove(tmpFile)

			resume, err := resumeSvc.UploadAndParse(tmpFile, format, file.Filename)
			if err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(201, resume)
		})
		api.GET("/resumes", func(c *gin.Context) {
			resumes, err := resumeSvc.List()
			if err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(200, resumes)
		})
		api.GET("/resumes/:id", func(c *gin.Context) {
			resume, err := resumeSvc.GetByID(c.Param("id"))
			if err != nil {
				c.JSON(404, gin.H{"error": err.Error()})
				return
			}
			c.JSON(200, resume)
		})
		api.DELETE("/resumes/:id", func(c *gin.Context) {
			if err := resumeSvc.Delete(c.Param("id")); err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(204, nil)
		})

		// JD
		api.POST("/jds/parse-url", func(c *gin.Context) {
			var req struct {
				URL string `json:"url"`
			}
			if err := c.ShouldBindJSON(&req); err != nil {
				c.JSON(400, gin.H{"error": "url required"})
				return
			}
			jd, err := jdSvc.ParseFromURL(req.URL, 10*time.Second)
			if err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(201, jd)
		})
		api.POST("/jds/parse-text", func(c *gin.Context) {
			var req struct {
				Text string `json:"text"`
			}
			if err := c.ShouldBindJSON(&req); err != nil {
				c.JSON(400, gin.H{"error": "text required"})
				return
			}
			jd, err := jdSvc.ParseFromText(req.Text)
			if err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(201, jd)
		})
		api.GET("/jds", func(c *gin.Context) {
			jds, err := jdSvc.List()
			if err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(200, jds)
		})
		api.GET("/jds/:id", func(c *gin.Context) {
			jd, err := jdSvc.GetByID(c.Param("id"))
			if err != nil {
				c.JSON(404, gin.H{"error": err.Error()})
				return
			}
			c.JSON(200, jd)
		})

		// Sessions
		api.POST("/sessions", func(c *gin.Context) {
			var req struct {
				ResumeID    string  `json:"resume_id"`
				JdID        string  `json:"jd_id"`
				Model       string  `json:"model"`
				Temperature float64 `json:"temperature"`
				MaxLength   int     `json:"max_length"`
			}
			if err := c.ShouldBindJSON(&req); err != nil {
				c.JSON(400, gin.H{"error": err.Error()})
				return
			}
			session, err := rewriteSvc.CreateSession(req.ResumeID, req.JdID, domain.RewriteConfig{
				Model:       req.Model,
				Temperature: req.Temperature,
				MaxLength:   req.MaxLength,
			})
			if err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(201, session)
		})
		api.GET("/sessions", func(c *gin.Context) {
			sessions, err := rewriteSvc.ListSessions()
			if err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(200, sessions)
		})
		api.GET("/sessions/:id", func(c *gin.Context) {
			session, err := rewriteSvc.GetSession(c.Param("id"))
			if err != nil {
				c.JSON(404, gin.H{"error": err.Error()})
				return
			}
			c.JSON(200, session)
		})
		api.POST("/sessions/:id/rewrite", func(c *gin.Context) {
			session, err := rewriteSvc.RunRewrite(context.Background(), c.Param("id"))
			if err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(200, session)
		})
		api.POST("/sessions/:id/export", func(c *gin.Context) {
			var req struct {
				Format string `json:"format"`
			}
			if err := c.ShouldBindJSON(&req); err != nil {
				c.JSON(400, gin.H{"error": "format required"})
				return
			}

			var exp service.ExportService
			switch req.Format {
			case "pdf":
				exp = *service.NewExportService(sessionRepo, resumeRepo, exporter.NewPDFExporter())
			case "docx":
				exp = *service.NewExportService(sessionRepo, resumeRepo, exporter.NewDOCXExporter())
			default:
				c.JSON(400, gin.H{"error": "unsupported format"})
				return
			}

			path, err := exp.Export(c.Param("id"), req.Format)
			if err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.File(path)
		})

		// 岗位筛选
		api.POST("/screening/start", func(c *gin.Context) {
			var req struct {
				Keywords       []string `json:"keywords"`
				SourceType     string   `json:"source_type"`
				URLs           []string `json:"urls"`
				TrustThreshold float64  `json:"trust_threshold"`
				MatchThreshold float64  `json:"match_threshold"`
			}
			if err := c.ShouldBindJSON(&req); err != nil {
				c.JSON(400, gin.H{"error": err.Error()})
				return
			}
			if req.TrustThreshold == 0 {
				req.TrustThreshold = 0.6
			}
			if req.MatchThreshold == 0 {
				req.MatchThreshold = 0.6
			}
			session, err := screeningSvc.StartScoringSession(req.Keywords, req.SourceType, req.URLs, req.TrustThreshold, req.MatchThreshold)
			if err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(201, session)
		})
		api.POST("/screening/:id/run", func(c *gin.Context) {
			if err := screeningSvc.RunScreening(context.Background(), c.Param("id")); err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(200, gin.H{"message": "screening completed"})
		})
		api.GET("/screening/:id", func(c *gin.Context) {
			session, err := screeningSvc.GetScreeningResult(c.Param("id"))
			if err != nil {
				c.JSON(404, gin.H{"error": err.Error()})
				return
			}
			c.JSON(200, session)
		})
		api.GET("/screening/:id/passed", func(c *gin.Context) {
			results, err := screeningSvc.GetPassedResults(c.Param("id"))
			if err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(200, results)
		})

		// 批量投递
		api.POST("/deliveries", func(c *gin.Context) {
			var req struct {
				ResumeID     string   `json:"resume_id"`
				TargetIDs    []string `json:"target_ids"`
				Format       string   `json:"format"`
			}
			if err := c.ShouldBindJSON(&req); err != nil {
				c.JSON(400, gin.H{"error": err.Error()})
				return
			}
			if req.Format == "" {
				req.Format = "pdf"
			}
			var targets []service.DeliveryTargetInput
			for _, id := range req.TargetIDs {
				targets = append(targets, service.DeliveryTargetInput{ScoredJDID: id})
			}
			session, err := deliverySvc.StartDelivery(context.Background(), req.ResumeID, targets, req.Format)
			if err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(201, session)
		})
		api.GET("/deliveries", func(c *gin.Context) {
			sessions, err := deliverySvc.ListDeliveries()
			if err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(200, sessions)
		})
		api.GET("/deliveries/:id", func(c *gin.Context) {
			session, err := deliverySvc.GetDelivery(c.Param("id"))
			if err != nil {
				c.JSON(404, gin.H{"error": err.Error()})
				return
			}
			c.JSON(200, session)
		})
		api.DELETE("/deliveries/:id", func(c *gin.Context) {
			if err := deliverySvc.RevokeDelivery(c.Param("id")); err != nil {
				c.JSON(500, gin.H{"error": err.Error()})
				return
			}
			c.JSON(204, nil)
		})
	}

	// 优雅关闭
	srv := &http.Server{
		Addr:    ":8080",
		Handler: r,
	}

	go func() {
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("listen: %s\n", err)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	log.Println("Shutdown Server ...")

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	if err := srv.Shutdown(ctx); err != nil {
		log.Fatal("Server Shutdown:", err)
	}
	log.Println("Server exiting")
}

func serverParser() port.ResumeParser {
	return parser.NewTXTParser()
}
