/*
 * Stellarium
 * Copyright (C) 2026 Stellarium Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef STELPLUGINAPI_HPP
#define STELPLUGINAPI_HPP

#include "StelMainExport.hpp"

#include <QVariantMap>

class StelCore;

//! Narrow, read-only API for external dynamic plug-ins.
namespace StelPluginAPI
{
//! Copy current time and observer state from a running Stellarium instance.
//!
//! The returned map owns its values and does not expose StelCore or
//! StelLocation object layouts. A successful snapshot contains these keys:
//! - `schemaVersion` (`int`, currently 1)
//! - `julianDayUt`, `julianDayTt`, `deltaTSeconds`, `utcOffsetHours`,
//!   `timeRateJdPerSecond`, `longitudeDegrees`, and `latitudeDegrees`
//!   (`double`)
//! - `currentTimeZone`, `locationTimeZone`, `locationId`, and `planetName`
//!   (`QString`)
//! - `locationValid` (`bool`)
//! - `altitudeMeters` (`int`)
//!
//! Longitude is east-positive and latitude is north-positive. The coordinates
//! use StelLocation's view-effective values, including its Observer
//! pseudo-planet behavior. Call this on Stellarium's main thread. Pass only the
//! opaque borrowed pointer returned by StelApp::getCore() in the matching
//! running Stellarium build; external code must not dereference it or retain
//! and use it after host shutdown.
//!
//! @return an empty map if @p core is null, otherwise the complete snapshot.
STELMAIN_EXPORT QVariantMap getCoreStateSnapshot(const StelCore* core);
}

#endif // STELPLUGINAPI_HPP
