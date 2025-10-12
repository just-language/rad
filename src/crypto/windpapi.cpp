#include <Windows.h>
#include <rad/crypto/wincrypt.h>
#include <rad/system_error.h>

using namespace RAD_LIB_NAMESPACE;
using namespace crypto;

namespace {
    struct data_blob : public DATA_BLOB {
        data_blob() : DATA_BLOB{0, nullptr} {
            static_assert(sizeof(data_blob) == sizeof(DATA_BLOB),
                          "sizeof data_blob != sizeof DATA_BLOB");
        }

        data_blob(const data_blob&) = delete;

        data_blob& operator=(const data_blob&) = delete;

        data_blob(data_blob&& other) noexcept
            : DATA_BLOB{other.cbData, other.pbData} {
            other.cbData = 0;
            other.pbData = nullptr;
        }

        data_blob& operator=(data_blob&& other) noexcept {
            clear();
            cbData = std::exchange(other.cbData, 0);
            pbData = std::exchange(other.pbData, nullptr);
            return *this;
        }

        ~data_blob() {
            if (pbData) {
                LocalFree(pbData);
            }
        }

        void clear() noexcept {
            if (pbData) {
                LocalFree(pbData);
            }
            pbData = nullptr;
            cbData = 0;
        }
    };

} // namespace

void crypto::protect_data(const_buffer input, const_buffer entropy,
                          protect_data_flags flags, wzstring_view description,
                          dynamic_buffer output) {
    DATA_BLOB in_blob{static_cast<DWORD>(input.size()), (BYTE*)input.data()};
    DATA_BLOB entropy_blob{static_cast<DWORD>(entropy.size()),
                           (BYTE*)entropy.data()};
    data_blob out_blob;
    DATA_BLOB* entropy_blob_ptr = entropy.empty() ? nullptr : &entropy_blob;
    const wchar_t* description_ptr =
        description.empty() ? nullptr : description.data();
    if (!::CryptProtectData(&in_blob, description_ptr, entropy_blob_ptr,
                            nullptr, nullptr, static_cast<DWORD>(flags),
                            &out_blob)) {
        throw std::system_error(GetLastError(), system_category(),
                                "CryptProtectData");
    }
    output.insert(out_blob.pbData, out_blob.cbData);
}

void crypto::unprotect_data(const_buffer input, const_buffer entropy,
                            unprotect_data_flags flags,
                            std::wstring& description, dynamic_buffer output) {
    DATA_BLOB in_blob{static_cast<DWORD>(input.size()), (BYTE*)input.data()};
    DATA_BLOB entropy_blob{static_cast<DWORD>(entropy.size()),
                           (BYTE*)entropy.data()};
    data_blob out_blob;
    DATA_BLOB* entropy_blob_ptr = entropy.empty() ? nullptr : &entropy_blob;
    wchar_t* allocated_description = nullptr;
    auto free_allocated_description = scope_exit([&] {
        if (allocated_description != nullptr) {
            ::LocalFree(allocated_description);
        }
    });
    if (!::CryptUnprotectData(&in_blob, &allocated_description,
                              entropy_blob_ptr, nullptr, nullptr,
                              static_cast<DWORD>(flags), &out_blob)) {
        throw std::system_error(GetLastError(), system_category(),
                                "CryptUnprotectData");
    }
    if (allocated_description != nullptr) {
        description += allocated_description;
    }
    output.insert(out_blob.pbData, out_blob.cbData);
}