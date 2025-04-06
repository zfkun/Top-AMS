#pragma once
#include <deque>
#include <semaphore>

namespace mstd {

    struct lock_key {
      private:
        std::binary_semaphore sp{1};

      public:
        void lock() noexcept { sp.acquire(); }
        void unlock() noexcept { sp.release(); }
    };// lock_key

    // 类似unique_lock的基础RAII锁
    struct RAII_lock {
        using key_type = lock_key;// 如果哪天要扩展key类型的话
      private:
        key_type &key;// 互斥体
      public:
        void lock() noexcept { key.lock(); }
        void unlock() noexcept { key.unlock(); }

        explicit RAII_lock(key_type &k) noexcept : key(k) { lock(); }
        ~RAII_lock() noexcept { unlock(); }
    };// RAII_lock



    // 普通带锁通道,用于只有原子布尔的操作的平台和基准测速
    template <typename T>
    struct channel_lock {
      private:
        std::deque<T> data;//@_@标准库队列不会默认留出一个区块,之后重构
        std::counting_semaphore<> Size{0};
        lock_key key;

      public:
        template <typename... V>
        void emplace(V &&...v) {
            {
                RAII_lock lock(key);
                data.emplace_back(std::forward<V>(v)...);
            }
            Size.release(1);
        }// emplace

        T pop() {
            Size.acquire();
            {
                RAII_lock lock(key);
                T temp = std::move(data.front());
                data.pop_front();
                return temp;//@_@其实return可以放在外面
            }
        }// pop

    };// channel_lock

}// mstd