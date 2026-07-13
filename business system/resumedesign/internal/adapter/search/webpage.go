package search

import (
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"resumedesign/internal/port"

	"golang.org/x/net/html"
)

// WebPageSearchEngine 招聘网页搜索结果解析实现
type WebPageSearchEngine struct {
	client *http.Client
}

func NewWebPageSearchEngine() *WebPageSearchEngine {
	return &WebPageSearchEngine{
		client: &http.Client{
			Timeout: 30 * time.Second,
		},
	}
}

// Search 根据关键词搜索岗位列表
// 注意：此为框架实现，实际需要适配具体招聘网站的搜索 URL 和 HTML 结构
func (e *WebPageSearchEngine) Search(keywords []string, page int) ([]*port.JDLink, error) {
	if len(keywords) == 0 {
		return nil, fmt.Errorf("keywords required")
	}

	query := strings.Join(keywords, " ")
	url := fmt.Sprintf("https://www.zhipin.com/web/geek/job?query=%s&page=%d", urlEncode(query), page)

	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return nil, fmt.Errorf("create request failed: %w", err)
	}
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")
	req.Header.Set("Accept", "text/html,application/xhtml+xml")

	resp, err := e.client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("search request failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("search returned status %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("read response failed: %w", err)
	}

	return e.ParseResultPage(string(body))
}

// ParseResultPage 解析搜索结果页 HTML
// 注意：此实现基于通用 HTML 结构，实际需要根据具体招聘网站调整选择器
func (e *WebPageSearchEngine) ParseResultPage(htmlContent string) ([]*port.JDLink, error) {
	doc, err := html.Parse(strings.NewReader(htmlContent))
	if err != nil {
		return nil, fmt.Errorf("parse HTML failed: %w", err)
	}

	var links []*port.JDLink
	seen := make(map[string]bool)

	var f func(*html.Node)
	f = func(n *html.Node) {
		if n.Type == html.ElementNode && n.Data == "a" {
			var href, title string
			for _, attr := range n.Attr {
				switch attr.Key {
				case "href":
					href = attr.Val
				case "title":
					title = attr.Val
				}
			}
			// 提取文本内容作为标题兜底
			if title == "" && n.FirstChild != nil && n.FirstChild.Type == html.TextNode {
				title = strings.TrimSpace(n.FirstChild.Data)
			}

			if href != "" && title != "" && !seen[href] {
				// 补全相对链接
				if strings.HasPrefix(href, "/") {
					href = "https://www.zhipin.com" + href
				}
				if strings.HasPrefix(href, "http") {
					seen[href] = true
					links = append(links, &port.JDLink{
						URL:   href,
						Title: title,
					})
				}
			}
		}
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			f(c)
		}
	}
	f(doc)

	if len(links) == 0 {
		return nil, fmt.Errorf("no job links found on page")
	}
	return links, nil
}

// FetchDetail 获取单个岗位详情
// 复用 HTTPFetcher 的逻辑，通过 HTTP 获取 JD 详情页内容
func (e *WebPageSearchEngine) FetchDetail(url string) (*port.FetchedJD, error) {
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return nil, fmt.Errorf("create request failed: %w", err)
	}
	req.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")

	resp, err := e.client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("fetch detail failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("fetch detail returned status %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("read response failed: %w", err)
	}

	text := extractText(string(body))
	title := extractTitle(string(body))

	return &port.FetchedJD{
		SourceURL:  url,
		Company:    extractCompany(text),
		Position:   title,
		RawContent: text,
	}, nil
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
			t := strings.TrimSpace(n.Data)
			if t != "" {
				text.WriteString(t)
				text.WriteString("\n")
			}
		}
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			f(c)
		}
	}
	f(doc)
	return text.String()
}

// extractTitle 提取 HTML title
func extractTitle(htmlContent string) string {
	doc, err := html.Parse(strings.NewReader(htmlContent))
	if err != nil {
		return ""
	}
	var title string
	var f func(*html.Node)
	f = func(n *html.Node) {
		if n.Type == html.ElementNode && n.Data == "title" && n.FirstChild != nil {
			title = strings.TrimSpace(n.FirstChild.Data)
			return
		}
		for c := n.FirstChild; c != nil; c = c.NextSibling {
			f(c)
		}
	}
	f(doc)
	return title
}

// extractCompany 提取公司名（占位逻辑）
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

// urlEncode 简单 URL 编码（仅编码空格和中文占位，生产环境建议使用 net/url）
func urlEncode(s string) string {
	s = strings.ReplaceAll(s, " ", "+")
	return s
}
