// The checkers and the dice, drawn together: each instance its place, its turn and what it is.

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;

struct PieceData {
    mat4 model;  // the size, the turn and the place
    vec4 params; // x: 0 a checker, 1 a die; y: side; z: a die's number; w: highlight
    vec4 extra;  // x: how far a die is darkened as played
};

layout(std430, binding = 0) readonly buffer Pieces {
    PieceData uPieces[];
};

uniform int uFirst;

out vec3 vWorld;
out vec3 vNormal;
out vec3 vLocal;
out vec2 vUv;
flat out vec4 vParams;
flat out vec4 vExtra;

void main()
{
    PieceData piece = uPieces[uFirst + gl_InstanceID];
    vec4 world = piece.model * vec4(aPosition, 1.0);
    vWorld = world.xyz;
    vNormal = mat3(piece.model) * aNormal;
    vLocal = aPosition;
    vUv = aUv;
    vParams = piece.params;
    vExtra = piece.extra;
    gl_Position = uViewProjection * world;
}
