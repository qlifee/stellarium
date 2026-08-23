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

#include <QString>
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

//! Copy current observer-centred state for a Solar System body.
//!
//! @p bodyId currently accepts `Sun` or `Moon`, matched case-insensitively. A
//! successful snapshot always contains:
//! - `schemaVersion` (`int`, currently 1)
//! - `englishName` (`QString`)
//! - `julianDayUt`, `julianDayTt`, `rightAscensionJ2000Degrees`,
//!   `declinationJ2000Degrees`, `azimuthGeometricDegrees`,
//!   `altitudeGeometricDegrees`, `distanceAu`, and
//!   `angularDiameterUnscaledDegrees` (`double`)
//! - `aberrationEnabled`, `topocentricCoordinatesEnabled`, and
//!   `phaseDataAvailable` (`bool`). `aberrationEnabled` reports the host
//!   setting; individual body algorithms may suppress a particular correction.
//!
//! If `phaseDataAvailable` is true, the snapshot additionally contains
//! `illuminatedFraction`, `phaseAngleDegrees`, and `elongationDegrees`
//! (`double`). These keys are absent for the Sun or degenerate geometry.
//!
//! Right ascension is normalized to [0, 360) degrees. Geometric altitude is
//! airless (refraction off). Azimuth is normalized to [0, 360) degrees with
//! north at 0 and east at 90, independently of the user's display convention.
//! Distance is observer-to-body distance. Angular diameter ignores artificial
//! display scaling.
//!
//! The returned map owns its values and exposes no SolarSystem, Planet, or
//! StelObject layout or pointer. Call this on Stellarium's main thread after
//! the host has completed a core update. The @p core pointer follows the same
//! borrowed-pointer contract as getCoreStateSnapshot().
//!
//! @return an empty map for a null core, unavailable SolarSystem, an empty or
//! unknown body ID, or unavailable/non-finite base position data.
STELMAIN_EXPORT QVariantMap getSolarSystemBodyStateSnapshot(
	const StelCore* core, const QString& bodyId);
}

#endif // STELPLUGINAPI_HPP
