#include "test/mem_fault.h"

#include <csignal>
#include <map>
#include <stdexcept>
#include <system_error>

#include <iostream>

namespace sme {

namespace {

using SignalHandlerType = void (*)(int sig_num, siginfo_t* sig_info, void*);

std::map<MemoryFaultCatcher*, MemoryFaultCatcher::AddressRange> G_address_ranges;

struct sigaction G_prev_sig_act {};
SignalHandlerType G_prev_mem_fault_handler{nullptr};

void MemoryFaultHandler(int sig_num, siginfo_t* sig_info, void* ucontext)
{
    for (const auto& range_item : G_address_ranges) {
        const MemoryFaultCatcher::AddressRange& addr_range = range_item.second;

        if (addr_range.begin <= sig_info->si_addr &&
            sig_info->si_addr < addr_range.end) {
            exit(128 + SIGSEGV);
        }
    }

    if (G_prev_mem_fault_handler)
        (*G_prev_mem_fault_handler)(sig_num, sig_info, ucontext);
}

auto GetMemoryFaultSignalAction() -> struct sigaction {
    struct sigaction sig_act {};

    auto res = sigaction(SIGSEGV, nullptr, &sig_act);
    if (res == -1)
        throw std::system_error(errno,
                                std::generic_category(),
                                "Can't get SIGSEGV signal handler info");
    return sig_act;
}

auto SetMemoryFaultSignalAction(const struct sigaction& sig_act) -> struct sigaction {
    struct sigaction prev_sig_act{};

    auto res = sigaction(SIGSEGV, &sig_act, &prev_sig_act);
    if (res == -1)
        throw std::system_error(errno, std::generic_category(),
                                "Can't set SIGSEGV signal handler");

    return prev_sig_act;
}

auto IsMemoryFaultHandler(const struct sigaction& sig_act)
{
    return (((sig_act.sa_flags & SA_SIGINFO) == SA_SIGINFO) &&
            (sig_act.sa_sigaction == MemoryFaultHandler));
}

auto SetMemoryFaultHandler() -> struct sigaction {
    struct sigaction prev_sig_act = GetMemoryFaultSignalAction();

    if (IsMemoryFaultHandler(prev_sig_act))
        return prev_sig_act;

    struct sigaction sig_act {};

    sig_act.sa_sigaction = MemoryFaultHandler;
    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = SA_SIGINFO | SA_RESETHAND;

    return SetMemoryFaultSignalAction(sig_act);
}

}  // namespace

MemoryFaultCatcher::MemoryFaultCatcher(const void* begin_addr, const void* end_addr)
{
    if (begin_addr == nullptr)
        throw std::invalid_argument("Memory begin address is null");
    if (end_addr == nullptr)
        throw std::invalid_argument("Memory end size is null");

    G_address_ranges[this] = AddressRange{.begin = begin_addr, .end = end_addr};

    try {
        auto prev_sig_act = SetMemoryFaultHandler();
        if (!IsMemoryFaultHandler(prev_sig_act) && G_prev_mem_fault_handler == nullptr) {
            G_prev_sig_act = prev_sig_act;
            G_prev_mem_fault_handler = G_prev_sig_act.sa_sigaction;
        }
    } catch(...) {
        G_address_ranges.erase(this);
    }
}

MemoryFaultCatcher::~MemoryFaultCatcher()
{
    try {
        G_address_ranges.erase(this);

        if (!G_address_ranges.empty())
            return;

        auto curr_sig_act = GetMemoryFaultSignalAction();
        if (!IsMemoryFaultHandler(curr_sig_act))
            return;

        SetMemoryFaultSignalAction(G_prev_sig_act);
        G_prev_mem_fault_handler = nullptr;

    } catch (...) {
    }
}

}  // namespace sme
