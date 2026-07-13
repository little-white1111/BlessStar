package browser

import (
	"os/exec"
	"runtime"
)

// DefaultOpener OS 默认浏览器打开实现
type DefaultOpener struct{}

func NewDefaultOpener() *DefaultOpener {
	return &DefaultOpener{}
}

// Open 在默认浏览器中打开 URL
func (o *DefaultOpener) Open(url string) error {
	switch runtime.GOOS {
	case "windows":
		return exec.Command("cmd", "/c", "start", url).Start()
	case "darwin":
		return exec.Command("open", url).Start()
	default:
		return exec.Command("xdg-open", url).Start()
	}
}
