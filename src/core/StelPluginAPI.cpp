/*
 * Stellarium
 * Copyright (C) 2026 Stellarium Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "StelPluginAPI.hpp"

#include "StelCore.hpp"
#include "StelLocation.hpp"

QVariantMap StelPluginAPI::getCoreStateSnapshot(const StelCore* core)
{
	if(core == nullptr)
		return {};

	const double julianDayUt = core->getJD();
	const StelLocation& location = core->getCurrentLocation();

	QVariantMap snapshot;
	snapshot.insert(QStringLiteral("schemaVersion"), 1);
	snapshot.insert(QStringLiteral("julianDayUt"), julianDayUt);
	snapshot.insert(QStringLiteral("julianDayTt"), core->getJDE());
	snapshot.insert(QStringLiteral("deltaTSeconds"), core->getDeltaT());
	snapshot.insert(QStringLiteral("utcOffsetHours"),
	                core->getUTCOffset(julianDayUt));
	snapshot.insert(QStringLiteral("timeRateJdPerSecond"),
	                core->getTimeRate());
	snapshot.insert(QStringLiteral("currentTimeZone"),
	                core->getCurrentTimeZone());
	snapshot.insert(QStringLiteral("locationTimeZone"),
	                location.ianaTimeZone);
	snapshot.insert(QStringLiteral("locationId"), location.getID());
	snapshot.insert(QStringLiteral("locationValid"), location.isValid());
	snapshot.insert(QStringLiteral("longitudeDegrees"),
	                static_cast<double>(location.getLongitude()));
	snapshot.insert(QStringLiteral("latitudeDegrees"),
	                static_cast<double>(location.getLatitude()));
	snapshot.insert(QStringLiteral("altitudeMeters"), location.altitude);
	snapshot.insert(QStringLiteral("planetName"), location.planetName);
	return snapshot;
}
