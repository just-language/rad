#include <rad/stack_forward_list.h>
#include <rad/stack_list.h>

#include <array>
#include <iostream>
#include <vector>

using namespace RAD_LIB_NAMESPACE;

namespace {

    void throw_if_false(bool is_true, const char* msg) {
        if (!is_true) {
            throw std::runtime_error(msg);
        }
    }

    void test_stack_list() {
        struct first_base {
            std::string unused;
        };

        struct list_elem : first_base, stack_double_list_node {
            int val;

            list_elem(int i) : val(i) {
            }

            bool operator==(const list_elem& other) const noexcept {
                return val == other.val;
            }
        };

        // all elements must exist until they are unlinked from the list
        // (pop, erase, clear, list destruction) if an element is copied
        // or moved, the original object stills the linked object in the
        // list

        list_elem elem1(1), elem2(2), elem3(3);

        stack_list<list_elem> slist;
        throw_if_false(slist.empty(), "stack_list empty() error");
        throw_if_false(slist.size() == 0, "stack_list size() error");

        slist.push_back(elem1);
        slist.push_back(elem2);
        slist.push_back(elem3);

        throw_if_false(!slist.empty(), "stack_list empty() error");
        throw_if_false(slist.size() == 3, "stack_list size() error");
        throw_if_false(slist.front() == elem1, "stack_list front() error");
        throw_if_false(slist.back() == elem3, "stack_list back() error");
        throw_if_false(&slist.front() == &elem1, "stack_list front() error");
        throw_if_false(&slist.back() == &elem3, "stack_list back() error");

        {
            auto expected = std::array{1, 2, 3};
            throw_if_false(
                std::equal(slist.begin(), slist.end(), expected.begin()),
                "stack_list begin() end() error");
        }

        slist.clear();
        throw_if_false(slist.empty(), "stack_list empty() error");
        throw_if_false(slist.size() == 0, "stack_list size() error");

        slist.push_back(elem1);
        slist.push_back(elem2);
        slist.push_back(elem3);

        throw_if_false(&slist.pop_front() == &elem1,
                       "stack_list pop_front() error");
        throw_if_false(&slist.pop_back() == &elem3,
                       "stack_list pop_front() error");
        throw_if_false(slist.size() == 1, "stack_list size() error");
        throw_if_false(slist.front() == slist.back() &&
                           &slist.front() == &slist.back(),
                       "stack_list front() back() error");
        throw_if_false(slist.front() == elem2 && &slist.front() == &elem2,
                       "stack_list front() error");
        slist.pop_front();

        std::vector<list_elem> elems_vec;
        elems_vec.reserve(10);
        for (int i = 0; i < 10; ++i) {
            elems_vec.emplace_back(i);
        }

        for (auto& elem : elems_vec) {
            slist.push_back(elem);
        }

        throw_if_false(slist.size() == elems_vec.size(),
                       "stack_list size() error");
        throw_if_false(
            std::equal(slist.begin(), slist.end(), elems_vec.begin()),
            "stack_list begin() end() error");

        {
            std::vector<int> expected;
            for (int i = 9; i >= 0; --i) {
                expected.emplace_back(i);
            }
            throw_if_false(
                slist.size() == expected.size() &&
                    std::equal(slist.rbegin(), slist.rend(), expected.begin()),
                "stack_list rbegin() rend() error");
        }

        size_t removed = 0;
        for (auto it = slist.begin(); it != slist.end();) {
            if (it->val % 2 == 0) {
                it = slist.erase(it);
                ++removed;
            }
            else {
                ++it;
            }
        }
        throw_if_false(slist.size() + removed == elems_vec.size(),
                       "stack_list erase() error");

        {
            std::vector<int> expected;
            for (int i = 0; i < 10; ++i) {
                if (i % 2 == 0) {
                    continue;
                }
                expected.emplace_back(i);
            }

            throw_if_false(
                slist.size() == expected.size() &&
                    std::equal(slist.begin(), slist.end(), expected.begin()),
                "stack_list begin() end() error");
        }

        stack_list<list_elem> slist2;
        slist2.push_front(elem1);
        slist2.push_front(elem2);
        slist2.push_front(elem3);

        throw_if_false(!slist2.empty(), "stack_list empty() error");
        throw_if_false(slist2.size() == 3, "stack_list size() error");

        std::size_t old_size = slist.size();
        slist.merge_back(slist2);

        throw_if_false(slist2.empty(), "stack_list empty() error");
        throw_if_false(slist2.size() == 0, "stack_list size() error");

        throw_if_false(slist.size() == old_size + 3, "stack_list size() error");

        old_size = slist.size();
        auto slist3 = std::move(slist);
        throw_if_false(slist3.size() == old_size, "stack_list move ctor error");
        throw_if_false(slist.empty() && slist.size() == 0,
                       "stack_list move ctor error");

        slist = std::move(slist3);
        throw_if_false(slist.size() == old_size,
                       "stack_list move assignment error");
        throw_if_false(slist3.empty() && slist3.size() == 0,
                       "stack_list move assignment error");

        // remove the elements of the vector from the list
        for (size_t i = 0; i < (elems_vec.size() - removed); ++i) {
            std::ignore = i;
            slist.pop_front();
        }

        slist.clear();

        slist.push_front(elem1);
        throw_if_false(slist.erase(&elem1) == nullptr,
                       "stack_list erase() error");

        slist.push_back(elem2);
        throw_if_false(slist.erase(&elem2) == nullptr,
                       "stack_list erase() error");

        slist.push_back(elem3);
        throw_if_false(slist.erase(slist.begin()) == slist.end(),
                       "stack_list erase() error");

        struct str_elem : stack_double_list_node {
            std::string str;

            str_elem(std::string_view s) : str(s) {
            }
        };

        str_elem selem1{"elem1"}, selem2{"elem2"}, selem3{"selem3"};

        stack_list<str_elem> selems;
        selems.push_back(selem1);
        selems.push_back(selem2);
        selems.push_back(selem3);

        auto sit =
            selems.find([](const str_elem& e) { return e.str == "elem2"; });
        throw_if_false(sit != selems.end(), "stack_list find() error");
        throw_if_false(sit->str == "elem2", "stack_list find() error");
        throw_if_false(sit.get() == &selem2, "stack_list find() error");

        selems.clear();

        std::vector<list_elem> ivec;
        ivec.reserve(10);
        for (int i = 0; i < 10; ++i) {
            ivec.emplace_back(i);
        }

        slist.insert_back(ivec.begin(), ivec.end());
        throw_if_false(!slist.empty(), "stack_list insert_back() error");
        throw_if_false(slist.size() == 10, "stack_list insert_back() error");
        throw_if_false(std::equal(slist.begin(), slist.end(), ivec.begin()),
                       "stack_list insert_back() error");
        slist.clear();

        slist.insert_front(ivec.begin(), ivec.end());
        throw_if_false(!slist.empty(), "stack_list insert_front() error");
        throw_if_false(slist.size() == 10, "stack_list insert_front() error");
        throw_if_false(std::equal(slist.begin(), slist.end(), ivec.begin()),
                       "stack_list insert_front() error");
        slist.clear();
    }

    void test_forward_list() {
        struct list_elem : stack_forward_list_node {
            int val;

            list_elem(int i) : val(i) {
            }

            bool operator==(const list_elem& other) const noexcept {
                return val == other.val;
            }
        };

        // all elements must exist until they are unlinked from the list
        // (pop, erase, clear, list destruction) if an element is copied
        // or moved, the original object stills the linked in the list

        list_elem elem1(1), elem2(2), elem3(3);

        stack_forward_list<list_elem> slist;

        throw_if_false(slist.empty(),
                       "stack_forward_list default constructor error");
        throw_if_false(slist.size() == 0,
                       "stack_forward_list default constructor error");

        slist.push_back(elem1);
        slist.push_back(elem2);
        slist.push_back(elem3);

        throw_if_false(!slist.empty(), "stack_forward_list push_back() error");
        throw_if_false(slist.size() == 3,
                       "stack_forward_list push_back() error");
        throw_if_false(slist.front() == elem1 && &slist.front() == &elem1,
                       "stack_forward_list push_back() error");
        throw_if_false(slist.back() == elem3 && &slist.back() == &elem3,
                       "stack_forward_list push_back() error");

        {
            auto expected = std::array{1, 2, 3};
            throw_if_false(
                std::equal(slist.begin(), slist.end(), expected.begin()),
                "stack_forward_list begin() end() error");
        }

        slist.clear();
        throw_if_false(slist.empty(), "stack_forward_list clear() error");
        throw_if_false(slist.size() == 0, "stack_forward_list clear() error");

        slist.push_back(elem1);
        slist.push_back(elem2);
        slist.push_back(elem3);

        throw_if_false(&slist.pop_front() == &elem1,
                       "stack_forward_list pop_front() error");
        throw_if_false(&slist.pop_back() == &elem3,
                       "stack_forward_list pop_back() error");
        throw_if_false(slist.size() == 1, "stack_forward_list size() error");
        throw_if_false(slist.front() == slist.back() &&
                           &slist.front() == &slist.back(),
                       "stack_forward_list front() back() error");
        throw_if_false(slist.front() == elem2 && &slist.front() == &elem2,
                       "stack_forward_list front() error");
        slist.pop_front();

        std::vector<list_elem> elems_vec;
        elems_vec.reserve(10);
        for (int i = 0; i < 10; ++i) {
            elems_vec.emplace_back(i);
        }

        for (auto& elem : elems_vec) {
            slist.push_back(elem);
        }

        throw_if_false(slist.size() == 10,
                       "stack_forward_list push_back() error");

        stack_forward_list<list_elem> slist2;
        slist2.push_front(elem1);
        slist2.push_front(elem2);
        slist2.push_front(elem3);

        throw_if_false(!slist2.empty(), "stack_forward_list push_back() error");
        throw_if_false(slist2.size() == 3,
                       "stack_forward_list push_back() error");

        auto old_size = slist.size();
        slist.merge_back(slist2);

        throw_if_false(slist2.empty(), "stack_forward_list merge_back() error");
        throw_if_false(slist2.size() == 0,
                       "stack_forward_list merge_back() error");

        throw_if_false(slist.size() == old_size + 3,
                       "stack_forward_list merge_back() error");

        old_size = slist.size();
        auto slist3 = std::move(slist);
        throw_if_false(slist3.size() == old_size,
                       "stack_forward_list move ctor error()");
        throw_if_false(slist.empty() && slist.size() == 0,
                       "stack_forward_list move ctor error()");

        slist = std::move(slist3);
        throw_if_false(slist.size() == old_size,
                       "stack_forward_list move assignment error()");
        throw_if_false(slist3.empty() && slist3.size() == 0,
                       "stack_forward_list move assignment error()");

        slist.clear();
    }

} // namespace

namespace tests_fn {
    bool do_stack_list_tests() {
        try {
            test_stack_list();
            test_forward_list();
        }
        catch (const std::exception& ex) {
            std::cout << "[!] stack lists tests failed ! " << ex.what()
                      << " !\n";
            return false;
        }

        std::cout << "[*] stack lists tests passed\n";
        return true;
    }
} // namespace tests_fn
