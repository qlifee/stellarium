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

	if(argc != 2)
	{
		qCritical() << "Usage: DynamicPluginProbeLoader <plugin DLL>";
		return 2;
	}

	const QString pluginPath = QFileInfo(
		QString::fromLocal8Bit(argv[1])).absoluteFilePath();
	if(!QFileInfo::exists(pluginPath))
	{
		qCritical().noquote() << "Plugin DLL does not exist:" << pluginPath;
		return 3;
	}

	QPluginLoader loader(pluginPath);
	if(!loader.load())
	{
		qCritical().noquote() << "QPluginLoader failed:" << loader.errorString();
		return 4;
	}

	QObject* instance = loader.instance();
	if(instance == nullptr)
	{
		qCritical().noquote() << "Plugin instance failed:" << loader.errorString();
		return 5;
	}

	StelPluginInterface* plugin =
		qobject_cast<StelPluginInterface*>(instance);
	if(plugin == nullptr)
	{
		qCritical() << "The DLL does not implement StelPluginInterface";
		return 6;
	}

	const StelPluginInfo info = plugin->getPluginInfo();
	if(info.id != QStringLiteral("DynamicPluginProbe"))
	{
		qCritical() << "Unexpected plug-in ID:" << info.id;
		return 7;
	}

	std::unique_ptr<StelModule> module(plugin->getStelModule());
	if(!module)
	{
		qCritical() << "The plug-in returned no StelModule";
		return 8;
	}
	if(module->objectName() != QStringLiteral("DynamicPluginProbe"))
	{
		qCritical() << "Unexpected module object name:" << module->objectName();
		return 9;
	}

	const QString moduleVersion = module->getModuleVersion();
	if(moduleVersion != QStringLiteral(STELLARIUM_EXPECTED_VERSION))
	{
		qCritical() << "Unexpected Stellarium module version:"
		            << moduleVersion
		            << "expected" << STELLARIUM_EXPECTED_VERSION;
		return 10;
	}

	module->init();
	QTextStream output(stdout);
	output << "STEL_DYNAMIC_PLUGIN_LOADER_OK "
	       << info.id << ' ' << moduleVersion << Qt::endl;
	return 0;
}
