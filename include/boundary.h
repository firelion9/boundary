#ifndef LLVM_BOUNDARY_ANALYSIS_H
#define LLVM_BOUNDARY_ANALYSIS_H

#include <llvm/ADT/MapVector.h>
#include <llvm/IR/AbstractCallSite.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>

#include <set>

using ComplexityScalarType = long double;

struct FunctionComplexity {
    ComplexityScalarType Total;
    ComplexityScalarType Io;
    ComplexityScalarType Memory;

    constexpr FunctionComplexity();

    constexpr FunctionComplexity(const FunctionComplexity &) = default;

    constexpr FunctionComplexity(unsigned Total, unsigned Io);

    constexpr FunctionComplexity(ComplexityScalarType Total, ComplexityScalarType Io, ComplexityScalarType Memory = 0.0);

    constexpr FunctionComplexity operator+(const FunctionComplexity &other) const;

    constexpr FunctionComplexity &operator+=(const FunctionComplexity &other);

    constexpr ComplexityScalarType Cpu() const;

    static constexpr FunctionComplexity IoAsComplexity(unsigned IoCalls);

    static constexpr FunctionComplexity IoAsComplexity(ComplexityScalarType IoCalls);
};

using ResultIoComplexity = llvm::MapVector<const llvm::Function *, FunctionComplexity>;

struct BoundaryAnalysis : public llvm::AnalysisInfoMixin<BoundaryAnalysis> {
    using Result = ResultIoComplexity;

    BoundaryAnalysis(const std::map<std::string, FunctionComplexity> &ExternalComplexityInfo) :
            ExternalComplexityInfo(ExternalComplexityInfo) {}

    Result run(llvm::Module &M, llvm::ModuleAnalysisManager &);

    Result runOnModule(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);

    void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    static bool isRequired() { return true; }

private:
    static llvm::AnalysisKey Key;
    friend struct llvm::AnalysisInfoMixin<BoundaryAnalysis>;
    std::map<std::string, FunctionComplexity> ExternalComplexityInfo;
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