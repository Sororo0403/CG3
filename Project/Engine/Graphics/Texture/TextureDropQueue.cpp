#include "TextureDropQueue.h"

void TextureDropQueue::Push(const std::string &path) {
    std::lock_guard<std::mutex> lock(mtx_);
    queue_.push(path);
}

bool TextureDropQueue::Pop(std::string &outPath) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (queue_.empty()) {
        return false;
    }

    outPath = queue_.front();
    queue_.pop();
    return true;
}

void TextureDropQueue::Clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::queue<std::string> empty;
    queue_.swap(empty);
}

bool TextureDropQueue::Empty() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return queue_.empty();
}
