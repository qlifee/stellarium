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
	const QString moduleVersion = getModuleVersion();
	const bool versionMatches =
		moduleVersion == QStringLiteral(STELLARIUM_EXPECTED_VERSION);
	const bool appInitialized = StelApp::isInitialized();
	bool selfRegistered = false;
	bool solarSystemAvailable = false;
	bool solarSystemIdentityMatches = false;
	QVariantMap coreState;

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
		coreState = StelPluginAPI::getCoreStateSnapshot(app.getCore());
	}

	const bool nullCoreReturnsEmpty =
		StelPluginAPI::getCoreStateSnapshot(nullptr).isEmpty();
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

	const bool success = versionMatches && appInitialized &&
		selfRegistered && solarSystemAvailable &&
		solarSystemIdentityMatches && nullCoreReturnsEmpty &&
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
		{QStringLiteral("core_state_available"), coreStateAvailable},
		{QStringLiteral("core_state_schema_matches"),
		 coreStateSchemaMatches},
		{QStringLiteral("core_state_types_match"), coreStateTypesMatch},
		{QStringLiteral("astronomy_values_valid"), astronomyValuesValid},
		{QStringLiteral("ephemeris_relation_valid"),
		 ephemerisRelationValid},
		{QStringLiteral("location_values_valid"), locationValuesValid},
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
		 coreState.value(QStringLiteral("planetName")).toString()}
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
