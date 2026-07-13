package filestorage

import (
	"os"
	"path/filepath"
)

// LocalFileStorage 本地文件系统存储实现
// 文件根目录: ~/.resumedesign/deliveries/
type LocalFileStorage struct {
	baseDir string
}

func NewLocalFileStorage() *LocalFileStorage {
	home, _ := os.UserHomeDir()
	baseDir := filepath.Join(home, ".resumedesign", "deliveries")
	return &LocalFileStorage{baseDir: baseDir}
}

// NewLocalFileStorageWithDir 使用自定义目录
func NewLocalFileStorageWithDir(dir string) *LocalFileStorage {
	return &LocalFileStorage{baseDir: dir}
}

// Save 保存文件到指定路径（相对于根目录）
func (s *LocalFileStorage) Save(path string, content []byte) error {
	fullPath := filepath.Join(s.baseDir, path)
	dir := filepath.Dir(fullPath)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return err
	}
	return os.WriteFile(fullPath, content, 0644)
}

// Delete 删除指定路径的文件
func (s *LocalFileStorage) Delete(path string) error {
	fullPath := filepath.Join(s.baseDir, path)
	if err := os.Remove(fullPath); err != nil && !os.IsNotExist(err) {
		return err
	}
	// 尝试删除空父目录（清理空文件夹）
	parentDir := filepath.Dir(fullPath)
	os.Remove(parentDir) // 忽略错误（非空时删除失败）
	return nil
}

// Exists 检查文件是否存在
func (s *LocalFileStorage) Exists(path string) (bool, error) {
	fullPath := filepath.Join(s.baseDir, path)
	_, err := os.Stat(fullPath)
	if err == nil {
		return true, nil
	}
	if os.IsNotExist(err) {
		return false, nil
	}
	return false, err
}

// Read 读取文件内容
func (s *LocalFileStorage) Read(path string) ([]byte, error) {
	fullPath := filepath.Join(s.baseDir, path)
	return os.ReadFile(fullPath)
}

// FullPath 获取文件完整路径
func (s *LocalFileStorage) FullPath(path string) string {
	return filepath.Join(s.baseDir, path)
}
