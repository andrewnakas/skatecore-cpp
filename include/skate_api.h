/*
SKATE_API.H

Engine-free skateboarding library (C++17 inside, C ABI outside). Used by the Halo CE web port
(engine/) and meant to be reusable by other hosts.

Space: skate-native, metres, +Y up, board nose at +Z when heading is 0, right-handed.
Time: the library always steps at 1/60 s (SKATE_DT); hosts with slower ticks call
skate_step several times per tick.
*/

#ifndef SKATE_API_H
#define SKATE_API_H

#ifdef __cplusplus
extern "C" {
#endif

#define SKATE_DT (1.0f / 60.0f)

typedef struct skate_world skate_world;
typedef struct skate_skater skate_skater;

/* ---------- collision provided by the host */

typedef struct skate_hit
{
	float point[3];
	float normal[3];
	float fraction;   /* 0..1 along the ray */
	int surface;      /* host surface/material id, opaque to the library */
} skate_hit;

/* Cast a ray from origin along delta (not normalised: the end point is origin + delta).
   Return nonzero and fill *hit on a hit with solid geometry. */
typedef int (*skate_raycast_proc)(void *user, const float origin[3], const float delta[3], skate_hit *hit);

/* ---------- world */

skate_world *skate_world_new(skate_raycast_proc raycast, void *user);
void skate_world_delete(skate_world *world);

/* Grind rails. Add authored polylines directly, or derive them from a triangle soup
   (vertices: 3 floats each; triangles: 3 indices each). Returns the number of rails. */
int skate_world_add_rail(skate_world *world, const float *points, int point_count);
int skate_world_derive_rails(skate_world *world, const float *vertices, int vertex_count,
	const int *indices, int triangle_count);
int skate_world_rail_count(const skate_world *world);
void skate_world_clear_rails(skate_world *world);
/* Rail polyline access (points are 3 floats each); NULL when out of range. */
const float *skate_world_rail_points(const skate_world *world, int rail, int *point_count);
/* Closest rail point within max metres: returns the rail index or -1, fills point and unit tangent. */
int skate_world_nearest_rail(const skate_world *world, const float position[3], float max, float point[3], float tangent[3]);

/* ---------- input, sampled once per 60 Hz step */

enum
{
	SKATE_BUTTON_PUSH = 1 << 0,        /* kick-push (A / Space / W) */
	SKATE_BUTTON_BRAKE = 1 << 1,       /* foot brake (S) */
	SKATE_BUTTON_GRAB_LEFT = 1 << 2,   /* left trigger */
	SKATE_BUTTON_GRAB_RIGHT = 1 << 3,  /* right trigger */
	SKATE_BUTTON_MANUAL = 1 << 4,      /* hold: manual */
	SKATE_BUTTON_NOSE_MANUAL = 1 << 5, /* hold: nose manual */
	SKATE_BUTTON_POWERSLIDE = 1 << 6,  /* hold with steer: powerslide */
	SKATE_BUTTON_SKITCH = 1 << 7,      /* reserved */
};

typedef struct skate_input
{
	float left_stick[2];  /* x right, y up: steer / air spin */
	float right_stick[2]; /* x right, y up: flick-it (crouch down, flick up = ollie) */
	unsigned buttons;
} skate_input;

/* ---------- skater */

typedef enum skate_state
{
	SKATE_STATE_ROLLING,
	SKATE_STATE_MANUAL,
	SKATE_STATE_POWERSLIDE,
	SKATE_STATE_AIR,
	SKATE_STATE_GRIND,
	SKATE_STATE_BAIL,
} skate_state;

typedef enum skate_event_type
{
	SKATE_EVENT_TRICK,    /* name, points */
	SKATE_EVENT_LANDED,   /* value = 1 clean, 0 sketchy */
	SKATE_EVENT_BAIL,
	SKATE_EVENT_GRIND,    /* name */
	SKATE_EVENT_BANKED,   /* points */
	SKATE_EVENT_POP,      /* name of the gesture that popped */
} skate_event_type;

typedef struct skate_event
{
	skate_event_type type;
	char name[48];
	unsigned points;
	int value;
} skate_event;

typedef struct skate_pose
{
	float position[3];     /* board contact point (bottom of the wheels) */
	float velocity[3];
	float board_rotation[4]; /* quaternion x, y, z, w: board in world space */
	float heading;         /* board yaw, radians (0 = nose toward +Z) */
	float travel_heading;  /* direction of travel, radians */
	float crouch;          /* 0..1 */
	float lean;            /* -1..1, carve/balance lean */
	int state;             /* skate_state */
	int fakie;
	unsigned combo_value;
	unsigned combo_multiplier;
	unsigned long long banked;
} skate_pose;

skate_skater *skate_skater_new(void);
void skate_skater_delete(skate_skater *skater);

/* Put the skater on the board at a position with a velocity (hand-off from walking). */
void skate_skater_enter(skate_skater *skater, const float position[3], const float velocity[3], float facing);
void skate_skater_step(skate_skater *skater, skate_world *world, const skate_input *input);
void skate_skater_get_pose(const skate_skater *skater, skate_pose *pose);
void skate_skater_force_bail(skate_skater *skater);
/* External bonus into the running combo (e.g. a kill while in the air). */
void skate_skater_add_bonus(skate_skater *skater, const char *name, unsigned points);
/* Pop queued events; returns 0 when empty. */
int skate_skater_poll_event(skate_skater *skater, skate_event *event);
/* Name of the current trick in progress (flip in air, grind, manual), or "" */
const char *skate_skater_current_trick(const skate_skater *skater);

/* Gesture recognizer self-check used by tests and hosts: feed one right-stick sample
   (x right, y up); returns the recognised trick name or NULL. */
const char *skate_gesture_debug_feed(skate_skater *skater, float x, float y);

#ifdef __cplusplus
}
#endif

#endif
