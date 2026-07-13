package exporter

import (
	"fmt"
	"os"
	"strings"

	"github.com/jung-kurt/gofpdf"
)

// PDFExporter PDF 导出器
type PDFExporter struct{}

func NewPDFExporter() *PDFExporter {
	return &PDFExporter{}
}

func (e *PDFExporter) Export(content string, format string, outputPath string) error {
	if format != "pdf" {
		return fmt.Errorf("PDFExporter: unsupported format %s", format)
	}

	pdf := gofpdf.New("P", "mm", "A4", "")
	pdf.AddPage()
	pdf.SetFont("SimSun", "", 12)

	// 按行写入 PDF
	lines := strings.Split(content, "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			pdf.Ln(4)
			continue
		}
		pdf.MultiCell(0, 6, line, "", "", false)
	}

	if err := pdf.OutputFileAndClose(outputPath); err != nil {
		return fmt.Errorf("PDFExporter: write file failed: %w", err)
	}

	// 验证文件已写入
	if _, err := os.Stat(outputPath); os.IsNotExist(err) {
		return fmt.Errorf("PDFExporter: output file was not created: %s", outputPath)
	}

	return nil
}
