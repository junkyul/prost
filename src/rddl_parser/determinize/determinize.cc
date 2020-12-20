#include "determinize.h"

#include "../evaluatables.h"
#include "../rddl.h"

#include "../utils/system.h"

#include <algorithm>

using namespace std;

namespace prost::parser::determinize {
void Determinizer::determinize() {
    map<ParametrizedVariable const*, LogicalExpression*> dummy;
    for (ConditionalProbabilityFunction* cpf : task->CPFs) {
        if (cpf->isProbabilistic()) {
            LogicalExpression* det = _determinize(cpf->formula);
            cpf->determinization = det->simplify(dummy);
        }
    }
}

LogicalExpression* MostLikelyDeterminizer::_determinize(
    LogicalExpression* formula) {
    if (dynamic_cast<ParametrizedVariable*>(formula) ||
        dynamic_cast<NumericConstant*>(formula)) {
        return formula;
    } else if (auto conn = dynamic_cast<Connective*>(formula)) {
        vector<LogicalExpression*> newExprs;
        for (LogicalExpression* expr : conn->exprs) {
            newExprs.push_back(_determinize(expr));
        }
        return conn->create(newExprs);
    } else if (auto neg = dynamic_cast<Negation*>(formula)) {
        return new Negation(_determinize(neg->expr));
    } else if (auto exp = dynamic_cast<ExponentialFunction*>(formula)) {
        return new ExponentialFunction(_determinize(exp->expr));
    } else if (auto bd = dynamic_cast<BernoulliDistribution*>(formula)) {
        vector<LogicalExpression*> newExprs = {new NumericConstant(0.5),
                                               _determinize(bd->expr)};
        return new LowerEqualsExpression(newExprs);
    } else if (auto disc = dynamic_cast<DiscreteDistribution*>(formula)) {
        // The most-likely determinization of a discrete distribution returns
        // the value with the highest probability. To do so, we generate a
        // multi condition checker with one condition / effect pair per value of
        // the discrete distribution, and the associated condition checks if the
        // probability of the value in the discrete distribution is higher than
        // the probabilities of all other values.
        //
        // Assume the discrete distribution p1 : e1; p2 : e2; p3 : e3,
        // this is turned in the multi condition checker:
        // if (p1 >= p2 && p1 >= p3) then e1
        // elif (p2 >= p1 && p2 >= p3) then e2
        // else e3
        vector<LogicalExpression*> detProbs;
        for (LogicalExpression* expr : disc->probabilities) {
            detProbs.push_back(_determinize(expr));
        }
        vector<LogicalExpression*> detVals;
        for (LogicalExpression* expr : disc->values) {
            detVals.push_back(_determinize(expr));
        }

        vector<LogicalExpression*> conditions;
        vector<LogicalExpression*> effects;
        for (size_t i = 0; i < detProbs.size(); ++i) {
            LogicalExpression* p1 = detProbs[i];
            vector<LogicalExpression*> conjuncts;
            for (LogicalExpression* p2 : detProbs) {
                if (p1 == p2) {
                    continue;
                }
                vector<LogicalExpression*> compare{p1, p2};
                conjuncts.push_back(new GreaterEqualsExpression(compare));
            }
            auto conjunction = new Conjunction(conjuncts);
            conditions.push_back(conjunction);
            effects.push_back(detVals[i]);
        }
        return new MultiConditionChecker(conditions, effects);
    } else if (auto ite = dynamic_cast<IfThenElseExpression*>(formula)) {
        return new IfThenElseExpression(_determinize(ite->condition),
                                        _determinize(ite->valueIfTrue),
                                        _determinize(ite->valueIfFalse));
    } else if (auto mcc = dynamic_cast<MultiConditionChecker*>(formula)) {
        vector<LogicalExpression*> newConds;
        vector<LogicalExpression*> newEffs;
        for (size_t i = 0; i < mcc->conditions.size(); ++i) {
            newConds.push_back(_determinize(mcc->conditions[i]));
            newEffs.push_back(_determinize(mcc->effects[i]));
        }
        return new MultiConditionChecker(newConds, newEffs);
    } else {
        utils::abort("Determinization of expression not implemented!");
    }
    return nullptr;
}
} // namespace prost::parser::determinize
