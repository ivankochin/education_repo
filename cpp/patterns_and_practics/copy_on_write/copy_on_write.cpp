#include <iostream>
#include <memory>

struct printed {
    printed() { std::cout<<"default ctor"<<std::endl; }
    printed(int) { std::cout<<"default ctor"<<std::endl; }
    printed(const printed&) { std::cout<<"copy ctor"<<std::endl; }
    printed& operator= (const printed&) { std::cout<<"copy assigned"<<std::endl; return *this; }
    ~printed() { std::cout<<"dtor"<<std::endl; }
};

template <typename value>
class cow {
    std::shared_ptr<value> val_ptr;
public:
    cow() {
        val_ptr = std::make_shared<value>();
    }
    cow(const value& v) {
        val_ptr = std::make_shared<value>(v);
    }
    
    cow(const cow<value>& c) {
        val_ptr = c.val_ptr;
    }
    
    cow(cow<value>&& c) {
        val_ptr = std::move(c.val_ptr);
    }
    
    cow& operator= (const value& v) {
        val_ptr = std::make_shared<value>(v);
        return *this;
    }
    
    cow& operator= (const cow<value>& c) {
        val_ptr = std::make_shared<value>(*c.val_ptr);
        return *this;
    }
};

int main() {
    cow<printed> c;
    cow<printed> c2 = c;
    cow<printed> c3 = c2;
    c3 = 3;

    return 0;
}
