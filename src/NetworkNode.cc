#include <spdlog/spdlog.h>
#include <iostream>

#include "NetworkNode.h"

NetworkNode::NetworkNode(int id, int io_threads)
    : node_id_(id), context_(io_threads), router_(context_, ZMQ_ROUTER), stop_flag_(false) {
    router_.bind(node_addresses[node_id_]);
    exit_pair_.bind("inproc://exit_" + std::to_string(node_id_));
    for (int peer_id = 1; peer_id <= 5; peer_id++) {
        if (peer_id != node_id_) {
            dealers_.emplace(std::piecewise_construct, std::forward_as_tuple(peer_id),
                             std::forward_as_tuple(context_, ZMQ_DEALER));
            dealers_[peer_id].connect(node_addresses[peer_id]);
        }
    }
    std::cout << "[Node " << node_id_ << "] Listening on " << node_addresses[node_id_] << "...\n";
}

void NetworkNode::AddMessage(int peer_id, int task_id, int operation_id, uint64_t value) {
    std::array<char, MSG_SIZE> msg;
    snprintf(msg.data(), msg.size(), "%d-%d-%d-%" PRIu64, peer_id, task_id, operation_id, value);

    if (!lockfree_queue_.push(msg)) {
        std::cerr << "Queue is full, dropping message!\n";
    } else {
        total_msg_count_.fetch_add(1, std::memory_order_relaxed);
    }

    if (total_msg_count_.load(std::memory_order_relaxed) >= BATCH_SIZE) {
        send_data_cv_.notify_one();
    }
}

void NetworkNode::SendMessages() {
    while (!stop_flag_.load()) {
        {
            std::unique_lock<std::mutex> lock(send_data_mutex_);
            send_data_cv_.wait_for(lock, std::chrono::milliseconds(WAIT_TIME), [&] {
                return stop_flag_.load() || total_msg_count_.load() >= BATCH_SIZE;
            });
        }

        if (total_msg_count_.load(std::memory_order_relaxed) == 0) {
            continue;
        }

        std::array<char, MSG_SIZE> msg;
        std::vector<std::array<char, MSG_SIZE>> batch;
        batch.reserve(BATCH_SIZE);

        while (batch.size() < BATCH_SIZE && lockfree_queue_.pop(msg)) {
            batch.push_back(msg);
        }
        total_msg_count_.fetch_sub(batch.size(), std::memory_order_relaxed);

        std::unordered_map<int, std::string> batched_messages;
        for (const auto& message : batch) {
            std::string_view msg_view(message.data());
            size_t pos1 = msg_view.find('-');
            int peer_id = std::stoi(std::string(msg_view.substr(0, pos1)));
            std::string updated_message =
                std::to_string(node_id_) + std::string(msg_view.substr(pos1));
            batched_messages[peer_id] += updated_message + "|";
        }

        for (auto& [peer_id, full_message] : batched_messages) {
            if (dealers_.count(peer_id)) {
                dealers_[peer_id].send(zmq::message_t(full_message), zmq::send_flags::none);
            } else {
                std::cerr << "Invalid peer_id: " << peer_id << std::endl;
            }
        }
    }
}

uint64_t NetworkNode::Jmp(const uint64_t va, const uint64_t vb, const uint64_t vc) {
    return (va == vb || va == vc) ? va : vb;
}

std::shared_ptr<TaskQueue> NetworkNode::GetOrCreateTaskQueue(int task_id) {
    std::unique_lock<std::shared_mutex> lock(map_mutex_);
    auto& queue = task_queues_[task_id];
    if (!queue) {
        queue = std::make_shared<TaskQueue>();
    }
    return queue;
}

uint64_t NetworkNode::Receive(int task_id, int operation_id, size_t peer_count) {
    std::vector<uint64_t> received;
    received.reserve(peer_count);
    auto queue = GetOrCreateTaskQueue(task_id);

    while (received.size() < peer_count) {
        std::unique_lock<std::mutex> lock(queue->mutex);
        queue->cv.wait(lock, [&]() {
            return stop_flag_.load() ||
                   std::any_of(queue->data.begin(), queue->data.end(),
                               [operation_id](const auto& p) { return p.first == operation_id; });
        });

        if (stop_flag_.load()) {
            throw std::runtime_error("Node is stopping");
        }

        auto it = std::find_if(queue->data.begin(), queue->data.end(),
                               [operation_id](const auto& p) { return p.first == operation_id; });

        if (it != queue->data.end()) {
            size_t to_take = std::min(peer_count - received.size(), it->second.size());
            received.insert(received.end(), it->second.begin(), it->second.begin() + to_take);
            it->second.erase(it->second.begin(), it->second.begin() + to_take);

            if (it->second.empty()) {
                queue->data.erase(it);
            }
        }
    }

    if (peer_count == 1)
        return received[0];
    if (peer_count == 3)
        return Jmp(received[0], received[1], received[2]);
    return 0;
}

void NetworkNode::ReceiveMessages() {
    zmq::pollitem_t items[] = {{static_cast<void*>(router_), 0, ZMQ_POLLIN, 0},
                               {static_cast<void*>(exit_pair_), 0, ZMQ_POLLIN, 0}};

    while (!stop_flag_.load()) {
        zmq::poll(items, 2, std::chrono::milliseconds(0));

        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t sender, message;
            (void)router_.recv(sender, zmq::recv_flags::none);
            (void)router_.recv(message, zmq::recv_flags::none);
            std::string msg_str = message.to_string();

            std::unordered_map<int, std::unordered_map<int, std::vector<uint64_t>>> batch_data;

            size_t pos = 0;
            while (pos < msg_str.size()) {
                size_t next_pos = msg_str.find('|', pos);
                std::string single_msg = msg_str.substr(pos, next_pos - pos);
                pos = (next_pos == std::string::npos) ? msg_str.size() : next_pos + 1;

                int received_task_id, operation_id, sender_id;
                uint64_t value;
                if (sscanf(single_msg.c_str(), "%d-%d-%d-%" PRIu64, &sender_id, &received_task_id,
                           &operation_id, &value) == 4) {
                    batch_data[received_task_id][operation_id].push_back(value);
                }
            }

            for (auto& [task_id, op_map] : batch_data) {
                auto queue = GetOrCreateTaskQueue(task_id);
                std::lock_guard<std::mutex> lock(queue->mutex);
                for (auto& [operation_id, values] : op_map) {
                    auto it = std::find_if(
                        queue->data.begin(), queue->data.end(),
                        [operation_id](const auto& p) { return p.first == operation_id; });
                    if (it != queue->data.end()) {
                        it->second.insert(it->second.end(), values.begin(), values.end());
                    } else {
                        queue->data.emplace_back(operation_id, values);
                    }
                }
                queue->cv.notify_one();
            }
        }
    }
}

void NetworkNode::Stop() {
    stop_flag_.store(true);
    send_data_cv_.notify_one();
    zmq::socket_t exit_sender(context_, ZMQ_PAIR);
    exit_sender.connect("inproc://exit_" + std::to_string(node_id_));
    std::string exit_msg = "exit";
    exit_sender.send(zmq::message_t(exit_msg.data(), exit_msg.size()), zmq::send_flags::none);
}
