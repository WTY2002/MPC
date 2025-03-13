#ifndef NETWORKNODE_H
#define NETWORKNODE_H

#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <chrono>
#include <cinttypes>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <zmq.hpp>

constexpr size_t MSG_SIZE = 100;
constexpr int BATCH_SIZE = 50;
constexpr int WAIT_TIME = 10;

inline static const std::unordered_map<int, std::string> default_node_addresses = {
    {1, "tcp://127.0.0.1:5551"}, {2, "tcp://127.0.0.1:5552"}, {3, "tcp://127.0.0.1:5553"},
    {4, "tcp://127.0.0.1:5554"}, {5, "tcp://127.0.0.1:5555"},
};

struct TaskContext {
    int task_id;
    int operation_id;
};

struct TaskQueue {
    std::deque<std::pair<int, std::vector<uint64_t>>> data;
    std::condition_variable cv;
    std::mutex mutex;
};

class NetworkNode {
  public:
    NetworkNode(
        int id, int io_threads,
        const std::unordered_map<int, std::string>& node_addresses = default_node_addresses);

    void AddMessage(int peer_id, int task_id, int operation_id, uint64_t value);

    void SendMessages();

    uint64_t Receive(int task_id, int operation_id, size_t peer_count);

    void ReceiveMessages();

    void Stop();

    int ID() const {
        return node_id_;
    }

  private:
    static uint64_t Jmp(uint64_t va, uint64_t vb, uint64_t vc);

    std::shared_ptr<TaskQueue> GetOrCreateTaskQueue(int task_id);

    int node_id_;
    zmq::context_t context_;
    zmq::socket_t router_;
    std::atomic<bool> stop_flag_;
    zmq::socket_t exit_pair_{context_, ZMQ_PAIR};
    std::unordered_map<int, zmq::socket_t> dealers_;

    std::mutex send_data_mutex_;
    std::condition_variable send_data_cv_;
    std::atomic<size_t> total_msg_count_{0};
    boost::lockfree::queue<std::array<char, MSG_SIZE>> lockfree_queue_{2000};

    mutable std::shared_mutex map_mutex_;
    std::unordered_map<int, std::shared_ptr<TaskQueue>> task_queues_;
};

#endif  // NETWORKNODE_H
