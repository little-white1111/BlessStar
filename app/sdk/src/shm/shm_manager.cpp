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
    backing_fd_(other.backing_fd_), layout_(other.layout_) {
    other.backing_fd_ = nullptr;
    other.layout_ = nullptr;
}

shm_manager& shm_manager::operator=(shm_manager&& other) noexcept {
    if (this != &other) {
        detach();
        name_ = std::move(other.name_);
        backing_fd_ = other.backing_fd_;
        layout_ = other.layout_;
        other.backing_fd_ = nullptr;
        other.layout_ = nullptr;
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

std::unique_ptr<shm_manager> shm_manager::create(const std::string& config_key) {
    auto manager = std::unique_ptr<shm_manager>(new shm_manager(config_key));

#ifdef _WIN32
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        SHM_TOTAL_SIZE,
        manager->name_.c_str()
    );
    if (!hMapFile) set_error("CreateFileMapping failed");

    void* pView = MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        SHM_TOTAL_SIZE
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

    if (ftruncate(fd, SHM_TOTAL_SIZE) == -1) set_error("ftruncate failed");

    void* ptr = mmap(nullptr, SHM_TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) set_error("mmap failed");

    manager->backing_fd_ = reinterpret_cast<void*>(fd);
    manager->layout_ = static_cast<shm_config_layout*>(ptr);
#endif

    shm_config_layout& layout = *manager->layout_;
    layout.version_counter = 0;
    layout.active_buf_idx = 0;
    layout.data_len = 0;
    std::memset(layout.bufA, 0, sizeof(layout.bufA));
    std::memset(layout.bufB, 0, sizeof(layout.bufB));

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

    void* pView = MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        SHM_TOTAL_SIZE
    );
    if (!pView) set_error("MapViewOfFile (attach) failed");

    manager->backing_fd_ = reinterpret_cast<void*>(hMapFile);
    manager->layout_ = static_cast<shm_config_layout*>(pView);
#else
    int fd = shm_open(manager->name_.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) set_error("shm_open (attach) failed");

    void* ptr = mmap(nullptr, SHM_TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) set_error("mmap (attach) failed");

    manager->backing_fd_ = reinterpret_cast<void*>(fd);
    manager->layout_ = static_cast<shm_config_layout*>(ptr);
#endif

    return manager;
}

void shm_manager::detach() {
    if (layout_) {
#ifdef _WIN32
        if (!UnmapViewOfFile(layout_)) set_error("UnmapViewOfFile failed");
        if (!CloseHandle(reinterpret_cast<HANDLE>(backing_fd_))) set_error("CloseHandle failed");
#else
        munmap(layout_, SHM_TOTAL_SIZE);
        close(reinterpret_cast<int>(backing_fd_));
#endif
        layout_ = nullptr;
        backing_fd_ = nullptr;
    }
}

} // namespace shm
} // namespace sdk
} // namespace app
} // namespace bs