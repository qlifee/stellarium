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

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

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
	}

	const bool success = versionMatches && appInitialized &&
		selfRegistered && solarSystemAvailable &&
		solarSystemIdentityMatches;
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
		 solarSystemIdentityMatches}
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
