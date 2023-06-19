#include "../search/reportedsearchvalues.h"

#include "../neuralnet/nninputs.h"
#include "../search/search.h"

ReportedSearchValues::ReportedSearchValues()
{}
ReportedSearchValues::~ReportedSearchValues()
{}
ReportedSearchValues::ReportedSearchValues(
  const Search& search,
  double winLossValueAvg,
  double utilityAvg,
  double totalWeight,
  int64_t totalVisits
) {
  winLossValue = winLossValueAvg;
  utility = utilityAvg;

  //Clamp. Due to tiny floating point errors, these could be outside range.
  if(winLossValue < -1.0) winLossValue = -1.0;
  if(winLossValue > 1.0) winLossValue = 1.0;

  winValue = 0.5 * winLossValue;
  lossValue = 0.5 * -winLossValue;

  //Handle float imprecision
  if(winValue < 0.0) winValue = 0.0;
  if(winValue > 1.0) winValue = 1.0;
  if(lossValue < 0.0) lossValue = 0.0;
  if(lossValue > 1.0) lossValue = 1.0;

  weight = totalWeight;
  visits = totalVisits;
}

std::ostream& operator<<(std::ostream& out, const ReportedSearchValues& values) {
  out << "winValue " << values.winValue << "\n";
  out << "lossValue " << values.lossValue << "\n";
  out << "winLossValue " << values.winLossValue << "\n";
  out << "utility " << values.utility << "\n";
  out << "weight " << values.weight << "\n";
  out << "visits " << values.visits << "\n";
  return out;
}
