#ifndef __SCENE_H__
#define __SCENE_H__

typedef enum {
	SCENE_HOME,
	COUNT_SCENE
} SCENE;

#ifdef __cplusplus
extern "C" {
#endif
void SceneHomeEnter(void* const context);
#ifdef __cplusplus
}
#endif
#endif
