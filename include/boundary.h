#ifndef LLVM_BOUNDARY_ANALYSIS_H
#define LLVM_BOUNDARY_ANALYSIS_H

#include <llvm/ADT/MapVector.h>
#include <llvm/IR/AbstractCallSite.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

#include <set>

struct FunctionComplexity {
    long double Total;
    long double Io;
    long double Memory;

    FunctionComplexity();

    FunctionComplexity(const FunctionComplexity &) = default;

    FunctionComplexity(unsigned Total, unsigned Io);

    FunctionComplexity(long double Total, long double Io, long double Memory = 0.0);

    FunctionComplexity operator+(const FunctionComplexity &other) const;

    FunctionComplexity &operator+=(const FunctionComplexity &other);

    static FunctionComplexity IoAsComplexity(unsigned IoCalls);

    static FunctionComplexity IoAsComplexity(long double IoCalls);
};

using ResultIoComplexity = llvm::MapVector<const llvm::Function *, FunctionComplexity>;

struct BoundaryAnalysis : public llvm::AnalysisInfoMixin<BoundaryAnalysis> {
    using Result = ResultIoComplexity;

    BoundaryAnalysis(const std::set<std::string> &ExternalIoFunctionNames,
               const std::set<std::string> &ExternalMemoryFunctionNames) : ExternalIoFunctionNames(
            ExternalIoFunctionNames), ExternalMemoryFunctionNames(ExternalMemoryFunctionNames) {}

    Result run(llvm::Module &M, llvm::ModuleAnalysisManager &);

    Result runOnModule(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);

    void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    static bool isRequired() { return true; }

private:
    static llvm::AnalysisKey Key;
    friend struct llvm::AnalysisInfoMixin<BoundaryAnalysis>;
    std::set<std::string> ExternalIoFunctionNames;
    std::set<std::string> ExternalMemoryFunctionNames;
};

class BoundaryAnalysisPrinter
        : public llvm::PassInfoMixin<BoundaryAnalysisPrinter> {
public:
    explicit BoundaryAnalysisPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}

    llvm::PreservedAnalyses run(llvm::Module &M,
                                llvm::ModuleAnalysisManager &MAM);

    void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    static bool isRequired() { return true; }

private:
    llvm::raw_ostream &OS;
};

class BoundaryAnalysisDumper
        : public llvm::PassInfoMixin<BoundaryAnalysisDumper> {
public:
    explicit BoundaryAnalysisDumper(const llvm::StringRef &OutDirName) : OutDirName(OutDirName) {}

    llvm::PreservedAnalyses run(llvm::Module &M,
                                llvm::ModuleAnalysisManager &MAM);

    void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    static bool isRequired() { return true; }

private:
    llvm::StringRef OutDirName;
};

class BoundaryInstrument
        : public llvm::PassInfoMixin<BoundaryInstrument> {
public:
    explicit BoundaryInstrument() {}

    llvm::PreservedAnalyses run(llvm::Module &M,
                                llvm::ModuleAnalysisManager &MAM);

    void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    static bool isRequired() { return true; }
};

#endif // LLVM_BOUNDARY_ANALYSIS_H