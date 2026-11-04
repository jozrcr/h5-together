// Mod: a bigger vertex pool for big maps zoomed out ([camera] vertex_pool=4, on; see
// vertex_pool.cpp). Also the hidden research trace [camera] vb_trace=1.
#pragma once

namespace mods {

// Only after DetectGameBuild() said the build is hookable, and before the game makes its
// vertex pools (at its Direct3D device): from InstallAll.
void ApplyVertexPoolFromSettings();

}  // namespace mods
