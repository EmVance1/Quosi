#pragma once
#include <cstdint>


namespace quosi {

class VirtualMachine {
public:
    struct Prop { const char* s = nullptr; uint8_t i = 0; };
    enum class UpCall { NONE = 0, Line, Pick, Event, Exit, Abort };
    using Context = uint64_t*(*)(uint32_t key, bool strict);

private:
    inline static constexpr size_t MAX_PROPS = 16;
    inline static constexpr size_t MAX_EXPRS = 128;
    inline static constexpr uint32_t V_START = 0;
    inline static constexpr uint32_t V_END   = UINT32_MAX;
    inline static constexpr uint32_t V_ABORT = UINT32_MAX-1;
    struct IProp { uint32_t s = 0; uint8_t i = 0; };

    uint64_t expr[MAX_EXPRS] = {};
    IProp text[MAX_PROPS] = {};
    uint32_t PC = 0, SP = 0;
    uint32_t TH = 0, TT = 0;
    uint32_t A  = 0, B  = 0;
    const uint8_t* code;

private:
    void enq_iprop(const IProp& prop);
    IProp deq_iprop();
    UpCall step(Context ctx);

public:
    VirtualMachine(const uint8_t* _code) : code(_code) {}
    void reset(const uint8_t* _code);

    const char* line() const { return (const char*)(code + B); }
    uint32_t id() const { return A; }
    uint32_t nq() const { return B; }

    void     push(uint64_t val) { expr[SP++] = val; }
    uint64_t pop() { return expr[--SP]; }
    uint64_t top() const { return expr[SP-1]; }
    Prop deq_text();

    UpCall exec(Context ctx);
};

}
