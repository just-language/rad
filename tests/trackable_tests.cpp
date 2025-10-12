#include <rad/trackable.h>

#include <iostream>

using namespace RAD_LIB_NAMESPACE;

namespace {
    struct object : public trackable {
        void print() {
            //	printf("this is a tracked object\n");
        }
    };

    struct copied {
        copied() {
            //	std::cout << "default constructor of
            // copied\n";
        }

        copied(const copied&) {
            //	std::cout << "copy constructor is called
            // for
            // copied\n";
        }

        ~copied() {
            //	std::cout << "destructor of copied\n";
        }
    };

    void test_pointer_lambda() {
        object obj;
        pointer<object> ptr = &obj;
        copied c;

        ptr->print();

        {
            object obj2;
            ptr = &obj2;
        }

        // ptr->print();
    }
} // namespace

namespace tests_fn {
    bool do_trackable_tests() {
        test_pointer_lambda();
        return true;
    }
} // namespace tests_fn