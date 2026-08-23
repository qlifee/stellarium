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

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelLocation.hpp"
#include "StelUtils.hpp"
#include "modules/Planet.hpp"
#include "modules/SolarSystem.hpp"

#include <cmath>

namespace
{
constexpr double radiansToDegrees = 180.0 / M_PI;

bool finiteBaseBodyState(double julianDayUt, double julianDayTt,
	double rightAscension, double declination, double azimuth,
	double altitude, double distanceAu, double angularDiameter)
{
	return std::isfinite(julianDayUt) && std::isfinite(julianDayTt) &&
		std::isfinite(rightAscension) && std::isfinite(declination) &&
		std::isfinite(azimuth) && std::isfinite(altitude) &&
		std::isfinite(distanceAu) && distanceAu > 0.0 &&
		std::isfinite(angularDiameter) && angularDiameter >= 0.0;
}
}

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

QVariantMap StelPluginAPI::getSolarSystemBodyStateSnapshot(
	const StelCore* core, const QString& bodyId)
{
	if(core == nullptr || bodyId.trimmed().isEmpty() ||
	   !StelApp::isInitialized())
	{
		return {};
	}

	SolarSystem* solarSystem = qobject_cast<SolarSystem*>(
		StelApp::getInstance().getModule(QStringLiteral("SolarSystem")));
	if(solarSystem == nullptr)
		return {};

	const QString normalizedId = bodyId.trimmed();
	PlanetP body;
	if(normalizedId.compare(QStringLiteral("Sun"),
	                        Qt::CaseInsensitive) == 0)
	{
		body = solarSystem->getSun();
	}
	else if(normalizedId.compare(QStringLiteral("Moon"),
	                             Qt::CaseInsensitive) == 0)
	{
		body = solarSystem->getMoon();
	}
	else
	{
		return {};
	}
	if(body.isNull())
		return {};

	const Vec3d j2000 = body->getJ2000EquatorialPos(core);
	double rightAscension = 0.0;
	double declination = 0.0;
	StelUtils::rectToSphe(&rightAscension, &declination, j2000);
	rightAscension = StelUtils::fmodpos(rightAscension, 2.0 * M_PI);

	const Vec3d geometricAltAz = body->getAltAzPosGeometric(core);
	double internalAzimuth = 0.0;
	double altitude = 0.0;
	StelUtils::rectToSphe(&internalAzimuth, &altitude, geometricAltAz);
	const double azimuth = StelUtils::fmodpos(
		M_PI - internalAzimuth, 2.0 * M_PI);

	const double distanceAu = j2000.norm();
	const double angularDiameter =
		2.0 * body->getAngularRadiusNoScale(core);
	const double julianDayUt = core->getJD();
	const double julianDayTt = core->getJDE();
	if(!finiteBaseBodyState(julianDayUt, julianDayTt, rightAscension,
	                        declination, azimuth, altitude, distanceAu,
	                        angularDiameter))
	{
		return {};
	}

	QVariantMap snapshot;
	snapshot.insert(QStringLiteral("schemaVersion"), 1);
	snapshot.insert(QStringLiteral("englishName"), body->getEnglishName());
	snapshot.insert(QStringLiteral("julianDayUt"), julianDayUt);
	snapshot.insert(QStringLiteral("julianDayTt"), julianDayTt);
	snapshot.insert(QStringLiteral("rightAscensionJ2000Degrees"),
	                rightAscension * radiansToDegrees);
	snapshot.insert(QStringLiteral("declinationJ2000Degrees"),
	                declination * radiansToDegrees);
	snapshot.insert(QStringLiteral("azimuthGeometricDegrees"),
	                azimuth * radiansToDegrees);
	snapshot.insert(QStringLiteral("altitudeGeometricDegrees"),
	                altitude * radiansToDegrees);
	snapshot.insert(QStringLiteral("distanceAu"), distanceAu);
	snapshot.insert(QStringLiteral("angularDiameterUnscaledDegrees"),
	                angularDiameter);
	snapshot.insert(QStringLiteral("aberrationEnabled"),
	                core->getUseAberration());
	snapshot.insert(QStringLiteral("topocentricCoordinatesEnabled"),
	                core->getUseTopocentricCoordinates());

	bool phaseDataAvailable = false;
	if(body != solarSystem->getSun())
	{
		const Vec3d observerPosition =
			core->getObserverHeliocentricEclipticPos();
		const double illuminatedFraction = body->getPhase(observerPosition);
		const double phaseAngle = body->getPhaseAngle(observerPosition);
		const double elongation = body->getElongation(observerPosition);
		phaseDataAvailable = std::isfinite(illuminatedFraction) &&
			illuminatedFraction >= 0.0 && illuminatedFraction <= 1.0 &&
			std::isfinite(phaseAngle) && phaseAngle >= 0.0 &&
			phaseAngle <= M_PI && std::isfinite(elongation) &&
			elongation >= 0.0 && elongation <= M_PI;
		if(phaseDataAvailable)
		{
			snapshot.insert(QStringLiteral("illuminatedFraction"),
			                illuminatedFraction);
			snapshot.insert(QStringLiteral("phaseAngleDegrees"),
			                phaseAngle * radiansToDegrees);
			snapshot.insert(QStringLiteral("elongationDegrees"),
			                elongation * radiansToDegrees);
		}
	}
	snapshot.insert(QStringLiteral("phaseDataAvailable"),
	                phaseDataAvailable);
	return snapshot;
}
