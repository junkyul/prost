#include "iterative_deepening_search.h"

#include "prost_planner.h"
#include "depth_first_search.h"
#include "minimal_lookahead_search.h"

#include "utils/math_utils.h"
#include "utils/system_utils.h"

#include <iostream>
#include <algorithm>

using namespace std;

/******************************************************************
                     Search Engine Creation
******************************************************************/

IDS::HashMap IDS::rewardCache;

IDS::IDS() :
    DeterministicSearchEngine("IDS"),
    currentState(State()),
    isLearning(false),
    timer(),
    mls(nullptr),
    maxSearchDepthForThisStep(0),
    ramLimitReached(false),
    strictTerminationTimeout(0.1),
    terminateWithReasonableAction(true),
    accumulatedSearchDepth(0),
    cacheHits(0),
    numberOfRuns(0) {

    setTimeout(0.005);

    if (rewardCache.bucket_count() < 520241) {
        rewardCache.reserve(520241);
    }

    elapsedTime.resize(maxSearchDepth + 1);

    dfs = new DepthFirstSearch();
    dfs->setMaxSearchDepth(maxSearchDepth);
}

bool IDS::setValueFromString(string& param, string& value) {
    if (param == "-st") {
        setStrictTerminationTimeout(atof(value.c_str()));
        return true;
    } else if (param == "-tra") {
        setTerminateWithReasonableAction(atoi(value.c_str()));
        return true;
    }

    return SearchEngine::setValueFromString(param, value);
}

/******************************************************************
                            Parameter
******************************************************************/

void IDS::setMaxSearchDepth(int newValue) {
    dfs->setMaxSearchDepth(newValue);
    SearchEngine::setMaxSearchDepth(newValue);
    elapsedTime.resize(newValue + 1);
}

void IDS::setCachingEnabled(bool newValue) {
    SearchEngine::setCachingEnabled(newValue);
    dfs->setCachingEnabled(newValue);
}

/******************************************************************
                 Search Engine Administration
******************************************************************/

void IDS::disableCaching() {
    dfs->disableCaching();
    SearchEngine::disableCaching();
    ramLimitReached = true;
}

void IDS::learn() {
    dfs->learn();
    cout << name << ": learning..." << endl;

    isLearning = true;
    bool cachingEnabledBeforeLearning = cachingIsEnabled();
    cachingEnabled = false;

    // Perform IDS for all states in trainingSet and record the time it takes
    for (size_t i = 0; i < trainingSet.size(); ++i) {
        State copy(trainingSet[i]);
        vector<double> res(numberOfActions);

        vector<int> actionsToExpand = getApplicableActions(copy);

        estimateQValues(copy, actionsToExpand, res);
    }

    isLearning = false;
    cachingEnabled = cachingEnabledBeforeLearning;
    assert(rewardCache.empty());

    if (maxSearchDepth > 1) {
        // Determine the maximal search depth based on the average time the
        // search needed on the training set
        maxSearchDepth = 0;

        for (size_t index = 2; index < elapsedTime.size(); ++index) {
            if (elapsedTime[index].size() > (trainingSet.size() / 2)) {
                double timeSum = 0.0;
                for (size_t j = 0; j < elapsedTime[index].size(); ++j) {
                    timeSum += elapsedTime[index][j];
                }
                double avgTime = timeSum / ((double) elapsedTime[index].size());
                cout << name << ": Search Depth " << index << ": " << timeSum
                     << " / " << elapsedTime[index].size() << " = "
                     << avgTime << endl;
                if (MathUtils::doubleIsSmaller(avgTime, timeout)) {
                    maxSearchDepth = index;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        setMaxSearchDepth(maxSearchDepth);
        cout << name << ": Setting max search depth to "
             << maxSearchDepth << "!" << endl;
    } else {
        mls = new MinimalLookaheadSearch();

        cout << "Replacing this with a MLS." << endl;
    }
    resetStats();
    cout << name << ": ...finished" << endl;
}

/******************************************************************
                       Main Search Functions
******************************************************************/

void IDS::estimateQValue(State const& state, int actionIndex, double& qValue) {
    HashMap::iterator it = rewardCache.find(state);
    if (it != rewardCache.end() &&
        !MathUtils::doubleIsMinusInfinity(it->second[actionIndex])) {
        ++cacheHits;
        qValue = it->second[actionIndex] * (double)state.stepsToGo();
    }  else if(mls) {     
        mls->estimateQValue(state, actionIndex, qValue);
    } else {
        timer.reset();

        maxSearchDepthForThisStep = std::min(maxSearchDepth, state.stepsToGo());

        currentState.setTo(state);
        currentState.stepsToGo() = 1;
        do {
            ++currentState.stepsToGo();
            dfs->estimateQValue(currentState, actionIndex, qValue);
        }  while (moreIterations());

        qValue /= ((double) currentState.stepsToGo());

        // TODO: Currently, we cache every result, but we should only do so if
        // the result was achieved with a reasonable action, with a timeout or
        // on a state with sufficient depth
        if (cachingEnabled) {
            if (it == rewardCache.end()) {
                rewardCache[currentState] =
                    vector<double>(SearchEngine::numberOfActions,
                                   -std::numeric_limits<double>::max());
            }
            rewardCache[currentState][actionIndex] = qValue;
        }
        qValue *= (double)state.stepsToGo();

        accumulatedSearchDepth += currentState.stepsToGo();
        ++numberOfRuns;
    }
}

void IDS::estimateQValues(State const& state,
                          vector<int> const& actionsToExpand,
                          vector<double>& qValues) {
    HashMap::iterator it = rewardCache.find(state);
    if (it != rewardCache.end()) {
        ++cacheHits;
        assert(qValues.size() == it->second.size());
        for (size_t index = 0; index < qValues.size(); ++index) {
            if (actionsToExpand[index] == index) {
                qValues[index] = it->second[index] * (double)state.stepsToGo();
            } else {
                qValues[index] = -std::numeric_limits<double>::max();
            }
        }
    } else if (mls) {
        mls->estimateQValues(state, actionsToExpand, qValues);
    } else {
        timer.reset();

        maxSearchDepthForThisStep = std::min(maxSearchDepth, state.stepsToGo());

        currentState.setTo(state);
        currentState.stepsToGo() = 1;
        do {
            ++currentState.stepsToGo();
            dfs->estimateQValues(currentState, actionsToExpand, qValues);
        }  while (moreIterations(actionsToExpand, qValues));

        double multiplier =
            (double)state.stepsToGo() / (double)currentState.stepsToGo();
        // TODO: Currently, we cache every result, but we should only do so if
        // the result was achieved with a reasonable action, with a timeout or
        // on a state with sufficient depth
        if (cachingEnabled) {
            rewardCache[currentState] = qValues;
            for (size_t index = 0; index < qValues.size(); ++index) {
                if (actionsToExpand[index] == index) {
                    qValues[index] *= multiplier;
                    rewardCache[currentState][index] /=
                        (double)currentState.stepsToGo();
                }
            }
        } else {
            for (size_t index = 0; index < qValues.size(); ++index) {
                if (actionsToExpand[index] == index) {
                    qValues[index] *= multiplier;
                }
            }
        }

        accumulatedSearchDepth += currentState.stepsToGo();
        ++numberOfRuns;
    }
}

bool IDS::moreIterations() {
    double time = timer();

    // 1. If caching was disabled, we check if the strict timeout is violated to
    // readjust the maximal search depth
    if (ramLimitReached &&
        MathUtils::doubleIsGreater(time, strictTerminationTimeout)) {
        if (currentState.stepsToGo() == 1) {
            mls = new MinimalLookaheadSearch();
            cout << name << ": Timeout violated (" << time
                 << "s) on minimal search depth. Replacing this with MLS." << endl;
        } else {
            cout << name << ": Timeout violated (" << time
                 << "s). Setting max search depth to "
                 << (currentState.stepsToGo() - 1) << "!" << endl;
            setMaxSearchDepth(currentState.stepsToGo() - 1);
        }
        return false;
    }

    // 2. Check if we have reached the max search depth for this step
    return currentState.stepsToGo() < maxSearchDepthForThisStep;
}

bool IDS::moreIterations(vector<int> const& actionsToExpand,
                         vector<double>& qValues) {
    double time = timer();

    // 0. If we are learning, we apply different termination criteria
    if (isLearning) {
        elapsedTime[currentState.stepsToGo()].push_back(time);

        if (MathUtils::doubleIsGreater(time, strictTerminationTimeout)) {
            elapsedTime.resize(currentState.stepsToGo());
            maxSearchDepth = std::max(currentState.stepsToGo() - 1, 1);
            return false;
        }
        return currentState.stepsToGo() < maxSearchDepthForThisStep;
    }

    // 1. If caching was disabled, we check if the strict timeout is violated to
    // readjust the maximal search depth
    if (ramLimitReached &&
        MathUtils::doubleIsGreater(time, strictTerminationTimeout)) {
        if (currentState.stepsToGo() == 1) {
            mls = new MinimalLookaheadSearch();
            cout << name << ": Timeout violated (" << time
                 << "s) on minimal search depth. Replacing this with MLS." << endl;
        } else {
            cout << name << ": Timeout violated (" << time
                 << "s). Setting max search depth to "
                 << (currentState.stepsToGo() - 1) << "!" << endl;
            setMaxSearchDepth(currentState.stepsToGo() - 1);
        }
        return false;
    }

    // 2. Check if the result is already significant (if noop is applicable, we
    // check if there is an action that yields a higher reward than noop)
    if (terminateWithReasonableAction &&
        actionStates[0].scheduledActionFluents.empty() &&
        (actionsToExpand[0] == 0)) {
        for (size_t index = 1; index < qValues.size(); ++index) {
            if ((actionsToExpand[index] == index) &&
                MathUtils::doubleIsGreater(qValues[index], qValues[0])) {
                return false;
            }
        }
    }

    // 3. Check if we have reached the max search depth for this step
    return currentState.stepsToGo() < maxSearchDepthForThisStep;
}

/******************************************************************
                   Statistics and Prints
******************************************************************/

void IDS::resetStats() {
    accumulatedSearchDepth = 0;
    cacheHits = 0;
    numberOfRuns = 0;
}

void IDS::printStats(ostream& out,
                     bool const& printRoundStats,
                     string indent) const {
    SearchEngine::printStats(out, printRoundStats, indent);
    if (numberOfRuns > 0) {
        out << indent << "Average search depth: "
            << ((double) accumulatedSearchDepth / (double) numberOfRuns)
            << " (in " << numberOfRuns << " runs)" << endl;
    }
    out << indent << "Maximal search depth: " << maxSearchDepth << endl;
    out << indent << "Cache hits: " << cacheHits << endl;
}
