package port

// FileStorage 文件存储 Port 接口
// 职责：管理投递简历文件的存储（保存、删除、读取）
type FileStorage interface {
	// Save 保存文件到指定路径
	Save(path string, content []byte) error
	// Delete 删除指定路径的文件
	Delete(path string) error
	// Exists 检查文件是否存在
	Exists(path string) (bool, error)
	// Read 读取文件内容
	Read(path string) ([]byte, error)
	// FullPath 获取相对于存储根目录的完整路径
	FullPath(path string) string
}
