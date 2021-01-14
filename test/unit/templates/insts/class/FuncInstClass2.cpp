// RUN: clang++ %s -emit-ast -o %t1.ast
// RUN: %S/../../../../../build/cxx-langstat --analyses=tia --store %t1.ast --
// RUN: diff %t1.ast.json %s.json

//
//
//

template<typename T>
class Widget {
};

// Test that multiple function instantiations don't cause multiple class insts.
template<typename T>
void f(){
    Widget<T> w;
}
void caller(){
    f<int>();
    f<int>();
}


// Test that explicit/implicit func inst order doesn't matter about class insts
template<typename T>
void f2(){
    Widget<T> w;
}
void caller2(){
    f2<short>();
}
template void f2<short>(); // Has no effect on class isnts

template<typename T>
void f3(){
    Widget<T> w;
}
template void f3<short>();
void caller3(){
    f3<short>(); // Has no effect on class isnts
}
