package parser

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"regexp"
	"strings"

	"resumedesign/internal/port"

	"github.com/pdfcpu/pdfcpu/pkg/api"
	"github.com/pdfcpu/pdfcpu/pkg/pdfcpu/model"
)

// PDFParser PDF 简历解析器（基于 pdfcpu）
type PDFParser struct{}

func NewPDFParser() *PDFParser {
	return &PDFParser{}
}

func (p *PDFParser) Parse(filePath string, format string) ([]port.ParsedSection, error) {
	if format != "pdf" {
		return nil, fmt.Errorf("PDFParser: unsupported format %s", format)
	}

	f, err := os.Open(filePath)
	if err != nil {
		return nil, fmt.Errorf("PDFParser: cannot open file %s: %w", filePath, err)
	}
	defer f.Close()

	conf := model.NewDefaultConfiguration()

	// 使用 pdfcpu ExtractContent 提取页面内容
	var buf bytes.Buffer
	if err := api.ExtractContent(f, nil, func(r io.Reader, pageNr int) error {
		data, readErr := io.ReadAll(r)
		if readErr != nil {
			return readErr
		}
		text := extractTextFromContent(string(data))
		_, _ = buf.WriteString(text)
		_, _ = buf.WriteString("\n")
		return nil
	}, conf); err != nil {
		return nil, fmt.Errorf("PDFParser: extract content failed: %w", err)
	}

	content := buf.String()

	// 将提取的文本写入临时文件，复用 TXTParser 的段落解析逻辑
	tmpFile, err := os.CreateTemp("", "resumedesign-pdf-*.txt")
	if err != nil {
		return nil, fmt.Errorf("PDFParser: create temp file failed: %w", err)
	}
	defer os.Remove(tmpFile.Name())

	if _, err := tmpFile.WriteString(content); err != nil {
		tmpFile.Close()
		return nil, fmt.Errorf("PDFParser: write temp file failed: %w", err)
	}
	tmpFile.Close()

	txtParser := NewTXTParser()
	sections, err := txtParser.Parse(tmpFile.Name(), "txt")
	if err != nil {
		return nil, fmt.Errorf("PDFParser: parse text failed: %w", err)
	}

	// 去除空段落
	filtered := make([]port.ParsedSection, 0, len(sections))
	for _, s := range sections {
		if strings.TrimSpace(s.Content) != "" || s.Type != "" {
			filtered = append(filtered, s)
		}
	}

	return filtered, nil
}

// extractTextFromContent 从 PDF 原始内容操作符中提取文本
// PDF 文本通常以 (content) Tj 或 [(content) number (content)] TJ 形式出现
func extractTextFromContent(content string) string {
	// 匹配 (content) Tj
	re := regexp.MustCompile(`\(([^)]*)\)\s*Tj`)
	matches := re.FindAllStringSubmatch(content, -1)
	var parts []string
	for _, m := range matches {
		parts = append(parts, m[1])
	}
	return strings.Join(parts, "\n")
}
