/*
 * Stellarium external dynamic plug-in proof
 * Copyright (C) 2026 Stellarium Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "StelModule.hpp"
#include "StelPluginInterface.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QPluginLoader>
#include <QTextStream>

#include <memory>

int main(int argc, char* argv[])
{
	QCoreApplication app(argc, argv);

	if(argc != 3 && argc != 4)
	{
		qCritical() << "Usage: StellariumPluginProbeLoader"
		            << "<plugin DLL> <expected ID> [--initialize]";
		return 2;
	}
	const bool initialize = argc == 4;
	if(initialize && QString::fromLocal8Bit(argv[3]) !=
		QStringLiteral("--initialize"))
	{
		qCritical() << "Unknown loader option:" << argv[3];
		return 3;
	}

	const QString pluginPath = QFileInfo(
		QString::fromLocal8Bit(argv[1])).absoluteFilePath();
	const QString expectedId = QString::fromLocal8Bit(argv[2]);
	if(!QFileInfo::exists(pluginPath))
	{
		qCritical().noquote() << "Plugin DLL does not exist:" << pluginPath;
		return 4;
	}

	QPluginLoader loader(pluginPath);
	if(!loader.load())
	{
		qCritical().noquote() << "QPluginLoader failed:" << loader.errorString();
		return 5;
	}

	QObject* instance = loader.instance();
	if(instance == nullptr)
	{
		qCritical().noquote() << "Plugin instance failed:" << loader.errorString();
		return 6;
	}

	StelPluginInterface* plugin =
		qobject_cast<StelPluginInterface*>(instance);
	if(plugin == nullptr)
	{
		qCritical() << "The DLL does not implement StelPluginInterface";
		return 7;
	}

	const StelPluginInfo info = plugin->getPluginInfo();
	if(info.id != expectedId)
	{
		qCritical() << "Unexpected plug-in ID:" << info.id;
		return 8;
	}
	if(info.version != QStringLiteral("1.0.0") ||
		info.license != QStringLiteral("GPL-2.0-or-later") ||
		!info.startByDefault)
	{
		qCritical() << "Unexpected plug-in metadata for" << info.id;
		return 9;
	}

	std::unique_ptr<StelModule> module(plugin->getStelModule());
	if(!module)
	{
		qCritical() << "The plug-in returned no StelModule";
		return 10;
	}
	if(module->objectName() != expectedId)
	{
		qCritical() << "Unexpected module object name:" << module->objectName();
		return 11;
	}

	const QString moduleVersion = module->getModuleVersion();
	if(moduleVersion != QStringLiteral(STELLARIUM_EXPECTED_VERSION))
	{
		qCritical() << "Unexpected Stellarium module version:"
		            << moduleVersion
		            << "expected" << STELLARIUM_EXPECTED_VERSION;
		return 12;
	}

	if(initialize)
		module->init();
	QTextStream output(stdout);
	output << "STEL_PLUGIN_METADATA_OK "
	       << info.id << ' ' << moduleVersion << Qt::endl;
	return 0;
}
