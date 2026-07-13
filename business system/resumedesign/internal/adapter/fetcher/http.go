package fetcher

import (
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"resumedesign/internal/port"

	"golang.org/x/net/html"
)

// HTTPFetcher HTTP 爬取 JD 获取器
type HTTPFetcher struct {
	client *http.Client
}

func NewHTTPFetcher() *HTTPFetcher {
	return &HTTPFetcher{
		client: &http.Client{
			Timeout: 30 * time.Second,
		},
	}
}

func (f *HTTPFetcher) FetchFromURL(url string, timeout time.Duration) (*port.FetchedJD, error) {
	if !strings.HasPrefix(url, "http://") && !strings.HasPrefix(url, "https://") {
		return nil, fmt.Errorf("invalid URL: %s", url)
	}

	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return nil, fmt.Errorf("create request failed: %w", err)
	}
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")

	client := &http.Client{Timeout: timeout}
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("fetch URL failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("fetch URL returned status %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("read response failed: %w", err)
	}

	// 尝试提取网页标题作为职位名
	title := extractTitle(string(body))
	// 提取纯文本内容
	textContent := extractText(string(body))

	return &port.FetchedJD{
		SourceURL:  url,
		Company:    extractCompany(textContent),
		Position:   title,
		RawContent: textContent,
	}, nil
}

// extractTitle 从 HTML 中提取 title 标签内容
func extractTitle(htmlContent string) string {
	doc, err := html.Parse(strings.NewReader(htmlContent))
	if err != nil {
		return ""
	}
	var title string
	var f func(*html.Node)
	f = func(n *html.Node) {
		if n.Type == html.ElementNode && n.Data == "title" && n.FirstChild != nil {
			title = n.FirstChild.Data
			return
		}
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			f(c)
		}
	}
	f(doc)
	return title
}

// extractText 从 HTML 中提取纯文本
func extractText(htmlContent string) string {
	doc, err := html.Parse(strings.NewReader(htmlContent))
	if err != nil {
		return htmlContent
	}
	var text strings.Builder
	var f func(*html.Node)
	f = func(n *html.Node) {
		if n.Type == html.TextNode {
			text.WriteString(strings.TrimSpace(n.Data))
			text.WriteString("\n")
		}
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			f(c)
		}
	}
	f(doc)
	return text.String()
}

// extractCompany 简单地从文本中猜测公司名（占位逻辑）
func extractContent(text string) string {
	// 简单截取前 5000 字符作为 JD 内容
	if len(text) > 5000 {
		return text[:5000]
	}
	return text
}

func extractCompany(text string) string {
	lines := strings.SplitN(text, "\n", 10)
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line != "" && len(line) < 50 {
			return line
		}
	}
	return ""
}
