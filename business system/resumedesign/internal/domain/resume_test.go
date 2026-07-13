package domain

import (
	"strings"
	"testing"
)

func TestResumeAsPrompt(t *testing.T) {
	resume := &Resume{
		ID:    "test-id",
		Title: "张三的简历",
		Sections: []*Section{
			{
				Type:    SectionPersonal,
				Title:   "个人信息",
				Content: "张三 | 本科",
				Order:   0,
			},
			{
				Type:    SectionWork,
				Title:   "工作经历",
				Content: "某公司 - 后端开发工程师",
				Order:   1,
			},
		},
	}

	prompt := resume.AsPrompt()

	if !strings.Contains(prompt, "原始简历内容") {
		t.Error("AsPrompt should contain header")
	}
	if !strings.Contains(prompt, "个人信息") {
		t.Error("AsPrompt should contain section title")
	}
	if !strings.Contains(prompt, "张三") {
		t.Error("AsPrompt should contain section content")
	}
	if !strings.Contains(prompt, "工作经历") {
		t.Error("AsPrompt should contain all sections")
	}
}

func TestResumeAsPromptEmptySections(t *testing.T) {
	resume := &Resume{
		ID:       "test-id",
		Title:    "空简历",
		Sections: []*Section{},
	}

	prompt := resume.AsPrompt()
	if !strings.Contains(prompt, "原始简历内容") {
		t.Error("AsPrompt should contain header even with empty sections")
	}
}
