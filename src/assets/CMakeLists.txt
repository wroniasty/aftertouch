# The runtime asset container format.
#
# Header-only and links NOTHING, on purpose. It is shared by the offline importer
# (src/tools/assetc) and the runtime (src/app/render), and belongs to neither: a
# format owned by its writer drifts, and a format owned by its reader cannot be
# written by a tool. Keeping it here also keeps it out of src/core, which has no
# business knowing that assets exist.
add_library(at_assets INTERFACE)
target_include_directories(at_assets INTERFACE include)
