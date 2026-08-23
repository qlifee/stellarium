/*
 * Stellarium external dynamic plug-in proof
 * Copyright (C) 2026 Stellarium Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef STELLARIUMSTATEPROBE_HPP
#define STELLARIUMSTATEPROBE_HPP

#include "StelModule.hpp"
#include "StelPluginInterface.hpp"

#include <QObject>

class StellariumStateProbe : public StelModule
{
public:
	StellariumStateProbe();
	void init() override;
	void update(double deltaTime) override;

private:
	bool probePending = false;
};

class StellariumStateProbeInterface : public QObject,
	public StelPluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID StelPluginInterface_iid)
	Q_INTERFACES(StelPluginInterface)

public:
	StelModule* getStelModule() const override;
	StelPluginInfo getPluginInfo() const override;
};

#endif // STELLARIUMSTATEPROBE_HPP
