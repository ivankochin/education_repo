## Static polymorphism
Динамический полиморфизм в С++ реализуется с помощью виртуальных функций. Но выбор вызываемой функции происходит в run-time, и при определенных обстоятельствах может сказаться на скорости выполнения программы. Статический полиморфизм переносит выбор в compile-time, что позволяет избавиться от лишних вычислений.

### CRTP (crtp.cpp)

**CRTP (Curiously Recurring Template Pattern)** - это идиома проектирования, заключающаяся в том, что класс наследует от базового шаблонного класса с самим собой в качестве параметра шаблона базового класса. Данная идиома позволяет реализовать статический полиморфизм.

[Источник](https://habr.com/ru/post/210894/)

### MixIn (mixin.cpp)

**MixIn** - это прием проектирования, при котором класс (интерфейс, модуль и т.п.) реализует некоторую функциональность, которую можно “подмешать”, внести в другой класс. Самостоятельно же MixIn класс обычно не используется. Этот прием не является специфическим для С++, и в некоторых других языках он поддерживается на уровне языковых конструкций.

[Источник](https://stackoverflow.com/questions/18773367/what-are-mixins-as-a-concept)


