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
#include "StelObserver.hpp"
#include "StelUtils.hpp"
#include "modules/Planet.hpp"
#include "modules/SolarSystem.hpp"
#include "planetsephems/precession.h"
#include "planetsephems/sidereal_time.h"

#include <QCoreApplication>
#include <QThread>

#include <algorithm>
#include <cmath>

namespace
{
constexpr double radiansToDegrees = 180.0 / M_PI;
constexpr double degreesToRadians = M_PI / 180.0;
constexpr double j2000JulianDay = 2451545.0;
constexpr double maximumArbitrarySampleSpanDays = 2000.0 * 36525.0;

bool supportedArbitrarySampleEpoch(double julianDay)
{
	return std::isfinite(julianDay) &&
		std::abs(julianDay - j2000JulianDay) <=
			maximumArbitrarySampleSpanDays;
}

bool finiteVector(const Vec3d& value)
{
	return std::isfinite(value[0]) && std::isfinite(value[1]) &&
		std::isfinite(value[2]);
}

bool isMainThread()
{
	const QCoreApplication* application = QCoreApplication::instance();
	return application != nullptr &&
		QThread::currentThread() == application->thread();
}

bool resolveSunMoonEarth(const StelCore* core, SolarSystem*& solarSystem,
	PlanetP& sun, PlanetP& moon, PlanetP& earth)
{
	if(core == nullptr || !StelApp::isInitialized() || !isMainThread())
		return false;

	StelApp& app = StelApp::getInstance();
	if(app.getCore() != core)
		return false;

	solarSystem = qobject_cast<SolarSystem*>(
		app.getModule(QStringLiteral("SolarSystem")));
	if(solarSystem == nullptr)
		return false;

	sun = solarSystem->getSun();
	moon = solarSystem->getMoon();
	earth = solarSystem->getEarth();
	return !sun.isNull() && !moon.isNull() && !earth.isNull();
}

bool computeGeometricEarthMoonState(const PlanetP& earth,
	const PlanetP& moon, double julianDayTt, Vec3d& earthHeliocentric,
	Vec3d& moonGeocentric)
{
	Vec3d earthVelocity;
	Vec3d moonVelocity;
	earth->computePosition(julianDayTt, earthHeliocentric, earthVelocity);
	moon->computePosition(julianDayTt, moonGeocentric, moonVelocity);
	return finiteVector(earthHeliocentric) && finiteVector(earthVelocity) &&
		finiteVector(moonGeocentric) && finiteVector(moonVelocity) &&
		earthHeliocentric.normSquared() > 0.0 &&
		moonGeocentric.normSquared() > 0.0;
}

double normalizedSignedDegrees(double radians)
{
	double normalized = std::remainder(radians, 2.0 * M_PI);
	if(normalized <= -M_PI)
		normalized += 2.0 * M_PI;
	return normalized * radiansToDegrees;
}

struct HorizontalCoordinates
{
	double azimuthDegrees = 0.0;
	double altitudeDegrees = 0.0;
};

bool horizontalCoordinates(const Vec3d& position,
	HorizontalCoordinates& result)
{
	if(!finiteVector(position) || position.normSquared() <= 0.0)
		return false;

	double internalAzimuth = 0.0;
	double altitude = 0.0;
	StelUtils::rectToSphe(&internalAzimuth, &altitude, position);
	const double compassAzimuth = StelUtils::fmodpos(
		M_PI - internalAzimuth, 2.0 * M_PI);
	result.azimuthDegrees = compassAzimuth * radiansToDegrees;
	result.altitudeDegrees = altitude * radiansToDegrees;
	return std::isfinite(result.azimuthDegrees) &&
		result.azimuthDegrees >= 0.0 && result.azimuthDegrees < 360.0 &&
		std::isfinite(result.altitudeDegrees) &&
		result.altitudeDegrees >= -90.0 &&
		result.altitudeDegrees <= 90.0;
}

Mat4d earthEquatorialToVsop87At(double julianDayTt)
{
	double epsilon = 0.0;
	double chi = 0.0;
	double omega = 0.0;
	double psi = 0.0;
	getPrecessionAnglesVondrakUncached(
		julianDayTt, &epsilon, &chi, &omega, &psi);
	Mat4d matrix = Mat4d::zrotation(-psi) *
		Mat4d::xrotation(-omega) * Mat4d::zrotation(chi);

	double deltaPsi = 0.0;
	double deltaEpsilon = 0.0;
	getNutationAnglesUncached(
		julianDayTt, &deltaPsi, &deltaEpsilon);
	const Mat4d nutation = Mat4d::xrotation(epsilon) *
		Mat4d::zrotation(-deltaPsi) *
		Mat4d::xrotation(-epsilon - deltaEpsilon);
	return matrix * nutation;
}

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

QVariantMap StelPluginAPI::getMoonSunConjunctionSampleAtJulianDayTt(
	const StelCore* core, double julianDayTt)
{
	if(!supportedArbitrarySampleEpoch(julianDayTt))
		return {};

	SolarSystem* solarSystem = nullptr;
	PlanetP sun;
	PlanetP moon;
	PlanetP earth;
	if(!resolveSunMoonEarth(core, solarSystem, sun, moon, earth))
		return {};
	Q_UNUSED(solarSystem)
	Q_UNUSED(sun)

	Vec3d earthHeliocentric;
	Vec3d moonGeocentric;
	if(!computeGeometricEarthMoonState(
		   earth, moon, julianDayTt, earthHeliocentric, moonGeocentric))
	{
		return {};
	}

	const double moonLongitude =
		std::atan2(moonGeocentric[1], moonGeocentric[0]);
	const Vec3d sunGeocentric = -earthHeliocentric;
	const double sunLongitude =
		std::atan2(sunGeocentric[1], sunGeocentric[0]);
	const double longitudeDifference = normalizedSignedDegrees(
		moonLongitude - sunLongitude);
	if(!std::isfinite(longitudeDifference) ||
	   longitudeDifference <= -180.0 || longitudeDifference > 180.0)
	{
		return {};
	}

	QVariantMap sample;
	sample.insert(QStringLiteral("schemaVersion"), 1);
	sample.insert(QStringLiteral("julianDayTt"), julianDayTt);
	sample.insert(
		QStringLiteral(
			"moonSunGeocentricEclipticLongitudeDifferenceDegrees"),
		longitudeDifference);
	return sample;
}

QVariantMap StelPluginAPI::getSunMoonVisibilitySampleAtJulianDayUt(
	const StelCore* core, double julianDayUt)
{
	if(!supportedArbitrarySampleEpoch(julianDayUt))
		return {};

	SolarSystem* solarSystem = nullptr;
	PlanetP sun;
	PlanetP moon;
	PlanetP earth;
	if(!resolveSunMoonEarth(core, solarSystem, sun, moon, earth))
		return {};
	Q_UNUSED(solarSystem)
	Q_UNUSED(sun)

	const StelObserver* observer = core->getCurrentObserver();
	if(observer == nullptr || observer->isTraveling() ||
	   core->getCurrentPlanet() != earth)
	{
		return {};
	}

	const StelLocation& location = core->getCurrentLocation();
	if(!location.isValid())
		return {};
	const double longitudeDegrees =
		static_cast<double>(location.getLongitude());
	const double latitudeDegrees =
		static_cast<double>(location.getLatitude());
	if(!std::isfinite(longitudeDegrees) ||
	   longitudeDegrees < -180.0 || longitudeDegrees > 180.0 ||
	   !std::isfinite(latitudeDegrees) ||
	   latitudeDegrees < -90.0 || latitudeDegrees > 90.0)
	{
		return {};
	}

	const double deltaTSeconds = core->computeDeltaTReadOnly(julianDayUt);
	const double julianDayTt =
		julianDayUt + deltaTSeconds / 86400.0;
	if(!std::isfinite(deltaTSeconds) ||
	   !supportedArbitrarySampleEpoch(julianDayTt))
		return {};

	Vec3d earthHeliocentric;
	Vec3d moonGeocentric;
	if(!computeGeometricEarthMoonState(
		   earth, moon, julianDayTt, earthHeliocentric, moonGeocentric))
	{
		return {};
	}

	const Mat4d equatorialToVsop87 =
		earthEquatorialToVsop87At(julianDayTt);
	const double siderealTimeDegrees =
		get_apparent_sidereal_time_uncached(julianDayUt, julianDayTt);
	if(!std::isfinite(siderealTimeDegrees))
		return {};
	const Mat4d altAzToEquatorial =
		Mat4d::zrotation(
			(siderealTimeDegrees + longitudeDegrees) *
			degreesToRadians) *
		Mat4d::yrotation(
			(90.0 - latitudeDegrees) * degreesToRadians);
	const Mat4d altAzToVsop87 =
		equatorialToVsop87 * altAzToEquatorial;

	const Vec4d offset = earth->getRectangularCoordinates(
		longitudeDegrees, latitudeDegrees, location.altitude);
	const double latitudeRadians =
		latitudeDegrees * degreesToRadians;
	const double sigma = latitudeRadians - offset[2];
	const Vec3d observerAltAz(
		offset[3] * std::sin(sigma), 0.0,
		offset[3] * std::cos(sigma));
	const Vec3d observerVsop87 =
		altAzToVsop87.multiplyWithoutTranslation(observerAltAz);
	if(!finiteVector(observerVsop87))
		return {};

	const Vec3d sunTopocentric =
		-earthHeliocentric - observerVsop87;
	const Vec3d moonTopocentric =
		moonGeocentric - observerVsop87;
	if(!finiteVector(sunTopocentric) ||
	   sunTopocentric.normSquared() <= 0.0 ||
	   !finiteVector(moonTopocentric) ||
	   moonTopocentric.normSquared() <= 0.0)
	{
		return {};
	}

	const Mat4d vsop87ToAltAz = altAzToVsop87.transpose();
	HorizontalCoordinates sunHorizontal;
	HorizontalCoordinates moonHorizontal;
	if(!horizontalCoordinates(
		   vsop87ToAltAz.multiplyWithoutTranslation(sunTopocentric),
		   sunHorizontal) ||
	   !horizontalCoordinates(
		   vsop87ToAltAz.multiplyWithoutTranslation(moonTopocentric),
		   moonHorizontal))
	{
		return {};
	}

	const Vec3d moonHeliocentric =
		earthHeliocentric + moonGeocentric;
	const Vec3d observerHeliocentric =
		earthHeliocentric + observerVsop87;
	const double observerMoonSquared = moonTopocentric.normSquared();
	const double moonHeliocentricSquared =
		moonHeliocentric.normSquared();
	const double observerHeliocentricSquared =
		observerHeliocentric.normSquared();
	const double phaseDenominator = 2.0 * std::sqrt(
		observerMoonSquared * moonHeliocentricSquared);
	if(!(phaseDenominator > 0.0) ||
	   !std::isfinite(phaseDenominator))
	{
		return {};
	}
	const double cosPhaseAngle = std::clamp(
		(observerMoonSquared + moonHeliocentricSquared -
		 observerHeliocentricSquared) / phaseDenominator,
		-1.0, 1.0);
	const double illuminatedFraction =
		0.5 * std::abs(1.0 + cosPhaseAngle);
	const double moonDistance = std::sqrt(observerMoonSquared);
	const double moonAngularDiameter = 2.0 * std::atan2(
		moon->getEquatorialRadius(), moonDistance) * radiansToDegrees;
	const double moonGeocentricDistance = moonGeocentric.norm();
	const double horizontalParallaxRatio = std::clamp(
		earth->getEquatorialRadius() / moonGeocentricDistance,
		-1.0, 1.0);
	const double moonHorizontalParallax =
		std::asin(horizontalParallaxRatio) * radiansToDegrees;
	if(!std::isfinite(illuminatedFraction) ||
	   illuminatedFraction < 0.0 || illuminatedFraction > 1.0 ||
	   !std::isfinite(moonAngularDiameter) ||
	   moonAngularDiameter <= 0.0 ||
	   !std::isfinite(moonHorizontalParallax) ||
	   moonHorizontalParallax <= 0.0 ||
	   moonHorizontalParallax >= 90.0)
	{
		return {};
	}

	QVariantMap sample;
	sample.insert(QStringLiteral("schemaVersion"), 1);
	sample.insert(QStringLiteral("julianDayUt"), julianDayUt);
	sample.insert(QStringLiteral("julianDayTt"), julianDayTt);
	sample.insert(QStringLiteral("deltaTSeconds"), deltaTSeconds);
	sample.insert(
		QStringLiteral("sunAzimuthTopocentricGeometricDegrees"),
		sunHorizontal.azimuthDegrees);
	sample.insert(
		QStringLiteral("sunAltitudeTopocentricGeometricDegrees"),
		sunHorizontal.altitudeDegrees);
	sample.insert(
		QStringLiteral("moonAzimuthTopocentricGeometricDegrees"),
		moonHorizontal.azimuthDegrees);
	sample.insert(
		QStringLiteral("moonAltitudeTopocentricGeometricDegrees"),
		moonHorizontal.altitudeDegrees);
	sample.insert(QStringLiteral("moonIlluminatedFraction"),
		illuminatedFraction);
	sample.insert(
		QStringLiteral(
			"moonAngularDiameterTopocentricUnscaledDegrees"),
		moonAngularDiameter);
	sample.insert(
		QStringLiteral("moonHorizontalParallaxGeocentricDegrees"),
		moonHorizontalParallax);
	return sample;
}
