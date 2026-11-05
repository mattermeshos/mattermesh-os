#include "Arduino.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
namespace {

struct Task {
  std::atomic<bool> cancel{false}, done{false};
  std::thread thread;
};
thread_local Task *current = nullptr;
// Retain task handles until shutdown, avoiding use-after-free of completed
// handles.
struct Tasks {
  std::mutex mutex;
  std::atomic<bool> stopping{false};
  std::vector<std::unique_ptr<Task>> all;
  void stop() {
    std::vector<Task *> list;
    {
      std::lock_guard<std::mutex> lock(mutex);
      stopping = true;
      for (auto &task : all) {
        task->cancel = true;
        list.push_back(task.get());
      }
    }
    for (auto task : list)
      if (task->thread.joinable())
        task->thread.join();
  }
  ~Tasks() { stop(); }
} tasks;
struct Queue {
  std::mutex mutex;
  std::condition_variable readable, writable;
  size_t depth, item;
  std::deque<std::vector<uint8_t>> data;
  Queue(size_t d, size_t i) : depth(d), item(i) {}
};
struct Semaphore {
  std::mutex mutex;
  std::condition_variable cv;
  bool available, owned;
  std::thread::id owner;
  Semaphore(bool m) : available(m), owned(m) {}
};
void cancellation() {
  if (current && current->cancel)
    throw emu::TaskExit{};
}
template <class Pred>
bool waitFor(std::condition_variable &cv, std::unique_lock<std::mutex> &lock,
             TickType_t ticks, Pred ready) {
  auto end =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(ticks);
  while (!ready()) {
    cancellation();
    if (!ticks)
      return false;
    auto now = std::chrono::steady_clock::now();
    if (ticks != portMAX_DELAY && now >= end)
      return false;
    cv.wait_for(
        lock,
        ticks == portMAX_DELAY
            ? std::chrono::milliseconds(5)
            : std::min(std::chrono::milliseconds(5),
                       std::chrono::duration_cast<std::chrono::milliseconds>(
                           end - now)));
  }
  cancellation();
  return true;
}
} // namespace
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *, uint32_t,
                                   void *arg, UBaseType_t, TaskHandle_t *out,
                                   BaseType_t) {
  if (out)
    *out = nullptr;
  if (!fn)
    return pdFAIL;
  try {
    auto task = std::make_unique<Task>();
    auto p = task.get();
    std::lock_guard<std::mutex> g(tasks.mutex);
    if (tasks.stopping)
      return pdFAIL;
    // Reclaim completed native stacks while keeping opaque handles stable.
    for (auto &finished : tasks.all)
      if (finished->done && finished->thread.joinable())
        finished->thread.join();
    tasks.all.push_back(std::move(task));
    try {
      p->thread = std::thread([p, fn, arg] {
        current = p;
        try {
          fn(arg);
        } catch (const emu::TaskExit &) {
        }
        p->done = true;
        current = nullptr;
      });
    } catch (...) {
      tasks.all.pop_back();
      throw;
    }
    if (out)
      *out = p;
    return pdPASS;
  } catch (...) {
    return pdFAIL;
  }
}
BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack,
                       void *arg, UBaseType_t priority, TaskHandle_t *handle) {
  return xTaskCreatePinnedToCore(fn, name, stack, arg, priority, handle,
                                 tskNO_AFFINITY);
}
void vTaskDelete(TaskHandle_t handle) {
  if (!handle || handle == current) {
    throw emu::TaskExit{};
  }
  auto task = static_cast<Task *>(handle);
  task->cancel = true;
  if (task->thread.joinable())
    task->thread.join();
}
void vTaskDelay(TickType_t ticks) {
  cancellation();
  if (!current) {
    delay(ticks);
    return;
  }
  auto end =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(ticks);
  do {
    cancellation();
    auto left = end - std::chrono::steady_clock::now();
    if (left <= decltype(left)::zero())
      break;
    std::this_thread::sleep_for(
        std::min(left, std::chrono::duration_cast<decltype(left)>(
                           std::chrono::milliseconds(5))));
  } while (true);
}
UBaseType_t xTaskGetStackHighWaterMark(TaskHandle_t) { return 4096; }
TickType_t xTaskGetTickCount() { return millis(); }
TaskHandle_t xTaskGetCurrentTaskHandle() { return current; }
QueueHandle_t xQueueCreate(UBaseType_t depth, UBaseType_t item) {
  if (!depth || !item)
    return nullptr;
  try {
    return new Queue(depth, item);
  } catch (...) {
    return nullptr;
  }
}
BaseType_t xQueueSendToBack(QueueHandle_t handle, const void *p,
                            TickType_t ticks) {
  if (!handle || !p)
    return pdFALSE;
  auto &q = *static_cast<Queue *>(handle);
  std::unique_lock<std::mutex> lock(q.mutex);
  if (!waitFor(q.writable, lock, ticks,
               [&] { return q.data.size() < q.depth; }))
    return pdFALSE;
  auto bytes = static_cast<const uint8_t *>(p);
  q.data.emplace_back(bytes, bytes + q.item);
  q.readable.notify_one();
  return pdTRUE;
}
BaseType_t xQueueSend(QueueHandle_t h, const void *p, TickType_t t) {
  return xQueueSendToBack(h, p, t);
}
BaseType_t xQueueReceive(QueueHandle_t handle, void *p, TickType_t ticks) {
  if (!handle || !p)
    return pdFALSE;
  auto &q = *static_cast<Queue *>(handle);
  std::unique_lock<std::mutex> lock(q.mutex);
  if (!waitFor(q.readable, lock, ticks, [&] { return !q.data.empty(); }))
    return pdFALSE;
  std::memcpy(p, q.data.front().data(), q.item);
  q.data.pop_front();
  q.writable.notify_one();
  return pdTRUE;
}
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t h) {
  if (!h)
    return 0;
  auto &q = *static_cast<Queue *>(h);
  std::lock_guard<std::mutex> g(q.mutex);
  return q.data.size();
}
void vQueueDelete(QueueHandle_t h) { delete static_cast<Queue *>(h); }
SemaphoreHandle_t xSemaphoreCreateMutex() { return new Semaphore(true); }
SemaphoreHandle_t xSemaphoreCreateBinary() { return new Semaphore(false); }
BaseType_t xSemaphoreTake(SemaphoreHandle_t h, TickType_t ticks) {
  if (!h)
    return pdFALSE;
  auto &s = *static_cast<Semaphore *>(h);
  std::unique_lock<std::mutex> lock(s.mutex);
  if (!waitFor(s.cv, lock, ticks, [&] { return s.available; }))
    return pdFALSE;
  s.available = false;
  s.owner = std::this_thread::get_id();
  return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t h) {
  if (!h)
    return pdFALSE;
  auto &s = *static_cast<Semaphore *>(h);
  std::lock_guard<std::mutex> g(s.mutex);
  if (s.available || (s.owned && s.owner != std::this_thread::get_id()))
    return pdFALSE;
  s.available = true;
  s.owner = {};
  s.cv.notify_one();
  return pdTRUE;
}
void vSemaphoreDelete(SemaphoreHandle_t h) {
  delete static_cast<Semaphore *>(h);
}

namespace emu {
void stopTasks() { tasks.stop(); }
} // namespace emu
