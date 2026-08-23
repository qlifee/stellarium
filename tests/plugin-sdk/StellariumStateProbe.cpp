/*
 * Stellarium external dynamic plug-in proof
 * Copyright (C) 2026 Stellarium Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "StellariumStateProbe.hpp"

#include "StelApp.hpp"
#include "StelPluginAPI.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
constexpr double j2000JulianDay = 2451545.0;
constexpr double maximumArbitrarySampleSpanDays = 2000.0 * 36525.0;
constexpr double arbitrarySampleAngularRepeatToleranceDegrees = 1e-3;
constexpr double arbitrarySampleScalarRepeatTolerance = 1e-5;
constexpr std::array<double, 4> multiEpochJulianDays{
	2415020.5, // 1900-01-01 00:00
	2451545.0, // J2000: 2000-01-01 12:00
	2460310.5, // 2024-01-01 00:00
	2469807.5  // 2050-01-01 00:00
};

struct ArbitraryEpochSamples
{
	QVariantMap conjunction;
	QVariantMap futureConjunction;
	QVariantMap repeatedConjunction;
	QVariantMap visibility;
	QVariantMap repeatedVisibility;
};

bool hasType(const QVariantMap& state, const QString& key, int typeId)
{
	const auto value = state.constFind(key);
	return value != state.cend() && value->metaType().id() == typeId;
}

double horizontalSeparationDegrees(double firstAzimuthDegrees,
	double firstAltitudeDegrees, double secondAzimuthDegrees,
	double secondAltitudeDegrees)
{
	constexpr double degreesToRadians =
		3.14159265358979323846 / 180.0;
	const double firstAltitude =
		firstAltitudeDegrees * degreesToRadians;
	const double secondAltitude =
		secondAltitudeDegrees * degreesToRadians;
	const double azimuthDifference =
		(firstAzimuthDegrees - secondAzimuthDegrees) * degreesToRadians;
	const double cosine = std::clamp(
		std::sin(firstAltitude) * std::sin(secondAltitude) +
		std::cos(firstAltitude) * std::cos(secondAltitude) *
			std::cos(azimuthDifference),
		-1.0, 1.0);
	return std::acos(cosine) / degreesToRadians;
}
}

StellariumStateProbe::StellariumStateProbe()
{
	setObjectName(QStringLiteral("StellariumStateProbe"));
}

void StellariumStateProbe::init()
{
	// Solar System positions are refreshed by the host's update loop after
	// plug-in initialization. Defer the one-shot proof until that has happened.
	probePending = true;
}

void StellariumStateProbe::update(double)
{
	if(!probePending)
		return;
	probePending = false;

	const QString moduleVersion = getModuleVersion();
	const bool versionMatches =
		moduleVersion == QStringLiteral(STELLARIUM_EXPECTED_VERSION);
	const bool appInitialized = StelApp::isInitialized();
	bool selfRegistered = false;
	bool solarSystemAvailable = false;
	bool solarSystemIdentityMatches = false;
	QVariantMap coreState;
	QVariantMap sunState;
	QVariantMap moonState;
	QVariantMap lowerCaseMoonState;
	QVariantMap conjunctionSample;
	QVariantMap repeatedConjunctionSample;
	QVariantMap futureConjunctionSample;
	QVariantMap visibilitySample;
	QVariantMap repeatedVisibilitySample;
	QVariantMap futureVisibilitySample;
	QVariantMap coreStateAfterArbitrarySamples;
	QVariantMap sunStateAfterArbitrarySamples;
	QVariantMap moonStateAfterArbitrarySamples;
	std::array<ArbitraryEpochSamples, multiEpochJulianDays.size()>
		multiEpochSamples;

	if(appInitialized)
	{
		StelApp& app = StelApp::getInstance();
		selfRegistered =
			app.getModule(QStringLiteral("StellariumStateProbe")) == this;
		StelModule* solarSystem =
			app.getModule(QStringLiteral("SolarSystem"));
		solarSystemAvailable = solarSystem != nullptr;
		solarSystemIdentityMatches = solarSystemAvailable &&
			solarSystem->objectName() == QStringLiteral("SolarSystem");
		StelCore* core = app.getCore();
		coreState = StelPluginAPI::getCoreStateSnapshot(core);
		sunState = StelPluginAPI::getSolarSystemBodyStateSnapshot(
			core, QStringLiteral("Sun"));
		moonState = StelPluginAPI::getSolarSystemBodyStateSnapshot(
			core, QStringLiteral("Moon"));
		lowerCaseMoonState =
			StelPluginAPI::getSolarSystemBodyStateSnapshot(
				core, QStringLiteral("moon"));
		const double currentJulianDayUt =
			coreState.value(QStringLiteral("julianDayUt")).toDouble();
		const double currentJulianDayTt =
			coreState.value(QStringLiteral("julianDayTt")).toDouble();
		conjunctionSample =
			StelPluginAPI::getMoonSunConjunctionSampleAtJulianDayTt(
				core, currentJulianDayTt);
		futureConjunctionSample =
			StelPluginAPI::getMoonSunConjunctionSampleAtJulianDayTt(
				core, currentJulianDayTt + 1.0);
		repeatedConjunctionSample =
			StelPluginAPI::getMoonSunConjunctionSampleAtJulianDayTt(
				core, currentJulianDayTt);
		visibilitySample =
			StelPluginAPI::getSunMoonVisibilitySampleAtJulianDayUt(
				core, currentJulianDayUt);
		futureVisibilitySample =
			StelPluginAPI::getSunMoonVisibilitySampleAtJulianDayUt(
				core, currentJulianDayUt + 1.0);
		repeatedVisibilitySample =
			StelPluginAPI::getSunMoonVisibilitySampleAtJulianDayUt(
				core, currentJulianDayUt);
		for(std::size_t index = 0; index < multiEpochJulianDays.size(); ++index)
		{
			const double julianDay = multiEpochJulianDays[index];
			ArbitraryEpochSamples& samples = multiEpochSamples[index];
			samples.conjunction =
				StelPluginAPI::getMoonSunConjunctionSampleAtJulianDayTt(
					core, julianDay);
			samples.futureConjunction =
				StelPluginAPI::getMoonSunConjunctionSampleAtJulianDayTt(
					core, julianDay + 1.0);
			samples.visibility =
				StelPluginAPI::getSunMoonVisibilitySampleAtJulianDayUt(
					core, julianDay);
			samples.repeatedConjunction =
				StelPluginAPI::getMoonSunConjunctionSampleAtJulianDayTt(
					core, julianDay);
			samples.repeatedVisibility =
				StelPluginAPI::getSunMoonVisibilitySampleAtJulianDayUt(
					core, julianDay);
		}
		coreStateAfterArbitrarySamples =
			StelPluginAPI::getCoreStateSnapshot(core);
		sunStateAfterArbitrarySamples =
			StelPluginAPI::getSolarSystemBodyStateSnapshot(
				core, QStringLiteral("Sun"));
		moonStateAfterArbitrarySamples =
			StelPluginAPI::getSolarSystemBodyStateSnapshot(
				core, QStringLiteral("Moon"));
	}

	const bool nullCoreReturnsEmpty =
		StelPluginAPI::getCoreStateSnapshot(nullptr).isEmpty();
	const bool nullBodyCoreReturnsEmpty =
		StelPluginAPI::getSolarSystemBodyStateSnapshot(
			nullptr, QStringLiteral("Moon")).isEmpty();
	const bool emptyBodyIdReturnsEmpty = appInitialized &&
		StelPluginAPI::getSolarSystemBodyStateSnapshot(
			StelApp::getInstance().getCore(), QString()).isEmpty();
	const bool unknownBodyReturnsEmpty = appInitialized &&
		StelPluginAPI::getSolarSystemBodyStateSnapshot(
			StelApp::getInstance().getCore(),
			QStringLiteral("NotARealSolarSystemBody")).isEmpty();
	const bool nullConjunctionCoreReturnsEmpty =
		StelPluginAPI::getMoonSunConjunctionSampleAtJulianDayTt(
			nullptr, 2451545.0).isEmpty();
	const bool invalidConjunctionDatesReturnEmpty = appInitialized &&
		StelPluginAPI::getMoonSunConjunctionSampleAtJulianDayTt(
			StelApp::getInstance().getCore(),
			std::numeric_limits<double>::quiet_NaN()).isEmpty() &&
		StelPluginAPI::getMoonSunConjunctionSampleAtJulianDayTt(
			StelApp::getInstance().getCore(),
			std::numeric_limits<double>::infinity()).isEmpty() &&
		StelPluginAPI::getMoonSunConjunctionSampleAtJulianDayTt(
			StelApp::getInstance().getCore(),
			j2000JulianDay + maximumArbitrarySampleSpanDays + 1.0)
			.isEmpty() &&
		StelPluginAPI::getMoonSunConjunctionSampleAtJulianDayTt(
			StelApp::getInstance().getCore(),
			j2000JulianDay - maximumArbitrarySampleSpanDays - 1.0)
			.isEmpty();
	const bool nullVisibilityCoreReturnsEmpty =
		StelPluginAPI::getSunMoonVisibilitySampleAtJulianDayUt(
			nullptr, 2451545.0).isEmpty();
	const bool invalidVisibilityDatesReturnEmpty = appInitialized &&
		StelPluginAPI::getSunMoonVisibilitySampleAtJulianDayUt(
			StelApp::getInstance().getCore(),
			std::numeric_limits<double>::quiet_NaN()).isEmpty() &&
		StelPluginAPI::getSunMoonVisibilitySampleAtJulianDayUt(
			StelApp::getInstance().getCore(),
			-std::numeric_limits<double>::infinity()).isEmpty() &&
		StelPluginAPI::getSunMoonVisibilitySampleAtJulianDayUt(
			StelApp::getInstance().getCore(),
			j2000JulianDay + maximumArbitrarySampleSpanDays + 1.0)
			.isEmpty() &&
		StelPluginAPI::getSunMoonVisibilitySampleAtJulianDayUt(
			StelApp::getInstance().getCore(),
			j2000JulianDay - maximumArbitrarySampleSpanDays - 1.0)
			.isEmpty();
	const bool coreStateAvailable = !coreState.isEmpty();
	const bool coreStateSchemaMatches = coreStateAvailable &&
		coreState.value(QStringLiteral("schemaVersion")).toInt() == 1;
	const bool coreStateTypesMatch = coreStateSchemaMatches &&
		coreState.size() == 14 &&
		hasType(coreState, QStringLiteral("schemaVersion"), QMetaType::Int) &&
		hasType(coreState, QStringLiteral("julianDayUt"), QMetaType::Double) &&
		hasType(coreState, QStringLiteral("julianDayTt"), QMetaType::Double) &&
		hasType(coreState, QStringLiteral("deltaTSeconds"), QMetaType::Double) &&
		hasType(coreState, QStringLiteral("utcOffsetHours"), QMetaType::Double) &&
		hasType(coreState, QStringLiteral("timeRateJdPerSecond"), QMetaType::Double) &&
		hasType(coreState, QStringLiteral("currentTimeZone"), QMetaType::QString) &&
		hasType(coreState, QStringLiteral("locationTimeZone"), QMetaType::QString) &&
		hasType(coreState, QStringLiteral("locationId"), QMetaType::QString) &&
		hasType(coreState, QStringLiteral("locationValid"), QMetaType::Bool) &&
		hasType(coreState, QStringLiteral("longitudeDegrees"), QMetaType::Double) &&
		hasType(coreState, QStringLiteral("latitudeDegrees"), QMetaType::Double) &&
		hasType(coreState, QStringLiteral("altitudeMeters"), QMetaType::Int) &&
		hasType(coreState, QStringLiteral("planetName"), QMetaType::QString);

	const double julianDayUt =
		coreState.value(QStringLiteral("julianDayUt")).toDouble();
	const double julianDayTt =
		coreState.value(QStringLiteral("julianDayTt")).toDouble();
	const double deltaTSeconds =
		coreState.value(QStringLiteral("deltaTSeconds")).toDouble();
	const double utcOffsetHours =
		coreState.value(QStringLiteral("utcOffsetHours")).toDouble();
	const double timeRateJdPerSecond =
		coreState.value(QStringLiteral("timeRateJdPerSecond")).toDouble();
	const double longitudeDegrees =
		coreState.value(QStringLiteral("longitudeDegrees")).toDouble();
	const double latitudeDegrees =
		coreState.value(QStringLiteral("latitudeDegrees")).toDouble();
	const bool astronomyValuesValid = coreStateTypesMatch &&
		std::isfinite(julianDayUt) && julianDayUt > 0.0 &&
		std::isfinite(julianDayTt) && julianDayTt > 0.0 &&
		std::isfinite(deltaTSeconds) &&
		std::isfinite(utcOffsetHours) &&
		utcOffsetHours >= -24.0 && utcOffsetHours <= 24.0 &&
		std::isfinite(timeRateJdPerSecond);
	const bool ephemerisRelationValid = astronomyValuesValid &&
		std::abs(julianDayTt -
		         (julianDayUt + deltaTSeconds / 86400.0)) <= 1e-9;
	const bool locationValuesValid = coreStateTypesMatch &&
		coreState.value(QStringLiteral("locationValid")).toBool() &&
		std::isfinite(longitudeDegrees) &&
		longitudeDegrees >= -180.0 && longitudeDegrees <= 180.0 &&
		std::isfinite(latitudeDegrees) &&
		latitudeDegrees >= -90.0 && latitudeDegrees <= 90.0 &&
		!coreState.value(QStringLiteral("locationId")).toString().isEmpty() &&
		!coreState.value(QStringLiteral("planetName")).toString().isEmpty();

	const auto hasBaseBodyStateTypes = [](const QVariantMap& state)
	{
		return
			hasType(state, QStringLiteral("schemaVersion"), QMetaType::Int) &&
			hasType(state, QStringLiteral("englishName"), QMetaType::QString) &&
			hasType(state, QStringLiteral("julianDayUt"), QMetaType::Double) &&
			hasType(state, QStringLiteral("julianDayTt"), QMetaType::Double) &&
			hasType(state, QStringLiteral("rightAscensionJ2000Degrees"), QMetaType::Double) &&
			hasType(state, QStringLiteral("declinationJ2000Degrees"), QMetaType::Double) &&
			hasType(state, QStringLiteral("azimuthGeometricDegrees"), QMetaType::Double) &&
			hasType(state, QStringLiteral("altitudeGeometricDegrees"), QMetaType::Double) &&
			hasType(state, QStringLiteral("distanceAu"), QMetaType::Double) &&
			hasType(state, QStringLiteral("angularDiameterUnscaledDegrees"), QMetaType::Double) &&
			hasType(state, QStringLiteral("aberrationEnabled"), QMetaType::Bool) &&
			hasType(state, QStringLiteral("topocentricCoordinatesEnabled"), QMetaType::Bool) &&
			hasType(state, QStringLiteral("phaseDataAvailable"), QMetaType::Bool);
	};
	const bool sunStateAvailable = !sunState.isEmpty();
	const bool moonStateAvailable = !moonState.isEmpty();
	const bool bodyStateSchemaMatches = sunStateAvailable &&
		moonStateAvailable &&
		sunState.value(QStringLiteral("schemaVersion")).toInt() == 1 &&
		moonState.value(QStringLiteral("schemaVersion")).toInt() == 1;
	const bool bodyIdentitiesMatch = bodyStateSchemaMatches &&
		sunState.value(QStringLiteral("englishName")).toString() ==
			QStringLiteral("Sun") &&
		moonState.value(QStringLiteral("englishName")).toString() ==
			QStringLiteral("Moon");
	const bool sunPhaseUnavailable = bodyStateSchemaMatches &&
		!sunState.value(QStringLiteral("phaseDataAvailable")).toBool() &&
		!sunState.contains(QStringLiteral("illuminatedFraction")) &&
		!sunState.contains(QStringLiteral("phaseAngleDegrees")) &&
		!sunState.contains(QStringLiteral("elongationDegrees"));
	const bool moonPhaseAvailable = bodyStateSchemaMatches &&
		moonState.value(QStringLiteral("phaseDataAvailable")).toBool();
	const bool bodyStateTypesMatch = bodyStateSchemaMatches &&
		sunState.size() == 13 && moonState.size() == 16 &&
		hasBaseBodyStateTypes(sunState) &&
		hasBaseBodyStateTypes(moonState) &&
		hasType(moonState, QStringLiteral("illuminatedFraction"),
		        QMetaType::Double) &&
		hasType(moonState, QStringLiteral("phaseAngleDegrees"),
		        QMetaType::Double) &&
		hasType(moonState, QStringLiteral("elongationDegrees"),
		        QMetaType::Double);
	const bool bodyLookupCaseInsensitive = bodyStateTypesMatch &&
		lowerCaseMoonState == moonState;
	const bool bodyStateTimesMatch = bodyStateTypesMatch &&
		std::abs(sunState.value(QStringLiteral("julianDayUt")).toDouble() -
		         julianDayUt) <= 1e-12 &&
		std::abs(sunState.value(QStringLiteral("julianDayTt")).toDouble() -
		         julianDayTt) <= 1e-12 &&
		std::abs(moonState.value(QStringLiteral("julianDayUt")).toDouble() -
		         julianDayUt) <= 1e-12 &&
		std::abs(moonState.value(QStringLiteral("julianDayTt")).toDouble() -
		         julianDayTt) <= 1e-12;
	const auto bodyPositionValuesValid = [](const QVariantMap& state)
	{
		const double ra = state.value(
			QStringLiteral("rightAscensionJ2000Degrees")).toDouble();
		const double dec = state.value(
			QStringLiteral("declinationJ2000Degrees")).toDouble();
		const double az = state.value(
			QStringLiteral("azimuthGeometricDegrees")).toDouble();
		const double alt = state.value(
			QStringLiteral("altitudeGeometricDegrees")).toDouble();
		const double distance = state.value(
			QStringLiteral("distanceAu")).toDouble();
		const double diameter = state.value(
			QStringLiteral("angularDiameterUnscaledDegrees")).toDouble();
		return std::isfinite(ra) && ra >= 0.0 && ra < 360.0 &&
			std::isfinite(dec) && dec >= -90.0 && dec <= 90.0 &&
			std::isfinite(az) && az >= 0.0 && az < 360.0 &&
			std::isfinite(alt) && alt >= -90.0 && alt <= 90.0 &&
			std::isfinite(distance) && distance > 0.0 &&
			std::isfinite(diameter) && diameter > 0.0;
	};
	const bool bodyPositionValuesMatch = bodyStateTypesMatch &&
		bodyPositionValuesValid(sunState) &&
		bodyPositionValuesValid(moonState);
	const double moonIlluminatedFraction =
		moonState.value(QStringLiteral("illuminatedFraction")).toDouble();
	const double moonPhaseAngleDegrees =
		moonState.value(QStringLiteral("phaseAngleDegrees")).toDouble();
	const double moonElongationDegrees =
		moonState.value(QStringLiteral("elongationDegrees")).toDouble();
	const bool moonPhaseValuesValid = bodyStateTypesMatch &&
		moonPhaseAvailable && std::isfinite(moonIlluminatedFraction) &&
		moonIlluminatedFraction >= 0.0 && moonIlluminatedFraction <= 1.0 &&
		std::isfinite(moonPhaseAngleDegrees) &&
		moonPhaseAngleDegrees >= 0.0 && moonPhaseAngleDegrees <= 180.0 &&
		std::isfinite(moonElongationDegrees) &&
		moonElongationDegrees >= 0.0 && moonElongationDegrees <= 180.0;
	constexpr double pi = 3.14159265358979323846;
	const double expectedMoonIlluminatedFraction =
		0.5 * (1.0 + std::cos(moonPhaseAngleDegrees * pi / 180.0));
	const bool moonPhaseRelationValid = moonPhaseValuesValid &&
		std::abs(moonIlluminatedFraction -
		         expectedMoonIlluminatedFraction) <= 1e-5;

	const bool conjunctionSampleAvailable = !conjunctionSample.isEmpty() &&
		!futureConjunctionSample.isEmpty();
	const auto hasConjunctionSampleTypes = [](const QVariantMap& sample)
	{
		return sample.size() == 3 &&
			sample.value(QStringLiteral("schemaVersion")).toInt() == 1 &&
			hasType(sample, QStringLiteral("schemaVersion"),
			        QMetaType::Int) &&
			hasType(sample, QStringLiteral("julianDayTt"),
			        QMetaType::Double) &&
			hasType(
				sample,
				QStringLiteral(
					"moonSunGeocentricEclipticLongitudeDifferenceDegrees"),
				QMetaType::Double);
	};
	const bool conjunctionSampleContractValid =
		conjunctionSampleAvailable &&
		hasConjunctionSampleTypes(conjunctionSample) &&
		hasConjunctionSampleTypes(repeatedConjunctionSample) &&
		hasConjunctionSampleTypes(futureConjunctionSample);
	const double conjunctionDifferenceDegrees = conjunctionSample.value(
		QStringLiteral(
			"moonSunGeocentricEclipticLongitudeDifferenceDegrees"))
		.toDouble();
	const double futureConjunctionDifferenceDegrees =
		futureConjunctionSample.value(
			QStringLiteral(
				"moonSunGeocentricEclipticLongitudeDifferenceDegrees"))
		.toDouble();
	const bool conjunctionSampleValuesValid =
		conjunctionSampleContractValid &&
		std::abs(conjunctionSample.value(
			QStringLiteral("julianDayTt")).toDouble() - julianDayTt) <= 1e-12 &&
		std::abs(futureConjunctionSample.value(
			QStringLiteral("julianDayTt")).toDouble() -
			(julianDayTt + 1.0)) <= 1e-12 &&
		std::isfinite(conjunctionDifferenceDegrees) &&
		conjunctionDifferenceDegrees > -180.0 &&
		conjunctionDifferenceDegrees <= 180.0 &&
		std::isfinite(futureConjunctionDifferenceDegrees) &&
		futureConjunctionDifferenceDegrees > -180.0 &&
		futureConjunctionDifferenceDegrees <= 180.0;
	// A query at another epoch can change interpolation rounding in the host
	// ephemerides. Require tight numerical repeatability rather than
	// bit-for-bit floating-point identity.
	const bool conjunctionSampleDeterministic =
		conjunctionSampleContractValid &&
		std::abs(repeatedConjunctionSample.value(
			QStringLiteral("julianDayTt")).toDouble() -
			conjunctionSample.value(
				QStringLiteral("julianDayTt")).toDouble()) <= 1e-12 &&
		std::abs(repeatedConjunctionSample.value(
			QStringLiteral(
				"moonSunGeocentricEclipticLongitudeDifferenceDegrees"))
			.toDouble() - conjunctionDifferenceDegrees) <=
			arbitrarySampleAngularRepeatToleranceDegrees;
	const double conjunctionDailyMotionDegrees = std::remainder(
		futureConjunctionDifferenceDegrees - conjunctionDifferenceDegrees,
		360.0);
	const bool conjunctionSampleChangesWithTime =
		conjunctionSampleValuesValid &&
		conjunctionDailyMotionDegrees > 5.0 &&
		conjunctionDailyMotionDegrees < 20.0;

	const auto hasVisibilitySampleTypes = [](const QVariantMap& sample)
	{
		return sample.size() == 11 &&
			hasType(sample, QStringLiteral("schemaVersion"), QMetaType::Int) &&
			hasType(sample, QStringLiteral("julianDayUt"), QMetaType::Double) &&
			hasType(sample, QStringLiteral("julianDayTt"), QMetaType::Double) &&
			hasType(sample, QStringLiteral("deltaTSeconds"), QMetaType::Double) &&
			hasType(sample,
			        QStringLiteral("sunAzimuthTopocentricGeometricDegrees"),
			        QMetaType::Double) &&
			hasType(sample,
			        QStringLiteral("sunAltitudeTopocentricGeometricDegrees"),
			        QMetaType::Double) &&
			hasType(sample,
			        QStringLiteral("moonAzimuthTopocentricGeometricDegrees"),
			        QMetaType::Double) &&
			hasType(sample,
			        QStringLiteral("moonAltitudeTopocentricGeometricDegrees"),
			        QMetaType::Double) &&
			hasType(sample, QStringLiteral("moonIlluminatedFraction"),
			        QMetaType::Double) &&
			hasType(
				sample,
				QStringLiteral(
					"moonAngularDiameterTopocentricUnscaledDegrees"),
				QMetaType::Double) &&
			hasType(sample,
			        QStringLiteral("moonHorizontalParallaxGeocentricDegrees"),
			        QMetaType::Double);
	};
	const auto visibilityValuesAreValid =
		[&hasVisibilitySampleTypes](const QVariantMap& sample,
		                            double expectedJulianDayUt)
	{
		if(!hasVisibilitySampleTypes(sample))
			return false;
		const double julianDayUtValue =
			sample.value(QStringLiteral("julianDayUt")).toDouble();
		const double julianDayTtValue =
			sample.value(QStringLiteral("julianDayTt")).toDouble();
		const double deltaTValue =
			sample.value(QStringLiteral("deltaTSeconds")).toDouble();
		const double sunAzimuth = sample.value(
			QStringLiteral("sunAzimuthTopocentricGeometricDegrees")).toDouble();
		const double sunAltitude = sample.value(
			QStringLiteral("sunAltitudeTopocentricGeometricDegrees")).toDouble();
		const double moonAzimuth = sample.value(
			QStringLiteral("moonAzimuthTopocentricGeometricDegrees")).toDouble();
		const double moonAltitude = sample.value(
			QStringLiteral("moonAltitudeTopocentricGeometricDegrees")).toDouble();
		const double illuminatedFraction = sample.value(
			QStringLiteral("moonIlluminatedFraction")).toDouble();
		const double angularDiameter = sample.value(
			QStringLiteral(
				"moonAngularDiameterTopocentricUnscaledDegrees")).toDouble();
		const double horizontalParallax = sample.value(
			QStringLiteral("moonHorizontalParallaxGeocentricDegrees")).toDouble();
		return std::abs(julianDayUtValue - expectedJulianDayUt) <= 1e-12 &&
			std::isfinite(julianDayTtValue) &&
			std::isfinite(deltaTValue) &&
			std::abs(julianDayTtValue -
				(julianDayUtValue + deltaTValue / 86400.0)) <= 1e-9 &&
			std::isfinite(sunAzimuth) &&
			sunAzimuth >= 0.0 && sunAzimuth < 360.0 &&
			std::isfinite(sunAltitude) &&
			sunAltitude >= -90.0 && sunAltitude <= 90.0 &&
			std::isfinite(moonAzimuth) &&
			moonAzimuth >= 0.0 && moonAzimuth < 360.0 &&
			std::isfinite(moonAltitude) &&
			moonAltitude >= -90.0 && moonAltitude <= 90.0 &&
			std::isfinite(illuminatedFraction) &&
			illuminatedFraction >= 0.0 && illuminatedFraction <= 1.0 &&
			std::isfinite(angularDiameter) &&
			angularDiameter >= 0.4 && angularDiameter <= 0.7 &&
			std::isfinite(horizontalParallax) &&
			horizontalParallax >= 0.7 && horizontalParallax <= 1.2;
	};
	const bool visibilitySampleAvailable = !visibilitySample.isEmpty() &&
		!futureVisibilitySample.isEmpty();
	const bool visibilitySampleContractValid = visibilitySampleAvailable &&
		visibilitySample.value(QStringLiteral("schemaVersion")).toInt() == 1 &&
		hasVisibilitySampleTypes(visibilitySample) &&
		hasVisibilitySampleTypes(futureVisibilitySample);
	const double visibilityJulianDayTt =
		visibilitySample.value(QStringLiteral("julianDayTt")).toDouble();
	const double visibilityDeltaTSeconds =
		visibilitySample.value(QStringLiteral("deltaTSeconds")).toDouble();
	const double visibilitySunAzimuth = visibilitySample.value(
		QStringLiteral("sunAzimuthTopocentricGeometricDegrees")).toDouble();
	const double visibilitySunAltitude = visibilitySample.value(
		QStringLiteral("sunAltitudeTopocentricGeometricDegrees")).toDouble();
	const double visibilityMoonAzimuth = visibilitySample.value(
		QStringLiteral("moonAzimuthTopocentricGeometricDegrees")).toDouble();
	const double visibilityMoonAltitude = visibilitySample.value(
		QStringLiteral("moonAltitudeTopocentricGeometricDegrees")).toDouble();
	const double visibilityMoonIlluminatedFraction = visibilitySample.value(
		QStringLiteral("moonIlluminatedFraction")).toDouble();
	const double visibilityMoonAngularDiameter = visibilitySample.value(
		QStringLiteral(
			"moonAngularDiameterTopocentricUnscaledDegrees")).toDouble();
	const double visibilityMoonHorizontalParallax = visibilitySample.value(
		QStringLiteral("moonHorizontalParallaxGeocentricDegrees")).toDouble();
	const bool visibilitySampleValuesValid = visibilitySampleContractValid &&
		visibilityValuesAreValid(visibilitySample, julianDayUt) &&
		visibilityValuesAreValid(repeatedVisibilitySample, julianDayUt) &&
		visibilityValuesAreValid(futureVisibilitySample, julianDayUt + 1.0) &&
		std::abs(visibilityDeltaTSeconds - deltaTSeconds) <= 1e-9;
	const bool visibilitySampleDeterministic =
		visibilitySampleContractValid &&
		std::abs(repeatedVisibilitySample.value(
			QStringLiteral("julianDayUt")).toDouble() -
			visibilitySample.value(
				QStringLiteral("julianDayUt")).toDouble()) <= 1e-12 &&
		std::abs(repeatedVisibilitySample.value(
			QStringLiteral("julianDayTt")).toDouble() -
			visibilityJulianDayTt) <= 1e-12 &&
		std::abs(repeatedVisibilitySample.value(
			QStringLiteral("deltaTSeconds")).toDouble() -
			visibilityDeltaTSeconds) <= 1e-12 &&
		std::abs(repeatedVisibilitySample.value(
			QStringLiteral(
				"sunAzimuthTopocentricGeometricDegrees")).toDouble() -
			visibilitySunAzimuth) <=
			arbitrarySampleAngularRepeatToleranceDegrees &&
		std::abs(repeatedVisibilitySample.value(
			QStringLiteral(
				"sunAltitudeTopocentricGeometricDegrees")).toDouble() -
			visibilitySunAltitude) <=
			arbitrarySampleAngularRepeatToleranceDegrees &&
		std::abs(repeatedVisibilitySample.value(
			QStringLiteral(
				"moonAzimuthTopocentricGeometricDegrees")).toDouble() -
			visibilityMoonAzimuth) <=
			arbitrarySampleAngularRepeatToleranceDegrees &&
		std::abs(repeatedVisibilitySample.value(
			QStringLiteral(
				"moonAltitudeTopocentricGeometricDegrees")).toDouble() -
			visibilityMoonAltitude) <=
			arbitrarySampleAngularRepeatToleranceDegrees &&
		std::abs(repeatedVisibilitySample.value(
			QStringLiteral("moonIlluminatedFraction")).toDouble() -
			visibilityMoonIlluminatedFraction) <=
			arbitrarySampleScalarRepeatTolerance &&
		std::abs(repeatedVisibilitySample.value(
			QStringLiteral(
				"moonAngularDiameterTopocentricUnscaledDegrees"))
			.toDouble() - visibilityMoonAngularDiameter) <=
			arbitrarySampleScalarRepeatTolerance &&
		std::abs(repeatedVisibilitySample.value(
			QStringLiteral(
				"moonHorizontalParallaxGeocentricDegrees"))
			.toDouble() - visibilityMoonHorizontalParallax) <=
			arbitrarySampleScalarRepeatTolerance;
	const double futureVisibilitySunAzimuth = futureVisibilitySample.value(
		QStringLiteral("sunAzimuthTopocentricGeometricDegrees")).toDouble();
	const double futureVisibilitySunAltitude = futureVisibilitySample.value(
		QStringLiteral("sunAltitudeTopocentricGeometricDegrees")).toDouble();
	const double futureVisibilityMoonAzimuth = futureVisibilitySample.value(
		QStringLiteral("moonAzimuthTopocentricGeometricDegrees")).toDouble();
	const double futureVisibilityMoonAltitude = futureVisibilitySample.value(
		QStringLiteral("moonAltitudeTopocentricGeometricDegrees")).toDouble();
	const bool visibilitySampleChangesWithTime =
		visibilitySampleValuesValid &&
		(horizontalSeparationDegrees(
			visibilityMoonAzimuth, visibilityMoonAltitude,
			futureVisibilityMoonAzimuth,
			futureVisibilityMoonAltitude) > 0.1 ||
		 horizontalSeparationDegrees(
			visibilitySunAzimuth, visibilitySunAltitude,
			futureVisibilitySunAzimuth,
			futureVisibilitySunAltitude) > 0.1);
	bool multiEpochSamplesValid = appInitialized;
	bool multiEpochSamplesDeterministic = appInitialized;
	bool multiEpochConjunctionMotionValid = appInitialized;
	QJsonArray multiEpochResults;
	for(std::size_t index = 0; index < multiEpochJulianDays.size(); ++index)
	{
		const double julianDay = multiEpochJulianDays[index];
		const ArbitraryEpochSamples& samples = multiEpochSamples[index];
		const bool conjunctionValid =
			hasConjunctionSampleTypes(samples.conjunction) &&
			hasConjunctionSampleTypes(samples.futureConjunction) &&
			hasConjunctionSampleTypes(samples.repeatedConjunction) &&
			std::abs(samples.conjunction.value(
				QStringLiteral("julianDayTt")).toDouble() - julianDay) <= 1e-12 &&
			std::abs(samples.futureConjunction.value(
				QStringLiteral("julianDayTt")).toDouble() -
				(julianDay + 1.0)) <= 1e-12;
		const double conjunctionDifference = samples.conjunction.value(
			QStringLiteral(
				"moonSunGeocentricEclipticLongitudeDifferenceDegrees"))
			.toDouble();
		const double futureConjunctionDifference =
			samples.futureConjunction.value(
				QStringLiteral(
					"moonSunGeocentricEclipticLongitudeDifferenceDegrees"))
				.toDouble();
		const double conjunctionDailyMotion = std::remainder(
			futureConjunctionDifference - conjunctionDifference, 360.0);
		const bool conjunctionValuesValid = conjunctionValid &&
			std::isfinite(conjunctionDifference) &&
			conjunctionDifference > -180.0 &&
			conjunctionDifference <= 180.0 &&
			std::isfinite(futureConjunctionDifference) &&
			futureConjunctionDifference > -180.0 &&
			futureConjunctionDifference <= 180.0;
		const bool conjunctionDeterministic = conjunctionValid &&
			std::abs(samples.repeatedConjunction.value(
				QStringLiteral("julianDayTt")).toDouble() - julianDay) <=
				1e-12 &&
			std::abs(samples.repeatedConjunction.value(
				QStringLiteral(
					"moonSunGeocentricEclipticLongitudeDifferenceDegrees"))
				.toDouble() - conjunctionDifference) <=
				arbitrarySampleAngularRepeatToleranceDegrees;
		const bool conjunctionMotionValid = conjunctionValuesValid &&
			conjunctionDailyMotion > 5.0 &&
			conjunctionDailyMotion < 20.0;

		const bool visibilityValid =
			visibilityValuesAreValid(samples.visibility, julianDay) &&
			visibilityValuesAreValid(samples.repeatedVisibility, julianDay);
		const auto visibilityFieldRepeats =
			[&samples](const QString& key, double tolerance)
		{
			return std::abs(samples.repeatedVisibility.value(key).toDouble() -
				samples.visibility.value(key).toDouble()) <= tolerance;
		};
		const bool visibilityDeterministic = visibilityValid &&
			visibilityFieldRepeats(QStringLiteral("julianDayUt"), 1e-12) &&
			visibilityFieldRepeats(QStringLiteral("julianDayTt"), 1e-12) &&
			visibilityFieldRepeats(QStringLiteral("deltaTSeconds"), 1e-12) &&
			visibilityFieldRepeats(
				QStringLiteral("sunAzimuthTopocentricGeometricDegrees"),
				arbitrarySampleAngularRepeatToleranceDegrees) &&
			visibilityFieldRepeats(
				QStringLiteral("sunAltitudeTopocentricGeometricDegrees"),
				arbitrarySampleAngularRepeatToleranceDegrees) &&
			visibilityFieldRepeats(
				QStringLiteral("moonAzimuthTopocentricGeometricDegrees"),
				arbitrarySampleAngularRepeatToleranceDegrees) &&
			visibilityFieldRepeats(
				QStringLiteral("moonAltitudeTopocentricGeometricDegrees"),
				arbitrarySampleAngularRepeatToleranceDegrees) &&
			visibilityFieldRepeats(
				QStringLiteral("moonIlluminatedFraction"),
				arbitrarySampleScalarRepeatTolerance) &&
			visibilityFieldRepeats(
				QStringLiteral(
					"moonAngularDiameterTopocentricUnscaledDegrees"),
				arbitrarySampleScalarRepeatTolerance) &&
			visibilityFieldRepeats(
				QStringLiteral("moonHorizontalParallaxGeocentricDegrees"),
				arbitrarySampleScalarRepeatTolerance);

		multiEpochSamplesValid = multiEpochSamplesValid &&
			conjunctionValuesValid && visibilityValid;
		multiEpochSamplesDeterministic =
			multiEpochSamplesDeterministic && conjunctionDeterministic &&
			visibilityDeterministic;
		multiEpochConjunctionMotionValid =
			multiEpochConjunctionMotionValid && conjunctionMotionValid;

		multiEpochResults.append(QJsonObject{
			{QStringLiteral("julian_day"), julianDay},
			{QStringLiteral("conjunction_difference_degrees"),
			 conjunctionDifference},
			{QStringLiteral("conjunction_daily_motion_degrees"),
			 conjunctionDailyMotion},
			{QStringLiteral("delta_t_seconds"),
			 samples.visibility.value(
				 QStringLiteral("deltaTSeconds")).toDouble()},
			{QStringLiteral("sun_azimuth_degrees"),
			 samples.visibility.value(QStringLiteral(
				 "sunAzimuthTopocentricGeometricDegrees")).toDouble()},
			{QStringLiteral("sun_altitude_degrees"),
			 samples.visibility.value(QStringLiteral(
				 "sunAltitudeTopocentricGeometricDegrees")).toDouble()},
			{QStringLiteral("moon_azimuth_degrees"),
			 samples.visibility.value(QStringLiteral(
				 "moonAzimuthTopocentricGeometricDegrees")).toDouble()},
			{QStringLiteral("moon_altitude_degrees"),
			 samples.visibility.value(QStringLiteral(
				 "moonAltitudeTopocentricGeometricDegrees")).toDouble()},
			{QStringLiteral("moon_illuminated_fraction"),
			 samples.visibility.value(
				 QStringLiteral("moonIlluminatedFraction")).toDouble()},
			{QStringLiteral("moon_angular_diameter_degrees"),
			 samples.visibility.value(QStringLiteral(
				 "moonAngularDiameterTopocentricUnscaledDegrees")).toDouble()},
			{QStringLiteral("moon_horizontal_parallax_degrees"),
			 samples.visibility.value(QStringLiteral(
				 "moonHorizontalParallaxGeocentricDegrees")).toDouble()}
		});
	}
	const bool arbitrarySamplesPreserveLiveState =
		coreStateAfterArbitrarySamples == coreState &&
		sunStateAfterArbitrarySamples == sunState &&
		moonStateAfterArbitrarySamples == moonState;
	const bool visibilityCurrentStateParity = visibilitySampleValuesValid &&
		sunState.value(
			QStringLiteral("topocentricCoordinatesEnabled")).toBool() &&
		horizontalSeparationDegrees(
			visibilitySunAzimuth, visibilitySunAltitude,
			sunState.value(
				QStringLiteral("azimuthGeometricDegrees")).toDouble(),
			sunState.value(
				QStringLiteral("altitudeGeometricDegrees")).toDouble()) <= 0.02 &&
		horizontalSeparationDegrees(
			visibilityMoonAzimuth, visibilityMoonAltitude,
			moonState.value(
				QStringLiteral("azimuthGeometricDegrees")).toDouble(),
			moonState.value(
				QStringLiteral("altitudeGeometricDegrees")).toDouble()) <= 0.02 &&
		std::abs(visibilityMoonIlluminatedFraction -
			moonIlluminatedFraction) <= 0.001 &&
		std::abs(visibilityMoonAngularDiameter -
			moonState.value(
				QStringLiteral("angularDiameterUnscaledDegrees")).toDouble()) <= 0.001;

	const bool success = versionMatches && appInitialized &&
		selfRegistered && solarSystemAvailable &&
		solarSystemIdentityMatches && nullCoreReturnsEmpty &&
		nullBodyCoreReturnsEmpty && emptyBodyIdReturnsEmpty &&
		unknownBodyReturnsEmpty && sunStateAvailable &&
		moonStateAvailable && bodyStateSchemaMatches &&
		bodyIdentitiesMatch && bodyStateTypesMatch &&
		bodyLookupCaseInsensitive && bodyStateTimesMatch &&
		bodyPositionValuesMatch && sunPhaseUnavailable &&
		moonPhaseAvailable && moonPhaseValuesValid &&
		moonPhaseRelationValid && nullConjunctionCoreReturnsEmpty &&
		invalidConjunctionDatesReturnEmpty &&
		nullVisibilityCoreReturnsEmpty &&
		invalidVisibilityDatesReturnEmpty &&
		conjunctionSampleAvailable && conjunctionSampleContractValid &&
		conjunctionSampleValuesValid && conjunctionSampleDeterministic &&
		conjunctionSampleChangesWithTime && visibilitySampleAvailable &&
		visibilitySampleContractValid && visibilitySampleValuesValid &&
		visibilitySampleDeterministic && visibilitySampleChangesWithTime &&
		multiEpochSamplesValid && multiEpochSamplesDeterministic &&
		multiEpochConjunctionMotionValid &&
		arbitrarySamplesPreserveLiveState && visibilityCurrentStateParity &&
		coreStateAvailable && coreStateSchemaMatches &&
		coreStateTypesMatch && astronomyValuesValid &&
		ephemerisRelationValid && locationValuesValid;
	const QJsonObject result{
		{QStringLiteral("schema_version"), 1},
		{QStringLiteral("probe_id"),
		 QStringLiteral("StellariumStateProbe")},
		{QStringLiteral("status"),
		 success ? QStringLiteral("ok") : QStringLiteral("failed")},
		{QStringLiteral("stellarium_version"), moduleVersion},
		{QStringLiteral("expected_stellarium_version"),
		 QStringLiteral(STELLARIUM_EXPECTED_VERSION)},
		{QStringLiteral("version_matches"), versionMatches},
		{QStringLiteral("app_initialized"), appInitialized},
		{QStringLiteral("self_registered"), selfRegistered},
		{QStringLiteral("solar_system_available"), solarSystemAvailable},
		{QStringLiteral("solar_system_identity_matches"),
		 solarSystemIdentityMatches},
		{QStringLiteral("null_core_returns_empty"), nullCoreReturnsEmpty},
		{QStringLiteral("null_body_core_returns_empty"),
		 nullBodyCoreReturnsEmpty},
		{QStringLiteral("empty_body_id_returns_empty"),
		 emptyBodyIdReturnsEmpty},
		{QStringLiteral("unknown_body_returns_empty"),
		 unknownBodyReturnsEmpty},
		{QStringLiteral("null_conjunction_core_returns_empty"),
		 nullConjunctionCoreReturnsEmpty},
		{QStringLiteral("invalid_conjunction_dates_return_empty"),
		 invalidConjunctionDatesReturnEmpty},
		{QStringLiteral("null_visibility_core_returns_empty"),
		 nullVisibilityCoreReturnsEmpty},
		{QStringLiteral("invalid_visibility_dates_return_empty"),
		 invalidVisibilityDatesReturnEmpty},
		{QStringLiteral("core_state_available"), coreStateAvailable},
		{QStringLiteral("core_state_schema_matches"),
		 coreStateSchemaMatches},
		{QStringLiteral("core_state_types_match"), coreStateTypesMatch},
		{QStringLiteral("astronomy_values_valid"), astronomyValuesValid},
		{QStringLiteral("ephemeris_relation_valid"),
		 ephemerisRelationValid},
		{QStringLiteral("location_values_valid"), locationValuesValid},
		{QStringLiteral("sun_state_available"), sunStateAvailable},
		{QStringLiteral("moon_state_available"), moonStateAvailable},
		{QStringLiteral("body_state_schema_matches"),
		 bodyStateSchemaMatches},
		{QStringLiteral("body_identities_match"), bodyIdentitiesMatch},
		{QStringLiteral("body_state_types_match"), bodyStateTypesMatch},
		{QStringLiteral("body_lookup_case_insensitive"),
		 bodyLookupCaseInsensitive},
		{QStringLiteral("body_state_times_match"), bodyStateTimesMatch},
		{QStringLiteral("body_position_values_valid"),
		 bodyPositionValuesMatch},
		{QStringLiteral("sun_phase_unavailable"), sunPhaseUnavailable},
		{QStringLiteral("moon_phase_available"), moonPhaseAvailable},
		{QStringLiteral("moon_phase_values_valid"),
		 moonPhaseValuesValid},
		{QStringLiteral("moon_phase_relation_valid"),
		 moonPhaseRelationValid},
		{QStringLiteral("conjunction_sample_available"),
		 conjunctionSampleAvailable},
		{QStringLiteral("conjunction_sample_contract_valid"),
		 conjunctionSampleContractValid},
		{QStringLiteral("conjunction_sample_values_valid"),
		 conjunctionSampleValuesValid},
		{QStringLiteral("conjunction_sample_deterministic"),
		 conjunctionSampleDeterministic},
		{QStringLiteral("conjunction_sample_changes_with_time"),
		 conjunctionSampleChangesWithTime},
		{QStringLiteral("visibility_sample_available"),
		 visibilitySampleAvailable},
		{QStringLiteral("visibility_sample_contract_valid"),
		 visibilitySampleContractValid},
		{QStringLiteral("visibility_sample_values_valid"),
		 visibilitySampleValuesValid},
		{QStringLiteral("visibility_sample_deterministic"),
		 visibilitySampleDeterministic},
		{QStringLiteral("visibility_sample_changes_with_time"),
		 visibilitySampleChangesWithTime},
		{QStringLiteral("multi_epoch_samples_valid"),
		 multiEpochSamplesValid},
		{QStringLiteral("multi_epoch_samples_deterministic"),
		 multiEpochSamplesDeterministic},
		{QStringLiteral("multi_epoch_conjunction_motion_valid"),
		 multiEpochConjunctionMotionValid},
		{QStringLiteral("multi_epoch_sample_count"),
		 static_cast<int>(multiEpochJulianDays.size())},
		{QStringLiteral("multi_epoch_samples"), multiEpochResults},
		{QStringLiteral("arbitrary_samples_preserve_live_state"),
		 arbitrarySamplesPreserveLiveState},
		{QStringLiteral("visibility_current_state_parity"),
		 visibilityCurrentStateParity},
		{QStringLiteral("julian_day_ut"), julianDayUt},
		{QStringLiteral("julian_day_tt"), julianDayTt},
		{QStringLiteral("delta_t_seconds"), deltaTSeconds},
		{QStringLiteral("utc_offset_hours"), utcOffsetHours},
		{QStringLiteral("time_rate_jd_per_second"),
		 timeRateJdPerSecond},
		{QStringLiteral("current_time_zone"),
		 coreState.value(QStringLiteral("currentTimeZone")).toString()},
		{QStringLiteral("location_time_zone"),
		 coreState.value(QStringLiteral("locationTimeZone")).toString()},
		{QStringLiteral("location_id"),
		 coreState.value(QStringLiteral("locationId")).toString()},
		{QStringLiteral("location_valid"),
		 coreState.value(QStringLiteral("locationValid")).toBool()},
		{QStringLiteral("longitude_degrees"), longitudeDegrees},
		{QStringLiteral("latitude_degrees"), latitudeDegrees},
		{QStringLiteral("altitude_meters"),
		 coreState.value(QStringLiteral("altitudeMeters")).toInt()},
		{QStringLiteral("planet_name"),
		 coreState.value(QStringLiteral("planetName")).toString()},
		{QStringLiteral("sun_english_name"),
		 sunState.value(QStringLiteral("englishName")).toString()},
		{QStringLiteral("moon_english_name"),
		 moonState.value(QStringLiteral("englishName")).toString()},
		{QStringLiteral("aberration_enabled"),
		 sunState.value(QStringLiteral("aberrationEnabled")).toBool()},
		{QStringLiteral("topocentric_coordinates_enabled"),
		 sunState.value(
			 QStringLiteral("topocentricCoordinatesEnabled")).toBool()},
		{QStringLiteral("sun_right_ascension_j2000_degrees"),
		 sunState.value(
			 QStringLiteral("rightAscensionJ2000Degrees")).toDouble()},
		{QStringLiteral("sun_declination_j2000_degrees"),
		 sunState.value(
			 QStringLiteral("declinationJ2000Degrees")).toDouble()},
		{QStringLiteral("sun_azimuth_geometric_degrees"),
		 sunState.value(
			 QStringLiteral("azimuthGeometricDegrees")).toDouble()},
		{QStringLiteral("sun_altitude_geometric_degrees"),
		 sunState.value(
			 QStringLiteral("altitudeGeometricDegrees")).toDouble()},
		{QStringLiteral("sun_distance_au"),
		 sunState.value(QStringLiteral("distanceAu")).toDouble()},
		{QStringLiteral("sun_angular_diameter_unscaled_degrees"),
		 sunState.value(
			 QStringLiteral("angularDiameterUnscaledDegrees")).toDouble()},
		{QStringLiteral("moon_right_ascension_j2000_degrees"),
		 moonState.value(
			 QStringLiteral("rightAscensionJ2000Degrees")).toDouble()},
		{QStringLiteral("moon_declination_j2000_degrees"),
		 moonState.value(
			 QStringLiteral("declinationJ2000Degrees")).toDouble()},
		{QStringLiteral("moon_azimuth_geometric_degrees"),
		 moonState.value(
			 QStringLiteral("azimuthGeometricDegrees")).toDouble()},
		{QStringLiteral("moon_altitude_geometric_degrees"),
		 moonState.value(
			 QStringLiteral("altitudeGeometricDegrees")).toDouble()},
		{QStringLiteral("moon_distance_au"),
		 moonState.value(QStringLiteral("distanceAu")).toDouble()},
		{QStringLiteral("moon_angular_diameter_unscaled_degrees"),
		 moonState.value(
			 QStringLiteral("angularDiameterUnscaledDegrees")).toDouble()},
		{QStringLiteral("moon_illuminated_fraction"),
		 moonIlluminatedFraction},
		{QStringLiteral("moon_phase_angle_degrees"),
		 moonPhaseAngleDegrees},
		{QStringLiteral("moon_elongation_degrees"),
		 moonElongationDegrees},
		{QStringLiteral("conjunction_longitude_difference_degrees"),
		 conjunctionDifferenceDegrees},
		{QStringLiteral("future_conjunction_longitude_difference_degrees"),
		 futureConjunctionDifferenceDegrees},
		{QStringLiteral("conjunction_daily_motion_degrees"),
		 conjunctionDailyMotionDegrees},
		{QStringLiteral("visibility_julian_day_tt"),
		 visibilityJulianDayTt},
		{QStringLiteral("visibility_delta_t_seconds"),
		 visibilityDeltaTSeconds},
		{QStringLiteral("visibility_sun_azimuth_degrees"),
		 visibilitySunAzimuth},
		{QStringLiteral("visibility_sun_altitude_degrees"),
		 visibilitySunAltitude},
		{QStringLiteral("visibility_moon_azimuth_degrees"),
		 visibilityMoonAzimuth},
		{QStringLiteral("visibility_moon_altitude_degrees"),
		 visibilityMoonAltitude},
		{QStringLiteral("visibility_moon_illuminated_fraction"),
		 visibilityMoonIlluminatedFraction},
		{QStringLiteral("visibility_moon_angular_diameter_degrees"),
		 visibilityMoonAngularDiameter},
		{QStringLiteral("visibility_moon_horizontal_parallax_degrees"),
		 visibilityMoonHorizontalParallax}
	};

	const QString sentinelPath =
		qEnvironmentVariable("STELLARIUM_STATE_PROBE_SENTINEL");
	if(!sentinelPath.isEmpty())
	{
		QFile sentinel(sentinelPath);
		if(sentinel.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			sentinel.write(QJsonDocument(result).toJson(
				QJsonDocument::Indented));
		}
		else
		{
			qCritical().noquote()
				<< "StellariumStateProbe could not write sentinel:"
				<< sentinel.errorString();
		}

		// Plug-ins initialize before Stellarium enters its top-level event
		// loop. Keep requesting shutdown until that loop handles it.
		QTimer* quitTimer = new QTimer(this);
		quitTimer->setInterval(2000);
		QObject::connect(quitTimer, &QTimer::timeout,
		                 QCoreApplication::instance(), []()
		{
			QCoreApplication::quit();
		});
		quitTimer->start();
	}

	if(success)
	{
		qInfo().noquote() << "STEL_STATE_PROBE_OK" << moduleVersion;
	}
	else
	{
		qCritical().noquote()
			<< "STEL_STATE_PROBE_FAILED"
			<< QString::fromUtf8(
				QJsonDocument(result).toJson(QJsonDocument::Compact));
	}
}

StelModule* StellariumStateProbeInterface::getStelModule() const
{
	return new StellariumStateProbe();
}

StelPluginInfo StellariumStateProbeInterface::getPluginInfo() const
{
	StelPluginInfo info;
	info.id = QStringLiteral("StellariumStateProbe");
	info.displayedName = QStringLiteral("Stellarium state API probe");
	info.authors = QStringLiteral("Stellarium Developers");
	info.contact = QStringLiteral("https://stellarium.org/");
	info.description = QStringLiteral(
		"Read-only Windows plug-in SDK host-state proof.");
	info.version = QStringLiteral("1.0.0");
	info.license = QStringLiteral("GPL-2.0-or-later");
	info.startByDefault = true;
	return info;
}
