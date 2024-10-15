// RUN: %clangxx %s -emit-ast -o %t1.ast -std=c++17
// RUN: %cxx-langstat --analyses=msa -emit-features -in %t1.ast -out %t1.ast.json -- -std=c++17
// RUN: sed -i '/^[[:space:]]*"GlobalLocation/d' %t1.ast.json
// RUN: diff %t1.ast.json %s.json
// RUN: rm %t1.ast.json || true

// Test copying/moving of vector. Needs that weird CXXBindTemporaryExpr in the
// clang AST, don't know why.
//

#include <vector>

void func(std::vector<int> v){

}

int main(int argc, char** argv){
    std::vector<int> v = {1};
    func(std::move(v));
    v.emplace_back(2);
}
