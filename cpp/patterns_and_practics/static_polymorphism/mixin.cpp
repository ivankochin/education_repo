#include <iostream>

struct Number
{
  typedef int value_type;
  int n;
  void set(int v) { n = v; }
  int get() const { return n; }
};

template <typename base, typename T = typename base::value_type>
struct Undoable : public base
{
  typedef T value_type;
  T before;
  void set(T v) { before = base::get(); base::set(v); }
  void undo() { base::set(before); }
};

template <typename base, typename T = typename base::value_type>
struct Redoable : public base
{
  typedef T value_type;
  T after;
  void set(T v) { after = v; base::set(v); }
  void redo() { base::set(after); }
};

typedef Redoable< Undoable<Number> > ReUndoableNumber;

int main()
{
  ReUndoableNumber mynum;
  mynum.set(42); mynum.set(84);
  std::cout << mynum.get() << std::endl;  // 84
  mynum.undo();
  std::cout << mynum.get() << std::endl;  // 42
  mynum.redo();
  std::cout << mynum.get() << std::endl;  // back to 84
}
