#include "bs/app/sdk/shm/shm_manager.h"

#include <cstring>
#include <stdexcept>
#include <system_error>
#include <cerrno>

namespace bs {
namespace app {
namespace sdk {
namespace shm {

shm_manager::shm_manager(const std::string& config_key) : name_("/bs_config_" + config_key) {}

shm_manager::~shm_manager() {
    detach();
}

shm_manager::shm_manager(shm_manager&& other) noexcept : name_(std::move(other.name_)),
    backing_fd_(other.backing_fd_), layout_(other.layout_), total_size_(other.total_size_) {
    other.backing_fd_ = nullptr;
    other.layout_ = nullptr;
    other.total_size_ = 0;
}

shm_manager& shm_manager::operator=(shm_manager&& other) noexcept {
    if (this != &other) {
        detach();
        name_ = std::move(other.name_);
        backing_fd_ = other.backing_fd_;
        layout_ = other.layout_;
        total_size_ = other.total_size_;
        other.backing_fd_ = nullptr;
        other.layout_ = nullptr;
        other.total_size_ = 0;
    }
    return *this;
}

static void set_error(const char* msg) {
#if defined(_WIN32)
    throw std::runtime_error(std::string(msg) + ": " + std::to_string(GetLastError()));
#else
    throw std::runtime_error(std::string(msg) + ": " + std::string(strerror(errno)));
#endif
}

std::unique_ptr<shm_manager> shm_manager::create(const std::string& config_key,
                                                  size_t schema_capacity) {
    auto manager = std::unique_ptr<shm_manager>(new shm_manager(config_key));
    size_t total = shm_total_size(schema_capacity);
    manager->total_size_ = total;

#ifdef _WIN32
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(total),
        manager->name_.c_str()
    );
    if (!hMapFile) set_error("CreateFileMapping failed");

    void* pView = MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        total
    );
    if (!pView) set_error("MapViewOfFile failed");

    manager->backing_fd_ = reinterpret_cast<void*>(hMapFile);
    manager->layout_ = static_cast<shm_config_layout*>(pView);
#else
    int fd = shm_open(manager->name_.c_str(), O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        if (errno == EEXIST) {
            fd = shm_open(manager->name_.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
            if (fd == -1) set_error("shm_open failed (attach)");
        } else set_error("shm_open failed (create)");
    }

    if (ftruncate(fd, total) == -1) set_error("ftruncate failed");

    void* ptr = mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) set_error("mmap failed");

    manager->backing_fd_ = reinterpret_cast<void*>(fd);
    manager->layout_ = static_cast<shm_config_layout*>(ptr);
#endif

    shm_config_layout& layout = *manager->layout_;
    std::memset(&layout, 0, total);
    layout.version_counter = 0;
    layout.active_buf_idx = 0;
    layout.data_len = 0;
    layout.schema_json_len = 0;
    layout.schema_json_offset = static_cast<uint32_t>(schema_area_offset());
    layout.schema_capacity = static_cast<uint32_t>(schema_capacity);
    /* P1 ⑤：哈希索引区紧随 JSON 兼容区之后 */
    layout.hash_table_offset = layout.schema_json_offset;
    layout.field_data_offset = layout.hash_table_offset;  /* 实际由序列化函数填充 */
    layout.page_table_offset = 0;
    layout.page_count = 0;
    layout.trie_root_offset = 0;

    return manager;
}

std::unique_ptr<shm_manager> shm_manager::attach(const std::string& config_key) {
    auto manager = std::unique_ptr<shm_manager>(new shm_manager(config_key));

#ifdef _WIN32
    HANDLE hMapFile = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        manager->name_.c_str()
    );
    if (!hMapFile) set_error("OpenFileMapping failed");

    /* 先 mapping 一个最小的 size 读取 schema_capacity */
    void* pView = MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        SHM_HEAD_SIZE + 2 * SHM_BUF_SIZE + sizeof(uint32_t)  /* 至少能读到 schema_capacity */
    );
    if (!pView) set_error("MapViewOfFile (head) failed");

    shm_config_layout* partial = static_cast<shm_config_layout*>(pView);
    size_t cap = partial->schema_capacity > 0
        ? partial->schema_capacity
        : DEFAULT_SCHEMA_CAPACITY;
    size_t total = shm_total_size(cap);

    UnmapViewOfFile(partial);

    pView = MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        total
    );
    if (!pView) set_error("MapViewOfFile (full) failed");

    manager->backing_fd_ = reinterpret_cast<void*>(hMapFile);
    manager->layout_ = static_cast<shm_config_layout*>(pView);
    manager->total_size_ = total;
#else
    int fd = shm_open(manager->name_.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) set_error("shm_open (attach) failed");

    /* 先映射头部读 schema_capacity */
    void* ptr = mmap(nullptr, SHM_HEAD_SIZE + 2 * SHM_BUF_SIZE + sizeof(uint32_t),
                     PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) set_error("mmap (head) failed");

    shm_config_layout* partial = static_cast<shm_config_layout*>(ptr);
    size_t cap = partial->schema_capacity > 0
        ? partial->schema_capacity
        : DEFAULT_SCHEMA_CAPACITY;
    size_t total = shm_total_size(cap);

    munmap(ptr, SHM_HEAD_SIZE + 2 * SHM_BUF_SIZE + sizeof(uint32_t));

    ptr = mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) set_error("mmap (full) failed");

    manager->backing_fd_ = reinterpret_cast<void*>(fd);
    manager->layout_ = static_cast<shm_config_layout*>(ptr);
    manager->total_size_ = total;
#endif

    return manager;
}

void shm_manager::detach() {
    if (layout_) {
#ifdef _WIN32
        if (!UnmapViewOfFile(layout_)) set_error("UnmapViewOfFile failed");
        if (!CloseHandle(reinterpret_cast<HANDLE>(backing_fd_))) set_error("CloseHandle failed");
#else
        munmap(layout_, total_size_);
        close(reinterpret_cast<int>(backing_fd_));
#endif
        layout_ = nullptr;
        backing_fd_ = nullptr;
        total_size_ = 0;
    }
}

} // namespace shm
} // namespace sdk
} // namespace app
} // namespace bs
