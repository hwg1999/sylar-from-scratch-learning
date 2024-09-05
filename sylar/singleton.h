#pragma once

#include <memory>

namespace sylar {
namespace {

template <typename T, typename X, int N>
T &GetInstanceX() {
    static T val;
    return val;
}

template <typename T, typename X, int N>
std::shared_ptr<T> GetInstancePtr() {
    static std::shared_ptr<T> val{new T};
    return val;
}

template <typename T, typename X = void, int N = 0>
class Singleton {
public:
    static T *GetInstance() {
        static T val;
        return &val;
    }
};

template <typename T, typename X = void, int N = 0>
class SingletonPtr {
public:
    static std::shared_ptr<T> GetInstance() {
        static std::shared_ptr<T> val{new T};
        return val;
    }
};

}  // namespace
}  // namespace sylar