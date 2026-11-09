// The interface, in pixels from the top left corner.

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec4 aShape;

uniform vec2 uScreen;

out vec2 vUv;
out vec4 vColor;
flat out vec4 vShape;

void main()
{
    vUv = aUv;
    vColor = aColor;
    vShape = aShape;
    gl_Position = vec4(aPosition.x / uScreen.x * 2.0 - 1.0, 1.0 - aPosition.y / uScreen.y * 2.0, 0.0, 1.0);
}
