// The depth of the board, the checkers and the dice as the key light sees them. The pieces are drawn together.

layout(location = 0) in vec3 aPosition;

struct PieceData {
    mat4 model;
    vec4 params;
    vec4 extra;
};

layout(std430, binding = 0) readonly buffer Pieces {
    PieceData uPieces[];
};

uniform mat4 uModel;
uniform int uInstanced;
uniform int uFirst;

void main()
{
    mat4 model = uInstanced != 0 ? uPieces[uFirst + gl_InstanceID].model : uModel;
    gl_Position = uLightViewProjection * model * vec4(aPosition, 1.0);
}
