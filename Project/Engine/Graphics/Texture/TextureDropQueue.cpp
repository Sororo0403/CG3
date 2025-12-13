#include "TextureDropQueue.h"

void TextureDropQueue::Push(const std::string &path) {
    queue_.push(path);
}

bool TextureDropQueue::Pop(std::string &outPath) {
    if (queue_.empty()) {
        return false;
    }

    outPath = queue_.front();
    queue_.pop();
    return true;
}

void TextureDropQueue::Clear() {
    while (!queue_.empty()) {
        queue_.pop();
    }
}
