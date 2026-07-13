package keyword

// Dict 同义词与类别词典
// 不变量 S5：同义词映射表和类别映射表应可动态配置
// 当前使用内置默认词典，后续可从 JSON 文件加载
type Dict struct {
	Synonyms   map[string][]string // 同义词映射：关键词 → 同义词列表
	Categories map[string][]string // 类别映射：类别名 → 关键词列表
}

// DefaultDict 返回内置默认词典
func DefaultDict() *Dict {
	return &Dict{
		Synonyms: map[string][]string{
			"go":       {"golang", "Go", "GO", "Golang"},
			"golang":   {"go", "Go", "GO"},
			"java":     {"Java", "JAVA", "J2EE"},
			"python":   {"Python", "PYTHON", "py"},
			"rust":     {"Rust", "RUST"},
			"vue":      {"Vue.js", "Vue3", "vuejs"},
			"react":    {"React.js", "ReactJS", "react"},
			"docker":   {"Docker", "container"},
			"kubernetes": {"k8s", "Kubernetes", "K8S"},
			"mysql":    {"MySQL", "MySql", "SQL"},
			"redis":    {"Redis", "redis"},
			"kotlin":   {"Kotlin", "kotlin", "Android"},
			"aws":      {"AWS", "Amazon Web Services", "cloud"},
			"gcp":      {"GCP", "Google Cloud"},
			"azure":    {"Azure", "Microsoft Azure"},
			"微服务":     {"microservice", "微服务架构", "Spring Cloud"},
			"分布式":     {"distributed", "集群"},
			"前端":       {"frontend", "FE", "web前端", "H5"},
			"后端":       {"backend", "BE", "server", "服务端"},
			"全栈":       {"fullstack", "full stack", "全栈工程师"},
		},
		Categories: map[string][]string{
			"后端":       {"Java", "Go", "Python", "Spring", "Spring Boot", "MyBatis", "微服务", "分布式"},
			"前端":       {"Vue", "React", "HTML", "CSS", "JavaScript", "TypeScript", "Webpack", "Node.js"},
			"算法":       {"机器学习", "深度学习", "NLP", "CV", "推荐系统", "数据挖掘", "TensorFlow", "PyTorch"},
			"运维/DevOps": {"Docker", "Kubernetes", "CI/CD", "Jenkins", "Linux", "Shell", "Ansible", "Terraform"},
			"数据":       {"MySQL", "Redis", "MongoDB", "Elasticsearch", "Kafka", "大数据", "Hadoop", "Spark"},
			"移动端":      {"Android", "iOS", "Flutter", "React Native", "Swift", "Kotlin"},
			"测试":       {"自动化测试", "性能测试", "Selenium", "JMeter", "测试开发"},
		},
	}
}

// GetSynonyms 获取指定关键词的同义词列表
func (d *Dict) GetSynonyms(word string) []string {
	lower := toLower(word)
	if syns, ok := d.Synonyms[lower]; ok {
		return syns
	}
	return nil
}

// GetCategoryKeywords 获取指定类别下的所有关键词
func (d *Dict) GetCategoryKeywords(category string) []string {
	if kws, ok := d.Categories[category]; ok {
		return kws
	}
	return nil
}

// AllCategories 返回所有类别名
func (d *Dict) AllCategories() []string {
	cats := make([]string, 0, len(d.Categories))
	for c := range d.Categories {
		cats = append(cats, c)
	}
	return cats
}

func toLower(s string) string {
	b := make([]byte, len(s))
	for i := range s {
		if s[i] >= 'A' && s[i] <= 'Z' {
			b[i] = s[i] + 32
		} else {
			b[i] = s[i]
		}
	}
	return string(b)
}
