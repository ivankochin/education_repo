#include <iostream>

// Simplify compile-time dispatching
template <size_t num>
struct val_to_type {
    enum { value = num };
};

template <typename T, bool IsPolymorphic>
class instance_copyist {
    T* instance;

    void copy_impl(const T& obj, val_to_type<true>){
        std::cout<<"copy method called"<<std::endl;
        instance = obj.copy();
    }

    void copy_impl(const T& obj, val_to_type<false>){
        std::cout<<"copy ctor called"<<std::endl;
        instance = new T(obj);
    }
public:
    void copy(const T& obj) {
        copy_impl(obj, val_to_type<IsPolymorphic>());
    }
};

struct polymorphic {
    polymorphic* copy() const { 
        return new polymorphic(*this);
    }
};

int main(){
    instance_copyist<polymorphic, true> polymorphic_copyist;
    polymorphic_copyist.copy(polymorphic());

    instance_copyist<int, false> trivial_copyist;
    trivial_copyist.copy(int());
}
