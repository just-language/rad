#include <rad/crypto/aes.h>
#include <rad/crypto/modes/cbc.h>
#include <rad/crypto/modes/ctr.h>
#include <rad/crypto/modes/ecb.h>
#include <rad/crypto/modes/gcm.h>
#include <rad/string.h>

#include <iostream>
#include <sstream>

using namespace RAD_LIB_NAMESPACE;
using namespace crypto;

template <class Range1, class Rnage2>
inline bool equal_ranges(const Range1& r1, const Rnage2& r2) {
    return std::size(r1) == std::size(r2) &&
           std::equal(std::begin(r1), std::end(r1), std::begin(r2));
}

inline void print_hex_vector(const std::vector<uint8_t>& v) {
    std::cout << "{";
    if (!v.empty()) {
        std::cout << " ";
        auto it = v.begin();
        auto pre_last = std::prev(v.end());
        while (it != pre_last) {
            std::cout << std::hex << (uint32_t)*it << ", ";
            ++it;
        }
        std::cout << (uint32_t)*pre_last << " ";
    }
    std::cout << "}";
}

std::vector<uint8_t> h_to_b(std::string_view hexstr) {
    std::vector<uint8_t> result;

    while (hexstr.size() >= 2) {
        result.push_back(to_uint8(hexstr.substr(0, 2), 16));
        hexstr.remove_prefix(2);
    }

    if (!hexstr.empty()) {
        char str[] = {'0', hexstr[0]};
        result.push_back(to_uint8(std::string_view(str, 2), 16));
    }

    return result;
}

struct ecb_aes_test_entry {
    int mode;
    std::vector<uint8_t> key;
    std::vector<uint8_t> plain;
    std::vector<uint8_t> cipher;
};

template <class AesMode>
void TestECBAes(std::string_view test_name, int testi,
                const ecb_aes_test_entry& testv) {
    using aes_mode = AesMode;
    using encrypter_type = ecb_mode<aes_mode>;
    using decrypter_type = ecb_mode<aes_mode>;

    aes_mode aes_key;
    aes_key.set_key(buffer(testv.key));

    std::vector<uint8_t> data_with_padding;
    std::span<uint8_t> data;

    {
        encrypter_type encrypter(aes_key);
        encrypter.encrypt(buffer(testv.plain),
                          dynamic_buffer(data_with_padding));
        data = {data_with_padding.data(),
                data_with_padding.data() + testv.plain.size()};
    }

    if (!equal_ranges(data, testv.cipher)) {
        std::string error_msg = test_name + " (" + std::to_string(testi) +
                                ") failed in encryption !";
        throw std::runtime_error(error_msg);
    }

    {
        decrypter_type decrypter(aes_key);
        decrypter.decrypt(dynamic_buffer(data_with_padding));
        data = {data_with_padding.data(), data_with_padding.size()};
    }

    if (!equal_ranges(data, testv.plain)) {
        std::string error_msg = test_name + " (" + std::to_string(testi) +
                                ") failed in decryption !";
        throw std::runtime_error(error_msg);
    }
}

void do_aes_ecb_tests() {
    std::vector<ecb_aes_test_entry> ecb_test_vectors;

    // 1 - 128
    {
        auto& testv = ecb_test_vectors.emplace_back();
        testv.mode = 128;
        testv.key = h_to_b("2b7e151628aed2a6abf7158809cf4f3c");

        testv.plain = h_to_b("6bc1bee22e409f96e93d7e117393172a"
                             "ae2d8a571e03ac9c9eb76fac45af8e51"
                             "30c81c46a35ce411e5fbc1191a0a52ef"
                             "f69f2445df4f9b17ad2b417be66c3710");

        testv.cipher = h_to_b("3ad77bb40d7a3660a89ecaf32466ef97"
                              "f5d3d58503b9699de785895a96fdbaaf"
                              "43b1cd7f598ece23881b00e3ed030688"
                              "7b0c785e27e8ad3f8223207104725dd4");
    }

    // 2 - 192
    {
        auto& testv = ecb_test_vectors.emplace_back();
        testv.mode = 192;
        testv.key = h_to_b("8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b");

        testv.plain = h_to_b("6bc1bee22e409f96e93d7e117393172a"
                             "ae2d8a571e03ac9c9eb76fac45af8e51"
                             "30c81c46a35ce411e5fbc1191a0a52ef"
                             "f69f2445df4f9b17ad2b417be66c3710");

        testv.cipher = h_to_b("bd334f1d6e45f25ff712a214571fa5cc"
                              "974104846d0ad3ad7734ecb3ecee4eef"
                              "ef7afd2270e2e60adce0ba2face6444e"
                              "9a4b41ba738d6c72fb16691603c18e0e");
    }

    // 3 - 256
    {
        auto& testv = ecb_test_vectors.emplace_back();
        testv.mode = 256;
        testv.key = h_to_b("603deb1015ca71be2b73aef0857d77811f352c073b6"
                           "108d72d9810a30914dff4");

        testv.plain = h_to_b("6bc1bee22e409f96e93d7e117393172a"
                             "ae2d8a571e03ac9c9eb76fac45af8e51"
                             "30c81c46a35ce411e5fbc1191a0a52ef"
                             "f69f2445df4f9b17ad2b417be66c3710");

        testv.cipher = h_to_b("f3eed1bdb5d2a03c064b5a7e3db181f8"
                              "591ccb10d410ed26dc5ba74a31362870"
                              "b6ed21b99ca6f4f9f153e7b1beafed1d"
                              "23304b7a39f9f3ff067d8d8f9e24ecc7");
    }

    int testi = 0;
    for (const auto& testv : ecb_test_vectors) {
        switch (testv.mode) {
        case 128:
            TestECBAes<aes128>("ecb aes 128-bit", ++testi, testv);
            break;

        case 192:
            TestECBAes<aes192>("ecb aes 192-bit", ++testi, testv);
            break;

        case 256:
            TestECBAes<aes256>("ecb aes 256-bit", ++testi, testv);
            break;

        default:
            break;
        }
    }
}

struct ctr_aes_test_entry {
    int mode;
    std::vector<uint8_t> key;
    std::vector<uint8_t> iv;
    std::vector<uint8_t> plain;
    std::vector<uint8_t> cipher;
};

using cbc_aes_test_entry = ctr_aes_test_entry;

template <class CipherMode, bool NeedsPadding>
void Test_CBC_CTR_Aes(std::string_view test_name, int testi,
                      const ctr_aes_test_entry& testv) {
    using aes_mode = typename CipherMode::cipher_type;
    using encrypter_type = CipherMode;
    using decrypter_type = CipherMode;

    aes_mode aes_key;
    aes_key.set_key(buffer(testv.key));

    std::vector<uint8_t> data_with_padding;
    std::span<uint8_t> data;

    {
        encrypter_type encrypter(aes_key, buffer(testv.iv));
        encrypter.encrypt(buffer(testv.plain),
                          dynamic_buffer(data_with_padding));
        data = {data_with_padding.data(),
                data_with_padding.data() + testv.plain.size()};
    }

    if (!equal_ranges(data, testv.cipher)) {
        std::string error_msg = test_name + " (" + std::to_string(testi) +
                                ") failed in encryption !";
        throw std::runtime_error(error_msg);
    }

    {
        decrypter_type decrypter(aes_key, buffer(testv.iv));
        if constexpr (NeedsPadding) {
            decrypter.decrypt(dynamic_buffer(data_with_padding));
        }
        else {
            data_with_padding.clear();
            decrypter.decrypt(buffer(testv.cipher),
                              dynamic_buffer(data_with_padding));
        }
        data = {data_with_padding.data(), data_with_padding.size()};
    }

    if (!equal_ranges(data, testv.plain)) {
        std::string error_msg = test_name + " (" + std::to_string(testi) +
                                ") failed in decryption !";
        throw std::runtime_error(error_msg);
    }
}

void do_cbc_aes_vector_tests() {
    std::vector<cbc_aes_test_entry> test_vectors;

    // 1 - 128
    {
        auto& testv = test_vectors.emplace_back();

        testv.mode = 128;
        testv.key = h_to_b("2b7e151628aed2a6abf7158809cf4f3c");
        testv.iv = h_to_b("000102030405060708090a0b0c0d0e0f");

        testv.plain = h_to_b("6bc1bee22e409f96e93d7e117393172a"
                             "ae2d8a571e03ac9c9eb76fac45af8e51"
                             "30c81c46a35ce411e5fbc1191a0a52ef"
                             "f69f2445df4f9b17ad2b417be66c3710");

        testv.cipher = h_to_b("7649abac8119b246cee98e9b12e9197d"
                              "5086cb9b507219ee95db113a917678b2"
                              "73bed6b8e3c1743b7116e69e22229516"
                              "3ff1caa1681fac09120eca307586e1a7");
    }

    // 2 - 192
    {
        auto& testv = test_vectors.emplace_back();

        testv.mode = 192;
        testv.key = h_to_b("8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b");
        testv.iv = h_to_b("000102030405060708090a0b0c0d0e0f");

        testv.plain = h_to_b("6bc1bee22e409f96e93d7e117393172a"
                             "ae2d8a571e03ac9c9eb76fac45af8e51"
                             "30c81c46a35ce411e5fbc1191a0a52ef"
                             "f69f2445df4f9b17ad2b417be66c3710");

        testv.cipher = h_to_b("4f021db243bc633d7178183a9fa071e8"
                              "b4d9ada9ad7dedf4e5e738763f69145a"
                              "571b242012fb7ae07fa9baac3df102e0"
                              "08b0e27988598881d920a9e64f5615cd");
    }

    // 3 - 256
    {
        auto& testv = test_vectors.emplace_back();

        testv.mode = 256;
        testv.key = h_to_b("603deb1015ca71be2b73aef0857d77811f352c073b6"
                           "108d72d9810a30914dff4");
        testv.iv = h_to_b("000102030405060708090a0b0c0d0e0f");

        testv.plain = h_to_b("6bc1bee22e409f96e93d7e117393172a"
                             "ae2d8a571e03ac9c9eb76fac45af8e51"
                             "30c81c46a35ce411e5fbc1191a0a52ef"
                             "f69f2445df4f9b17ad2b417be66c3710");

        testv.cipher = h_to_b("f58c4c04d6e5f1ba779eabfb5f7bfbd6"
                              "9cfc4e967edb808d679f777bc6702c7d"
                              "39f23369a9d9bacfa530e26304231461"
                              "b2eb05e2c39be9fcda6c19078c6a9d1b");
    }

    int testi = 0;

    for (const auto& test_vector : test_vectors) {
        switch (test_vector.mode) {
        case 128:
            Test_CBC_CTR_Aes<cbc_mode<aes128>, true>("cbc aes 128-bit", ++testi,
                                                     test_vector);
            break;

        case 192:
            Test_CBC_CTR_Aes<cbc_mode<aes192>, true>("cbc aes 192-bit", ++testi,
                                                     test_vector);
            break;

        case 256:
            Test_CBC_CTR_Aes<cbc_mode<aes256>, true>("cbc aes 256-bit", ++testi,
                                                     test_vector);
            break;

        default:
            break;
        }
    }
}

void do_ctr_aes_vector_tests() {
    std::vector<ctr_aes_test_entry> ctr_test_vectors;

    // 1 - 128
    {
        auto& testv = ctr_test_vectors.emplace_back();

        testv.mode = 128;
        testv.key = h_to_b("2b7e151628aed2a6abf7158809cf4f3c");
        testv.iv = h_to_b("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");

        testv.plain = h_to_b("6bc1bee22e409f96e93d7e117393172a"
                             "ae2d8a571e03ac9c9eb76fac45af8e51"
                             "30c81c46a35ce411e5fbc1191a0a52ef"
                             "f69f2445df4f9b17ad2b417be66c3710");

        testv.cipher = h_to_b("874d6191b620e3261bef6864990db6ce"
                              "9806f66b7970fdff8617187bb9fffdff"
                              "5ae4df3edbd5d35e5b4f09020db03eab"
                              "1e031dda2fbe03d1792170a0f3009cee");
    }

    // 2 - 192
    {
        auto& testv = ctr_test_vectors.emplace_back();

        testv.mode = 192;
        testv.key = h_to_b("8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b");
        testv.iv = h_to_b("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");

        testv.plain = h_to_b("6bc1bee22e409f96e93d7e117393172a"
                             "ae2d8a571e03ac9c9eb76fac45af8e51"
                             "30c81c46a35ce411e5fbc1191a0a52ef"
                             "f69f2445df4f9b17ad2b417be66c3710");

        testv.cipher = h_to_b("1abc932417521ca24f2b0459fe7e6e0b"
                              "090339ec0aa6faefd5ccc2c6f4ce8e94"
                              "1e36b26bd1ebc670d1bd1d665620abf7"
                              "4f78a7f6d29809585a97daec58c6b050");
    }

    // 3 - 256
    {
        auto& testv = ctr_test_vectors.emplace_back();

        testv.mode = 256;
        testv.key = h_to_b("603deb1015ca71be2b73aef0857d77811f352c073b6"
                           "108d72d9810a30914dff4");
        testv.iv = h_to_b("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");

        testv.plain = h_to_b("6bc1bee22e409f96e93d7e117393172a"
                             "ae2d8a571e03ac9c9eb76fac45af8e51"
                             "30c81c46a35ce411e5fbc1191a0a52ef"
                             "f69f2445df4f9b17ad2b417be66c3710");

        testv.cipher = h_to_b("601ec313775789a5b7a7f504bbf3d228"
                              "f443e3ca4d62b59aca84e990cacaf5c5"
                              "2b0930daa23de94ce87017ba2d84988d"
                              "dfc9c58db67aada613c2dd08457941a6");
    }

    int testi = 0;

    for (const auto& test_vector : ctr_test_vectors) {
        switch (test_vector.mode) {
        case 128:
            Test_CBC_CTR_Aes<ctr_mode<aes128>, false>("ctr aes 128-bit",
                                                      ++testi, test_vector);
            break;

        case 192:
            Test_CBC_CTR_Aes<ctr_mode<aes192>, false>("ctr aes 192-bit",
                                                      ++testi, test_vector);
            break;

        case 256:
            Test_CBC_CTR_Aes<ctr_mode<aes256>, false>("ctr aes 256-bit",
                                                      ++testi, test_vector);
            break;

        default:
            break;
        }
    }
}

struct gcm_aes_test_entry {
    int mode;
    std::vector<uint8_t> key;
    std::vector<uint8_t> iv;
    std::vector<uint8_t> plain;
    std::vector<uint8_t> aad;
    std::vector<uint8_t> cipher;
    std::vector<uint8_t> tag;
};

template <class AesMode>
void TestGCMAes(std::string_view test_name, int testi,
                const gcm_aes_test_entry& testv) {
    using aes_mode = AesMode;

    std::vector<uint8_t> unaligned_aad_vec;
    unaligned_aad_vec.push_back(0);
    unaligned_aad_vec.insert(unaligned_aad_vec.end(), testv.aad.begin(),
                             testv.aad.end());

    std::span<const uint8_t> unaligned_aad(unaligned_aad_vec);
    unaligned_aad = unaligned_aad.subspan(1);

    aes_mode aes_cipher;
    aes_cipher.set_key(buffer(testv.key));

    using encryption_type = typename gcm_mode<aes_mode>::encryption;
    using decryption_type = typename gcm_mode<aes_mode>::decryption;

    auto ciphertext = testv.plain;
    typename gcm_mode<aes_mode>::tag_type tag;

    // aligned encryption
    {
        encryption_type gcm_encrypter(aes_cipher, buffer(testv.iv));
        gcm_encrypter.encrypt(buffer(ciphertext), buffer(testv.aad),
                              buffer(tag));
    }

    bool cipher_matches = equal_ranges(ciphertext, testv.cipher);
    bool tag_matches = equal_ranges(tag, testv.tag);

    if (!cipher_matches) {
        throw std::runtime_error(
            test_name + " (" + std::to_string(testi) +
            ") failed in encryption, the cipher mismatches !");
    }
    else if (!tag_matches) {
        throw std::runtime_error(
            test_name + " (" + std::to_string(testi) +
            ") failed in encryption, the auth tag mismatches !");
    }

    {
        std::vector<uint8_t> unaligned_ciphertext;
        std::vector<uint8_t> unaligned_tag;

        unaligned_tag.resize(tag.size() + 1);

        unaligned_ciphertext.push_back(0);
        unaligned_ciphertext.insert(unaligned_ciphertext.end(),
                                    testv.plain.begin(), testv.plain.end());

        encryption_type gcm_encrypter(aes_cipher, buffer(testv.iv));
        gcm_encrypter.encrypt(buffer(unaligned_ciphertext) + 1,
                              buffer(unaligned_aad), buffer(unaligned_tag) + 1);
        unaligned_ciphertext.erase(unaligned_ciphertext.begin());
        unaligned_tag.erase(unaligned_tag.begin());

        bool cipher_matches = equal_ranges(unaligned_ciphertext, testv.cipher);
        bool tag_matches = equal_ranges(unaligned_tag, testv.tag);

        if (!cipher_matches) {
            throw std::runtime_error(
                test_name + " (" + std::to_string(testi) +
                ") failed in unalinged encryption, the cipher "
                "mismatches !");
        }
        else if (!tag_matches) {
            throw std::runtime_error(
                test_name + " (" + std::to_string(testi) +
                ") failed in unaligned encryption, the auth tag "
                "mismatches !");
        }
    }

    {
        std::error_code ec;
        decryption_type gcm_decrypter(aes_cipher, buffer(testv.iv));
        gcm_decrypter.decrypt(buffer(ciphertext), buffer(testv.aad),
                              buffer(tag), ec);
        if (ec) {
            throw std::runtime_error(test_name + " (" + std::to_string(testi) +
                                     ") failed in decryption, the "
                                     "auth tag mismatches !");
        }
    }

    bool plain_matches = equal_ranges(ciphertext, testv.plain);

    if (!plain_matches) {
        throw std::runtime_error(
            test_name + " (" + std::to_string(testi) +
            ") failed in decryption, the plaintext mismatches !");
    }

    {
        std::vector<uint8_t> unaligned_ciphertext;
        unaligned_ciphertext.push_back(0);
        unaligned_ciphertext.insert(unaligned_ciphertext.end(),
                                    testv.cipher.begin(), testv.cipher.end());

        std::error_code ec;
        decryption_type gcm_decrypter(aes_cipher, buffer(testv.iv));
        gcm_decrypter.decrypt(buffer(unaligned_ciphertext) + 1,
                              buffer(unaligned_aad), buffer(tag), ec);
        if (ec) {
            throw std::runtime_error(
                test_name + " (" + std::to_string(testi) +
                ") failed in unaligned decryption, the auth tag "
                "mismatches !");
        }
        unaligned_ciphertext.erase(unaligned_ciphertext.begin());
        if (!equal_ranges(unaligned_ciphertext, testv.plain)) {
            throw std::runtime_error(
                test_name + " (" + std::to_string(testi) +
                ") failed in unaligned decryption, the plaintext "
                "mismatches !");
        }
    }

    std::stringstream input_stream;
    input_stream.write(reinterpret_cast<const char*>(testv.plain.data()),
                       testv.plain.size());
    std::stringstream out_stream;
    encryption_type gcm_encrypter(aes_cipher, buffer(testv.iv));
    gcm_encrypter.encrypt(input_stream, out_stream, buffer(testv.aad),
                          buffer(tag));
    ciphertext.clear();
    for (auto ch : out_stream.str()) {
        ciphertext.push_back(static_cast<uint8_t>(ch));
    }
    cipher_matches = equal_ranges(testv.cipher, ciphertext);
    tag_matches = equal_ranges(tag, testv.tag);

    if (!cipher_matches) {
        throw std::runtime_error(
            test_name + " (" + std::to_string(testi) +
            ") failed in stream encryption, the cipher mismatches !");
    }
    else if (!tag_matches) {
        throw std::runtime_error(
            test_name + " (" + std::to_string(testi) +
            ") failed in stream encryption, the auth tag mismatches !");
    }
}

void do_gcm_aes_tests() {
    std::vector<gcm_aes_test_entry> test_vectors;

    // test 1
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 128;
        entry.key = h_to_b("00000000000000000000000000000000");
        entry.iv = h_to_b("000000000000000000000000");
        entry.tag = h_to_b("58e2fccefa7e3061367f1d57a4e7455a");
    }

    // test 2
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 128;
        entry.key = h_to_b("00000000000000000000000000000000");
        entry.plain = h_to_b("00000000000000000000000000000000");
        entry.iv = h_to_b("000000000000000000000000");
        entry.cipher = h_to_b("0388dace60b6a392f328c2b971b2fe78");
        entry.tag = h_to_b("ab6e47d42cec13bdf53a67b21257bddf");
    }

    // test 3
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 128;
        entry.key = h_to_b("feffe9928665731c6d6a8f9467308308");
        entry.plain = h_to_b("d9313225f88406e5a55909c5aff5269a86a7a9531"
                             "534f7da2e4c303d8a318a721c3c0c"
                             "95956809532fcf0e2449a6b525b16aedf5aa0de65"
                             "7ba637b391aafd255");
        entry.iv = h_to_b("cafebabefacedbaddecaf888");
        entry.cipher = h_to_b("42831ec2217774244b7221b784d0d49ce3aa212f"
                              "2c02a4e035c17e2329aca12e21d514"
                              "b25466931c7d8f6a5aac84aa051ba30b396a0aac"
                              "973d58e091473f5985");
        entry.tag = h_to_b("4d5c2af327cd64a62cf35abd2ba6fab4");
    }

    // test 4
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 128;
        entry.key = h_to_b("feffe9928665731c6d6a8f9467308308");
        entry.plain = h_to_b("d9313225f88406e5a55909c5aff5269a86a7a9531"
                             "534f7da2e4c303d8a318a7"
                             "21c3c0c95956809532fcf0e2449a6b525b16aedf5"
                             "aa0de657ba637b39");
        entry.aad = h_to_b("feedfacedeadbeeffeedfacedeadbeefabaddad2");
        entry.iv = h_to_b("cafebabefacedbaddecaf888");
        entry.cipher = h_to_b("42831ec2217774244b7221b784d0d49ce3aa212f"
                              "2c02a4e035c17e2329aca12"
                              "e21d514b25466931c7d8f6a5aac84aa051ba30b3"
                              "96a0aac973d58e091");
        entry.tag = h_to_b("5bc94fbc3221a5db94fae95ae7121a47");
    }

    // test 5
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 128;
        entry.key = h_to_b("feffe9928665731c6d6a8f9467308308");
        entry.plain = h_to_b("d9313225f88406e5a55909c5aff5269a86a7a9531"
                             "534f7da2e4c303d8a318a7"
                             "21c3c0c95956809532fcf0e2449a6b525b16aedf5"
                             "aa0de657ba637b39");
        entry.aad = h_to_b("feedfacedeadbeeffeedfacedeadbeefabaddad2");
        entry.iv = h_to_b("cafebabefacedbad");
        entry.cipher = h_to_b("61353b4c2806934a777ff51fa22a4755699b2a71"
                              "4fcdc6f83766e5f97b6c742"
                              "373806900e49f24b22b097544d4896b424989b5e"
                              "1ebac0f07c23f4598");
        entry.tag = h_to_b("3612d2e79e3b0785561be14aaca2fccb");
    }

    // test 6
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 128;
        entry.key = h_to_b("feffe9928665731c6d6a8f9467308308");
        entry.plain = h_to_b("d9313225f88406e5a55909c5aff5269a86a7a9531"
                             "534f7da2e4c303d8a318a7"
                             "21c3c0c95956809532fcf0e2449a6b525b16aedf5"
                             "aa0de657ba637b39");
        entry.aad = h_to_b("feedfacedeadbeeffeedfacedeadbeefabaddad2");
        entry.iv = h_to_b("9313225df88406e555909c5aff5269aa6a7a9538534f"
                          "7da1e4c303d2a318a72"
                          "8c3c0c95156809539fcf0e2429a6b525416aedbf5a0d"
                          "e6a57a637b39b");
        entry.cipher = h_to_b("8ce24998625615b603a033aca13fb894be9112a5"
                              "c3a211a8ba262a3cca7e2ca"
                              "701e4a9a4fba43c90ccdcb281d48c7c6fd62875d"
                              "2aca417034c34aee5");
        entry.tag = h_to_b("619cc5aefffe0bfa462af43c1699d050");
    }

    // test 7
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 192;
        entry.key = h_to_b("000000000000000000000000000000000000000000000000");
        entry.iv = h_to_b("000000000000000000000000");
        entry.tag = h_to_b("cd33b28ac773f74ba00ed1f312572435");
    }

    // test 8
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 192;
        entry.key = h_to_b("000000000000000000000000000000000000000000000000");
        entry.plain = h_to_b("00000000000000000000000000000000");
        entry.iv = h_to_b("000000000000000000000000");
        entry.cipher = h_to_b("98e7247c07f0fe411c267e4384b0f600");
        entry.tag = h_to_b("2ff58d80033927ab8ef4d4587514f0fb");
    }

    // test 9
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 192;
        entry.key = h_to_b("feffe9928665731c6d6a8f9467308308feffe9928665731c");
        entry.plain = h_to_b("d9313225f88406e5a55909c5aff5269a86a7a9531"
                             "534f7da2e4c303d8a318a721c3c0c"
                             "95956809532fcf0e2449a6b525b16aedf5aa0de65"
                             "7ba637b391aafd255");
        entry.iv = h_to_b("cafebabefacedbaddecaf888");
        entry.cipher = h_to_b("3980ca0b3c00e841eb06fac4872a2757859e1cea"
                              "a6efd984628593b40ca1e19c7d773d"
                              "00c144c525ac619d18c84a3f4718e2448b2fe324"
                              "d9ccda2710acade256");
        entry.tag = h_to_b("9924a7c8587336bfb118024db8674a14");
    }

    // test 10
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 192;
        entry.key = h_to_b("feffe9928665731c6d6a8f9467308308feffe9928665731c");
        entry.plain = h_to_b("d9313225f88406e5a55909c5aff5269a86a7a9531"
                             "534f7da2e4c303d8a318a7"
                             "21c3c0c95956809532fcf0e2449a6b525b16aedf5"
                             "aa0de657ba637b39");
        entry.aad = h_to_b("feedfacedeadbeeffeedfacedeadbeefabaddad2");
        entry.iv = h_to_b("cafebabefacedbaddecaf888");
        entry.cipher = h_to_b("3980ca0b3c00e841eb06fac4872a2757859e1cea"
                              "a6efd984628593b40ca1e19"
                              "c7d773d00c144c525ac619d18c84a3f4718e2448"
                              "b2fe324d9ccda2710");
        entry.tag = h_to_b("2519498e80f1478f37ba55bd6d27618c");
    }

    // test 11
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 192;
        entry.key = h_to_b("feffe9928665731c6d6a8f9467308308feffe9928665731c");
        entry.plain = h_to_b("d9313225f88406e5a55909c5aff5269a86a7a9531"
                             "534f7da2e4c303d8a318a7"
                             "21c3c0c95956809532fcf0e2449a6b525b16aedf5"
                             "aa0de657ba637b39");
        entry.aad = h_to_b("feedfacedeadbeeffeedfacedeadbeefabaddad2");
        entry.iv = h_to_b("cafebabefacedbad");
        entry.cipher = h_to_b("0f10f599ae14a154ed24b36e25324db8c566632e"
                              "f2bbb34f8347280fc450705"
                              "7fddc29df9a471f75c66541d4d4dad1c9e93a19a"
                              "58e8b473fa0f062f7");
        entry.tag = h_to_b("65dcc57fcf623a24094fcca40d3533f8");
    }

    // test 12
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 192;
        entry.key = h_to_b("feffe9928665731c6d6a8f9467308308feffe9928665731c");
        entry.plain = h_to_b("d9313225f88406e5a55909c5aff5269a86a7a9531"
                             "534f7da2e4c303d8a318a7"
                             "21c3c0c95956809532fcf0e2449a6b525b16aedf5"
                             "aa0de657ba637b39");
        entry.aad = h_to_b("feedfacedeadbeeffeedfacedeadbeefabaddad2");
        entry.iv = h_to_b("9313225df88406e555909c5aff5269aa6a7a9538534f"
                          "7da1e4c303d2a318a72"
                          "8c3c0c95156809539fcf0e2429a6b525416aedbf5a0d"
                          "e6a57a637b39b");
        entry.cipher = h_to_b("d27e88681ce3243c4830165a8fdcf9ff1de9a1d8"
                              "e6b447ef6ef7b79828666e4"
                              "581e79012af34ddd9e2f037589b292db3e67c036"
                              "745fa22e7e9b7373b");
        entry.tag = h_to_b("dcf566ff291c25bbb8568fc3d376a6d9");
    }

    // test 13
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 256;
        entry.key = h_to_b("0000000000000000000000000000000000000000000"
                           "000000000000000000000");
        entry.iv = h_to_b("000000000000000000000000");
        entry.tag = h_to_b("530f8afbc74536b9a963b4f1c4cb738b");
    }

    // test 14
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 256;
        entry.key = h_to_b("0000000000000000000000000000000000000000000"
                           "000000000000000000000");
        entry.plain = h_to_b("00000000000000000000000000000000");
        entry.iv = h_to_b("000000000000000000000000");
        entry.cipher = h_to_b("cea7403d4d606b6e074ec5d3baf39d18");
        entry.tag = h_to_b("d0d1c8a799996bf0265b98b5d48ab919");
    }

    // test 15
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 256;
        entry.key = h_to_b("feffe9928665731c6d6a8f9467308308feffe992866"
                           "5731c6d6a8f9467308308");
        entry.plain = h_to_b("d9313225f88406e5a55909c5aff5269a86a7a9531"
                             "534f7da2e4c303d8a318a721c3c0c"
                             "95956809532fcf0e2449a6b525b16aedf5aa0de65"
                             "7ba637b391aafd255");
        entry.iv = h_to_b("cafebabefacedbaddecaf888");
        entry.cipher = h_to_b("522dc1f099567d07f47f37a32a84427d643a8cdc"
                              "bfe5c0c97598a2bd2555d1aa8cb08e"
                              "48590dbb3da7b08b1056828838c5f61e6393ba7a"
                              "0abcc9f662898015ad");
        entry.tag = h_to_b("b094dac5d93471bdec1a502270e3cc6c");
    }

    // test 16
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 256;
        entry.key = h_to_b("feffe9928665731c6d6a8f9467308308feffe992866"
                           "5731c6d6a8f9467308308");
        entry.plain = h_to_b("d9313225f88406e5a55909c5aff5269a86a7a9531"
                             "534f7da2e4c303d8a318a7"
                             "21c3c0c95956809532fcf0e2449a6b525b16aedf5"
                             "aa0de657ba637b39");
        entry.aad = h_to_b("feedfacedeadbeeffeedfacedeadbeefabaddad2");
        entry.iv = h_to_b("cafebabefacedbaddecaf888");
        entry.cipher = h_to_b("522dc1f099567d07f47f37a32a84427d643a8cdc"
                              "bfe5c0c97598a2bd2555d1a"
                              "a8cb08e48590dbb3da7b08b1056828838c5f61e6"
                              "393ba7a0abcc9f662");
        entry.tag = h_to_b("76fc6ece0f4e1768cddf8853bb2d551b");
    }

    // test 17
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 256;
        entry.key = h_to_b("feffe9928665731c6d6a8f9467308308feffe992866"
                           "5731c6d6a8f9467308308");
        entry.plain = h_to_b("d9313225f88406e5a55909c5aff5269a86a7a9531"
                             "534f7da2e4c303d8a318a7"
                             "21c3c0c95956809532fcf0e2449a6b525b16aedf5"
                             "aa0de657ba637b39");
        entry.aad = h_to_b("feedfacedeadbeeffeedfacedeadbeefabaddad2");
        entry.iv = h_to_b("cafebabefacedbad");
        entry.cipher = h_to_b("c3762df1ca787d32ae47c13bf19844cbaf1ae14d"
                              "0b976afac52ff7d79bba9de"
                              "0feb582d33934a4f0954cc2363bc73f7862ac430"
                              "e64abe499f47c9b1f");
        entry.tag = h_to_b("3a337dbf46a792c45e454913fe2ea8f2");
    }

    // test 18
    {
        gcm_aes_test_entry& entry = test_vectors.emplace_back();
        entry.mode = 256;
        entry.key = h_to_b("feffe9928665731c6d6a8f9467308308feffe992866"
                           "5731c6d6a8f9467308308");
        entry.plain = h_to_b("d9313225f88406e5a55909c5aff5269a86a7a9531"
                             "534f7da2e4c303d8a318a7"
                             "21c3c0c95956809532fcf0e2449a6b525b16aedf5"
                             "aa0de657ba637b39");
        entry.aad = h_to_b("feedfacedeadbeeffeedfacedeadbeefabaddad2");
        entry.iv = h_to_b("9313225df88406e555909c5aff5269aa6a7a9538534f"
                          "7da1e4c303d2a318a72"
                          "8c3c0c95156809539fcf0e2429a6b525416aedbf5a0d"
                          "e6a57a637b39b");
        entry.cipher = h_to_b("5a8def2f0c9e53f1f75d7853659e2a20eeb2b22a"
                              "afde6419a058ab4f6f746bf"
                              "40fc0c3b780f244452da3ebf1c5d82cdea241899"
                              "7200ef82e44ae7e3f");
        entry.tag = h_to_b("a44a8266ee1c8eb0c8b5d4cf5ae9f19a");
    }

    int i = 0;
    for (const auto& testv : test_vectors) {
        ++i;
        if (testv.mode == 128) {
            TestGCMAes<aes128>("gcm aes 128-bit", i, testv);
        }
        else if (testv.mode == 192) {
            TestGCMAes<aes192>("gcm aes 192-bit", i, testv);
        }
        else if (testv.mode == 256) {
            TestGCMAes<aes256>("gcm aes 256-bit", i, testv);
        }
    }
}

namespace tests_fn {
    bool do_crypto_tests() {
        enable_aesni();
        enable_pclmulqdq();

        try {
            do_aes_ecb_tests();
            do_cbc_aes_vector_tests();
            do_ctr_aes_vector_tests();
            do_gcm_aes_tests();
        }
        catch (const std::exception& ex) {
            std::cout << "[!] crypto tests failed : " << ex.what() << "\n";
            return false;
        }

        disable_aesni();
        disable_pclmulqdq();

        try {
            do_aes_ecb_tests();
            do_cbc_aes_vector_tests();
            do_ctr_aes_vector_tests();
            do_gcm_aes_tests();
        }
        catch (const std::exception& ex) {
            std::cout << "[!] crypto tests failed without cpu "
                         "instructions : "
                      << ex.what() << "\n";
            return false;
        }

        std::cout << "[*] crypto tests passed\n";
        return true;
    }
} // namespace tests_fn