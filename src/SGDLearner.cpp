#include "sgdtk/SGDLearner.h"
#include <iostream>

using namespace sgdtk;

Model *SGDLearner::create(void *p)
{
    this->learningRateSchedule->reset(eta0, lambda);
    return this->modelFactory->newInstance(p);
}

void SGDLearner::trainOne(Model *model, const FeatureVector *fv)
{
    LinearModel *lm = (LinearModel *) model;
    double eta = learningRateSchedule->update();
   //double eta = eta0 / (1 + lambda * eta0 * numSeenTotal);

    double y = fv->getY();
    double fx = lm->predict(fv);
    double dLoss = lossFunction->dLoss(fx, y);

    lm->updateWeights(fv->getX(), eta, lambda, dLoss, y);
}

void SGDLearner::preprocess(Model *model, const std::vector<FeatureVector *> &sample)
{
    double lowEta = LOW_ETA_0;
    double lowCost = evalEta(model, sample, lowEta);
    double highEta = lowEta * ETA_FACTOR;
    double highCost = evalEta(model, sample, highEta);
    if (lowCost < highCost)
    {
        while (lowCost < highCost)
        {
            highEta = lowEta;
            highCost = lowCost;
            lowEta = highEta / ETA_FACTOR;
            lowCost = evalEta(model, sample, lowEta);
        }
    }
    else if (highCost < lowCost)
    {
        while (highCost < lowCost)
        {
            lowEta = highEta;
            lowCost = highCost;
            highEta = lowEta * ETA_FACTOR;
            highCost = evalEta(model, sample, highEta);
        }
    }
    eta0 = lowEta;
    std::cout << "picked: " << eta0 << " in eta0" << std::endl;
}


double SGDLearner::evalEta(Model *model, const std::vector<FeatureVector *> &sample, double eta)
{
    auto *clone = (LinearModel *) model->prototype();

    for (size_t i = 0, sz = sample.size(); i < sz; ++i)
    {
        const auto* fv = sample[i];
        auto y = fv->getY();
        auto fx = clone->predict(fv);
        auto dLoss = lossFunction->dLoss(fx, y);
        clone->updateWeights(fv->getX(), eta, lambda, dLoss, y);

    }

    Metrics metrics;
    eval(clone, sample, metrics);
    return metrics.getCost();
}

Model *SGDLearner::trainEpoch(Model *model,
                              const std::vector<FeatureVector *> &trainingExamples)
{
    if (eta0 <= 0)
    {

        // get samples from trainingExamples
        size_t nSamples = std::min<size_t>(1000, trainingExamples.size());

        std::vector<FeatureVector *> samples(nSamples);
        for (size_t i = 0; i < nSamples; ++i)
        {
            samples[i] = trainingExamples[i];
        }
        preprocess(model, samples);
    }


    for (auto example : trainingExamples)
    {
        //double eta = eta0 / (1 + lambda * eta0 * numSeenTotal);
        trainOne(model, example);
        //++numSeenTotal;
    }

    auto *lm = (LinearModel *) model;

    std::cout << "wNorm=" << lm->mag() << std::endl;
    return model;

}

double SGDLearner::evalOne(Model *model, const FeatureVector *fv, Metrics &metrics)
{
    auto y = fv->getY();
    auto fx = model->predict(fv);
    auto loss = lossFunction->loss(fx, y);
    auto error = (fx * y <= 0) ? 1 : 0;
    metrics.add(loss, error);
    return fx;
}

void SGDLearner::eval(Model *model,
                      const std::vector<FeatureVector *> &testingExamples,
                      Metrics &metrics)
{
    for (auto fv : testingExamples)
    {
        evalOne(model, fv, metrics);
    }

    auto *lm = (LinearModel *) model;
    auto normW = lm->mag();
    auto cost = metrics.getLoss() + 0.5 * lambda * normW;
    metrics.setCost(cost);
}