package port

import "time"

// JDFetcher JD 获取器 Port 接口
// 职责：从 URL 或外部 API 获取岗位描述内容
// 实现：adapter/fetcher/http.go（URL 爬取），adapter/fetcher/api.go（招聘 API）
type JDFetcher interface {
	// FetchFromURL 从 URL 抓取 JD 内容
	FetchFromURL(url string, timeout time.Duration) (*FetchedJD, error)
}

// FetchedJD 获取到的岗位描述
type FetchedJD struct {
	SourceURL  string
	Company    string
	Position   string
	RawContent string
}
