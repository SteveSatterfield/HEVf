//GLSL#version 150 compatibility

uniform vec4 iris_Viewport;	// corner and size of window
uniform float osg_FrameTime;	// for selection

#include <sphere.glsl>		// sphere support functions
#include <light.glsl>		// lighting support functions
#include <sphere_frag.glsl> // ray/sphere interactions

uniform float fatpointSize;	// sphere radius
in vec4 center;			// sphere center
in vec2 zrange;			// min and max depth

// type of color (choose one)
#define SOLID     0
#define CUBEMAP   0
#define SPHEREMAP 0
#define SPHERICAL 0
#define PROCEDURE 1

// lighting computations (choose one)
#define NOLIGHT  1
#define DIFFUSE  0
#define SPECULAR 0
#define PHYSICAL 0

// fake Fresnel environment glow
#define ENVGLOW 0

// selection blink
#define SELECT 0

// cube map texture
uniform samplerCube texcube;

// sphere map texture
uniform sampler2D texsphere;

// 2D texture
uniform sampler2D tex2d;

// color determined by a procedure, based on unit direction N
vec4 sphereColorProc(vec3 N)
{
    if (distance(vec3( 1,0,0),N) < 0.25 || 
	distance(vec3(-1,0,0),N) < 0.25) 
	return vec4(1,0,0,1);

    if (distance(vec3(0, 1,0),N) < 0.25 || 
	distance(vec3(0,-1,0),N) < 0.25) 
	return vec4(0,1,0,1);

    if (distance(vec3(0,0, 1),N) < 0.25 || 
	distance(vec3(0,0,-1),N) < 0.25) 
	return vec4(0,0,1,1);

    return vec4(.5,.5,.8,1);
}

void main()
{
    // compute sphere
    vec4 Fe; vec3 Ne; float face, r = fatpointSize;
    gl_FragDepth = sphereDraw(/* in */ gl_FragCoord, center, r, zrange, 
			      /* out */ Fe, Ne, face);
    gl_FragColor = gl_Color;

    // not strictly necessary, but saves computation
    if (gl_FragDepth > gl_DepthRange.far) discard;

    // sphere material color
    gl_MaterialParameters mtl = gl_FrontMaterial;

    // different ways to do color
#if SOLID
    // per-sphere color
    mtl.ambient = mtl.diffuse = gl_Color;
#elif CUBEMAP
    // cube-map texture-based color
    mtl.ambient = mtl.diffuse = sphereColorCubeMap(texcube, Ne);
#elif SPHEREMAP
    // sphere-map texture-based color
    mtl.ambient = mtl.diffuse = sphereColorSphMap(texsphere, Ne);
#elif SPHERICAL
    // spherical coordinate texture-based color
    mtl.ambient = mtl.diffuse = sphereColorSphCoord(tex2d, Ne);
#elif PROCEDURE
    // procedural color
    // Also shows using model coordinates rather than eye
    // Assumes modelview matrix has just scaling and rotation to
    // avoid a matrix inverse (this is normally a safe assumption)
    mtl.ambient = mtl.diffuse = 
	sphereColorProc(normalize(Ne*gl_NormalMatrix));
#endif
    

    // different ways to do lighting
#if DIFFUSE
    // diffuse only
    gl_FragColor = diffuse(gl_LightSource[0], mtl, Ne, Fe);
#elif SPECULAR
    // OpenGL-style specular
    mtl.specular = vec4(1); mtl.shininess = 512;
    gl_FragColor = light(gl_LightSource[0], mtl, Ne, Fe);

#elif PHYSICAL
    // Physically-plausible BRDF
    mtl.specular = vec4(0.04); mtl.shininess = 512;
    gl_FragColor = physLight(gl_LightSource[0], mtl, Ne, Fe);

#else
    // no shading
    gl_FragColor = mtl.ambient;
#endif

    // fake environment
#if ENVGLOW
    gl_FragColor = fakeEnv(gl_FragColor, Ne, Fe);
#endif

    // blinking selection
    // - would presumably do this based on a shader variable, distance
    // to the wand position, or some other selection criteria.
    // - could also do before glow
    // - could also blend between lit and diffuse, or lit and unlit,
    // or glow and noglow, or ...
#if SELECT
    gl_FragColor = selectBlink(vec4(0), 4.*gl_FragColor, osg_FrameTime);
#endif
}
