#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "sme/basic_allocator.h"
#include "sme/mdm/string.h"
#include "sme/mem_domain.h"
#include "sme/mem_space.h"

#define CATCH_AND_EXIT \
    catch (const std::exception& e) { \
        std::cerr << "FAILED: " << __builtin_FUNCTION() << ": " << e.what() << std::endl; \
        std::exit(1); \
    }

namespace {

using Clock = std::chrono::high_resolution_clock;

constexpr unsigned int kIterations = 2'000'000U;
constexpr size_t kMdmBufSize = 256ULL * 1024 * 1024;
struct MdmFixture {
    std::unique_ptr<char[]> buffer{new char[kMdmBufSize]};
    sme::MemorySpace mem_space;
    sme::Pointer<sme::MemoryDomain> domain_ptr;
    sme::MemoryDomain* domain;
    sme::AllocationContext<sme::MemoryDomain> ctx;

    MdmFixture()
        : mem_space(sme::Pointer<void>(buffer.get()), kMdmBufSize),
          domain_ptr(sme::CreateMemoryDomain(
              mem_space, sme::SynchronizationType::kNone)),
          domain(domain_ptr.GetAddress()),
          ctx(*domain) {}
};

struct MdmFixtureNoCtx {
    std::unique_ptr<char[]> buffer{new char[kMdmBufSize]};
    sme::MemorySpace mem_space;
    sme::Pointer<sme::MemoryDomain> domain_ptr;
    sme::MemoryDomain* domain;

    MdmFixtureNoCtx()
        : mem_space(sme::Pointer<void>(buffer.get()), kMdmBufSize),
          domain_ptr(sme::CreateMemoryDomain(
              mem_space, sme::SynchronizationType::kNone)),
          domain(domain_ptr.GetAddress()) {}
};

struct Result {
    const char* name;
    double std_ns;
    double pmr_ns;
    double mdm_ns;
    double mdm_exp_ns;
};

template <typename F>
double measure(F&& f, int n) {
    auto start = Clock::now();
    for (int i = 0; i < n; ++i)
        f();
    auto end = Clock::now();
    return std::chrono::duration<double, std::nano>(end - start).count() / n;
}

template <typename F, typename G>
double measure(F&& op, G&& refill, int n) {
    double total = 0;
    for (int i = 0; i < n; ++i) {
        auto t0 = Clock::now();
        op();
        auto t1 = Clock::now();
        total += std::chrono::duration<double, std::nano>(t1 - t0).count();
        refill();
    }
    return total / n;
}

template <typename F>
double measure_std(F&& op, int n) {
    std::string s;
    double r = measure([&]() { op(s); }, n);
    return r;
}

template <typename F, typename G>
double measure_std(F&& op, G&& refill, int n) {
    std::string s;
    refill(s);
    return measure([&]() { op(s); }, [&]() { refill(s); }, n);
}

template <typename F>
double measure_pmr(F&& op, int n) {
    std::pmr::string s{std::pmr::new_delete_resource()};
    double r = measure([&]() { op(s); }, n);
    return r;
}

template <typename F, typename G>
double measure_pmr(F&& op, G&& refill, int n) {
    std::pmr::string s{std::pmr::new_delete_resource()};
    refill(s);
    return measure([&]() { op(s); }, [&]() { refill(s); }, n);
}

template <typename F>
double measure_sme(F&& op, int n) {
    MdmFixture fix;
    sme::mdm::string s;
    double r = measure([&]() { op(s); }, n);
    return r;
}

template <typename F, typename G>
double measure_sme(F&& op, G&& refill, int n) {
    MdmFixture fix;
    sme::mdm::string s;
    refill(s);
    return measure([&]() { op(s); }, [&]() { refill(s); }, n);
}

template <typename F>
double measure_sme_explicit(F&& op, int n) {
    MdmFixtureNoCtx fix;
    sme::mdm::MemoryDomainAllocator<char> alloc{*fix.domain};
    sme::mdm::string s{alloc};
    double r = measure([&]() { op(s); }, n);
    return r;
}

template <typename F, typename G>
double measure_sme_explicit(F&& op, G&& refill, int n) {
    MdmFixtureNoCtx fix;
    sme::mdm::MemoryDomainAllocator<char> alloc{*fix.domain};
    sme::mdm::string s{alloc};
    refill(s);
    return measure([&]() { op(s); }, [&]() { refill(s); }, n);
}

std::string MakeLongStr() {
    std::string s;
    s.reserve(600);
    for (int i = 0; i < 10; ++i)
        s += "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
             "Sed do eiusmod tempor incididunt ut labore et dolore magna "
             "aliqua. Ut enim ad minim veniam, quis nostrud exercitation "
             "ullamco laboris nisi ut aliquip ex ea commodo consequat. ";
    return s;
}

const char* kShortStr = "Hello, World!";

std::string FmtNs(double ns) {
    std::ostringstream os;
    if (ns < 10.0)
        os << std::fixed << std::setprecision(2) << ns;
    else if (ns < 100.0)
        os << std::fixed << std::setprecision(2) << ns;
    else if (ns < 10000.0)
        os << std::fixed << std::setprecision(1) << ns;
    else
        os << static_cast<int>(ns);
    return os.str();
}

void PrintTable(const std::vector<Result>& results) {
    size_t w_name = 8;
    size_t w_std = 8;
    size_t w_pmr = 8;
    size_t w_mdm = 8;
    size_t w_mdm_exp = 8;
    size_t w_r1 = 7;
    size_t w_r2 = 7;
    size_t w_r3 = 11;
    size_t w_r4 = 11;

    for (const auto& r : results) {
        w_name = std::max(w_name, std::char_traits<char>::length(r.name));
        w_std = std::max(w_std, FmtNs(r.std_ns).size() + 3);
        w_pmr = std::max(w_pmr, FmtNs(r.pmr_ns).size() + 3);
        w_mdm = std::max(w_mdm, FmtNs(r.mdm_ns).size() + 3);
        w_mdm_exp = std::max(w_mdm_exp, FmtNs(r.mdm_exp_ns).size() + 3);
        std::ostringstream rs;
        rs << std::fixed << std::setprecision(2) << (r.mdm_ns / r.std_ns) << 'x';
        w_r1 = std::max(w_r1, rs.str().size());
        rs.str("");
        rs << std::fixed << std::setprecision(2) << (r.mdm_ns / r.pmr_ns) << 'x';
        w_r2 = std::max(w_r2, rs.str().size());
        rs.str("");
        rs << std::fixed << std::setprecision(2) << (r.mdm_exp_ns / r.mdm_ns) << 'x';
        w_r3 = std::max(w_r3, rs.str().size());
        rs.str("");
        rs << std::fixed << std::setprecision(2) << (r.mdm_exp_ns / r.pmr_ns) << 'x';
        w_r4 = std::max(w_r4, rs.str().size());
    }

    auto sep = std::string(w_name + w_std + w_pmr + w_mdm + w_mdm_exp + w_r1 + w_r2 + w_r3 + w_r4 + 27, '-');

    std::cout << "\n# String Performance Benchmark\n\n";
    std::cout << "+-" << sep << "-+\n";
    std::cout << "| " << std::left << std::setw(w_name) << "Operation"
              << " | " << std::right << std::setw(w_std) << "std"
              << " | " << std::right << std::setw(w_pmr) << "pmr"
              << " | " << std::right << std::setw(w_mdm) << "sme"
               << " | " << std::right << std::setw(w_mdm_exp) << "sme_exp"
               << " || " << std::right << std::setw(w_r1) << "sme/std"
              << " | " << std::right << std::setw(w_r2) << "sme/pmr"
              << " | " << std::right << std::setw(w_r3) << "sme_exp/sme"
              << " | " << std::right << std::setw(w_r4) << "sme_exp/pmr"
              << " |\n";
    std::cout << "+-" << sep << "-+\n";

    for (const auto& r : results) {
        double ratio_std = r.mdm_ns / r.std_ns;
        double ratio_pmr = r.mdm_ns / r.pmr_ns;
        double ratio_exp = r.mdm_exp_ns / r.mdm_ns;
        double ratio_exp_pmr = r.mdm_exp_ns / r.pmr_ns;
        std::ostringstream rs1, rs2, rs3, rs4;
        rs1 << std::fixed << std::setprecision(2) << ratio_std << 'x';
        rs2 << std::fixed << std::setprecision(2) << ratio_pmr << 'x';
        rs3 << std::fixed << std::setprecision(2) << ratio_exp << 'x';
        rs4 << std::fixed << std::setprecision(2) << ratio_exp_pmr << 'x';
        std::cout << "| " << std::left << std::setw(w_name) << r.name
                  << " | " << std::right << std::setw(w_std) << (FmtNs(r.std_ns) + " ns")
                  << " | " << std::right << std::setw(w_pmr) << (FmtNs(r.pmr_ns) + " ns")
                  << " | " << std::right << std::setw(w_mdm) << (FmtNs(r.mdm_ns) + " ns")
                   << " | " << std::right << std::setw(w_mdm_exp) << (FmtNs(r.mdm_exp_ns) + " ns")
                   << " || " << std::right << std::setw(w_r1) << rs1.str()
                  << " | " << std::right << std::setw(w_r2) << rs2.str()
                  << " | " << std::right << std::setw(w_r3) << rs3.str()
                  << " | " << std::right << std::setw(w_r4) << rs4.str()
                  << " |\n";
    }

    std::cout << "+-" << sep << "-+\n";
}

// ---- Benchmark functions ----

void MeasureDefaultConstruct(std::vector<Result>& results) try {
    const double std_ns = measure(
        []() { std::string s; }, kIterations);
    const double pmr_ns = measure_pmr(
        [](std::pmr::string&) {}, kIterations);
    const double mdm_ns = measure_sme(
        [](sme::mdm::string&) {}, kIterations);
    const double mdm_exp_ns = measure_sme_explicit(
        [](sme::mdm::string&) {}, kIterations);

    results.push_back({"Default construct", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureShortStringSSO(std::vector<Result>& results) try {
    
    const double std_ns = measure(
        []() { std::string s(kShortStr); }, kIterations);
    const double pmr_ns = measure(
        [&]() { std::pmr::string s{kShortStr, std::pmr::new_delete_resource()}; },
        kIterations);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        mdm_ns = measure(
            []() { sme::mdm::string s(kShortStr); }, kIterations);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        mdm_exp_ns = measure(
            [&]() { sme::mdm::string s(kShortStr, alloc_exp); }, kIterations);
    }

    results.push_back({"C-string short (SSO)", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureLongStringHeap(std::vector<Result>& results) try {
        constexpr int kIter = kIterations / 2;
    const auto long_str = MakeLongStr();

    const double std_ns = measure(
        [&]() { std::string s(long_str); }, kIter);
    const double pmr_ns = measure(
        [&]() { std::pmr::string s{long_str, std::pmr::new_delete_resource()}; },
        kIter);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        mdm_ns = measure(
            [&]() { sme::mdm::string s(long_str); }, kIter);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        mdm_exp_ns = measure(
            [&]() { sme::mdm::string s(long_str, alloc_exp); }, kIter);
    }

    results.push_back({"C-string long (heap)", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureFillShort(std::vector<Result>& results) try {
    constexpr int kSsoFillIter = kIterations;

    auto op = [](auto& s) { s.assign(10, 'X'); };

    const double std_ns = measure_std(op, kSsoFillIter);
    const double pmr_ns = measure_pmr(op, kSsoFillIter);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        sme::mdm::string s;
        mdm_ns = measure([&]() { op(s); }, kSsoFillIter);
    }
    mdm_exp_ns = measure_sme_explicit(op, kSsoFillIter);

    results.push_back({"Fill short (SSO)", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureFillLong(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;

    auto op = [](auto& s) { s.assign(1000, 'X'); };

    const double std_ns = measure_std(op, kIter);
    const double pmr_ns = measure_pmr(op, kIter);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        sme::mdm::string s;
        mdm_ns = measure([&]() { op(s); }, kIter);
    }
    mdm_exp_ns = measure_sme_explicit(op, kIter);

    results.push_back({"Fill long (heap)", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureCopyConstruct(std::vector<Result>& results) try {
        constexpr int kIter = kIterations / 2;
    const auto src = MakeLongStr();

    const double std_ns = measure(
        [&]() { std::string s(src); }, kIter);

    std::pmr::string pmr_src{src, std::pmr::new_delete_resource()};
    const double pmr_ns = measure(
        [&]() { std::pmr::string s(pmr_src); }, kIter);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        sme::mdm::string mdm_src{src.c_str()};
        mdm_ns = measure(
            [&]() { sme::mdm::string s(mdm_src); }, kIter);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        sme::mdm::string mdm_src_exp{src.c_str(), alloc_exp};
        mdm_exp_ns = measure(
            [&]() { sme::mdm::string s(mdm_src_exp); }, kIter);
    }

    results.push_back({"Copy construct", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureMoveConstruct(std::vector<Result>& results) try {
        constexpr int kIter = kIterations / 2;
    auto make_src = []() { return std::string(MakeLongStr()); };

    const double std_ns = measure(
        [&]() { std::string s(make_src()); }, kIter);
    const double pmr_ns = measure(
        [&]() {
            auto src = std::pmr::string{MakeLongStr(), std::pmr::new_delete_resource()};
            std::pmr::string s(std::move(src));
        },
        kIter);

    auto make_mdm_src = []() -> sme::mdm::string {
        return sme::mdm::string{MakeLongStr().c_str()};
    };
    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        mdm_ns = measure(
            [&]() { sme::mdm::string s(make_mdm_src()); }, kIter);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        auto make_mdm_src_exp = [&]() -> sme::mdm::string {
            return sme::mdm::string{MakeLongStr().c_str(), alloc_exp};
        };
        mdm_exp_ns = measure(
            [&]() { sme::mdm::string s(make_mdm_src_exp()); }, kIter);
    }

    results.push_back({"Move construct", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureCopyAssign(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    const auto src = MakeLongStr();

    auto op = [&](auto& s) { s = src; };

    std::string std_dst;
    const double std_ns = measure([&]() { op(std_dst); }, kIter);

    const double pmr_ns = measure_pmr(op, kIter);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        sme::mdm::string mdm_dst;
        mdm_ns = measure([&]() { op(mdm_dst); }, kIter);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        sme::mdm::string mdm_dst_exp{alloc_exp};
        mdm_exp_ns = measure([&]() { op(mdm_dst_exp); }, kIter);
    }

    results.push_back({"Copy assign", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureMoveAssign(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    auto make_src = []() { return std::string(MakeLongStr()); };
    std::string std_dst;

    const double std_ns = measure(
        [&]() { std_dst = make_src(); }, kIter);

    std::pmr::string pmr_src{std::pmr::new_delete_resource()};
    auto pmr_refill = [&](auto&) { pmr_src = MakeLongStr(); };
    auto pmr_op = [&](auto& s) { s = std::move(pmr_src); };
    const double pmr_ns = measure_pmr(pmr_op, pmr_refill, kIter);

    auto make_mdm_src = []() -> sme::mdm::string {
        return sme::mdm::string{MakeLongStr().c_str()};
    };
    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        sme::mdm::string mdm_dst;
        mdm_ns = measure([&]() { mdm_dst = make_mdm_src(); }, kIter);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        auto make_mdm_src_exp = [&]() -> sme::mdm::string {
            return sme::mdm::string{MakeLongStr().c_str(), alloc_exp};
        };
        sme::mdm::string mdm_dst_exp{alloc_exp};
        mdm_exp_ns = measure(
            [&]() { mdm_dst_exp = make_mdm_src_exp(); }, kIter);
    }

    results.push_back({"Move assign", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasurePushBack(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;

    auto refill = [](auto& s) { s.clear(); };
    auto op = [](auto& s) {
        for (int i = 0; i < 100; ++i)
            s.push_back('A');
    };

    const double std_ns = measure_std(op, refill, kIter);
    const double pmr_ns = measure_pmr(op, refill, kIter);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        sme::mdm::string s;
        mdm_ns = measure([&]() { op(s); }, [&]() { refill(s); }, kIter);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        sme::mdm::string s{alloc_exp};
        mdm_exp_ns = measure([&]() { op(s); }, [&]() { refill(s); }, kIter);
    }

    results.push_back({"push_back x100", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureAppend(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    const auto suffix = MakeLongStr();

    auto refill = [](auto& s) { s.clear(); };
    auto op = [&](auto& s) { s.append(suffix.c_str()); };

    const double std_ns = measure_std(op, refill, kIter);
    const double pmr_ns = measure_pmr(op, refill, kIter);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        sme::mdm::string s;
        mdm_ns = measure([&]() { op(s); }, [&]() { refill(s); }, kIter);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        sme::mdm::string s{alloc_exp};
        mdm_exp_ns = measure([&]() { op(s); }, [&]() { refill(s); }, kIter);
    }

    results.push_back({"append", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureOperatorPlusEq(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    const auto suffix = MakeLongStr();

    auto refill = [](auto& s) { s.clear(); };
    auto op = [&](auto& s) { s += suffix.c_str(); };

    const double std_ns = measure_std(op, refill, kIter);
    const double pmr_ns = measure_pmr(op, refill, kIter);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        sme::mdm::string s;
        mdm_ns = measure([&]() { op(s); }, [&]() { refill(s); }, kIter);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        sme::mdm::string s{alloc_exp};
        mdm_exp_ns = measure([&]() { op(s); }, [&]() { refill(s); }, kIter);
    }

    results.push_back({"operator+=", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureInsert(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 8;
    const auto frag_cstr = MakeLongStr();

    auto refill = [&](auto& s) { s.assign(frag_cstr.c_str()); };
    auto op = [&](auto& s) { s.insert(10, frag_cstr.c_str()); };

    const double std_ns = measure_std(op, refill, kIter);
    const double pmr_ns = measure_pmr(op, refill, kIter);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        sme::mdm::string s;
        refill(s);
        mdm_ns = measure([&]() { op(s); }, [&]() { refill(s); }, kIter);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        sme::mdm::string s{alloc_exp};
        refill(s);
        mdm_exp_ns = measure([&]() { op(s); }, [&]() { refill(s); }, kIter);
    }

    results.push_back({"insert", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureErase(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    const auto frag_cstr = MakeLongStr();

    auto refill = [&](auto& s) { s.assign(frag_cstr.c_str()); };
    auto op = [&](auto& s) { s.erase(10, 50); };

    const double std_ns = measure_std(op, refill, kIter);
    const double pmr_ns = measure_pmr(op, refill, kIter);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        sme::mdm::string s;
        refill(s);
        mdm_ns = measure([&]() { op(s); }, [&]() { refill(s); }, kIter);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        sme::mdm::string s{alloc_exp};
        refill(s);
        mdm_exp_ns = measure([&]() { op(s); }, [&]() { refill(s); }, kIter);
    }

    results.push_back({"erase", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureOperatorBracketRead(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 20;
    const auto content = MakeLongStr();

        std::pmr::string content_pmr{content, std::pmr::new_delete_resource()};

    MdmFixture fix;
    sme::mdm::string content_mdm{content};

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
    sme::mdm::string content_exp{content, alloc_exp};

    const double std_ns = measure(
        [&]() {
            [[maybe_unused]] volatile char c;
            for (size_t i = 0; i < content.size(); ++i)
                c = content[i];
        },
        kIter);
    const double pmr_ns = measure(
        [&]() {
            [[maybe_unused]] volatile char c;
            for (size_t i = 0; i < content_pmr.size(); ++i)
                c = content_pmr[i];
        },
        kIter);
    const double mdm_ns = measure(
        [&]() {
            [[maybe_unused]] volatile char c;
            for (size_t i = 0; i < content_mdm.size(); ++i)
                c = content_mdm[i];
        },
        kIter);
    const double mdm_exp_ns = measure(
        [&]() {
            [[maybe_unused]] volatile char c;
            for (size_t i = 0; i < content_exp.size(); ++i)
                c = content_exp[i];
        },
        kIter);

    results.push_back({"operator[] read", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureRangeFor(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 20;
    const auto content = MakeLongStr();

        std::pmr::string content_pmr{content, std::pmr::new_delete_resource()};

    MdmFixture fix;
    sme::mdm::string content_mdm{content};

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
    sme::mdm::string content_exp{content, alloc_exp};

    const double std_ns = measure(
        [&]() {
            [[maybe_unused]] volatile char c;
            for (auto ch : content)
                c = ch;
        },
        kIter);
    const double pmr_ns = measure(
        [&]() {
            [[maybe_unused]] volatile char c;
            for (auto ch : content_pmr)
                c = ch;
        },
        kIter);
    const double mdm_ns = measure(
        [&]() {
            [[maybe_unused]] volatile char c;
            for (auto ch : content_mdm)
                c = ch;
        },
        kIter);
    const double mdm_exp_ns = measure(
        [&]() {
            [[maybe_unused]] volatile char c;
            for (auto ch : content_exp)
                c = ch;
        },
        kIter);

    results.push_back({"Range-for iter.", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureFind(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    const auto content = MakeLongStr();
    const auto sub = content.substr(50, 100);

        std::pmr::string content_pmr{content, std::pmr::new_delete_resource()};

    MdmFixture fix;
    sme::mdm::string content_mdm{content};

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
    sme::mdm::string content_exp{content, alloc_exp};

    const double std_ns = measure(
        [&]() { volatile auto p = content.find(sub); (void)p; },
        kIter);
    const double pmr_ns = measure(
        [&]() { volatile auto p = content_pmr.find(sub); (void)p; },
        kIter);
    const double mdm_ns = measure(
        [&]() { volatile auto p = content_mdm.find(sub); (void)p; },
        kIter);
    const double mdm_exp_ns = measure(
        [&]() { volatile auto p = content_exp.find(sub); (void)p; },
        kIter);

    results.push_back({"find", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureRFind(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    const auto content = MakeLongStr();
    const auto sub = content.substr(50, 100);

        std::pmr::string content_pmr{content, std::pmr::new_delete_resource()};

    MdmFixture fix;
    sme::mdm::string content_mdm{content};

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
    sme::mdm::string content_exp{content, alloc_exp};

    const double std_ns = measure(
        [&]() { volatile auto p = content.rfind(sub); (void)p; },
        kIter);
    const double pmr_ns = measure(
        [&]() { volatile auto p = content_pmr.rfind(sub); (void)p; },
        kIter);
    const double mdm_ns = measure(
        [&]() { volatile auto p = content_mdm.rfind(sub); (void)p; },
        kIter);
    const double mdm_exp_ns = measure(
        [&]() { volatile auto p = content_exp.rfind(sub); (void)p; },
        kIter);

    results.push_back({"rfind", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureOperatorEq(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    const auto content = MakeLongStr();
    const auto b_std = content;

        std::pmr::string a_pmr{content, std::pmr::new_delete_resource()};
    std::pmr::string b_pmr{a_pmr};

    MdmFixture fix;
    sme::mdm::string a_mdm{content};
    sme::mdm::string b_mdm{a_mdm};

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
    sme::mdm::string a_exp{content, alloc_exp};
    sme::mdm::string b_exp{a_exp};

    const double std_ns = measure(
        [&]() { volatile auto eq = (content == b_std); (void)eq; },
        kIter);
    const double pmr_ns = measure(
        [&]() { volatile auto eq = (a_pmr == b_pmr); (void)eq; },
        kIter);
    const double mdm_ns = measure(
        [&]() { volatile auto eq = (a_mdm == b_mdm); (void)eq; },
        kIter);
    const double mdm_exp_ns = measure(
        [&]() { volatile auto eq = (a_exp == b_exp); (void)eq; },
        kIter);

    results.push_back({"operator==", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureCompare(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    const auto content = MakeLongStr();

        std::pmr::string a_pmr{content, std::pmr::new_delete_resource()};
    std::pmr::string b_pmr{a_pmr};

    MdmFixture fix;
    sme::mdm::string a_mdm{content};
    sme::mdm::string b_mdm{a_mdm};

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
    sme::mdm::string a_exp{content, alloc_exp};
    sme::mdm::string b_exp{a_exp};

    const double std_ns = measure(
        [&]() { volatile auto cmp = content.compare(content); (void)cmp; },
        kIter);
    const double pmr_ns = measure(
        [&]() { volatile auto cmp = a_pmr.compare(b_pmr); (void)cmp; },
        kIter);
    const double mdm_ns = measure(
        [&]() { volatile auto cmp = a_mdm.compare(b_mdm); (void)cmp; },
        kIter);
    const double mdm_exp_ns = measure(
        [&]() { volatile auto cmp = a_exp.compare(b_exp); (void)cmp; },
        kIter);

    results.push_back({"compare", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureSize(std::vector<Result>& results) try {
    const auto content = MakeLongStr();

        std::pmr::string content_pmr{content, std::pmr::new_delete_resource()};

    MdmFixture fix;
    sme::mdm::string content_mdm{content};

    MdmFixtureNoCtx fix_exp;
    sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
    sme::mdm::string content_exp{content, alloc_exp};

    const double std_ns = measure(
        [&]() { volatile auto sz = content.size(); (void)sz; },
        kIterations);
    const double pmr_ns = measure(
        [&]() { volatile auto sz = content_pmr.size(); (void)sz; },
        kIterations);
    const double mdm_ns = measure(
        [&]() { volatile auto sz = content_mdm.size(); (void)sz; },
        kIterations);
    const double mdm_exp_ns = measure(
        [&]() { volatile auto sz = content_exp.size(); (void)sz; },
        kIterations);

    results.push_back({"size()", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureReserve(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 2;

    auto op = [](auto& s) { s.reserve(2000); };

    const double std_ns = measure_std(op, kIter);
    const double pmr_ns = measure_pmr(op, kIter);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        sme::mdm::string s;
        mdm_ns = measure([&]() { op(s); }, kIter);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        sme::mdm::string s{alloc_exp};
        mdm_exp_ns = measure([&]() { op(s); }, kIter);
    }

    results.push_back({"reserve", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureResize(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;

    auto op = [](auto& s) {
        s.resize(1000, 'X');
        s.resize(100);
    };

    const double std_ns = measure_std(op, kIter);
    const double pmr_ns = measure_pmr(op, kIter);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        sme::mdm::string s;
        mdm_ns = measure([&]() { op(s); }, kIter);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        sme::mdm::string s{alloc_exp};
        mdm_exp_ns = measure([&]() { op(s); }, kIter);
    }

    results.push_back({"resize", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

void MeasureClear(std::vector<Result>& results) try {
    constexpr int kIter = kIterations / 4;
    const auto src_cstr = MakeLongStr();

    auto refill = [&](auto& s) { s.assign(src_cstr.c_str()); };
    auto op = [](auto& s) { s.clear(); };

    const double std_ns = measure_std(op, refill, kIter);
    const double pmr_ns = measure_pmr(op, refill, kIter);

    double mdm_ns, mdm_exp_ns;
    {
        MdmFixture fix;
        sme::mdm::string s;
        refill(s);
        mdm_ns = measure([&]() { op(s); }, [&]() { refill(s); }, kIter);
    }
    {
        MdmFixtureNoCtx fix_exp;
        sme::mdm::MemoryDomainAllocator<char> alloc_exp{*fix_exp.domain};
        sme::mdm::string s{alloc_exp};
        refill(s);
        mdm_exp_ns = measure([&]() { op(s); }, [&]() { refill(s); }, kIter);
    }

    results.push_back({"clear", std_ns, pmr_ns, mdm_ns, mdm_exp_ns});
} CATCH_AND_EXIT

}  // namespace

int main() {
    std::vector<Result> results;

    MeasureDefaultConstruct(results);
    MeasureShortStringSSO(results);
    MeasureLongStringHeap(results);
    MeasureFillShort(results);
    MeasureFillLong(results);
    MeasureCopyConstruct(results);
    MeasureMoveConstruct(results);
    MeasureCopyAssign(results);
    MeasureMoveAssign(results);
    MeasurePushBack(results);
    MeasureAppend(results);
    MeasureOperatorPlusEq(results);
    MeasureInsert(results);
    MeasureErase(results);
    MeasureOperatorBracketRead(results);
    MeasureRangeFor(results);
    MeasureFind(results);
    MeasureRFind(results);
    MeasureOperatorEq(results);
    MeasureCompare(results);
    MeasureSize(results);
    MeasureReserve(results);
    MeasureResize(results);
    MeasureClear(results);

    PrintTable(results);
    return 0;
}
