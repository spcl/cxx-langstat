// RUN: %clangxx %s -emit-ast -o %t1.ast -std=c++17
// RUN: %cxx-langstat --analyses=msa -emit-features -in %t1.ast -out %t1.ast.json -- -std=c++17
// RUN: sed -i '/^[[:space:]]*"GlobalLocation/d' %t1.ast.json
// RUN: diff %t1.ast.json %s.json
// RUN: rm %t1.ast.json || true

// Test that aliases form no barrier to MSA.
//
//

#include <utility>
#include <iostream>

struct C{
public:
    C(){std::cout << "ctor\n";}
};
using D = C;
typedef C E;

void func(C c){
}
void funcD(D d){
}
void funcE(E e){
}


int main(int argc, char** argv){
    C c;
    D d;
    E e;
    // These should all result in copy ctor calls
    func(c);
    func(d);
    func(e);
    funcD(c);
    funcD(d);
    funcD(e);
    funcE(c);
    funcE(d);
    funcE(e);
}
