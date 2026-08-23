# Headers intentionally included in the experimental Windows plug-in SDK.
# Paths are relative to the Stellarium src directory. Keep API entry headers
# separate from headers which are installed only to satisfy their includes.
SET(STELLARIUM_PLUGIN_SDK_API_HEADERS
     "core/StelApp.hpp"
     "core/StelModule.hpp"
     "core/StelPluginAPI.hpp"
     "core/StelPluginInterface.hpp")

SET(STELLARIUM_PLUGIN_SDK_DEPENDENCY_HEADERS
     "StelMainExport.hpp"
     "core/StelTextureTypes.hpp"
     "core/StelUtils.hpp"
     "core/VecMath.hpp")

SET(STELLARIUM_PLUGIN_SDK_HEADERS
     ${STELLARIUM_PLUGIN_SDK_API_HEADERS}
     ${STELLARIUM_PLUGIN_SDK_DEPENDENCY_HEADERS})
LIST(SORT STELLARIUM_PLUGIN_SDK_HEADERS)

SET(STELLARIUM_PLUGIN_SDK_QT_COMPONENTS
     Core
     Gui)
