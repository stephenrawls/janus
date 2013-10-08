#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <pittpatt_errors.h>
#include <pittpatt_license.h>
#include <pittpatt_sdk.h>

#include "janus.h"

static janus_error to_janus_error(ppr_error_type error)
{
    if (error != PPR_SUCCESS)
        printf("PittPatt 5: %s\n", ppr_error_message(error));

    switch (error) {
      case PPR_SUCCESS:             return JANUS_SUCCESS;
      case PPR_INVALID_MODELS_PATH: return JANUS_INVALID_SDK_PATH;
      case PPR_NULL_MODELS_PATH:    return JANUS_INVALID_SDK_PATH;
      default:                      return JANUS_UNKNOWN_ERROR;
    }
}

janus_error janus_initialize(const char *sdk_path)
{
    const char *models = "/models/";
    const size_t models_path_len = strlen(sdk_path) + strlen(models);
    char *models_path = malloc(models_path_len);
    snprintf(models_path, models_path_len, "%s%s", sdk_path, models);

    janus_error error = to_janus_error(ppr_initialize_sdk(models_path, my_license_id, my_license_key));
    free(models_path);
    return error;
}

void janus_finalize()
{
    ppr_finalize_sdk();
}

typedef struct janus_context_type
{
    ppr_error_type ppr_error;
} janus_context_type;

janus_error janus_initialize_context(janus_context *context)
{
    ppr_settings_type settings = ppr_get_default_settings();
    settings.detection.enable = 1;
    settings.detection.min_size = 4;
    settings.detection.max_size = PPR_MAX_MAX_SIZE;
    settings.detection.adaptive_max_size = 1.f;
    settings.detection.adaptive_min_size = 0.01f;
    settings.detection.threshold = 0;
    settings.detection.use_serial_face_detection = 1;
    settings.detection.num_threads = 1;
    settings.detection.search_pruning_aggressiveness = 0;
    settings.detection.detect_best_face_only = 0;
    settings.landmarks.enable = 1;
    settings.landmarks.landmark_range = PPR_LANDMARK_RANGE_COMPREHENSIVE;
    settings.landmarks.manually_detect_landmarks = 0;
    settings.recognition.enable_extraction = 1;
    settings.recognition.enable_comparison = 1;
    settings.recognition.recognizer = PPR_RECOGNIZER_MULTI_POSE;
    settings.recognition.num_comparison_threads = 1;
    settings.recognition.automatically_extract_templates = 1;
    settings.recognition.extract_thumbnails = 0;
    settings.tracking.enable = 1;
    settings.tracking.cutoff = 0;
    settings.tracking.discard_completed_tracks = 0;
    settings.tracking.enable_shot_boundary_detection = 1;
    settings.tracking.shot_boundary_threshold = 0;
    return to_janus_error(ppr_initialize_context(settings, (ppr_context_type*)context));
}

void janus_finalize_context(janus_context *context)
{
    if (!context || !*context)
        return;
    ppr_finalize_context((ppr_context_type)*context);
    *context = NULL;
}

janus_error janus_detect(const janus_context context, const janus_image image, janus_object_list *object_list)
{
    if (!object_list)
        return JANUS_NULL_OBJECT_LIST;

    if (!context) {
        *object_list = NULL;
        return JANUS_NULL_CONTEXT;
    }

    if (!image) {
        object_list = NULL;
        return JANUS_NULL_IMAGE;
    }

    ppr_raw_image_type raw_image;
    raw_image.bytes_per_line = image->channels * image->width;
    raw_image.color_space = (image->channels == 1 ? PPR_RAW_IMAGE_GRAY8 : PPR_RAW_IMAGE_BGR24);
    raw_image.data = image->data;
    raw_image.height = image->height;
    raw_image.width = image->width;

    ppr_image_type ppr_image;
    ppr_create_image(raw_image, &ppr_image);

    ppr_face_list_type face_list;
//    ppr_detect_faces(context, ppr_image, &face_list);

    *object_list = janus_allocate_object_list(face_list.length);
    for (janus_size i=0; i<(*object_list)->size; i++) {
        ppr_face_type face = face_list.faces[i];
        ppr_face_attributes_type face_attributes;
        ppr_get_face_attributes(face, &face_attributes);

        const int num_face_attributes = 8;
        janus_object object = janus_allocate_object(num_face_attributes + 2*face_attributes.num_landmarks);
        object->attributes[0] = JANUS_FACE_CONFIDENCE;
        object->values[0] = face_attributes.confidence;
        object->attributes[1] = JANUS_FACE_WIDTH;
        object->values[1] = face_attributes.dimensions.width;
        object->attributes[2] = JANUS_FACE_HEIGHT;
        object->values[2] = face_attributes.dimensions.height;
        object->attributes[3] = JANUS_FACE_X;
        object->values[3] = face_attributes.position.x;
        object->attributes[4] = JANUS_FACE_Y;
        object->values[4] = face_attributes.position.y;
        object->attributes[5] = JANUS_FACE_ROLL;
        object->values[5] = face_attributes.rotation.roll;
        object->attributes[6] = JANUS_FACE_PITCH;
        object->values[6] = face_attributes.rotation.pitch;
        object->attributes[7] = JANUS_FACE_YAW;
        object->values[7] = face_attributes.rotation.yaw;

        ppr_landmark_list_type landmark_list;
        ppr_get_face_landmarks(face, &landmark_list);
        for (int j=0; j<face_attributes.num_landmarks; j++) {
            const int index = num_face_attributes + 2*j;
            switch (landmark_list.landmarks[j].category) {
              case PPR_LANDMARK_CATEGORY_LEFT_EYE:
                object->attributes[index] = JANUS_LEFT_EYE_X;
                object->attributes[index+1] = JANUS_LEFT_EYE_Y;
                break;
              case PPR_LANDMARK_CATEGORY_RIGHT_EYE:
                object->attributes[index] = JANUS_RIGHT_EYE_X;
                object->attributes[index+1] = JANUS_RIGHT_EYE_Y;
                break;
              case PPR_LANDMARK_CATEGORY_NOSE_BASE:
                object->attributes[index] = JANUS_NOSE_BASE_X;
                object->attributes[index+1] = JANUS_NOSE_BASE_Y;
                break;
              case PPR_LANDMARK_CATEGORY_NOSE_BRIDGE:
                object->attributes[index] = JANUS_NOSE_BRIDGE_X;
                object->attributes[index+1] = JANUS_NOSE_BRIDGE_Y;
                break;
              case PPR_LANDMARK_CATEGORY_EYE_NOSE:
                object->attributes[index] = JANUS_EYE_NOSE_X;
                object->attributes[index+1] = JANUS_EYE_NOSE_Y;
                break;
              case PPR_LANDMARK_CATEGORY_LEFT_UPPER_CHEEK:
                object->attributes[index] = JANUS_LEFT_UPPER_CHEEK_X;
                object->attributes[index+1] = JANUS_LEFT_UPPER_CHEEK_Y;
                break;
              case PPR_LANDMARK_CATEGORY_LEFT_LOWER_CHEEK:
                object->attributes[index] = JANUS_LEFT_LOWER_CHEEK_X;
                object->attributes[index+1] = JANUS_LEFT_LOWER_CHEEK_Y;
                break;
              case PPR_LANDMARK_CATEGORY_RIGHT_UPPER_CHEEK:
                object->attributes[index] = JANUS_RIGHT_UPPER_CHEEK_X;
                object->attributes[index+1] = JANUS_RIGHT_UPPER_CHEEK_Y;
                break;
              case PPR_LANDMARK_CATEGORY_RIGHT_LOWER_CHEEK:
                object->attributes[index] = JANUS_RIGHT_LOWER_CHEEK_X;
                object->attributes[index+1] = JANUS_RIGHT_LOWER_CHEEK_Y;
                break;
              case PPR_NUM_LANDMARK_CATEGORIES:
                object->attributes[index] = JANUS_INVALID_ATTRIBUTE;
                object->attributes[index+1] = JANUS_INVALID_ATTRIBUTE;
                break;
            }
            object->values[index]   = landmark_list.landmarks[j].position.x;
            object->values[index+1] = landmark_list.landmarks[j].position.y;
        }
        ppr_free_landmark_list(landmark_list);

        (*object_list)->objects[i] = object;
    }

    ppr_free_face_list(face_list);
    ppr_free_image(ppr_image);

    return JANUS_SUCCESS;
}

typedef struct janus_track_type
{
    ppr_context_type context;
} janus_track_type;

janus_track janus_allocate_track()
{
    janus_track track = malloc(sizeof(janus_track_type));
    return track;
}

void janus_track_frame(const janus_image frame, janus_track *track)
{
    (void) frame;
    (void) track;
}

janus_object_list janus_free_track(janus_track track)
{
    (void) track;
    return janus_allocate_object_list(0);
}
