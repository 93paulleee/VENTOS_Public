//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#include "src/msg/AirFrame_m.h"
#include "LogNormalShadowing.h"
#include "Mapping.h"

LogNormalShadowing::LogNormalShadowing(double mean, double stdDev, omnetpp::simtime_t_cref interval):
	mean(mean), stdDev(stdDev), interval(interval)
{ }

LogNormalShadowing::~LogNormalShadowing() {}

double LogNormalShadowing::randomLogNormalGain() const {
	return FWMath::dBm2mW(-1.0 * omnetpp::cSimulation::getActiveSimulation()->getContext()->normal(mean, stdDev));
}

void LogNormalShadowing::filterSignal(AirFrame *frame, const Coord& sendersPos, const Coord& receiverPos) {
	Signal&   signal = frame->getSignal();
	omnetpp::simtime_t start  = signal.getReceptionStart();
	omnetpp::simtime_t end    = signal.getReceptionEnd();
	Mapping*  att    = MappingUtils::createMapping(DimensionSet::timeDomain(), Mapping::LINEAR);

	Argument pos;

	for(omnetpp::simtime_t t = start; t <= end; t += interval)
	{
		pos.setTime(t);
		att->appendValue(pos, randomLogNormalGain());
	}

	signal.addAttenuation(att);
}
