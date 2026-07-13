package port

// JDLink 搜索结果中的岗位链接
type JDLink struct {
	URL      string
	Title    string
	Company  string
	Location string
	Salary   string
}

// JDSearchEngine 岗位搜索引擎 Port 接口
// 职责：从招聘网站搜索岗位，解析搜索结果页，获取 JD 详情
type JDSearchEngine interface {
	// Search 根据关键词搜索岗位列表，page 从 1 开始
	Search(keywords []string, page int) ([]*JDLink, error)
	// ParseResultPage 解析搜索结果页 HTML，提取岗位链接列表
	ParseResultPage(htmlContent string) ([]*JDLink, error)
	// FetchDetail 获取单个岗位的详细 JD 内容
	FetchDetail(url string) (*FetchedJD, error)
}
