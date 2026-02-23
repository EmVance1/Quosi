#pragma once
#include "num.h"
#include <cstdlib>


namespace quosi {

struct ByteVec {
    byte* data = nullptr;
    size_t cap = 0;
    size_t len = 0;

    ByteVec() : data((byte*)malloc(10)), cap(10) {}
    ByteVec(size_t init) : data((byte*)malloc(init)), cap(init) {}
    ByteVec(ByteVec&& other) : data(other.data), cap(other.cap) { other.data = nullptr; }
    ByteVec(const ByteVec&) = delete;
    ~ByteVec() { free(data); }

    void clear() {
        free(data);
        data = (byte*)malloc(4);
        cap = 4;
        len = 0;
    }
    byte* leak() {
        byte* tmp = data;
        data = nullptr;
        cap = 0;
        len = 0;
        return tmp;
    }
    size_t size() const { return len; }

    template<typename T>
    bool push(const T& v) {
        if (len + sizeof(T) > cap) {
            void* tmp = realloc(data, cap * 2);
            if (!tmp) return false;
            data = (byte*)tmp;
            cap = cap * 2;
        }
        memcpy(data + len, &v, sizeof(T));
        len += sizeof(T);
        return true;
    }
    template<>
    bool push(const byte& v) {
        if (len + 1 > cap) {
            void* tmp = realloc(data, cap * 2);
            if (!tmp) return false;
            data = (byte*)tmp;
            cap = cap * 2;
        }
        data[len++] = v;
        return true;
    }
    const byte& last() const { return data[len-1]; }
    byte& last() { return data[len-1]; }
};

}

