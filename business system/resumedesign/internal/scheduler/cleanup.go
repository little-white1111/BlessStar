package scheduler

import (
	"context"
	"log"
	"time"

	"resumedesign/internal/port"
	"resumedesign/internal/repository"
)

// CleanupTask 定时清理过期投递记录
// 不变量 D2：过期必删
// 不变量 D3：文件与记录共存亡
type CleanupTask struct {
	deliveryRepo *repository.DeliveryRepo
	fileStorage  port.FileStorage
	interval     time.Duration
}

func NewCleanupTask(deliveryRepo *repository.DeliveryRepo, fileStorage port.FileStorage, interval time.Duration) *CleanupTask {
	if interval <= 0 {
		interval = 1 * time.Hour // 默认每小时清理一次
	}
	return &CleanupTask{
		deliveryRepo: deliveryRepo,
		fileStorage:  fileStorage,
		interval:     interval,
	}
}

// Start 启动定时清理协程
func (t *CleanupTask) Start(ctx context.Context) {
	ticker := time.NewTicker(t.interval)
	defer ticker.Stop()

	log.Printf("[CleanupTask] started, interval=%v", t.interval)

	// 启动时立即执行一次
	t.clean()

	for {
		select {
		case <-ticker.C:
			t.clean()
		case <-ctx.Done():
			log.Printf("[CleanupTask] stopped: %v", ctx.Err())
			return
		}
	}
}

// clean 执行清理：查找过期记录 → 删除文件 → 删除数据库记录
// D2：超过 TTL 的投递记录和文件必须被清理
// D3：有记录必有简历文件；删记录必删文件
func (t *CleanupTask) clean() {
	log.Printf("[CleanupTask] running cleanup...")

	expired, err := t.deliveryRepo.FindExpiredSessions()
	if err != nil {
		log.Printf("[CleanupTask] find expired sessions failed: %v", err)
		return
	}

	if len(expired) == 0 {
		log.Printf("[CleanupTask] no expired sessions found")
		return
	}

	for _, session := range expired {
		// 先删除文件（D3：删记录必删文件）
		for _, target := range session.Targets {
			if target.ExportedFilePath != "" {
				if err := t.fileStorage.Delete(target.ExportedFilePath); err != nil {
					log.Printf("[CleanupTask] delete file failed: session=%s target=%s err=%v",
						session.ID, target.ID, err)
				}
			}
		}

		// 再删除数据库记录（D3：文件与记录共存亡）
		if err := t.deliveryRepo.DeleteSession(session.ID); err != nil {
			log.Printf("[CleanupTask] delete session failed: %s err=%v", session.ID, err)
			continue
		}

		log.Printf("[CleanupTask] cleaned session: %s (targets: %d)", session.ID, len(session.Targets))
	}
}
