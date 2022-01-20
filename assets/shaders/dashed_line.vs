#version 330

layout (location = 0) in vec3 vertexPosition;

flat out vec3 startPos;
out vec3 vertPos;

uniform mat4 mvp;

void main()
{
    vec4 pos    = mvp * vec4(vertexPosition, 1.0);
    gl_Position = pos;
    vertPos     = pos.xyz / pos.w;
    startPos    = vertPos;
}