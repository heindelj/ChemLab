#include "render/shaders.h"

// Desktop targets GL 3.3 core; the web build targets WebGL2 (GLSL ES 3.00),
// which shares the in/out + texture() syntax below but needs explicit
// precision qualifiers. WebGL1 / GLSL ES 1.00 is not supported: the impostor
// renderer needs gl_FragDepth, flat varyings and instancing, all of which are
// core in ES 3.00 but extensions at best in ES 1.00.
#if defined(__EMSCRIPTEN__)
#define GLSL_VERSION_LINE "#version 300 es\nprecision highp float;\nprecision highp int;\n"
#else
#define GLSL_VERSION_LINE "#version 330\n"
#endif

static const char* kLightingVS = GLSL_VERSION_LINE R"(
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
void main()
{
    fragPosition = vec3(matModel*vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragNormal = normalize(vec3(matNormal*vec4(vertexNormal, 1.0)));
    gl_Position = mvp*vec4(vertexPosition, 1.0);
}
)";

static const char* kLightingFS = GLSL_VERSION_LINE R"(
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;
out vec4 finalColor;
#define MAX_LIGHTS 4
#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT 1
struct Light {
    int enabled;
    int type;
    vec3 position;
    vec3 target;
    vec4 color;
};
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform Light lights[MAX_LIGHTS];
uniform vec4 ambient;
uniform vec3 viewPos;
void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 lightDot = vec3(0.0);
    vec3 normal = normalize(fragNormal);
    vec3 viewD = normalize(viewPos - fragPosition);
    vec3 specular = vec3(0.0);
    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (lights[i].enabled == 1)
        {
            vec3 light = vec3(0.0);
            if (lights[i].type == LIGHT_DIRECTIONAL) light = -normalize(lights[i].target - lights[i].position);
            if (lights[i].type == LIGHT_POINT) light = normalize(lights[i].position - fragPosition);
            float NdotL = max(dot(normal, light), 0.0);
            lightDot += lights[i].color.rgb*NdotL;
            float specCo = 0.0;
            if (NdotL > 0.0) specCo = pow(max(0.0, dot(viewD, reflect(-(light), normal))), 16.0);
            specular += specCo / 10.0;
        }
    }
    finalColor = (texelColor*((colDiffuse + vec4(specular, 1.0))*vec4(lightDot, 1.0)));
    finalColor += texelColor*ambient*colDiffuse;
    finalColor = pow(finalColor, vec4(1.0/2.2));
    finalColor.a = colDiffuse.a;
}
)";

Shader LoadLightingShader() {
    Shader shader = LoadShaderFromMemory(kLightingVS, kLightingFS);
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    const int ambientLoc = GetShaderLocation(shader, "ambient");
    const float ambient[4] = {0.2f, 0.2f, 0.2f, 1.0f};
    SetShaderValue(shader, ambientLoc, ambient, SHADER_UNIFORM_VEC4);
    return shader;
}
