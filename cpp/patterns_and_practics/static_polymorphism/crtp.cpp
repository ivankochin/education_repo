#include <iostream>

template<typename D>
struct base {
    void pech() {
        static_cast<D*>(this)->pech_impl();
    }
    void pech_impl() {
        std::cout<<"Pech Base"<<std::endl;
    }
};

struct derived1 : base<derived1> {
    void pech_impl() {
        std::cout<<"Pech 1"<<std::endl;
    }
};

struct derived2 : base<derived2> {
    void pech_impl() {
        std::cout<<"Pech 2"<<std::endl;
    }
};

template<typename T>
void polymorhic_pech(T& w) {
    w.pech();
}


int main()
{
    derived1 d1;
    polymorhic_pech(d1);
    
    derived2 d2;
    polymorhic_pech(d2);
}
