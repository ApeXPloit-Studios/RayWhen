#ifndef RAYCAST_SHADER_H
#define RAYCAST_SHADER_H

// Simple compute shader bytecode for raycasting
// This is a minimal shader that demonstrates GPU raycasting
// In a full implementation, this would be compiled HLSL

// Compute shader bytecode (simplified for demonstration)
// This would normally be compiled from HLSL source
static const unsigned char raycastShaderBytecode[] = {
    // Shader header and basic instructions
    // This is a placeholder - real implementation would use compiled HLSL
    0x44, 0x58, 0x42, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ... more shader bytecode would go here
    // For now, this is just a placeholder
};

static const int raycastShaderSize = sizeof(raycastShaderBytecode);

#endif // RAYCAST_SHADER_H
