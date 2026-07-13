package parser

import (
	"archive/zip"
	"encoding/xml"
	"fmt"
	"io"
	"os"
	"strings"

	"resumedesign/internal/port"
)

// DOCXParser DOCX 简历解析器（自建 ZIP+XML 解析）
type DOCXParser struct{}

func NewDOCXParser() *DOCXParser {
	return &DOCXParser{}
}

// wordDocument 表示 docx 文档的 XML 结构（简化版）
type wordDocument struct {
	Body struct {
		Paragraphs []struct {
			Runs []struct {
				Text string `xml:"t"`
			} `xml:"r"`
		} `xml:"p"`
	} `xml:"body"`
}

func (p *DOCXParser) Parse(filePath string, format string) ([]port.ParsedSection, error) {
	if format != "docx" {
		return nil, fmt.Errorf("DOCXParser: unsupported format %s", format)
	}

	r, err := zip.OpenReader(filePath)
	if err != nil {
		return nil, fmt.Errorf("DOCXParser: cannot open zip: %w", err)
	}
	defer r.Close()

	// 查找 document.xml
	var docFile io.ReadCloser
	for _, f := range r.File {
		if f.Name == "word/document.xml" {
			docFile, err = f.Open()
			if err != nil {
				return nil, fmt.Errorf("DOCXParser: cannot open document.xml: %w", err)
			}
			defer docFile.Close()
			break
		}
	}
	if docFile == nil {
		return nil, fmt.Errorf("DOCXParser: document.xml not found in docx")
	}

	// 解析 XML
	data, err := io.ReadAll(docFile)
	if err != nil {
		return nil, fmt.Errorf("DOCXParser: read document.xml failed: %w", err)
	}

	var doc wordDocument
	if err := xml.Unmarshal(data, &doc); err != nil {
		return nil, fmt.Errorf("DOCXParser: parse document.xml failed: %w", err)
	}

	// 提取文本
	var textLines []string
	for _, p := range doc.Body.Paragraphs {
		var line string
		for _, r := range p.Runs {
			line += r.Text
		}
		textLines = append(textLines, line)
	}

	content := strings.Join(textLines, "\n")

	// 写入临时文件并复用 TXTParser
	tmpFile, err := os.CreateTemp("", "resumedesign-docx-*.txt")
	if err != nil {
		return nil, fmt.Errorf("DOCXParser: create temp file failed: %w", err)
	}
	defer os.Remove(tmpFile.Name())

	if _, err := tmpFile.WriteString(content); err != nil {
		tmpFile.Close()
		return nil, fmt.Errorf("DOCXParser: write temp file failed: %w", err)
	}
	tmpFile.Close()

	txtParser := NewTXTParser()
	sections, err := txtParser.Parse(tmpFile.Name(), "txt")
	if err != nil {
		return nil, fmt.Errorf("DOCXParser: parse text failed: %w", err)
	}

	filtered := make([]port.ParsedSection, 0, len(sections))
	for _, s := range sections {
		if strings.TrimSpace(s.Content) != "" || s.Type != "" {
			filtered = append(filtered, s)
		}
	}

	return filtered, nil
}
