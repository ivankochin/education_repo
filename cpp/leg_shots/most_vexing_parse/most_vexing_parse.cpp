class Timer {
 public:
  Timer();
};

class TimeKeeper {
 public:
  TimeKeeper(const Timer& t);

  int get_time();
};

int main() {
  TimeKeeper time_keeper(Timer()); // this is not TimeKeeper constructions, this is function forward declarationvoid f(double adouble) {
  int i(int(adouble)); // this is function forward declaration too.

  return 0;
}