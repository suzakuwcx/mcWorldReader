#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <mutex>
#include <thread>
#include <memory>
#include <future>

namespace Pool
{
    static std::once_flag once_f;
    static std::unique_ptr<boost::asio::thread_pool> pool;

    static void init() {
        pool = std::make_unique<boost::asio::thread_pool>(std::thread::hardware_concurrency());
    }

    template <typename F, typename ...Args>
    std::future<typename std::invoke_result<F, Args...>::type> submit(F&& f, Args&& ...args) {
        std::call_once(once_f, init);

        using ret_type = typename std::invoke_result<F, Args...>::type;

        std::shared_ptr<std::packaged_task<ret_type()> > task = std::make_shared<std::packaged_task<ret_type()> >(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<ret_type> result = task->get_future();

        boost::asio::post(*pool, [task]() -> void {(*task)();});
        return result;
    }

    inline void join() {
        pool->join();
    }
};