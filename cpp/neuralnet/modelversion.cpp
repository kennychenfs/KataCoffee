#include "../neuralnet/modelversion.h"
#include "../neuralnet/nninputs.h"

static void fail(int modelVersion) {
  throw StringError(
    "NNModelVersion: Model version not currently implemented or supported: " + Global::intToString(modelVersion));
}

static_assert(NNModelVersion::oldestModelVersionImplemented == 1, "");
static_assert(NNModelVersion::oldestInputsVersionImplemented == 1, "");
static_assert(NNModelVersion::latestModelVersionImplemented == 1, "");
static_assert(NNModelVersion::latestInputsVersionImplemented == 1, "");

int NNModelVersion::getInputsVersion(int modelVersion) {
  if(modelVersion >= 1 && modelVersion <= 1)
    return 1;

  fail(modelVersion);
  return -1;
}

int NNModelVersion::getNumSpatialFeatures(int modelVersion) {
  if(modelVersion >= 1 && modelVersion <= 1)
    return NNInputs::NUM_FEATURES_SPATIAL_V1;

  fail(modelVersion);
  return -1;
}

int NNModelVersion::getNumGlobalFeatures(int modelVersion) {
  if(modelVersion >= 1 && modelVersion <= 1)
    return NNInputs::NUM_FEATURES_GLOBAL_V1;

  fail(modelVersion);
  return -1;
}
