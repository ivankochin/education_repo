class singleton {
    singleton(){}
    singleton(const singleton&) = delete;
    singleton(singleton&&) = delete;
    singleton& operator=(const singleton&) = delete;
    singleton& operator=(singleton&&) = delete;
public:
    static singleton& instance() {
        static singleton obj;
        return obj;
    }
};
