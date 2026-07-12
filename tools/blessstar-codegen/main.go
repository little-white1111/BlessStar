package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/blessstar/blessstar-codegen/backend"
	c_backend "github.com/blessstar/blessstar-codegen/backend/c"
	go_backend "github.com/blessstar/blessstar-codegen/backend/go"
	java_backend "github.com/blessstar/blessstar-codegen/backend/java"
	python_backend "github.com/blessstar/blessstar-codegen/backend/python"
	ts_backend "github.com/blessstar/blessstar-codegen/backend/ts"
	"github.com/blessstar/blessstar-codegen/parser"
	"github.com/blessstar/blessstar-codegen/types"
)

type schemaContext struct {
	schema        *types.ConfigSchema
	schemaVersion string
}

func main() {
	lang := flag.String("lang", "go", "Target language(s), comma-separated (go, java, python, c)")
	manifestPath := flag.String("manifest", "", "Path to manifest.json")
	metadataPath := flag.String("metadata", "", "Path to config_metadata.json (optional)")
	schemaPath := flag.String("schema", "", "Path to config-schema.yaml (Schema-First mode)")
	outputDir := flag.String("output", "biz-adapters", "Output directory for generated code")
	checkMode := flag.Bool("check", false, "CI mode: generate to temp dir and diff with existing; exit 1 if different")
	dryRun := flag.Bool("dry-run", false, "Preview generated files without writing to disk")
	checkOnly := flag.Bool("check-consistency", false, "Only validate manifest-metadata consistency, skip code generation")
	schemaVersion := flag.String("schema-version", "", "Schema version (e.g., \"v1\") for SchemaLoader matching")
	genTests := flag.Bool("gen-tests", false, "Generate boundary test cases to test_cases/ directory")
	genObs := flag.Bool("gen-obs", false, "Generate observability rules to observability/ directory")
	bundledMode := flag.Bool("bundled", false, "Input is a config-schema.bundled.yaml (bundled cache format)")
	flag.Parse()

	// ─── Schema-First 模式 vs 传统模式 ───
	schemaMode := *schemaPath != ""

	if schemaMode {
		// Schema-First 路径
		if _, err := os.Stat(*schemaPath); os.IsNotExist(err) {
			log.Fatalf("config-schema.yaml not found: %s", *schemaPath)
		}

		if *bundledMode {
			// Bundled 格式：自动检测 + 解析
			isBundled, err := parser.IsBundledSchema(*schemaPath)
			if err != nil {
				log.Fatalf("Failed to check bundled schema: %v", err)
			}
			if !isBundled {
				log.Fatalf("--bundled flag set but %s is not a bundled schema (missing generated_at)", *schemaPath)
			}
			fmt.Println("📦 Bundled Schema 模式: 使用 config-schema.bundled.yaml 作为缓存源")
		} else {
			fmt.Println("📐 Schema-First 模式: 使用 config-schema.yaml 作为唯一真理源")
		}
		fmt.Println()

		schema, err := parser.ParseSchemaYAML(*schemaPath)
		if err != nil {
			log.Fatalf("Failed to parse schema: %v", err)
		}

		fmt.Printf("📋 Schema domain: %s, version: %s\n", schema.Domain, schema.Version)
		fmt.Printf("   配置字段数: %d\n", len(schema.Fields))
		fmt.Println()

		// Use schema domain as bizID, and domain as displayName
		bizID := schema.Domain
		displayName := schema.Domain
		if *schemaVersion != "" {
			bizID = bizID + "-" + *schemaVersion
		}

		biz, err := parser.SchemaToBizSystem(schema, bizID, displayName)
		if err != nil {
			log.Fatalf("Failed to build biz system from schema: %v", err)
		}

		// Parse target languages
		languages := parseLanguages(*lang)
		allGenerated := make(map[string][]*backend.File)

		for _, l := range languages {
			gen := selectBackend(l)
			if gen == nil {
				log.Fatalf("Unsupported language: %s (supported: go, java, python, c, ts)", l)
			}

			fmt.Printf("🔧 正在生成 [%s] 代码...\n", gen.Name())
			files := generateForBackendSchema(gen, biz, *outputDir)
			allGenerated[l] = files

			// Schema-First 附加产物
			extraFiles := generateSchemaArtifacts(gen, biz, schema, *genTests, *genObs)
			allGenerated[l] = append(allGenerated[l], extraFiles...)

			fmt.Printf("   ✅ %s: %d 个文件已生成\n", gen.Name(), len(files)+len(extraFiles))
		}

		// Handle --check mode
		if *checkMode {
			runCheckMode(allGenerated, biz.BizID, *outputDir)
			return
		}

		// Handle --dry-run mode
		if *dryRun {
			runDryRun(allGenerated, biz.BizID, *outputDir)
			return
		}

		// Write files
		totalWritten := writeGeneratedFiles(allGenerated, biz.BizID, *outputDir)
		fmt.Printf("\n🎉 Schema-First 生成完成! %d 个文件已写入 %s\n", totalWritten, filepath.Join(*outputDir, biz.BizID))
		return
	}

	// ─── 传统 JSON 模式（向后兼容） ───
	if *manifestPath == "" {
		log.Fatal("--manifest is required when --schema is not provided")
	}

	if _, err := os.Stat(*manifestPath); os.IsNotExist(err) {
		log.Fatalf("manifest.json not found: %s", *manifestPath)
	}

	// ─── Step 1: Validate manifest-metadata consistency ───
	fmt.Println("🔍 正在校验 manifest.json 与 config_metadata.json 一致性...")
	if !parser.ValidateConsistencyOrExit(*manifestPath, *metadataPath) {
		os.Exit(1)
	}
	fmt.Println("   ✅ 一致性校验通过")

	// If --check-consistency only, exit here
	if *checkOnly {
		return
	}
	fmt.Println()

	// ─── Step 2: Build business system model ───
	biz, err := parser.BuildBizSystem(*manifestPath, *metadataPath)
	if err != nil {
		log.Fatalf("Failed to build business system model: %v", err)
	}

	fmt.Printf("📋 已加载业务系统: %s (%s)\n", biz.DisplayName, biz.BizID)
	fmt.Printf("   配置项总数: %d\n", len(biz.AllConfigs))
	fmt.Printf("   域名数量: %d\n", len(biz.DomainShards))
	fmt.Println()

	// ─── Step 3: Parse target languages ───
	languages := parseLanguages(*lang)

	// ─── Step 4: Generate for each language ───
	allGenerated := make(map[string][]*backend.File) // lang -> files

	for _, l := range languages {
		gen := selectBackend(l)
		if gen == nil {
			log.Fatalf("Unsupported language: %s (supported: go, java, python, c, ts)", l)
		}

		fmt.Printf("🔧 正在生成 [%s] 代码...\n", gen.Name())
		files := generateForBackend(gen, biz, *outputDir)
		allGenerated[l] = files
		fmt.Printf("   ✅ %s: %d 个文件已生成\n", gen.Name(), len(files))
	}

	// ─── Step 5: Handle --check mode (CI diff) ───
	if *checkMode {
		runCheckMode(allGenerated, biz.BizID, *outputDir)
		return
	}

	// ─── Step 6: Handle --dry-run mode (preview) ───
	if *dryRun {
		runDryRun(allGenerated, biz.BizID, *outputDir)
		return
	}

	// ─── Step 7: Write files to disk ───
	totalWritten := writeGeneratedFiles(allGenerated, biz.BizID, *outputDir)
	fmt.Printf("\n🎉 生成完成! %d 个文件已写入 %s\n", totalWritten, filepath.Join(*outputDir, biz.BizID))
}

// generateSchemaArtifacts 生成 Schema-First 模式下的附加产物
func generateSchemaArtifacts(gen backend.LanguageBackend, biz *types.BizSystem, schema *types.ConfigSchema, genTests, genObs bool) []*backend.File {
	var files []*backend.File

	// Extract gate configs from schema contracts
	gateConfigs := parser.ExtractGateConfigs(schema)
	if len(gateConfigs) > 0 {
		gateFile, err := gen.GenerateGateConfigs(biz, gateConfigs)
		if err != nil {
			log.Printf("⚠️  [%s] Failed to generate gate configs: %v", gen.Name(), err)
		} else if gateFile != nil {
			gateFile.Content = prependHeaderSchema(gateFile.Path, gateFile.Content, biz, true)
			files = append(files, gateFile)
		}
	}

	// Generate test cases if requested
	if genTests {
		testCases := parser.GenerateTestCases(schema)
		if len(testCases) > 0 {
			testFile, err := gen.GenerateTestCases(biz, testCases)
			if err != nil {
				log.Printf("⚠️  [%s] Failed to generate test cases: %v", gen.Name(), err)
			} else if testFile != nil {
				testFile.Content = prependHeaderSchema(testFile.Path, testFile.Content, biz, true)
				files = append(files, testFile)
			}
		}
	}

	// Generate observability rules if requested
	if genObs {
		obsRules := parser.ExtractObservabilityRules(schema)
		if len(obsRules) > 0 {
			obsFile, err := gen.GenerateObservability(biz, obsRules)
			if err != nil {
				log.Printf("⚠️  [%s] Failed to generate observability rules: %v", gen.Name(), err)
			} else if obsFile != nil {
				obsFile.Content = prependHeaderSchema(obsFile.Path, obsFile.Content, biz, true)
				files = append(files, obsFile)
			}
		}
	}

	return files
}

// writeGeneratedFiles writes all generated files to disk and returns the total count.
func writeGeneratedFiles(allGenerated map[string][]*backend.File, bizID, outputDir string) int {
	totalWritten := 0
	for lang, files := range allGenerated {
		bizOutputDir := filepath.Join(outputDir, lang, bizID)
		for _, f := range files {
			fullPath := filepath.Join(bizOutputDir, f.Path)
			dir := filepath.Dir(fullPath)
			if err := os.MkdirAll(dir, 0755); err != nil {
				log.Printf("⚠️  [%s] Failed to create directory %s: %v", lang, dir, err)
				continue
			}
			if err := os.WriteFile(fullPath, []byte(f.Content), 0644); err != nil {
				log.Printf("⚠️  [%s] Failed to write %s: %v", lang, fullPath, err)
				continue
			}
			totalWritten++
		}
	}
	return totalWritten
}

// parseLanguages parses a comma-separated language string, deduplicates, and returns sorted list.
func parseLanguages(langFlag string) []string {
	seen := make(map[string]bool)
	var result []string
	for _, l := range strings.Split(langFlag, ",") {
		l = strings.TrimSpace(l)
		if l == "" || seen[l] {
			continue
		}
		seen[l] = true
		result = append(result, l)
	}
	// Default to ["go"] if empty after parsing
	if len(result) == 0 {
		return []string{"go"}
	}
	return result
}

// selectBackend returns the appropriate LanguageBackend for the given language name.
func selectBackend(lang string) backend.LanguageBackend {
	switch lang {
	case "go":
		return go_backend.New()
	case "java":
		return java_backend.New()
	case "python":
		return python_backend.New()
	case "c":
		return c_backend.New()
	case "ts":
		return ts_backend.New()
	default:
		return nil
	}
}

// generateForBackend runs all generation methods for a given backend and returns the files.
func generateForBackend(gen backend.LanguageBackend, biz *types.BizSystem, outputDir string) []*backend.File {
	var files []*backend.File

	// Generate per-domain files
	for domain, configs := range biz.ConfigsByDomain {
		if len(configs) == 0 {
			continue
		}

		// Port interface
		portFile, err := gen.GeneratePortInterface(biz, domain, configs)
		if err != nil {
			log.Printf("⚠️  [%s] Failed to generate port for domain %s: %v", gen.Name(), domain, err)
			continue
		}
		if portFile != nil {
			portFile.Content = prependHeader(portFile.Path, portFile.Content, biz)
			files = append(files, portFile)
		}

		// BlessStar adapter
		adapterFile, err := gen.GenerateBlessStarAdapter(biz, domain, configs)
		if err != nil {
			log.Printf("⚠️  [%s] Failed to generate adapter for domain %s: %v", gen.Name(), domain, err)
			continue
		}
		if adapterFile != nil {
			adapterFile.Content = prependHeader(adapterFile.Path, adapterFile.Content, biz)
			files = append(files, adapterFile)
		}

		// Mock adapter
		mockFile, err := gen.GenerateMockAdapter(biz, domain, configs)
		if err != nil {
			log.Printf("⚠️  [%s] Failed to generate mock for domain %s: %v", gen.Name(), domain, err)
			continue
		}
		if mockFile != nil {
			mockFile.Content = prependHeader(mockFile.Path, mockFile.Content, biz)
			files = append(files, mockFile)
		}
	}

	// Generate provider
	providerFile, err := gen.GenerateProvider(biz)
	if err != nil {
		log.Printf("⚠️  [%s] Failed to generate provider: %v", gen.Name(), err)
	} else if providerFile != nil {
		providerFile.Content = prependHeader(providerFile.Path, providerFile.Content, biz)
		files = append(files, providerFile)
	}

	// Generate go.mod / pom.xml / CMakeLists.txt
	gomodFile, err := gen.GenerateGoMod(biz)
	if err != nil {
		log.Printf("⚠️  [%s] Failed to generate build file: %v", gen.Name(), err)
	} else if gomodFile != nil {
		gomodFile.Content = prependHeader(gomodFile.Path, gomodFile.Content, biz)
		files = append(files, gomodFile)
	}

	// Generate ConfigReader interface + CachedReader decorator
	readerFiles, err := gen.GenerateConfigReaderFile(biz)
	if err != nil {
		log.Printf("⚠️  [%s] Failed to generate ConfigReader files: %v", gen.Name(), err)
	} else {
		for _, f := range readerFiles {
			f.Content = prependHeader(f.Path, f.Content, biz)
			files = append(files, f)
		}
	}

	// Generate language-specific init files (e.g. Python __init__.py)
	initFiles, err := gen.GenerateInitFiles(biz)
	if err != nil {
		log.Printf("⚠️  [%s] Failed to generate init files: %v", gen.Name(), err)
	} else {
		for _, f := range initFiles {
			f.Content = prependHeader(f.Path, f.Content, biz)
			files = append(files, f)
		}
	}

	return files
}

// generateForBackendSchema is the schema-aware version of generateForBackend.
// It uses prependHeaderSchema to annotate files with "Source: config-schema.yaml".
func generateForBackendSchema(gen backend.LanguageBackend, biz *types.BizSystem, outputDir string) []*backend.File {
	var files []*backend.File

	// Generate per-domain files
	for domain, configs := range biz.ConfigsByDomain {
		if len(configs) == 0 {
			continue
		}

		// Port interface
		portFile, err := gen.GeneratePortInterface(biz, domain, configs)
		if err != nil {
			log.Printf("⚠️  [%s] Failed to generate port for domain %s: %v", gen.Name(), domain, err)
			continue
		}
		if portFile != nil {
			portFile.Content = prependHeaderSchema(portFile.Path, portFile.Content, biz, true)
			// Replace hardcoded "Source: manifest.json" in port file headers
			portFile.Content = strings.ReplaceAll(portFile.Content, "Source: manifest.json", "Source: config-schema.yaml")
			files = append(files, portFile)
		}

		// BlessStar adapter
		adapterFile, err := gen.GenerateBlessStarAdapter(biz, domain, configs)
		if err != nil {
			log.Printf("⚠️  [%s] Failed to generate adapter for domain %s: %v", gen.Name(), domain, err)
			continue
		}
		if adapterFile != nil {
			adapterFile.Content = prependHeaderSchema(adapterFile.Path, adapterFile.Content, biz, true)
			adapterFile.Content = strings.ReplaceAll(adapterFile.Content, "Source: manifest.json", "Source: config-schema.yaml")
			files = append(files, adapterFile)
		}

		// Mock adapter
		mockFile, err := gen.GenerateMockAdapter(biz, domain, configs)
		if err != nil {
			log.Printf("⚠️  [%s] Failed to generate mock for domain %s: %v", gen.Name(), domain, err)
			continue
		}
		if mockFile != nil {
			mockFile.Content = prependHeaderSchema(mockFile.Path, mockFile.Content, biz, true)
			mockFile.Content = strings.ReplaceAll(mockFile.Content, "Source: manifest.json", "Source: config-schema.yaml")
			files = append(files, mockFile)
		}
	}

	// Generate provider
	providerFile, err := gen.GenerateProvider(biz)
	if err != nil {
		log.Printf("⚠️  [%s] Failed to generate provider: %v", gen.Name(), err)
	} else if providerFile != nil {
		providerFile.Content = prependHeaderSchema(providerFile.Path, providerFile.Content, biz, true)
		files = append(files, providerFile)
	}

	// Generate go.mod / pom.xml / CMakeLists.txt
	gomodFile, err := gen.GenerateGoMod(biz)
	if err != nil {
		log.Printf("⚠️  [%s] Failed to generate build file: %v", gen.Name(), err)
	} else if gomodFile != nil {
		gomodFile.Content = prependHeaderSchema(gomodFile.Path, gomodFile.Content, biz, true)
		files = append(files, gomodFile)
	}

	// Generate ConfigReader interface + CachedReader decorator
	readerFiles, err := gen.GenerateConfigReaderFile(biz)
	if err != nil {
		log.Printf("⚠️  [%s] Failed to generate ConfigReader files: %v", gen.Name(), err)
	} else {
		for _, f := range readerFiles {
			f.Content = prependHeaderSchema(f.Path, f.Content, biz, true)
			f.Content = strings.ReplaceAll(f.Content, "Source: manifest.json", "Source: config-schema.yaml")
			files = append(files, f)
		}
	}

	// Generate language-specific init files (e.g. Python __init__.py)
	initFiles, err := gen.GenerateInitFiles(biz)
	if err != nil {
		log.Printf("⚠️  [%s] Failed to generate init files: %v", gen.Name(), err)
	} else {
		for _, f := range initFiles {
			f.Content = prependHeaderSchema(f.Path, f.Content, biz, true)
			files = append(files, f)
		}
	}

	return files
}

// prependHeader adds the DO NOT EDIT header to generated content.
// It deduplicates: if the content already has a DO NOT EDIT line, it's not added again.
func prependHeader(path, content string, biz *types.BizSystem) string {
	return prependHeaderSchema(path, content, biz, false)
}

// prependHeaderSchema adds the DO NOT EDIT header with schema-aware source annotation.
func prependHeaderSchema(path, content string, biz *types.BizSystem, schemaMode bool) string {
	if strings.Contains(content, "DO NOT EDIT") {
		return content
	}
	// Skip XML files (pom.xml) - XML declaration must be first in file
	if strings.HasSuffix(path, ".xml") {
		return content
	}
	var header string
	if schemaMode {
		header = backend.HeaderForBizWithSource(path, biz.BizID, biz.DisplayName, "config-schema.yaml")
	} else {
		header = backend.HeaderForBiz(path, biz.BizID, biz.DisplayName)
	}
	return header + "\n" + content
}

// runCheckMode generates code to a temp directory and diffs with the existing output.
// Exits with code 1 if differences are found (CI use case).
func runCheckMode(allGenerated map[string][]*backend.File, bizID, outputDir string) {
	tmpDir, err := os.MkdirTemp("", "blessstar-codegen-check-*")
	if err != nil {
		log.Fatalf("Failed to create temp directory: %v", err)
	}
	defer os.RemoveAll(tmpDir)

	fmt.Println("\n🔍 --check 模式: 生成代码到临时目录并比对差异...")

	// Write all generated files to temp dir
	written := 0
	for lang, files := range allGenerated {
		bizOutputDir := filepath.Join(tmpDir, lang, bizID)
		for _, f := range files {
			fullPath := filepath.Join(bizOutputDir, f.Path)
			dir := filepath.Dir(fullPath)
			if err := os.MkdirAll(dir, 0755); err != nil {
				log.Printf("⚠️  [%s] Failed to create temp dir %s: %v", lang, dir, err)
				continue
			}
			if err := os.WriteFile(fullPath, []byte(f.Content), 0644); err != nil {
				log.Printf("⚠️  [%s] Failed to write temp file %s: %v", lang, fullPath, err)
				continue
			}
			written++
		}
	}
	fmt.Printf("   临时目录: %s (%d 个文件)\n", tmpDir, written)

	// Diff with existing output — per-language
	anyDiff := false
	for lang := range allGenerated {
		existingDir := filepath.Join(outputDir, lang, bizID)
		if _, err := os.Stat(existingDir); os.IsNotExist(err) {
			fmt.Printf("   ❌ [%s] 现有代码目录不存在: %s\n", lang, existingDir)
			fmt.Println("   💡 请先运行 blessstar-codegen 生成代码")
			os.Exit(1)
		}

		// git diff --no-index doesn't support pathspec excludes, so we copy
		// non-generated directories (e.g. tests/, src/test/) from existing to
		// tmp before comparing. This way, hand-written smoke test files in
		// existing/ that are not produced by the code generator won't cause
		// false positives.
		nonGeneratedDirs := []string{"tests", "src"}
		tmpLangDir := filepath.Join(tmpDir, lang, bizID)
		for _, d := range nonGeneratedDirs {
			src := filepath.Join(existingDir, d)
			if _, err := os.Stat(src); err == nil {
				dst := filepath.Join(tmpLangDir, d)
				// Use src/. dst/ to copy the *contents* of src/ into dst/,
				// not the directory itself. This matters when dst/ already
				// exists (e.g. Java's src/ is created by codegen).
				if err := exec.Command("cp", "-r", src+string(filepath.Separator)+".", dst+string(filepath.Separator)).Run(); err != nil {
					log.Printf("⚠️  [%s] Failed to copy non-generated dir %s: %v", lang, d, err)
				}
			}
		}
		cmd := exec.Command("git", "diff", "--no-index", "--exit-code", existingDir, tmpLangDir)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr

		if err := cmd.Run(); err != nil {
			if exitErr, ok := err.(*exec.ExitError); ok && exitErr.ExitCode() == 1 {
				fmt.Printf("\n❌ [%s] 生成的代码与现有代码不一致!\n", lang)
				anyDiff = true
			} else {
				fmt.Printf("⚠️  [%s] diff 执行失败: %v\n", lang, err)
			}
		} else {
			fmt.Printf("   ✅ [%s] 一致\n", lang)
		}
	}

	if anyDiff {
		fmt.Println("\n❌ 存在不一致的语言，请运行 blessstar-codegen 并提交生成的代码")
		os.Exit(1)
	}

	fmt.Println("\n✅ 所有语言生成的代码与现有代码完全一致!")
}

// runDryRun prints a preview of all generated files without writing them.
func runDryRun(allGenerated map[string][]*backend.File, bizID, outputDir string) {
	fmt.Println("\n📋 --dry-run 模式: 以下文件将被生成")
	fmt.Println(strings.Repeat("─", 60))

	totalFiles := 0
	for lang, files := range allGenerated {
		fmt.Printf("\n  [%s]\n", strings.ToUpper(lang))
		for _, f := range files {
			bizOutputDir := filepath.Join(outputDir, lang, bizID)
			fullPath := filepath.Join(bizOutputDir, f.Path)
			// Count lines
			lineCount := strings.Count(f.Content, "\n")
			fmt.Printf("    📄 %s (%d 行)\n", fullPath, lineCount)
			totalFiles++
		}
	}

	fmt.Println(strings.Repeat("─", 60))
	fmt.Printf("\n📊 总计: %d 个文件 (语言: ", totalFiles)
	first := true
	for lang := range allGenerated {
		if !first {
			fmt.Print(", ")
		}
		fmt.Print(strings.ToUpper(lang))
		first = false
	}
	fmt.Println(")")
	fmt.Println("\n💡 移除 --dry-run 标志以实际写入文件")
}


