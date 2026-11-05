#include "FS.h"
#include "SD.h"
#include "emu_hal.h"
#include <climits>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
namespace {
namespace pathfs = std::filesystem;
// Resolve through existing symlinks before checking containment in the emulated
// card.
std::string resolve(const char *name) {
  if (!name)
    return {};
  std::error_code ec;
  auto root = pathfs::weakly_canonical(emu::sdRoot(), ec);
  if (ec)
    return {};
  auto relative = pathfs::path(name).relative_path();
  auto candidate = pathfs::weakly_canonical(root / relative, ec);
  if (ec)
    return {};
  auto r = root.begin(), p = candidate.begin();
  for (; r != root.end(); ++r, ++p)
    if (p == candidate.end() || *p != *r)
      return {};
  return candidate.string();
}
} // namespace
namespace fs {
struct FileImpl {
  std::mutex mutex;
  FILE *file = nullptr;
  bool directory = false;
  std::string host, logical, basename;
  std::vector<std::string> entries;
  size_t next = 0;
  ~FileImpl() {
    if (file)
      std::fclose(file);
  }
  void rewind() {
    entries.clear();
    next = 0;
    std::error_code ec;
    for (pathfs::directory_iterator it(host, ec), end; !ec && it != end;
         it.increment(ec))
      entries.push_back(it->path().filename().string());
  }
};
namespace {
File openHost(const char *name, const char *mode) {
  std::string host = resolve(name);
  if (host.empty() || !mode)
    return {};
  std::error_code ec;
  bool dir = pathfs::is_directory(host, ec);
  if (dir && std::strcmp(mode, FILE_READ))
    return {};
  auto impl = std::make_shared<FileImpl>();
  impl->host = host;
  auto root = pathfs::path(emu::sdRoot());
  impl->logical = "/" + pathfs::path(host)
                            .lexically_relative(pathfs::weakly_canonical(root))
                            .generic_string();
  if (impl->logical == "/.")
    impl->logical = "/";
  impl->basename =
      impl->logical == "/" ? "/" : pathfs::path(host).filename().string();
  impl->directory = dir;
  if (dir)
    impl->rewind();
  else {
    impl->file = std::fopen(host.c_str(), mode);
    if (!impl->file)
      return {};
  }
  return File(impl);
}
} // namespace
File FS::open(const char *name, const char *mode, bool) {
  if (!ready())
    return {};
  return openHost(name, mode);
}
bool FS::exists(const char *name) {
  if (!ready())
    return false;
  auto p = resolve(name);
  std::error_code ec;
  return !p.empty() && pathfs::exists(p, ec);
}
bool FS::remove(const char *name) {
  if (!ready())
    return false;
  auto p = resolve(name);
  std::error_code ec;
  if (p.empty() || pathfs::is_directory(p, ec))
    return false;
  return pathfs::remove(p, ec);
}
bool FS::rename(const char *a, const char *b) {
  if (!ready())
    return false;
  auto from = resolve(a), to = resolve(b);
  if (from.empty() || to.empty() || from == resolve("/") || to == resolve("/"))
    return false;
  std::error_code ec;
  pathfs::rename(from, to, ec);
  return !ec;
}
bool FS::mkdir(const char *name) {
  if (!ready())
    return false;
  auto p = resolve(name);
  std::error_code ec;
  if (p.empty())
    return false;
  if (pathfs::is_directory(p, ec))
    return true;
  return pathfs::create_directory(p, ec);
}
bool FS::rmdir(const char *name) {
  if (!ready())
    return false;
  auto p = resolve(name);
  std::error_code ec;
  if (p.empty() || p == resolve("/") || !pathfs::is_directory(p, ec))
    return false;
  return pathfs::remove(p, ec);
}
File::operator bool() const {
  return impl_ && (impl_->directory || impl_->file);
}
size_t File::write(uint8_t b) { return write(&b, 1); }
size_t File::write(const uint8_t *p, size_t n) {
  if (!impl_ || !impl_->file || !p)
    return 0;
  std::lock_guard<std::mutex> g(impl_->mutex);
  return std::fwrite(p, 1, n, impl_->file);
}
int File::read() {
  if (!impl_ || !impl_->file)
    return -1;
  std::lock_guard<std::mutex> g(impl_->mutex);
  return std::fgetc(impl_->file);
}
size_t File::read(uint8_t *p, size_t n) {
  if (!impl_ || !impl_->file || !p)
    return 0;
  std::lock_guard<std::mutex> g(impl_->mutex);
  return std::fread(p, 1, n, impl_->file);
}
int File::available() {
  if (!impl_ || !impl_->file)
    return 0;
  std::lock_guard<std::mutex> g(impl_->mutex);
  struct stat st{};
  if (fstat(fileno(impl_->file), &st))
    return 0;
  auto pos = ftell(impl_->file);
  if (pos < 0)
    return 0;
  return int(
      std::min<long long>(INT_MAX, std::max<long long>(0, st.st_size - pos)));
}
int File::peek() {
  if (!impl_ || !impl_->file)
    return -1;
  std::lock_guard<std::mutex> g(impl_->mutex);
  int c = std::fgetc(impl_->file);
  if (c != EOF)
    std::ungetc(c, impl_->file);
  return c;
}
void File::flush() {
  if (impl_ && impl_->file) {
    std::lock_guard<std::mutex> g(impl_->mutex);
    std::fflush(impl_->file);
  }
}
bool File::seek(uint32_t pos, SeekMode mode) {
  if (!impl_ || !impl_->file || mode > SeekEnd)
    return false;
  std::lock_guard<std::mutex> g(impl_->mutex);
  return std::fseek(impl_->file, pos,
                    mode == SeekSet   ? SEEK_SET
                    : mode == SeekCur ? SEEK_CUR
                                      : SEEK_END) == 0;
}
size_t File::position() const {
  if (!impl_ || !impl_->file)
    return 0;
  std::lock_guard<std::mutex> g(impl_->mutex);
  auto n = std::ftell(impl_->file);
  return n < 0 ? 0 : size_t(n);
}
size_t File::size() const {
  if (!impl_ || !impl_->file)
    return 0;
  std::lock_guard<std::mutex> g(impl_->mutex);
  struct stat st{};
  std::fflush(impl_->file);
  return fstat(fileno(impl_->file), &st) ? 0 : size_t(st.st_size);
}
const char *File::name() const {
  return impl_ ? impl_->basename.c_str() : nullptr;
}
const char *File::path() const {
  return impl_ ? impl_->logical.c_str() : nullptr;
}
bool File::isDirectory() { return impl_ && impl_->directory; }
File File::openNextFile(const char *mode) {
  if (!isDirectory())
    return {};
  std::lock_guard<std::mutex> g(impl_->mutex);
  while (impl_->next < impl_->entries.size()) {
    auto name = impl_->logical + "/" + impl_->entries[impl_->next++];
    auto f = openHost(name.c_str(), mode);
    if (f)
      return f;
  }
  return {};
}
void File::rewindDirectory() {
  if (isDirectory()) {
    std::lock_guard<std::mutex> g(impl_->mutex);
    impl_->rewind();
  }
}
time_t File::getLastWrite() {
  if (!impl_)
    return 0;
  std::lock_guard<std::mutex> g(impl_->mutex);
  struct stat st{};
  if (impl_->file) {
    fflush(impl_->file);
    return fstat(fileno(impl_->file), &st) ? 0 : st.st_mtime;
  }
  return stat(impl_->host.c_str(), &st) ? 0 : st.st_mtime;
}
uint64_t SDFS::usedBytes() {
  if (!ready())
    return 0;
  auto root = emu::sdRoot();
  uint32_t now = millis();
  if (cached_ && cachedRoot_ == root && uint32_t(now - cachedAt_) < 5000)
    return used_;
  used_ = 0;
  std::error_code ec;
  for (pathfs::recursive_directory_iterator
           it(root, pathfs::directory_options::skip_permission_denied, ec),
       end;
       !ec && it != end; it.increment(ec)) {
    if (it->is_symlink(ec))
      continue;
    if (it->is_regular_file(ec)) {
      auto n = it->file_size(ec);
      if (!ec)
        used_ += n;
    }
  }
  cached_ = true;
  cachedAt_ = now;
  cachedRoot_ = root;
  return used_;
}
} // namespace fs
fs::SDFS SD;
