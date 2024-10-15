// RUN: rm %t1.ast.json || true
// RUN: %clangxx %s -emit-ast -o %t1.ast
// RUN: %cxx-langstat --analyses=tia -emit-features -in %t1.ast -out %t1.ast.json --
// RUN: sed -i '/^[[:space:]]*"GlobalLocation/d' %t1.ast.json
// RUN: diff %t1.ast.json %s.json
// Probably counterintuitive, but hear me out: Instantiations: Super<float> as VarDecl,
// Sub<int> and Sub<int> as FieldDecls. Sub<int> is counted twice, namely once inside
// the Super class template, and once inside of the Super<float> CTSD.

template<typename T>
class Sub {};

template<typename T>
class Super {
    Sub<int> w;
};
Super<float> s;
