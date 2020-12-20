#ifndef PARSER_EVALUATABLES_H
#define PARSER_EVALUATABLES_H

#include <cassert>

#include "logical_expressions.h"
#include "probability_distribution.h"

namespace prost::parser {
struct ConditionalProbabilityFunction;

struct Evaluatable {
    Evaluatable(std::string _name, LogicalExpression* _formula)
        : name(_name),
          formula(_formula),
          determinization(nullptr),
          isProb(false),
          hasArithmeticFunction(false) {}

    // Initialization
    virtual void initialize();

    // Simplification
    virtual void simplify(Simplifications& replace);

    bool isActionIndependent() const {
        return dependentActionFluents.empty();
    }

    bool const& isProbabilistic() const {
        return isProb;
    }

    // A unique string that describes this (only used for print)
    std::string name;

    // The formula that is evaluatable
    LogicalExpression* formula;

    // The determinized version of formula
    // except probabilistic CPFs)
    LogicalExpression* determinization;

    // All evaluatables have a hash index that is used to quckly update the
    // state fluent hash key of this evaluatable
    int hashIndex;

    // The caching type that will be used (initially) for this evaluatable
    std::string cachingType;

    // If the caching type is VECTOR, this contains all possible results of
    // evaluating this (the first of the determinization, and if this is
    // probabilistic the second of formula)
    std::vector<double> precomputedResults;
    std::vector<DiscretePD> precomputedPDResults;

    // The kleene caching type that will be used (initially) for this
    // evaluatable and the size of the vector if kleeneCachingType is VECTOR
    std::string kleeneCachingType;
    int kleeneCachingVectorSize;

    // Properties of this Evaluatable
    std::set<StateFluent*> dependentStateFluents;
    std::set<ActionFluent*> dependentActionFluents;
    bool isProb;
    bool hasArithmeticFunction;

    // The actionHashKeyMap contains the hash keys of the actions that influence
    // this Evaluatable (these are added to the state fluent hash keys of a
    // state)
    std::vector<long> actionHashKeyMap;

    // The stateFluentHashKeyMap contains the state fluent hash key (base) of
    // each of the dependent state fluents
    std::vector<std::pair<int, long>> stateFluentHashKeyBases;
};

struct ActionPrecondition : public Evaluatable {
    ActionPrecondition(LogicalExpression* _formula)
        : Evaluatable("precond", _formula) {}

    bool containsStateFluent() const {
        return !dependentStateFluents.empty();
    }

    int index;
};

struct RewardFunction : public Evaluatable {
    RewardFunction(LogicalExpression* _formula)
        : Evaluatable("Reward", _formula) {}

    double minValue;
    double maxValue;
};

struct ConditionalProbabilityFunction : public Evaluatable {
    ConditionalProbabilityFunction(StateFluent* _head,
                                   LogicalExpression* _formula)
        : Evaluatable(_head->fullName, _formula),
          head(_head),
          kleeneDomainSize(0) {}

    int getDomainSize() const {
        return domain.size();
    }

    void setDomainSize(int numVals);

    void setIndex(int _index) {
        head->index = _index;
    }

    double getInitialValue() const {
        return head->initialValue;
    }

    StateFluent* head;

    // The values this CPF can take
    std::vector<int> domain;

    // Hashing of KleeneStates
    long kleeneDomainSize;
};
} // namespace prost::parser

#endif // PARSER_EVALUATABLES_H
