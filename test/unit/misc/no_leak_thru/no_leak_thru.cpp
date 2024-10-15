// rm Output/nlt.ast.json || true
// rm Output/empty.ast.json || true
// RUN: %clangxx %s -emit-ast -o Output/nlt.ast
// RUN: %clangxx %S/empty.cpp -emit-ast -o Output/empty.ast
// RUN: %cxx-langstat --analyses=ala,cca,cea,cla,fpa,lda,lka,msa,tia,tpa,ua,ula,vta -emit-features -in Output/nlt.ast -in Output/empty.ast -outdir Output/ --
// RUN: cat Output/empty.ast.json
// RUN: diff Output/empty.ast.json %S/empty.cpp.json

// Test to test that when input are multiple files, that features from first
// file don't accidentally transfer to features of second file. Analyses retain
// their state until they run on a new file. This used to go south
// with LDA, FPA because those analyses didn't correctly overwrite old state.

#include <vector>
#include <utility>
#include <algorithm>


template<int N> // tpa
class C {};
template class C<0>; // tia

void func(C<1> c){}

int main(int argc, char** argv){ // cca, fpa
    for(;;){ // lda, lka

    }
    typedef int newint; // ua
    std::vector<int> v; // cla
    std::pair<int, int> twoints; // ula
    int a = std::max(twoints.first, twoints.second); // ala
    C<1> c;
    func(c); //msa
}

template<int N> // vta
int n = N;
