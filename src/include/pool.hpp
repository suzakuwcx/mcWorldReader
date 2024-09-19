#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <queue>
#include <mutex>
#include <thread>
#include <memory>
#include <future>
#include <vector>

namespace Pool
{
    static std::once_flag once_f;
    static std::unique_ptr<std::vector<std::thread> > pool;
    static std::shared_ptr<boost::interprocess::interprocess_semaphore> sem;
    static std::shared_ptr<std::mutex> lock;
    static std::shared_ptr<std::queue<std::function<void()> > > tasks;

    static void loop_main(std::shared_ptr<std::queue<std::function<void()> > > tasks, std::shared_ptr<std::mutex> lock, std::shared_ptr<boost::interprocess::interprocess_semaphore> sem) {
        while (true) {
            sem->wait();

            lock->lock();
            
            std::function<void()> func = tasks->front();
            tasks->pop();

            lock->unlock();
            func();
        }
    }

    static void init() {
        sem = std::make_shared<boost::interprocess::interprocess_semaphore> (0);
        lock = std::make_shared<std::mutex> ();
        tasks = std::make_shared<std::queue<std::function<void()> > >();

        pool = std::make_unique<std::vector<std::thread> >();

        for (int i = 0; i < std::thread::hardware_concurrency(); ++i) {
            pool->emplace_back(loop_main, tasks, lock, sem);
            pool->at(i).detach();
        }        
    }

    template <typename F, typename ...Args>
    std::future<typename std::invoke_result<F, Args...>::type> submit(F&& f, Args&& ...args) {
        std::call_once(once_f, init);

        using ret_type = typename std::invoke_result<F, Args...>::type;

        std::shared_ptr<std::packaged_task<ret_type()> > task = std::make_shared<std::packaged_task<ret_type()> >(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<ret_type> result = task->get_future();

        tasks->emplace([task]() -> void {(*task)();});
        sem->post();
        return result;
    }
};