package repository

import (
	"os"
	"path/filepath"

	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// DB 全局数据库实例
var DB *gorm.DB

// InitDB 初始化 SQLite 数据库
// dbPath: 数据库文件路径，空则使用默认路径 ~/.resumedesign/resumedesign.db
func InitDB(dbPath string) error {
	if dbPath == "" {
		home, err := os.UserHomeDir()
		if err != nil {
			return err
		}
		dir := filepath.Join(home, ".resumedesign")
		if err := os.MkdirAll(dir, 0755); err != nil {
			return err
		}
		dbPath = filepath.Join(dir, "resumedesign.db")
	}

	var err error
	DB, err = gorm.Open(sqlite.Open(dbPath), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Warn),
	})
	if err != nil {
		return err
	}

	// 自动迁移
	if err := DB.AutoMigrate(
		&ResumeModel{},
		&SectionModel{},
		&JobDescriptionModel{},
		&RewriteSessionModel{},
		&DiffModel{},
		&ScoringSessionModel{},
		&ScoredJDModel{},
		&DeliverySessionModel{},
		&DeliveryTargetModel{},
	); err != nil {
		return err
	}

	return nil
}

// GetDB 获取数据库实例
func GetDB() *gorm.DB {
	return DB
}
