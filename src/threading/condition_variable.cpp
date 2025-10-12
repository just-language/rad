#include <rad/threading/condition_variable.h>

#include <vector>

using namespace RAD_LIB_NAMESPACE;

namespace {
    class exit_notifier_t {
        struct holder {
            condition_variable* cv;
            std::unique_lock<mutex> lck;
            holder(condition_variable& cond, std::unique_lock<mutex> lock)
                : cv(&cond), lck(std::move(lock)) {
            }
        };
        std::vector<holder> conds_locks;

    public:
        ~exit_notifier_t() {
            for (auto& cond_lock : conds_locks) {
                cond_lock.lck.unlock();
                cond_lock.cv->notify_all();
            }
        }
        void add_cond_lock(condition_variable& cond,
                           std::unique_lock<mutex> lock) {
            conds_locks.emplace_back(cond, std::move(lock));
        }
    };

    thread_local exit_notifier_t exit_notifier;
}; // namespace

namespace RAD_LIB_NAMESPACE {
    void notify_all_at_thread_exit(condition_variable& cond,
                                   std::unique_lock<mutex> lock) {
        exit_notifier.add_cond_lock(cond, std::move(lock));
    }
} // namespace RAD_LIB_NAMESPACE
