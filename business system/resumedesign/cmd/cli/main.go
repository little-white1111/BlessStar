package main

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"strings"
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
	"resumedesign/internal/service"

	"github.com/spf13/cobra"
)

var rootCmd = &cobra.Command{
	Use:   "resumedesign",
	Short: "resumedesign - 简历智能改写工具",
	Long:  `基于 LLM 的简历智能改写系统。上传简历、解析 JD、智能改写、导出。`,
}

func main() {
	if err := rootCmd.Execute(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func init() {
	// resume commands
	resumeCmd := &cobra.Command{Use: "resume", Short: "简历管理"}
	resumeCmd.AddCommand(&cobra.Command{
		Use:   "upload [file]",
		Short: "上传并解析简历文件",
		Args:  cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleResumeUpload(args[0])
		},
	})
	resumeCmd.AddCommand(&cobra.Command{
		Use:   "list",
		Short: "简历列表",
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleResumeList()
		},
	})
	resumeCmd.AddCommand(&cobra.Command{
		Use:   "get [id]",
		Short: "简历详情",
		Args:  cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleResumeGet(args[0])
		},
	})
	rootCmd.AddCommand(resumeCmd)

	// jd commands
	jdCmd := &cobra.Command{Use: "jd", Short: "岗位要求管理"}
	jdCmd.AddCommand(&cobra.Command{
		Use:   "parse-url [url]",
		Short: "从 URL 解析 JD",
		Args:  cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleJDParseURL(args[0])
		},
	})
	jdCmd.AddCommand(&cobra.Command{
		Use:   "parse-text [text]",
		Short: "从文本解析 JD",
		Args:  cobra.MinimumNArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleJDParseText(strings.Join(args, " "))
		},
	})
	jdCmd.AddCommand(&cobra.Command{
		Use:   "list",
		Short: "JD 列表",
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleJDList()
		},
	})
	jdCmd.AddCommand(&cobra.Command{
		Use:   "get [id]",
		Short: "JD 详情",
		Args:  cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleJDGet(args[0])
		},
	})
	rootCmd.AddCommand(jdCmd)

	// rewrite commands
	rewriteCmd := &cobra.Command{Use: "rewrite", Short: "改写管理"}
	rewriteCmd.AddCommand(&cobra.Command{
		Use:   "create",
		Short: "创建改写会话",
		RunE: func(cmd *cobra.Command, args []string) error {
			resumeID, _ := cmd.Flags().GetString("resume")
			jdID, _ := cmd.Flags().GetString("jd")
			if resumeID == "" || jdID == "" {
				return fmt.Errorf("--resume and --jd are required")
			}
			return handleRewriteCreate(resumeID, jdID)
		},
	})
	rewriteCmd.PersistentFlags().String("resume", "", "简历 ID")
	rewriteCmd.PersistentFlags().String("jd", "", "JD ID")

	rewriteCmd.AddCommand(&cobra.Command{
		Use:   "run [session-id]",
		Short: "执行改写",
		Args:  cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleRewriteRun(args[0])
		},
	})
	rewriteCmd.AddCommand(&cobra.Command{
		Use:   "diff [session-id]",
		Short: "查看改写差异",
		Args:  cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleRewriteDiff(args[0])
		},
	})
	rewriteCmd.AddCommand(&cobra.Command{
		Use:   "list",
		Short: "改写会话列表",
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleRewriteList()
		},
	})
	rootCmd.AddCommand(rewriteCmd)

	// screening commands
	screeningCmd := &cobra.Command{Use: "screening", Short: "岗位筛选管理"}
	screeningStartCmd := &cobra.Command{
		Use:   "start",
		Short: "开始岗位筛选",
		RunE: func(cmd *cobra.Command, args []string) error {
			keywords, _ := cmd.Flags().GetString("keywords")
			sourceType, _ := cmd.Flags().GetString("source")
			urls, _ := cmd.Flags().GetStringSlice("urls")
			if keywords != "" {
				return handleScreeningStart(strings.Split(keywords, ","), sourceType, nil)
			}
			if len(urls) > 0 {
				return handleScreeningStart(nil, "urls", urls)
			}
			return fmt.Errorf("please provide --keywords or --urls")
		},
	}
	screeningStartCmd.Flags().String("keywords", "", "筛选关键词（逗号分隔）")
	screeningStartCmd.Flags().String("source", "search", "数据来源（search/urls）")
	screeningStartCmd.Flags().StringSlice("urls", nil, "目标 URL 列表")
	screeningCmd.AddCommand(screeningStartCmd)

	screeningCmd.AddCommand(&cobra.Command{
		Use:   "run [session-id]",
		Short: "执行筛选",
		Args:  cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleScreeningRun(args[0])
		},
	})
	screeningCmd.AddCommand(&cobra.Command{
		Use:   "result [session-id]",
		Short: "查看筛选结果",
		Args:  cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleScreeningResult(args[0])
		},
	})
	rootCmd.AddCommand(screeningCmd)

	// delivery commands
	deliveryCmd := &cobra.Command{Use: "delivery", Short: "投递管理"}
	deliveryStartCmd := &cobra.Command{
		Use:   "start [resume-id]",
		Short: "开始投递",
		Args:  cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			targets, _ := cmd.Flags().GetStringSlice("targets")
			format, _ := cmd.Flags().GetString("format")
			if len(targets) == 0 {
				return fmt.Errorf("--targets is required")
			}
			return handleDeliveryStart(args[0], targets, format)
		},
	}
	deliveryStartCmd.Flags().StringSlice("targets", nil, "投递目标 JD ID 列表")
	deliveryStartCmd.Flags().String("format", "pdf", "导出格式（pdf/docx）")
	deliveryCmd.AddCommand(deliveryStartCmd)

	deliveryCmd.AddCommand(&cobra.Command{
		Use:   "list",
		Short: "投递列表",
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleDeliveryList()
		},
	})
	deliveryCmd.AddCommand(&cobra.Command{
		Use:   "show [session-id]",
		Short: "查看投递详情",
		Args:  cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleDeliveryShow(args[0])
		},
	})
	deliveryCmd.AddCommand(&cobra.Command{
		Use:   "revoke [session-id]",
		Short: "撤销投递",
		Args:  cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleDeliveryRevoke(args[0])
		},
	})
	rootCmd.AddCommand(deliveryCmd)

	// export command
	exportCmd := &cobra.Command{
		Use:   "export [session-id]",
		Short: "导出改写后简历",
		Args:  cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			format, _ := cmd.Flags().GetString("format")
			if format == "" {
				format = "pdf"
			}
			return handleExport(args[0], format)
		},
	}
	exportCmd.Flags().String("format", "pdf", "导出格式（pdf/docx）")
	rootCmd.AddCommand(exportCmd)

	// config command
	configCmd := &cobra.Command{Use: "config", Short: "配置管理"}
	configCmd.AddCommand(&cobra.Command{
		Use:   "show",
		Short: "查看当前配置",
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleConfigShow()
		},
	})
	configCmd.AddCommand(&cobra.Command{
		Use:   "set [key] [value]",
		Short: "修改配置",
		Args:  cobra.ExactArgs(2),
		RunE: func(cmd *cobra.Command, args []string) error {
			return handleConfigSet(args[0], args[1])
		},
	})
	rootCmd.AddCommand(configCmd)
}

func initDB() *config.LocalConfigReader {
	repository.InitDB("")
	return config.NewLocalConfigReader()
}

func handleResumeUpload(filePath string) error {
	cfg := initDB()
	db := repository.GetDB()
	resumeRepo := repository.NewResumeRepo(db)
	sectionRepo := repository.NewSectionRepo(db)

	ext := strings.TrimPrefix(filepath.Ext(filePath), ".")
	if ext == "" {
		ext = "txt"
	}

	var p port.ResumeParser
	switch ext {
	case "pdf":
		p = parser.NewPDFParser()
	case "docx":
		p = parser.NewDOCXParser()
	default:
		p = parser.NewTXTParser()
	}

	svc := service.NewResumeService(resumeRepo, sectionRepo, p)
	resume, err := svc.UploadAndParse(filePath, ext, filepath.Base(filePath))
	if err != nil {
		return err
	}
	fmt.Printf("简历上传成功 ID: %s\n标题: %s\n段落数: %d\n", resume.ID, resume.Title, len(resume.Sections))
	_ = cfg
	return nil
}

func handleResumeList() error {
	initDB()
	db := repository.GetDB()
	resumeRepo := repository.NewResumeRepo(db)

	svc := service.NewResumeService(resumeRepo, nil, nil)
	resumes, err := svc.List()
	if err != nil {
		return err
	}
	if len(resumes) == 0 {
		fmt.Println("暂无简历")
		return nil
	}
	for _, r := range resumes {
		fmt.Printf("%s | %s | %s\n", r.ID[:8], r.Title, r.CreatedAt.Format("2006-01-02 15:04"))
	}
	return nil
}

func handleResumeGet(id string) error {
	initDB()
	db := repository.GetDB()
	resumeRepo := repository.NewResumeRepo(db)
	sectionRepo := repository.NewSectionRepo(db)

	svc := service.NewResumeService(resumeRepo, sectionRepo, nil)
	resume, err := svc.GetByID(id)
	if err != nil {
		return err
	}
	fmt.Printf("ID: %s\n标题: %s\n格式: %s\n创建时间: %s\n段落:\n", resume.ID, resume.Title, resume.ImportFormat, resume.CreatedAt.Format("2006-01-02 15:04"))
	for _, s := range resume.Sections {
		fmt.Printf("  [%s] %s\n", s.Type, s.Title)
	}
	return nil
}

func handleJDParseURL(url string) error {
	cfg := initDB()
	db := repository.GetDB()
	jdRepo := repository.NewJDRepo(db)
	fetcher := fetcher.NewHTTPFetcher()

	svc := service.NewJDService(jdRepo, fetcher)
	jd, err := svc.ParseFromURL(url, 10*time.Second)
	if err != nil {
		return err
	}
	fmt.Printf("JD 解析成功 ID: %s\n公司: %s\n职位: %s\n", jd.ID, jd.Company, jd.Position)
	_ = cfg
	return nil
}

func handleJDParseText(text string) error {
	initDB()
	db := repository.GetDB()
	jdRepo := repository.NewJDRepo(db)

	svc := service.NewJDService(jdRepo, nil)
	jd, err := svc.ParseFromText(text)
	if err != nil {
		return err
	}
	fmt.Printf("JD 解析成功 ID: %s\n内容长度: %d\n", jd.ID, len(jd.RawContent))
	return nil
}

func handleJDList() error {
	initDB()
	db := repository.GetDB()
	jdRepo := repository.NewJDRepo(db)

	svc := service.NewJDService(jdRepo, nil)
	jds, err := svc.List()
	if err != nil {
		return err
	}
	if len(jds) == 0 {
		fmt.Println("暂无 JD")
		return nil
	}
	for _, j := range jds {
		fmt.Printf("%s | %s | %s | %s\n", j.ID[:8], j.Source, j.Company, j.Position)
	}
	return nil
}

func handleJDGet(id string) error {
	initDB()
	db := repository.GetDB()
	jdRepo := repository.NewJDRepo(db)

	svc := service.NewJDService(jdRepo, nil)
	jd, err := svc.GetByID(id)
	if err != nil {
		return err
	}
	fmt.Printf("ID: %s\n来源: %s\n公司: %s\n职位: %s\n内容:\n%s\n", jd.ID, jd.Source, jd.Company, jd.Position, jd.RawContent)
	return nil
}

func handleRewriteCreate(resumeID, jdID string) error {
	initDB()
	db := repository.GetDB()
	sessionRepo := repository.NewSessionRepo(db)
	diffRepo := repository.NewDiffRepo(db)
	resumeRepo := repository.NewResumeRepo(db)
	sectionRepo := repository.NewSectionRepo(db)
	jdRepo := repository.NewJDRepo(db)
	llmClient := llm.NewOpenAIClient(os.Getenv("OPENAI_API_KEY"))
	cacheStore := cache.NewSQLiteCache()

	svc := service.NewRewriteService(sessionRepo, diffRepo, resumeRepo, sectionRepo, jdRepo, llmClient, cacheStore)
	session, err := svc.CreateSession(resumeID, jdID, domain.RewriteConfig{
		Model:       "gpt-4o",
		Temperature: 0.7,
	})
	if err != nil {
		return err
	}
	fmt.Printf("改写会话创建成功 ID: %s\n", session.ID)
	return nil
}

func handleRewriteRun(sessionID string) error {
	initDB()
	db := repository.GetDB()
	sessionRepo := repository.NewSessionRepo(db)
	diffRepo := repository.NewDiffRepo(db)
	resumeRepo := repository.NewResumeRepo(db)
	sectionRepo := repository.NewSectionRepo(db)
	jdRepo := repository.NewJDRepo(db)
	llmClient := llm.NewOpenAIClient(os.Getenv("OPENAI_API_KEY"))
	cacheStore := cache.NewSQLiteCache()

	svc := service.NewRewriteService(sessionRepo, diffRepo, resumeRepo, sectionRepo, jdRepo, llmClient, cacheStore)
	fmt.Println("正在调用 LLM 改写简历（可能需要 10-30 秒）...")
	session, err := svc.RunRewrite(context.Background(), sessionID)
	if err != nil {
		return err
	}
	fmt.Printf("改写完成 ID: %s\n状态: %s\n模型: %s\n差异数: %d\n", session.ID, session.Status, session.Model, len(session.Diff))
	return nil
}

func handleRewriteDiff(sessionID string) error {
	initDB()
	db := repository.GetDB()
	diffRepo := repository.NewDiffRepo(db)

	svc := service.NewDiffService(diffRepo)
	diffs, err := svc.GetDiffBySession(sessionID)
	if err != nil {
		return err
	}
	for _, d := range diffs {
		fmt.Printf("[%s] %s | 变更: %s | 置信度: %.1f\n", d.SectionType, "段落", d.ChangeType, d.Confidence)
	}
	return nil
}

func handleRewriteList() error {
	initDB()
	db := repository.GetDB()
	sessionRepo := repository.NewSessionRepo(db)

	svc := service.NewRewriteService(sessionRepo, nil, nil, nil, nil, nil, nil)
	sessions, err := svc.ListSessions()
	if err != nil {
		return err
	}
	if len(sessions) == 0 {
		fmt.Println("暂无改写会话")
		return nil
	}
	for _, s := range sessions {
		fmt.Printf("%s | 简历: %s | JD: %s | 状态: %s | %s\n",
			s.ID[:8], s.ResumeID[:8], s.JdID[:8], s.Status, s.CreatedAt.Format("2006-01-02 15:04"))
	}
	return nil
}

func handleExport(sessionID string, format string) error {
	initDB()
	db := repository.GetDB()
	sessionRepo := repository.NewSessionRepo(db)
	resumeRepo := repository.NewResumeRepo(db)

	var exp service.ExportService
	switch format {
	case "pdf":
		exp = *service.NewExportService(sessionRepo, resumeRepo, exporter.NewPDFExporter())
	case "docx":
		exp = *service.NewExportService(sessionRepo, resumeRepo, exporter.NewDOCXExporter())
	default:
		return fmt.Errorf("unsupported format: %s", format)
	}

	path, err := exp.Export(sessionID, format)
	if err != nil {
		return err
	}
	fmt.Printf("导出成功: %s\n", path)
	return nil
}

func handleConfigShow() error {
	for _, k := range config.Keys() {
		val := config.FormatValue(k, config.DefaultConfig())
		fmt.Printf("%s = %s\n", k, val)
	}
	return nil
}

func handleConfigSet(key, value string) error {
	cfg := config.NewLocalConfigReader()
	if err := cfg.Set(key, value); err != nil {
		return err
	}
	fmt.Printf("配置已更新: %s = %s\n", key, value)
	return nil
}

func handleScreeningStart(keywords []string, sourceType string, urls []string) error {
	cfg := initDB()
	db := repository.GetDB()
	scoringRepo := repository.NewScoringRepo(db)
	scorerImpl := scorer.NewHeuristicScorer("")
	expanderImpl := keyword.NewExpander(keyword.DefaultDict())
	searchEng := search.NewWebPageSearchEngine()
	fetcherImpl := fetcher.NewHTTPFetcher()

	svc := service.NewScreeningService(scorerImpl, expanderImpl, searchEng, fetcherImpl, cfg, scoringRepo)
	session, err := svc.StartScoringSession(keywords, sourceType, urls, 0.6, 0.6)
	if err != nil {
		return err
	}
	fmt.Printf("筛选会话已创建 ID: %s\n关键词: %v\n来源: %s\n", session.ID, session.Keywords, sourceType)
	return nil
}

func handleScreeningRun(sessionID string) error {
	initDB()
	db := repository.GetDB()
	scoringRepo := repository.NewScoringRepo(db)
	scorerImpl := scorer.NewHeuristicScorer("")
	expanderImpl := keyword.NewExpander(keyword.DefaultDict())
	searchEng := search.NewWebPageSearchEngine()
	fetcherImpl := fetcher.NewHTTPFetcher()

	svc := service.NewScreeningService(scorerImpl, expanderImpl, searchEng, fetcherImpl, nil, scoringRepo)
	fmt.Println("正在执行岗位筛选...")
	if err := svc.RunScreening(context.Background(), sessionID); err != nil {
		return err
	}
	session, _ := svc.GetScreeningResult(sessionID)
	passed := session.GetPassedResults()
	fmt.Printf("筛选完成! 通过: %d/%d 个岗位\n", len(passed), len(session.Results))
	for _, p := range passed {
		fmt.Printf("  ✅ %s | %s | 可信度: %.2f | 匹配度: %.2f\n", p.Company, p.Position, p.TrustScore, p.MatchScore)
	}
	rejected := session.GetRejectedResults()
	for _, r := range rejected {
		fmt.Printf("  ❌ %s | %s | 原因: %s\n", r.Company, r.Position, r.RejectReason)
	}
	return nil
}

func handleScreeningResult(sessionID string) error {
	initDB()
	db := repository.GetDB()
	scoringRepo := repository.NewScoringRepo(db)
	scorerImpl := scorer.NewHeuristicScorer("")
	expanderImpl := keyword.NewExpander(keyword.DefaultDict())
	searchEng := search.NewWebPageSearchEngine()
	fetcherImpl := fetcher.NewHTTPFetcher()

	svc := service.NewScreeningService(scorerImpl, expanderImpl, searchEng, fetcherImpl, nil, scoringRepo)
	session, err := svc.GetScreeningResult(sessionID)
	if err != nil {
		return err
	}
	fmt.Printf("筛选会话: %s\n状态: %s\n关键词: %v\n阈值: 可信度>=%.1f 匹配度>=%.1f\n结果: %d 个岗位\n",
		session.ID, session.Status, session.Keywords, session.TrustThreshold, session.MatchThreshold, len(session.Results))
	passed := session.GetPassedResults()
	for _, p := range passed {
		fmt.Printf("[通过] %s | %s | URL: %s | 可信度: %.2f | 匹配度: %.2f\n",
			p.Company, p.Position, p.URL, p.TrustScore, p.MatchScore)
	}
	return nil
}

func handleDeliveryStart(resumeID string, targetIDs []string, format string) error {
	cfg := initDB()
	db := repository.GetDB()
	deliveryRepo := repository.NewDeliveryRepo(db)
	resumeRepo := repository.NewResumeRepo(db)
	scoringRepo := repository.NewScoringRepo(db)
	var exprt port.Exporter
	switch format {
	case "pdf":
		exprt = exporter.NewPDFExporter()
	case "docx":
		exprt = exporter.NewDOCXExporter()
	default:
		return fmt.Errorf("unsupported format: %s", format)
	}
	fileStorage := filestorage.NewLocalFileStorage()
	browserOpener := browser.NewDefaultOpener()

	svc := service.NewDeliveryService(deliveryRepo, resumeRepo, scoringRepo, exprt, fileStorage, browserOpener, cfg)
	// Build targets from scored JD IDs
	var targets []service.DeliveryTargetInput
	for _, jdID := range targetIDs {
		targets = append(targets, service.DeliveryTargetInput{
			ScoredJDID: jdID,
			Company:    jdID,
			Position:   jdID,
			URL:        "",
		})
	}
	session, err := svc.StartDelivery(context.Background(), resumeID, targets, format)
	if err != nil {
		return err
	}
	fmt.Printf("投递会话已创建 ID: %s\n目标数: %d\n到期时间: %s\n",
		session.ID, len(session.Targets), session.ExpireAt.Format("2006-01-02 15:04"))
	return nil
}

func handleDeliveryList() error {
	initDB()
	db := repository.GetDB()
	deliveryRepo := repository.NewDeliveryRepo(db)
	svc := service.NewDeliveryService(deliveryRepo, nil, nil, nil, nil, nil, nil)
	sessions, err := svc.ListDeliveries()
	if err != nil {
		return err
	}
	if len(sessions) == 0 {
		fmt.Println("暂无投递记录")
		return nil
	}
	for _, s := range sessions {
		fmt.Printf("%s | 简历: %s | 目标数: %d | 状态: %s | 过期: %s\n",
			s.ID[:8], s.ResumeID[:8], len(s.Targets), s.Status, s.ExpireAt.Format("2006-01-02"))
	}
	return nil
}

func handleDeliveryShow(sessionID string) error {
	initDB()
	db := repository.GetDB()
	deliveryRepo := repository.NewDeliveryRepo(db)
	svc := service.NewDeliveryService(deliveryRepo, nil, nil, nil, nil, nil, nil)
	session, err := svc.GetDelivery(sessionID)
	if err != nil {
		return err
	}
	fmt.Printf("投递会话: %s\n简历: %s\n状态: %s\n过期时间: %s\n剩余天数: %d\n目标:\n",
		session.ID, session.ResumeID, session.Status, session.ExpireAt.Format("2006-01-02 15:04"), session.RemainingDays())
	for _, t := range session.Targets {
		fmt.Printf("  %s | %s | %s | 状态: %s | 文件: %s\n",
			t.ID[:8], t.Company, t.Position, t.Status, t.ExportedFilePath)
	}
	return nil
}

func handleDeliveryRevoke(sessionID string) error {
	initDB()
	db := repository.GetDB()
	deliveryRepo := repository.NewDeliveryRepo(db)
	fileStorage := filestorage.NewLocalFileStorage()
	svc := service.NewDeliveryService(deliveryRepo, nil, nil, nil, fileStorage, nil, nil)
	if err := svc.RevokeDelivery(sessionID); err != nil {
		return err
	}
	fmt.Printf("投递已撤销: %s\n", sessionID)
	return nil
}
