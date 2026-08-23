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
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QTimer>

#include <cmath>

namespace
{
bool hasType(const QVariantMap& state, const QString& key, int typeId)
{
	const auto value = state.constFind(key);
	return value != state.cend() && value->metaType().id() == typeId;
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
		const StelCore* core = app.getCore();
		coreState = StelPluginAPI::getCoreStateSnapshot(core);
		sunState = StelPluginAPI::getSolarSystemBodyStateSnapshot(
			core, QStringLiteral("Sun"));
		moonState = StelPluginAPI::getSolarSystemBodyStateSnapshot(
			core, QStringLiteral("Moon"));
		lowerCaseMoonState =
			StelPluginAPI::getSolarSystemBodyStateSnapshot(
				core, QStringLiteral("moon"));
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
		moonPhaseRelationValid &&
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
		 moonElongationDegrees}
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
