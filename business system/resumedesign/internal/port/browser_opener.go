package port

// BrowserOpener 浏览器打开器 Port 接口
// 职责：在用户默认浏览器中打开指定 URL
type BrowserOpener interface {
	// Open 在默认浏览器中打开 URL
	Open(url string) error
}
