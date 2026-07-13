package port

// Exporter 简历导出器 Port 接口
// 职责：将改写后的简历内容导出为指定格式文件
// 实现：adapter/exporter/pdf.go，adapter/exporter/docx.go
type Exporter interface {
	// Export 导出简历
	// content: 简历文本内容
	// format: "pdf" | "docx"
	// outputPath: 输出文件路径
	Export(content string, format string, outputPath string) error
}
