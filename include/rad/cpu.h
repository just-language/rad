#pragma once
#include <rad/libbase.h>

#include <array>
#include <bitset>
#include <string>
#include <vector>

#ifdef _WIN32
#include <intrin.h>
#else
#include <cpuid.h>
#endif // _WIN32

namespace RAD_LIB_NAMESPACE {
    class cpu {
        struct instruction_set;

    public:
        static bool is_amd() noexcept {
            return instset.is_amd_;
        }

        static bool is_intel() noexcept {
            return instset.is_intel_;
        }

        static const std::string& vendor() noexcept {
            return instset.vendor_;
        }

        static const std::string& brand() noexcept {
            return instset.brand_;
        }

        static int highest_function_id() noexcept {
            return instset.nids_;
        }

        static int highest_subfunction_id() noexcept {
            return instset.extnids_;
        }

        static bool sse3() noexcept {
            return instset.ecx_1[0];
        }

        static bool pclmulqdq() noexcept {
            return instset.ecx_1[1];
        }

        static bool dtes64() noexcept {
            return instset.ecx_1[2];
        }

        static bool monitor() noexcept {
            return instset.ecx_1[3];
        }

        static bool ds_cpl() noexcept {
            return instset.ecx_1[4];
        }

        static bool vmx() noexcept {
            return instset.ecx_1[5];
        }

        static bool smx() noexcept {
            return instset.ecx_1[6];
        }

        static bool est() noexcept {
            return instset.ecx_1[7];
        }

        static bool tm2() noexcept {
            return instset.ecx_1[8];
        }

        static bool ssse3() noexcept {
            return instset.ecx_1[9];
        }

        static bool cnxt_id() noexcept {
            return instset.ecx_1[10];
        }

        static bool sdbg() noexcept {
            return instset.ecx_1[11];
        }

        static bool fma() noexcept {
            return instset.ecx_1[12];
        }

        static bool cx16() noexcept {
            return instset.ecx_1[13];
        }

        static bool xtpr() noexcept {
            return instset.ecx_1[14];
        }

        static bool pdcm() noexcept {
            return instset.ecx_1[15];
        }

        static bool pcid() noexcept {
            return instset.ecx_1[17];
        }

        static bool dca() noexcept {
            return instset.ecx_1[18];
        }

        static bool sse41() noexcept {
            return instset.ecx_1[19];
        }

        static bool sse42() noexcept {
            return instset.ecx_1[20];
        }

        static bool x2apic() noexcept {
            return instset.ecx_1[21];
        }

        static bool movbe() noexcept {
            return instset.ecx_1[22];
        }

        static bool popcnt() noexcept {
            return instset.ecx_1[23];
        }

        static bool tsc_deadline() noexcept {
            return instset.ecx_1[24];
        }

        static bool aes() noexcept {
            return instset.ecx_1[25];
        }

        static bool xsave() noexcept {
            return instset.ecx_1[26];
        }

        static bool osxsave() noexcept {
            return instset.ecx_1[27];
        }

        static bool avx() noexcept {
            return instset.ecx_1[28];
        }

        static bool f16c() noexcept {
            return instset.ecx_1[29];
        }

        static bool rdrnd() noexcept {
            return instset.ecx_1[30];
        }

        static bool hypervisor() noexcept {
            return instset.ecx_1[31];
        }

        static bool fpu() noexcept {
            return instset.edx_1[0];
        }

        static bool vme() noexcept {
            return instset.edx_1[1];
        }

        static bool de() noexcept {
            return instset.edx_1[2];
        }

        static bool pse() noexcept {
            return instset.edx_1[3];
        }

        static bool tsc() noexcept {
            return instset.edx_1[4];
        }

        static bool msr() noexcept {
            return instset.edx_1[5];
        }

        static bool pae() noexcept {
            return instset.edx_1[6];
        }

        static bool mce() noexcept {
            return instset.edx_1[7];
        }

        static bool cx8() noexcept {
            return instset.edx_1[8];
        }

        static bool apic() noexcept {
            return instset.edx_1[9];
        }

        static bool sep() noexcept {
            return instset.edx_1[11];
        }

        static bool mtrr() noexcept {
            return instset.edx_1[12];
        }

        static bool pge() noexcept {
            return instset.edx_1[13];
        }

        static bool mca() noexcept {
            return instset.edx_1[14];
        }

        static bool cmov() noexcept {
            return instset.edx_1[15];
        }

        static bool pat() noexcept {
            return instset.edx_1[16];
        }

        static bool pse36() noexcept {
            return instset.edx_1[17];
        }

        static bool psn() noexcept {
            return instset.edx_1[18];
        }

        static bool clfsh() noexcept {
            return instset.edx_1[19];
        }

        static bool ds() noexcept {
            return instset.edx_1[21];
        }

        static bool acpi() noexcept {
            return instset.edx_1[22];
        }

        static bool mmx() noexcept {
            return instset.edx_1[23];
        }

        static bool fxsr() noexcept {
            return instset.edx_1[24];
        }

        static bool sse() noexcept {
            return instset.edx_1[25];
        }

        static bool sse2() noexcept {
            return instset.edx_1[26];
        }

        static bool ss() noexcept {
            return instset.edx_1[27];
        }

        static bool htt() noexcept {
            return instset.edx_1[28];
        }

        static bool tm() noexcept {
            return instset.edx_1[29];
        }

        static bool ia64() noexcept {
            return instset.edx_1[30];
        }

        static bool pbe() noexcept {
            return instset.edx_1[31];
        }

        static bool fsgsbase() noexcept {
            return instset.ebx_7[0];
        }

        static bool sgx() noexcept {
            return instset.ebx_7[2];
        }

        static bool bmi1() noexcept {
            return instset.ebx_7[3];
        }

        static bool hle() noexcept {
            return instset.ebx_7[4];
        }

        static bool avx2() noexcept {
            return instset.ebx_7[5];
        }

        static bool smep() noexcept {
            return instset.ebx_7[7];
        }

        static bool bmi2() noexcept {
            return instset.ebx_7[8];
        }

        static bool erms() noexcept {
            return instset.ebx_7[9];
        }

        static bool invpcid() noexcept {
            return instset.ebx_7[10];
        }

        static bool rtm() noexcept {
            return instset.ebx_7[11];
        }

        static bool pqm() noexcept {
            return instset.ebx_7[12];
        }

        static bool mpx() noexcept {
            return instset.ebx_7[14];
        }

        static bool pqe() noexcept {
            return instset.ebx_7[15];
        }

        static bool avx512_f() noexcept {
            return instset.ebx_7[16];
        }

        static bool avx512_dq() noexcept {
            return instset.ebx_7[17];
        }

        static bool rdseed() noexcept {
            return instset.ebx_7[18];
        }

        static bool adx() noexcept {
            return instset.ebx_7[19];
        }

        static bool smap() noexcept {
            return instset.ebx_7[20];
        }

        static bool avx512_ifma() noexcept {
            return instset.ebx_7[21];
        }

        static bool pcommit() noexcept {
            return instset.ebx_7[22];
        }

        static bool clflushopt() noexcept {
            return instset.ebx_7[23];
        }

        static bool clwb() noexcept {
            return instset.ebx_7[24];
        }

        static bool intel_pt() noexcept {
            return instset.ebx_7[25];
        }

        static bool avx512_pf() noexcept {
            return instset.ebx_7[26];
        }

        static bool avx512_er() noexcept {
            return instset.ebx_7[27];
        }

        static bool avx512_cd() noexcept {
            return instset.ebx_7[28];
        }

        static bool sha() noexcept {
            return instset.ebx_7[29];
        }

        static bool avx512_bw() noexcept {
            return instset.ebx_7[30];
        }

        static bool avx512_vl() noexcept {
            return instset.ebx_7[31];
        }

        static bool prefetchwt1() noexcept {
            return instset.ecx_7[0];
        }

        static bool avx512_vbmi() noexcept {
            return instset.ecx_7[1];
        }

        static bool umip() noexcept {
            return instset.ecx_7[2];
        }

        static bool pku() noexcept {
            return instset.ecx_7[3];
        }

        static bool ospke() noexcept {
            return instset.ecx_7[4];
        }

        static bool waitpkg() noexcept {
            return instset.ecx_7[5];
        }

        static bool avx512_vbmi2() noexcept {
            return instset.ecx_7[6];
        }

        static bool cet_ss() noexcept {
            return instset.ecx_7[7];
        }

        static bool gfni() noexcept {
            return instset.ecx_7[8];
        }

        static bool vaes() noexcept {
            return instset.ecx_7[9];
        }

        static bool vpclmulqdq() noexcept {
            return instset.ecx_7[10];
        }

        static bool avx512_vnni() noexcept {
            return instset.ecx_7[11];
        }

        static bool avx512_bitalg() noexcept {
            return instset.ecx_7[12];
        }

        static bool avx512_vpopcntdq() noexcept {
            return instset.ecx_7[14];
        }

        static bool rdpid() noexcept {
            return instset.ecx_7[22];
        }

        static bool cldemote() noexcept {
            return instset.ecx_7[25];
        }

        static bool MOVDIRI() noexcept {
            return instset.ecx_7[27];
        }

        static bool MOVDIR64B() noexcept {
            return instset.ecx_7[28];
        }

        static bool ENQCMD() noexcept {
            return instset.ecx_7[29];
        }

        static bool sgx_lc() noexcept {
            return instset.ecx_7[30];
        }

        static bool pks() noexcept {
            return instset.ecx_7[31];
        }

        static bool lahf_lm(void) {
            return instset.ecx_81[0];
        }
        static bool lzcnt(void) {
            return instset.is_intel_ && instset.ecx_81[5];
        }
        static bool abm(void) {
            return instset.is_amd_ && instset.ecx_81[5];
        }
        static bool sse4a(void) {
            return instset.ecx_81[6];
        }
        static bool xop(void) {
            return instset.ecx_81[11];
        }
        static bool tbm(void) {
            return instset.ecx_81[21];
        }

        static bool syscall(void) {
            return instset.edx_81[11];
        }
        static bool mmxext(void) {
            return instset.edx_81[22];
        }
        static bool rdtscp(void) {
            return instset.edx_81[27];
        }
        static bool amd_3dnowext(void) {
            return instset.edx_81[30];
        }
        static bool amd_3dnow(void) {
            return instset.edx_81[31];
        }

    private:
        struct instruction_set {
            using register_type = std::array<bool, 32>;

            struct cpuid_data {
                struct registers {
                    int eax;
                    int ebx;
                    int ecx;
                    int edx;
                };

                union {
                    std::array<int, 4> i32_data = {};
                    std::array<char, sizeof(int) * 4> i8_data;
                    registers regs;
                };
            };

            struct vendor_string_format {
                union {
                    std::array<int, 3> i32data;
                    std::array<char, 12> i8data;
                };

                vendor_string_format(int ebx, int ecx, int edx) noexcept
                    : i32data{ebx, edx, ecx} {
                }
            };

            struct brand_string_format {
                union {
                    std::array<int, 4> i32data;
                    std::array<char, 16> i8data;
                };

                brand_string_format(int eax, int ebx, int ecx, int edx) noexcept
                    : i32data{eax, ebx, ecx, edx} {
                }
            };

            static cpuid_data invoke_cpuid(int level) {
                cpuid_data data;
#ifdef _WIN32
                __cpuid(data.i32_data.data(), level);
#else
                __cpuid(level, data.regs.eax, data.regs.ebx, data.regs.ecx,
                        data.regs.edx);
#endif // _WIN32
                return data;
            }

            static cpuid_data invoke_cpuidex(int level, int sub) {
                cpuid_data data;
#ifdef _WIN32
                __cpuidex(data.i32_data.data(), level, sub);
#else
                __cpuid_count(level, sub, data.regs.eax, data.regs.ebx,
                              data.regs.ecx, data.regs.edx);
#endif // _WIN32
                return data;
            }

            instruction_set() {
                auto int_to_bit_array = [](int reg, register_type& bits) {
                    for (auto& bit : bits) {
                        bit = static_cast<bool>(reg & 1);
                        reg >>= 1;
                    }
                };

                {
                    cpuid_data data = invoke_cpuid(0);
                    nids_ = data.regs.eax;
                    vendor_string_format vendor_str = {
                        data.regs.ebx, data.regs.ecx, data.regs.edx};
                    vendor_.insert(vendor_.end(), vendor_str.i8data.begin(),
                                   vendor_str.i8data.end());
                }

                if (vendor_ == "GenuineIntel") {
                    is_intel_ = true;
                }
                else if (vendor_ == "AuthenticAMD") {
                    is_amd_ = true;
                }

                if (nids_ >= 1) {
                    cpuid_data data = invoke_cpuidex(1, 0);
                    int_to_bit_array(data.regs.ecx, ecx_1);
                    int_to_bit_array(data.regs.edx, edx_1);
                }

                if (nids_ >= 7) {
                    cpuid_data data = invoke_cpuidex(7, 0);
                    int_to_bit_array(data.regs.ebx, ebx_7);
                    int_to_bit_array(data.regs.ecx, ecx_7);
                    int_to_bit_array(data.regs.edx, edx_7);
                }

                {
                    cpuid_data data = invoke_cpuid(0x80000000);
                    extnids_ = data.regs.eax;
                }

                if (extnids_ >= 0x80000001) {
                    cpuid_data data = invoke_cpuidex(0x80000001, 0);
                    int_to_bit_array(data.regs.ecx, ecx_81);
                    int_to_bit_array(data.regs.edx, edx_81);
                }

                if (extnids_ >= 0x80000004) {
                    brand_.reserve(48);
                    for (int i : range(0x80000002, 0x80000005)) {
                        cpuid_data data = invoke_cpuidex(i, 0);
                        brand_string_format brand_str = {
                            data.regs.eax, data.regs.ebx, data.regs.ecx,
                            data.regs.edx};
                        brand_.insert(brand_.end(), brand_str.i8data.begin(),
                                      brand_str.i8data.end());
                    }
                    if (brand_.back() == '\0') {
                        brand_.pop_back();
                    }
                }
            }

            int nids_ = 0;
            int extnids_ = 0;
            std::string vendor_;
            std::string brand_;
            bool is_intel_ = false;
            bool is_amd_ = false;
            register_type ecx_1 = {false};
            register_type edx_1 = {false};
            register_type ebx_7 = {false};
            register_type ecx_7 = {false};
            register_type edx_7 = {false};
            register_type ecx_81 = {false};
            register_type edx_81 = {false};
        };

        inline static const instruction_set instset;
    };
}; // namespace RAD_LIB_NAMESPACE