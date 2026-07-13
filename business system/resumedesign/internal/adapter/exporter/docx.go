package exporter

import (
	"archive/zip"
	"fmt"
	"os"
	"strings"
)

// DOCXExporter DOCX 导出器（自建 ZIP+XML 生成）
type DOCXExporter struct{}

func NewDOCXExporter() *DOCXExporter {
	return &DOCXExporter{}
}

func (e *DOCXExporter) Export(content string, format string, outputPath string) error {
	if format != "docx" {
		return fmt.Errorf("DOCXExporter: unsupported format %s", format)
	}

	// 创建 DOCX 文件（ZIP 包）
	f, err := os.Create(outputPath)
	if err != nil {
		return fmt.Errorf("DOCXExporter: create file failed: %w", err)
	}
	defer f.Close()

	w := zip.NewWriter(f)
	defer w.Close()

	// 写入 [Content_Types].xml
	contentTypes := `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="xml" ContentType="application/xml"/>
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
</Types>`
	if err := writeZipEntry(w, "[Content_Types].xml", contentTypes); err != nil {
		return err
	}

	// 写入 _rels/.rels
	rels := `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>`
	if err := writeZipEntry(w, "_rels/.rels", rels); err != nil {
		return err
	}

	// 写入 word/_rels/document.xml.rels
	wordRels := `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
</Relationships>`
	if err := writeZipEntry(w, "word/_rels/document.xml.rels", wordRels); err != nil {
		return err
	}

	// 生成 document.xml
	docXML := e.buildDocumentXML(content)
	if err := writeZipEntry(w, "word/document.xml", docXML); err != nil {
		return err
	}

	return nil
}

func (e *DOCXExporter) buildDocumentXML(content string) string {
	var sb strings.Builder
	sb.WriteString(`<?xml version="1.0" encoding="UTF-8" standalone="yes"?>`)
	sb.WriteString(`<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">`)
	sb.WriteString(`<w:body>`)

	lines := strings.Split(content, "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			sb.WriteString(`<w:p><w:r><w:br/></w:r></w:p>`)
			continue
		}
		// 转义 XML 特殊字符
		escaped := strings.ReplaceAll(line, "&", "&amp;")
		escaped = strings.ReplaceAll(escaped, "<", "&lt;")
		escaped = strings.ReplaceAll(escaped, ">", "&gt;")

		sb.WriteString(`<w:p><w:r><w:t>`)
		sb.WriteString(escaped)
		sb.WriteString(`</w:t></w:r></w:p>`)
	}

	sb.WriteString(`</w:body>`)
	sb.WriteString(`</w:document>`)
	return sb.String()
}

func writeZipEntry(w *zip.Writer, name, content string) error {
	entry, err := w.Create(name)
	if err != nil {
		return fmt.Errorf("create zip entry %s failed: %w", name, err)
	}
	_, err = entry.Write([]byte(content))
	return err
}
